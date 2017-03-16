#ifndef FAT32DEF_H
#define FAT32DEF_H

#include "portable.h"

#if defined(_WIN32) && !defined(OS2DEF_INCLUDED)

typedef struct _FDATE {
    USHORT day:5;
    USHORT month:4;
    USHORT year:7;
} FDATE, *PFDATE;

typedef struct _FTIME {
    USHORT twosecs:5;
    USHORT minutes:6;
    USHORT hours:5;
} FTIME, *PFTIME;

#endif

#define _FAR_

#if defined(__WATCOM) && defined(__16BITS__)
/* File info levels: Dos...PathInfo only */
#define FIL_STANDARD           1   /* Info level 1, standard file info */
#define FIL_QUERYEASIZE        2   /* Level 2, return Full EA size     */
#define FIL_QUERYEASFROMLIST   3   /* Level 3, return requested EA's   */
#define FIL_QUERYALLEAS        4   /* Level 4, return all EA's         */
#define FIL_QUERYFULLNAME      5   /* Level 5, return fully qualified  */
                                   /*   name of file                   */
#define FIL_NAMEISVALID        6   /* Level 6, check validity of       */
#define FIL_STANDARDL         11
#define FIL_QUERYEASIZEL      12
#define FIL_QUERYEASFROMLISTL 13

#pragma pack(2)
typedef struct _FILESTATUS2 {  /* fsts2 */
    FDATE   fdateCreation;
    FTIME   ftimeCreation;
    FDATE   fdateLastAccess;
    FTIME   ftimeLastAccess;
    FDATE   fdateLastWrite;
    FTIME   ftimeLastWrite;
    ULONG   cbFile;
    ULONG   cbFileAlloc;
    USHORT  attrFile;
    ULONG   cbList;
} FILESTATUS2;
typedef FILESTATUS2 FAR *PFILESTATUS2;

typedef struct _FILESTATUS3L     /* fsts3L */
{
   FDATE    fdateCreation;
   FTIME    ftimeCreation;
   FDATE    fdateLastAccess;
   FTIME    ftimeLastAccess;
   FDATE    fdateLastWrite;
   FTIME    ftimeLastWrite;
   LONGLONG cbFile;
   LONGLONG cbFileAlloc;
   ULONG    attrFile;
} FILESTATUS3L;
typedef FILESTATUS3L FAR *PFILESTATUS3L;

typedef struct _FILESTATUS4L      /* fsts4L */
{
   FDATE    fdateCreation;
   FTIME    ftimeCreation;
   FDATE    fdateLastAccess;
   FTIME    ftimeLastAccess;
   FDATE    fdateLastWrite;
   FTIME    ftimeLastWrite;
   LONGLONG cbFile;
   LONGLONG cbFileAlloc;
   ULONG    attrFile;
   ULONG    cbList;
} FILESTATUS4L;
typedef FILESTATUS4L FAR *PFILESTATUS4L;
#pragma pack()

#define ERROR_EA_FILE_CORRUPT   276     /* MSG%ERROR_EAS_CORRUPT */
#endif

#ifdef __OS2__
#define PARTITION_ENTRY_UNUSED          0x00
#define PARTITION_FAT_12                0x01
#define PARTITION_XENIX_1               0x02
#define PARTITION_XENIX_2               0x03
#define PARTITION_FAT_16                0x04
#define PARTITION_EXTENDED              0x05
#define PARTITION_HUGE                  0x06
#define PARTITION_IFS                   0x07
#define PARTITION_BOOTMANAGER           0x0A
#define PARTITION_FAT32                 0x0B
#define PARTITION_FAT32_XINT13          0x0C
#define PARTITION_XINT13                0x0E
#define PARTITION_XINT13_EXTENDED       0x0F
#define PARTITION_PREP                  0x41
#define PARTITION_UNIX                  0x63
#define VALID_NTFT                      0xC0
#define PARTITION_LINUX                  0x83
#define PARTITION_HIDDEN                0x10
#endif

#if defined(_WIN32) && !defined(OS2DEF_INCLUDED)
//typedef unsigned short APIRET;
#endif

#ifndef FP_SEG
#define FP_SEG(fp) (*((unsigned _far *)&(fp)+1))
#define FP_OFF(fp) (*((unsigned _far *)&(fp)))
#endif

#include "ver.h"

#ifndef __MSC__
#define _enable()	_asm { sti }
#define _disable()	_asm { cli }
#endif

#define FSINFO_OFFSET  484
#define MBRTABLEOFFSET 446
#define FILE_VOLID     0x0008
#define FILE_LONGNAME  0x000F
#define FILE_HIDE      0x8000
#define FAT12_EOF        0x0FFF
#define FAT12_EOF2       0x0FF8
#define FAT12_BAD_CLUSTER 0x0FF7
#define FAT16_EOF        0x0FFFF
#define FAT16_EOF2       0x0FFF8
#define FAT16_BAD_CLUSTER 0x0FFF7
#define FAT32_EOF        0x0FFFFFFF
#define FAT32_EOF2       0x0FFFFFF8
#define FAT32_BAD_CLUSTER 0x0FFFFFF7
#define EXFAT_EOF        0xFFFFFFFF
#define EXFAT_EOF2       0xFFFFFFF8
#define EXFAT_BAD_CLUSTER 0xFFFFFFF7
#define FAT_ASSIGN_NEW 0xFFFFFFFF
#define FAT_NOTUSED    0x00000000
#define SECTOR_SIZE    512
#define MAX_CLUSTER_SIZE 65536
#define MAX_RASECTORS  128
#define MAX_DRIVES     10
#define DELETED_ENTRY  0xE5
#define ERROR_VOLUME_NOT_MOUNTED 0xEE00
#ifdef __OS2__
#define ERROR_VOLUME_DIRTY 627
#endif
#define FAT32MAXPATHCOMP 250
#define FAT32MAXPATH 260
#define OPEN_ACCESS_EXECUTE   0x0003
#define MAX_EA_SIZE ((ULONG)65536)
#define EA_EXTENTION " EA. SF"
#define FILE_HAS_NO_EAS             0x00
#define FILE_HAS_EAS                0x40
#define FILE_HAS_CRITICAL_EAS       0x80
#define FILE_HAS_OLD_EAS            0xEA
#define FILE_HAS_OLD_CRITICAL_EAS   0xEC

#define HAS_EAS( fEAS ) ((( fEAS ) & FILE_HAS_EAS  ) ||\
                         (( fEAS ) & FILE_HAS_CRITICAL_EAS ))
#define HAS_CRITICAL_EAS( fEAS ) (( fEAS ) & FILE_HAS_CRITICAL_EAS )
#define HAS_OLD_EAS( fEAS ) ((( fEAS ) == FILE_HAS_OLD_EAS ) || (( fEAS ) == FILE_HAS_OLD_CRITICAL_EAS ))

#define FILE_HAS_WINNT_EXT       ( FILE_HAS_WINNT_EXT_NAME | FILE_HAS_WINNT_EXT_EXT )
#define FILE_HAS_WINNT_EXT_NAME  0x08
#define FILE_HAS_WINNT_EXT_EXT   0x10

#define HAS_WINNT_EXT( fEAS ) (( fEAS ) & FILE_HAS_WINNT_EXT )
#define HAS_WINNT_EXT_NAME( fEAS ) (( fEAS ) & FILE_HAS_WINNT_EXT_NAME )
#define HAS_WINNT_EXT_EXT( fEAS ) (( fEAS ) & FILE_HAS_WINNT_EXT_EXT )

#define FAT12_CLEAN_SHUTDOWN  0x080
#define FAT12_NO_DISK_ERROR   0x040
#define FAT16_CLEAN_SHUTDOWN  0x0800
#define FAT16_NO_DISK_ERROR   0x0400
#define FAT32_CLEAN_SHUTDOWN  0x08000000
#define FAT32_NO_DISK_ERROR   0x04000000
#define EXFAT_CLEAN_SHUTDOWN  0x80000000
#define EXFAT_NO_DISK_ERROR   0x40000000

/* FSCTL function numbers */
#define FAT32_GETLOGDATA        0x8000
#define FAT32_SETMESSAGE        0x8001
#define FAT32_STARTLW           0x8002
#define FAT32_STOPLW            0x8003
#define FAT32_DOLW              0x8004
#define FAT32_SETPARMS          0x8005
#define FAT32_GETPARMS          0x8006
#define FAT32_SETTRANSTABLE     0x8007
#define FAT32_WIN2OS            0x8008
#define FAT32_EMERGTHREAD       0x8009
#define FAT32_QUERYSHORTNAME    0x800A
#define FAT32_GETCASECONVERSION 0x800B
#define FAT32_GETFIRSTINFO      0x800C

#define FAT32_SECTORIO        0x9014

/* IOCTL function numbers */
#define IOCTL_FAT32        IOCTL_GENERAL
#define FAT32_SETRASECTORS    0xF0
#define FAT32_GETVOLCLEAN     0xF1
#define FAT32_MARKVOLCLEAN    0xF2
#define FAT32_FORCEVOLCLEAN   0xF3
#define FAT32_RECOVERCHAIN    0xF4
#define FAT32_DELETECHAIN     0xF5
#define FAT32_QUERYRASECTORS  0xF6
#define FAT32_GETFREESPACE    0xF7
#define FAT32_SETFILESIZE     0xF8
#define FAT32_QUERYEAS        0xF9
#define FAT32_SETEAS          0xFA
#define FAT32_SETCLUSTER      0xFB
#define FAT32_READCLUSTER     0xFC
#define FAT32_READSECTOR      0xFD
#define FAT32_WRITECLUSTER    0xFE
#define FAT32_WRITESECTOR     0xFF

#define FAT_TYPE_NONE  0
#define FAT_TYPE_FAT12 1
#define FAT_TYPE_FAT16 2
#define FAT_TYPE_FAT32 3
#define FAT_TYPE_EXFAT 4

#pragma pack(1)

typedef struct _BPB
{
// common for all FAT's
USHORT BytesPerSector;
UCHAR  SectorsPerCluster;
USHORT ReservedSectors;
BYTE   NumberOfFATs;
USHORT RootDirEntries;
USHORT TotalSectors;
BYTE   MediaDescriptor;
USHORT SectorsPerFat;
USHORT SectorsPerTrack;
USHORT Heads;
ULONG  HiddenSectors;
ULONG  BigTotalSectors;
// fat32 specific
ULONG  BigSectorsPerFat;
USHORT ExtFlags;
USHORT FS_Version;
ULONG  RootDirStrtClus;
USHORT FSinfoSec;
USHORT BkUpBootSec;
BYTE   bReserved[12];
} BPB, *PBPB;

typedef struct _BPB0
{
// common for all FAT's
USHORT BytesPerSector;
UCHAR  SectorsPerCluster;
USHORT ReservedSectors;
BYTE   NumberOfFATs;
USHORT RootDirEntries;
USHORT TotalSectors;
BYTE   MediaDescriptor;
USHORT SectorsPerFat;
USHORT SectorsPerTrack;
USHORT Heads;
ULONG  HiddenSectors;
ULONG  BigTotalSectors;
} BPB0, *PBPB0;

//#pragma pack(1)

typedef struct _Fat32Parms
{
ULONG  ulDiskIdle;
ULONG  ulBufferIdle;
ULONG  ulMaxAge;
USHORT fMessageActive;
USHORT fEAS;
BYTE   szVersion[10];

USHORT fLW;
USHORT fInShutDown;
ULONG  ulTotalReads;
ULONG  ulTotalHits;
ULONG  ulTotalRA;
USHORT usCacheUsed;
USHORT usDirtySectors;
USHORT usPendingFlush;
USHORT usDirtyTreshold;
USHORT volatile usCacheSize;
USHORT usSegmentsAllocated;
USHORT fTranslateNames;
ULONG  ulCurCP;
USHORT fHighMem;
USHORT fForceLoad;
USHORT fCalcFree;
CHAR   fLargeFiles;
CHAR   fReadonly;
CHAR   fFat;
CHAR   fExFat;
} F32PARMS, *PF32PARMS;

typedef struct _Options
{
volatile USHORT fTerminate;
ULONG  ulLWTID;
ULONG  ulEMTID;
BYTE   bLWPrio;
} LWOPTS, * PLWOPTS;

typedef struct _BootSector
{
BYTE bJmp[3];
BYTE oemID[8];
BPB  bpb;
BYTE bUnknown[3];
ULONG ulVolSerial;
BYTE VolumeLabel[11];
BYTE FileSystem[8];
} BOOTSECT, *PBOOTSECT;

typedef struct _BootSector0
{
BYTE bJmp[3];
BYTE oemID[8];
BPB0  bpb;
BYTE bUnknown[3];
ULONG ulVolSerial;
BYTE VolumeLabel[11];
BYTE FileSystem[8];
} BOOTSECT0, *PBOOTSECT0;

typedef struct _BOOTFSINFO
{
ULONG ulInfSig;
ULONG ulFreeClusters;
ULONG ulNextFreeCluster;
ULONG ulReserved[3];
} BOOTFSINFO, *PBOOTFSINFO;

typedef struct _DiskOffset
{
BYTE   Head ;
int    Sectors:6;
int    Cylinders256:2;
BYTE   Cylinders;
} DISKOFFSET;

typedef struct _Partition
{
BYTE        iBoot;
DISKOFFSET  Start;
BYTE        cSystem;
DISKOFFSET  End;
ULONG       ulRelSectorNr;
ULONG       ulTotalSectors;
} PARTITIONENTRY, *PPARTITIONENTRY;

typedef struct _MasterBootrecord
{
PARTITIONENTRY partition[4];
} MBR, *PMBR;

typedef struct _DirEntry
{
BYTE   bFileName[8];
BYTE   bExtention[3];
BYTE   bAttr;
BYTE   fEAS;
BYTE   bReserved;
FTIME  wCreateTime;
FDATE  wCreateDate;
FDATE  wAccessDate;
USHORT wClusterHigh;

FTIME wLastWriteTime;
FDATE wLastWriteDate;
USHORT wCluster;
ULONG  ulFileSize;
} DIRENTRY, * PDIRENTRY;

typedef struct _LongNameEntry
{
BYTE   bNumber;
USHORT usChar1[5];
BYTE   bAttr;
BYTE   bReserved;
BYTE   bVFATCheckSum;
USHORT usChar2[6];
USHORT wCluster;
USHORT usChar3[2];
} LNENTRY, * PLNENTRY;

typedef struct _FileSizeStruct
{
BYTE   szFileName[FAT32MAXPATH];
ULONG  ulFileSize;
} FILESIZEDATA, *PFILESIZEDATA;

typedef struct _MarkFileEASBuf
{
BYTE   szFileName[FAT32MAXPATH];
BYTE   fEAS;
} MARKFILEEASBUF, *PMARKFILEEASBUF;

typedef struct _SetClusterData
{
ULONG ulCluster;
ULONG ulNextCluster;
} SETCLUSTERDATA, *PSETCLUSTERDATA;

typedef struct _ReadSectorData
{
ULONG ulSector;
USHORT nSectors;
} READSECTORDATA, *PREADSECTORDATA;

typedef struct _WriteSectorData
{
ULONG ulSector;
USHORT nSectors;
} WRITESECTORDATA, *PWRITESECTORDATA;

#define BLOCK_SIZE  0x4000
#define MAX_MESSAGE 2048
#define TYPE_LONG    0
#define TYPE_STRING  1
#define TYPE_PERC    2
#define TYPE_LONG2   3
#define TYPE_DOUBLE  4
#define TYPE_DOUBLE2 5

#define FAT_ERROR 0xFFFFFFFF
#define MAX_LOST_CHAINS 1024

typedef struct _DiskInfo
{
ULONG filesys_id;
ULONG sectors_per_cluster;
ULONG total_clusters;
ULONG avail_clusters;
WORD  bytes_per_sector;
} DISKINFO, *PDISKINFO;


typedef struct _CDInfo
{
BYTE        szDrive[3];
HANDLE      hDisk;
BOOTSECT    BootSect;
BOOTFSINFO  FSInfo;
ULONG       ulStartOfData;
ULONG       ulActiveFatStart;
ULONG       ulClusterSize;
ULONG       ulTotalClusters;
ULONG       ulCurFATSector;
BYTE        pbFATSector[512];
BOOL        fDetailed;
BOOL        fPM;
BOOL        fFix;
BOOL        fAutoCheck;
BOOL        fAutoRecover;
BOOL        fFatOk;
BOOL        fCleanOnBoot;
ULONG       ulFreeClusters;
ULONG       ulUserClusters;
ULONG       ulHiddenClusters;
ULONG       ulDirClusters;
ULONG       ulEAClusters;
ULONG       ulLostClusters;
ULONG       ulBadClusters;
ULONG       ulRecoveredClusters;
ULONG       ulRecoveredFiles;
ULONG       ulUserFiles;
ULONG       ulTotalDirs;
ULONG       ulHiddenFiles;
ULONG       ulErrorCount;
ULONG       ulTotalChains;
ULONG       ulFragmentedChains;
UCHAR       bFatType;
ULONG       ulFatEof;
ULONG       ulFatEof2;
ULONG       ulFatBad;
ULONG       ulFatClean;
BYTE _huge * pFatBits;
//BYTE      * pFatBits;
DISKINFO    DiskInfo;
USHORT      usLostChains;
ULONG       rgulLost[MAX_LOST_CHAINS];
ULONG       rgulSize[MAX_LOST_CHAINS];

} CDINFO, *PCDINFO;

#pragma pack()




#endif
