.486p

extrn    _strategy   :near

public   _strat
public   _code_end
public   _data_end
public   _pIORBHead

_DATA segment byte public 'DATA' use16

_data_end dw offset data_end
_code_end    dw offset code_end
_pIORBHead   dd 0

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


                public  _notify_hook
_notify_hook    proc    far
                pushad
                push    ds
                push    es
                xor     eax,eax
                xchg    _pIORBHead,eax
                or      eax,eax
                jz      @@nh_exit
                mov     bp,sp
                push    eax
                push    eax
@@nh_loop:
                les     di,[bp-8]
                mov     eax,es:[di+18h]
                mov     [bp-4],eax
                call    far ptr es:[di+1Ch]

                mov     eax,[bp-4]
                mov     [bp-8],eax
                or      eax,eax
                jnz     @@nh_loop
                mov     sp,bp
@@nh_exit:
                pop     es
                pop     ds
                popad
		        ret
_notify_hook    endp

code_end label word

_TEXT ends

    end