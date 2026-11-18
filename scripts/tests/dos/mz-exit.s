# 8086 real mode. DOS relocates the embedded segment word to our load segment.
.text
.code16
start:
    movw %cs, %ax
    movw %cs:(segment_word-start), %dx
    cmpw %dx, %ax
    jne failed
    movw $0x4c2a, %ax
    int $0x21
failed:
    movw $0x4c01, %ax
    int $0x21
segment_word:
    .word 0

.section .mzrel,"dr"
    .word segment_word-start
