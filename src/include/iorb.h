 #ifndef __IORB_H__
 #define __IORB_H__

 #define MAX_IORB_SIZE             128

 //
 // IORB header
 //

 #define ADD_WORKSPACE_SIZE  16
 #define DM_WORKSPACE_SIZE   20

 //
 // forward declarations
 //

 typedef struct _IORBH IORBH;
 typedef struct _IORBH NEAR *NPIORBH;
 typedef struct _IORBH FAR *PIORBH;
 typedef struct _IORBH FAR *PIORB;
 typedef struct _IORBH NEAR *NPIORB;

 typedef struct _ADAPTERINFO ADAPTERINFO;
 typedef struct _ADAPTERINFO NEAR *NPADAPTERINFO;
 typedef struct _ADAPTERINFO FAR *PADAPTERINFO;

 typedef struct _UNITINFO UNITINFO;
 typedef struct _UNITINFO NEAR *NPUNITINFO;
 typedef struct _UNITINFO FAR *PUNITINFO;

 typedef struct _RESOURCE_ENTRY RESOURCE_ENTRY;
 typedef struct _RESOURCE_ENTRY NEAR *NPRESOURCE_ENTRY;
 typedef struct _RESOURCE_ENTRY FAR *PRESOURCE_ENTRY;

 typedef struct _GEOMETRY GEOMETRY;
 typedef struct _GEOMETRY NEAR *NPGEOMETRY;
 typedef struct _GEOMETRY FAR *PGEOMETRY;

 typedef struct _DEVICETABLE DEVICETABLE;
 typedef struct _DEVICETABLE NEAR *NPDEVICETABLE;
 typedef struct _DEVICETABLE FAR *PDEVICETABLE;

 typedef struct _SCATGATENTRY SCATGATENTRY;
 typedef struct _SCATGATENTRY NEAR *NPSCATGATENTRY;
 typedef struct _SCATGATENTRY FAR *PSCATGATENTRY;

 typedef struct _IORBH
 {
    USHORT  Length;
    USHORT  UnitHandle;
    USHORT  CommandCode;
    USHORT  CommandModifier;
    USHORT  RequestControl;
    USHORT  Status;
    USHORT  ErrorCode;
    ULONG   Timeout;
    USHORT  StatusBlockLen;
    NPBYTE  pStatusBlock;
    USHORT  Reserved_1;
    PIORB   pNxtIORB;
    PIORB   (FAR *NotifyAddress)(PIORB);
    UCHAR   DMWorkSpace[DM_WORKSPACE_SIZE];
    UCHAR   ADDWorkSpace[ADD_WORKSPACE_SIZE];
 } IORBH;

 //
 // IORB Command codes (IOCC) and Command modifiers (IOCM)
 //

 #define IOCC_CONFIGURATION        0x0001
 #define IOCM_GET_DEVICE_TABLE     0x0001
 #define IOCM_COMPLETE_INIT        0x0002

 #define IOCC_GEOMETRY             0x0003
 #define IOCM_GET_MEDIA_GEOMETRY   0x0001
 #define IOCM_SET_MEDIA_GEOMETRY   0x0002
 #define IOCM_GET_DEVICE_GEOMETRY  0x0003
 #define IOCM_SET_LOGICAL_GEOMETRY 0x0004

 #define IOCC_UNIT_CONTROL         0x0002
 #define IOCM_ALLOCATE_UNIT        0x0001
 #define IOCM_DEALLOCATE_UNIT      0x0002
 #define IOCM_CHANGE_UNITINFO      0x0003

 #define IOCC_UNIT_STATUS          0x0006
 #define IOCM_GET_UNIT_STATUS      0x0001
 #define IOCM_GET_CHANGELINE_STATE 0x0002
 #define IOCM_GET_MEDIA_SENSE      0x0003
 #define IOCM_GET_LOCK_STATUS      0x0004

 #define IOCC_DEVICE_CONTROL       0x0007
 #define IOCM_ABORT                0x0001
 #define IOCM_RESET                0x0002
 #define IOCM_SUSPEND              0x0003
 #define IOCM_RESUME               0x0004
 #define IOCM_LOCK_MEDIA           0x0005
 #define IOCM_UNLOCK_MEDIA         0x0006
 #define IOCM_EJECT_MEDIA          0x0007
 #define IOCM_GET_QUEUE_STATUS     0x0008

 #define IOCC_RESOURCE             0x0009
 #define IOCM_REPORT_RESOURCES     0x0001

 #define IOCC_FORMAT               0x0005
 #define IOCM_FORMAT_MEDIA         0x0001
 #define IOCM_FORMAT_TRACK         0x0002
 #define IOCM_FORMAT_PROGRESS      0x0003

 #define IOCC_EXECUTE_IO           0x0004
 #define IOCM_READ                 0x0001
 #define IOCM_READ_VERIFY          0x0002
 #define IOCM_READ_PREFETCH        0x0003
 #define IOCM_WRITE                0x0004
 #define IOCM_WRITE_VERIFY         0x0005

 #define IOCC_ADAPTER_PASSTHRU     0x0008
 #define MAX_IOCC_COMMAND          IOCC_ADAPTER_PASSTHRU
 #define IOCM_EXECUTE_SCB          0x0001
 #define IOCM_EXECUTE_CDB          0x0002
 #define IOCM_EXECUTE_ATA          0x0003

 //
 // Error codes
 //

 #define IOERR_CMD                       0x0100
 #define IOERR_CMD_NOT_SUPPORTED         IOERR_CMD+1
 #define IOERR_CMD_SYNTAX                IOERR_CMD+2
 #define IOERR_CMD_SGLIST_BAD            IOERR_CMD+3
 #define IOERR_CMD_SW_RESOURCE           IOERR_CMD+IOERR_RETRY+4
 #define IOERR_CMD_ABORTED               IOERR_CMD+5
 #define IOERR_CMD_ADD_SOFTWARE_FAILURE  IOERR_CMD+6
 #define IOERR_CMD_OS_SOFTWARE_FAILURE   IOERR_CMD+7

 #define IOERR_UNIT                      0x0200
 #define IOERR_UNIT_NOT_ALLOCATED        IOERR_UNIT+1
 #define IOERR_UNIT_ALLOCATED            IOERR_UNIT+2
 #define IOERR_UNIT_NOT_READY            IOERR_UNIT+3
 #define IOERR_UNIT_PWR_OFF              IOERR_UNIT+4

 #define IOERR_RBA                       0x0300
 #define IOERR_RBA_ADDRESSING_ERROR      IOERR_RBA+IOERR_RETRY+1
 #define IOERR_RBA_LIMIT                 IOERR_RBA+2
 #define IOERR_RBA_CRC_ERROR             IOERR_RBA+IOERR_RETRY+3

 #define IOERR_MEDIA                     0x0400
 #define IOERR_MEDIA_NOT_FORMATTED       IOERR_MEDIA+1
 #define IOERR_MEDIA_NOT_SUPPORTED       IOERR_MEDIA+2
 #define IOERR_MEDIA_WRITE_PROTECT       IOERR_MEDIA+3
 #define IOERR_MEDIA_CHANGED             IOERR_MEDIA+4
 #define IOERR_MEDIA_NOT_PRESENT         IOERR_MEDIA+5

 #define IOERR_ADAPTER                   0x0500
 #define IOERR_ADAPTER_HOSTBUSCHECK      IOERR_ADAPTER+1
 #define IOERR_ADAPTER_DEVICEBUSCHECK    IOERR_ADAPTER+IOERR_RETRY+2
 #define IOERR_ADAPTER_OVERRUN           IOERR_ADAPTER+IOERR_RETRY+3
 #define IOERR_ADAPTER_UNDERRUN          IOERR_ADAPTER+IOERR_RETRY+4
 #define IOERR_ADAPTER_DIAGFAIL          IOERR_ADAPTER+5
 #define IOERR_ADAPTER_TIMEOUT           IOERR_ADAPTER+IOERR_RETRY+6
 #define IOERR_ADAPTER_DEVICE_TIMEOUT    IOERR_ADAPTER+IOERR_RETRY+7
 #define IOERR_ADAPTER_REQ_NOT_SUPPORTED IOERR_ADAPTER+8
 #define IOERR_ADAPTER_REFER_TO_STATUS   IOERR_ADAPTER+9
 #define IOERR_ADAPTER_NONSPECIFIC       IOERR_ADAPTER+10

 #define IOERR_DEVICE                    0x0600
 #define IOERR_DEVICE_DEVICEBUSCHECK     IOERR_DEVICE+IOERR_RETRY+1
 #define IOERR_DEVICE_REQ_NOT_SUPPORTED  IOERR_DEVICE+2
 #define IOERR_DEVICE_DIAGFAIL           IOERR_DEVICE+3
 #define IOERR_DEVICE_BUSY               IOERR_DEVICE+IOERR_RETRY+4
 #define IOERR_DEVICE_OVERRUN            IOERR_DEVICE+IOERR_RETRY+5
 #define IOERR_DEVICE_UNDERRUN           IOERR_DEVICE+IOERR_RETRY+6
 #define IOERR_DEVICE_RESET              IOERR_DEVICE+7
 #define IOERR_DEVICE_NONSPECIFIC        IOERR_DEVICE+8
 #define IOERR_DEVICE_ULTRA_CRC          IOERR_DEVICE+IOERR_RETRY+9

 #define IOERR_RETRY                     0x8000

 //
 // Status flags
 //

 #define IORB_DONE                0x0001
 #define IORB_ERROR               0x0002
 #define IORB_RECOV_ERROR         0x0004
 #define IORB_STATUSBLOCK_AVAIL   0x0008

 //
 // Request control flags
 //

 #define  IORB_ASYNC_POST         0x0001
 #define  IORB_CHAIN              0x0002
 #define  IORB_CHS_ADDRESSING     0x0004
 #define  IORB_REQ_STATUSBLOCK    0x0008
 #define  IORB_DISABLE_RETRY      0x0010

 #define ADD_LEVEL_MAJOR          0x01
 #define ADD_LEVEL_MINOR          0x00

 typedef struct _DEVICETABLE
 {
    UCHAR ADDLevelMajor;
    UCHAR ADDLevelMinor;
    USHORT ADDHandle;
    USHORT TotalAdapters;
    NPADAPTERINFO pAdapter[1];
 } DEVICETABLE, FAR *PDEVICETABLE;

 //
 // Configuration Iorb
 //

 #define UIB_TYPE_DISK           0x0000
 #define UIB_TYPE_TAPE           0x0001
 #define UIB_TYPE_PRINTER        0x0002
 #define UIB_TYPE_PROCESSOR      0x0003
 #define UIB_TYPE_WORM           0x0004
 #define UIB_TYPE_CDROM          0x0005
 #define UIB_TYPE_SCANNER        0x0006
 #define UIB_TYPE_OPTICAL_MEMORY 0x0007
 #define UIB_TYPE_CHANGER        0x0008
 #define UIB_TYPE_COMM           0x0009
 #define UIB_TYPE_ATAPI          0x000A
 #define UIB_TYPE_UNKNOWN        0x001F

 #define UF_REMOVABLE            0x0001
 #define UF_CHANGELINE           0x0002
 #define UF_PREFETCH             0x0004
 #define UF_A_DRIVE              0x0008
 #define UF_B_DRIVE              0x0010
 #define UF_NODASD_SUPT          0x0020
 #define UF_NOSCSI_SUPT          0x0040
 #define UF_DEFECTIVE            0x0080
 #define UF_FORCE                0x0100
 #define UF_NOTPRESENT           0x0200
 #define UF_LARGE_FLOPPY         0x0400
 #define UF_USB_DEVICE           0x0800

 typedef struct _UNITINFO
 {
    USHORT AdapterIndex;
    USHORT UnitIndex;
    USHORT UnitFlags;
    USHORT Reserved;
    USHORT UnitHandle;
    USHORT FilterADDHandle;
    USHORT UnitType;
    USHORT QueuingCount;
    UCHAR UnitSCSITargetID;
    UCHAR UnitSCSILUN;
 } UNITINFO;

 #define  AI_DEVBUS_OTHER         0x0000
 #define  AI_DEVBUS_ST506         0x0001
 #define  AI_DEVBUS_ST506_II      0x0002
 #define  AI_DEVBUS_ESDI          0x0003
 #define  AI_DEVBUS_FLOPPY        0x0004
 #define  AI_DEVBUS_SCSI_1        0x0005
 #define  AI_DEVBUS_SCSI_2        0x0006
 #define  AI_DEVBUS_SCSI_3        0x0007
 #define  AI_DEVBUS_NONSCSI_CDROM 0x0008
 #define  AI_DEVBUS_FAST_SCSI     0x0100
 #define  AI_DEVBUS_8BIT          0x0200
 #define  AI_DEVBUS_16BIT         0x0400
 #define  AI_DEVBUS_32BIT         0x0800

 #define  AI_HOSTBUS_OTHER        0x00
 #define  AI_HOSTBUS_ISA          0x01
 #define  AI_HOSTBUS_EISA         0x02
 #define  AI_HOSTBUS_uCHNL        0x03
 #define  AI_HOSTBUS_UNKNOWN      0x0f

 #define  AI_IOACCESS_OTHER       0x00
 #define  AI_IOACCESS_BUS_MASTER  0x01
 #define  AI_IOACCESS_PIO         0x02
 #define  AI_IOACCESS_DMA_SLAVE   0x04
 #define  AI_IOACCESS_MEMORY_MAP  0x08

 #define  AI_BUSWIDTH_8BIT        0x10
 #define  AI_BUSWIDTH_16BIT       0x20
 #define  AI_BUSWIDTH_32BIT       0x30
 #define  AI_BUSWIDTH_64BIT       0x40
 #define  AI_BUSWIDTH_UNKNOWN     0xf0

 #define AF_16M                   0x0001
 #define AF_IBM_SCB               0x0002
 #define AF_HW_SCATGAT            0x0004
 #define AF_CHS_ADDRESSING        0x0008
 #define AF_ASSOCIATED_DEVBUS     0x0010

 typedef struct _ADAPTERINFO
 {
    UCHAR AdapterName[17];
    UCHAR Reserved;
    USHORT AdapterUnits;
    USHORT AdapterDevBus;
    UCHAR AdapterIOAccess;
    UCHAR AdapterHostBus;
    UCHAR AdapterSCSITargetID;
    UCHAR AdapterSCSILUN;
    USHORT AdapterFlags;
    USHORT MaxHWSGList;
    ULONG MaxCDBTransferLength;
    UNITINFO UnitInfo[1];
 } ADAPTERINFO;

 typedef struct _IORB_CONFIGURATION
 {
    IORBH iorbh;
    DEVICETABLE far *pDeviceTable;
    USHORT DeviceTableLen;
 } IORB_CONFIGURATION, FAR *PIORB_CONFIGURATION, NEAR *NPIORB_CONFIGURATION;

 //
 // Geometry Iorb
 //

 typedef struct _GEOMETRY
 {
    ULONG TotalSectors;
    USHORT BytesPerSector;
    USHORT Reserved;
    USHORT NumHeads;
    ULONG TotalCylinders;
    USHORT SectorsPerTrack;
 } GEOMETRY, FAR *PGEOMETRY, NEAR *NPGEOMETRY;

 typedef struct _IORB_GEOMETRY
 {
    IORBH iorbh;
    PGEOMETRY pGeometry;
    USHORT GeometryLen;
 } IORB_GEOMETRY, FAR *PIORB_GEOMETRY, NEAR *NPIORB_GEOMETRY;

 //
 // ExecuteIO Iorb
 //

 #define MAXSGLISTSIZE (sizeof(SCATGATENTRY)) * 16

 typedef struct _SCATGATENTRY
 {
    ULONG ppXferBuf;
    ULONG XferBufLen;
 } SCATGATENTRY, FAR *PSCATGATENTRY, NEAR *NPSCATGATENTRY;

 #define  XIO_DISABLE_HW_WRITE_CACHE      0x0001
 #define  XIO_DISABLE_HW_READ_CACHE       0x0002

 typedef struct _CHS_ADDR
 {
    USHORT Cylinder;
    UCHAR Head;
    UCHAR Sector;
 } CHS_ADDR, FAR *PCHS_ADDR, NEAR *NPCHS_ADDR;

 typedef struct _IORB_EXECUTEIO
 {
    IORBH iorbh;
    USHORT cSGList;
    PSCATGATENTRY pSGList;
    ULONG ppSGList;
    ULONG RBA;
    USHORT BlockCount;
    USHORT BlocksXferred;
    USHORT BlockSize;
    USHORT Flags;
 } IORB_EXECUTEIO, FAR *PIORB_EXECUTEIO, NEAR *NPIORB_EXECUTEIO;

 //
 // Unit status Iorb
 //

 #define US_MEDIA_UNKNOWN      0x0000
 #define US_MEDIA_720KB        0x0001
 #define US_MEDIA_144MB        0x0002
 #define US_MEDIA_288MB        0x0003
 #define US_MEDIA_LARGE_FLOPPY 0x0004

 #define US_CHANGELINE_ACTIVE  0x0001

 #define US_READY              0x0001
 #define US_POWER              0x0002
 #define US_DEFECTIVE          0x0004

 #define US_LOCKED             0x0001

 typedef struct _IORB_UNIT_STATUS
 {
    IORBH iorbh;
    USHORT UnitStatus;
 } IORB_UNIT_STATUS, FAR *PIORB_UNIT_STATUS, NEAR *NPIORB_UNIT_STATUS;

 //
 // Adapter passthrough Iorb
 //

 #define PT_DIRECTION_IN   0x0001

 typedef struct _IORB_ADAPTER_PASSTHRU
 {
    IORBH iorbh;
    USHORT cSGList;
    PSCATGATENTRY pSGList;
    ULONG ppSGLIST;
    USHORT ControllerCmdLen;
    PBYTE pControllerCmd;
    ULONG ppSCB;
    USHORT Flags;
 } IORB_ADAPTER_PASSTHRU, FAR *PIORB_ADAPTER_PASSTHRU, NEAR *NPIORB_ADAPTER_PASSTHRU;

 //
 // Resource Iorb
 //

 typedef struct _UNIT_ENTRY NEAR *NPUNIT_ENTRY;
 typedef struct _UNIT_ENTR  FAR *PUNIT_ENTRY;
 typedef struct _MEMORY_ENTRY NEAR *NPMEMORY_ENTRY;
 typedef struct _MEMORY_ENTRY FAR *PMEMORY_ENTRY;
 typedef struct _IRQ_ENTRY NEAR *NPIRQ_ENTRY;
 typedef struct _IRQ_ENTRY FAR *PIRQ_ENTRY;
 typedef struct _DMA_ENTRY NEAR *NPDMA_ENTRY;
 typedef struct _DMA_ENTRY FAR *PDMA_ENTRY;
 typedef struct _PCI_IRQ_ENTRY NEAR *NPPCI_IRQ_ENTRY;
 typedef struct _PCI_IRQ_ENTRY FAR *PPCI_IRQ_ENTRY;
 typedef struct _PORT_ENTRY NEAR *NPPORT_ENTRY;
 typedef struct _PORT_ENTRY FAR *PPORT_ENTRY;

 #define MEMORY_8_BIT             1
 #define MEMORY_16_BIT            2

 #define MEMORY_WRITEABLE         0x0001
 #define MEMORY_CACHEABLE         0x0002
 #define MEMORY_SHADOWABLE        0x0004
 #define MEMORY_ROM               0x0008

 typedef struct _MEMORY_ENTRY
 {
    USHORT Memory_Flags;
    USHORT Memory_Width;
    ULONG StartMemory;
    USHORT cMemory;
 } MEMORY_ENTRY;

 #define RE_DMA_BUS_MASTER      0x0001

 #define RE_DMA_TYPE_A          1
 #define RE_DMA_TYPE_B          2
 #define RE_DMA_TYPE_F          3

 #define RE_DMA_WIDTH_8_BIT     1
 #define RE_DMA_WIDTH_16_BIT    2

 typedef struct _DMA_ENTRY
 {
    USHORT DMA_Flags;
    USHORT DMA_Width;
    USHORT DMA_Type;
    USHORT DMA_Channel;
    USHORT DMA_Arbitration_Level;
 } DMA_ENTRY;

 #define IRQ_RISING_EDGE_TRIGGERED    0x0001
 #define IRQ_FALLING_EDGE_TRIGGERED   0x0002
 #define IRQ_LOW_LEVEL_TRIGGERED      0x0004
 #define IRQ_HIGH_LEVEL_TRIGGERED     0x0008

 typedef struct _IRQ_ENTRY
 {
    USHORT IRQ_Flags;
    USHORT IRQ_Value;
 } IRQ_ENTRY;

 typedef struct _PCI_IRQ_ENTRY
 {
    USHORT PCI_IRQ_Flags;
    USHORT PCI_IRQ_Value;
 } PCI_IRQ_ENTRY;

 typedef struct _PORT_ENTRY
 {
    USHORT Port_Flags;
    USHORT StartPort;
    USHORT cPorts;
 } PORT_ENTRY;

 #define RE_DMA       1
 #define RE_IRQ       2
 #define RE_PORT      3
 #define RE_MEMORY    4
 #define RE_PCI_IRQ   5

 #define RE_ADAPTER_RESOURCE  0x1000
 #define RE_SYSTEM_RESOURCE   0x2000
 #define RE_RESOURCE_CONFLICT 0x4000

 typedef struct _RESOURCE_ENTRY
 {
    USHORT Max_Resource_Entry;
    USHORT cDMA_Entries;
    NPDMA_ENTRY npDMA_Entry;
    USHORT cIRQ_Entries;
    NPIRQ_ENTRY npIRQ_Entry;
    USHORT cPort_Entries;
    NPPORT_ENTRY npPort_Entry;
    USHORT cMemory_Entries;
    NPMEMORY_ENTRY npMemory_Entry;
    USHORT cPCI_IRQ_Entries;
    NPIRQ_ENTRY npPCI_IRQ_Entry;
    ULONG Reserved[4];
 } RESOURCE_ENTRY, NEAR *NPRESOURCE_ENTRY, FAR *PRESOURCE_ENTRY;

 typedef struct _IORB_RESOURCE
 {
    IORBH iorbh;
    USHORT Flags;
    USHORT ResourceEntryLen;
    PRESOURCE_ENTRY pResourceEntry;
 } IORB_RESOURCE, FAR *PIORB_RESOURCE, NEAR *NPIORB_RESOURCE;

 //
 // Device control Iorb
 //

 #define DC_Queue_Empty            0

 #define DC_SUSPEND_DEFERRED       0x0000
 #define DC_SUSPEND_IMMEDIATE      0x0001
 #define DC_SUSPEND_IRQADDR_VALID  0x0002

 typedef struct _IORB_DEVICE_CONTROL
 {
    IORBH iorbh;
    USHORT Flags;
    USHORT Reserved;
    USHORT QueueStatus;
    USHORT (FAR *IRQHandlerAddress)();
 } IORB_DEVICE_CONTROL, FAR *PIORB_DEVICE_CONTROL, NEAR *NPIORB_DEVICE_CONTROL;

 //
 // Unit control Iorb
 //

 typedef struct _IORB_UNIT_CONTROL
 {
    IORBH iorbh;
    USHORT Flags;
    PUNITINFO pUnitInfo;
    USHORT UnitInfoLen;
 } IORB_UNIT_CONTROL, FAR *PIORB_UNIT_CONTROL, NEAR *NPIORB_UNIT_CONTROL;

 //
 // Format Iorb
 //

 #define FF_VERIFY       0x0001
 #define FF_FMTGAPLEN    0x0002

 typedef struct _FORMAT_CMD_TRACK
 {
    USHORT Flags;
    ULONG RBA;
    USHORT cTrackEntries;
    USHORT FmtGapLength;
    USHORT BlockSize;
 } FORMAT_CMD_TRACK, FAR *PFORMAT_CMD_TRACK, NEAR *NPFORMAT_CMD_TRACK;

 typedef struct _IORB_FORMAT
 {
    IORBH iorbh;
    USHORT cSGList;
    PSCATGATENTRY pSGList;
    ULONG ppSGList;
    USHORT FormatCmdLen;
    PBYTE pFormatCmd;
    UCHAR Reserved1[8];
 } IORB_FORMAT, FAR *PIORB_FORMAT, NEAR *NPIORB_FORMAT;

 #endif /* __IORB_H__ */
