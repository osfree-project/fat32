.486p

extrn    DOS32FLATDS :abs
extrn    _strategy   :near
;extrn    _IoHook     :near

public   _strat
public   _code_end
public   _data_end
public   _pIORBHead
public   _DS

_DATA segment byte public 'DATA' use16

_data_end  dw  offset data_end
_code_end  dw  offset code_end
_FlatDS    dw  DOS32FLATDS
_DS        dw  DGROUP
_pIORBHead dd  0

_DATA ends

CONST segment byte public 'DATA' use16
CONST ends

CONST2 segment byte public 'DATA' use16
CONST2 ends

_BSS segment byte public 'BSS' use16
data_end label word
_BSS ends

DGROUP group _DATA, CONST, CONST2, _BSS

_TEXT segment byte public 'CODE' use16

_strat proc far
    pusha

    push    ds

    push    es
    push    bx

    call    _strategy
    add     sp, 4

    pop     ds

    popa

    retf
_strat endp

                public  _put_IORB
_put_IORB       proc    near
                push    bp
                mov     bp,sp
                push    es
                push    di
                les     di,[bp+4]
                mov     ecx,[bp+4]
@@putIORB_loop:
                mov     eax,_pIORBHead
                mov     es:[di+18h],eax
           lock cmpxchg _pIORBHead,ecx
                jnz     @@putIORB_loop

                pop     di
                pop     es
                pop     bp
                ret
_put_IORB       endp

                public  _get_IORB
_get_IORB       proc    near
                push    bp
                mov     bp, sp

                push    es
                push    di

                mov     eax, [bp+4]

                test    eax, eax
                jz      @@getIORB_exit

                les     di, [bp+4]
                mov     eax, es:[di+18h]

           lock xchg    _pIORBHead, eax

@@getIORB_exit:
                mov     edx, eax
                shr     edx, 16

                pop     di
                pop     es

                pop     bp
                ret
_get_IORB       endp

                public _memlcpy
; void memlcpy(LIN lDst, LIN lSrc, ULONG numBytes)
_memlcpy        proc   near
lDst    equ  dword ptr [bp + 4]
lSrc    equ  dword ptr [bp + 8]
nBytes  equ  dword ptr [bp + 0ch]

                push    bp
                mov     bp,  sp

                push    esi
                push    edi
                push    ds

                mov     ax,  _FlatDS
                mov     es,  ax
                mov     ds,  ax
                mov     esi, lSrc
                mov     edi, lDst
                mov     ecx, nBytes
                mov     bl,  cl
                shr     ecx, 2
                cld
            rep movs    dword ptr es:[edi], dword ptr ds:[esi]
                mov     cl,  bl
                and     ecx, 3
            rep movs    byte ptr es:[edi],  byte  ptr ds:[esi]

                pop     ds
                pop     edi
                pop     esi

                pop     bp

                ret 
_memlcpy        endp

code_end label word

_TEXT ends

      end
