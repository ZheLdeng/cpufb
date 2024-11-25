.global multiple_issue

multiple_issue:

    mov %rdx, %r10
.multiple.issue.L2:
    mov %rdi, %r8
    mov %rsi, %r9
.multiple.issue.L1:

    movups (%r8), %xmm0
    imul %r11d, %r11d
    mulps %xmm8, %xmm8
    

    sub $0x1, %r9
    jne .multiple.issue.L1
    
    sub $0x1, %r10
    jne .multiple.issue.L2
    ret


