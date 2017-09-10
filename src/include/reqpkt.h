 #ifndef __REQPQT_H__
 #define __REQPQT_H__

 typedef struct _BPB
 {
    USHORT  BytesPerSector;
    UCHAR   SectorsPerCluster;
    USHORT  ReservedSectors;
    UCHAR   NumFATs;
    USHORT  MaxDirEntries;
    USHORT  TotalSectors;
    UCHAR   MediaType;
    USHORT  NumFATSectors;
    USHORT  SectorsPerTrack;
    USHORT  NumHeads;
    ULONG   HiddenSectors;
    ULONG   BigTotalSectors;
    UCHAR   Reserved1[6];
 } BPB, FAR *PBPB, *NPBPB;

 typedef BPB  near *BPBS[];
 typedef BPBS far  *PBPBS;

 typedef struct _DDHDR
 {
    PVOID   NextHeader;
    USHORT  DevAttr;
    USHORT  StrategyEP;
    USHORT  InterruptEP;
    UCHAR   DevName[8];
    USHORT  ProtModeCS;
    USHORT  ProtModeDS;
    USHORT  RealModeCS;
    USHORT  RealModeDS;
    ULONG   SDevCaps;
 } DDHDR;

 #define MAX_DISKDD_CMD 29

 #define STERR   0x8000
 #define STINTER 0x0400
 #define STBUI   0x0200
 #define STDON   0x0100
 #define WRECODE 0x0000
 #define STECODE 0x00FF

 #define RPF_Int13RP          0x01
 #define RPF_CallOutDone      0x02
 #define RPF_PktDiskIOTchd    0x04
 #define RPF_CHS_ADDRESSING   0x08
 #define RPF_Internal         0x10
 #define RPF_TraceComplete    0x20

 #define STATUS_DONE          0x0100
 #define STATUS_ERR_UNKCMD    0x8003

 typedef struct _RPH RPH;
 typedef struct _RPH *NPRPH;
 typedef struct _RPH FAR *PRPH;

 //
 // RP header
 // 

 typedef struct _RPH
 {
    UCHAR  Len;
    UCHAR  Unit;
    UCHAR  Cmd;
    USHORT Status;
    UCHAR  Flags;
    UCHAR  Reserved1[3];
    PRPH   Link;
 } RPH;

 //
 // Init
 //

 typedef struct _RPINIT
 {
    RPH   rph;
    UCHAR Unit;
    PFN   DevHlpEP;
    PSZ   InitArgs;
    UCHAR DriveNum;
 } RPINITIN, FAR *PRPINITIN;

 typedef struct _RPINITOUT
 {
    RPH    rph;
    UCHAR  Unit;
    USHORT CodeEnd;
    USHORT DataEnd;
    PBPBS  BPBArray;
    USHORT Status;
 } RPINITOUT, FAR *PRPINITOUT;

 #ifndef INCL_INITRP_ONLY

 //
 // Build BPB
 //

 typedef struct _RP_BUILDBPB 
 {
    RPH   rph;
    UCHAR MediaDescr;
    ULONG XferAddr;
    PBPB  bpb;
    UCHAR DriveNum;
 } RP_BUILDBPB, FAR *PRP_BUILDBPB;

 //
 // Media check
 //

 typedef struct _RP_MEDIACHECK
 {
    RPH   rph;
    UCHAR MediaDescr;
    UCHAR rc;
    PSZ   PrevVolID;
 } RP_MEDIACHECK, FAR *PRP_MEDIACHECK;

 //
 // Open/close
 //

 typedef struct _RP_OPENCLOSE
 {
    RPH    rph;
    USHORT sfn;
 } RP_OPENCLOSE, FAR *PRP_OPENCLOSE;

 //
 // Read/write/write-verify
 //

 typedef struct _RP_RWV
 {
    RPH    rph;
    UCHAR  MediaDescr;
    ULONG  XferAddr;
    USHORT NumSectors;
    ULONG  rba;
    USHORT sfn;
 } RP_RWV, FAR *PRP_RWV;

 //
 // Non-destruct. read
 //

 typedef struct _RP_NONDESTRUCREAD
 {
    RPH   rph;
    UCHAR character;
 } RP_NONDESTRUCREAD, *RPR_NONDESTRUCREAD;

 //
 // Ioctl
 //

 typedef struct _RP_GENIOCTL
 {
    RPH    rph;
    UCHAR  Category;
    UCHAR  Function;
    PUCHAR ParmPacket;
    PUCHAR DataPacket;
    USHORT sfn;
    USHORT ParmLen;
    USHORT DataLen;
 } RP_GENIOCTL, FAR *PRP_GENIOCTL;

 //
 // PRM fixed disks
 //

 typedef struct _RP_PARTFIXEDDISKS
 {
    RPH   rph;
    UCHAR NumFixedDisks;
 } RP_PARTFIXEDDISKS, FAR *PRP_PARTFIXEDDISKS;

 //
 // Get driver caps
 //

 typedef struct _RP_GETDRIVERCAPS
 {
    RPH          rph;
    UCHAR        Reserved[3];
    P_DriverCaps pDCS;
    P_VolChars   pVCS;
 } RP_GETDRIVERCAPS, FAR *PRP_GETDRIVERCAPS;

 //
 // Save/restore
 //

 typedef struct _RPSaveRestore
 {
    RPH   rph;
    UCHAR FuncCode;
 } RPSAVERESTORE, FAR *PRPSAVERESTORE;

 //
 // Shutdown
 //

 typedef struct _RP_SHUTDOWN
 {
    RPH    rph;
    UCHAR  Function;
    USHORT Reserved;
 } RP_SHUTDOWN, FAR *PRP_SHUTDOWN;

 //
 // Get unit map
 //

 typedef struct _RP_GETUNITMAP
 {
    RPH   rph;
    ULONG UnitMap;
 } RP_GETUNITMAP, FAR *PRP_GETUNITMAP;

 #endif /* INCL_INITRP_ONLY */

 #endif /* __REQPQT_H__     */
