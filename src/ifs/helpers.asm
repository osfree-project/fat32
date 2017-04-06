.386p

;SAS_SEL equ 70h
ERROR_NOT_SUPPORTED        equ 50
ERROR_INVALID_PARAMETER    equ 87
ERROR_PROTECTION_VIOLATION equ 115

DevHlp_AllocGDTSelector    equ 2dh
DevHlp_FreeGDTSelector     equ 53h
DevHlp_LinToGDTSelector    equ 5ch

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
    mov     eax, ss
    mov     es, ax
    lea     edi, sel                           ; &sel in ES:DI
    mov     ecx, 1                               ; one selector
    mov     dl, DevHlp_AllocGDTSelector
    ;mov     eax, ds:_Device_Help
    mov     eax, offset DGROUP:_Device_Help
    call    far ptr [eax]
endm

;;
; Free GDT selector
;
; @param   sel     Selector esp offset
;
FreeGdtSel macro sel
    mov     ax, [sel]                            ; sel
    mov     dl, DevHlp_FreeGDTSelector
    ;mov     eax, ds:_Device_Help
    mov     eax, offset DGROUP:_Device_Help
    call    far ptr [eax]
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
    ;mov     eax, ds:_Device_Help
    mov     eax, offset DGROUP:_Device_Help
    call    far ptr [eax]
endm

extern KernThunkStackTo16 :near
extern KernThunkStackTo32 :near
extern _Device_Help       :near
extern _TKSSBase          :near
extern Dos32FlatDS        :near

extern FS_READ            :far
extern FS_WRITE           :far
extern FS_CHGFILEPTR      :far
extern FS_CHGFILEPTRL     :far

CONST segment word public 'DATA' use16
CONST ends

CONST2 segment word public 'DATA' use16
CONST2 ends

_DATA segment word public 'DATA' use16
_DATA ends

_BSS  segment word public 'BSS' use16
_BSS  ends

DGROUP group CONST, CONST2, _DATA, _BSS

_TEXT segment word public 'CODE' use16

;public _SaSSel

;SaSSel PROC
;    mov ax,SAS_SEL
;    ret
;SaSSel ENDP

_TEXT ends

_TEXT32 segment word public 'CODE' use32

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

      assume cs:_TEXT
      assume ds:FLAT, es:FLAT

      ;assume cs:FLAT
      ;assume ds:FLAT, es:FLAT

      push ebp
      mov  ebp, esp

      sub  esp, 10h

      int  3
      SaveCRegs

      call KernThunkStackTo16

      jmp  far ptr FS32_READ_16

_TEXT segment

FS32_READ_16:

      ;assume cs:_TEXT
      ;assume ds:DGROUP, es:DGROUP

      ;assume  ds:DGROUP
      ;assume  es:DGROUP

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
      ;jmp far ptr _TEXT32:FS32_READ_32

_TEXT ends

FS32_READ_32:

      ;assume cs:FLAT
      ;assume ds:FLAT, es:FLAT

      mov  ax, offset Dos32FlatDS
      mov  ds, ax
      mov  es, ax

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

_DATA segment word public 'DATA' use16

InitFlag DW 1

_DATA ends

end
