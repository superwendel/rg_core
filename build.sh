#!/usr/bin/env bash
# Native Linux builds. Windows builds remain available through build.bat.
set -euo pipefail

repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
test_output="$repo/.test-build/linux"
bench_output="$repo/.bench-build/linux/current"
target=${1:-test}
if (($#)); then shift; fi
run_args=("$@")
read -r -a cc <<< "${CC:-gcc}"
read -r -a cxx <<< "${CXX:-g++}"
read -r -a user_cppflags <<< "${CPPFLAGS:-}"
read -r -a user_cflags <<< "${CFLAGS:-}"
read -r -a user_cxxflags <<< "${CXXFLAGS:-}"
read -r -a user_ldflags <<< "${LDFLAGS:-}"
feature_flags=(-D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L)
test_flags=(-std=c11 -O2 -Wall -Wextra -Werror -Wno-unused-function
    "${feature_flags[@]}" "${user_cppflags[@]}" "${user_cflags[@]}")
bench_flags=(-O2 -DNDEBUG -Wall -Wextra -Wno-unused-function
    "${feature_flags[@]}" "${user_cppflags[@]}" -I "$repo/src" -I "$repo/benchmarks")
have_avx2=0
have_fma=0
asm_object=
sdl_cflags=()
sdl_libs=()

usage() {
    cat <<'EOF'
Usage: ./build.sh [target] [benchmark arguments...]
  test (default), test_<module>, test_ubsan
  bench                 algo, hash, and containers
  bench_<suite>         algo, hash, containers, math, sprintf, storage, input, prof
  bench_regression      math, sprintf, storage, input, and prof
  clean                 remove only native Linux build artifacts
Environment: CC, CXX, CPPFLAGS, CFLAGS, CXXFLAGS, LDFLAGS,
             RG_BUILD_SIMD=auto|off, RG_BENCH_BUILD_ONLY=1, RG_BENCH_DEPS
SDL3 is discovered through pkg-config sdl3. Aggregate targets skip missing SDL3;
explicit SDL/input targets require it. AVX2/FMA and assembly runs are host gated.
EOF
}

probe_cpu() {
    case ${RG_BUILD_SIMD:-auto} in
        off) echo 'Optional AVX2/FMA and assembly variants disabled.'; return ;;
        auto) ;;
        *) echo 'RG_BUILD_SIMD must be auto or off.' >&2; exit 1 ;;
    esac
    mkdir -p "$test_output"
    # Compile the probe at the compiler's baseline, before adding any SIMD flags.
    "${cc[@]}" -std=c11 -O2 -x c - -o "$test_output/cpu_probe" <<'EOF'
int main(int argc, char **argv)
{
#if defined(__x86_64__) && (defined(__GNUC__) || defined(__clang__))
    __builtin_cpu_init();
    if (argc == 2 && argv[1][0] == 'a') return !__builtin_cpu_supports("avx2");
    if (argc == 2 && argv[1][0] == 'f') return !__builtin_cpu_supports("fma");
#else
    (void)argc; (void)argv;
#endif
    return 1;
}
EOF
    if "$test_output/cpu_probe" avx2; then have_avx2=1; fi
    if "$test_output/cpu_probe" fma; then have_fma=1; fi
    echo "Host capabilities: AVX2=$have_avx2 FMA=$have_fma"
}

find_sdl() {
    if command -v pkg-config >/dev/null && pkg-config --exists sdl3; then
        read -r -a sdl_cflags <<< "$(pkg-config --cflags sdl3)"
        read -r -a sdl_libs <<< "$(pkg-config --libs sdl3)"
        return 0
    fi
    return 1
}

require_sdl() {
    if ! find_sdl; then
        echo 'SDL3 development files required; pkg-config could not find sdl3.' >&2
        exit 1
    fi
}

build_asm() {
    if [[ -z $asm_object ]]; then
        asm_object="$test_output/sprintf_helpers.o"
        "${cc[@]}" -c "$repo/src/asm/sprintf/linux_x64/rg_sprintf_asm_x64.S" -o "$asm_object"
    fi
}

test_case() {
    local name=$1 source=$2
    shift 2
    mkdir -p "$test_output"
    echo "Building $name..."
    "${cc[@]}" "${test_flags[@]}" "$repo/tests/$source" "$@" \
        "${user_ldflags[@]}" -lm -o "$test_output/$name"
    "$test_output/$name"
}

test_includes() {
    local order expected
    for order in 0 1 2 3; do
        expected=0
        if [[ $order == 2 ]]; then expected=1; fi
        test_case "includes_default_$order" test_sprintf_includes.c \
            "-DRG_SPRINTF_INCLUDE_ORDER=$order" "-DRG_SPRINTF_EXPECT_ASM=$expected"
    done
    test_case includes_force_c test_sprintf_includes.c -DRG_SPRINTF_INCLUDE_ORDER=1 -DRG_SPRINTF_HYBRID_FORCE_C
    test_case includes_no_asm test_sprintf_includes.c -DRG_SPRINTF_INCLUDE_ORDER=3 -DRG_SPRINTF_NO_ASM
    test_case includes_force_c_precedence test_sprintf_includes.c -DRG_SPRINTF_INCLUDE_ORDER=1 \
        -DRG_SPRINTF_HYBRID_FORCE_C -DRG_SPRINTF_HYBRID_FORCE_ASM -DRG_SPRINTF_HAS_ASM
    test_case includes_no_asm_precedence test_sprintf_includes.c -DRG_SPRINTF_INCLUDE_ORDER=3 \
        -DRG_SPRINTF_NO_ASM -DRG_SPRINTF_HYBRID_FORCE_ASM -DRG_SPRINTF_HAS_ASM
    test_case includes_direct_asm_fallback test_sprintf_includes.c -DRG_SPRINTF_INCLUDE_ORDER=2 \
        -DRG_SPRINTF_EXPECT_ASM=1 -DRG_SPRINTF_NO_ASM
    if ((have_avx2)); then
        build_asm
        for order in 1 2 3; do
            test_case "includes_asm_$order" test_sprintf_includes.c -mavx2 \
                "-DRG_SPRINTF_INCLUDE_ORDER=$order" -DRG_SPRINTF_EXPECT_ASM=1 \
                -DRG_SPRINTF_HAS_ASM "$asm_object"
        done
        test_case includes_force_asm test_sprintf_includes.c -mavx2 -DRG_SPRINTF_INCLUDE_ORDER=1 \
            -DRG_SPRINTF_EXPECT_ASM=1 -DRG_SPRINTF_HYBRID_FORCE_ASM -DRG_SPRINTF_HAS_ASM "$asm_object"
    fi
}

test_module() {
    local module=$1 clip
    case $module in
        sprintf)
            test_case test_sprintf test_sprintf.c
            test_case test_sprintf_scalar test_sprintf.c -DRG_SPRINTF_NO_SIMD
            test_case test_sprintf_hybrid_fallback test_sprintf.c -DRG_SPRINTF_TEST_HYBRID -DRG_SPRINTF_NO_ASM -DRG_SPRINTF_NO_SIMD
            test_case test_sprintf_asm_fallback test_sprintf.c -DRG_SPRINTF_TEST_ASM -DRG_SPRINTF_NO_ASM -DRG_SPRINTF_NO_SIMD
            test_case test_sprintf_secure test_sprintf.c -DRG_SPRINTF_SECURE
            if ((have_avx2)); then
                build_asm
                test_case test_sprintf_avx2 test_sprintf.c -mavx2
                test_case test_sprintf_avx2_secure test_sprintf.c -mavx2 -DRG_SPRINTF_SECURE
                test_case test_sprintf_hybrid_asm test_sprintf.c -mavx2 -DRG_SPRINTF_TEST_HYBRID -DRG_SPRINTF_HAS_ASM "$asm_object"
                test_case test_sprintf_asm test_sprintf.c -mavx2 -DRG_SPRINTF_TEST_ASM -DRG_SPRINTF_HAS_ASM "$asm_object"
                test_case test_sprintf_asm_secure test_sprintf.c -mavx2 -DRG_SPRINTF_TEST_ASM -DRG_SPRINTF_SECURE -DRG_SPRINTF_HAS_ASM "$asm_object"
                test_case test_sprintf_abi test_sprintf_abi.c "$repo/tests/asm/test_sprintf_abi_x64.S" "$asm_object"
            fi
            test_includes
            ;;
        log)
            test_case test_log test_log.c
            test_case test_log_fallback test_log.c -DRG_SPRINTF_NO_ASM -DRG_SPRINTF_NO_SIMD
            if ((have_avx2)); then
                build_asm
                test_case test_log_asm test_log.c -mavx2 -DRG_SPRINTF_HAS_ASM "$asm_object"
            fi
            ;;
        assert)
            test_case test_assert test_assert.c
            test_case test_assert_disabled test_assert_disabled.c
            ;;
        mem)
            test_case test_mem test_mem.c
            test_case test_mem_eager test_mem.c -DRG_MALLOC_LAZY_COMMIT=0
            test_case test_mem_secure test_mem.c -DRG_MALLOC_SECURE
            ;;
        containers)
            test_case test_containers test_containers.c
            test_case test_containers_config test_containers.c -DRG_CONTAINERS_MIN_CAP=3 -DRG_SPARSE_INVALID=17
            ;;
        time)
            test_case test_time test_time.c
            test_case test_time_custom test_time_custom.c
            ;;
        prof)
            test_case test_prof test_prof.c
            test_case test_prof_disabled test_prof_disabled.c
            ;;
        bin)
            test_case test_bin test_bin.c
            test_case test_bin_unaligned test_bin.c -DRG_BIN_FAST_UNALIGNED=1
            test_case test_bin_bytewise test_bin.c -DRG_BIN_LITTLE_ENDIAN=0
            ;;
        hash)
            test_case test_hash test_hash.c
            test_case test_hash_eager test_hash.c -DRG_MALLOC_LAZY_COMMIT=0
            ;;
        random)
            test_case test_random test_random.c
            test_case test_random_portable test_random.c -DRG_RANDOM_FORCE_PORTABLE_MUL128
            ;;
        algo)
            test_case test_algo test_algo.c
            test_case test_algo_config test_algo.c -DRG_ALGO_RADIX_BITS=4 -DRG_ALGO_STABLE_RUN=5 -DRG_ALGO_INSERTION_CUTOFF=9 -DRG_ALGO_STACK_CAP=1
            ;;
        string)
            test_case test_string test_string.c
            test_case test_string_scalar test_string.c -DRG_STRING_NO_SIMD
            test_case test_string_secure test_string.c -DRG_STRING_SECURE
            if ((have_avx2)); then
                test_case test_string_avx2 test_string.c -mavx2
                test_case test_string_avx2_scalar test_string.c -mavx2 -DRG_STRING_NO_SIMD
                test_case test_string_avx2_secure test_string.c -mavx2 -DRG_STRING_SECURE
            fi
            ;;
        math)
            test_case test_math test_math.c
            test_case test_math_scalar test_math.c -DRG_MATH_NO_SIMD -DRG_MATH_MAX_PERF=0
            test_case test_math_checked test_math.c -DRG_MATH_MAX_PERF=0
            test_case test_math_lean test_math_lean.c -DRG_MATH_NO_SIMD
            for clip in RH_ZO LH_NO LH_ZO; do
                test_case "test_math_clip_$clip" test_math.c "-DRG_MATH_CLIP_CONTROL=RG_MATH_CLIP_CONTROL_$clip"
            done
            if ((have_avx2)); then
                test_case test_math_avx2 test_math.c -mavx2
                test_case test_math_plain test_math.c -mavx2 -DRG_MATH_VEC3_PLAIN -DRG_MATH_VEC4_PLAIN
                for clip in RH_ZO LH_NO LH_ZO; do
                    test_case "test_math_avx2_clip_$clip" test_math.c -mavx2 "-DRG_MATH_CLIP_CONTROL=RG_MATH_CLIP_CONTROL_$clip"
                done
            fi
            if ((have_fma)); then test_case test_math_fma test_math.c -mfma; fi
            ;;
        sdl) require_sdl; test_case test_sdl test_sdl.c "${sdl_cflags[@]}" "${sdl_libs[@]}" ;;
        input)
            require_sdl
            test_case test_input test_input.c "${sdl_cflags[@]}" "${sdl_libs[@]}"
            test_case test_input_frame test_input_frame.c "${sdl_cflags[@]}" "${sdl_libs[@]}"
            ;;
        *) echo "Unknown test module: $module" >&2; exit 1 ;;
    esac
}

test_all() {
    local module
    for module in sprintf log assert mem containers time prof bin string hash random algo math; do
        test_module "$module"
    done
    if find_sdl; then
        test_module sdl
        test_module input
    else
        echo 'Skipping SDL-dependent tests: pkg-config sdl3 is unavailable.'
    fi
    echo 'All Linux tests passed.'
}

test_ubsan() {
    local source name
    test_output="$test_output/ubsan"
    test_flags+=(-g -fsanitize=undefined -fno-sanitize-recover=undefined)
    for source in "$repo"/tests/test_*.c; do
        name=${source##*/}
        case $name in test_sdl.c|test_input.c|test_input_frame.c|test_sprintf_abi.c) continue ;; esac
        test_case "${name%.c}" "$name"
    done
    # Keep the portable-default sanitizer pass free of AVX2; cover the opt-in
    # unaligned access mode separately because it crosses C alignment rules.
    test_case test_bin_unaligned test_bin.c -DRG_BIN_FAST_UNALIGNED=1
    test_case test_sprintf_secure test_sprintf.c -DRG_SPRINTF_SECURE
    test_case test_string_secure test_string.c -DRG_STRING_SECURE
    test_case test_mem_secure test_mem.c -DRG_MALLOC_SECURE
    echo 'All Linux UBSan tests passed.'
}

bench_suite() {
    local suite=$1 aggregate=${2:-0}
    local compiler extension=c
    local extra=() objects=() links=()
    if [[ $suite == input ]] && ! find_sdl; then
        if ((aggregate)); then echo 'Skipping input benchmark: pkg-config sdl3 is unavailable.'; return; fi
        require_sdl
    fi
    mkdir -p "$bench_output"
    case $suite in
        algo|hash|containers|math|storage) extension=cpp; compiler=cxx ;;
        sprintf|input|prof) compiler=cc ;;
        *) echo "Unknown benchmark suite: $suite" >&2; exit 1 ;;
    esac
    if [[ -n ${RG_BENCH_DEPS:-} ]]; then
        case $suite in
            algo)
                if [[ -f $RG_BENCH_DEPS/quadsort.h && -f $RG_BENCH_DEPS/crumsort.h ]]; then
                    extra+=(-DRG_BENCH_ALGO_EXTRAS -I "$RG_BENCH_DEPS")
                    "${cc[@]}" -std=c11 "${bench_flags[@]}" "${user_cflags[@]}" \
                        -DRG_BENCH_ALGO_EXTRAS -I "$RG_BENCH_DEPS" -c "$repo/benchmarks/bench_algo_refs.c" -o "$bench_output/bench_algo_refs.o"
                    objects+=("$bench_output/bench_algo_refs.o")
                fi
                ;;
            hash|containers)
                if [[ -f $RG_BENCH_DEPS/stb_ds.h ]]; then extra+=(-DRG_BENCH_STB_DS -I "$RG_BENCH_DEPS"); fi
                if [[ $suite == containers && -f $RG_BENCH_DEPS/entt/single_include/entt/entt.hpp ]]; then
                    extra+=(-DRG_BENCH_ENTT -I "$RG_BENCH_DEPS/entt/single_include")
                fi
                ;;
            math)
                if [[ -f $RG_BENCH_DEPS/cglm/include/cglm/cglm.h ]]; then extra+=(-DRG_BENCH_CGLM -I "$RG_BENCH_DEPS/cglm/include"); fi
                if [[ -f $RG_BENCH_DEPS/glm/glm.hpp ]]; then extra+=(-DRG_BENCH_GLM -I "$RG_BENCH_DEPS"); fi
                ;;
        esac
    fi
    case $suite in
        math|sprintf|storage|input|prof)
            # The sink remains an opaque boundary even when callers set -flto.
            "${cc[@]}" -std=c11 "${bench_flags[@]}" "${user_cflags[@]}" -fno-lto \
                -c "$repo/benchmarks/bench_sink.c" -o "$bench_output/bench_sink.o"
            objects+=("$bench_output/bench_sink.o")
            ;;
    esac
    if [[ $suite == sprintf ]]; then
        "${cc[@]}" -std=c11 "${bench_flags[@]}" "${user_cflags[@]}" \
            -c "$repo/benchmarks/bench_sprintf_backend.c" -o "$bench_output/bench_sprintf_c.o"
        objects+=("$bench_output/bench_sprintf_c.o")
        if ((have_avx2)); then
            "${cc[@]}" -std=c11 "${bench_flags[@]}" "${user_cflags[@]}" -mavx2 \
                -DRG_BENCH_SPRINTF_ASM -DRG_SPRINTF_HAS_ASM -c "$repo/benchmarks/bench_sprintf_backend.c" -o "$bench_output/bench_sprintf_asm.o"
            "${cc[@]}" -c "$repo/src/asm/sprintf/linux_x64/rg_sprintf_asm_x64.S" -o "$bench_output/sprintf_helpers.o"
            objects+=("$bench_output/bench_sprintf_asm.o" "$bench_output/sprintf_helpers.o")
        else
            extra+=(-DRG_BENCH_SPRINTF_NO_ASM)
        fi
        if [[ -n ${RG_BENCH_DEPS:-} && -f $RG_BENCH_DEPS/stb_sprintf.h ]]; then
            extra+=(-DRG_BENCH_SPRINTF_STB)
            "${cc[@]}" -std=c11 "${bench_flags[@]}" "${user_cflags[@]}" \
                -DRG_BENCH_SPRINTF_STB -I "$RG_BENCH_DEPS" -c "$repo/benchmarks/bench_sprintf_backend.c" -o "$bench_output/bench_sprintf_stb.o"
            objects+=("$bench_output/bench_sprintf_stb.o")
        fi
    fi
    if [[ $suite == input ]]; then extra+=("${sdl_cflags[@]}"); links+=("${sdl_libs[@]}"); fi
    echo "Building bench_$suite..."
    if [[ $compiler == cxx ]]; then
        "${cxx[@]}" -std=c++17 "${bench_flags[@]}" "${user_cxxflags[@]}" "${extra[@]}" \
            "$repo/benchmarks/bench_$suite.$extension" "${objects[@]}" "${user_ldflags[@]}" "${links[@]}" -lm -o "$bench_output/bench_$suite"
    else
        "${cc[@]}" -std=c11 "${bench_flags[@]}" "${user_cflags[@]}" "${extra[@]}" \
            "$repo/benchmarks/bench_$suite.$extension" "${objects[@]}" "${user_ldflags[@]}" "${links[@]}" -lm -o "$bench_output/bench_$suite"
    fi
    if [[ -z ${RG_BENCH_BUILD_ONLY:-} ]]; then "$bench_output/bench_$suite" "${run_args[@]}"; fi
}

case $target in
    help|-h|--help) usage ;;
    clean) rm -rf -- "$test_output" "$repo/.bench-build/linux" ;;
    test_ubsan) test_ubsan ;;
    test) probe_cpu; test_all ;;
    test_*) probe_cpu; test_module "${target#test_}" ;;
    bench) probe_cpu; for suite in algo hash containers; do bench_suite "$suite" 1; done ;;
    bench_regression) probe_cpu; for suite in math sprintf storage input prof; do bench_suite "$suite" 1; done ;;
    bench_*) probe_cpu; bench_suite "${target#bench_}" ;;
    *) usage >&2; echo "Unknown target: $target" >&2; exit 1 ;;
esac
