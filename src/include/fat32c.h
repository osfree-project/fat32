/*   FAT32 defs
 *
 */

#ifdef __WATCOMC__
#include <ctype.h>
#endif

#include "fat32def.h"

#if defined(__OS2__) //&& !defined(OS2_INCLUDED)

#include <stdarg.h>

INT cdecl iShowMessage(PCDINFO pCD, USHORT usNr, USHORT usNumFields, ...);
INT cdecl iShowMessage2(PCDINFO pCD, USHORT usNr, USHORT usNumFields, va_list va);

#else

#define LOUSHORT(l) ((USHORT)((ULONG)l))
#define HIUSHORT(l) ((USHORT)(((ULONG)(l) >> 16) & 0xffff))

#include <windows.h>
#define UNICODE
#include <shlwapi.h>
#include <winioctl.h>  // From the Win32 SDK \Mstools\Include, or Visual Studio.Net

#ifdef GetFreeSpace
#undef GetFreeSpace
#endif

// This is just so it will build with old versions of Visual Studio. Yeah, I know...
#ifndef IOCTL_DISK_GET_PARTITION_INFO_EX
	#define IOCTL_DISK_GET_PARTITION_INFO_EX    CTL_CODE(IOCTL_DISK_BASE, 0x0012, METHOD_BUFFERED, FILE_ANY_ACCESS)

	typedef struct _PARTITION_INFORMATION_MBR {
		BYTE  PartitionType;
		BOOLEAN BootIndicator;
		BOOLEAN RecognizedPartition;
		DWORD HiddenSectors;
	} PARTITION_INFORMATION_MBR, *PPARTITION_INFORMATION_MBR;

	typedef struct _PARTITION_INFORMATION_GPT {
		GUID PartitionType;                 // Partition type. See table 16-3.
		GUID PartitionId;                   // Unique GUID for this partition.
		DWORD64 Attributes;                 // See table 16-4.
		WCHAR Name [36];                    // Partition Name in Unicode.
	} PARTITION_INFORMATION_GPT, *PPARTITION_INFORMATION_GPT;


	typedef enum _PARTITION_STYLE {
		PARTITION_STYLE_MBR,
		PARTITION_STYLE_GPT,
		PARTITION_STYLE_RAW
	} PARTITION_STYLE;

	typedef struct _PARTITION_INFORMATION_EX {
		PARTITION_STYLE PartitionStyle;
		LARGE_INTEGER StartingOffset;
		LARGE_INTEGER PartitionLength;
		DWORD PartitionNumber;
		BOOLEAN RewritePartition;
		union {
			PARTITION_INFORMATION_MBR Mbr;
			PARTITION_INFORMATION_GPT Gpt;
		} DUMMYUNIONNAME;
	} PARTITION_INFORMATION_EX, *PPARTITION_INFORMATION_EX;
#endif

#ifndef FSCTL_ALLOW_EXTENDED_DASD_IO
 #define FSCTL_ALLOW_EXTENDED_DASD_IO 0x00090083
#endif

#endif

#define MAX_MESSAGE 2048

//#define BLOCK_SIZE  0x4000 // read multiples of three sectors, 
#define BLOCK_SIZE  0x3000   // to fit a whole number of FAT12 entries
#define MAX_MESSAGE 2048
#define TYPE_LONG    0
#define TYPE_STRING  1
#define TYPE_PERC    2
#define TYPE_LONG2   3
#define TYPE_DOUBLE  4
#define TYPE_DOUBLE2 5

#pragma pack(push, 1)
typedef struct tagFAT_BOOTSECTOR16
{
    // Common fields.
    BYTE sJmpBoot[3];
    BYTE sOEMName[8];
    WORD wBytsPerSec;
    BYTE bSecPerClus;
    WORD wRsvdSecCnt;
    BYTE bNumFATs;
    WORD wRootEntCnt;
    WORD wTotSec16; // if zero, use dTotSec32 instead
    BYTE bMedia;
    WORD wFATSz16;
    WORD wSecPerTrk;
    WORD wNumHeads;
    DWORD dHiddSec;
    DWORD dTotSec32;
    // Fat 12/16 only
    BYTE bDrvNum;
    BYTE Reserved1;
    BYTE bBootSig; // == 0x29 if next three fields are ok
    DWORD dBS_VolID;
    BYTE sVolLab[11];
    BYTE sBS_FilSysType[8];
} FAT_BOOTSECTOR16;

typedef struct tagFAT_BOOTSECTOR32
{
    // Common fields.
    BYTE sJmpBoot[3];
    BYTE sOEMName[8];
    WORD wBytsPerSec;
    BYTE bSecPerClus;
    WORD wRsvdSecCnt;
    BYTE bNumFATs;
    WORD wRootEntCnt;
    WORD wTotSec16; // if zero, use dTotSec32 instead
    BYTE bMedia;
    WORD wFATSz16;
    WORD wSecPerTrk;
    WORD wNumHeads;
    DWORD dHiddSec;
    DWORD dTotSec32;
    // Fat 32 only
    DWORD dFATSz32;
    WORD wExtFlags;
    WORD wFSVer;
    DWORD dRootClus;
    WORD wFSInfo;
    WORD wBkBootSec;
    BYTE Reserved[12];
    BYTE bDrvNum;
    BYTE Reserved1;
    BYTE bBootSig; // == 0x29 if next three fields are ok
    DWORD dBS_VolID;
    BYTE sVolLab[11];
    BYTE sBS_FilSysType[8];
} FAT_BOOTSECTOR32;

typedef struct tagEXFAT_BOOTSECTOR
{
    BYTE sJmpBoot[3];                      // jump over BPB
    BYTE sOEMName[8];                      // 'EXFAT   '
    char bpb[53];                          // BPB (zeroes)
    ULONGLONG ullPartitionOffset;          // Partition Offset
    ULONGLONG ullVolumeLength;             // Volume Length
    ULONG     ulFatOffset;                 // FAT offset, in sectors
    ULONG     ulFatLength;                 // FAT length, in sectors
    ULONG     ulClusterHeapOffset;         // Cluster Heap offset, in sectors
    ULONG     ulClusterCount;              // Total number of clusters
    ULONG     RootDirStrtClus;             // Root dir cluster
    ULONG     ulVolSerial;                 // Volume serial number
    USHORT    usFsRev;                     // As Major.minor, major rev. is high byte
    USHORT    usVolumeFlags;               // Volume Flags
    BYTE      bBytesPerSectorShift;        // Bytes per sector shift
    BYTE      bSectorsPerClusterShift;     // Sector per cluster shift
    BYTE      bNumFats;                    // Number of FATs
    BYTE      bDrive;                      // int 13h drive select (usually 0x80)
    BYTE      bPercentInUse;               // Percent of disk space in use
    BYTE      bReserved[7];                // Reserved
    BYTE      bBootCode[390];              // Boot code
    USHORT    usBootSig;                   // 0xaa55
} EXFAT_BOOTSECTOR;

typedef struct {
    DWORD dLeadSig;         // 0x41615252
    BYTE sReserved1[480];   // zeros
    DWORD dStrucSig;        // 0x61417272
    DWORD dFree_Count;      // 0xFFFFFFFF
    DWORD dNxt_Free;        // 0xFFFFFFFF
    BYTE sReserved2[12];    // zeros
    DWORD dTrailSig;     // 0xAA550000
} FAT_FSINFO;

typedef struct 
    {
    BYTE  bFatType;
    DWORD ulFatEof;
    DWORD ulFatEof2;
    DWORD ulFatBad;
    int sectors_per_cluster;        // can be zero for default or 1,2,4,8,16,32 or 64
    char volume_label[12];
    int reserved_sectors;
    }
format_params;

struct extbpb
{
  WORD BytesPerSect;
  UCHAR SectorsPerCluster;
  WORD ReservedSectCount;
  BYTE NumFATs;
  WORD RootDirEnt;
  WORD TotalSectors16;
  BYTE MediaDesc;
  WORD FatSize;
  WORD SectorsPerTrack;
  WORD TracksPerCylinder;
  DWORD HiddenSectors;
  DWORD TotalSectors;
  BYTE Reserved3[6];
};

#pragma pack(pop)

#pragma pack(1)
typedef struct _ShOpenInfo
{
BYTE   szFileName[FAT32MAXPATH];  /* 275 */
SHORT  sOpenCount;
ULONG  ulDirCluster;
ULONG  ulStartCluster;
ULONG  ulLastCluster;
BYTE   bAttr;
BYTE   fMustCommit;
PVOID  pNext;
PVOID  pChild;
BOOL   fLock;
BOOL   fNoFatChain;
} SHOPENINFO, *PSHOPENINFO;
#pragma pack()

//
// API
//

void die ( char * error, DWORD rc );
DWORD get_vol_id (void);
void seek_to_sect( HANDLE hDevice, DWORD Sector, DWORD BytesPerSect );
ULONG write_sect ( HANDLE hDevice, DWORD Sector, DWORD BytesPerSector, void *Data, DWORD NumSects );
void zero_sectors ( HANDLE hDevice, DWORD Sector, DWORD BytesPerSect, DWORD NumSects); //, DISK_GEOMETRY* pdgDrive  )
void open_drive (char *path, HANDLE *hDevice);
void lock_drive(HANDLE hDevice);
void unlock_drive(HANDLE hDevice);
BOOL get_drive_params(HANDLE hDevice, struct extbpb *dp);
void set_part_type(HANDLE hDevice, struct extbpb *dp, int type);
void begin_format (HANDLE hDevice);
void remount_media (HANDLE hDevice);
void sectorio(HANDLE hDevice);
void startlw(HANDLE hDevice);
void stoplw(HANDLE hDevice);
void close_drive(HANDLE hDevice);
int  mem_alloc(void **p, ULONG cb);
void mem_free(void *p, ULONG cb);
void query_freq(ULONGLONG *freq);
void query_time(ULONGLONG *time);
void check_vol_label(char *path, char **vol_label);
char *get_vol_label(char *path, char *vol);
void set_vol_label (char *path, char *vol);
ULONG query_vol_label(char *path, char *pszVolLabel, int cbVolLabel);
void set_datetime(DIRENTRY *pDir);
char *get_error(USHORT rc);
void query_current_disk(char *pszDrive);
BOOL OutputToFile(HANDLE hFile);
void cleanup ( void );
void quit (int rc);
void show_progress (float fPercentWritten);
int show_message (char *pszMsg, unsigned short usLogMsg, unsigned short usMsg, unsigned short usNumFields, ...);
