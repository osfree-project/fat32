 #ifndef __DEVHELP_H__
 #define __DEVHELP_H__

 #ifdef __cplusplus
 extern "C" {
 #endif

 #define MSG_MEMORY_ALLOCATION_FAILED 0x00
 #define ERROR_LID_ALREADY_OWNED      0x01
 #define ERROR_LID_DOES_NOT_EXIST     0x02
 #define ERROR_ABIOS_NOT_PRESENT      0x03
 #define ERROR_NOT_YOUR_LID           0x04
 #define ERROR_INVALID_ENTRY_POINT    0x05

 #ifdef __WATCOMC__ 

 #ifndef OS2_INCLUDED
 # define INCL_NOPMAPI
 # include <os2.h>
 #endif

 typedef VOID  FAR   *PVOID;
 typedef PVOID FAR   *PPVOID;

 typedef ULONG LIN;
 typedef ULONG FAR   *PLIN;

 extern ULONG Device_Help;
 extern USHORT DGROUP;

 #define DEVICE_HELP \
    "call    dword ptr ds:[Device_Help]" \
    "jc      error" \
    "sub     ax, ax" \
    "error:" \
    value [ax]

 //
 // memory operations (alloc/free/etc)
 //

 APIRET DevHelp_AllocGDTSelector(
    PSEL pSels,
    USHORT cbSels
 );

 #pragma aux DevHelp_AllocGDTSelector = \
    "mov     dl, 2Dh" \
    DEVICE_HELP \
    parm caller [es di] [cx] \
    modify nomemory exact [ax dl];

 #define MEMTYPE_ABOVE_1M   0
 #define MEMTYPE_BELOW_1M   1

 APIRET DevHelp_AllocPhys(
    ULONG ulSize,
    USHORT usMemType,
    PULONG pPhysAddr
 );

 #pragma aux DevHelp_AllocPhys = \
    "mov     dl, 18h" \
    "xchg    ax, bx" \
    "call    dword ptr ds:[Device_Help]" \
    "jc      error" \
    "mov     es:[di], bx" \
    "mov     es:[di+2], ax" \
    "sub     ax, ax" \
    "error:" \
    value [ax] \
    parm caller nomemory [ax bx] [dh] [es di] \
    modify exact [ax bx dx];

 APIRET DevHelp_FreePhys(
    ULONG pPhysAddr
 );

 #pragma aux DevHelp_FreePhys = \
    "mov     dl, 19h" \
    "xchg    ax, bx" \
    "call    dword ptr ds:[Device_Help]" \
    "mov     ax, 0" \
    "sbb     ax, 0" \
    value [ax] \
    parm caller nomemory [ax bx] \
    modify exact [ax bx dl];

 APIRET DevHelp_PhysToGDTSelector(
    ULONG pPhysAddr,
    USHORT usSize,
    SEL Sel
 );

 #pragma aux DevHelp_PhysToGDTSelector = \
    "mov     dl, 2Eh" \
    "xchg    ax, bx" \
    DEVICE_HELP \
    parm nomemory [ax bx] [cx] [si] \
    modify nomemory exact [ax bx dl];

 APIRET DevHelp_PhysToGDTSel(
    ULONG pPhysAddr,
    ULONG ulSize,
    SEL Sel,
    UCHAR cAccess
 );

 #pragma aux DevHelp_PhysToGDTSel = \
    ".386p" \
    "push    bp" \
    "mov     dl, 54h" \
    "mov     bp, sp" \
    "mov     si, [bp+10]" \
    "mov     dh, [bp+12]" \
    "mov     ecx, [bp+6]" \
    "mov     eax, [bp+2]" \
    "call    dword ptr ds:[Device_Help]" \
    "jc      error" \
    "xor     ax, ax" \
    "error:" \
    "pop     bp" \
    value [ax] \
    parm caller nomemory [] \
    modify nomemory exact [ax cx dx si];

 APIRET DevHelp_PhysToVirt(
    ULONG pPhysAddr,
    USHORT usLen,
    PVOID SelOffset,
    PUSHORT pusModeFlag
 );

 #pragma aux DevHelp_PhysToVirt = \
    "xchg    ax, bx" \
    "mov     dx, 15h" \
    "push    ds" \
    "call    dword ptr ds:[Device_Help]" \
    "jc      error" \
    "sub     ax, ax" \
    "mov     es:[di], si" \
    "mov     es:[di+2], ds" \
    "error:" \
    "pop     ds" \
    value [ax] \
    parm caller nomemory [bx ax] [cx] [es di] [] \
    modify exact [ax bx dx si];

 APIRET DevHelp_VirtToPhys(
    PVOID SelOffset,
    PULONG pPhysAddr
 );

 #pragma aux DevHelp_VirtToPhys = \
    "push    ds" \
    "push    es" \
    "mov     dl, 16h" \
    "mov     si, ds" \
    "mov     es, si" \
    "mov     ds, bx" \
    "mov     si, ax" \
    "call    dword ptr ds:[Device_Help]" \
    "pop     es" \
    "mov     es:[di], bx" \
    "mov     es:[di+2], ax" \
    "pop     ds" \
    "xor     ax, ax" \
    value [ax] \
    parm caller nomemory [ax bx] [es di] \
    modify exact [ax bx dl es si];

 #define SELTYPE_R3CODE    0
 #define SELTYPE_R3DATA    1
 #define SELTYPE_FREE      2
 #define SELTYPE_R2CODE    3
 #define SELTYPE_R2DATA    4
 #define SELTYPE_R3VIDEO   5

 APIRET DevHelp_PhysToUVirt(
    ULONG pPhysAddr,
    USHORT usLen,
    USHORT usFlags,
    USHORT usTagType,
    PVOID SelOffset
 );

 #pragma aux DevHelp_PhysToUVirt = \
    "mov     dl, 17h" \
    "xchg    ax, bx" \
    "call    dword ptr ds:[Device_Help]" \
    "jc      error" \
    "push    bx" \
    "push    es" \
    "mov     bx, sp" \
    "les     bx, ss:[bx+4]" \
    "pop     es:[bx+2]" \
    "pop     es:[bx]" \
    "xor     ax, ax" \
    "error:" \
    value [ax] \
    parm caller nomemory [bx ax] [cx] [dh] [si] [] \
    modify exact [ax bx dl es];

 #define LOCKTYPE_SHORT_ANYMEM 0x00
 #define LOCKTYPE_LONG_ANYMEM  0x01
 #define LOCKTYPE_LONG_HIGHMEM 0x03
 #define LOCKTYPE_SHORT_VERIFY 0x04

 APIRET DevHelp_Lock(
    SEL Sel,
    USHORT usLockType,
    USHORT WusaitFlag,
    PULONG hLock
 );

 #pragma aux DevHelp_Lock = \
    "mov     dl, 13h" \
    "call    dword ptr ds:[Device_Help]" \
    "jc      error" \
    "mov     es:[di], bx" \
    "mov     es:[di+2], ax" \
    "xor     ax, ax" \
    "error:" \
    value [ax] \
    parm caller [ax] [bh] [bl] [es di] \
    modify exact [ax dl];

 APIRET DevHelp_UnLock(
    ULONG hLock
 );

 #pragma aux DevHelp_UnLock = \
    "mov     dl, 14h" \
    "xchg    ax, bx" \
    DEVICE_HELP \
    parm nomemory [ax bx] \
    modify nomemory exact [ax bx dl];

 #define VERIFY_READONLY  0
 #define VERIFY_READWRITE 1

 APIRET DevHelp_VerifyAccess(
    SEL Sel,
    USHORT usLen,
    USHORT usMemOffset,
    UCHAR ucAccessFlag
 );

 #pragma aux DevHelp_VerifyAccess = \
    "mov     dl, 27h" \
    DEVICE_HELP \
    parm caller nomemory [ax] [cx] [di] [dh] \
    modify nomemory exact [ax dl];

 #define VMDHA_16M          0x0001
 #define VMDHA_FIXED        0x0002
 #define VMDHA_SWAP         0x0004
 #define VMDHA_CONTIG       0x0008
 #define VMDHA_PHYS         0x0010
 #define VMDHA_PROCESS      0x0020
 #define VMDHA_SGSCONT      0x0040
 #define VMDHA_RESERVE      0x0100
 #define VMDHA_USEHIGHMEM   0x0800
 
 APIRET DevHelp_VMAlloc(
    ULONG ulFlags,
    ULONG ulSize,
    ULONG pPhysAddr,
    PLIN pLinearAddr,
    PPVOID SelOffset
 );

 #pragma aux DevHelp_VMAlloc = \
    ".386p" \
    "mov     dl, 57h" \
    "movzx   esp, sp" \
    "mov     eax, [esp]" \
    "mov     ecx, [esp+4]" \
    "mov     edi, [esp+8]" \
    "call    dword ptr ds:[Device_Help]" \
    "jc      error" \
    "les     di, [esp+16]" \
    "mov     es:[di], ecx" \
    "les     di, [esp+12]" \
    "mov     es:[di], eax" \
    "xor     ax, ax" \
    "error:" \
    value [ax] \
    parm caller nomemory [] \
    modify exact [ax cx di dl es];

 APIRET DevHelp_VMFree(
    LIN LinAddr
 );

 #pragma aux DevHelp_VMFree = \
    ".386p" \
    "mov     dl, 58h" \
    "movzx   esp, sp" \
    "mov     eax, [esp]" \
    DEVICE_HELP \
    parm caller nomemory [] \
    modify nomemory exact [ax dl];

 #define VMDHL_NOBLOCK        0x0001
 #define VMDHL_CONTIGUOUS     0x0002
 #define VMDHL_16M            0x0004
 #define VMDHL_WRITE          0x0008
 #define VMDHL_LONG           0x0010
 #define VMDHL_VERIFY         0x0020

 APIRET DevHelp_VMLock(
    ULONG ulFlags,
    LIN LinAddr,
    ULONG ulLen,
    LIN pPagelist,
    LIN pLockHandle,
    PULONG pulPageListCnt
 );

 #pragma aux DevHelp_VMLock = \
    ".386p" \
    "mov     dl, 55h" \
    "mov     eax, [esp]" \
    "mov     edi, [esp+12]" \
    "mov     esi, [esp+16]" \
    "mov     ebx, [esp+4]" \
    "mov     ecx, [esp+8]" \
    "call    dword ptr ds:[Device_Help]" \
    "jc      error" \
    "les     bx, [esp+20]" \
    "mov     es:[bx], eax" \
    "xor     ax, ax" \
    "error:" \
    value [ax] \
    parm caller nomemory [] \
    modify exact [ax bx cx dl si di es];

 APIRET DevHelp_VMUnLock(
    LIN phLock
 );

 #pragma aux DevHelp_VMUnLock = \
    ".386p" \
    "mov     dl, 56h" \
    "mov     esi, [esp]" \
    DEVICE_HELP \
    parm caller [] \
    modify exact [ax si dl];

 #define VMDHPG_READONLY     0x0000
 #define VMDHPG_WRITE        0x0001

 APIRET DevHelp_VMProcessToGlobal(
    ULONG ulFlags,
    LIN LinAddr,
    ULONG ulLen,
    PLIN pGlobLinAddr
 );

 #pragma aux DevHelp_VMProcessToGlobal = \
    ".386p" \
    "mov     dl, 59h" \
    "mov     eax, [esp]" \
    "mov     ecx, [esp+8]" \
    "mov     ebx, [esp+4]" \
    "call    dword ptr ds:[Device_Help]" \
    "jc      error" \
    "les     bx, [esp+12]" \
    "mov     es:[bx], eax" \
    "xor     ax, ax" \
    "error:" \
    value [ax] \
    parm caller nomemory [] \
    modify exact [ax bx cx dl es];

 #define VMDHGP_WRITE       0x0001
 #define VMDHGP_SELMAP      0x0002
 #define VMDHGP_SGSCONTROL  0x0004
 #define VMDHGP_4MEG        0x0008

 APIRET DevHelp_VMGlobalToProcess(
    ULONG ulFlags,
    LIN LinAddr,
    ULONG ulLen,
    PLIN pProcLinAddr
 );

 #pragma aux DevHelp_VMGlobalToProcess = \
    ".386p" \
    "mov     dl, 5ah" \
    "mov     eax, [esp]" \
    "mov     ecx, [esp+8]" \
    "mov     ebx, [esp+4]" \
    "call    dword ptr ds:[Device_Help]" \
    "jc      error" \
    "les     bx, [esp+12]" \
    "mov     es:[bx], eax" \
    "xor     ax, ax" \
    "error:" \
    value [ax] \
    parm caller nomemory [] \
    modify exact [ax bx cx dl es];

 APIRET DevHelp_VirtToLin(
    SEL Sel,
    ULONG ulOffset,
    PLIN pLinAddr
 );

 #pragma aux DevHelp_VirtToLin = \
    ".386p" \
    "mov     dl, 5bh" \
    "movzx   esp, sp" \
    "mov     esi, [esp]" \
    "call    dword ptr ds:[Device_Help]" \
    "jc      error" \
    "les     bx, [esp+4]" \
    "mov     es:[bx], eax" \
    "xor     ax, ax" \
    "error:" \
    value [ax] \
    parm caller nomemory [ax] [] \
    modify exact [ax si es bx dl];

 APIRET DevHelp_LinToGDTSelector(
    SEL Sel,
    LIN LinAddr,
    ULONG ulSize
 );

 #pragma aux DevHelp_LinToGDTSelector = \
    ".386p" \
    "mov     dl, 5ch" \
    "mov     bx, sp" \
    "mov     ecx, ss:[bx+4]" \
    "mov     ebx, ss:[bx]" \
    "call    dword ptr ds:[Device_Help]" \
    "jc      error" \
    "xor     ax, ax" \
    "error:" \
    value [ax] \
    parm caller nomemory [ax] [] \
    modify nomemory exact [ax bx cx dl];

 typedef struct _SELDESCINFO
 {
    UCHAR Type;
    UCHAR Granularity;
    LIN BaseAddr;
    ULONG Limit;
 } SELDESCINFO, FAR *PSELDESCINFO;  
                                        
 typedef struct _GATEDESCINFO
 {
    UCHAR Type;                        
    UCHAR ParmCount;                   
    SEL Selector;                    
    USHORT Reserved_1;                  
    ULONG Offset;                      
 } GATEDESCINFO, FAR *PGATEDESCINFO;

 APIRET DevHelp_GetDescInfo(
    SEL Sel,
    PBYTE pSelInfo
 );

 #pragma aux DevHelp_GetDescInfo = \
    ".386p" \
    "mov     dl, 5dh" \
    "call    dword ptr ds:[Device_Help]" \
    "jc      error" \
    "mov     es:[bx], ax" \
    "mov     es:[bx+6], edx" \
    "mov     es:[bx+2], ecx" \
    "xor     ax, ax" \
    "error:" \
    value [ax] \
    parm caller nomemory [ax] [es bx] \
    modify exact [ax cx dx];

 APIRET DevHelp_PageListToLin(
    ULONG ulSize,
    LIN pPageList,
    PLIN pLinAddr
 );

 #pragma aux DevHelp_PageListToLin = \
    ".386p" \
    "mov     dl, 5fh" \
    "mov     edi, [esp+4]" \
    "mov     ecx, [esp]" \
    "call    dword ptr ds:[Device_Help]" \
    "jc      error" \
    "les     di, [esp+8]" \
    "mov     es:[di], eax" \
    "xor     ax, ax" \
    "error:" \
    value [ax] \
    parm caller nomemory [] \
    modify exact [ax cx dx di];

 APIRET DevHelp_LinToPageList(
    LIN LinAddr,
    ULONG ulSize,
    LIN pPageList,
    PULONG pulPageListCnt
 );

 #pragma aux DevHelp_LinToPageList = \
    ".386p" \
    "mov     dl, 5eh" \
    "mov     edi, [esp+8]" \
    "mov     eax, [esp]" \
    "mov     ecx, [esp+4]" \
    "call    dword ptr ds:[Device_Help]" \
    "jc      error" \
    "mov     [esp+8], edi" \
    "mov     di, [esp+12]" \
    "mov     ss:[di], eax" \
    "xor     eax, eax" \
    "error:" \
    value [ax] \
    parm caller [] \
    modify exact [ax cx di dx];

 #define GDTSEL_R3CODE   0x0000
 #define GDTSEL_R3DATA   0x0001
 #define GDTSEL_R2CODE   0x0003
 #define GDTSEL_R2DATA   0x0004
 #define GDTSEL_R0CODE   0x0005
 #define GDTSEL_R0DATA   0x0006

 #define GDTSEL_ADDR32   0x0080

 APIRET DevHelp_PageListToGDTSelector(
    SEL Sel,
    ULONG ulSize,
    USHORT usAccess,
    LIN pPageList
 );

 #pragma aux DevHelp_PageListToGDTSelector = \
    ".386p" \
    "mov     dl, 60h" \
    "mov     dh, [esp+4]" \
    "mov     ecx, [esp]" \
    "mov     edi, [esp+6]" \
    "call    dword ptr ds:[Device_Help]" \
    "jc      error" \
    "xor     ax, ax" \
    "error:" \
    value [ax] \
    parm caller [] \
    modify nomemory exact [ax cx dx di];

 #define VMDHS_DECOMMIT        0x0001
 #define VMDHS_RESIDENT        0x0002
 #define VMDHS_SWAP            0x0004

 typedef struct _PAGELIST
 {
    ULONG PhysAddr;
    ULONG Size;
 } PAGELIST, NEAR *NPPAGELIST, FAR *PPAGELIST;

 APIRET DevHelp_VMSetMem(
    LIN LinAddr,
    ULONG ulSize,
    ULONG ulFlags
 );

 #pragma aux DevHelp_VMSetMem = \
    ".386p" \
    "mov     dl, 66h" \
    "mov     eax, [esp+8]" \
    "mov     ebx, [esp]" \
    "mov     ecx, [esp+4]" \
    DEVICE_HELP \
    parm caller nomemory [] \
    modify nomemory exact [ax bx cx dl];

 APIRET DevHelp_FreeGDTSelector(
    SEL Sel
 );

 #pragma aux DevHelp_FreeGDTSelector = \
    "mov     dl, 53h" \
    DEVICE_HELP \
    parm caller nomemory [ax] \
    modify nomemory exact [ax dl];

 //
 // Working with request packets
 //

 #define WAIT_NOT_ALLOWED 0
 #define WAIT_IS_ALLOWED  1

 APIRET DevHelp_AllocReqPacket(
    USHORT usWaitFlag,
    PBYTE FAR *pRPAddr
 );

 #pragma aux DevHelp_AllocReqPacket = \
    "mov     dl, 0dh", \
    "call    dword ptr ds:[Device_Help]" \
    "jc      error" \
    "push    bx" \
    "push    es" \
    "mov     bx, sp" \
    "les     bx, ss:[bx]" \
    "pop     es:[bx+2]" \
    "pop     es:[bx]" \
    "error:" \
    "mov     ax, 0" \
    "sbb     ax, 0" \
    value [ax] \
    parm caller [dh] [] \
    modify exact [ax bx dl es];

 APIRET DevHelp_FreeReqPacket(
    PBYTE pRPAddr
 );

 #pragma aux DevHelp_FreeReqPacket = \
    "mov     dl, 0eh", \
    "call    dword ptr ds:[Device_Help]" \
    "xor     ax, ax" \
    value [ax] \
    parm caller [es bx] \
    modify exact [ax dl];

 APIRET DevHelp_PullParticular(
    NPBYTE pQueue,
    PBYTE pRPAddr
 );

 #pragma aux DevHelp_PullParticular= \
    "mov     dl, 0bh" \
    DEVICE_HELP \
    parm [si] [es bx] \
    modify exact [ax dl];

 APIRET DevHelp_PullRequest(
    NPBYTE pQueue,
    PBYTE FAR *pRPAddr
 );

 #pragma aux DevHelp_PullRequest = \
    ".386p" \
    "mov     dl, 0ah" \
    "push    es" \
    "call    dword ptr ds:[Device_Help]" \
    "jc      error" \
    "movzx   esp, sp" \
    "push    bx" \
    "push    es" \
    "mov     bx, sp" \
    "les     bx, [esp]" \
    "pop     es:[bx+2]" \
    "pop     es:[bx]" \
    "xor     ax, ax" \
    "error:" \
    value [ax] \
    parm [si] [] \
    modify exact [ax bx dl es];

 APIRET DevHelp_PushRequest(
    NPBYTE pQueue,
    PBYTE pRPAddr
 );

 #pragma aux DevHelp_PushRequest = \
    "mov     dl, 09h" \
    "call    dword ptr ds:[Device_Help]" \
    "xor     ax, ax" \
    value [ax] \
    parm [si] [es bx] \
    modify exact [ax dl];

 APIRET DevHelp_SortRequest(
    NPBYTE pQueue,
    PBYTE pRPAddr
 );

 #pragma aux DevHelp_SortRequest = \
    "mov     dl, 0ch" \
    "call    dword ptr ds:[Device_Help]" \
    "xor     ax, ax" \
    value [ax] \
    parm [si] [es bx] \
    modify exact [ax dl];

 //
 // IDC and other functions
 //

 APIRET DevHelp_InternalError(
    PSZ pMsg,
    USHORT cbMsg
 );

 #pragma aux DevHelp_InternalError aborts = \
    "mov     dl, 2bh" \
    "push    ds" \
    "push    es" \
    "pop     ds" \
    "pop     es" \
    "call    dword ptr ds:[Device_Help]" \
    parm [es si] [di] \
    modify nomemory exact [];

 APIRET DevHelp_RAS(
    USHORT usMajor,
    USHORT usMinor,
    USHORT usSize,
    PBYTE pData
 );

 #pragma aux DevHelp_RAS = \
    "mov     dl, 28h" \
    "push    ds" \
    "push    es" \
    "pop     ds" \
    "pop     es" \
    "call    dword ptr ds:[Device_Help]" \
    "push    es" \
    "pop     ds" \
    "mov     ax, 0" \
    "sbb     ax, 0" \
    value [ax] \
    parm [ax] [cx] [bx] [es si] \
    modify nomemory exact [ax dl es];

 APIRET APIENTRY DevHelp_RegisterPerfCtrs(
    NPBYTE pData,
    NPBYTE pText,
    USHORT usFlags
 );

 #define DHGETDOSV_SYSINFOSEG       1
 #define DHGETDOSV_LOCINFOSEG       2
 #define DHGETDOSV_VECTORSDF        4
 #define DHGETDOSV_VECTORREBOOT     5
 #define DHGETDOSV_YIELDFLAG        7
 #define DHGETDOSV_TCYIELDFLAG      8
 #define DHGETDOSV_DOSCODEPAGE      11
 #define DHGETDOSV_INTERRUPTLEV     13
 #define DHGETDOSV_DEVICECLASSTABLE 14
 #define DHGETDOSV_DMQSSELECTOR     15
 #define DHGETDOSV_APMINFO          16

 APIRET DevHelp_GetDOSVar(
    USHORT usVarNum,
    USHORT usVarMember,
    PPVOID pVar
 );

 #pragma aux DevHelp_GetDOSVar = \
    "mov     dl, 24h" \
    "call    dword ptr ds:[Device_Help]" \
    "mov     es:[di], bx" \
    "mov     es:[di+2], ax" \
    "xor     ax, ax" \
    value [ax] \
    parm caller nomemory [al] [cx] [es di] \
    modify exact [ax bx dl];

 typedef struct _IDCTABLE
 {
    USHORT Reserved[3];
    VOID (FAR *ProtIDCEntry) (VOID);
    USHORT ProtIDC_DS;
 } IDCTABLE, NEAR *NPIDCTABLE;

 APIRET DevHelp_AttachDD(
    NPSZ pDDName,
    NPBYTE pDDTable
 );

 #pragma aux DevHelp_AttachDD = \
    "mov     dl, 2ah" \
    "call    dword ptr ds:[Device_Help]" \
    "mov     ax, 0" \
    "sbb     ax, 0" \
    value [ax] \
    parm caller [bx] [di] \
    modify exact [ax dl];

 typedef struct _MSGTABLE
 {
    USHORT MsgId;
    USHORT cMsgStrings;
    PSZ MsgStrings[1];
 } MSGTABLE, NEAR *NPMSGTABLE;

 APIRET DevHelp_Save_Message(
    NPBYTE pMsgTbl
 );

 #pragma aux DevHelp_Save_Message = \
    "mov     dl, 3dh" \
    "xor     bx, bx" \
    DEVICE_HELP \
    parm caller [si] \
    modify nomemory exact [ax bx dl];

 USHORT APIENTRY DevHelp_LogEntry(
    PVOID,
    USHORT
 );

 //
 // ADD / DMD interaction
 //

 #define DEVICECLASS_ADDDM 1
 #define DEVICECLASS_MOUSE 2

 APIRET DevHelp_RegisterDeviceClass(
    NPSZ pDevStr,
    PFN pfnDrvEP,
    USHORT usDevFlags,
    USHORT usDevClass,
    PUSHORT hDev
 );

 #pragma aux DevHelp_RegisterDeviceClass = \
    ".386p" \
    "mov     dl, 43h" \
    "xchg    bx, ax" \
    "call    dword ptr ds:[Device_Help]" \
    "jc      error" \
    "les     bx, [esp]" \
    "mov     es:[bx], ax" \
    "xor     ax, ax" \
    "error:" \
    value [ax] \
    parm [si] [ax bx] [di] [cx] [] \
    modify exact [ax bx dl es];

 USHORT APIENTRY DevHelp_CreateInt13VDM(
    PBYTE pVDMInt13CtrlBlk
 );

 //
 // Interrupt/Thread Management
 //

 APIRET DevHelp_SetIRQ(
    NPFN pIRQHndlr,
    USHORT usIRQL,
    USHORT usShrFlag
 );

 #pragma aux DevHelp_SetIRQ = \
    "mov     dl, 1bh" \
    DEVICE_HELP \
    parm caller nomemory [ax] [bx] [dh] \
    modify nomemory exact [ax dl];

 APIRET DevHelp_UnSetIRQ(
    USHORT usIRQL
 );

 #pragma aux DevHelp_UnSetIRQ = \
    "mov     dl, 1ch" \
    DEVICE_HELP \
    parm caller nomemory [bx] \
    modify nomemory exact [ax dl];

 APIRET DevHelp_EOI(
    USHORT usIrqL
 );

 #pragma aux DevHelp_EOI = \
    "mov     dl, 31h" \
    "call    dword ptr ds:[Device_Help]" \
    "xor     ax, ax" \
    value [ax] \
    parm caller nomemory [al] \
    modify nomemory exact [ax dl];

 typedef struct _STACKUSAGEDATA
 {
    USHORT Size;
    USHORT Flags;
    USHORT IRQLevel;
    USHORT CLIStack;
    USHORT STIStack;
    USHORT EOIStack;
    USHORT NestingLevel;
 } STACKUSAGEDATA;

 APIRET DevHelp_RegisterStackUsage(
    PVOID pData
 );

 #pragma aux DevHelp_RegisterStackUsage = \
    "mov     dl, 3ah" \
    "call    dword ptr ds:[Device_Help]" \
    "mov     ax, 0" \
    "sbb     ax, 0" \
    value [ax] \
    parm caller [bx] \
    modify nomemory exact [ax dl];

 APIRET DevHelp_Yield(void);

 #pragma aux DevHelp_Yield = \
    "mov     dl, 2" \
    "call    dword ptr ds:[Device_Help]" \
    "xor     ax, ax" \
    value [ax] \
    parm caller nomemory [] \
    modify nomemory exact [ax dl];

 APIRET DevHelp_TCYield(void);

 #pragma aux DevHelp_TCYield = \
    "mov     dl, 3" \
    "call    dword ptr ds:[Device_Help]" \
    "xor     ax, ax" \
    value [ax] \
    parm caller nomemory [] \
    modify nomemory exact [ax dl];

 APIRET DevHelp_ProcRun(
    ULONG ulEvId,
    PUSHORT pusAwakeCnt
 );

 #pragma aux DevHelp_ProcRun = \
    "mov     dl, 5" \
    "xchg    bx, ax" \
    "call    dword ptr ds:[Device_Help]" \
    "mov     es:[si], ax" \
    "xor     ax, ax" \
    value [ax] \
    parm caller nomemory [ax bx] [es si] \
    modify exact [ax bx dl];

 #define WAIT_INTERRUPTED             0x8003
 #define WAIT_TIMED_OUT               0x8001

 #define WAIT_IS_INTERRUPTABLE        0
 #define WAIT_IS_NOT_INTERRUPTABLE    1

 APIRET DevHelp_ProcBlock(
    ULONG ulEvId,
    ULONG ulWaitTime,
    USHORT usIntWaitFlag
 );

/* #pragma aux DevHelp_ProcBlock = \
    "mov     dl, 4" \
    "xchg    di, cx" \
    "xchg    ax, bx" \
    "call    dword ptr ds:[Device_Help]" \
    "jc      error" \
    "xor     ax, ax" \
    "error:" \
    value [ax] \
    parm caller nomemory [ax bx] [di cx] [dh] \
    modify nomemory exact [ax bx cx dl di]; */

#pragma aux DevHelp_ProcBlock = \
   "mov     dl, 4" \
   "xchg    cx, di" \
   "xchg    bx, ax" \
   "call    dword ptr ds:[Device_Help]" \
   "jnc     success" \
   "jnz     interrupted" \
   "mov     ax, 8001h" \
   "jmp     quit" \
   "interrupted:" \
   "mov     ax, 8003h" \
   "jmp     quit" \
   "success:" \
   "mov     ax, 0" \
   "quit:" \
   value [ax] \
   parm caller nomemory [ax bx] [di cx] [dh] \
   modify nomemory exact [ax bx cx dl di];
   
 APIRET DevHelp_DevDone(
    PBYTE pReqPktAddr
 );

 #pragma aux DevHelp_DevDone = \
    "mov     dl, 1" \
    DEVICE_HELP \
    parm caller [es bx] \
    modify exact [ax dl];

 #define VIDEO_PAUSE_OFF  0
 #define VIDEO_PAUSE_ON   1

 APIRET DevHelp_VideoPause(
    USHORT usOnOff
 );

 #pragma aux DevHelp_VideoPause = \
    "mov     dl, 3ch" \
    "call    dword ptr ds:[Device_Help]" \
    "xor     ax, ax" \
    "sbb     ax, 0" \
    value [ax] \
    parm nomemory [al] \
    modify nomemory exact [ax dl];

 //
 // Semaphores
 //

 APIRET DevHelp_OpenEventSem(
    ULONG hEvent
 );
 
 #pragma aux DevHelp_OpenEventSem = \
    ".386p" \
    "mov     dl, 67h" \
    "mov     eax, [esp]" \
    DEVICE_HELP \
    parm caller nomemory [] \
    modify nomemory exact [ax dl];

 APIRET DevHelp_CloseEventSem(
    ULONG hEvent
 );

 #pragma aux DevHelp_CloseEventSem = \
    ".386p" \
    "mov     dl, 68h" \
    "movzx   esp, sp" \
    "mov     eax, [esp]" \
    DEVICE_HELP \
    parm caller nomemory [] \
    modify nomemory exact [ax dl];

 APIRET DevHelp_PostEventSem(
    ULONG hEvent
 );

 #pragma aux DevHelp_PostEventSem = \
    ".386p" \
    "mov     dl, 69h" \
    "mov     eax, [esp]" \
    DEVICE_HELP \
    parm caller nomemory [] \
    modify nomemory exact [ax dl];

 APIRET DevHlp_ResetEventSem(
    ULONG hEvent,
    PULONG pNumPosts
 );

 #pragma aux DevHelp_ResetEventSem = \
    ".386p" \
    "mov     dl, 6ah" \
    "mov     eax, [esp]" \
    "mov     edi, [esp+4]" \
    DEVICE_HELP \
    parm caller nomemory [] \
    modify exact [ax di dl];

 #define SEMUSEFLAG_IN_USE     0
 #define SEMUSEFLAG_NOT_IN_USE 1

 APIRET DevHelp_SemHandle(
    ULONG ulSemKey,
    USHORT usSemUseFlag,
    PULONG pSemHandle
 );

 #pragma aux DevHelp_SemHandle = \
    "mov     dl, 8" \
    "xchg    bx, ax" \
    "call    dword ptr ds:[Device_Help]" \
    "jc      error" \
    "mov     es:[si+2], ax" \
    "mov     es:[si], bx" \
    "xor     ax, ax" \
    "error:" \
    value [ax] \
    parm nomemory [ax bx] [dh] [es si] \
    modify exact [ax bx dl];

 APIRET DevHelp_SemRequest(
    ULONG hSem,
    ULONG ulTimeout
 );

 #pragma aux DevHelp_SemRequest = \
    "mov     dl, 6" \
    "xchg    ax, bx" \
    "xchg    cx, di" \
    DEVICE_HELP \
    parm nomemory [ax bx] [cx di] \
    modify nomemory exact [ax bx cx di dl];

 APIRET DevHelp_SemClear(
    ULONG hSem
 );

 #pragma aux DevHelp_SemClear = \
    "mov     dl, 7" \
    "xchg    bx, ax" \
    DEVICE_HELP \
    parm nomemory [ax bx] \
    modify nomemory exact [ax bx dl];

 #define EVENT_MOUSEHOTKEY 0
 #define EVENT_CTRLBREAK   1
 #define EVENT_CTRLC       2
 #define EVENT_CTRLNUMLOCK 3
 #define EVENT_CTRLPRTSC   4
 #define EVENT_SHIFTPRTSC  5
 #define EVENT_KBDHOTKEY   6
 #define EVENT_KBDREBOOT   7

 APIRET DevHelp_SendEvent(
    USHORT usEvType,
    USHORT usParm
 );

 #pragma aux DevHelp_SendEvent = \
    "mov     dl, 25h" \
    "call    dword ptr ds:[Device_Help]" \
    "xor     ax, ax" \
    "sbb     ax, 0" \
    value [ax] \
    parm nomemory [ah] [bx] \
    modify nomemory exact [ax dl];

 //
 // Timers
 //

 APIRET DevHelp_SetTimer(
    NPFN pTmrHndlr
 );

 #pragma aux DevHelp_SetTimer = \
    "mov     dl, 1dh" \
    DEVICE_HELP \
    parm caller nomemory [ax] \
    modify nomemory exact [ax dl];

 APIRET DevHelp_ResetTimer(
    NPFN pTmrHndlr
 );

 #pragma aux DevHelp_ResetTimer = \
    "mov     dl, 1eh" \
    DEVICE_HELP \
    parm caller nomemory [ax] \
    modify nomemory exact [ax dl];

 APIRET DevHelp_TickCount(
    NPFN pTmrHndlr,
    USHORT usTickCnt
 );

 #pragma aux DevHelp_TickCount = \
    "mov     dl, 33h" \
    DEVICE_HELP \
    parm caller nomemory [ax] [bx] \
    modify nomemory exact [ax dl];

 APIRET DevHelp_SchedClock(
    PFN NEAR *pSchedRtnAddr
 );

 #pragma aux DevHelp_SchedClock = \
    "mov     dl, 0" \
    "call    dword ptr ds:[Device_Help]" \
    "xor     ax, ax" \
    value [ax] \
    parm caller [ax] \
    modify nomemory exact [ax dl];

 APIRET DevHelp_RegisterTmrDD(
    NPFN pTmrEntry,
    PULONG pulTmrRollover,
    PULONG pulTmr
 );

 #pragma aux DevHelp_RegisterTmrDD = \
    ".386p" \
    "mov     dl, 61h" \
    "call    dword ptr ds:[Device_Help]" \
    "mov     ax, bx" \
    "les     bx, [esp]" \
    "mov     es:[bx+2], di" \
    "mov     es:[bx], ax" \
    "les     bx, [esp+4]" \
    "mov     es:[bx+2], di" \
    "mov     es:[bx], cx" \
    "xor     ax, ax" \
    value [ax] \
    parm caller nomemory [di] [] \
    modify exact [ax bx cx di dl es];

 //
 // Device Monitors
 //

 APIRET DevHelp_MonitorCreate(
    USHORT hMon,
    PBYTE pFinalBuf,
    NPFN pfnNotifyRtn,
    PUSHORT hMonChain
 );

 #pragma aux DevHelp_MonitorCreate = \
    ".386p" \
    "mov     dl, 1fh" \
    "call    dword ptr ds:[Device_Help]" \
    "jc      error" \
    "mov     si, [esp]" \
    "mov     es, [esp+2]" \
    "mov     es:[si], ax" \
    "xor     ax, ax" \
    "error:" \
    value [ax] \
    parm caller [ax] [es si] [di] [] \
    modify exact [ax dl es si];

 #define CHAIN_AT_TOP       0
 #define CHAIN_AT_BOTTOM    1

 APIRET DevHelp_Register(
    USHORT hMon,
    USHORT usMonPID,
    PBYTE pInputBuf,
    NPBYTE pOutputBuf,
    USHORT usChainFlag
 );

 #pragma aux DevHelp_Register = \
    "mov     dl, 20h" \
    DEVICE_HELP \
    parm caller nomemory [ax] [cx] [es si] [di] [dh] \
    modify nomemory exact [ax dl];

 APIRET DevHelp_DeRegister(
    USHORT usMonPID,
    USHORT hMon,
    PUSHORT pusMonsLeft
 );

 #pragma aux DevHelp_DeRegister = \
    "mov     dl, 21h" \
    "call    dword ptr ds:[Device_Help]" \
    "jc      error" \
    "mov     es:[di], ax" \
    "xor     ax, ax" \
    "error:" \
    value [ax] \
    parm caller nomemory [bx] [ax] [es di] \
    modify exact [ax dl];

 APIRET DevHelp_MonFlush(
    USHORT hMon
 );

 #pragma aux DevHelp_MonFlush = \
    "mov     dl, 23h" \
    DEVICE_HELP \
    parm caller [ax] \
    modify exact [ax dl];

 APIRET DevHelp_MonWrite(
    USHORT hMon,
    NPBYTE pDataRec,
    USHORT usCnt,
    ULONG ulTimeStamp,
    USHORT usWaitFlag
 );

 #pragma aux DevHelp_MonWrite = \
    "mov     dl, 22h" \
    DEVICE_HELP \
    parm caller [ax] [si] [cx] [di bx] [dh] \
    modify exact [ax dl];

 //
 // Character queues
 //

 typedef struct _QUEUEHDR
 {
    USHORT QSize;
    USHORT QChrOut;
    USHORT QCount;
    BYTE Queue[1];
 } QUEUEHDR, FAR *PQUEUEHDR;

 APIRET DevHelp_QueueInit(
    NPBYTE pQue
 );

 #pragma aux DevHelp_QueueInit = \
    "mov     dl, 0fh" \
    "call    dword ptr ds:[Device_Help]" \
    "mov     ax, 0" \
    value [ax] \
    parm [bx] \
    modify exact [ax dl];

 APIRET DevHelp_QueueRead(
    NPBYTE pQue,
    PBYTE pChar
 );

 #pragma aux DevHelp_QueueRead = \
    "mov     dl, 12h" \
    "call    dword ptr ds:[Device_Help]" \
    "jc      error" \
    "mov     es:[di], al" \
    "xor     ax, ax" \
    "error:" \
    value [ax] \
    parm [bx] [es di] \
    modify exact [ax dl];

 APIRET DevHelp_QueueWrite(
    NPBYTE pQue,
    UCHAR Char
 );

 #pragma aux DevHelp_QueueWrite = \
    "mov     dl, 11h" \
    DEVICE_HELP \
    parm [bx] [al] \
    modify exact [ax dl];

 APIRET DevHelp_QueueFlush(
    NPBYTE pQue
 );

 #pragma aux DevHelp_QueueFlush = \
    "mov     dl, 10h" \
    "call    dword ptr ds:[Device_Help]" \
    "xor     ax, ax" \
    value [ax] \
    parm [bx] \
    modify exact [ax dl];

 //
 // Context Hooks
 //

 APIRET DevHelp_AllocateCtxHook(
    NPFN pfnHookHndlr,
    PULONG hHook
 );

 #pragma aux DevHelp_AllocateCtxHook = \
    ".386p" \
    "mov     dl, 63h" \
    "movzx   eax, ax" \
    "push    bx" \
    "mov     ebx, -1" \
    "call    dword ptr ds:[Device_Help]" \
    "pop     bx" \
    "jc      error" \
    "mov     es:[bx], eax" \
    "xor     ax, ax" \
    "error:" \
    value [ax] \
    parm caller nomemory [ax] [es bx] \
    modify exact [ax bx dl es];

 APIRET DevHelp_FreeCtxHook(
    ULONG hHook
 );

 #pragma aux DevHelp_FreeCtxHook = \
    ".386p" \
    "mov     dl, 64h", \
    "movzx   esp,sp" \
    "mov     eax, [esp]" \
    DEVICE_HELP \
    parm caller nomemory [] \
    modify nomemory exact [ax dl];

 APIRET DevHelp_ArmCtxHook(
    ULONG ulHookData,
    ULONG hHook
 );

 #pragma aux DevHelp_ArmCtxHook = \
    ".386p" \
    "mov     dl, 65h", \
    "mov     bx, sp" \
    "mov     eax, ss:[bx]" \
    "mov     ebx, ss:[bx+4]" \
    "mov     ecx, -1" \
    DEVICE_HELP \
    parm caller nomemory [] \
    modify nomemory exact [ax bx cx dl];

 //
 // Mode Switching / Real Mode
 //

 APIRET APIENTRY DevHelp_RealToProt(void);
 
 APIRET APIENTRY DevHelp_ProtToReal(void);

 APIRET APIENTRY DevHelp_ROMCritSection(
    USHORT usEnterExit
 );

 APIRET APIENTRY DevHelp_SetROMVector(
    NPFN pfnIntHandler,
    USHORT usINTNum,
    USHORT usSaveDSLoc,
    PULONG pulLastHdr
 );

 //
 // File System access (basedev init time only)
 //

 typedef struct FOPEN
 {
    PSZ   FileName;
    ULONG FileSize;
 } FILEOPEN;

 typedef struct FREAD
 {
    PBYTE Buffer;
    ULONG ReadSize;
 } FILEREAD;

 typedef struct FREADAT
 {
    PBYTE Buffer;
    ULONG ReadSize;
    ULONG StartPosition;
 } FILEREADAT;

 typedef struct FCLOSE
 {
    USHORT Reserved;
 } FILECLOSE;

 typedef union FILEIOOP
 {
    struct FOPEN FileOpen;
    struct FCLOSE FileClose;
    struct FREAD FileRead;
    struct FREADAT FileReadAt;
 } FILEIOOP;

 typedef struct _DDFileIo
 {
    USHORT   Length;
    FILEIOOP Data;
 } FILEIOINFO, FAR * PFILEIOINFO;

 APIRET APIENTRY DevHelp_OpenFile(
    PFILEIOINFO pFileOpen
 );

 APIRET APIENTRY DevHelp_CloseFile(
    PFILEIOINFO pFileClose
 );

 APIRET APIENTRY DevHelp_ReadFile(
    PFILEIOINFO pFileRead
 );

 APIRET APIENTRY DevHelp_ReadFileAt(
    PFILEIOINFO pFileReadAT
 );

 //
 // ABIOS ops
 //

 APIRET DevHelp_ABIOSCall(
    USHORT usLid,
    NPBYTE pReqBlk,
    USHORT usEntryType
 );

 #pragma aux DevHelp_ABIOSCall = \
    "mov     dl, 36h" \
    DEVICE_HELP \
    parm caller [ax] [si] [dh] \
    modify exact [ax dl];

 APIRET DevHelp_ABIOSCommonEntry(
    NPBYTE pReqBlk,
    USHORT usEntryType
 );

 #pragma aux DevHelp_ABIOSCommonEntry = \
    "mov     dl, 37h" \
    DEVICE_HELP \
    parm caller [si] [dh] \
    modify exact [ax dl];

 APIRET APIENTRY DevHelp_ABIOSGetParms(
    USHORT usLid,
    NPBYTE pParmsBlk
 );

 APIRET DevHelp_FreeLIDEntry(
    USHORT usLID
 );

 #pragma aux DevHelp_FreeLIDEntry = \
    "mov     dl, 35h" \
    DEVICE_HELP \
    parm caller nomemory [ax] \
    modify nomemory exact [ax dl];

 APIRET DevHelp_GetLIDEntry(
    USHORT usDeviceType,
    SHORT sLIDIndex,
    USHORT usType,
    PUSHORT pusLID
 );

 #pragma aux DevHelp_GetLIDEntry = \
    "mov     dl, 34h" \
    "call    dword ptr ds:[Device_Help]" \
    "jc      error" \
    "mov     es:[di], ax" \
    "xor     ax, ax" \
    "error:" \
    value [ax] \
    parm caller nomemory [al] [bl] [dh] [es di] \
    modify exact [ax dl];

 APIRET DevHelp_GetDeviceBlock(
    USHORT usLid,
    PPVOID pDeviceBlock
 );

 #pragma aux DevHelp_GetDeviceBlock = \
    "mov     dl, 38h" \
    "call    dword ptr ds:[Device_Help]" \
    "jc      error" \
    "mov     es:[si+2], cx" \
    "mov     es:[si], dx" \
    "xor     ax, ax" \
    "error:" \
    value [ax] \
    parm caller nomemory [ax] [es si] \
    modify exact [ax bx cx dx];

 //
 // Other
 //

 APIRET DevHelp_RegisterBeep(
    PFN pfnBeepHndlr
 );

 #pragma aux DevHelp_RegisterBeep = \
    "mov     dl, 51h" \
    "call    dword ptr ds:[Device_Help]" \
    "xor     ax, ax" \
    value [ax] \
    parm caller nomemory [cx di] \
    modify nomemory exact [ax dl];

 APIRET DevHelp_Beep(
    USHORT usFreq,
    USHORT usTime
 );

 #pragma aux DevHelp_Beep = \
    "mov     dl, 52h" \
    DEVICE_HELP \
    parm caller nomemory [bx] [cx] \
    modify nomemory exact [ax dx];

 APIRET DevHelp_RegisterPDD(
    NPSZ pszPhysDevName,
    PFN pfnHndlrRtn
 );

 #pragma aux DevHelp_RegisterPDD = \
    "mov     dl, 50h" \
    DEVICE_HELP \
    parm caller [si] [es di] \
    modify nomemory exact [ax dl];

 #define DYNAPI_ROUTINE16        0x0002
 #define DYNAPI_ROUTINE32        0x0000
 #define DYNAPI_CALLGATE16       0x0001
 #define DYNAPI_CALLGATE32       0x0000

 APIRET DevHelp_DynamicAPI(
    PVOID pRoutineAddress,
    USHORT cbParm,
    USHORT usFlags,
    PSEL pCallGateSel
 );

 #pragma aux DevHelp_DynamicAPI = \
    "mov     dl, 6ch" \
    "xchg    bx, ax" \
    "call    dword ptr ds:[Device_Help]" \
    "jc      error" \
    "mov     es:[si], di" \
    "xor     ax, ax" \
    "error:" \
    value [ax] \
    parm caller [ax bx] [cx] [dh] [es si] \
    modify exact [ax bx di dl];

 #ifdef __cplusplus
   }
 #endif

 #endif

 #endif /* __DEVHELP_H__ */
