// First-include selection and logger interoperability in C.
// RG_SPRINTF_INCLUDE_ORDER: 0 = portable/log, 1 = log/portable,
//                          2 = ASM/log,      3 = log/ASM.
// RG_SPRINTF_EXPECT_ASM: expected selected header, independent of helper use.
#ifndef RG_SPRINTF_INCLUDE_ORDER
#define RG_SPRINTF_INCLUDE_ORDER 0
#endif
#ifndef RG_SPRINTF_EXPECT_ASM
#define RG_SPRINTF_EXPECT_ASM 0
#endif
#define RG_LOG_NO_COLOR

#if RG_SPRINTF_INCLUDE_ORDER == 0
#include "../src/rg_sprintf.h"
#include "../src/rg_log.h"
#elif RG_SPRINTF_INCLUDE_ORDER == 1
#include "../src/rg_log.h"
#include "../src/rg_sprintf.h"
#elif RG_SPRINTF_INCLUDE_ORDER == 2
#include "../src/rg_sprintf_asm.h"
#include "../src/rg_log.h"
#elif RG_SPRINTF_INCLUDE_ORDER == 3
#include "../src/rg_log.h"
#include "../src/rg_sprintf_asm.h"
#else
#error Invalid RG_SPRINTF_INCLUDE_ORDER
#endif

// Later direct and hybrid includes must all leave the chosen backend intact.
#include "../src/rg_sprintf.h"
#include "../src/rg_sprintf_asm.h"
#include "../src/rg_sprintf_hybrid.h"

#if RG_SPRINTF_EXPECT_ASM
#if !defined(RG_SPRINTF_ASM_H) || defined(RG_SPRINTF_H)
#error Expected the ASM header to retain ownership
#endif
#else
#if !defined(RG_SPRINTF_H) || defined(RG_SPRINTF_ASM_H)
#error Expected the portable header to retain ownership
#endif
#endif

int main(void)
{
	char buffer[64];
	RgBuilder builder;
	int count = rg_snprintf(buffer, sizeof(buffer), "slice=%.*s value=%d", 4, "abcd-tail", 42);
	if (count != 19 || strcmp(buffer, "slice=abcd value=42") != 0)
		return 1;
	rg_builder_init(&builder, buffer, sizeof(buffer));
	rg_builder_appendf(&builder, "%d", 42);
	if (rg_builder_finish(&builder) != 2 || strcmp(buffer, "42") != 0)
		return 1;
	rg_log_init(RG_LOG_NONE);
	RG_INFO("filtered %s", buffer);
	puts("Formatter include-order checks passed.");
	return 0;
}
