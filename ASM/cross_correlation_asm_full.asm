segment .text
global cross_correlation_asm_full


;List of parameters and their addresses
;[esp + 8]   Arr1
;[esp + 12]  Len1
;[esp + 16]  Arr2
;[esp + 20]  Len2
;[esp + 24]  Output
;[esp + 28]  LenOut

cross_correlation_asm_full:
	push ebp
	mov  ebp,esp
	
	mov dword [ebp - 20], 0 ;Count non zero
	mov eax, [ebp + 12]     ;Indexstart = size1
	dec eax
	
While:
	;Loop condition (outputSize != 0) I will mutate the original value
	push esi 
	mov esi, [ebp + 28]
	cmp esi, 0
	jz  BreakW
	pop esi

	mov esi, eax		;int current = Indexstart
	xor ebx, ebx		;int sum = 0
	xor ecx, ecx		;int i = 0
For:
	;loop condition
	cmp ecx, [ebp + 20]	;if(size2 > i)
	jz  BreakF
	;if condition (current < size1 && current > -1)
	cmp esi, [ebp + 12] 
	jge SkipMul
	cmp esi, 0
	jl SkipMul
	
	;Save register values
	mov [ebp - 8], eax
	mov [ebp - 12], edx
	mov [ebp - 16], edi
	
	;Perform Multipication
	mov edi, [ebp + 16]
	mov eax, [edi + ecx * 4]
	mov edi, [ebp + 8]
	mov edx, [edi + esi * 4]
	mul edx
	add ebx, eax
	
	;Load register values
	mov eax, [ebp - 8]
	mov edx, [ebp - 12]
	mov edi, [ebp - 16]
	
SkipMul:
	inc esi
	inc ecx
	jmp For
	
BreakF:
	;Copy sum to output array and count if not zero
	mov edx, [ebp + 28]
	dec edx
	mov [ebp + 28], edx
	mov edi, [ebp + 24]
	cmp ebx, 0
	jz  SkipInc
	inc dword [ebp - 20]
SkipInc:
	mov [edi + edx*4], ebx
	dec eax
	jmp While 
    
BreakW:
	;Prepare for return
	pop esi
	mov eax, [ebp - 20]
	pop  ebp
	ret

