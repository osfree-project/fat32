.MODEL LARGE,C
.386p

PUBLIC SaSSel
;EXTRN PASCAL SAS_SEL:ABS
;EXTRN C Device_Help:DWORD

SAS_SEL equ 70h

.DATA
InitFlag DW 1

.CODE
SaSSel PROC
    mov ax,SAS_SEL
    ret
SaSSel ENDP

END
