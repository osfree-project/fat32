.MODEL LARGE,PASCAL
.686p

ERROR_NOT_SUPPORTED        equ 50
ERROR_INVALID_PARAMETER    equ 87
ERROR_PROTECTION_VIOLATION equ 115

DevHlp_AllocGDTSelector    equ 2dh
DevHlp_FreeGDTSelector     equ 53h
DevHlp_LinToGDTSelector    equ 5ch
DevHlp_GetDOSVar           EQU 24h

DHGETDOSV_LOCINFOSEG       EQU 2

InfoSegLDT	STRUC
LIS_CurProcID	DW	?
LIS_ParProcID	DW	?
LIS_CurThrdPri	DW	?
LIS_CurThrdID	DW	?
LIS_CurScrnGrp	DW	?
LIS_ProcStatus	DB	?
LIS_fillbyte1	DB	?
LIS_Fgnd	DW	?
LIS_ProcType	DB	?
LIS_fillbyte2	DB	?
LIS_AX	DW	?
LIS_BX	DW	?
LIS_CX	DW	?
LIS_DX	DW	?
LIS_SI	DW	?
LIS_DI	DW	?
LIS_DS	DW	?
LIS_PackSel	DW	?
LIS_PackShrSel	DW	?
LIS_PackPckSel	DW	?
InfoSegLDT	ENDS

extrn C Device_Help      :dword
extrn FS_READ            :far
extrn FS_WRITE           :far
extrn FS_CHGFILEPTR      :far
extrn FS_CHGFILEPTRL     :far

extrn SYSCALL KernThunkStackTo16 :near
extrn SYSCALL KernThunkStackTo32 :near
extrn C TKSSBase         :dword
extrn SYSCALL Dos32FlatDS :abs



_TEXT segment word public use16 'CODE'
_TEXT ends

CONST segment word public use16 'DATA'
CONST ends

CONST2 segment word public use16 'DATA'
CONST2 ends

_DATA segment word public use16 'DATA'
_DATA ends

_BSS  segment word public use16 'BSS'
_BSS  ends

DGROUP group CONST, CONST2, _DATA, _BSS

_TEXT segment word public use16 'CODE'
ASSUME ds:DGROUP
ASSUME cs:_TEXT

   PUBLIC AcquireLightLock
AcquireLightLock PROC USES esi edi,ControlVar:DWORD
   mov al,DHGETDOSV_LOCINFOSEG
   xor cx,cx
   mov dl,DevHlp_GetDOSVar
   call DWORD PTR Device_Help
   mov es,ax
   les bx,es:[bx]
   mov si,es:[bx]+InfoSegLDT.LIS_CurProcID
   shl esi,010h
   mov si,es:[bx]+InfoSegLDT.LIS_CurThrdID
   
   les bx,ControlVar

@@:
   xor eax,eax                   ; check for eax = 0 = semaphore not yet claimed
   lock cmpxchg es:[bx],esi
   mov ecx,eax
   jz short @F                   ; we grabbed the sem if eax was zero

   cmp ecx,esi                   ; ecx (eax) was not zero, is it our own thread ?
   jz short @F                   ; nested call: we again grabbed the sem

   ;pause                        ; was introduced with SSE2 but will not trap on any processor !
   db 0f3h,090h                  ; this pauses the CPU which is more CPU friendly

   jmp short @B                  ; else, go back and try to claim RAM semaphore

@@:
   inc DWORD PTR es:[bx+4]       ; increase nesting count
   xor ax,ax
   ret
AcquireLightLock ENDP



   PUBLIC ReleaseLightLock
ReleaseLightLock PROC USES esi,ControlVar:DWORD
   mov al,DHGETDOSV_LOCINFOSEG
   xor cx,cx
   mov dl,DevHlp_GetDOSVar
   call DWORD PTR Device_Help
   mov es,ax
   les bx,es:[bx]
   mov si,es:[bx]+InfoSegLDT.LIS_CurProcID
   shl esi,010h
   mov si,es:[bx]+InfoSegLDT.LIS_CurThrdID
   
   les bx,ControlVar

   cmp es:[bx],esi
   jnz short @F                  ; don't do anything if we didn't claim the semaphore

   dec DWORD PTR es:[bx+4]       ; decrease nesting count
   jnz short @F                  ; we are still in a nested call, skip doing the final release

   xor eax,eax
   lock xchg es:[bx],eax         ; free the semaphore

@@:
   xor ax,ax
   ret
ReleaseLightLock ENDP
_TEXT ends

SaveReg macro Which
    irp y,<Which>
        push y
    endm
endm

RestReg macro Which
    irp y,<Which>
        pop y
    endm
endm

SaveCRegs macro
    SaveReg <ds,es,ebx,edi,esi>
endm

RestCRegs macro
    RestReg <esi,edi,ebx,es,ds>
endm

;;
; Allocate a GDT selector
;
; @param   sel     selector
;
AllocGdtSel macro sel
    mov     ax, ss
    mov     es, ax
    lea     edi, sel                           ; &sel in ES:DI
    mov     ecx, 1                               ; one selector
    mov     dl, DevHlp_AllocGDTSelector
    call    dword ptr Device_Help
endm

;;
; Free GDT selector
;
; @param   sel     Selector esp offset
;
FreeGdtSel macro sel
    mov     ax, [sel]                            ; sel
    mov     dl, DevHlp_FreeGDTSelector
    call    dword ptr Device_Help
endm

;;
; Map Linear address to a GDT selector
;
; @param   sel     Selector esp offset
; @param   lin     Linear address ebp offset
; @param   size    Size (immediate)
;
; carry flag if unsuccessful
;
LinToGdtSel macro sel, lin, size
    xor     eax, eax
    mov     ax,  sel                            ; sel
    mov     ebx, lin                            ; lin
    mov     ecx, size                           ; size
    mov     dl, DevHlp_LinToGDTSelector
    call    dword ptr Device_Help
endm

_TEXT32 segment word public 'CODE' use32
ASSUME DS:FLAT,ES:FLAT




public FS32_CHGFILEPTR
public FS32_CHGFILEPTRL
public FS32_READ
public FS32_READFILEATCACHE
public FS32_RETURNFILECACHE
public FS32_WRITE

; APIRET APIENTRY
; FS32_CHGFILEPTR(PSFFSI psffsi, PVBOXSFFSD psffsd, LONG off, ULONG ulMethod, ULONG IOflag)
FS32_CHGFILEPTR proc
psffsi   equ <dword ptr ebp + 8>
psffsd   equ <dword ptr ebp + 0ch>
off      equ <dword ptr ebp + 10h>
ulMethod equ <dword ptr ebp + 14h>
IOflag   equ <dword ptr ebp + 18h>

    push ebp
    mov ebp, esp

    push [IOflag]
    push [ulMethod]
    push 0
    push [off]
    push [psffsd]
    push [psffsi]

    call dword ptr FS32_CHGFILEPTRL

    add  esp, 24

    pop  ebp

    ret
FS32_CHGFILEPTR endp

; APIRET APIENTRY
; FS32_CHGFILEPTRL(PSFFSI psffsi, PVBOXSFFSD psffsd, LONGLONG off, ULONG ulMethod, ULONG IOflag)
FS32_CHGFILEPTRL proc
psffsi   equ <dword ptr ebp + 8>
psffsd   equ <dword ptr ebp + 0ch>
off      equ <dword ptr ebp + 10h>
ulMethod equ <dword ptr ebp + 18h>
IOflag   equ <dword ptr ebp + 1ch>

    push ebp
    mov ebp, esp

    ;;;;

    pop ebp

    mov eax, ERROR_NOT_SUPPORTED
    ret
FS32_CHGFILEPTRL endp

; APIRET APIENTRY
; FS32_READ(PSFFSI psffsi, PVBOXSFFSD psffsd, PVOID pvData, PULONG pcb, ULONG IOflag)
FS32_READ proc
; formal parameters
psffsi   equ <dword ptr ebp + 8>
psffsd   equ <dword ptr ebp + 0ch>
pvData   equ <dword ptr ebp + 10h>
pcb      equ <dword ptr ebp + 14h>
IOflag   equ <dword ptr ebp + 18h>
; local variables
psffsi16 equ <dword ptr ebp - 4h>
psffsd16 equ <dword ptr ebp - 8h>
pvData16 equ <dword ptr ebp - 0ch>
pcb16    equ <dword ptr ebp - 10h>

      push ebp
      mov  ebp, esp

      sub  esp, 10h

      int  3
      SaveCRegs

      call KernThunkStackTo16

      jmp  far ptr _TEXT:FS32_READ_16

_TEXT32 ends
_TEXT segment word public use16 'CODE'

FS32_READ_16:

      assume cs:_TEXT
      assume ds:DGROUP, es:DGROUP


      mov  ax, seg DGROUP
      mov  ds, ax
      mov  es, ax

      ;int 3
      ;AllocGdtSel <word ptr [ebp - 2h]>
      ;jnc  @ok1
      ;mov  ebx, ERROR_PROTECTION_VIOLATION
      ;jmp  @exit2
@ok1:
      ; map pData FLAT addr to an allocated selector
      ;LinToGdtSel  <word ptr [ebp - 2h]>, psffsi, 50 ; sizeof(struct sffsi)
      ;jnc   @ok3
      ;mov   ebx, ERROR_PROTECTION_VIOLATION
      ;jmp   @exit1
@ok3:

      ;push [psffsi]
      ;push [psffsd]
      ;push [pvData]
      ;push [pcb]
      ;push [IOflag]

      ;call far ptr FS_READ

      ; save return code
      xor   ebx, ebx
      mov   bx, ax

      ; -18 is because of "ret 8" command at the end of last function
      ;;;;sub   esp, 18

@exit1:
      ; free GDT selectors
      ;FreeGdtSel <word ptr [ebp - 2h]>
@exit2:

      ;;;;add   esp, 18

      jmp far ptr FLAT:FS32_READ_32

_TEXT ends

_TEXT32 segment word public 'CODE' use32
ASSUME DS:FLAT,ES:FLAT

FS32_READ_32:

      mov  eax, Dos32FlatDS
      mov  ds, eax
      mov  es, eax

      call KernThunkStackTo32

      RestCRegs    

      add  esp, 10h

      pop  ebp

      mov  eax, ERROR_NOT_SUPPORTED
      ret
FS32_READ endp

; APIRET APIENTRY
; FS32_READFILEATCACHE(PSFFSI psffsi, PVBOXSFFSD psffsd, ULONG IOflag, LONGLONG off, ULONG pcb, KernCacheList_t **ppCacheList)
FS32_READFILEATCACHE proc
    mov eax, ERROR_NOT_SUPPORTED
    ret
FS32_READFILEATCACHE endp

; APIRET APIENTRY
; FS32_RETURNFILECACHE(KernCacheList_t *pCacheList)
FS32_RETURNFILECACHE proc
    mov eax, ERROR_NOT_SUPPORTED
    ret
FS32_RETURNFILECACHE endp

; APIRET APIENTRY
; FS32_WRITE(PSFFSI psffsi, PVBOXSFFSD psffsd, PVOID pvData, PULONG pcb, ULONG IOflag)
FS32_WRITE proc
psffsi   equ <dword ptr ebp + 8>
psffsd   equ <dword ptr ebp + 0ch>
pvData   equ <dword ptr ebp + 10h>
pcb      equ <dword ptr ebp + 14h>
IOflag   equ <dword ptr ebp + 18h>

    push ebp
    mov ebp, esp

    ;;;;

    pop ebp

    mov eax, ERROR_NOT_SUPPORTED
    ret
FS32_WRITE endp

_TEXT32 ends

_DATA segment word public use16 'DATA'

InitFlag DW 1

_DATA ends

end
