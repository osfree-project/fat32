#ifndef FAT32IFS_H
#define FAT32IFS_H

#include "fsd.h"
#include "fsh.h"
#include "fat32def.h"

#ifdef __WATCOMC__
#include "devhelp.h"
#else
#include "dhcalls.h"
#endif

#include "strat2.h"

#define CALL_YIELD

#define MYNULL         ((PVOID)0x00030000L)

#define LONGNAME_OFF         0
#define LONGNAME_OK          1
#define LONGNAME_MAKE_UNIQUE 2
#define LONGNAME_ERROR       3

#define MODIFY_DIR_INSERT 0
#define MODIFY_DIR_DELETE 1
#define MODIFY_DIR_UPDATE 2
#define MODIFY_DIR_RENAME 3

#define TRANSLATE_AUTO           0
#define TRANSLATE_SHORT_TO_LONG  1
#define TRANSLATE_LONG_TO_SHORT  2

#define LOG_FS        0x0001
#define LOG_FUNCS     0x0002
#define LOG_CACHE     0x0004
#define LOG_MEM       0x0008
#define LOG_FIND      0x0010
#define LOG_OTHER     0x0020
#define LOG_EAS       0x0040
#define LOG_WAIT      0x0080

#define FSCTL_DAEMON_QUERY          4  /* FSD query for daemon rqrmnt */

#if OLD_SOURCE
typedef struct _FSDTHREAD  /* fsdthread struct for FSCTL fn 4 */
{
USHORT  usFunc;
USHORT  usStackSize;
ULONG   ulPriorityClass;
LONG    lPriorityLevel;
} FSDTHREAD;

/* Data associated with an FSD Daemon thread requirements     */
typedef struct _FSDDAEMON  /* fsddaemon struct for FSCTL fn 4 */
{
USHORT     usNumThreads;
USHORT     usMoreFlag;
USHORT     usCallInstance;
FSDTHREAD  tdThrds[16];
} FSDDAEMON, *PFSDDAEMON;
#endif

typedef Req_List_Header RLH;
typedef RLH *PRLH;

typedef Req_Header RH;
typedef RH *PRH;

typedef PB_Read_Write PB;
typedef PB *PPB;

typedef SG_Descriptor SG;
typedef SG *PSG;

typedef VOID (*STRATFUNC)(VOID);

#pragma pack(1)

#if 0
#define MAXRQENTRIES 48
#define MAX_RQS     20
#endif
//#define MAXRQENTRIES 128
#define MAXRQENTRIES 64
#define MAX_RQS     8

typedef struct _Request
{
PB pb;
SG sg;
USHORT usCBIndex;
//USHORT rgCBIndex[ 8 ];
} REQUEST, *PREQUEST;

typedef struct _RQList
{
USHORT  usNr;
BOOL    fBusy;
SEL     Sel;
ULONG   ulLin;
ULONG   rgPhys[MAXRQENTRIES / 8 + 1];
RLH     rlh;
REQUEST rgReq[MAXRQENTRIES];
} RQLIST, *PRQLIST;

typedef struct _Cache
{
BYTE  bSector[SECTOR_SIZE];
//BYTE  bSector[SECTOR_SIZE * 8];
} CACHE, *PCACHE;

#define SET  1
#define OFF  0

typedef struct _CacheBase
{
USHORT   usNext;
ULONG    ulSector;
unsigned bDrive:8;
unsigned fLocked:1;
unsigned fFlushPending:1;
unsigned fDiscard:1;
/* sequential number of a 512-byte sub-sector
   in current sector (which could be larger,
   e.g., 2048-byte sectors on CD or MO drives) */
unsigned secNum:3;
ULONG    ulAccessTime;
ULONG    ulCreateTime;
} CACHEBASE, *PCACHEBASE;

typedef struct _CacheBase2
{
USHORT usNewer;
USHORT usOlder;
} CACHEBASE2, *PCACHEBASE2;


#pragma pack()

typedef struct _ProcInfo
{
USHORT usPid;
USHORT usUid;
USHORT usPdb;
} PROCINFO, *PPROCINFO; // FAR ?

typedef struct _VolInfo
{
PVOID    pNextVolInfo;
BOOTSECT BootSect;
PBOOTFSINFO pBootFSInfo;
ULONG    ulActiveFatStart;
ULONG    ulStartOfData;
ULONG    ulCurFatSector;
PBYTE    pbFatSector;
ULONG    ulTotalClusters;
ULONG    ulClusterSize;
ULONG    ulBlockSize;
ULONG    SectorsPerCluster;
USHORT   hVBP;
PVOID    pFindInfo;
BYTE     bDrive;
BYTE     bUnit;
USHORT   hDupVBP;
BOOL     fLocked;
BOOL     fWriteProtected;
PROCINFO ProcLocked;
ULONG    ulOpenFiles;
USHORT   usRASectors;
BOOL     fDiskClean;
BOOL     fDiskCleanOnMount;
ULONG    ulLastDiskTime;
STRATFUNC pfnStrategy;
STRATFUNC pfnPriority;
BOOL      fFormatInProgress;
BOOL      fRemovable;
UCHAR     bFatType;
ULONG     ulFatEof;
ULONG     ulFatEof2;
ULONG     ulFatBad;
ULONG     ulFatClean;
ULONG     ulAllocBmpCluster;
ULONG     ulAllocBmpLen;
ULONG     ulUpcaseTblCluster;
ULONG     ulUpcaseTblLen;
PBYTE     pbFatBits;
ULONG     ulCurBmpSector;
ULONG     ulBmpStartSector;
PBYTE     pbFilename;
PBYTE     pbMntPoint;
ULONGLONG ullOffset;
ULONG     hf;
} VOLINFO, *PVOLINFO; // FAR ?

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

typedef struct _OpenInfo /* 30 bytes maximal ! */
{
PSHOPENINFO pSHInfo;
PSHOPENINFO pDirSHInfo;
PVOID       pNext;
PVOLINFO    pVolInfo;
BOOL        fSectorMode;
BOOL        fCommitAttr;
ULONG       ulCurCluster;
ULONG       ulCurBlock;
BOOL        fLargeVolume;
} OPENINFO, *POPENINFO;

typedef struct _FindInfo2
{
PVOID     pNextEntry;
BYTE      szSearch[CCHMAXPATHCOMP];
EAOP      EAOP;
PROCINFO  ProcInfo;
PDIRENTRY pDirEntries;
LONG      lCurEntry;
ULONG     ulMaxEntry;
USHORT    usEntriesPerBlock;
LONG      lBlockIndex;
ULONG     ulTotalBlocks;
BOOL      fLongNames;
BYTE      bAttr;
BYTE      bMustAttr;
ULONG     rgClusters[1];
} FINFO, *PFINFO;

typedef struct _FindInfo /* MAX 24 BYTES ! */
{
PFINFO    pInfo;
PSHOPENINFO pSHInfo;
PVOLINFO  pVolInfo;
} FINDINFO, *PFINDINFO;

typedef struct _EASizeBuf
{
USHORT cbMaxEASize;
ULONG  cbMaxEAListSize;
} EASIZEBUF, *PEASIZEBUF;

#pragma pack(1)
typedef struct _FILEFNDBUF {    /* findbuf */
    FDATE   fdateCreation;
    FTIME   ftimeCreation;
    FDATE   fdateLastAccess;
    FTIME   ftimeLastAccess;
    FDATE   fdateLastWrite;
    FTIME   ftimeLastWrite;
    ULONG   cbFile;
    ULONG   cbFileAlloc;
    USHORT  attrFile;
    UCHAR   cchName;
    //CHAR    achName[CCHMAXPATHCOMP];
    CHAR    achName[1]; /* initial room for zero terminator */
} FILEFNDBUF;
typedef FILEFNDBUF FAR *PFILEFNDBUF;

typedef struct _FILEFNDBUF3 {   /* findbuf3 */
    FDATE   fdateCreation;
    FTIME   ftimeCreation;
    FDATE   fdateLastAccess;
    FTIME   ftimeLastAccess;
    FDATE   fdateLastWrite;
    FTIME   ftimeLastWrite;
    ULONG   cbFile;
    ULONG   cbFileAlloc;
    USHORT  attrFile;
    UCHAR   cchName;
    CHAR    achName[1]; /* initial room for zero terminator */
} FILEFNDBUF3;
typedef FILEFNDBUF3 FAR *PFILEFNDBUF3;

typedef struct _FILEFNDBUF2 {   /* findbuf2 */
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
    UCHAR   cchName;
    //CHAR    achName[CCHMAXPATHCOMP];
    CHAR    achName[1]; /* initial room for zero terminator */
} FILEFNDBUF2;


typedef FILEFNDBUF2 FAR *PFILEFNDBUF2;

typedef struct _FILEFNDBUF3L {
    FDATE    fdateCreation;
    FTIME    ftimeCreation;
    FDATE    fdateLastAccess;
    FTIME    ftimeLastAccess;
    FDATE    fdateLastWrite;
    FTIME    ftimeLastWrite;
    LONGLONG cbFile;
    LONGLONG cbFileAlloc;
    ULONG    attrFile;
    UCHAR    cchName;
    //CHAR     achName[CCHMAXPATHCOMP];
    CHAR     achName[1]; /* initial room for zero terminator */
} FILEFNDBUF3L;

typedef FILEFNDBUF3L FAR *PFILEFNDBUF3L;

typedef struct _FILEFNDBUF4L {
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
    UCHAR    cchName;
    //CHAR     achName[CCHMAXPATHCOMP];
    CHAR     achName[1]; /* initial room for zero terminator */
} FILEFNDBUF4L;

typedef  FILEFNDBUF4L FAR *PFILEFNDBUF4L;
#pragma pack()

IMPORT PVOLINFO   pGlobVolInfo;
IMPORT BYTE       szArguments[];
IMPORT ULONG      ulCacheSectors;
IMPORT USHORT     usDefaultRASectors;
IMPORT PGINFOSEG  pGI;
IMPORT PULONG     pGITicks;
IMPORT F32PARMS   f32Parms;

#define RETURN_PARENT_DIR 0xFFFF

IMPORT VOID PutMessage(PSZ pszMsg);
IMPORT USHORT ReadBlock(PVOLINFO pVolInfo, ULONG ulCluster, ULONG ulBlock, PVOID pbCluster, USHORT usIOMode);
IMPORT USHORT WriteBlock(PVOLINFO pVolInfo, ULONG ulCluster, ULONG ulBlock, PVOID pbCluster, USHORT usIOMode);
IMPORT ULONG GetNextCluster(PVOLINFO pVolInfo, PSHOPENINFO pSHInfo, ULONG ulCluster);
IMPORT ULONG SetNextCluster(PVOLINFO pVolInfo, ULONG ulCluster, ULONG ulNext);
IMPORT USHORT ReadSector(PVOLINFO pVolInfo, ULONG ulSector, USHORT nSectors, PCHAR pbData, USHORT usIOMode);
IMPORT USHORT WriteSector(PVOLINFO pVolInfo, ULONG ulSector, USHORT nSectors, PCHAR pbData, USHORT usIOMode);
IMPORT PVOLINFO GetVolInfo(USHORT hVBP);
IMPORT PVOLINFO GetVolInfoCD(struct cdfsi *pcdfsi, struct cdfsd *pcdfsd);
IMPORT PVOLINFO GetVolInfoSF(struct sffsd *psffsd);
IMPORT PVOLINFO GetVolInfoFS(struct fsfsd *pfsfsd);
IMPORT PVOLINFO GetVolInfoX(char *pszName);
IMPORT VOID   MakeName(PDIRENTRY pDir, PSZ pszName, USHORT usMax);
void FileSetSize(PVOLINFO pVolInfo, PDIRENTRY pDirEntry, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo, PSZ pszFile, ULONGLONG ullSize);
void FileGetSize(PVOLINFO pVolInfo, PDIRENTRY pDirEntry, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo, PSZ pszFile, PULONGLONG pullSize);
IMPORT BOOL   fGetLongName(PDIRENTRY pDir, PSZ pszName, USHORT wMax, PBYTE pbCheck);
IMPORT BOOL   fGetLongName1(PDIRENTRY1 pDir, PSZ pszName, USHORT wMax);
FDATE GetDate1(TIMESTAMP ts);
FTIME GetTime1(TIMESTAMP ts);
TIMESTAMP SetTimeStamp(FDATE date, FTIME time);
IMPORT ULONG FindDirCluster(PVOLINFO pVolInfo,
   struct cdfsi far * pcdfsi,       /* pcdfsi   */
   struct cdfsd far * pcdfsd,       /* pcdfsd   */
   PSZ pDir,
   USHORT usCurDirEnd,
   USHORT usAttrWanted,
   PSZ *pDirEnd,
   PDIRENTRY1 pStreamEntry);
IMPORT ULONG FindPathCluster(PVOLINFO pVolInfo, ULONG ulCluster, PSZ pszPath, PSHOPENINFO pSHInfo,
                             PDIRENTRY pDirEntry, PDIRENTRY1 pDirEntryStream, PSZ pszFullName);
IMPORT BOOL RemoveFindEntry(PVOLINFO pVolInfo, PFINFO pFindInfo);
IMPORT USHORT GetProcInfo(PPROCINFO pProcInfo, USHORT usSize);
IMPORT VOID cdecl _loadds Message(PSZ pszMessage, ...);
IMPORT VOID cdecl _loadds MessageL(USHORT usLevel, PSZ pszMessage, ...);
IMPORT APIRET cdecl Trace(const char *fmt, ...);
IMPORT USHORT cdecl CritMessage(PSZ pszMessage, ...);
IMPORT VOID cdecl FatalMessage(PSZ pszMessage, ...);
IMPORT VOID InternalError(PSZ pszMessage);
IMPORT USHORT GetLogBuffer(PBYTE pData, USHORT cbData, ULONG ulTimeOut);
IMPORT BOOL InitCache(ULONG ulSectors);
//IMPORT ULONG linalloc(ULONG tSize, BOOL fHighMem, BOOL fIgnore);
IMPORT APIRET linalloc(ULONG tSize, BOOL fHighMem, BOOL fIgnore, PULONG pulPhysAddr);
IMPORT PVOID virtalloc(ULONG tSize, BOOL fHighMem, BOOL fIgnore);
IMPORT void *gdtAlloc(ULONG tSize, BOOL fSwap);
IMPORT void *ldtAlloc(ULONG tSize);
IMPORT void freeseg(void *p);

IMPORT USHORT ModifyDirectory(PVOLINFO pVolInfo, ULONG ulDirCluster,
                              PSHOPENINFO pDirSHInfo, USHORT usMode,
                              PDIRENTRY pOld, PDIRENTRY pNew,
                              PDIRENTRY1 pStreamOld, PDIRENTRY1 pStreamNew,
                              PSZ pszLongNameOld, PSZ pszLongNameNew, USHORT usIOMode);
IMPORT USHORT MakeDirEntry(PVOLINFO pVolInfo, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo, PDIRENTRY pNew, PDIRENTRY1 pNewStream, PSZ pszName);
IMPORT USHORT MakeShortName(PVOLINFO pVolInfo, ULONG ulDirCluster, ULONG ulDirEntry, PSZ pszLongName, PSZ pszShortName);
IMPORT USHORT MakeLongName1(PVOLINFO pVolInfo, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo, PSZ pszLongName, PSZ pszLongName2);
IMPORT BOOL   DeleteFatChain(PVOLINFO pVolInfo, ULONG ulCluster);
IMPORT BYTE   GetVFATCheckSum(PDIRENTRY pDir);
IMPORT USHORT usHeapInit(VOID);
IMPORT ULONG MakeFatChain(PVOLINFO pVolInfo, PSHOPENINFO pSHInfo, ULONG ulPrevCluster, ULONG ulClustersRequested, PULONG pulLast);
IMPORT USHORT SemRequest(void far * hSem, ULONG ulTimeOut, PSZ pszTest);
IMPORT ULONG GetFreeSpace(PVOLINFO pVolInfo);
IMPORT BOOL UpdateFSInfo(PVOLINFO pVolInfo);
IMPORT BOOL IsDriveLocked(PVOLINFO pVolInfo);
IMPORT ULONG GetLastCluster(PVOLINFO pVolInfo, ULONG ulCluster, PDIRENTRY1 pDirEntryStream);
IMPORT ULONG SeekToCluster(PVOLINFO pVolInfo, PSHOPENINFO pSHInfo, ULONG ulCluster, LONGLONG llPosition);
IMPORT USHORT usFlushAll(VOID);
IMPORT USHORT usFlushVolume(PVOLINFO, USHORT, BOOL, BYTE);
IMPORT BOOL MarkDiskStatus(PVOLINFO pVolInfo, BOOL fClean);
IMPORT BOOL GetDiskStatus(PVOLINFO pVolInfo);
IMPORT USHORT fGetSetVolLabel(PVOLINFO pVolInfo, USHORT usFlag, PSZ pszVolLabel, PUSHORT pusSize);
IMPORT USHORT fGetAllocBitmap(PVOLINFO pVolInfo, PULONG pulFirstCluster, PULONGLONG pullLen);
IMPORT USHORT fGetUpCaseTbl(PVOLINFO pVolInfo, PULONG pulFirstCluster, PULONGLONG pullLen, PULONG pulChecksum);
IMPORT void SetSHInfo1(PVOLINFO pVolInfo, PDIRENTRY1 pStreamEntry, PSHOPENINFO pSHInfo);
IMPORT USHORT TranslateName(PVOLINFO pVolInfo, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo, PSZ pszPath, PSZ pszTarget, USHORT usMode);
IMPORT VOID DoLW(PVOLINFO pVolInfo, PLWOPTS pOptions);
IMPORT VOID DoEmergencyFlush(PLWOPTS pOptions);
IMPORT VOID Yield(void);
IMPORT VOID TriggerLW(VOID);
IMPORT BOOL IsDosSession(VOID);
IMPORT USHORT usModifyEAS(PVOLINFO pVolInfo, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo, PSZ pszFileName, PEAOP pEAOP);
IMPORT USHORT usGetEASize(PVOLINFO pVolInfo, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo, PSZ pszFileName, PULONG pulSize);
IMPORT USHORT usGetEAS(PVOLINFO pVolInfo, USHORT usLevel, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo, PSZ pszFileName, PEAOP pEAOP);
IMPORT USHORT usDeleteEAS(PVOLINFO pVolInfo, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo, PSZ pszFileName);
IMPORT BOOL   IsEASFile(PSZ pszFileName);
IMPORT USHORT usCopyEAS(PVOLINFO pVolInfo,
                        ULONG ulSrcDirCluster, PSZ pszSrcFile, PSHOPENINFO pDirSrcSHInfo,
                        ULONG ulTarDirCluster, PSZ pszTarFile, PSHOPENINFO pDirTarSHInfo);
IMPORT USHORT usMoveEAS(PVOLINFO pVolInfo,
                        ULONG ulSrcDirCluster, PSZ pszSrcFile, PSHOPENINFO pDirSrcSHInfo,
                        ULONG ulTarDirCluster, PSZ pszTarFile, PSHOPENINFO pDirTarSHInfo);
IMPORT USHORT CopyChain(PVOLINFO pVolInfo, PSHOPENINFO pSHInfo, ULONG ulCluster, PULONG pulNew);
IMPORT USHORT MarkFileEAS(PVOLINFO pVolInfo, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo, PSZ pszFileName, BYTE fEAS);
IMPORT VOID   TranslateInitDBCSEnv( VOID );
IMPORT BOOL   IsDBCSLead( UCHAR uch );
IMPORT VOID   TranslateAllocBuffer( VOID );
IMPORT VOID   TranslateFreeBuffer( VOID );
IMPORT USHORT Translate2Win(PSZ pszName, PUSHORT puniName, USHORT usLen);
IMPORT VOID   Translate2OS2(PUSHORT puniName, PSZ pszName, USHORT usLen);
IMPORT BOOL   TranslateInit(BYTE rgTrans[], USHORT usSize);
IMPORT USHORT MY_PROBEBUF(USHORT usOperation, char far * pData, USHORT cbData);
IMPORT POPENINFO GetOpenInfo(struct sffsd far * psffsd);
IMPORT PSHOPENINFO GetSH( PSZ pszFileName, POPENINFO pOpenInfo);
IMPORT BOOL ReleaseSH(POPENINFO pOI);
IMPORT USHORT usGetEmptyEAS(PSZ pszFileName,PEAOP pEAOP);
IMPORT USHORT MY_ISCURDIRPREFIX( PSZ pszName );
IMPORT VOID   GetFirstInfo( PBOOL pLead );
IMPORT VOID   GetCaseConversion( PUCHAR pCase );

/*
#define GetCurTime() (pGI->time * 100 + pGI->hundredths)
*/
#define GetCurTime() (*pGITicks)


#define STORAGE_NEEDED (sizeof (VOLINFO) + sizeof (BOOTFSINFO) + SECTOR_SIZE * 8 * 3 + SECTOR_SIZE * 8 + 10000)


#endif
