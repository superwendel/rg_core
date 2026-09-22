.intel_syntax noprefix
.text

.globl rg_strlen_asm
.type rg_strlen_asm, @function
rg_strlen_asm:
	mov rax, rdi
	vpxor ymm0, ymm0, ymm0

	mov r8, rax
	and r8, -4096
	add r8, 4096
	lea r9, [r8 - 32]

.Lstrlen_loop:
	cmp rax, r9
	ja .Lstrlen_tail

	vmovdqu ymm1, ymmword ptr [rax]
	vpcmpeqb ymm1, ymm1, ymm0
	vpmovmskb edx, ymm1
	test edx, edx
	jnz .Lstrlen_found
	add rax, 32
	jmp .Lstrlen_loop

.Lstrlen_tail:
	cmp rax, r8
	jae .Lstrlen_next_page
	cmp byte ptr [rax], 0
	je .Lstrlen_found_byte
	inc rax
	jmp .Lstrlen_tail

.Lstrlen_next_page:
	add r8, 4096
	lea r9, [r8 - 32]
	jmp .Lstrlen_loop

.Lstrlen_found:
	bsf edx, edx
	add rax, rdx

.Lstrlen_found_byte:
	sub rax, rdi
	vzeroupper
	ret
.size rg_strlen_asm, .-rg_strlen_asm

.globl rg_utoa_asm
.type rg_utoa_asm, @function
rg_utoa_asm:
	mov r9, rcx
	mov eax, edi
	lea r10, [rsi + rdx]
	mov r11, r10
	mov byte ptr [r10], 0

.Lutoa_loop4:
	cmp eax, 10000
	jb .Lutoa_loop2

	mov r8d, eax
	mov ecx, 0xD1B71759
	mul ecx
	mov ecx, edx
	shr ecx, 13

	imul edx, ecx, 10000
	sub r8d, edx

	sub r10, 4

	mov eax, r8d
	imul rax, rax, 0x51EB851F
	shr rax, 37
	mov edx, eax
	imul eax, edx, 100
	sub r8d, eax

	lea eax, [rdx * 2]
	mov ax, word ptr [r9 + rax]
	mov word ptr [r10], ax
	lea eax, [r8d * 2]
	mov ax, word ptr [r9 + rax]
	mov word ptr [r10 + 2], ax

	mov eax, ecx
	jmp .Lutoa_loop4

.Lutoa_loop2:
	cmp eax, 100
	jb .Lutoa_tail

	mov ecx, eax
	imul rax, rax, 0x51EB851F
	shr rax, 37
	mov edx, eax
	imul eax, edx, 100
	sub ecx, eax

	sub r10, 2
	lea eax, [rcx * 2]
	mov ax, word ptr [r9 + rax]
	mov word ptr [r10], ax

	mov eax, edx
	jmp .Lutoa_loop2

.Lutoa_tail:
	cmp eax, 10
	jb .Lutoa_one
	sub r10, 2
	lea eax, [rax * 2]
	mov ax, word ptr [r9 + rax]
	mov word ptr [r10], ax
	jmp .Lutoa_done

.Lutoa_one:
	sub r10, 1
	add al, '0'
	mov byte ptr [r10], al

.Lutoa_done:
	mov rax, r11
	ret
.size rg_utoa_asm, .-rg_utoa_asm

.globl rg_u64toa_asm
.type rg_u64toa_asm, @function
rg_u64toa_asm:
	push r15

	mov r9, rcx
	mov rax, rdi
	lea r10, [rsi + rdx]
	mov r11, r10
	mov byte ptr [r10], 0

.Lu64_loop:
	cmp rax, 1000000000
	jb .Lu64_tail

	# Exact unsigned division by 10^9: remove its factor of 2^9 first,
	# then multiply by ceil(2^75 / 1953125) and take the high bits.
	mov r8d, eax
	shr rax, 9
	mov rcx, 0x0044B82FA09B5A53
	mul rcx
	shr rdx, 11
	mov r15, rdx
	# The remainder is below 10^9, so low-32-bit subtraction is sufficient.
	imul edx, edx, 1000000000
	sub r8d, edx
	sub r10, 9

	mov eax, r8d
	mov ecx, 0x2AF31DC5
	mul ecx
	shr edx, 24
	imul ecx, edx, 100000000
	sub r8d, ecx

	add dl, '0'
	mov byte ptr [r10], dl

	mov eax, r8d
	mov ecx, 0xD1B71759
	mul ecx
	shr edx, 13
	imul eax, edx, 10000
	sub r8d, eax

	mov eax, dword ptr [r9 + rdx * 4]
	mov dword ptr [r10 + 1], eax
	mov eax, dword ptr [r9 + r8 * 4]
	mov dword ptr [r10 + 5], eax

	mov rax, r15
	jmp .Lu64_loop

.Lu64_tail:
.Lu64_tail_loop:
	cmp eax, 100
	jb .Lu64_tail_small

	mov r8d, eax
	mov ecx, 0x51EB851F
	mul ecx
	mov ecx, edx
	shr ecx, 5
	imul edx, ecx, 100
	sub r8d, edx

	sub r10, 2
	lea edx, [r8 * 4 + 2]
	mov ax, word ptr [r9 + rdx]
	mov word ptr [r10], ax

	mov eax, ecx
	jmp .Lu64_tail_loop

.Lu64_tail_small:
	cmp eax, 10
	jb .Lu64_tail_one
	sub r10, 2
	lea edx, [rax * 4 + 2]
	mov ax, word ptr [r9 + rdx]
	mov word ptr [r10], ax
	jmp .Lu64_done

.Lu64_tail_one:
	sub r10, 1
	add al, '0'
	mov byte ptr [r10], al

.Lu64_done:
	mov rax, r11
	pop r15
	ret
.size rg_u64toa_asm, .-rg_u64toa_asm

.section .note.GNU-stack,"",@progbits
