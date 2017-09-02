 #ifndef __STRAT2_H__
 #define __STRAT2_H__

 //
 // Req list header
 //

 #define RLH_No_Req_Queued     0x00
 #define RLH_Req_Not_Queued    0x01
 #define RLH_All_Req_Queued    0x02
 #define RLH_All_Req_Done      0x04
 #define RLH_Seq_In_Process    0x08
 #define RLH_Abort_pendings    0x08

 #define RLH_Req_From_PB       0x0001
 #define RLH_Single_Req        0x0002
 #define RLH_Exe_Req_Seq       0x0004
 #define RLH_Abort_Err         0x0008
 #define RLH_Notify_Err        0x0010
 #define RLH_Notify_Done       0x0020

 #define RLH_No_Error          0x00
 #define RLH_Rec_Error         0x10
 #define RLH_Unrec_Error       0x20
 #define RLH_Unrec_Error_Retry 0x30

 typedef struct _Req_List_Header
 {
    ULONG  Count;
    PVOID  Notify_Address;
    USHORT Request_Control;
    BYTE   Block_Dev_Unit;
    BYTE   Lst_Status;
    ULONG  y_Done_Count;
    ULONG  y_PhysAddr;
 } Req_List_Header;

 typedef struct _Req_List_Header_1
 {
    USHORT Short_Count;
    USHORT Dummy1;
    PVOID  Dummy2;
    USHORT Dummy3;
    BYTE   Dummy4;
    BYTE   Dummy5;
    USHORT Count_Done;
    USHORT Queued;
    USHORT Next_RH;
    USHORT Current_RH;
 } Req_List_Header_1;

 //
 // A request
 //

 typedef struct _Req_Header
 {
    USHORT  Length;
    BYTE    Old_Command;
    BYTE    Command_Code;
    ULONG   Head_Offset;
    BYTE    Req_Control;
    BYTE    Priority;
    BYTE    Status;
    BYTE    Error_Code;
    PVOID   Notify_Address;
    ULONG   Hint_Pointer;
    ULONG   Waiting;
    ULONG   FT_Orig_Pkt;
    ULONG   Physical;
 } Req_Header;

 #define RH_LAST_REQ  0xFFFF
 #define PB_REQ_LIST  0x1C

 //
 // strat2 requests
 //

 #define PB_READ_X     0x1E
 #define PB_WRITE_X    0x1F
 #define PB_WRITEV_X   0x20
 #define PB_PREFETCH_X 0x21

 //
 // strat2 request status
 //

 #define RH_NOT_QUEUED         0x00
 #define RH_QUEUED             0x01
 #define RH_PROCESSING         0x02
 #define RH_DONE               0x04
 #define RH_NO_ERROR           0x00
 #define RH_RECOV_ERROR        0x10
 #define RH_UNREC_ERROR        0x20
 #define RH_UNREC_ERROR_RETRY  0x30
 #define RH_ABORTED            0x40

 //
 // start2 error code
 //

 #define RH_PB_REQUEST         0x01
 #define RH_NOTIFY_ERROR       0x10
 #define RH_NOTIFY_DONE        0x20

 //
 // Read/write
 //

 #define RW_Cache_WriteThru    0x0001
 #define RW_Cache_Req          0x0002

 //
 // priority
 //

 #define PRIO_PREFETCH         0x00
 #define PRIO_LAZY_WRITE       0x01
 #define PRIO_PAGER_READ_AHEAD 0x02
 #define PRIO_BACKGROUND_USER  0x04
 #define PRIO_FOREGROUND_USER  0x08
 #define PRIO_PAGER_HIGH       0x10
 #define PRIO_URGENT           0x80

 typedef struct _PB_Read_Write
 {
    Req_Header RqHdr;
    ULONG  Start_Block;
    ULONG  Block_Count;
    ULONG  Blocks_Xferred;
    USHORT RW_Flags;
    USHORT SG_Desc_Count;
    USHORT SG_Desc_Count2;
    USHORT reserved;
 } PB_Read_Write;

 //
 // Driver caps
 //

 #define GDC_DD_Read2    0x00000001
 #define GDC_DD_DMA_Word 0x00000002
 #define GDC_DD_DMA_Byte 0x00000006
 #define GDC_DD_Mirror   0x00000008
 #define GDC_DD_Duplex   0x00000010
 #define GDC_DD_No_Block 0x00000020
 #define GDC_DD_16M      0x00000040

 typedef struct _DriverCaps
 {
    USHORT Reserved;
    BYTE   VerMajor;
    BYTE   VerMinor;
    ULONG  Capabilities;
    PVOID  Strategy2;
    PVOID  EndofInt;
    PVOID  ChgPriority;
    PVOID  SetRestPos;
    PVOID  GetBoundary;
 } DriverCaps, FAR *P_DriverCaps;

 //
 // Volume characteristics
 //

 #define VC_REMOVABLE_MEDIA 0x0001
 #define VC_READ_ONLY       0x0002
 #define VC_RAM_DISK        0x0004
 #define VC_HWCACHE         0x0008
 #define VC_SCB             0x0010
 #define VC_PREFETCH        0x0020

 typedef struct _VolChars
 {
    USHORT VolDescriptor;
    USHORT AvgSeekTime;
    USHORT AvgLatency;
    USHORT TrackMinBlocks;
    USHORT TrackMaxBlocks;
    USHORT HeadsPerCylinder;
    ULONG  VolCylinderCount;
    ULONG  VolMedianBlock;
    USHORT MaxSGList;
 } VolChars, FAR * P_VolChars;

 //
 // Scatter/gather descriptor
 //

 typedef struct _SG_Descriptor
 {
    PVOID BufferPtr;
    ULONG BufferSize;
 } SG_Descriptor;

 typedef SG_Descriptor SGENTRY;
 typedef SGENTRY FAR *PSGENTRY;

 #endif /* __STRAT2_H__ */
