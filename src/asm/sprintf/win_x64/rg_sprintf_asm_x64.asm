; rg_sprintf x64 asm helpers

OPTION CASEMAP:NONE

PUBLIC rg_strlen_asm
PUBLIC rg_utoa_asm
PUBLIC rg_u64toa_asm

.code

rg_strlen_asm PROC
	; Check the first unaligned probe, then align all subsequent loads.
	mov rax, rcx
	mov edx, eax
	and edx, 4095
	cmp edx, 4064
	ja scan_page_tail
scan_vectors:
	vpxor ymm0, ymm0, ymm0
	vpcmpeqb ymm1, ymm0, ymmword ptr [rax]
	vpmovmskb edx, ymm1
	test edx, edx
	jnz scan_found
	add rax, 32
	and rax, -32
ALIGN 16
scan_loop:
	; Aligned loads stay within a page, including the final string block.
	vpcmpeqb ymm1, ymm0, ymmword ptr [rax]
	vpmovmskb edx, ymm1
	test edx, edx
	jnz scan_found
	add rax, 32
	jmp scan_loop
scan_found:
	bsf edx, edx
	add rax, rdx
	sub rax, rcx
	vzeroupper
	ret
scan_page_tail:
	cmp byte ptr [rax], 0
	je scan_scalar_found
	inc rax
	test al, 31
	jnz scan_page_tail
	jmp scan_vectors
scan_scalar_found:
	sub rax, rcx
	ret
rg_strlen_asm ENDP

; Keep the existing integer helpers at their original offsets.
ALIGN 4

; char* rg_utoa_asm(uint32_t value, char* buf, int digits, const char* digit_pairs)
rg_utoa_asm PROC
	; rcx = value
	; rdx = buf
	; r8d = digits
	; r9  = digit_pairs
	mov eax, ecx
	lea r10, [rdx + r8]
	mov r11, r10
	mov byte ptr [r10], 0

rg_utoa_loop4:
	cmp eax, 10000
	jb rg_utoa_loop2

	mov r8d, eax
	mov ecx, 0D1B71759h
	mul ecx
	mov ecx, edx
	shr ecx, 13

	imul edx, ecx, 10000
	sub r8d, edx

	sub r10, 4

	mov eax, r8d
	imul rax, rax, 051EB851Fh
	shr rax, 37
	mov edx, eax
	imul eax, edx, 100
	sub r8d, eax

	lea eax, [rdx*2]
	mov ax, word ptr [r9 + rax]
	mov word ptr [r10], ax
	lea eax, [r8d*2]
	mov ax, word ptr [r9 + rax]
	mov word ptr [r10 + 2], ax

	mov eax, ecx
	jmp rg_utoa_loop4

rg_utoa_loop2:
	cmp eax, 100
	jb rg_utoa_tail

	mov ecx, eax
	imul rax, rax, 051EB851Fh
	shr rax, 37
	mov edx, eax
	imul eax, edx, 100
	sub ecx, eax

	sub r10, 2
	lea eax, [rcx*2]
	mov ax, word ptr [r9 + rax]
	mov word ptr [r10], ax

	mov eax, edx
	jmp rg_utoa_loop2

rg_utoa_tail:
	cmp eax, 10
	jb rg_utoa_one
	sub r10, 2
	lea eax, [rax*2]
	mov ax, word ptr [r9 + rax]
	mov word ptr [r10], ax
	jmp rg_utoa_done

rg_utoa_one:
	sub r10, 1
	add al, '0'
	mov byte ptr [r10], al

rg_utoa_done:
	mov rax, r11
	ret
rg_utoa_asm ENDP

; char* rg_u64toa_asm(uint64_t value, char* buf, int digits, const char* digit_quads)
rg_u64toa_asm PROC
	push r15

	mov rax, rcx
	lea r10, [rdx + r8]
	mov r11, r10
	mov byte ptr [r10], 0

rg_u64_loop:
	cmp rax, 1000000000
	jb rg_u64_tail

	; Exact unsigned division by 10^9: remove its factor of 2^9 first,
	; then multiply by ceil(2^75 / 1953125) and take the high bits.
	mov r8d, eax
	shr rax, 9
	mov rcx, 0044B82FA09B5A53h
	mul rcx
	shr rdx, 11
	mov r15, rdx
	; The remainder is below 10^9, so low-32-bit subtraction is sufficient.
	imul edx, edx, 1000000000
	sub r8d, edx
	sub r10, 9

	mov eax, r8d
	mov ecx, 02AF31DC5h
	mul ecx
	shr edx, 24
	imul ecx, edx, 100000000
	sub r8d, ecx

	add dl, '0'
	mov byte ptr [r10], dl

	mov eax, r8d
	mov ecx, 0D1B71759h
	mul ecx
	shr edx, 13
	imul eax, edx, 10000
	sub r8d, eax

	mov eax, dword ptr [r9 + rdx*4]
	mov dword ptr [r10 + 1], eax
	mov eax, dword ptr [r9 + r8*4]
	mov dword ptr [r10 + 5], eax

	mov rax, r15
	jmp rg_u64_loop

rg_u64_tail:
rg_u64_tail_loop:
	cmp eax, 100
	jb rg_u64_tail_small

	mov r8d, eax
	mov ecx, 051EB851Fh
	mul ecx
	mov ecx, edx
	shr ecx, 5
	imul edx, ecx, 100
	sub r8d, edx

	sub r10, 2
	lea edx, [r8d*4 + 2]
	mov ax, word ptr [r9 + rdx]
	mov word ptr [r10], ax

	mov eax, ecx
	jmp rg_u64_tail_loop

rg_u64_tail_small:
	cmp eax, 10
	jb rg_u64_tail_one
	sub r10, 2
	lea edx, [rax*4 + 2]
	mov ax, word ptr [r9 + rdx]
	mov word ptr [r10], ax
	jmp rg_u64_done

rg_u64_tail_one:
	sub r10, 1
	add al, '0'
	mov byte ptr [r10], al

rg_u64_done:
	mov rax, r11
	pop r15
	ret
rg_u64toa_asm ENDP

; Preserve the original helper block footprint. Shifting the following code
; regresses unrelated formatter paths even though this conversion is faster.
DB 41 DUP (0CCh)

END
