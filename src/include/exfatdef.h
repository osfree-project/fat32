#ifndef __EXFATDEF_H__
#define __EXFATDEF_H__

// volume flags
#define VOL_FLAG_ACTIVEFAT     0x0001   // 0 means 1st FAT and alloc. bitmap are active
#define VOL_FLAG_VOLDIRTY      0x0002   // 1 means volume is dirty, 0 means clean
#define VOL_FLAG_MEDIAFAILURE  0x0004   // 1 means that some I/O operation is failed
#define VOL_FLAG_ZERO          0x0008   // zero
// other bits are reserved

#pragma pack(1)

typedef struct _BootSector1
{
BYTE bJmp[3];                          // jump over BPB
BYTE oemID[8];                         // 'EXFAT   '
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
} BOOTSECT1, *PBOOTSECT1;

#define   ENTRY_TYPE_CODE_MASK     0x1f
#define   ENTRY_TYPE_IMPORTANCE    0x20
#define   ENTRY_TYPE_CATEGORY      0x40
#define   ENTRY_TYPE_IN_USE_STATUS 0x80

// dir entry types
#define   ENTRY_TYPE_EOD           0x00
#define   ENTRY_TYPE_ALLOC_BITMAP  0x81
#define   ENTRY_TYPE_UPCASE_TABLE  0x82
#define   ENTRY_TYPE_VOLUME_LABEL  0x83
#define   ENTRY_TYPE_FILE          0x85
#define   ENTRY_TYPE_VOLUME_GUID   0xa0
#define   ENTRY_TYPE_TEXFAT_PAD    0xa1
#define   ENTRY_TYPE_WINCE_ACTBL   0xa2
#define   ENTRY_TYPE_STREAM_EXT    0xc0
#define   ENTRY_TYPE_FILE_NAME     0xc1

typedef struct _TimeStamp
{
   ULONG twosecs:5;
   ULONG minutes:6;
   ULONG hour:5;
   ULONG day:5;
   ULONG month:4;
   ULONG year:7;
} TIMESTAMP, *PTIMESTAMP;

typedef struct _DirEntry1
{
BYTE  bEntryType;
union
   {
   struct _AllocBmp
      {
      BYTE      bRsvd[19];
      ULONG     ulFirstCluster;
      ULONGLONG ullDataLength;
      } AllocBmp;
   struct _UpCaseTbl
      {
      BYTE      bRsvd1[3];
      ULONG     ulTblCheckSum;
      BYTE      bRsvd2[12];
      ULONG     ulFirstCluster;
      ULONGLONG ullDataLength;
      } UpCaseTbl;
   struct _VolLbl
      {
      BYTE    bCharCount;
      USHORT  usChars[11];
      BYTE    bRsvd[8];
      } VolLbl;
   struct _TexFatPad
      {
      BYTE      bRsvd[31];
      } TexFatPad;
   struct _WinCEAct
      {
      BYTE      bRsvd[31];
      } WinCEAct;
   struct _VolGUID // All values are valid except null GUID
      {            // {00000000-0000-0000-0000-000000000000}
      BYTE      bSecondaryCount;     // must be 00
      USHORT    usSetCheckSum;
      USHORT    usResvd1;            // must be 00
      BYTE      bVolumeGUID[16];
      BYTE      bResvd2[10];
      } VolGUID;
   struct _File
      {
      BYTE      bSecondaryCount;
      USHORT    usSetCheckSum;
      USHORT    usFileAttr;
      BYTE      fEAS;
      BYTE      bRsvd1;
      TIMESTAMP ulCreateTimestp;
      TIMESTAMP ulLastModifiedTimestp;
      TIMESTAMP ulLastAccessedTimestp;
      BYTE      bCreate10msIncrement;
      BYTE      bLastModified10msIncrement;
      BYTE      bCreateTimezoneOffset;
      BYTE      bLastModifiedTimezoneOffset;
      BYTE      bLastAccessedTimezoneOffset;
      BYTE      bResvd2[7];
      } File;
   struct _Stream
      {
      BYTE      bAllocPossible:1;
      BYTE      bNoFatChain:1;
      BYTE      bCustomDefined1:6;
      BYTE      bResvd1;
      BYTE      bNameLen;
      USHORT    usNameHash;
      USHORT    usResvd2;
      ULONGLONG ullValidDataLen;
      ULONG     ulResvd3;
      ULONG     ulFirstClus;
      ULONGLONG ullDataLen;
      } Stream;
   struct _FileName
      {
      BYTE      bAllocPossible:1;
      BYTE      bNoFatChain:1;
      BYTE      bCustomDefined1:6;
      USHORT    bFileName[15];
      } FileName;
   } u;
} DIRENTRY1, * PDIRENTRY1;

#pragma pack()

#endif
