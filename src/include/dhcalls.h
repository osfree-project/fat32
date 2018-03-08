 #ifndef __DHCALLS_H__
 #define __DHCALLS_H__

 typedef VOID   NEAR    *NPVOID;
 typedef VOID   FAR     *PVOID;
 typedef PVOID  FAR     *PPVOID;
 typedef USHORT NEAR    *NPUSHORT;

 typedef ULONG          LIN;
 typedef ULONG     _far *PLIN;

 #define MSG_MEMORY_ALLOCATION_FAILED 0x00
 #define ERROR_LID_ALREADY_OWNED      0x01
 #define ERROR_LID_DOES_NOT_EXIST     0x02
 #define ERROR_ABIOS_NOT_PRESENT      0x03
 #define ERROR_NOT_YOUR_LID           0x04
 #define ERROR_INVALID_ENTRY_POINT    0x05

 //
 // memory operations (alloc/free/etc)
 //

 USHORT APIENTRY DevHelp_AllocGDTSelector(
    PSEL   Selectors,
    USHORT Count
 );

 #define MEMTYPE_ABOVE_1M      0
 #define MEMTYPE_BELOW_1M      1

 USHORT APIENTRY DevHelp_AllocPhys(
    ULONG  lSize,
    USHORT MemType,
    PULONG PhysAddr
 );

 USHORT APIENTRY DevHelp_FreePhys(
    ULONG PhysAddr
 );

 USHORT APIENTRY DevHelp_PhysToGDTSelector(
    ULONG  PhysAddr,
    USHORT Count,
    SEL    Selector
 );

 USHORT APIENTRY DevHelp_PhysToGDTSel(
    ULONG PhysAddr,
    ULONG Count,
    SEL   Selector,
    UCHAR Access
 );

 USHORT APIENTRY DevHelp_PhysToVirt(
    ULONG   PhysAddr,
    USHORT  usLength,
    PVOID   SelOffset,
    PUSHORT ModeFlag
 );

 USHORT APIENTRY DevHelp_UnPhysToVirt(
    PUSHORT ModeFlag
 );

 #define SELTYPE_R3CODE        0
 #define SELTYPE_R3DATA        1
 #define SELTYPE_FREE          2
 #define SELTYPE_R2CODE        3
 #define SELTYPE_R2DATA        4
 #define SELTYPE_R3VIDEO       5

 USHORT APIENTRY DevHelp_PhysToUVirt(
    ULONG  PhysAddr,
    USHORT Length,
    USHORT Flags,
    USHORT TagType,
    PVOID  SelOffset
 );

 USHORT APIENTRY DevHelp_VirtToPhys(
    PVOID  SelOffset,
    PULONG PhysAddr
 );

 #define LOCKTYPE_SHORT_ANYMEM 0x00
 #define LOCKTYPE_LONG_ANYMEM  0x01
 #define LOCKTYPE_LONG_HIGHMEM 0x03
 #define LOCKTYPE_SHORT_VERIFY 0x04

 USHORT APIENTRY DevHelp_Lock(
    SEL    Segment,
    USHORT LockType,
    USHORT WaitFlag,
    PULONG LockHandle
 );

 USHORT APIENTRY DevHelp_UnLock(
    ULONG LockHandle
 );

 #define VERIFY_READONLY       0
 #define VERIFY_READWRITE      1

 USHORT APIENTRY DevHelp_VerifyAccess(
    SEL    MemSelector,
    USHORT Length,
    USHORT MemOffset,
    UCHAR  AccessFlag
 );

 #define VMDHA_16M             0x0001
 #define VMDHA_FIXED           0x0002
 #define VMDHA_SWAP            0x0004
 #define VMDHA_CONTIG          0x0008
 #define VMDHA_PHYS            0x0010
 #define VMDHA_PROCESS         0x0020
 #define VMDHA_SGSCONT         0x0040
 #define VMDHA_SELMAP          0x0080
 #define VMDHA_RESERVE         0x0100
 #define VMDHA_SHARED          0x0400
 #define VMDHA_USEHIGHMEM      0x0800
 #define VMDHA_ALIGN64K        0x1000
 #define VMDHA_USEHMA          0x2000

 USHORT APIENTRY DevHelp_VMAlloc(
    ULONG  Flags,
    ULONG  Size,
    ULONG  PhysAddr,
    PLIN   LinearAddr,
    PPVOID SelOffset
 );

 USHORT APIENTRY DevHelp_VMFree(
    LIN LinearAddr
 );

 #define VMDHL_NOBLOCK        0x0001
 #define VMDHL_CONTIGUOUS     0x0002
 #define VMDHL_16M            0x0004
 #define VMDHL_WRITE          0x0008
 #define VMDHL_LONG           0x0010
 #define VMDHL_VERIFY         0x0020
 #define VMDHL_TRY_CONTIG     0x8000

 USHORT APIENTRY DevHelp_VMLock(
    ULONG  Flags,
    LIN    LinearAddr,
    ULONG  Length,
    LIN    pPagelist,
    LIN    pLockHandle,
    PULONG PageListCount
 );

 USHORT APIENTRY DevHelp_VMUnLock(
    LIN pLockHandle
 );

 #define VMDHPG_READONLY            0x0000
 #define VMDHPG_WRITE               0x0001

 USHORT APIENTRY DevHelp_VMProcessToGlobal(
    ULONG Flags,
    LIN   LinearAddr,
    ULONG Length,
    PLIN  GlobalLinearAddr
 );

 #define VMDHGP_WRITE               0x0001
 #define VMDHGP_SELMAP              0x0002
 #define VMDHGP_SGSCONTROL          0x0004
 #define VMDHGP_4MEG                0x0008
 #define VMDHGP_HMA                 0x0020

 USHORT APIENTRY DevHelp_VMGlobalToProcess(
    ULONG Flags,
    LIN   LinearAddr,
    ULONG Length,
    PLIN  ProcessLinearAddr
 );

 USHORT APIENTRY DevHelp_VirtToLin( 
    SEL   Selector,
    ULONG Offset,
    PLIN  LinearAddr
 );

 USHORT APIENTRY DevHelp_LinToGDTSelector(
    SEL   Selector,
    LIN   LinearAddr,
    ULONG Size
 );

 typedef struct _SELDESCINFO
 {
    UCHAR  Type;
    UCHAR  Granularity;
    LIN    BaseAddr;
    ULONG  Limit;
 } SELDESCINFO, _far *PSELDESCINFO;

 typedef struct _GATEDESCINFO
 {
    UCHAR  Type;
    UCHAR  ParmCount;
    SEL    Selector;
    USHORT Reserved_1;
    ULONG  Offset;
 } GATEDESCINFO, _far *PGATEDESCINFO;

 USHORT APIENTRY DevHelp_GetDescInfo(
    SEL Selector,
    PBYTE SelInfo
 );

 USHORT APIENTRY DevHelp_PageListToLin(
    ULONG Size,
    LIN   pPageList,
    PLIN  LinearAddr
 );

 USHORT APIENTRY DevHelp_LinToPageList(
    LIN    LinearAddr,
    ULONG  Size,
    LIN    pPageList,
    PULONG PageListCount
 );

 #define GDTSEL_R3CODE          0x0000
 #define GDTSEL_R3DATA          0x0001
 #define GDTSEL_R2CODE          0x0003
 #define GDTSEL_R2DATA          0x0004
 #define GDTSEL_R0CODE          0x0005
 #define GDTSEL_R0DATA          0x0006

 #define GDTSEL_ADDR32          0x0080

 USHORT APIENTRY DevHelp_PageListToGDTSelector(
    SEL    Selector,
    ULONG  Size,
    USHORT Access,
    LIN    pPageList
 );

 #define VMDHS_DECOMMIT         0x0001
 #define VMDHS_RESIDENT         0x0002
 #define VMDHS_SWAP             0x0004

 typedef struct _PAGELIST
 {
    ULONG PhysAddr;
    ULONG Size;
 } PAGELIST, NEAR *NPPAGELIST, _far  *PPAGELIST;

 USHORT APIENTRY DevHelp_VMSetMem(
    LIN LinearAddr,
    ULONG Size,
    ULONG Flags
 );

 USHORT APIENTRY DevHelp_FreeGDTSelector(
    SEL Selector
 );

 //
 // Working with request packets
 //

 #define WAIT_NOT_ALLOWED    0
 #define WAIT_IS_ALLOWED     1

 USHORT APIENTRY DevHelp_AllocReqPacket(
    USHORT    WaitFlag,
    PBYTE _far *ReqPktAddr
 );

 USHORT APIENTRY DevHelp_FreeReqPacket(
    PBYTE ReqPktAddr
 );

 USHORT APIENTRY DevHelp_PullParticular(
    NPBYTE Queue,
    PBYTE  ReqPktAddr
 );

 USHORT APIENTRY DevHelp_PullRequest(
    NPBYTE    Queue,
    PBYTE _far *ReqPktAddr);

 USHORT APIENTRY DevHelp_PushRequest(
    NPBYTE Queue,
    PBYTE  ReqPktAddr
 );

 USHORT APIENTRY DevHelp_SortRequest(
    NPBYTE Queue,
    PBYTE  ReqPktAddr
 );

 //
 // IDC and other functions
 //

 USHORT APIENTRY DevHelp_RealToProt();

 USHORT APIENTRY DevHelp_ProtToReal();

 USHORT APIENTRY DevHelp_InternalError(
    PSZ    MsgText,
    USHORT MsgLength
 );

 USHORT APIENTRY DevHelp_RAS(
    USHORT Major,
    USHORT Minor,
    USHORT Size,
    PBYTE Data
 );

 USHORT APIENTRY DevHelp_RegisterPerfCtrs(
    NPBYTE pDataBlock,
    NPBYTE pTextBlock,
    USHORT Flags
 );

 #define DHGETDOSV_SYSINFOSEG         1
 #define DHGETDOSV_LOCINFOSEG         2
 #define DHGETDOSV_VECTORSDF          4
 #define DHGETDOSV_VECTORREBOOT       5
 #define DHGETDOSV_YIELDFLAG          7
 #define DHGETDOSV_TCYIELDFLAG        8
 #define DHGETDOSV_DOSCODEPAGE        11
 #define DHGETDOSV_INTERRUPTLEV       13
 #define DHGETDOSV_DEVICECLASSTABLE   14
 #define DHGETDOSV_DMQSSELECTOR       15
 #define DHGETDOSV_APMINFO            16

 USHORT APIENTRY DevHelp_GetDOSVar(
    USHORT VarNumber,
    USHORT VarMember,
    PPVOID KernelVar
 );

 typedef struct _IDCTABLE 
 {
    USHORT        Reserved[3];
    VOID          (_far *ProtIDCEntry)(VOID);
    USHORT        ProtIDC_DS;
 } IDCTABLE, NEAR *NPIDCTABLE;

 USHORT APIENTRY DevHelp_AttachDD(
    NPSZ   DDName,
    NPBYTE IDCTable
 );

 typedef struct _MSGTABLE
 {
    USHORT   MsgId;
    USHORT   cMsgStrings;
    PSZ      MsgStrings[1];
 } MSGTABLE, NEAR *NPMSGTABLE;

 USHORT APIENTRY DevHelp_Save_Message( NPBYTE MsgTable );

 USHORT APIENTRY DevHelp_LogEntry(
     PVOID,
     USHORT
 );

 //
 // ADD / DMD interaction
 //

 #define DEVICECLASS_ADDDM          1
 #define DEVICECLASS_MOUSE          2

 USHORT APIENTRY DevHelp_RegisterDeviceClass(
    NPSZ    DeviceString,
    PFN     DriverEP,
    USHORT  DeviceFlags,
    USHORT  DeviceClass,
    PUSHORT DeviceHandle
 );

 USHORT APIENTRY DevHelp_CreateInt13VDM(
    PBYTE VDMInt13CtrlBlk
 );

 //
 // Interrupt/Thread Management
 //

 USHORT APIENTRY DevHelp_SetIRQ(
    NPFN   IRQHandler,
    USHORT IRQLevel,
    USHORT SharedFlag
 );

 USHORT APIENTRY DevHelp_UnSetIRQ(
    USHORT IRQLevel
 );

 USHORT APIENTRY DevHelp_EOI( USHORT IRQLevel );

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

 USHORT APIENTRY DevHelp_RegisterStackUsage(
    PVOID StackUsageData
 );

 USHORT APIENTRY DevHelp_Yield(void);

 USHORT APIENTRY DevHelp_TCYield(void);

 USHORT APIENTRY DevHelp_ProcRun(
    ULONG   EventId,
    PUSHORT AwakeCount
 );

 #define WAIT_IS_INTERRUPTABLE      0
 #define WAIT_IS_NOT_INTERRUPTABLE  1

 #define WAIT_INTERRUPTED           0x8003
 #define WAIT_TIMED_OUT             0x8001

 USHORT APIENTRY DevHelp_ProcBlock(
    ULONG  EventId,
    ULONG  WaitTime,
    USHORT IntWaitFlag
 );

 USHORT APIENTRY DevHelp_DevDone(
    PBYTE ReqPktAddr
 );

 #define VIDEO_PAUSE_OFF            0
 #define VIDEO_PAUSE_ON             1

 USHORT APIENTRY DevHelp_VideoPause(
    USHORT OnOff
 );

 //
 // Semaphores
 //

 USHORT APIENTRY DevHelp_OpenEventSem(
    ULONG hEvent
 );

 USHORT APIENTRY DevHelp_CloseEventSem(
    ULONG hEvent
 );

 USHORT APIENTRY DevHelp_PostEventSem(
    ULONG hEvent
 );

 USHORT APIENTRY DevHlp_ResetEventSem(
    ULONG hEvent,
    PULONG pNumPosts
 );

 #define SEMUSEFLAG_IN_USE       0
 #define SEMUSEFLAG_NOT_IN_USE   1

 USHORT APIENTRY DevHelp_SemHandle(
    ULONG  SemKey,
    USHORT SemUseFlag,
    PULONG SemHandle
 );

 USHORT APIENTRY DevHelp_SemRequest(
    ULONG SemHandle,
    ULONG SemTimeout
 );

 USHORT APIENTRY DevHelp_SemClear(
    ULONG SemHandle
 );

 #define EVENT_MOUSEHOTKEY      0
 #define EVENT_CTRLBREAK        1
 #define EVENT_CTRLC            2
 #define EVENT_CTRLNUMLOCK      3
 #define EVENT_CTRLPRTSC        4
 #define EVENT_SHIFTPRTSC       5
 #define EVENT_KBDHOTKEY        6
 #define EVENT_KBDREBOOT        7

 USHORT APIENTRY DevHelp_SendEvent(
    USHORT EventType,
    USHORT Parm
 );

 //
 // Timers
 //

 USHORT APIENTRY DevHelp_SetTimer(
    NPFN TimerHandler
 );

 USHORT APIENTRY DevHelp_ResetTimer(
    NPFN TimerHandler
 );

 USHORT APIENTRY DevHelp_TickCount(
    NPFN   TimerHandler,
    USHORT TickCount
 );

 USHORT APIENTRY DevHelp_SchedClock(
    PFN NEAR *SchedRoutineAddr
 );

 USHORT APIENTRY DevHelp_RegisterTmrDD(
    NPFN   TimerEntry,
    PULONG TmrRollover,
    PULONG Tmr
 );

 //
 // Device Monitors
 //

 USHORT APIENTRY DevHelp_MonitorCreate(
    USHORT  MonitorHandle,
    PBYTE   FinalBuffer,
    NPFN    NotifyRtn,
    PUSHORT MonitorChainHandle
 );

 #define CHAIN_AT_TOP    0
 #define CHAIN_AT_BOTTOM 1

 USHORT APIENTRY DevHelp_Register(
    USHORT MonitorHandle,
    USHORT MonitorPID,
    PBYTE  InputBuffer,
    NPBYTE OutputBuffer,
    USHORT ChainFlag
 );

 USHORT APIENTRY DevHelp_DeRegister(
    USHORT  MonitorPID,
    USHORT  MonitorHandle,
    PUSHORT MonitorsLeft
 );

 USHORT APIENTRY DevHelp_MonFlush(
    USHORT MonitorHandle
 );

 USHORT APIENTRY DevHelp_MonWrite(
    USHORT MonitorHandle,
    PBYTE  DataRecord,
    USHORT Count,
    ULONG  TimeStampMS,
    USHORT WaitFlag
 );

 //
 // Character queues
 //

 typedef struct _QUEUEHDR
 {
    USHORT QSize;
    USHORT QChrOut;
    USHORT Count;
    BYTE   Queue[1];
 } QUEUEHDR, _far *PQUEUEHDR;

 USHORT APIENTRY DevHelp_QueueInit(
   NPBYTE Queue
 );

 USHORT APIENTRY DevHelp_QueueRead(
    NPBYTE Queue,
    PBYTE  Char
 );

 USHORT APIENTRY DevHelp_QueueWrite(
    NPBYTE Queue,
    UCHAR  Char
 );

 USHORT APIENTRY DevHelp_QueueFlush(
    NPBYTE Queue
 );

 //
 // Context Hooks
 //

 USHORT APIENTRY DevHelp_AllocateCtxHook(
    NPFN   HookHandler,
    PULONG HookHandle
 );

 USHORT APIENTRY DevHelp_FreeCtxHook(
    ULONG HookHandle
 );

 USHORT APIENTRY DevHelp_ArmCtxHook(
    ULONG HookData,
    ULONG HookHandle
 );

 //
 // Mode Switching / Real Mode
 //

 USHORT APIENTRY DevHelp_RealToProt();
 
 USHORT APIENTRY DevHelp_ProtToReal();

 USHORT APIENTRY DevHelp_ROMCritSection(
    USHORT EnterExit
 );

 USHORT APIENTRY DevHelp_SetROMVector(
    NPFN   IntHandler,
    USHORT INTNum,
    USHORT SaveDSLoc,
    PULONG LastHeader
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
 } FILEIOINFO, _far * PFILEIOINFO;

 USHORT APIENTRY DevHelp_OpenFile(
    PFILEIOINFO pFileOpen
 );

 USHORT APIENTRY DevHelp_CloseFile(
    PFILEIOINFO pFileClose
 );

 USHORT APIENTRY DevHelp_ReadFile(
    PFILEIOINFO pFileRead
 );

 USHORT APIENTRY DevHelp_ReadFileAt(
    PFILEIOINFO pFileReadAT
 );

 //
 // ABIOS ops
 //

 USHORT APIENTRY DevHelp_ABIOSCall(
    USHORT Lid,
    NPBYTE ReqBlk,
    USHORT Entry_Type
 );

 USHORT APIENTRY DevHelp_ABIOSCommonEntry(
    NPBYTE ReqBlk,
    USHORT Entry_Type
 );

 USHORT APIENTRY DevHelp_ABIOSGetParms(
    USHORT Lid,
    NPBYTE ParmsBlk
 );

 USHORT APIENTRY DevHelp_FreeLIDEntry(
    USHORT LIDNumber
 );

 USHORT APIENTRY DevHelp_GetLIDEntry(
    USHORT  DeviceType,
    USHORT  LIDIndex,
    USHORT  LIDType,
    PUSHORT LID
 );

 USHORT APIENTRY DevHelp_GetDeviceBlock(
    USHORT Lid,
    PPVOID DeviceBlockPtr
 );

 //
 // Other
 //

 USHORT APIENTRY DevHelp_RegisterBeep(
    PFN BeepHandler
 );

 USHORT APIENTRY DevHelp_Beep(
    USHORT Frequency,
    USHORT DurationMS
 );

 USHORT APIENTRY DevHelp_RegisterPDD(
    NPSZ PhysDevName,
    PFN  HandlerRoutine
 );

 #define DYNAPI_ROUTINE16     0x0002
 #define DYNAPI_ROUTINE32     0x0000
 #define DYNAPI_CALLGATE16    0x0001
 #define DYNAPI_CALLGATE32    0x0000

 USHORT APIENTRY DevHelp_DynamicAPI(
    PVOID  RoutineAddress,
    USHORT ParmCount,
    USHORT Flags,
    PSEL   CallGateSel
 );

 #endif /* __DHCALLS_H__ */
