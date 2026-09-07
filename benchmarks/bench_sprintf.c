// Formatter comparison driver. Each backend is compiled in its own translation
// unit from bench_sprintf_backend.c; see docs/rg_sprintf.md for methodology.
void rg_bench_sprintf_c(int argc, char** argv);
void rg_bench_sprintf_asm(int argc, char** argv);
#if defined(RG_BENCH_SPRINTF_STB)
void rg_bench_sprintf_stb(int argc, char** argv);
#endif

int main(int argc, char** argv)
{
	rg_bench_sprintf_c(argc, argv);
	rg_bench_sprintf_asm(argc, argv);
#if defined(RG_BENCH_SPRINTF_STB)
	rg_bench_sprintf_stb(argc, argv);
#endif
	return 0;
}
