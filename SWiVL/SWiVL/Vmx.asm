
IA32_FEATURE_CONTROL    equ 00000003ah
IA32_VMX_BASIC          equ 000000480h
IA32_VMX_CR0_FIXED0     equ 000000486h
IA32_VMX_CR0_FIXED1     equ 000000487h
IA32_VMX_CR4_FIXED0     equ 000000488h
IA32_VMX_CR4_FIXED1     equ 000000489h

STATUS_UNSUCCESSFUL         equ 0c0000001h
STATUS_NOT_IMPLEMENTED      equ 0c0000002h
STATUS_INVALID_DEVICE_STATE equ 0c0000184h
STATUS_RESOURCE_IN_USE      equ 0c0000708h

_text SEGMENT

VmxGetVmcsIdentifier PROC
    mov     ecx, IA32_VMX_BASIC
    rdmsr
    ; clear shadow bit
    btr     eax, 31
    ret
VmxGetVmcsIdentifier ENDP

VmxEnable PROC FRAME
    ; save arg
    push    rcx
.PUSHREG    rcx
    ; save nonvolatile rbx
    push    rbx
.PUSHREG    rbx

.ENDPROLOG

    ; first, ensure VMX is supported
    mov     eax, 1
    cpuid
    test    cl, 020h
    jnz     vmxSupported
    ; not supported
    mov     eax, STATUS_NOT_IMPLEMENTED
    pop     rbx
    pop     rcx
    ret

vmxSupported:
    ; ensure the IA32_FEATURE_CONTROL MSR is setup correctly
    mov     ecx, IA32_FEATURE_CONTROL
    rdmsr
    test    cl, 001h
    jnz     msrSetUp
    ; program the MSR
    or      al, 007h ; enable VMXON inside/outside SMX operation, set lock bit
    wrmsr
msrSetUp:

    ; test required bits
    mov     rbx, cr0
    mov     ecx, IA32_VMX_CR0_FIXED1
    rdmsr
    shl     rdx, 16
    or      rdx, rax
    not     rdx
    ; all bits set in rdx (~FIXED1) must also be clear in rbx
    test    rbx, rdx
    jnz     badBits

    not     rbx
    mov     ecx, IA32_VMX_CR0_FIXED0
    rdmsr
    shl     rdx, 16
    or      rdx, rax
    ; all bits set in rdx must be clear in rbx (~cr0)
    test    rbx, rdx
    jnz     badBits

    mov     rbx, cr4
    mov     ecx, IA32_VMX_CR4_FIXED1
    rdmsr
    shl     rdx, 16
    or      rdx, rax
    not     rdx
    ; all bits set in rdx (~FIXED1) must also be clear in rbx
    test    rbx, rdx
    jnz     badBits

    not     rbx
    mov     ecx, IA32_VMX_CR4_FIXED0
    rdmsr
    shl     rdx, 16
    or      rdx, rax
    ; ignore VMX bit
    btr     rdx, 13
    ; all bits set in rdx must be clear in rbx (~cr4)
    test    rbx, rdx
    jnz     badBits
    jmp     goodBits

badBits:
    mov     eax, STATUS_INVALID_DEVICE_STATE
    pop     rbx
    pop     rcx
    ret
goodBits:

    ; set VMX bit
    mov     rax, cr4
    bts     rax, 13
    jnc     vmxEnabled
    ; already enabled
    mov     eax, STATUS_RESOURCE_IN_USE
    pop     rbx
    pop     rcx
    ret
vmxEnabled:
    mov     cr4, rax

    ; grab arg
    mov     rax, QWORD PTR [rsp + 8]
    ; enter VMX operation
    vmxon   QWORD PTR [rax]
    jnc     success
    ; something went wrong
    mov     eax, STATUS_UNSUCCESSFUL
    pop     rbx
    pop     rcx
    ret
success:

    xor     eax, eax
    pop     rbx
    pop     rcx
    ret
VmxEnable ENDP

VmxDisable PROC
    ; leave VMX operation
    vmxoff

    ; clear VMX bit
    mov     rax, cr4
    btr     rax, 13
    mov     cr4, rax

    ret
VmxDisable ENDP

_text ENDS

END
