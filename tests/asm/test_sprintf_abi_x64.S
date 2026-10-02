# Call the integer helpers with legal int arguments whose upper register bits
# are nonzero. The fifth argument supplies those bits, independent of compiler
# optimization or argument-extension choices.
.intel_syntax noprefix
.text

.globl rg_test_utoa_dirty_digits
.type rg_test_utoa_dirty_digits, @function
rg_test_utoa_dirty_digits:
	.cfi_startproc
	mov edx, edx
	shl r8, 32
	or rdx, r8
	jmp rg_utoa_asm@PLT
	.cfi_endproc
.size rg_test_utoa_dirty_digits, .-rg_test_utoa_dirty_digits

.globl rg_test_u64toa_dirty_digits
.type rg_test_u64toa_dirty_digits, @function
rg_test_u64toa_dirty_digits:
	.cfi_startproc
	mov edx, edx
	shl r8, 32
	or rdx, r8
	jmp rg_u64toa_asm@PLT
	.cfi_endproc
.size rg_test_u64toa_dirty_digits, .-rg_test_u64toa_dirty_digits

.section .note.GNU-stack,"",@progbits
