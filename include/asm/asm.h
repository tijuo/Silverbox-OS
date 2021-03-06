.ifndef ASM_H
.set ASM_H, 1

.macro EXPORT symbol
.ifdef ASM_USCORE
.global _\symbol
_\symbol:
.else
.global \symbol
\symbol:
.endif
.endm

.macro IMPORT symbol
.ifdef ASM_USCORE
.extern _\symbol
.else
.extern \symbol
.endif
.endm

.endif
