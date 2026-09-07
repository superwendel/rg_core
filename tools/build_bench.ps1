param([ValidateSet('all', 'math', 'sprintf', 'storage', 'input', 'prof')][string]$Suite = 'all')
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$sourceRoot = if ($env:RG_BENCH_SOURCE_ROOT) { $env:RG_BENCH_SOURCE_ROOT } else { $repo }
$outputRoot = if ($env:RG_BENCH_OUTPUT_DIR) { $env:RG_BENCH_OUTPUT_DIR } else { Join-Path $repo '.bench-build\current' }
New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null
$sourceRoot = (Resolve-Path -LiteralPath $sourceRoot).Path
$outputRoot = (Resolve-Path -LiteralPath $outputRoot).Path
$flags = @('/nologo', '/W4', '/wd4505', '/O2', '/Ob3', '/Oi', '/Ot', '/Oy', '/GL',
    '/arch:AVX2', '/fp:fast', '/GS-', '/DNDEBUG', '/D_CRT_SECURE_NO_WARNINGS',
    '/I', (Join-Path $sourceRoot 'src'), '/I', (Join-Path $repo 'benchmarks'))
# Suppress successful compiler output from functions that return object paths.
function Build-Object([string]$Source, [string]$Name, [string[]]$Extra = @()) {
    $target = Join-Path $outputRoot "$Name.obj"
    $baseFlags = if ('/GL-' -in $Extra) { @($flags | Where-Object { $_ -ne '/GL' }) } else { $flags }
    $arguments = $baseFlags + $Extra + @('/c', (Join-Path $repo $Source), "/Fo:$target")
    & cl @arguments | ForEach-Object { Write-Host $_ }
    if ($LASTEXITCODE -ne 0) { throw "Could not compile $Source" }
    return $target
}
$suites = if ($Suite -eq 'all') { @('math', 'sprintf', 'storage', 'input', 'prof') } else { @($Suite) }
Push-Location $repo
try {
    $sink = Build-Object 'benchmarks\bench_sink.c' 'bench_sink' @('/std:c11', '/GL-')
    foreach ($name in $suites) {
        $objects = @($sink)
        $extra = @('/std:c11')
        $links = @()
        $extension = 'c'
        if ($name -eq 'math' -or $name -eq 'storage') {
            $extra = @('/std:c++17', '/EHsc')
            $extension = 'cpp'
        }
        if ($name -eq 'math' -and $env:RG_BENCH_DEPS) {
            if (Test-Path (Join-Path $env:RG_BENCH_DEPS 'cglm\include\cglm\cglm.h')) {
                $extra += @('/DRG_BENCH_CGLM', '/I', (Join-Path $env:RG_BENCH_DEPS 'cglm\include'))
            }
            if (Test-Path (Join-Path $env:RG_BENCH_DEPS 'glm\glm.hpp')) {
                $extra += @('/DRG_BENCH_GLM', '/I', $env:RG_BENCH_DEPS)
            }
        }
        if ($name -eq 'math' -and $env:RG_BENCH_DIRECTXMATH) { $extra += '/DRG_BENCH_DIRECTXMATH' }
        if ($name -eq 'sprintf') {
            $objects += Build-Object 'benchmarks\bench_sprintf_backend.c' 'bench_sprintf_c' @('/std:c11')
            $objects += Build-Object 'benchmarks\bench_sprintf_backend.c' 'bench_sprintf_asm' @('/std:c11', '/DRG_BENCH_SPRINTF_ASM')
            $asm = Join-Path $outputRoot 'sprintf_helpers.obj'
            & ml64 /nologo /c /Fo $asm (Join-Path $sourceRoot 'src\asm\sprintf\win_x64\rg_sprintf_asm_x64.asm')
            if ($LASTEXITCODE -ne 0) { throw 'Could not build formatter assembly.' }
            $objects += $asm
            if ($env:RG_BENCH_DEPS -and (Test-Path (Join-Path $env:RG_BENCH_DEPS 'stb_sprintf.h'))) {
                $extra += '/DRG_BENCH_SPRINTF_STB'
                $objects += Build-Object 'benchmarks\bench_sprintf_backend.c' 'bench_sprintf_stb' @('/std:c11', '/DRG_BENCH_SPRINTF_STB', '/I', $env:RG_BENCH_DEPS)
            }
        }
        if ($name -eq 'input') {
            if (!$env:SDL3_DIR -or !(Test-Path (Join-Path $env:SDL3_DIR 'include\SDL3\SDL.h'))) {
                if ($Suite -ne 'all') { throw 'Input benchmark requires SDL3_DIR.' }
                Write-Host 'Skipping input benchmark: set SDL3_DIR to enable it.'
                continue
            }
            $extra += @('/I', (Join-Path $env:SDL3_DIR 'include'))
            $links += @("/LIBPATH:$(Join-Path $env:SDL3_DIR 'lib\x64')", 'SDL3.lib')
            Copy-Item -LiteralPath (Join-Path $env:SDL3_DIR 'lib\x64\SDL3.dll') -Destination $outputRoot -Force
        }
        $objects += Build-Object "benchmarks\bench_$name.$extension" "bench_$name" $extra
        $executable = Join-Path $outputRoot "bench_$name.exe"
        & link /nologo /LTCG "/OUT:$executable" @objects @links
        if ($LASTEXITCODE -ne 0) { throw "Could not link $name benchmark." }
        $inputs = @((Get-ChildItem (Join-Path $sourceRoot 'src') -Filter '*.h' -File).FullName)
        $inputs += @((Join-Path $repo 'benchmarks\bench_common.h'), (Join-Path $repo 'benchmarks\bench_sink.c'),
            (Join-Path $repo "benchmarks\bench_$name.$extension"))
        if ($name -eq 'sprintf') {
            $inputs += @((Join-Path $repo 'benchmarks\bench_sprintf_backend.c'),
                (Join-Path $sourceRoot 'src\asm\sprintf\win_x64\rg_sprintf_asm_x64.asm'))
        }
        if ($name -eq 'input') { $inputs += Join-Path $outputRoot 'SDL3.dll' }
        $fingerprints = @($inputs | ForEach-Object { @{ path=$_; sha256=(Get-FileHash -LiteralPath $_ -Algorithm SHA256).Hash } })
        @{
            suite=$name; source=$sourceRoot; flags=($flags + $extra); links=$links;
            compiler=(Get-Command cl).Source; compiler_version=(Get-Item (Get-Command cl).Source).VersionInfo.FileVersion;
            dependencies=$env:RG_BENCH_DEPS; sdl=$env:SDL3_DIR; inputs=$fingerprints;
            executable_hash=(Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash;
            timestamp_utc=[DateTime]::UtcNow.ToString('o')
        } | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $outputRoot "build-$name.json") -Encoding UTF8
        if (!$env:RG_BENCH_BUILD_ONLY) {
            & $executable
            if ($LASTEXITCODE -ne 0) { throw "$name benchmark failed." }
        }
    }
    @{
        source = $sourceRoot; output = $outputRoot; flags = $flags; suites = $suites
        compiler = (Get-Command cl).Source; timestamp_utc = [DateTime]::UtcNow.ToString('o')
    } | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $outputRoot 'build.json') -Encoding UTF8
} finally { Pop-Location }
