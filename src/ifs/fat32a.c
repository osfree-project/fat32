#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <dos.h>

#define INCL_DOS
#define INCL_DOSDEVIOCTL
#define INCL_DOSDEVICES
#define INCL_DOSERRORS

#include "os2.h"
#include "portable.h"
#include "fat32ifs.h"

PUBLIC BYTE  pascal FS_NAME[8]    ="FAT32";
PUBLIC ULONG pascal FS_ATTRIBUTE = FSA_LVL7;

PUBLIC ULONG     Device_Help = 0L;
PUBLIC PVOLINFO  pGlobVolInfo = NULL;
PUBLIC ULONG     ulCacheSectors=2048;
PUBLIC USHORT    usDefaultRASectors = 0xFFFF;
PUBLIC BYTE      szArguments[512] = "";
PUBLIC PGINFOSEG pGI = NULL;
PUBLIC PULONG    pGITicks = NULL;
PUBLIC F32PARMS  f32Parms = {0};

PUBLIC VOID _cdecl InitMessage(PSZ pszMessage,...);

static BYTE szDiskLocked[]="The disk is in use or locked by another process.\r\n";
static BYTE rgValidChars[]="01234567890 ABCDEFGHIJKLMNOPQRSTUVWXYZ!#$%&'()-_@^`{}~";
static ULONG ulSemRWFat = 0UL;
static SEL sGlob = 0;
static SEL sLoc = 0;

static BYTE szBanner[]=
"FAT32.IFS version " FAT32_VERSION " " __DATE__ "\r\n"
"Made by Henk Kelder + Netlabs";

PCPDATA pCPData = NULL;
BYTE lock[12] = {0};
PID pidDaemon = 0;

USHORT NameHash(USHORT *pszFilename, int NameLen);
static BOOL ClusterInUse(PVOLINFO pVolInfo, ULONG ulCluster);
static ULONG GetFreeCluster(PVOLINFO pVolInfo);
PDIRENTRY fSetLongName(PDIRENTRY pDir, PSZ pszName, BYTE bCheck);
PDIRENTRY1 fSetLongName1(PDIRENTRY1 pDir, PSZ pszLongName, PUSHORT pusNameHash);
ULONG GetNextCluster2(PVOLINFO pVolInfo, PSHOPENINFO pSHInfo, ULONG ulCluster);
USHORT ReadSector2(PVOLINFO pVolInfo, ULONG ulSector, USHORT nSectors, PCHAR pbData, USHORT usIOMode);
static ULONG SetNextCluster2(PVOLINFO pVolInfo, ULONG ulCluster, ULONG ulNext);
PDIRENTRY CompactDir(PDIRENTRY pStart, ULONG ulSize, USHORT usNeededEntries);
PDIRENTRY1 CompactDir1(PDIRENTRY1 pStart, ULONG ulSize, USHORT usEntriesNeeded);
USHORT GetFreeEntries(PDIRENTRY pDirBlock, ULONG ulSize);
VOID MarkFreeEntries(PDIRENTRY pDirBlock, ULONG ulSize);
VOID MarkFreeEntries1(PDIRENTRY1 pDirBlock, ULONG ulSize);
USHORT GetFatAccess(PVOLINFO pVolInfo, PSZ pszName);
VOID   ReleaseFat(PVOLINFO pVolInfo);
int Mount2(PMNTOPTS opts, PVOLINFO *pVolInfo);
static USHORT RecoverChain(PVOLINFO pVolInfo, ULONG ulCluster, PBYTE pData, USHORT cbData);
static USHORT WriteFatSector(PVOLINFO pVolInfo, ULONG ulSector);
static USHORT ReadFatSector(PVOLINFO pVolInfo, ULONG ulSector);
static ULONG  GetVolDevice(PVOLINFO pVolInfo);
static USHORT SetFileSize(PVOLINFO pVolInfo, PFILESIZEDATA pFileSize);
static ULONG GetChainSize(PVOLINFO pVolInfo, PSHOPENINFO pSHInfo, ULONG ulCluster);
static USHORT MakeChain(PVOLINFO pVolInfo, PSHOPENINFO pSHInfo, ULONG ulFirstCluster, ULONG ulSize);
static USHORT GetSetFileEAS(PVOLINFO pVolInfo, USHORT usFunc, PMARKFILEEASBUF pMark);
USHORT DBCSStrlen( const PSZ pszStr );
int fGetSetFSInfo(unsigned short usFlag, PVOLINFO pVolInfo, char far * pData,
    unsigned short cbData, unsigned short usLevel);

static ULONG GetFatEntrySec(PVOLINFO pVolInfo, ULONG ulCluster);
static ULONG GetFatEntryBlock(PVOLINFO pVolInfo, ULONG ulCluster, ULONG ulBlockSize);
static ULONG GetFatEntry(PVOLINFO pVolInfo, ULONG ulCluster);
static ULONG GetFatEntryEx(PVOLINFO pVolInfo, PBYTE pFatStart, ULONG ulCluster, ULONG ulBlockSize);
static void SetFatEntry(PVOLINFO pVolInfo, ULONG ulCluster, ULONG ulValue);
static void SetFatEntryEx(PVOLINFO pVolInfo, PBYTE pFatStart, ULONG ulCluster, ULONG ulValue, ULONG ulBlockSize);

int PreSetup(void);
int PostSetup(void);

LONG semSerialize = 0;
LONG semRqAvail = 0;
LONG semRqDone = 0;

extern ULONG autocheck_mask;
extern ULONG force_mask;
extern ULONG fat_mask;
extern ULONG fat32_mask;
#ifdef EXFAT
extern ULONG exfat_mask;
#endif

void _cdecl autocheck(char *args);

APIRET SemClear(long far *sem)
{
    USHORT usValue;
    APIRET rc = 0;

    (*sem)--;

#ifdef DEBUG
    log("%lx\n", *sem);
#endif

    rc = DevHelp_ProcRun((ULONG)sem, &usValue);

    return rc;
}

APIRET SemSet(long far *sem)
{
    APIRET rc = 0;

    (*sem)++;

#ifdef DEBUG
    log("%lx\n", *sem);
#endif

    return rc;
}

APIRET SemWait(long far *sem)
{
    APIRET rc = 0;

#ifdef DEBUG
    log("%lx\n", *sem);
#endif

    if (*sem >= 0)
    {
        rc = DevHelp_ProcBlock((ULONG)sem, -1, 0);
    }

    return rc;
}

/******************************************************************
*
******************************************************************/
int far pascal _loadds FS_ATTACH(unsigned short usFlag,     /* flag     */
                         char far * pDev,           /* pDev     */
                         void far * pvpfsd,         /* if remote drive
                                                 struct vpfsd far *
                                                   else if remote device
                                                    null ptr (0L)    */
                         void far * pdevfsd,            /* if remote drive
                                                   struct cdfsd far *
                                                   else
                                                    struct devfsd far * */
                         char far * pParm,          /* pParm    */
                         unsigned short far * pLen) /* pLen     */
{
   MessageL(LOG_FS, "FS_ATTACH%m - NOT SUPPORTED", 0x0023);
   usFlag = usFlag;
   pDev = pDev;
   pvpfsd = pvpfsd;
   pdevfsd = pdevfsd;
   pParm = pParm;
   pLen = pLen;
   return ERROR_NOT_SUPPORTED;
}

/******************************************************************
*
******************************************************************/
int far pascal _loadds FS_COPY(
    unsigned short usMode,      /* copy mode    */
    struct cdfsi far * pcdfsi,      /* pcdfsi   */
    struct cdfsd far * pcdfsd,      /* pcdfsd   */
    char far * pSrc,            /* source name  */
    unsigned short usSrcCurDirEnd,      /* iSrcCurrDirEnd   */
    char far * pDst,            /* pDst     */
    unsigned short usDstCurDirEnd,      /* iDstCurrDirEnd   */
    unsigned short usNameType       /* nameType (flags) */
)
{
PVOLINFO pVolInfo;
ULONG ulSrcDirCluster;
ULONG ulDstDirCluster;
PSZ   pszSrcFile;
PSZ   pszDstFile;
PSZ   pszSrc, pszDst;
PDIRENTRY pDirEntry = NULL;
PDIRENTRY pTarEntry = NULL;
PDIRENTRY1 pDirStreamEntry = NULL;
PDIRENTRY1 pTarStreamEntry = NULL;
PDIRENTRY1 pDirSrcStream = NULL;
PDIRENTRY1 pDirDstStream = NULL;
#ifdef EXFAT
PDIRENTRY1 pDirEntry1;
PDIRENTRY1 pTarEntry1;
#endif
PSHOPENINFO pSrcSHInfo = NULL;
PSHOPENINFO pDirSrcSHInfo = NULL, pDirDstSHInfo = NULL;
ULONG    ulSrcCluster;
ULONG    ulDstCluster;
USHORT   rc, rc2;
POPENINFO pOpenInfo = NULL;
//BYTE     szSrcLongName[ FAT32MAXPATH ];
PSZ      szSrcLongName = NULL;
//BYTE     szDstLongName[ FAT32MAXPATH ];
PSZ      szDstLongName = NULL;

   _asm push es;

   MessageL(LOG_FS, "FS_COPY%m %s to %s, mode %d", 0x0025, pSrc, pDst, usMode);

   pVolInfo = GetVolInfoX(pSrc);

   if (pVolInfo != GetVolInfoX(pDst))
      {
      rc = ERROR_CANNOT_COPY;
      goto FS_COPYEXIT;
      }

   if (! pVolInfo)
      {
      pVolInfo = GetVolInfo(pcdfsi->cdi_hVPB);
      }

   if (! pVolInfo)
      {
      rc = ERROR_INVALID_DRIVE;
      goto FS_COPYEXIT;
      }

   if (pVolInfo->fFormatInProgress)
      {
      rc = ERROR_ACCESS_DENIED;
      goto FS_COPYEXIT;
      }

   pVolInfo->ulOpenFiles++;

   pOpenInfo = malloc(sizeof (OPENINFO));
   if (!pOpenInfo)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto FS_COPYEXIT;
      }
   memset(pOpenInfo, 0, sizeof (OPENINFO));

   pDirEntry = (PDIRENTRY)malloc((size_t)sizeof(DIRENTRY));
   if (!pDirEntry)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto FS_COPYEXIT;
      }
   pTarEntry = (PDIRENTRY)malloc((size_t)sizeof(DIRENTRY));
   if (!pTarEntry)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto FS_COPYEXIT;
      }
#ifdef EXFAT
   pDirEntry1 = (PDIRENTRY1)pDirEntry;
   pTarEntry1 = (PDIRENTRY1)pTarEntry;

   pDirStreamEntry = (PDIRENTRY1)malloc((size_t)sizeof(DIRENTRY1));
   if (!pDirStreamEntry)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto FS_COPYEXIT;
      }
   pTarStreamEntry = (PDIRENTRY1)malloc((size_t)sizeof(DIRENTRY1));
   if (!pTarStreamEntry)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto FS_COPYEXIT;
      }
   pDirSrcStream = (PDIRENTRY1)malloc((size_t)sizeof(DIRENTRY1));
   if (!pDirSrcStream)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto FS_COPYEXIT;
      }
   pDirDstStream = (PDIRENTRY1)malloc((size_t)sizeof(DIRENTRY1));
   if (!pDirDstStream)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto FS_COPYEXIT;
      }
   pDirSrcSHInfo = (PSHOPENINFO)malloc((size_t)sizeof(SHOPENINFO));
   if (!pDirSrcSHInfo)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto FS_COPYEXIT;
      }
   pDirDstSHInfo = (PSHOPENINFO)malloc((size_t)sizeof(SHOPENINFO));
   if (!pDirDstSHInfo)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto FS_COPYEXIT;
      }
   pSrcSHInfo = (PSHOPENINFO)malloc((size_t)sizeof(SHOPENINFO));
   if (!pSrcSHInfo)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto FS_COPYEXIT;
      }
#endif

   /*
      Not on the same drive: cannot copy
   */
   if (*pSrc != *pDst)
      {
      rc = ERROR_CANNOT_COPY;
      goto FS_COPYEXIT;
      }

   szSrcLongName = (PSZ)malloc((size_t)FAT32MAXPATH);
   if (!szSrcLongName)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto FS_COPYEXIT;
      }

   szDstLongName = (PSZ)malloc((size_t)FAT32MAXPATH);
   if (!szDstLongName)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto FS_COPYEXIT;
      }

   if( TranslateName(pVolInfo, 0L, NULL, pSrc, szSrcLongName, TRANSLATE_SHORT_TO_LONG ))
      strcpy( szSrcLongName, pSrc );

   if( TranslateName(pVolInfo, 0L, NULL, pDst, szDstLongName, TRANSLATE_SHORT_TO_LONG ))
      strcpy( szDstLongName, pDst );

#ifdef EXFAT
   if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
      {
      pszSrc = szSrcLongName;
      pszDst = szDstLongName;
      }
   else
#endif
      {
      pszSrc = pSrc;
      pszDst = pDst;
      }

   if (usSrcCurDirEnd == (USHORT)(strrchr(pSrc, '\\') - pSrc + 1))
      {
      usSrcCurDirEnd = strrchr(pszSrc, '\\') - pszSrc + 1;
      }

   if (usDstCurDirEnd == (USHORT)(strrchr(pDst, '\\') - pDst + 1))
      {
      usDstCurDirEnd = strrchr(pszDst, '\\') - pszDst + 1;
      }

   if (!stricmp( szSrcLongName, szDstLongName ))
      {
      rc = ERROR_CANNOT_COPY;
      goto FS_COPYEXIT;
      }

   pOpenInfo->pSHInfo = GetSH( szDstLongName, pOpenInfo);
   if (!pOpenInfo->pSHInfo)
      {
      rc = ERROR_TOO_MANY_OPEN_FILES;
      goto FS_COPYEXIT;
      }
   //pOpenInfo->pSHInfo->sOpenCount++; //
   if (pOpenInfo->pSHInfo->sOpenCount > 1)
      {
      rc = ERROR_ACCESS_DENIED;
      goto FS_COPYEXIT;
      }
   pOpenInfo->pSHInfo->fLock = TRUE;

   usNameType = usNameType;

   if (IsDriveLocked(pVolInfo))
      {
      rc =ERROR_DRIVE_LOCKED;
      goto FS_COPYEXIT;
      }
   if (!pVolInfo->fDiskCleanOnMount)
      {
      rc = ERROR_VOLUME_DIRTY;
      goto FS_COPYEXIT;
      }
   if (pVolInfo->fWriteProtected)
      {
      rc = ERROR_WRITE_PROTECT;
      goto FS_COPYEXIT;
      }

   /*
      Check source
   */

   ulSrcDirCluster = FindDirCluster(pVolInfo,
      pcdfsi,
      pcdfsd,
      pszSrc,
      usSrcCurDirEnd,
      RETURN_PARENT_DIR,
      &pszSrcFile,
      pDirSrcStream);

   if (ulSrcDirCluster == pVolInfo->ulFatEof)
      {
      rc = ERROR_PATH_NOT_FOUND;
      goto FS_COPYEXIT;
      }

#ifdef EXFAT
   if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
      {
      SetSHInfo1(pVolInfo, pDirSrcStream, pDirSrcSHInfo);
      }
#endif

   ulSrcCluster = FindPathCluster(pVolInfo, ulSrcDirCluster, pszSrcFile,
      pDirSrcSHInfo, pDirEntry, pDirStreamEntry, NULL);
   if (ulSrcCluster == pVolInfo->ulFatEof)
      {
      rc = ERROR_FILE_NOT_FOUND;
      goto FS_COPYEXIT;
      }
   /*
      Do not allow directories to be copied
   */
#ifdef EXFAT
   if ( ((pVolInfo->bFatType <  FAT_TYPE_EXFAT) && (pDirEntry->bAttr & FILE_DIRECTORY)) ||
        ((pVolInfo->bFatType == FAT_TYPE_EXFAT) && (pDirEntry1->u.File.usFileAttr & FILE_DIRECTORY)) )
#else
   if ( pDirEntry->bAttr & FILE_DIRECTORY )
#endif
      {
      int iLen = strlen( szSrcLongName );

      if ( !strnicmp(szSrcLongName, szDstLongName, iLen ) &&
           ( szDstLongName[ iLen ] == '\\' ))
      {
        rc = ERROR_DIRECTORY;
        goto FS_COPYEXIT;
      }

      rc = ERROR_CANNOT_COPY;
      goto FS_COPYEXIT;
      }


   if (ulSrcCluster == pVolInfo->BootSect.bpb.RootDirStrtClus)
      {
      rc = ERROR_ACCESS_DENIED;
      goto FS_COPYEXIT;
      }

   /*
      Check destination
   */

   ulDstDirCluster = FindDirCluster(pVolInfo,
      pcdfsi,
      pcdfsd,
      pszDst,
      usDstCurDirEnd,
      RETURN_PARENT_DIR,
      &pszDstFile,
      pDirDstStream);

   if (ulDstDirCluster == pVolInfo->ulFatEof)
      {
      rc = ERROR_PATH_NOT_FOUND;
      goto FS_COPYEXIT;
      }

#ifdef EXFAT
   if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
      {
      SetSHInfo1(pVolInfo, pDirDstStream, pDirDstSHInfo);
      }
#endif

   pszDstFile = strrchr( szDstLongName, '\\' ) + 1; /* To preserve long name */

   ulDstCluster = FindPathCluster(pVolInfo, ulDstDirCluster, pszDstFile,
      pDirDstSHInfo, pTarEntry, pTarStreamEntry, NULL);
   if (ulDstCluster != pVolInfo->ulFatEof)
      {
#ifdef EXFAT
      if ( ((pVolInfo->bFatType <  FAT_TYPE_EXFAT) && (pTarEntry->bAttr & FILE_DIRECTORY)) ||
           ((pVolInfo->bFatType == FAT_TYPE_EXFAT) && (pTarEntry1->u.File.usFileAttr & FILE_DIRECTORY)) )
#else
      if ( pTarEntry->bAttr & FILE_DIRECTORY )
#endif
         {
         rc = ERROR_CANNOT_COPY;
         goto FS_COPYEXIT;
         }
      if (usMode == DCPY_APPEND)
         {
         rc = ERROR_CANNOT_COPY;
         goto FS_COPYEXIT;
         }

      /*
         Delete target
      */
      if (f32Parms.fEAS)
         {
         rc = usDeleteEAS(pVolInfo, ulDstDirCluster, pDirDstSHInfo, pszDstFile);
         if (rc)
            goto FS_COPYEXIT;
#if 0
         if (pTarEntry->fEAS == FILE_HAS_EAS || pTarEntry->fEAS == FILE_HAS_CRITICAL_EAS)
            pTarEntry->fEAS = FILE_HAS_NO_EAS;
#endif
         }

      rc = ModifyDirectory(pVolInfo, ulDstDirCluster, pDirDstSHInfo, MODIFY_DIR_DELETE,
                           pTarEntry, NULL, pTarStreamEntry, NULL, pszDstFile, pszDstFile, 0);
      //rc = ModifyDirectory(pVolInfo, ulDstDirCluster, pDirDstSHInfo, MODIFY_DIR_DELETE,
      //                     pTarEntry, NULL, pTarStreamEntry, NULL, NULL, 0);
      if (rc)
         goto FS_COPYEXIT;

      if (!DeleteFatChain(pVolInfo, ulDstCluster))
         {
         rc = ERROR_FILE_NOT_FOUND;
         goto FS_COPYEXIT;
         }
      }

   /*
      Make new direntry
   */

   memcpy(pTarEntry, pDirEntry, sizeof(DIRENTRY));
#ifdef EXFAT
   if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
      {
#endif
      pTarEntry->wCluster = 0;
      pTarEntry->wClusterHigh = 0;
      pTarEntry->ulFileSize = 0L;
#ifdef EXFAT
      }
   else
      {
      pTarStreamEntry->u.Stream.ulFirstClus = 0;
#ifdef INCL_LONGLONG
      pTarStreamEntry->u.Stream.ullValidDataLen = 0;
      pTarStreamEntry->u.Stream.ullDataLen = 0;
#else
      AssignUL(&pTarStreamEntry->u.Stream.ullValidDataLen, 0);
      AssignUL(&pTarStreamEntry->u.Stream.ullDataLen, 0);
#endif
      }
#endif
   rc = ModifyDirectory(pVolInfo, ulDstDirCluster, pDirDstSHInfo, MODIFY_DIR_INSERT, NULL, pTarEntry, NULL, pTarStreamEntry, pszDstFile, pszDstFile, 0);
   if (rc)
      goto FS_COPYEXIT;
#ifdef EXFAT
   if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
#endif
      memcpy(pDirEntry->bFileName, pTarEntry->bFileName, 11);

#ifdef EXFAT
   if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
      {
      SetSHInfo1(pVolInfo, pDirStreamEntry, pSrcSHInfo);
      }
#endif

   /*
      Do the copying
   */
   rc = CopyChain(pVolInfo, pSrcSHInfo, ulSrcCluster, &ulDstCluster);

#ifdef EXFAT
   if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
      {
#endif
      if (ulDstCluster != pVolInfo->ulFatEof)
         {
         pDirEntry->wCluster = LOUSHORT(ulDstCluster);
         pDirEntry->wClusterHigh = HIUSHORT(ulDstCluster);
         }
      else
         {
         pDirEntry->wCluster = 0;
         pDirEntry->wClusterHigh = 0;
         pDirEntry->ulFileSize = 0L;
         }
#ifdef EXFAT
      }
   else
      {
      if (ulDstCluster != pVolInfo->ulFatEof)
         {
         pDirStreamEntry->u.Stream.ulFirstClus = ulDstCluster;
         }
      else
         {
         pDirStreamEntry->u.Stream.ulFirstClus = 0;
#ifdef INCL_LONGLONG
         pDirStreamEntry->u.Stream.ullValidDataLen = 0;
         pDirStreamEntry->u.Stream.ullDataLen = 0;
#else
         AssignUL(&pDirStreamEntry->u.Stream.ullValidDataLen, 0);
         AssignUL(&pDirStreamEntry->u.Stream.ullDataLen, 0);
#endif
         }
      }
#endif
   /*
      modify new entry
   */
   rc2 = ModifyDirectory(pVolInfo, ulDstDirCluster, pDirDstSHInfo, MODIFY_DIR_UPDATE, pTarEntry, pDirEntry, pTarStreamEntry, pDirStreamEntry, pszDstFile, pszDstFile, 0);
   //rc2 = ModifyDirectory(pVolInfo, ulDstDirCluster, pDirDstSHInfo, MODIFY_DIR_UPDATE, pTarEntry, pDirEntry, pTarStreamEntry, pDirStreamEntry, NULL, 0);
   if (rc2 && !rc)
      rc = rc2;

   if (rc)
      goto FS_COPYEXIT;

   if (f32Parms.fEAS)
      {
      rc = usCopyEAS(pVolInfo, ulSrcDirCluster, pszSrcFile, pDirSrcSHInfo,
                               ulDstDirCluster, pszDstFile, pDirDstSHInfo);
      }

FS_COPYEXIT:

   pVolInfo->ulOpenFiles--;

   if (pOpenInfo)
      {
      if (pOpenInfo->pSHInfo)
         ReleaseSH(pOpenInfo);
      else
         free(pOpenInfo);
      }

   if (szSrcLongName)
      free(szSrcLongName);
   if (szDstLongName)
      free(szDstLongName);
   if (pDirEntry)
      free(pDirEntry);
   if (pTarEntry)
      free(pTarEntry);
#ifdef EXFAT
   if (pDirStreamEntry)
      free(pDirStreamEntry);
   if (pTarStreamEntry)
      free(pTarStreamEntry);
   if (pDirSrcStream)
      free(pDirSrcStream);
   if (pDirDstStream)
      free(pDirDstStream);
   if (pSrcSHInfo)
      free(pSrcSHInfo);
   if (pDirSrcSHInfo)
      free(pDirSrcSHInfo);
   if (pDirDstSHInfo)
      free(pDirDstSHInfo);
#endif

   MessageL(LOG_FS, "FS_COPY%m returned %u", 0x8025, rc);

   _asm pop es;

   return rc;
}

/******************************************************************
*
******************************************************************/
int far pascal _loadds FS_DELETE(
    struct cdfsi far * pcdfsi,      /* pcdfsi   */
    struct cdfsd far * pcdfsd,      /* pcdfsd   */
    char far * pFile,           /* pFile    */
    unsigned short usCurDirEnd      /* iCurDirEnd   */
)
{
PVOLINFO pVolInfo;
ULONG    ulCluster;
ULONG    ulDirCluster;
PSZ      pszFile, pszFile2;
USHORT   rc;
PDIRENTRY pDirEntry = NULL;
#ifdef EXFAT
PDIRENTRY1 pDirEntry1;
#endif
PDIRENTRY1 pDirEntryStream = NULL;
POPENINFO pOpenInfo = NULL;
PDIRENTRY1 pDirStream = NULL;
PSHOPENINFO pDirSHInfo = NULL;
//BYTE     szLongName[ FAT32MAXPATH ];
PSZ      szLongName = NULL;

   _asm push es;

   MessageL(LOG_FS, "FS_DELETE%m for %s", 0x0026, pFile);

   pVolInfo = GetVolInfoX(pFile);

   if (! pVolInfo)
      {
      pVolInfo = GetVolInfo(pcdfsi->cdi_hVPB);
      }

   if (! pVolInfo)
      {
      rc = ERROR_INVALID_DRIVE;
      goto FS_DELETEEXIT;
      }

   if (pVolInfo->fFormatInProgress)
      {
      rc = ERROR_ACCESS_DENIED;
      goto FS_DELETEEXIT;
      }

   if (IsDriveLocked(pVolInfo))
      {
      rc = ERROR_DRIVE_LOCKED;
      goto FS_DELETEEXIT;
      }
   if (!pVolInfo->fDiskCleanOnMount)
      {
      rc = ERROR_VOLUME_DIRTY;
      goto FS_DELETEEXIT;
      }
   if (pVolInfo->fWriteProtected)
      {
      rc = ERROR_WRITE_PROTECT;
      goto FS_DELETEEXIT;
      }

   pOpenInfo = malloc(sizeof (OPENINFO));
   if (!pOpenInfo)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto FS_DELETEEXIT;
      }

   pDirEntry = (PDIRENTRY)malloc((size_t)sizeof(DIRENTRY));
   if (!pDirEntry)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto FS_DELETEEXIT;
      }
#ifdef EXFAT
   pDirEntry1 = (PDIRENTRY1)pDirEntry;

   pDirEntryStream = (PDIRENTRY1)malloc((size_t)sizeof(DIRENTRY1));
   if (!pDirEntryStream)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto FS_DELETEEXIT;
      }
   pDirStream = (PDIRENTRY1)malloc((size_t)sizeof(DIRENTRY1));
   if (!pDirStream)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto FS_DELETEEXIT;
      }
   pDirSHInfo = (PSHOPENINFO)malloc((size_t)sizeof(SHOPENINFO));
   if (!pDirSHInfo)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto FS_DELETEEXIT;
      }
#endif

   szLongName = (PSZ)malloc((size_t)FAT32MAXPATH);
   if (!szLongName)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto FS_DELETEEXIT;
      }

   if( TranslateName(pVolInfo, 0L, NULL, pFile, szLongName, TRANSLATE_SHORT_TO_LONG ))
      strcpy( szLongName, pFile );

#ifdef EXFAT
   if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
      pszFile2 = szLongName;
   else
#endif
      pszFile2 = pFile;

   if (usCurDirEnd == (USHORT)(strrchr(pFile, '\\') - pFile + 1))
      {
      usCurDirEnd = strrchr(pszFile2, '\\') - pszFile2 + 1;
      }

   pOpenInfo->pSHInfo = GetSH( szLongName, pOpenInfo);
   if (!pOpenInfo->pSHInfo)
      {
      rc = ERROR_TOO_MANY_OPEN_FILES;
      goto FS_DELETEEXIT;
      }
   //pOpenInfo->pSHInfo->sOpenCount++; //
   if (pOpenInfo->pSHInfo->sOpenCount > 1)
      {
      rc = ERROR_ACCESS_DENIED;
      goto FS_DELETEEXIT;
      }
   pOpenInfo->pSHInfo->fLock = TRUE;

   if (strlen(pFile) > FAT32MAXPATH)
      {
      rc = ERROR_FILENAME_EXCED_RANGE;
      goto FS_DELETEEXIT;
      }

   ulDirCluster = FindDirCluster(pVolInfo,
      pcdfsi,
      pcdfsd,
      pszFile2,
      usCurDirEnd,
      RETURN_PARENT_DIR,
      &pszFile,
      pDirStream);

   if (ulDirCluster == pVolInfo->ulFatEof)
      {
      rc = ERROR_PATH_NOT_FOUND;
      goto FS_DELETEEXIT;
      }

#ifdef EXFAT
   if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
      {
      SetSHInfo1(pVolInfo, pDirStream, pDirSHInfo);
      }
#endif

   ulCluster = FindPathCluster(pVolInfo, ulDirCluster, pszFile, pDirSHInfo,
                               pDirEntry, pDirEntryStream, NULL);
   if (ulCluster == pVolInfo->ulFatEof)
      {
      rc = ERROR_FILE_NOT_FOUND;
      goto FS_DELETEEXIT;
      }


#ifdef EXFAT
   if ( ((pVolInfo->bFatType <  FAT_TYPE_EXFAT) && (pDirEntry->bAttr & FILE_DIRECTORY)) ||
        ((pVolInfo->bFatType == FAT_TYPE_EXFAT) && (pDirEntry1->u.File.usFileAttr & FILE_DIRECTORY)) )
#else
   if ( pDirEntry->bAttr & FILE_DIRECTORY )
#endif
      {
      rc = ERROR_ACCESS_DENIED;
      goto FS_DELETEEXIT;
      }

#ifdef EXFAT
   if ( ((pVolInfo->bFatType <  FAT_TYPE_EXFAT) && (pDirEntry->bAttr & FILE_READONLY)) ||
        ((pVolInfo->bFatType == FAT_TYPE_EXFAT) && (pDirEntry1->u.File.usFileAttr & FILE_READONLY)) )
#else
   if ( pDirEntry->bAttr & FILE_READONLY )
#endif
      {
      rc = ERROR_ACCESS_DENIED;
      goto FS_DELETEEXIT;
      }

   if (f32Parms.fEAS)
      {
      rc = usDeleteEAS(pVolInfo, ulDirCluster, pDirSHInfo, pszFile);
      if (rc)
         goto FS_DELETEEXIT;
#if 0
      if (pDirEntry->fEAS == FILE_HAS_EAS || pDirEntry->fEAS == FILE_HAS_CRITICAL_EAS)
         pDirEntry->fEAS = FILE_HAS_NO_EAS;
#endif
      }

   rc = ModifyDirectory(pVolInfo, ulDirCluster, pDirSHInfo, MODIFY_DIR_DELETE,
                        pDirEntry, NULL, pDirEntryStream, NULL, pszFile, pszFile, 0);
   //rc = ModifyDirectory(pVolInfo, ulDirCluster, pDirSHInfo, MODIFY_DIR_DELETE,
   //                     pDirEntry, NULL, pDirEntryStream, NULL, NULL, 0);
   if (rc)
      goto FS_DELETEEXIT;

   if (ulCluster)
      DeleteFatChain(pVolInfo, ulCluster);
   rc = 0;


FS_DELETEEXIT:

   if (pOpenInfo)
      {
      if (pOpenInfo->pSHInfo)
         ReleaseSH(pOpenInfo);
      else
         free(pOpenInfo);
      }

   if (szLongName)
      free(szLongName);
   if (pDirEntry)
      free(pDirEntry);
#ifdef EXFAT
   if (pDirEntryStream)
      free(pDirEntryStream);
   if (pDirStream)
      free(pDirStream);
   if (pDirSHInfo)
      free(pDirSHInfo);
#endif

   MessageL(LOG_FS, "FS_DELETE%m returned %u", 0x8026, rc);

   _asm pop es;

   return rc;
}

/******************************************************************
*
******************************************************************/
void far pascal _loadds FS_EXIT(
    unsigned short usUid,       /* uid      */
    unsigned short usPid,       /* pid      */
    unsigned short usPdb        /* pdb      */
)
{
PVOLINFO pVolInfo = pGlobVolInfo;
PFINFO pFindInfo;
USHORT rc;

   _asm push es;

   MessageL(LOG_FS, "FS_EXIT%m for PID: %X, PDB %X",
            0x0027, usPid, usPdb);

   while (pVolInfo)
      {
      rc = MY_PROBEBUF(PB_OPWRITE, (PBYTE)pVolInfo, sizeof (VOLINFO));
      if (rc)
         {
         FatalMessage("FAT32: Protection VIOLATION (Volinfo) in FS_EXIT! (SYS%d)", rc);
         goto FS_EXITEXIT;
         }

      if (pVolInfo->fLocked &&
          pVolInfo->ProcLocked.usPid == usPid &&
          pVolInfo->ProcLocked.usUid == usUid &&
          pVolInfo->ProcLocked.usPdb == usPdb)
         pVolInfo->fLocked = FALSE;

      pFindInfo = (PFINFO)pVolInfo->pFindInfo;
      while (pFindInfo)
         {
         MessageL(LOG_FUNCS, "Still findinfo's allocated%m", 0x400e);
         rc = MY_PROBEBUF(PB_OPWRITE, (PBYTE)pFindInfo, sizeof (FINFO));
         if (rc)
            {
            FatalMessage("FAT32: Protection VIOLATION (FindInfo) in FS_EXIT! (SYS%d)", rc);
            Message("FAT32: Protection VIOLATION (FindInfo) in FS_EXIT! (SYS%d)", rc);
            goto FS_EXITEXIT;
            }

         if (pFindInfo->ProcInfo.usPid == usPid &&
             pFindInfo->ProcInfo.usUid == usUid &&
             pFindInfo->ProcInfo.usPdb == usPdb)
            {
            MessageL(LOG_FUNCS, "Removing a FINDINFO%m", 0x400f);
            if (RemoveFindEntry(pVolInfo, pFindInfo))
               free(pFindInfo);
            pFindInfo = (PFINFO)pVolInfo->pFindInfo;
            }
         else
            pFindInfo = (PFINFO)pFindInfo->pNextEntry;
         }
      pVolInfo = (PVOLINFO)pVolInfo->pNextVolInfo;
      }

FS_EXITEXIT:
   _asm pop es;

   return ;
}



/******************************************************************
*
******************************************************************/
int far pascal _loadds FS_FLUSHBUF(
    unsigned short hVPB,        /* hVPB     */
    unsigned short usFlag       /* flag     */
)
{
PVOLINFO pVolInfo;
USHORT rc;

   _asm push es;

   pVolInfo = GetVolInfo(hVPB);

   if (! pVolInfo)
      {
      rc = ERROR_INVALID_DRIVE;
      goto FS_FLUSHEXIT;
      }

   if (pVolInfo->fFormatInProgress)
      {
      rc = ERROR_ACCESS_DENIED;
      goto FS_FLUSHEXIT;
      }

   MessageL(LOG_FS, "FS_FLUSHBUF%m, flag = %d", 0x0028, usFlag);

   if (pVolInfo->fWriteProtected)
      {
      rc = 0;
      goto FS_FLUSHEXIT;
      }

   rc = usFlushVolume(pVolInfo, usFlag, TRUE, PRIO_URGENT);

   if (rc)
      goto FS_FLUSHEXIT;

   if (!UpdateFSInfo(pVolInfo))
      {
      rc = ERROR_SECTOR_NOT_FOUND;
      goto FS_FLUSHEXIT;
      }

   if (! pVolInfo->fRemovable && (pVolInfo->bFatType != FAT_TYPE_FAT12) )
      {
      // ignore dirty status on floppies (and FAT12)
      if (!MarkDiskStatus(pVolInfo, pVolInfo->fDiskCleanOnMount))
         {
         rc = ERROR_SECTOR_NOT_FOUND;
         goto FS_FLUSHEXIT;
         }
      }
FS_FLUSHEXIT:

   MessageL(LOG_FS, "FS_FLUSHBUF%m returned %u", 0x8028, rc);

   _asm pop es;

   return rc;
}

PID queryCurrentPid()
{
   APIRET rc;

   struct {
         PID pid;
         USHORT uid;
         USHORT pdb;
   } info;

   rc = FSH_QSYSINFO(QSI_PROCID, (char far *) &info, sizeof info);

   if (rc)
      Message("Cannot query PID!");

   return info.pid;
}

LIN virtToLin(void far *p)
{
   APIRET rc;
   LIN lin;

   rc = DevHelp_VirtToLin(
      SELECTOROF(p),
      OFFSETOF(p),
      &lin);

   if (rc)
      Message("virtToLin failed!");

   return lin;
}


APIRET daemonStarted(PEXBUF pexbuf)
{
APIRET rc;
PAGELIST pagelist[17];
ULONG pages;
LIN linaddr;

   rc = DevHelp_AllocGDTSelector((PSEL)&SELECTOROF(pCPData), 1);

   if (rc)
      return rc;

   rc = DevHelp_VMProcessToGlobal(VMDHPG_WRITE,
                                  pexbuf->buf, sizeof(CPDATA),
                                  &linaddr);

   if (rc)
      return rc;

   rc = DevHelp_VMLock(VMDHL_WRITE | VMDHL_LONG,
                       linaddr, sizeof(CPDATA),
                       virtToLin(pagelist),
                       virtToLin(lock),
                       &pages);

   if (rc)
      return rc;

   rc = DevHelp_LinToGDTSelector(SELECTOROF(pCPData),
                                 linaddr, sizeof(CPDATA));

   if (rc)
      return rc;

   pidDaemon = queryCurrentPid();

   rc = SemClear(&semRqDone); ////
   //rc = SemSet(&semRqAvail);

   if (rc)
      return rc;
   
   rc = FSH_SEMCLEAR(&semSerialize);

   return rc;
}

APIRET daemonStopped(void)
{
APIRET rc;

   pidDaemon = 0;

   SemClear(&semRqDone);

   SemSet(&semRqDone); ////
   SemSet(&semRqDone); ////

   rc = FSH_SEMREQUEST(&semSerialize, -1);

   if (rc)
      return rc;

   rc = FSH_SEMCLEAR(&semSerialize);

   DevHelp_VMUnLock(virtToLin(lock));
   DevHelp_FreeGDTSelector(SELECTOROF(pCPData));

   if (rc)
      return rc;

   return NO_ERROR;
}

/******************************************************************
*
******************************************************************/
int far pascal _loadds FS_FSCTL(
    union argdat far * pArgDat,     /* pArgdat  */
    unsigned short usArgType,       /* iArgType */
    unsigned short usFunc,      /* func     */
    char far * pParm,           /* pParm    */
    unsigned short cbParm,      /* lenParm  */
    unsigned short far * pcbParm,   /* plenParmOut  */
    char far * pData,           /* pData    */
    unsigned short cbData,      /* lenData  */
    unsigned short far * pcbData    /* plenDataOut  */
)
{
USHORT rc;
POPENINFO pOpenInfo;

   _asm push es;

   if (usFunc != FAT32_GETLOGDATA)
      MessageL(LOG_FS, "FS_FSCTL%m, Func = %x", 0x0029, usFunc);

   rc = 0;
   if (pData && pData != MYNULL)
      {
      if (cbData)
         {
         rc = MY_PROBEBUF(PB_OPWRITE, pData, cbData);
         if (rc)
            {
            Message("Protection VIOLATION in Data of FS_FSCTL!");
            goto FS_FSCTLEXIT;
            }
         }
      if (pcbData)
         {
         rc = MY_PROBEBUF(PB_OPWRITE, (PBYTE)pcbData, sizeof (USHORT));
         if (rc)
            pcbData = NULL;
         }
      if (pcbData)
         *pcbData = cbData;
      }
   else
      {
      cbData = 0;
      pcbData = NULL;
      }

   if (pParm && pParm != MYNULL)
      {
      if (cbParm)
         {
         rc = MY_PROBEBUF(PB_OPREAD, pParm, cbParm);
         if (rc)
            {
            Message("Protection VIOLATION in Parm of FS_FSCTL!");
            goto FS_FSCTLEXIT;
            }
         }
      if (pcbParm)
         {
         rc = MY_PROBEBUF(PB_OPREAD, (PBYTE)pcbParm, sizeof (USHORT));
         if (rc)
            pcbParm = NULL;
         }
      if (pcbParm)
         *pcbParm = cbParm;
      }
   else
      {
      cbParm = 0;
      pcbParm = NULL;
      }


   switch (usFunc)
      {
      case FSCTL_FUNC_NEW_INFO:
         if (cbData > 15)
            {
            strcpy(pData, "Unknown error");
            if (pcbData)
               *pcbData = strlen(pData) + 1;
            rc = 0;
            }
         else
            {
            if (pcbData)
               *pcbData = 15;
            rc = ERROR_BUFFER_OVERFLOW;
            }
         break;

      case FSCTL_FUNC_EASIZE:
         {
         PEASIZEBUF pEA = (PEASIZEBUF)pData;
         if (pcbData)
            *pcbData = sizeof (EASIZEBUF);
         if (cbData < sizeof (EASIZEBUF))
            {
            rc = ERROR_BUFFER_OVERFLOW;
            goto FS_FSCTLEXIT;
            }
         if (f32Parms.fEAS)
            {
            pEA->cbMaxEASize = MAX_EA_SIZE - sizeof (ULONG) - sizeof (FEA);
            pEA->cbMaxEAListSize = MAX_EA_SIZE;
            }
         else
            {
            pEA->cbMaxEASize = 0;
            pEA->cbMaxEAListSize = 0;
            }
         rc = 0;
         break;
         }

      case FSCTL_DAEMON_QUERY:
         rc = ERROR_INVALID_FUNCTION;
         goto FS_FSCTLEXIT;
#if 0
         {
         PFSDDAEMON pDaemon = (PFSDDAEMON)pData;
         if (cbData != sizeof (PFSDDAEMON))
            {
            rc = ERROR_BUFFER_OVERFLOW;
            goto FS_FSCTLEXIT;
            }
         memset(pDaemon, 0, sizeof (PFSDDAEMON));
         pDaemon->usNumThreads = 1;
         pDaemon->tdThrds[0].usFunc = FAT32_DOLW;
         pDaemon->tdThrds[0].usStackSize = 2028;
         pDaemon->tdThrds[0].ulPriorityClass = PRTYC_IDLETIME;
         pDaemon->tdThrds[0].lPriorityLevel = 0;
         rc = 0;
         }
         break;
#endif

      case FAT32_SECTORIO:
         if (usArgType != 1)
            {
            rc = ERROR_INVALID_FUNCTION;
            goto FS_FSCTLEXIT;
            }

         if (!(pArgDat->sf.psffsi->sfi_mode & OPEN_FLAGS_DASD))
            {
            rc = ERROR_INVALID_FUNCTION;
            goto FS_FSCTLEXIT;
            }
         if (cbParm < sizeof (ULONG))
            {
            rc = ERROR_INSUFFICIENT_BUFFER;
            goto FS_FSCTLEXIT;
            }
         if (*(PULONG)pParm != 0xDEADFACE)
            {
            rc = ERROR_INVALID_PARAMETER;
            goto FS_FSCTLEXIT;
            }
         pOpenInfo = GetOpenInfo(pArgDat->sf.psffsd);

         /* if a less volume than 2GB, converts a size from in bytes to in sectors. */
         if(!pOpenInfo->fLargeVolume)
            {
            struct vpfsi far * pvpfsi;
            struct vpfsd far * pvpfsd;

            FSH_GETVOLPARM(pArgDat->sf.psffsi->sfi_hVPB, &pvpfsi, &pvpfsd);
            pArgDat->sf.psffsi->sfi_size /= pvpfsi->vpi_bsize;

            if (f32Parms.fLargeFiles)
#ifdef INCL_LONGLONG
               pArgDat->sf.psffsi->sfi_sizel /= pvpfsi->vpi_bsize;
#else
               pArgDat->sf.psffsi->sfi_sizel = iDivUS(pArgDat->sf.psffsi->sfi_sizel, pvpfsi->vpi_bsize);
#endif
            }

         pOpenInfo->fSectorMode = TRUE;
         rc = 0;
         break;

      case FAT32_GETLOGDATA:
         if (f32Parms.fInShutDown)
            {
            rc = ERROR_ALREADY_SHUTDOWN;
            goto FS_FSCTLEXIT;
            }
         rc = GetLogBuffer(pData, cbData, *(PULONG)pParm);
         break;

      case FAT32_SETMESSAGE:
         if (cbParm < sizeof (USHORT))
            {
            rc = ERROR_INSUFFICIENT_BUFFER;
            goto FS_FSCTLEXIT;
            }
         f32Parms.fMessageActive = *(BOOL *)pParm;
         rc = 0;
         break;

      case FAT32_STARTLW:
         if (f32Parms.fLW || ( !f32Parms.usCacheSize && !f32Parms.fForceLoad ))
            {
            rc = ERROR_INVALID_FUNCTION;
            goto FS_FSCTLEXIT;
            }

         // disable it temporarily
         f32Parms.fLW = TRUE;
         rc = 0;
         break;

      case FAT32_STOPLW:
         if (!f32Parms.fLW)
            {
            rc = ERROR_INVALID_FUNCTION;
            goto FS_FSCTLEXIT;
            }

         f32Parms.fLW = FALSE;
         Message("Lazy writing is OFF");
         rc = usFlushAll();
         break;

      case FAT32_DOLW:
         if (cbParm < sizeof (LWOPTS))
            {
            rc = ERROR_INSUFFICIENT_BUFFER;
            goto FS_FSCTLEXIT;
            }
         DoLW(NULL, (PLWOPTS)pParm);
         rc = 0;
         break;

      case FAT32_EMERGTHREAD:
         if (cbParm < sizeof (LWOPTS))
            {
            rc = ERROR_INSUFFICIENT_BUFFER;
            goto FS_FSCTLEXIT;
            }
         DoEmergencyFlush((PLWOPTS)pParm);
         rc = 0;
         break;

      case FAT32_SETPARMS:
         if (cbParm > sizeof (F32PARMS))
            {
            rc = ERROR_INSUFFICIENT_BUFFER;
            goto FS_FSCTLEXIT;
            }
         f32Parms.ulDiskIdle       = ((PF32PARMS)pParm)->ulDiskIdle;
         f32Parms.ulBufferIdle     = ((PF32PARMS)pParm)->ulBufferIdle;
         f32Parms.ulMaxAge         = ((PF32PARMS)pParm)->ulMaxAge;
         f32Parms.fMessageActive   = ((PF32PARMS)pParm)->fMessageActive;
         f32Parms.ulCurCP          = ((PF32PARMS)pParm)->ulCurCP;
         f32Parms.fForceLoad       = ((PF32PARMS)pParm)->fForceLoad;

         /*
            Codepage is changed only by FAT32_SETTRANSTABLE,
            so we don't worry about not modifying DBCS lead byte info here.
         */

         rc = 0;
         break;

      case FAT32_GETPARMS:
         if (pcbData)
            *pcbData = sizeof f32Parms;
         if (cbData < sizeof (F32PARMS))
            {
            rc = ERROR_BUFFER_OVERFLOW;
            goto FS_FSCTLEXIT;
            }
         memcpy(pData, &f32Parms, sizeof (F32PARMS));
         rc = 0;
         break;

      case FAT32_SETTRANSTABLE:
         if (!cbParm)
            f32Parms.fTranslateNames = FALSE;
         else
            {
#if 0
            if (cbParm != 512)
               {
               rc = ERROR_INSUFFICIENT_BUFFER;
               goto FS_FSCTLEXIT;
               }
#endif
            if( !TranslateInit(pParm, cbParm))
                {
                rc = ERROR_INVALID_PARAMETER;
                goto FS_FSCTLEXIT;
                }
            }
         rc = 0;
         break;

      case FAT32_WIN2OS:
         Translate2OS2((PUSHORT)pParm, pData, cbData);
         rc = 0;
         break;

      case FAT32_QUERYSHORTNAME:
         {
         //char szShortPath[ FAT32MAXPATH ] = { 0, };
         PSZ szShortPath = (PSZ)malloc((size_t)FAT32MAXPATH);
         PVOLINFO pVolInfo = pGlobVolInfo;
         BYTE     bDrive;

         bDrive = *pParm;
         if (bDrive >= 'a' && bDrive <= 'z')
            bDrive -= ('a' - 'A');
         bDrive -= 'A';
         while (pVolInfo)
            {
            if (pVolInfo->bDrive == bDrive)
               break;
            pVolInfo = (PVOLINFO)pVolInfo->pNextVolInfo;
            }
         if (pVolInfo->fFormatInProgress)
            {
            free(szShortPath);
            rc = ERROR_ACCESS_DENIED;
            goto FS_FSCTLEXIT;
            }
         if (pVolInfo)
         {
            TranslateName(pVolInfo, 0L, NULL, (PSZ)pParm, szShortPath, TRANSLATE_LONG_TO_SHORT);
            if( strlen( szShortPath ) >= cbData )
            {
                free(szShortPath);
                rc = ERROR_BUFFER_OVERFLOW;
                goto FS_FSCTLEXIT;
            }

            strcpy((PSZ)pData, szShortPath );
         }

         free(szShortPath);
         rc = 0;
         break;
         }

      case FAT32_GETCASECONVERSION:
        if( cbData < 256 )
        {
            rc = ERROR_BUFFER_OVERFLOW;
            goto FS_FSCTLEXIT;
        }

        GetCaseConversion( pData );

        rc = 0;
        break;

      case FAT32_GETFIRSTINFO:
        if( cbData < sizeof( UCHAR ) * 256 )
        {
            rc = ERROR_BUFFER_OVERFLOW;
            goto FS_FSCTLEXIT;
        }

        GetFirstInfo(( PBOOL )pData );
        rc = 0;
        break;

      case FAT32_MOUNT:
        {
        PVOLINFO pVolInfo2; 

        if (cbParm < sizeof(MNTOPTS))
           {
           rc = ERROR_BUFFER_OVERFLOW;
           goto FS_FSCTLEXIT;
           }
        rc = Mount2((PMNTOPTS)pParm, &pVolInfo2);
        }
        break;

      case FAT32_MOUNTED:
         {
         PVOLINFO pVolInfo2 = pGlobVolInfo;

         typedef struct
            {
            UCHAR ucIsMounted;
            char szPath[CCHMAXPATHCOMP];
            } Data;

         Data far *pData2 = (Data far *)pData;

         while (pVolInfo2)
            {
            if (pVolInfo2->pbFilename && !stricmp(pVolInfo2->pbFilename, pData2->szPath))
               break;
            pVolInfo2 = pVolInfo2->pNextVolInfo;
            }
         
         if (! pVolInfo2)
            pData2->ucIsMounted = 0;
         else
            pData2->ucIsMounted = 1;
         }
         break;

      case FAT32_DAEMON_STARTED:
         if (pidDaemon)
            {
            rc = ERROR_ALREADY_EXISTS;
            goto FS_FSCTLEXIT;
            }

         if (cbParm < sizeof(EXBUF))
            {
            rc = ERROR_INVALID_PARAMETER;
            goto FS_FSCTLEXIT;
            }

         rc = daemonStarted((PEXBUF)pParm);
         break;

      case FAT32_DAEMON_STOPPED:
         if (! pidDaemon)
            {
            rc = NO_ERROR;
            goto FS_FSCTLEXIT;
            }

         rc = daemonStopped();
         break;

      case FAT32_DONE_REQ:
         if (! pidDaemon)
             return ERROR_INVALID_PROCID;

         if (queryCurrentPid() != pidDaemon)
            {
            rc = ERROR_ALREADY_ASSIGNED;
            goto FS_FSCTLEXIT;
            }

         rc = SemClear(&semRqDone);
         // fall through
         //break;

     case FAT32_GET_REQ:
         if (! pidDaemon)
             return ERROR_INVALID_PROCID;

         if (queryCurrentPid() != pidDaemon)
            {
            rc = ERROR_ALREADY_ASSIGNED;
            goto FS_FSCTLEXIT;
            }

         FSH_SEMCLEAR(&semSerialize);

         rc = SemWait(&semRqAvail);

         if (rc == WAIT_INTERRUPTED)
            {
            daemonStopped();
            rc = ERROR_INTERRUPT;
            break;
            }

         if (rc)
            break;
         
         rc = SemSet(&semRqAvail);
         break;

      case FAT32_GETVOLLABEL:
         {
         PVOLINFO pVolInfo = GetVolInfoX(pParm);

         if (! pVolInfo)
            {
            rc = ERROR_INVALID_DRIVE;
            goto FS_FSCTLEXIT;
            }

         if (! pidDaemon)
             return ERROR_INVALID_PROCID;

         rc = fGetSetFSInfo(INFO_RETRIEVE,
                            pVolInfo,
                            pData,
                            cbData,
                            FSIL_VOLSER);
         }
         break;

      case FAT32_GETALLOC:
         {
         PVOLINFO pVolInfo = GetVolInfoX(pParm);

         if (! pVolInfo)
            {
            rc = ERROR_INVALID_DRIVE;
            goto FS_FSCTLEXIT;
            }

         if (! pidDaemon)
             return ERROR_INVALID_PROCID;

         rc = fGetSetFSInfo(INFO_RETRIEVE,
                            pVolInfo,
                            pData,
                            cbData,
                            FSIL_ALLOC);
         }
         break;

      case FAT32_SETVOLLABEL:
         {
         PVOLINFO pVolInfo = GetVolInfoX(pParm);

         if (! pVolInfo)
            {
            rc = ERROR_INVALID_DRIVE;
            goto FS_FSCTLEXIT;
            }

         if (! pidDaemon)
             return ERROR_INVALID_PROCID;

         rc = fGetSetFSInfo(INFO_SET,
                            pVolInfo,
                            pData,
                            cbData,
                            FSIL_VOLSER);
         }
         break;

      default :
         rc = ERROR_INVALID_FUNCTION;
         break;
      }

FS_FSCTLEXIT:

   if (usFunc != FAT32_GETLOGDATA)
      MessageL(LOG_FS, "FS_FSCTL%m returned %u", 0x8029, rc);

   _asm pop es;

   return rc;
}

/******************************************************************
*
******************************************************************/
int far pascal _loadds FS_FSINFO(
    unsigned short usFlag,      /* flag     */
    unsigned short hVBP,        /* hVPB     */
    char far * pData,           /* pData    */
    unsigned short cbData,      /* cbData   */
    unsigned short usLevel      /* level    */
)
{
PVOLINFO pVolInfo;
USHORT rc;

   _asm push es;

   MessageL(LOG_FS, "FS_FSINFO%m, Flag = %d, Level = %d", 0x002a, usFlag, usLevel);

   pVolInfo = GetVolInfo(hVBP);

   if (! pVolInfo)
      {
      rc = ERROR_INVALID_DRIVE;
      goto FS_FSINFOEXIT;
      }

   rc = fGetSetFSInfo(usFlag,
                      pVolInfo,
                      pData,
                      cbData,
                      usLevel);

FS_FSINFOEXIT:

   MessageL(LOG_FS, "FS_FSINFO%m returned %u", 0x802a, rc);

   _asm pop es;

   return rc;
}

int fGetSetFSInfo(
    unsigned short usFlag,      /* flag     */
    PVOLINFO pVolInfo,          /* pVolInfo */
    char far * pData,           /* pData    */
    unsigned short cbData,      /* cbData   */
    unsigned short usLevel      /* level    */
)
{
USHORT rc;

   if (pVolInfo->fFormatInProgress)
      {
      rc = ERROR_ACCESS_DENIED;
      goto FSINFOEXIT;
      }

   if (IsDriveLocked(pVolInfo))
      {
      rc = ERROR_DRIVE_LOCKED;
      goto FSINFOEXIT;
      }

   rc = MY_PROBEBUF(PB_OPWRITE, pData, cbData);
   if (rc)
      {
      Message("Protection VIOLATION in FS_FSINFO!");
      goto FSINFOEXIT;
      }

   if (usFlag == INFO_RETRIEVE)
      {
      switch (usLevel)
         {
         case FSIL_ALLOC:
            {
            PFSALLOCATE pAlloc = (PFSALLOCATE)pData;

            if (cbData < sizeof (FSALLOCATE))
               {
               rc = ERROR_BUFFER_OVERFLOW;
               goto FSINFOEXIT;
               }
#ifdef OLD_SOURCE
            pAlloc->ulReserved  = 0L;
#endif
            pAlloc->cbSector = pVolInfo->BootSect.bpb.BytesPerSector;

            if (IsDosSession()) /* Dos Session */
               {
               ULONG ulTotalSectors = pVolInfo->SectorsPerCluster * pVolInfo->ulTotalClusters;
               ULONG ulFreeSectors  = pVolInfo->SectorsPerCluster * pVolInfo->pBootFSInfo->ulFreeClusters;

               if (ulTotalSectors > 32L * 65526L)
                  pAlloc->cSectorUnit = 64;
               else if (ulTotalSectors > 16L * 65526L)
                  pAlloc->cSectorUnit = 32;
               else if (ulTotalSectors > 8L * 65526L)
                  pAlloc->cSectorUnit = 16;
               else
                  pAlloc->cSectorUnit = 8;

               if ((ULONG)pVolInfo->SectorsPerCluster > pAlloc->cSectorUnit)
                  pAlloc->cSectorUnit = (USHORT)pVolInfo->SectorsPerCluster;

               pAlloc->cUnit = min(65526L, ulTotalSectors / pAlloc->cSectorUnit);
               pAlloc->cUnitAvail = min(65526L, ulFreeSectors / pAlloc->cSectorUnit);

               MessageL(LOG_FUNCS, "DOS Free space%m: sc: %lu tc: %lu fc: %lu",
                        0x4010, pAlloc->cSectorUnit, pAlloc->cUnit, pAlloc->cUnitAvail);
               }
            else
               {
               pAlloc->cSectorUnit = pVolInfo->SectorsPerCluster;
               pAlloc->cUnit = pVolInfo->ulTotalClusters;
               pAlloc->cUnitAvail = pVolInfo->pBootFSInfo->ulFreeClusters;
               }
            rc = 0;
            break;
            }
         case FSIL_VOLSER:
            {
            PFSINFO pInfo = (PFSINFO)pData;
            USHORT usSize;
            if (cbData < sizeof (FSINFO))
               {
               rc = ERROR_BUFFER_OVERFLOW;
               goto FSINFOEXIT;
               }

#ifndef __WATCOMC__
            // no such field in Toolkit or Watcom headers
            pInfo->ulVSN = pVolInfo->BootSect.ulVolSerial;
#else
            // low word (aka fdateCreation)
            *((PUSHORT)(pInfo))     = (pVolInfo->BootSect.ulVolSerial & 0xffff);
            *((PUSHORT)(pInfo) + 1) = (pVolInfo->BootSect.ulVolSerial >> 16);
#endif

            memset(pInfo->vol.szVolLabel, 0, sizeof pInfo->vol.szVolLabel);

            usSize = 11;
            rc = fGetSetVolLabel(pVolInfo, usFlag, pInfo->vol.szVolLabel, &usSize);
            pInfo->vol.cch = (BYTE)usSize;
            break;
            }
         default :
            rc = ERROR_BAD_COMMAND;
            break;
         }
      }
   else if (usFlag == INFO_SET)
      {
      switch (usLevel)
         {
         case FSIL_VOLSER:
            {
            PVOLUMELABEL pVol = (PVOLUMELABEL)pData;
            USHORT usSize;
            if (cbData < 1)
               {
               rc = ERROR_INSUFFICIENT_BUFFER;
               goto FSINFOEXIT;
               }

            if (pVol->cch < (BYTE)12)
               {
               usSize = (USHORT) pVol->cch;
               while (pVol->szVolLabel[usSize - 1] == ' ')
                  usSize --;
               rc = fGetSetVolLabel(pVolInfo, usFlag, pVol->szVolLabel, &usSize);
               }
            else
               {
               usSize = 11;
               while (pVol->szVolLabel[usSize - 1] == ' ')
                  usSize --;
               rc = fGetSetVolLabel(pVolInfo, usFlag, pVol->szVolLabel, &usSize);
               if (!rc)
                  rc = ERROR_LABEL_TOO_LONG;
               }
            break;
            }

         default :
            rc = ERROR_BAD_COMMAND;
            break;
         }
      }
   else
      rc = ERROR_BAD_COMMAND;

FSINFOEXIT:

   return rc;
}

USHORT fGetSetVolLabel(PVOLINFO pVolInfo, USHORT usFlag, PSZ pszVolLabel, PUSHORT pusSize)
{
struct vpfsi far * pvpfsi;
struct vpfsd far * pvpfsd;
PDIRENTRY pDirStart, pDir, pDirEnd;
ULONG ulCluster;
ULONG ulBlock;
PDIRENTRY pDirEntry;
BOOL     fFound;
USHORT   rc;
BYTE     bAttr = FILE_VOLID | FILE_ARCHIVED;
USHORT   usIndex;
PBOOTSECT pBootSect;
ULONG  ulSector;
USHORT usSectorsRead;
USHORT usSectorsPerBlock;
ULONG  ulDirEntries = 0;

   // some sanity checks first
   if (! pszVolLabel || ! pusSize)
      return ERROR_INVALID_PARAMETER;

   pDir = NULL;

   pDirEntry = (PDIRENTRY)malloc((size_t)sizeof(DIRENTRY));
   if (!pDirEntry)
      {
      return ERROR_NOT_ENOUGH_MEMORY;
      }

   pDirStart = (PDIRENTRY)malloc((size_t)pVolInfo->ulBlockSize);
   if (!pDirStart)
      {
      free(pDirEntry);
      return ERROR_NOT_ENOUGH_MEMORY;
      }

   fFound = FALSE;
   ulCluster = pVolInfo->BootSect.bpb.RootDirStrtClus;
   if (ulCluster == 1)
      {
      // FAT12/FAT16 root directory starting sector
      ulSector = pVolInfo->BootSect.bpb.ReservedSectors +
         (ULONG)pVolInfo->BootSect.bpb.SectorsPerFat * pVolInfo->BootSect.bpb.NumberOfFATs;
      usSectorsPerBlock = (USHORT)((ULONG)pVolInfo->SectorsPerCluster /
         ((ULONG)pVolInfo->ulClusterSize / pVolInfo->ulBlockSize));
      usSectorsRead = 0;
      }
   while (!fFound && ulCluster != pVolInfo->ulFatEof)
      {
      for (ulBlock = 0; ulBlock < pVolInfo->ulClusterSize / pVolInfo->ulBlockSize; ulBlock++)
         {
         if (ulCluster == 1)
            // reading root directory on FAT12/FAT16
            rc = ReadSector(pVolInfo, ulSector + ulBlock * usSectorsPerBlock, usSectorsPerBlock, (void *)pDirStart, 0);
         else
            rc = ReadBlock(pVolInfo, ulCluster, ulBlock, pDirStart, 0);
         if (rc)
            {
            free(pDirStart);
            free(pDirEntry);
            return rc;
            }
         pDir    = pDirStart;
         pDirEnd = (PDIRENTRY)((PBYTE)pDirStart + pVolInfo->ulBlockSize);
#ifdef EXFAT
         if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
            {
#endif
            while (pDir < pDirEnd)
               {
               if ((pDir->bAttr & 0x0F) == FILE_VOLID && pDir->bFileName[0] != DELETED_ENTRY)
                  {
                  fFound = TRUE;
                  memcpy(pDirEntry, pDir, sizeof (DIRENTRY));
                  break;
                  }
               pDir++;
               ulDirEntries++;
               if (ulCluster == 1 && ulDirEntries > pVolInfo->BootSect.bpb.RootDirEntries)
                  break;
               }
#ifdef EXFAT
            }
         else
            {
            // exFAT case
            while (pDir < pDirEnd)
               {
               if (((PDIRENTRY1)pDir)->bEntryType == ENTRY_TYPE_VOLUME_LABEL)
                  {
                  fFound = TRUE;
                  memcpy(pDirEntry, pDir, sizeof (DIRENTRY));
                  break;
                  }
               pDir++;
               ulDirEntries++;
               }
            }
#endif
         if (fFound)
            break;
         if (!fFound)
            {
            if (ulCluster == 1)
               {
               // reading the root directory in case of FAT12/FAT16
               ulSector += pVolInfo->SectorsPerCluster;
               usSectorsRead += pVolInfo->SectorsPerCluster;
               if ((ULONG)usSectorsRead * pVolInfo->BootSect.bpb.BytesPerSector >=
                   (ULONG)pVolInfo->BootSect.bpb.RootDirEntries * sizeof(DIRENTRY))
                  // root directory ended
                  ulCluster = 0;
               }
            else
               ulCluster = GetNextCluster(pVolInfo, NULL, ulCluster);
            if (!ulCluster)
               ulCluster = pVolInfo->ulFatEof;
            }
         pDir++;
         ulDirEntries++;
         if (ulCluster == pVolInfo->ulFatEof)
            break;
         if (ulCluster == 1 && ulDirEntries > pVolInfo->BootSect.bpb.RootDirEntries)
            break;
         }
      if (ulCluster == 1 && ulDirEntries > pVolInfo->BootSect.bpb.RootDirEntries)
         break;
      }

   if (usFlag == INFO_RETRIEVE)
      {
      free(pDirStart);
      if (!fFound)
         {
         memset(pszVolLabel, 0, *pusSize);
         *pusSize = 0;
         free(pDirEntry);
         return 0;
         }
      *pusSize = 11;
#ifdef EXFAT
      if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
#endif
         memcpy(pszVolLabel, pDirEntry->bFileName, 11);
#ifdef EXFAT
      else
         {
         // exFAT case
         USHORT pVolLabel[11];

         // additional sanity checks
         if (((PDIRENTRY1)pDirEntry)->u.VolLbl.bCharCount > 11)
            {            
            free(pDirEntry);
            return ERROR_INVALID_PARAMETER;
            }

         memcpy(pVolLabel, ((PDIRENTRY1)pDirEntry)->u.VolLbl.usChars,
            ((PDIRENTRY1)pDirEntry)->u.VolLbl.bCharCount * sizeof(USHORT));
         pVolLabel[((PDIRENTRY1)pDirEntry)->u.VolLbl.bCharCount] = 0;
         Translate2OS2(pVolLabel, pszVolLabel, ((PDIRENTRY1)pDirEntry)->u.VolLbl.bCharCount);
         }
#endif
      while (*pusSize > 0 && pszVolLabel[(*pusSize)-1] == 0x20)
         {
         (*pusSize)--;
         pszVolLabel[*pusSize] = 0;
         }
      free(pDirEntry);
      return 0;
      }

   strupr(pszVolLabel);
   for (usIndex = 0; usIndex < *pusSize; usIndex++)
      {
      if (!strchr(rgValidChars, pszVolLabel[usIndex]))
         {
         free(pDirStart);
         free(pDirEntry);
         return ERROR_INVALID_NAME;
         }
      }

   memset(pDirEntry, 0, sizeof(DIRENTRY));

#ifdef EXFAT
   if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
      {
#endif
      memset(pDirEntry->bFileName, 0x20, 11);
      memcpy(pDirEntry->bFileName, pszVolLabel, min(11, *pusSize));
      pDirEntry->bAttr = bAttr;
#ifdef EXFAT
      }
   else
      {
      // exFAT case
      USHORT pVolLabel[11];
      USHORT usChars = min(11, *pusSize);
      Translate2Win(pszVolLabel, pVolLabel, usChars);
      ((PDIRENTRY1)pDirEntry)->u.VolLbl.bCharCount = (BYTE)usChars;
      memcpy(((PDIRENTRY1)pDirEntry)->u.VolLbl.usChars, pVolLabel, usChars * sizeof(USHORT));
      ((PDIRENTRY1)pDirEntry)->bEntryType = ENTRY_TYPE_VOLUME_LABEL;
      }
#endif

   if (fFound)
      {
      memcpy(pDir, pDirEntry, sizeof (DIRENTRY));
      if (ulCluster == 1)
         // reading root directory on FAT12/FAT16
         rc = WriteSector(pVolInfo, ulSector + ulBlock * usSectorsPerBlock, usSectorsPerBlock, (void *)pDirStart, DVIO_OPWRTHRU);
      else
         rc = WriteBlock(pVolInfo, ulCluster, ulBlock, (PBYTE)pDirStart, DVIO_OPWRTHRU);
      if (rc)
         {
         free(pDirStart);
         free(pDirEntry);
         return rc;
         }
      }
   else
      {
      rc = ModifyDirectory(pVolInfo, pVolInfo->BootSect.bpb.RootDirStrtClus, NULL,
         MODIFY_DIR_INSERT, NULL, pDirEntry, NULL, NULL, pszVolLabel, pszVolLabel, DVIO_OPWRTHRU);
      }
   if (rc)
      {
      free(pDirStart);
      free(pDirEntry);
      return rc;
      }

#ifdef EXFAT
   if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
#endif
      {
      rc = ReadSector(pVolInfo, 0L, 1, (PBYTE)pDirStart, DVIO_OPNCACHE);
      if (!rc)
         {
         pBootSect = (PBOOTSECT)pDirStart;
         memcpy(pBootSect->VolumeLabel, pDirEntry->bFileName, 11);
         rc = WriteSector(pVolInfo, 0L, 1, (PBYTE)pBootSect, DVIO_OPWRTHRU | DVIO_OPNCACHE);
         if (rc)
            {
            free(pDirStart);
            free(pDirEntry);
            return rc;
            }
         }
      free(pDirStart);

      if (!rc)
         {
         memcpy(pVolInfo->BootSect.VolumeLabel, pDirEntry->bFileName, 11);
         rc = FSH_GETVOLPARM(pVolInfo->hVBP, &pvpfsi, &pvpfsd);
         if (!rc)
            memcpy(pvpfsi->vpi_text, pDirEntry->bFileName, 11);
         }
      }

   free(pDirEntry);
   return rc;
}

#ifdef EXFAT

USHORT fGetAllocBitmap(PVOLINFO pVolInfo, PULONG pulFirstCluster, PULONGLONG pullLen)
{
// This is relevant for exFAT only
PDIRENTRY1 pDirStart, pDir, pDirEnd;
PDIRENTRY1 pDirEntry;
ULONG ulCluster;
ULONG ulBlock;
BOOL fFound;

   pDir = NULL;

   pDirEntry = (PDIRENTRY1)malloc((size_t)sizeof(DIRENTRY1));
   if (!pDirEntry)
      {
      return ERROR_NOT_ENOUGH_MEMORY;
      }

   pDirStart = (PDIRENTRY1)malloc((size_t)pVolInfo->ulBlockSize);
   if (!pDirStart)
      {
      free(pDirEntry);
      return ERROR_NOT_ENOUGH_MEMORY;
      }

   fFound = FALSE;
   ulCluster = pVolInfo->BootSect.bpb.RootDirStrtClus;
   while (!fFound && ulCluster != pVolInfo->ulFatEof)
      {
      for (ulBlock = 0; ulBlock < pVolInfo->ulClusterSize / pVolInfo->ulBlockSize; ulBlock++)
         {
         ReadBlock(pVolInfo, ulCluster, ulBlock, pDirStart, 0);
         pDir    = pDirStart;
         pDirEnd = (PDIRENTRY1)((PBYTE)pDirStart + pVolInfo->ulBlockSize);
         while (pDir < pDirEnd)
            {
            if (pDir->bEntryType == ENTRY_TYPE_ALLOC_BITMAP)
               {
               fFound = TRUE;
               memcpy(pDirEntry, pDir, sizeof (DIRENTRY1));
               break;
               }
            pDir++;
            }
         if (fFound)
            break;
         else
            {
            ulCluster = GetNextCluster(pVolInfo, NULL, ulCluster);
            if (!ulCluster)
               ulCluster = pVolInfo->ulFatEof;
            }
         pDir++;
         if (ulCluster == pVolInfo->ulFatEof)
            break;
         }
      }

   *pulFirstCluster = pDirEntry->u.AllocBmp.ulFirstCluster;
   *pullLen = pDirEntry->u.AllocBmp.ullDataLength;

   free(pDirStart);
   free(pDirEntry);
   return 0;
}


USHORT fGetUpCaseTbl(PVOLINFO pVolInfo, PULONG pulFirstCluster, PULONGLONG pullLen, PULONG pulChecksum)
{
// This is relevant for exFAT only
PDIRENTRY1 pDirStart, pDir, pDirEnd;
PDIRENTRY1 pDirEntry;
ULONG ulCluster;
ULONG ulBlock;
BOOL fFound;

   pDir = NULL;

   pDirEntry = (PDIRENTRY1)malloc((size_t)sizeof(DIRENTRY1));
   if (!pDirEntry)
      {
      return ERROR_NOT_ENOUGH_MEMORY;
      }

   pDirStart = (PDIRENTRY1)malloc((size_t)pVolInfo->ulBlockSize);
   if (!pDirStart)
      return ERROR_NOT_ENOUGH_MEMORY;

   fFound = FALSE;
   ulCluster = pVolInfo->BootSect.bpb.RootDirStrtClus;
   while (!fFound && ulCluster != pVolInfo->ulFatEof)
      {
      for (ulBlock = 0; ulBlock < pVolInfo->ulClusterSize / pVolInfo->ulBlockSize; ulBlock++)
         {
         ReadBlock(pVolInfo, ulCluster, ulBlock, pDirStart, 0);
         pDir    = pDirStart;
         pDirEnd = (PDIRENTRY1)((PBYTE)pDirStart + pVolInfo->ulBlockSize);
         while (pDir < pDirEnd)
            {
            if (pDir->bEntryType == ENTRY_TYPE_UPCASE_TABLE)
               {
               fFound = TRUE;
               memcpy(pDirEntry, pDir, sizeof (DIRENTRY1));
               break;
               }
            pDir++;
            }
         if (fFound)
            break;
         else
            {
            ulCluster = GetNextCluster(pVolInfo, NULL, ulCluster);
            if (!ulCluster)
               ulCluster = pVolInfo->ulFatEof;
            }
         pDir++;
         if (ulCluster == pVolInfo->ulFatEof)
            break;
         }
      }

   *pulFirstCluster = pDirEntry->u.UpCaseTbl.ulFirstCluster;
   *pullLen = pDirEntry->u.UpCaseTbl.ullDataLength;
   *pulChecksum = pDirEntry->u.UpCaseTbl.ulTblCheckSum;

   free(pDirStart);
   free(pDirEntry);
   return 0;
}

#endif


/******************************************************************
*
******************************************************************/
int far pascal _loadds FS_INIT(
    char far * pszParm,         /* szParm   */
    unsigned long pDevHlp,      /* pDevHlp  */
    unsigned long far * pMiniFSD    /* pMiniFSD */
)
{
BOOL fSilent = FALSE;
PSZ  p;
PSZ  cmd = NULL;
char szObjname[256];
HMODULE hmod;
APIRET rc = 0;

   _asm push es;

   pMiniFSD = pMiniFSD;

   memset(&f32Parms, 0, sizeof f32Parms);

   f32Parms.fLW = FALSE;
   f32Parms.ulDiskIdle = 1000;
   f32Parms.ulBufferIdle = 500;
   f32Parms.ulMaxAge = 5000;
   f32Parms.usSegmentsAllocated = 0;
   strcpy(f32Parms.szVersion, FAT32_VERSION);

   Device_Help = pDevHlp;
   if (pszParm)
      {
      strncpy(szArguments, pszParm, sizeof szArguments);
      strlwr( szArguments );

      p = strstr(szArguments, "/monitor");
      if( !p )
        p = strstr( szArguments, "-monitor");
      if( p )
         f32Parms.fMessageActive = LOG_FS;

      p = strstr(szArguments, "/q");
      if( !p )
         p = strstr( szArguments, "-q");
      if( p )
         fSilent = TRUE;

      /*
         Get size of cache
      */

      p = strstr(szArguments, "/cache:");
      if (!p)
         p = strstr(szArguments, "-cache:");
      if (p)
         {
         p += 7;
         ulCacheSectors = atol(p) * 2;
         }
      else
         {
         InitMessage("FAT32: Using default cache size of 1024 Kb.");
         }

      /*
         Get RA Sectors
      */
      p = strstr(szArguments, "/rasectors:");
      if (!p)
         p = strstr(szArguments, "-rasectors:");
      if (p)
         {
         p +=11;
         usDefaultRASectors = atoi(p);
         }
      /*
         Get EA Settings
      */
      p = strstr(szArguments, "/eas");
      if (!p)
         p = strstr(szArguments, "-eas");
      if (p)
         f32Parms.fEAS = TRUE;

      p = strstr(szArguments, "/eas2");
      if (!p)
         p = strstr(szArguments, "-eas2");
      if (p)
         f32Parms.fEAS2 = TRUE;

      p = strstr( szArguments, "/h");
      if( !p )
        p = strstr( szArguments, "-h");
      if( p )
         f32Parms.fHighMem = TRUE;
      }

      p = strstr( szArguments, "/calcfree");
      if( !p )
         p = strstr( szArguments, "-calcfree");
      if( p )
         f32Parms.fCalcFree = TRUE;

      p = strstr( szArguments, "/cacheopt");
      if( !p )
         p = strstr( szArguments, "-cacheopt");
      if( p )
         {
         p += 9;
         cmd = p;
         }

      p = strstr(szArguments, "/autocheck:");
      if (!p)
         p = strstr(szArguments, "-autocheck:");
      if (p)
         {
         p += 11;
         }
      else
         {
         p = strstr(szArguments, "/ac:");
         if (!p)
            p = strstr(szArguments, "-ac:");
         if (p)
            {
            p += 4;
            }
         }

      if (p)
         {
         while (*p != '\0' && *p != ' ')
            {
            char ch = (char)tolower(*p);
            int num;
            if ('a' <= ch && ch <= 'z')
               {
               num = ch - 'a';
               autocheck_mask |= (1UL << num);
               }
            else if (*p == '+')
               {
               force_mask |= (1UL << num);
               }
            else if (*p == '*')
               {
               autocheck_mask = 0xffffffff;
               break;
               }
            p++;
            }
         }

      p = strstr(szArguments, "/largefiles");
      if (!p)
         p = strstr(szArguments, "-largefiles");
      if (p)
         {
         f32Parms.fLargeFiles = TRUE;
         }

      p = strstr(szArguments, "/readonly");
      if (!p)
         p = strstr(szArguments, "-readonly");
      if (p)
         {
         // mount dirty disk readonly
         f32Parms.fReadonly = TRUE;
         }

      p = strstr(szArguments, "/plus");
      if (!p)
         p = strstr(szArguments, "-plus");
      if (p)
         {
         // enable FAT+ support
         f32Parms.fFatPlus = TRUE;
         // need Large Files support too
         f32Parms.fLargeFiles = TRUE;
         }

      p = strstr(szArguments, "/fat:");
      if (!p)
         p = strstr(szArguments, "-fat:");
      if (p)
         {
         // mount FAT12/FAT16 disks
         f32Parms.fFat = TRUE;
         p += 5;

         if (*p == '-')
            {
            // 'disable' mask
            fat_mask = 0xffffffff;

            while (*p != '\0' && *p != ' ')
               {
               char ch = (char)tolower(*p);
               int num;
               if ('a' <= ch && ch <= 'z')
                  {
                  num = ch - 'a';
                  // set FAT12/FAT16 disks mount mask
                  fat_mask &= ~(1UL << num);
                  }
               p++;
               }
            }
         else
            {
            while (*p != '\0' && *p != ' ')
               {
               char ch = (char)tolower(*p);
               int num;
               if ('a' <= ch && ch <= 'z')
                  {
                  num = ch - 'a';
                  // set FAT12/FAT16 disks mount mask
                  fat_mask |= (1UL << num);
                  }
               else if (*p == '*')
                  {
                  // mount all FAT12/FAT16 disks
                  fat_mask = 0xffffffff;
                  break;
                  }
               p++;
               }
            }
         }
      else
         {
         p = strstr(szArguments, "/fat");
         if (!p)
            p = strstr(szArguments, "-fat");
         if (p)
            {
            // mount all FAT12/FAT16 disks
            f32Parms.fFat = TRUE;
            fat_mask = 0xffffffff;
            }
         }

      p = strstr(szArguments, "/fat32:");
      if (!p)
         p = strstr(szArguments, "-fat32:");
      if (p)
         {
         p += 7;

         if (*p == '-')
            {
            // 'disable' mask
            fat32_mask = 0xffffffff;

            while (*p != '\0' && *p != ' ')
               {
               char ch = (char)tolower(*p);
               int num;
               if ('a' <= ch && ch <= 'z')
                  {
                  num = ch - 'a';
                  // set FAT12/FAT16 disks mount mask
                  fat32_mask &= ~(1UL << num);
                  }
               p++;
               }
            }
         else
            {
            while (*p != '\0' && *p != ' ')
               {
               char ch = (char)tolower(*p);
               int num;
               if ('a' <= ch && ch <= 'z')
                  {
                  num = ch - 'a';
                  // set FAT32 disks mount mask
                  fat32_mask |= (1UL << num);
                  }
               else if (*p == '*')
                  {
                  // mount all FAT32 disks
                  fat32_mask = 0xffffffff;
                  break;
                  }
               p++;
               }
            }
         }
      else
         {
         // mount all FAT32 disks
         fat32_mask = 0xffffffff;
         }

#ifdef EXFAT
      p = strstr(szArguments, "/exfat:");
      if (!p)
         p = strstr(szArguments, "-exfat:");
      if (p)
         {
         // mount exFAT disks
         f32Parms.fExFat = TRUE;
         p += 7;

         if (*p == '-')
            {
            // 'disable' mask
            exfat_mask = 0xffffffff;

            while (*p != '\0' && *p != ' ')
               {
               char ch = (char)tolower(*p);
               int num;
               if ('a' <= ch && ch <= 'z')
                  {
                  num = ch - 'a';
                  // set FAT12/FAT16 disks mount mask
                  exfat_mask &= ~(1UL << num);
                  }
               p++;
               }
            }
         else
            {
            while (*p != '\0' && *p != ' ')
               {
               char ch = (char)tolower(*p);
               int num;
               if ('a' <= ch && ch <= 'z')
                  {
                  num = ch - 'a';
                  // set exFAT disks mount mask
                  exfat_mask |= (1UL << num);
                  }
               else if (*p == '*')
                  {
                  // mount all exFAT disks
                  exfat_mask = 0xffffffff;
                  break;
                  }
               p++;
               }
            }
         }
      else
         {
         p = strstr(szArguments, "/exfat");
         if (!p)
            p = strstr(szArguments, "-exfat");
         if (p)
            {
            // mount all exFAT disks
            f32Parms.fExFat = TRUE;
            exfat_mask = 0xffffffff;
            }
         }
#endif

#if 1
   if (!DosGetInfoSeg(&sGlob, &sLoc))
#else
   if (!DevHelp_GetDOSVar(DHGETDOSV_SYSINFOSEG, 0, (PPVOID)&sGlob))
#endif
      pGI = MAKEPGINFOSEG(sGlob);
   else
      {
      InitMessage("FAT32: Unable to acquire Global Infoseg!");
      _asm pop es;
      return 1;
      }
   pGITicks = &pGI->msecs;
   if (!fSilent)
      InitMessage(szBanner);

   //if (!ulCacheSectors)
   //   InitMessage("FAT32: Warning CACHE size is zero!");
   //else
   //   InitCache(ulCacheSectors);

   // add bootdrive to autocheck mask
   autocheck_mask |= (1UL << (pGI->bootdrive - 1));

   /* disk autocheck */
   autocheck(cmd);

   /* Here we check for a 64-bit file API presence in kernel,
    * and if it is missing, we just disable the large file support 
    * (considering that KEE module is always loaded in Aurora and
    * it is missing in older versions).
    */
   //rc = DosLoadModule(szObjname, sizeof(szObjname), "DOSCALLS", &hmod);
   rc = DosLoadModule(szObjname, sizeof(szObjname), "KEE", &hmod);

   if (rc)
      {
      rc = 0;
      InitMessage("WARNING: Large files support will be disabled.");
      f32Parms.fLargeFiles = FALSE;
      }

   //rc = DosGetProcAddr(hmod, MAKEP(0, 40), &pfn);

   //if (rc || ! pfn)
   //   {
   //   InitMessage("WARNING: Large files support will be disabled.");
   //   rc = 0;
   //   f32Parms.fLargeFiles = FALSE;
   //   goto FS_INITEXIT;
   //   }

   //pKernThunkStackTo16 = (ULONG)pfn;

   //rc = DosGetProcAddr(hmod, MAKEP(0, 41), &pfn);

   //if (rc || ! pfn)
   //   {
   //   InitMessage("WARNING: Large files support will be disabled.");
   //   rc = 0;
   //   f32Parms.fLargeFiles = FALSE;
   //   goto FS_INITEXIT;
   //   }

   //pKernThunkStackTo32 = (ULONG)pfn;

   if (f32Parms.fLargeFiles)
      {
      // Support for files > 2 GB
      FS_ATTRIBUTE |= FSA_LARGEFILE;
      }

#ifdef EXFAT
   //if (f32Parms.fFat || f32Parms.fExFat)
#else
   //if (f32Parms.fFat)
#endif
   //   {
   //   // report itself as "UNIFAT" driver, instead of "FAT32"
   //   // (to avoid confusion, when FAT16 drives are shown as FAT32)
   //   strcpy(FS_NAME, "UNIFAT");
   //   }

//FS_INITEXIT:
   _asm pop es;

   return rc;
}

VOID _cdecl InitMessage(PSZ pszMessage,...)
{
    UCHAR pszBuf[256];
    USHORT usWritten;
    va_list arg_ptr;

    va_start(arg_ptr,pszMessage);
    vsprintf(pszBuf, pszMessage, arg_ptr);
    va_end(arg_ptr);
    strcat(pszBuf,"\r\n");

    DosWrite(1, pszBuf, strlen(pszBuf), &usWritten);

}

/******************************************************************
*
******************************************************************/
int far pascal _loadds FS_IOCTL(
    struct sffsi far * psffsi,      /* psffsi   */
    struct sffsd far * psffsd,      /* psffsd   */
    unsigned short usCat,       /* cat      */
    unsigned short usFunc,      /* func     */
    char far * pParm,           /* pParm    */
    unsigned short cbParm,      /* lenParm  */
    unsigned far * pcbParm,     /* pParmLenInOut */
    char far * pData,           /* pData    */
    unsigned short cbData,      /* lenData  */
    unsigned far * pcbData      /* pDataLenInOut */
)
{
USHORT rc;
PVOLINFO pVolInfo;
ULONG hDEV;
PBIOSPARAMETERBLOCK pBPB;

   _asm push es;

   psffsd = psffsd;

   MessageL(LOG_FS, "FS_IOCTL%m, Cat %Xh, Func %Xh, File# %u",
            0x002b, usCat, usFunc, psffsi->sfi_selfsfn);

   pVolInfo = GetVolInfoSF(psffsd);

   if (! pVolInfo)
      {
      rc = ERROR_INVALID_DRIVE;
      goto FS_IOCTLEXIT;
      }

   rc = 0;

   if (pData && pData != MYNULL)
      {
      if (cbData > 0)
         {
         rc = MY_PROBEBUF(PB_OPWRITE, pData, cbData);
         if (rc)
            {
            Message("Protection VIOLATION in data of FS_IOCTL!");
            goto FS_IOCTLEXIT;
            }
         }

      if (pcbData)
         {
         rc = MY_PROBEBUF(PB_OPWRITE, (PBYTE)pcbData, sizeof (unsigned));
         if (rc)
            pcbData = NULL;
         }

      if (pcbData)
         *pcbData = cbData;
      }
   else
      {
      cbData = 0;
      pcbData = NULL;
      }

   if (pParm && pParm != MYNULL)
      {
      if (cbParm > 0)
         {
         rc = MY_PROBEBUF(PB_OPREAD, pParm, cbParm);
         if (rc)
            {
            Message("Protection VIOLATION in parm of FS_IOCTL, address %lX, len %u!",
               pParm, cbParm);
            goto FS_IOCTLEXIT;
            }
         }
      if (pcbParm)
         {
         rc = MY_PROBEBUF(PB_OPREAD, (PBYTE)pcbParm, sizeof (unsigned));
         if (rc)
            pcbParm = NULL;
         }
      if (pcbParm)
         *pcbParm = cbParm;
      }
   else
      {
      cbParm = 0;
      pcbParm = NULL;
      }

   switch (usCat)
      {
      case IOCTL_DISK    :
         Message("pVolInfo->fLocked=%u, pVolInfo->ulOpenFiles=%lx", pVolInfo->fLocked, pVolInfo->ulOpenFiles);
         switch (usFunc)
            {
            case DSK_LOCKDRIVE:
               if (pVolInfo->fLocked)
                  {
                  Message("Drive locked, pVolInfo->fLocked=%u", pVolInfo->fLocked);
                  rc = ERROR_DRIVE_LOCKED;
                  goto FS_IOCTLEXIT;
                  }
               if (pVolInfo->ulOpenFiles > 1L)
                  {
                  Message("Cannot lock, %lu open files", pVolInfo->ulOpenFiles);
                  rc = ERROR_DRIVE_LOCKED;
                  goto FS_IOCTLEXIT;
                  }

               hDEV = GetVolDevice(pVolInfo);
               if (pVolInfo->hVBP)
                  {
                  rc = FSH_DOVOLIO2(hDEV, psffsi->sfi_selfsfn,
                                    usCat, usFunc, pParm, cbParm, pData, cbData);
                  }
               else
                  {
                  rc = NO_ERROR;
                  }

               if (!rc) {
                 pVolInfo->fLocked = TRUE;

                 if (pcbData) {
                   *pcbData = cbData;
                 }

                 if (pcbParm) {
                   *pcbParm = cbParm;
                 }
               }

               GetProcInfo(&pVolInfo->ProcLocked, sizeof (PROCINFO));
               break;

            case DSK_UNLOCKDRIVE:
               hDEV = GetVolDevice(pVolInfo);
               if (pVolInfo->hVBP)
                  {
                  rc = FSH_DOVOLIO2(hDEV, psffsi->sfi_selfsfn,
                                    usCat, usFunc, pParm, cbParm, pData, cbData);
                  }
               else
                  {
                  rc = NO_ERROR;
                  }

               if (!rc) {

                 if (pcbData) {
                   *pcbData = cbData;
                 }

                 if (pcbParm) {
                   *pcbParm = cbParm;
                 }

                 if (pVolInfo->fLocked)
                    {
                    pVolInfo->fLocked = FALSE;
                    //rc = 0;
                    }
                 //else
                 //   rc = ERROR_INVALID_PARAMETER;
               }
               break;

            case DSK_BLOCKREMOVABLE :
               //if (pcbData)
               //   *pcbData = sizeof (BYTE);

               hDEV = GetVolDevice(pVolInfo);
               if (pVolInfo->hVBP)
                  {
                  rc = FSH_DOVOLIO2(hDEV, psffsi->sfi_selfsfn,
                                    usCat, usFunc, pParm, cbParm, pData, cbData);
                  }
               else
                  {
                  rc = NO_ERROR;
                  }

               if (!rc) {

                 if (pcbData) {
                   *pcbData = cbData;
                 }

                 if (pcbParm) {
                   *pcbParm = cbParm;
                 }
               }

               break;
/*
            case DSK_QUERYMEDIASENSE :
               hDEV = GetVolDevice(pVolInfo);
               rc = FSH_DOVOLIO2(hDEV, psffsi->sfi_selfsfn,
                  usCat, usFunc, pParm, cbParm, pData, cbData);

               if (rc == ERROR_SECTOR_NOT_FOUND)
                  rc = ERROR_NOT_DOS_DISK;
               else if (rc)
                  rc = 0;                                            

                  if (pData)
                     *pData = 0;

               if (!rc) {

                  if (pcbData) {
                     *pcbData = cbData;
                  }

                  if (pcbParm) {
                     *pcbParm = cbParm;
                  }
               }

               break;
 */
            case 0x66 : /* DSK_GETLOCKSTATUS */
               if (pcbData)
                  *pcbData = sizeof (USHORT);
               if (cbData < 2)
                  {
                  rc = ERROR_BUFFER_OVERFLOW;
                  goto FS_IOCTLEXIT;
                  }
               if (pVolInfo->fLocked)
                  *(PWORD)pData = 0x0005;
               else
                  *(PWORD)pData = 0x0006;

               Message("DSK_GETLOCKSTATUS, rc=%u", rc);
               rc = 0;
               break;

            case DSK_READTRACK :
               if (pcbData)
                  *pcbData = sizeof (BIOSPARAMETERBLOCK);
               hDEV = GetVolDevice(pVolInfo);
               if (pVolInfo->hVBP)
                  rc = FSH_DOVOLIO2(hDEV, psffsi->sfi_selfsfn,
                                    usCat, usFunc, pParm, cbParm, pData, cbData);
               else
                  {
                  // image, mounted at the mount point
                  PTRACKLAYOUT ptrk = (PTRACKLAYOUT)pParm;
                  ULONG ulFirstSector;

                  if (! (ptrk->bCommand & 1) )
                     {
                     rc = ERROR_NOT_SUPPORTED;
                     goto FS_IOCTLEXIT;
                     }

                  if (! pidDaemon)
                     return ERROR_INVALID_PROCID;

                  rc = PreSetup();

                  if (rc)
                     {
                     goto FS_IOCTLEXIT;
                     }                    

                  ulFirstSector = (ULONG)ptrk->usCylinder * pVolInfo->BootSect.bpb.SectorsPerTrack * pVolInfo->BootSect.bpb.Heads +
                     (ULONG)ptrk->usHead * pVolInfo->BootSect.bpb.SectorsPerTrack + ptrk->usFirstSector;

                  pCPData->Op = OP_READ;
                  pCPData->hf = pVolInfo->hf;
#ifdef INCL_LONGLONG
                  pCPData->llOffset = (LONGLONG)ulFirstSector * pVolInfo->BootSect.bpb.BytesPerSector;
#else
                  iAssignUL(&pCPData->llOffset, ulFirstSector * pVolInfo->BootSect.bpb.BytesPerSector);
#endif
                  pCPData->cbData = (ULONG)ptrk->cSectors * pVolInfo->BootSect.bpb.BytesPerSector;

                  rc = PostSetup();

                  if (rc)
                     {
                     goto FS_IOCTLEXIT;
                     }                    

                  memcpy(pData, &pCPData->Buf, (USHORT)pCPData->cbData);
                  rc = (USHORT)pCPData->rc;
                  }

               if (!rc)
                  {
                  if (pcbData)
                     {
                     *pcbData = cbData;
                     }

                  if (pcbParm)
                     {
                     *pcbParm = cbParm;
                     }
                  }
               break;

            case DSK_WRITETRACK :
               if (pcbData)
                  *pcbData = sizeof (BIOSPARAMETERBLOCK);
               hDEV = GetVolDevice(pVolInfo);
               if (pVolInfo->hVBP)
                  rc = FSH_DOVOLIO2(hDEV, psffsi->sfi_selfsfn,
                                    usCat, usFunc, pParm, cbParm, pData, cbData);
               else
                  {
                  // image, mounted at the mount point
                  PTRACKLAYOUT ptrk = (PTRACKLAYOUT)pParm;
                  ULONG ulFirstSector;

                  if (! (ptrk->bCommand & 1) )
                     {
                     rc = ERROR_NOT_SUPPORTED;
                     goto FS_IOCTLEXIT;
                     }

                  if (! pidDaemon)
                     return ERROR_INVALID_PROCID;

                  rc = PreSetup();

                  if (rc)
                     {
                     goto FS_IOCTLEXIT;
                     }                    

                  ulFirstSector = (ULONG)ptrk->usCylinder * pVolInfo->BootSect.bpb.SectorsPerTrack * pVolInfo->BootSect.bpb.Heads +
                     (ULONG)ptrk->usHead * pVolInfo->BootSect.bpb.SectorsPerTrack + ptrk->usFirstSector;

                  pCPData->Op = OP_WRITE;
                  pCPData->hf = pVolInfo->hf;
#ifdef INCL_LONGLONG
                  pCPData->llOffset = (LONGLONG)ulFirstSector * pVolInfo->BootSect.bpb.BytesPerSector;
#else
                  iAssignUL(&pCPData->llOffset, ulFirstSector * pVolInfo->BootSect.bpb.BytesPerSector);
#endif
                  pCPData->cbData = (ULONG)ptrk->cSectors * pVolInfo->BootSect.bpb.BytesPerSector;
                  memcpy(&pCPData->Buf, pData, (USHORT)pCPData->cbData);

                  rc = PostSetup();

                  if (rc)
                     {
                     goto FS_IOCTLEXIT;
                     }                    

                  rc = (USHORT)pCPData->rc;
                  }

               if (!rc)
                  {
                  if (pcbData)
                     {
                     *pcbData = cbData;
                     }

                  if (pcbParm)
                     {
                     *pcbParm = cbParm;
                     }
                  }
               break;

            case DSK_GETDEVICEPARAMS :
               if (pcbData)
                  *pcbData = sizeof (BIOSPARAMETERBLOCK);

               hDEV = GetVolDevice(pVolInfo);

               if (pVolInfo->hVBP)
                  rc = FSH_DOVOLIO2(hDEV, psffsi->sfi_selfsfn,
                                    usCat, usFunc, pParm, cbParm, pData, cbData);
               else
                  {
                  ULONG cSectors;
                  PBPB pbpb;

                  if (! pidDaemon)
                     return ERROR_INVALID_PROCID;

                  // image, mounted at the mount point
                  if (*(PULONG)pParm & 1)
                     {
                     // return BPB currently in drive
                     rc = PreSetup();

                     if (rc)
                        {
                        goto FS_IOCTLEXIT;
                        }                    

                     pCPData->Op = OP_READ;
                     pCPData->hf = pVolInfo->hf;
#ifdef INCL_LONGLONG
                     pCPData->llOffset = (LONGLONG)pVolInfo->ullOffset;
#else
                     iAssign(&pCPData->llOffset, *(PLONGLONG)&pVolInfo->ullOffset);
#endif
                     pCPData->cbData = pVolInfo->BootSect.bpb.BytesPerSector;

                     rc = PostSetup();

                     if (rc)
                        {
                        goto FS_IOCTLEXIT;
                        }                    

                     pBPB = (PBIOSPARAMETERBLOCK)pData;

                     pbpb = (PBPB)((PBYTE)pCPData->Buf + 0xb);

                     pBPB->usBytesPerSector = pVolInfo->BootSect.bpb.BytesPerSector;
                     pBPB->bSectorsPerCluster = pVolInfo->BootSect.bpb.SectorsPerCluster;
                     pBPB->usReservedSectors = pVolInfo->BootSect.bpb.ReservedSectors;
                     pBPB->cFATs = pVolInfo->BootSect.bpb.NumberOfFATs;
                     pBPB->cRootEntries = pVolInfo->BootSect.bpb.RootDirEntries;
                     pBPB->cSectors = pVolInfo->BootSect.bpb.TotalSectors;
                     pBPB->bMedia = pVolInfo->BootSect.bpb.MediaDescriptor;
                     pBPB->usSectorsPerFAT = pVolInfo->BootSect.bpb.SectorsPerFat;
                     pBPB->usSectorsPerTrack = pVolInfo->BootSect.bpb.SectorsPerTrack;
                     pBPB->cHeads = pVolInfo->BootSect.bpb.Heads;
                     pBPB->cHiddenSectors = pVolInfo->BootSect.bpb.HiddenSectors;
                     pBPB->cLargeSectors = pVolInfo->BootSect.bpb.BigTotalSectors;

                     cSectors = pBPB->cSectors;

                     if (! cSectors)
                        cSectors = pBPB->cLargeSectors;

                     if (! pBPB->usSectorsPerTrack || ! pBPB->cHeads)
                        {
                        rc = ERROR_SECTOR_NOT_FOUND;
                        goto FS_IOCTLEXIT;
                        }                    

                     pBPB->cCylinders = (USHORT)(cSectors / (pBPB->usSectorsPerTrack * pBPB->cHeads));

                     pBPB->bDeviceType = 1 << 5;  // hard disk
                     pBPB->fsDeviceAttr = 1 << 2; // > 16 MB flag
                     }
                  else
                     {
                     // return recommended BPB
                     ULONG cCylinders;

                     pBPB = (PBIOSPARAMETERBLOCK)pData;

                     memset(pBPB, 0, sizeof(PBIOSPARAMETERBLOCK));
                     pBPB->usBytesPerSector = pVolInfo->BootSect.bpb.BytesPerSector;
                     pBPB->cHiddenSectors = pVolInfo->BootSect.bpb.HiddenSectors;

                     cSectors = pVolInfo->BootSect.bpb.BigTotalSectors;

                     if (cSectors < 65536UL)
                        pBPB->cSectors = (USHORT)cSectors;
                     else
                        pBPB->cLargeSectors = cSectors;

                     pBPB->bMedia = 0xf8;
                     pBPB->usSectorsPerTrack = pVolInfo->BootSect.bpb.SectorsPerTrack;
                     pBPB->cHeads = pVolInfo->BootSect.bpb.Heads;

                     if (! pBPB->usSectorsPerTrack)
                        {
                        pBPB->usSectorsPerTrack = 63;
                        }                    

                     if (! pBPB->cHeads)
                        {
                        pBPB->cHeads = 255;
                        }                    

                     cCylinders = cSectors / (pBPB->usSectorsPerTrack * pBPB->cHeads);

                     if (cCylinders > 0xffff)
                        {
                        pBPB->usSectorsPerTrack = 127;
                        cCylinders = cSectors / (pBPB->usSectorsPerTrack * pBPB->cHeads);
                        }

                     if (cCylinders > 0xffff)
                        {
                        pBPB->usSectorsPerTrack = 255;
                        cCylinders = cSectors / (pBPB->usSectorsPerTrack * pBPB->cHeads);
                        }

                     pBPB->cCylinders = (USHORT)cCylinders;

                     pBPB->bDeviceType = 1 << 5;  // hard disk
                     pBPB->fsDeviceAttr = 1 << 2; // > 16 MB flag
                     }

                  rc = NO_ERROR;
                  }

               if (!rc) {

                  if (pcbData) {
                    *pcbData = sizeof (BIOSPARAMETERBLOCK);
                  }

                  if (pcbParm) {
                    *pcbParm = cbParm;
                  }

                  pBPB = (PBIOSPARAMETERBLOCK)pData;

                  pBPB->bSectorsPerCluster = (BYTE)pVolInfo->SectorsPerCluster;
                  pBPB->usReservedSectors = pVolInfo->BootSect.bpb.ReservedSectors;
                  pBPB->cFATs = pVolInfo->BootSect.bpb.NumberOfFATs;
                  if (pVolInfo->bFatType >= FAT_TYPE_FAT32)
                     pBPB->cRootEntries = (USHORT)(GetChainSize(pVolInfo, NULL, pVolInfo->BootSect.bpb.RootDirStrtClus) * 128); ////
                  else
                     pBPB->cRootEntries = pVolInfo->BootSect.bpb.RootDirEntries;
                  if (pVolInfo->BootSect.bpb.BigSectorsPerFat < 0x10000L)
                     pBPB->usSectorsPerFAT = (USHORT)pVolInfo->BootSect.bpb.BigSectorsPerFat;
                  else
                     pBPB->usSectorsPerFAT = 0xFFFF; // ??? vs!
               }

               break;

            case DSK_SETDEVICEPARAMS :
               {
               #define IODC_SP_MEDIA 2
               typedef struct
                  {
                  BYTE bCmdInfo;
                  BYTE bUnit;
                  } ParamPkt;

               ParamPkt *pkt;

               if (pcbData)
                  *pcbData = sizeof (BIOSPARAMETERBLOCK);
               hDEV = GetVolDevice(pVolInfo);

               pBPB = (PBIOSPARAMETERBLOCK)pData;

               pkt = (ParamPkt *)pParm;

               // flip IODC_SP_MEDIA (2) bit, for kernel not to call SetVPB
               // (otherwise, the kernel will hurt VPB when doing loaddskf
               // on a floppy device)
               pkt->bCmdInfo &= ~IODC_SP_MEDIA;

               if (pVolInfo->hVBP)
                  {
                  rc = FSH_DOVOLIO2(hDEV, psffsi->sfi_selfsfn,
                                    usCat, usFunc, pParm, cbParm, pData, cbData);
                  }
               else
                  {
                  rc = NO_ERROR;
                  }

               if (!rc)
                  {
                  if (pcbData)
                     {
                     *pcbData = sizeof (BIOSPARAMETERBLOCK);
                     }

                  if (pcbParm)
                     {
                     *pcbParm = cbParm;
                     }
                  }
               }
               break;

            default  :
               hDEV = GetVolDevice(pVolInfo);

               if (pVolInfo->hVBP)
                  {
                  rc = FSH_DOVOLIO2(hDEV, psffsi->sfi_selfsfn,
                                    usCat, usFunc, pParm, cbParm, pData, cbData);
                  }
               else
                  {
                  rc = NO_ERROR;
                  }

                if (!rc) {
                 if (pcbData) {
                   *pcbData = cbData;
                 }
  
                 if (pcbParm) {
                   *pcbParm = cbParm;
                 }
               }

               //Message("usFunc=default, cat=%u, func=%u rc=%u", usCat, usFunc, rc);
               break;
            }
         break;

      case IOCTL_FAT32 :
         switch (usFunc)
            {
            case FAT32_SETRASECTORS:
               if (cbParm < sizeof (USHORT))
                  {
                  rc = ERROR_INSUFFICIENT_BUFFER;
                  goto FS_IOCTLEXIT;
                  }
               pVolInfo->usRASectors = *(PUSHORT)pParm;
#if 1
               if (pVolInfo->usRASectors * (pVolInfo->BootSect.bpb.BytesPerSector / 512) > MAX_RASECTORS)
                  pVolInfo->usRASectors = MAX_RASECTORS;
               else
                  pVolInfo->usRASectors *= pVolInfo->BootSect.bpb.BytesPerSector / 512;
#else
               if (pVolInfo->usRASectors > (pVolInfo->ulBlockSize / pVolInfo->BootSect.bpb.BytesPerSector ) * 4)
                  pVolInfo->usRASectors = (pVolInfo->ulBlockSize / pVolInfo->BootSect.bpb.BytesPerSector ) * 4;
#endif
               *(PUSHORT)pParm = pVolInfo->usRASectors;
               Message("usRASectors changed to %u", pVolInfo->usRASectors);
               rc = 0;
               break;

            case FAT32_QUERYRASECTORS:
               if (cbData < sizeof (USHORT))
                  {
                  rc = ERROR_BUFFER_OVERFLOW;
                  goto FS_IOCTLEXIT;
                  }
               *(PUSHORT)pData = pVolInfo->usRASectors;
               rc = 0;
               break;

            case FAT32_GETVOLCLEAN :
               if (cbData < sizeof (BOOL))
                  {
                  rc = ERROR_BUFFER_OVERFLOW;
                  goto FS_IOCTLEXIT;
                  }
               *(PBOOL)pData = pVolInfo->fDiskCleanOnMount;
               if (cbData >= sizeof (BOOL) * 2)
                  *(PBOOL)(pData + 2) = GetDiskStatus(pVolInfo);
               rc = 0;
               break;

            case FAT32_MARKVOLCLEAN:
               Message("IOCTL:Marking volume clean");
               pVolInfo->fDiskCleanOnMount = TRUE;
               rc = 0;
               break;

            case FAT32_FORCEVOLCLEAN:
               if (cbParm < sizeof (BOOL))
                  {
                  rc = ERROR_INSUFFICIENT_BUFFER;
                  goto FS_IOCTLEXIT;
                  }
               Message("IOCTL:Forcing volume clean");
               pVolInfo->fDiskCleanOnMount = *(PBOOL)pParm;
               if (!MarkDiskStatus(pVolInfo, pVolInfo->fDiskCleanOnMount))
                  {
                  rc = ERROR_SECTOR_NOT_FOUND;
                  goto FS_IOCTLEXIT;
                  }
               *(PBOOL)pParm = GetDiskStatus(pVolInfo);
               rc = 0;
               break;

            case FAT32_RECOVERCHAIN :
               if (!pVolInfo->fLocked)
                  {
                  rc = ERROR_BAD_COMMAND;
                  goto FS_IOCTLEXIT;
                  }
               if (cbParm < sizeof (ULONG))
                  {
                  rc = ERROR_INSUFFICIENT_BUFFER;
                  goto FS_IOCTLEXIT;
                  }
               rc = RecoverChain(pVolInfo, *(PULONG)pParm, pData, cbData);
               break;

            case FAT32_DELETECHAIN  :
               if (!pVolInfo->fLocked)
                  {
                  rc = ERROR_BAD_COMMAND;
                  goto FS_IOCTLEXIT;
                  }
               if (cbParm < sizeof (ULONG))
                  {
                  rc = ERROR_INSUFFICIENT_BUFFER;
                  goto FS_IOCTLEXIT;
                  }
               DeleteFatChain(pVolInfo, *(PULONG)pParm);
               rc = 0;
               break;

            case FAT32_GETFREESPACE :
               if (!pVolInfo->fLocked)
                  {
                  rc = ERROR_BAD_COMMAND;
                  goto FS_IOCTLEXIT;
                  }
               if (cbData < sizeof (ULONG))
                  {
                  rc = ERROR_BUFFER_OVERFLOW;
                  goto FS_IOCTLEXIT;
                  }

               *(PULONG)pData = GetFreeSpace(pVolInfo);
               rc = 0;
               break;

            case FAT32_SETFILESIZE:
               if (!pVolInfo->fLocked)
                  {
                  rc = ERROR_BAD_COMMAND;
                  goto FS_IOCTLEXIT;
                  }
               if (cbParm < sizeof (FILESIZEDATA))
                  {
                  rc = ERROR_INSUFFICIENT_BUFFER;
                  goto FS_IOCTLEXIT;
                  }

               rc = SetFileSize(pVolInfo, (PFILESIZEDATA)pParm);
               break;
            case FAT32_QUERYEAS:
            case FAT32_SETEAS  :
               if (usFunc == FAT32_SETEAS && !pVolInfo->fLocked)
                  {
                  rc = ERROR_BAD_COMMAND;
                  goto FS_IOCTLEXIT;
                  }
               if (cbParm < sizeof (MARKFILEEASBUF))
                  {
                  rc = ERROR_INSUFFICIENT_BUFFER;
                  goto FS_IOCTLEXIT;
                  }
               rc = GetSetFileEAS(pVolInfo, usFunc, (PMARKFILEEASBUF)pParm);
               break;

            case FAT32_SETCLUSTER:
               if (cbParm < sizeof (SETCLUSTERDATA))
                  rc = ERROR_INSUFFICIENT_BUFFER;
               else
                  {
                  PSETCLUSTERDATA pSet = (PSETCLUSTERDATA)pParm;

                  if (SetNextCluster(pVolInfo, pSet->ulCluster,
                     pSet->ulNextCluster) != pSet->ulNextCluster)
                     rc = ERROR_SECTOR_NOT_FOUND;
                  else
                     rc = 0;
                  }
               break;
            case FAT32_READBLOCK:
               {
               ULONG ulCluster, ulBlock;
               if (cbParm < 2 * sizeof(ULONG))
                  {
                  rc = ERROR_INSUFFICIENT_BUFFER;
                  goto FS_IOCTLEXIT;
                  }
               if ((ULONG)cbData < pVolInfo->ulClusterSize)
                  {
                  rc = ERROR_BUFFER_OVERFLOW;
                  goto FS_IOCTLEXIT;
                  }
               ulCluster = *(PULONG)pParm;
               ulBlock = *((PULONG)pParm + 1);
               rc = ReadBlock(pVolInfo, ulCluster, ulBlock, pData, 0);
               break;
               }
            case FAT32_READSECTOR:
               {
               PREADSECTORDATA pRSD;
               if (cbParm < sizeof(READSECTORDATA))
                  {
                  rc = ERROR_INSUFFICIENT_BUFFER;
                  goto FS_IOCTLEXIT;
                  }
               pRSD = (PREADSECTORDATA)pParm;
               if ((USHORT)cbData < pRSD->nSectors * pVolInfo->BootSect.bpb.BytesPerSector)
                  {
                  rc = ERROR_BUFFER_OVERFLOW;
                  goto FS_IOCTLEXIT;
                  }
               rc = ReadSector(pVolInfo, pRSD->ulSector, pRSD->nSectors, pData, 0);
               break;
               }
            case FAT32_WRITEBLOCK:
               {
               ULONG ulCluster, ulBlock;
               if (cbParm < 2 * sizeof(ULONG))
                  {
                  rc = ERROR_INSUFFICIENT_BUFFER;
                  goto FS_IOCTLEXIT;
                  }
               if ((ULONG)cbData < pVolInfo->ulClusterSize)
                  {
                  rc = ERROR_INSUFFICIENT_BUFFER;
                  goto FS_IOCTLEXIT;
                  }
               ulCluster = *(PULONG)pParm;
               ulBlock = *((PULONG)pParm + 1);
               rc = WriteBlock(pVolInfo, ulCluster, ulBlock, pData, 0);
               break;
               }
            case FAT32_WRITESECTOR:
               {
               PWRITESECTORDATA pWSD;
               if (cbParm < sizeof(WRITESECTORDATA))
                  {
                  rc = ERROR_INSUFFICIENT_BUFFER;
                  goto FS_IOCTLEXIT;
                  }
               pWSD = (PWRITESECTORDATA)pParm;
               if ((USHORT)cbData < pWSD->nSectors * pVolInfo->BootSect.bpb.BytesPerSector)
                  {
                  rc = ERROR_INSUFFICIENT_BUFFER;
                  goto FS_IOCTLEXIT;
                  }
               rc = WriteSector(pVolInfo, pWSD->ulSector, pWSD->nSectors, pData, 0);
               break;
               }

            case FAT32_BEGINFORMAT :
               {
               // image, mounted at the mount point
               PVOLINFO pVolInfo2;
               POPENINFO pOpenInfo;
               PMNTOPTS opts;

               opts = (PMNTOPTS)malloc(sizeof(MNTOPTS));

               if (! opts)
                  {
                  rc = ERROR_NOT_ENOUGH_MEMORY;
                  goto FS_IOCTLEXIT;
                  }

               pOpenInfo = *(POPENINFO *)psffsd;

               if (! pOpenInfo)
                  {
                  rc = ERROR_VOLUME_NOT_MOUNTED;
                  free(opts);
                  goto FS_IOCTLEXIT;
                  }

               opts->usOp = MOUNT_ACCEPT;
               strcpy(opts->pFilename, pVolInfo->pbFilename);
               strcpy(opts->pMntPoint, pVolInfo->pbMntPoint);
               opts->ullOffset = pVolInfo->ullOffset;
               opts->hf = pVolInfo->hf;

               rc = Mount2(opts, &pVolInfo2);

               if (rc)
                  {
                  free(opts);
                  goto FS_IOCTLEXIT;
                  }

               pVolInfo2->fLocked = pVolInfo->fLocked;
               pVolInfo2->ulOpenFiles = pVolInfo->ulOpenFiles;
               pVolInfo2->BootSect.bpb.HiddenSectors = pVolInfo->BootSect.bpb.HiddenSectors;
               pVolInfo2->BootSect.bpb.BigTotalSectors = pVolInfo->BootSect.bpb.BigTotalSectors;
               pVolInfo2->BootSect.bpb.TotalSectors = pVolInfo->BootSect.bpb.TotalSectors;
               pVolInfo2->BootSect.bpb.BytesPerSector = pVolInfo->BootSect.bpb.BytesPerSector;
               pVolInfo2->BootSect.bpb.SectorsPerTrack = pVolInfo->BootSect.bpb.SectorsPerTrack;
               pVolInfo2->BootSect.bpb.Heads = pVolInfo->BootSect.bpb.Heads;
               pOpenInfo->pVolInfo = pVolInfo2;

               opts->usOp = MOUNT_RELEASE;
               strcpy(opts->pFilename, pVolInfo->pbFilename);
               strcpy(opts->pMntPoint, pVolInfo->pbMntPoint);
               opts->ullOffset = pVolInfo->ullOffset;
               opts->hf = pVolInfo->hf;

               rc = Mount2(opts, &pVolInfo);

               free(opts);

               if (!rc)
                  {
                  if (pcbData)
                     {
                     *pcbData = cbData;
                     }

                  if (pcbParm)
                     {
                     *pcbParm = cbParm;
                     }
                  }
               else
                  {
                  pOpenInfo->pVolInfo = NULL;
                  }
               }
               break;

            case FAT32_REDETERMINEMEDIA :
               {
               // image, mounted at the mount point
               PVOLINFO pVolInfo2;
               POPENINFO pOpenInfo;
               PMNTOPTS opts;

               opts = (PMNTOPTS)malloc(sizeof(MNTOPTS));

               if (! opts)
                  {
                  rc = ERROR_NOT_ENOUGH_MEMORY;
                  goto FS_IOCTLEXIT;
                  }

               pOpenInfo = *(POPENINFO *)psffsd;

               if (! pOpenInfo)
                  {
                  rc = ERROR_VOLUME_NOT_MOUNTED;
                  free(opts);
                  goto FS_IOCTLEXIT;
                  }

               opts->usOp = MOUNT_MOUNT;
               strcpy(opts->pFilename, pVolInfo->pbFilename);
               strcpy(opts->pMntPoint, pVolInfo->pbMntPoint);
               opts->ullOffset = pVolInfo->ullOffset;
               opts->hf = pVolInfo->hf;

               rc = Mount2(opts, &pVolInfo2);

               if (rc)
                  {
                  free(opts);
                  goto FS_IOCTLEXIT;
                  }

               pVolInfo2->fLocked = pVolInfo->fLocked;
               pVolInfo2->ulOpenFiles = pVolInfo->ulOpenFiles;
               pVolInfo2->BootSect.bpb.HiddenSectors = pVolInfo->BootSect.bpb.HiddenSectors;
               pVolInfo2->BootSect.bpb.BigTotalSectors = pVolInfo->BootSect.bpb.BigTotalSectors;
               pVolInfo2->BootSect.bpb.TotalSectors = pVolInfo->BootSect.bpb.TotalSectors;
               pVolInfo2->BootSect.bpb.BytesPerSector = pVolInfo->BootSect.bpb.BytesPerSector;
               pVolInfo2->BootSect.bpb.SectorsPerTrack = pVolInfo->BootSect.bpb.SectorsPerTrack;
               pVolInfo2->BootSect.bpb.Heads = pVolInfo->BootSect.bpb.Heads;
               pOpenInfo->pVolInfo = pVolInfo2;

               opts->usOp = MOUNT_RELEASE;
               strcpy(opts->pFilename, pVolInfo->pbFilename);
               strcpy(opts->pMntPoint, pVolInfo->pbMntPoint);
               opts->ullOffset = pVolInfo->ullOffset;
               opts->hf = pVolInfo->hf;

               rc = Mount2(opts, &pVolInfo);

               free(opts);

               if (!rc)
                  {
                  if (pcbData)
                     {
                     *pcbData = cbData;
                     }

                  if (pcbParm)
                     {
                     *pcbParm = cbParm;
                     }
                  }
               else
                  {
                  pOpenInfo->pVolInfo = NULL;
                  }
               }
               break;

            default :
               rc = ERROR_BAD_COMMAND;
               break;
            }
         break;

      default:
         hDEV = GetVolDevice(pVolInfo);

         if (pVolInfo->hVBP)
            {
            rc = FSH_DOVOLIO2(hDEV, psffsi->sfi_selfsfn,
                              usCat, usFunc, pParm, cbParm, pData, cbData);
            }
         else
            {
            rc = NO_ERROR;
            }

         if (!rc)
            {
            if (pcbData)
               {
               *pcbData = cbData;
               }

            if (pcbParm)
               {
               *pcbParm = cbParm;
               }
            }
         break;
      }

FS_IOCTLEXIT:
   MessageL(LOG_FS, "FS_IOCTL%m returned %u", 0x802b, rc);

   _asm pop es;

   return rc;
}


USHORT GetSetFileEAS(PVOLINFO pVolInfo, USHORT usFunc, PMARKFILEEASBUF pMark)
{
ULONG ulDirCluster;
PDIRENTRY1 pDirStream = NULL;
PSHOPENINFO pDirSHInfo = NULL;
PSZ   pszFile;

   MessageL(LOG_FUNCS, "GetSetFileEAS%m", 0x036);

#ifdef EXFAT
   pDirStream = (PDIRENTRY1)malloc((size_t)sizeof(DIRENTRY1));
   if (!pDirStream)
      {
      return ERROR_NOT_ENOUGH_MEMORY;
      }
#endif

   ulDirCluster = FindDirCluster(pVolInfo,
      NULL,
      NULL,
      pMark->szFileName,
      0xFFFF,
      RETURN_PARENT_DIR,
      &pszFile,
      pDirStream);

   if (ulDirCluster == pVolInfo->ulFatEof)
      {
#ifdef EXFAT
      free(pDirStream);
#endif
      return ERROR_PATH_NOT_FOUND;
      }

#ifdef EXFAT
   if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
      {
      SetSHInfo1(pVolInfo, pDirStream, pDirSHInfo);
      }
#endif

   if (usFunc == FAT32_QUERYEAS)
      {
      ULONG ulCluster;
      PDIRENTRY pDirEntry;
#ifdef EXFAT
      PDIRENTRY1 pDirEntry1;
#endif

      pDirEntry = (PDIRENTRY)malloc((size_t)sizeof(DIRENTRY));
      if (!pDirEntry)
         {
#ifdef EXFAT
         free(pDirStream);
#endif
         return ERROR_NOT_ENOUGH_MEMORY;
         }
#ifdef EXFAT
      pDirEntry1 = (PDIRENTRY1)pDirEntry;
#endif

      ulCluster = FindPathCluster(pVolInfo, ulDirCluster, pszFile, pDirSHInfo, pDirEntry, NULL, NULL);
      if (ulCluster == pVolInfo->ulFatEof)
         {
         free(pDirEntry);
#ifdef EXFAT
         free(pDirStream);
#endif
         return ERROR_FILE_NOT_FOUND;
         }
#ifdef EXFAT
      if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
#endif
         pMark->fEAS = pDirEntry->fEAS;
#ifdef EXFAT
      else
         pMark->fEAS = pDirEntry1->u.File.fEAS;
#endif
      free(pDirEntry);
#ifdef EXFAT
      free(pDirStream);
#endif
      return 0;
      }

#ifdef EXFAT
   free(pDirStream);
#endif
   return MarkFileEAS(pVolInfo, ulDirCluster, pDirSHInfo, pszFile, pMark->fEAS);
}

USHORT RecoverChain(PVOLINFO pVolInfo, ULONG ulCluster, PBYTE pData, USHORT cbData)
{
PDIRENTRY pDirEntry;
PDIRENTRY1 pDirStream = NULL;
BYTE     szFileName[14];
USHORT   usNr;

      pDirEntry = (PDIRENTRY)malloc((size_t)sizeof(DIRENTRY));
      if (!pDirEntry)
         {
         return ERROR_NOT_ENOUGH_MEMORY;
         }
#ifdef EXFAT
      pDirStream = (PDIRENTRY1)malloc((size_t)sizeof(DIRENTRY1));
      if (!pDirStream)
         {
         free(pDirEntry);
         return ERROR_NOT_ENOUGH_MEMORY;
         }
#endif

   memset(pDirEntry, 0, sizeof (DIRENTRY));
#ifdef EXFAT
   memset(pDirStream, 0, sizeof (DIRENTRY1));
#endif

#ifdef EXFAT
   if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
#endif
      {
      memcpy(pDirEntry->bFileName, "FILE0000CHK", 11);
      strcpy(szFileName, "FILE0000.CHK");
      }
   for (usNr = 0; usNr < 9999; usNr++)
      {
      USHORT iPos = 7;
      USHORT usNum = usNr;

         while (usNum)
            {
            szFileName[iPos] = (BYTE)((usNum % 10) + '0');
            usNum /= 10;
            iPos--;
            }

         if (FindPathCluster(pVolInfo, pVolInfo->BootSect.bpb.RootDirStrtClus,
            szFileName, NULL, NULL, NULL, NULL) == pVolInfo->ulFatEof)
            break;
      }
   if (usNr == 9999)
      {
      free(pDirEntry);
#ifdef EXFAT
      free(pDirStream);
#endif
      return ERROR_FILE_EXISTS;
      }
#ifdef EXFAT
   if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
#endif
      memcpy(pDirEntry->bFileName, szFileName, 8);
   if (pData)
      strncpy(pData, szFileName, cbData);

#ifdef EXFAT
   if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
      {
#endif
      pDirEntry->wCluster = LOUSHORT(ulCluster);
      pDirEntry->wClusterHigh = HIUSHORT(ulCluster);
#ifdef EXFAT
      }
   else
      {
      pDirStream->u.Stream.ulFirstClus = ulCluster;
      }
#endif
   while (ulCluster != pVolInfo->ulFatEof)
      {
      ULONG ulNextCluster;
#ifdef EXFAT
      if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
#endif
         {
         ULONGLONG ullSize;
         FileGetSize(pVolInfo, pDirEntry, pVolInfo->BootSect.bpb.RootDirStrtClus, NULL, szFileName, &ullSize);
#ifdef INCL_LONGLONG
         ullSize += pVolInfo->ulClusterSize;
#else
         AssignUL(&ullSize, pVolInfo->ulClusterSize);
#endif
         FileSetSize(pVolInfo, pDirEntry, pVolInfo->BootSect.bpb.RootDirStrtClus, NULL, szFileName, ullSize);
         }
#ifdef EXFAT
      else
         {
#ifdef INCL_LONGLONG
         pDirStream->u.Stream.ullValidDataLen += pVolInfo->ulClusterSize;
         pDirStream->u.Stream.ullDataLen += pVolInfo->ulClusterSize;
#else
         pDirStream->u.Stream.ullValidDataLen = AddUL(pDirStream->u.Stream.ullValidDataLen, pVolInfo->ulClusterSize);
         pDirStream->u.Stream.ullDataLen = AddUL(pDirStream->u.Stream.ullDataLen, pVolInfo->ulClusterSize);
#endif
         }
#endif
      ulNextCluster = GetNextCluster(pVolInfo, NULL, ulCluster);
      if (!ulNextCluster)
         {
         SetNextCluster(pVolInfo, ulCluster, pVolInfo->ulFatEof);
         ulCluster = pVolInfo->ulFatEof;
         }
      else
         ulCluster = ulNextCluster;
      }

   free(pDirEntry);
#ifdef EXFAT
   free(pDirStream);
#endif

   return MakeDirEntry(pVolInfo,
      pVolInfo->BootSect.bpb.RootDirStrtClus, NULL,
      pDirEntry, pDirStream, szFileName);
}

USHORT SetFileSize(PVOLINFO pVolInfo, PFILESIZEDATA pFileSize)
{
ULONG ulDirCluster;
PSZ   pszFile;
ULONG ulCluster;
PDIRENTRY pDirEntry;
PDIRENTRY pDirNew;
PDIRENTRY1 pDirStream = NULL;
PDIRENTRY1 pDirStreamNew = NULL;
PDIRENTRY1 pDirStreamEntry = NULL;
ULONG ulClustersNeeded;
ULONG ulClustersUsed;
PSHOPENINFO pSHInfo = NULL;
PSHOPENINFO pDirSHInfo = NULL;
USHORT rc;

   pDirEntry = (PDIRENTRY)malloc((size_t)sizeof(DIRENTRY));
   if (!pDirEntry)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto SetFileSizeExit;
      }
   pDirNew = (PDIRENTRY)malloc((size_t)sizeof(DIRENTRY));
   if (!pDirNew)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto SetFileSizeExit;
      }
#ifdef EXFAT
   pDirStream = (PDIRENTRY1)malloc((size_t)sizeof(DIRENTRY1));
   if (!pDirStream)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto SetFileSizeExit;
      }
   pDirStreamNew = (PDIRENTRY1)malloc((size_t)sizeof(DIRENTRY1));
   if (!pDirStreamNew)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto SetFileSizeExit;
      }
   pDirStreamEntry = (PDIRENTRY1)malloc((size_t)sizeof(DIRENTRY1));
   if (!pDirStreamEntry)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto SetFileSizeExit;
      }
   pSHInfo = (PSHOPENINFO)malloc((size_t)sizeof(SHOPENINFO));
   if (!pSHInfo)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto SetFileSizeExit;
      }
   pDirSHInfo = (PSHOPENINFO)malloc((size_t)sizeof(SHOPENINFO));
   if (!pDirSHInfo)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto SetFileSizeExit;
      }
#endif

   ulDirCluster = FindDirCluster(pVolInfo,
      NULL,
      NULL,
      pFileSize->szFileName,
      0xFFFF,
      RETURN_PARENT_DIR,
      &pszFile,
      pDirStreamEntry);

   if (ulDirCluster == pVolInfo->ulFatEof)
      return ERROR_PATH_NOT_FOUND;

#ifdef EXFAT
   if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
      {
      SetSHInfo1(pVolInfo, pDirStreamEntry, pDirSHInfo);
      }
#endif

   ulCluster = FindPathCluster(pVolInfo, ulDirCluster, pszFile,
      pDirSHInfo, pDirEntry, pDirStream, NULL);
   if (ulCluster == pVolInfo->ulFatEof)
      return ERROR_FILE_NOT_FOUND;
   if (!ulCluster)
#ifdef INCL_LONGLONG
      pFileSize->ullFileSize = 0L;
#else
      AssignUL(&pFileSize->ullFileSize, 0);
#endif

#ifdef INCL_LONGLONG
   ulClustersNeeded = pFileSize->ullFileSize / pVolInfo->ulClusterSize;

   if (pFileSize->ullFileSize % pVolInfo->ulClusterSize)
      ulClustersNeeded++;
#else
   {
   ULONGLONG ullFileSize;
   Assign(&ullFileSize, pFileSize->ullFileSize);
   DivUL(ullFileSize, pVolInfo->ulClusterSize);
   ulClustersNeeded = ullFileSize.ulLo;

   Assign(&ullFileSize, pFileSize->ullFileSize);
   ModUL(ullFileSize, pVolInfo->ulClusterSize);

   if (ullFileSize.ulLo)
      ulClustersNeeded++;
   }
#endif

#ifdef EXFAT
   if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
      {
      SetSHInfo1(pVolInfo, pDirStream, pSHInfo);
      }
#endif

#ifdef INCL_LONGLONG
   if (pFileSize->ullFileSize > 0)
#else
   if (GreaterUL(pFileSize->ullFileSize, 0))
#endif
      {
      ulClustersUsed = 1;
      while (ulClustersUsed < ulClustersNeeded)
         {
         ULONG ulNextCluster = GetNextCluster(pVolInfo, pSHInfo, ulCluster);
         if (!ulNextCluster)
            break;
         ulCluster = ulNextCluster;
         if (ulCluster == pVolInfo->ulFatEof)
            break;
         ulClustersUsed++;
         }
      if (ulCluster == pVolInfo->ulFatEof)
#ifdef INCL_LONGLONG
         pFileSize->ullFileSize = ulClustersUsed * pVolInfo->ulClusterSize;
#else
         {
         AssignUL(&pFileSize->ullFileSize, ulClustersUsed);
         pFileSize->ullFileSize = MulUL(pFileSize->ullFileSize, pVolInfo->ulClusterSize);
         }
#endif
      else
         SetNextCluster(pVolInfo, ulCluster, pVolInfo->ulFatEof);
      }

   memcpy(pDirNew, pDirEntry, sizeof (DIRENTRY));
#ifdef EXFAT
   memcpy(pDirStreamNew, pDirStream, sizeof (DIRENTRY1));

   if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
      {
#endif
      FileSetSize(pVolInfo, pDirNew, ulDirCluster, pDirSHInfo, pszFile, pFileSize->ullFileSize);
#ifdef EXFAT
      }
   else
      {
#ifdef INCL_LONGLONG
      pDirStreamNew->u.Stream.ullValidDataLen = pFileSize->ullFileSize;
      pDirStreamNew->u.Stream.ullDataLen = pDirStreamNew->u.Stream.ullValidDataLen;
      //   (pFileSize->ullFileSize / pVolInfo->ulClusterSize) * pVolInfo->ulClusterSize +
      //   ((pFileSize->ullFileSize % pVolInfo->ulClusterSize) ? pVolInfo->ulClusterSize : 0);
#else
      Assign(&pDirStreamNew->u.Stream.ullValidDataLen, pFileSize->ullFileSize);
      Assign(&pDirStreamNew->u.Stream.ullDataLen, pDirStreamNew->u.Stream.ullValidDataLen);
      //   (pFileSize->ullFileSize / pVolInfo->ulClusterSize) * pVolInfo->ulClusterSize +
      //   ((pFileSize->ullFileSize % pVolInfo->ulClusterSize) ? pVolInfo->ulClusterSize : 0));
#endif
      }
#endif

#ifdef INCL_LONGLONG
   if (!pFileSize->ullFileSize)
#else
   if (EqUL(pFileSize->ullFileSize, 0))
#endif
      {
#ifdef EXFAT
      if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
         {
#endif
         pDirNew->wCluster = 0;
         pDirNew->wClusterHigh = 0;
#ifdef EXFAT
         }
      else
         {
         pDirStreamNew->u.Stream.ulFirstClus = 0;
         }
#endif
      }

   rc = ModifyDirectory(pVolInfo, ulDirCluster, pDirSHInfo, MODIFY_DIR_UPDATE,
      pDirEntry, pDirNew, pDirStream, pDirStreamNew, pszFile, pszFile, 0);
   //rc = ModifyDirectory(pVolInfo, ulDirCluster, pDirSHInfo, MODIFY_DIR_UPDATE,
   //   pDirEntry, pDirNew, pDirStream, pDirStreamNew, NULL, 0);

SetFileSizeExit:
   if (pDirEntry)
      free(pDirEntry);
   if (pDirNew)
      free(pDirNew);
#ifdef EXFAT
   if (pDirStream)
      free(pDirStream);
   if (pDirStreamNew)
      free(pDirStreamNew);
   if (pDirStreamEntry)
      free(pDirStreamEntry);
   if (pSHInfo)
      free(pSHInfo);
   if (pDirSHInfo)
      free(pDirSHInfo);
#endif

   return rc;
}


/******************************************************************
*
******************************************************************/
int far pascal _loadds FS_MOVE(
    struct cdfsi far * pcdfsi,      /* pcdfsi   */
    struct cdfsd far * pcdfsd,      /* pcdfsd   */
    char far * pSrc,            /* pSrc     */
    unsigned short usSrcCurDirEnd,      /* iSrcCurDirEnd*/
    char far * pDst,            /* pDst     */
    unsigned short usDstCurDirEnd,      /* iDstCurDirEnd*/
    unsigned short usFlags      /* flags    */
)
{
PVOLINFO pVolInfo;
ULONG ulSrcDirCluster;
ULONG ulDstDirCluster;
PSZ   pszSrcFile;
PSZ   pszDstFile;
PSZ   pszSrc, pszDst;
PDIRENTRY pDirEntry = NULL;
PDIRENTRY1 pDirEntry1;
PDIRENTRY1 pDirEntryStream = NULL, pDirEntryStreamOld = NULL;
ULONG    ulCluster;
ULONG    ulTarCluster;
USHORT   rc;
POPENINFO pOISrc = NULL;
POPENINFO pOIDst = NULL;
PDIRENTRY1 pDirSrcStream = NULL;
PDIRENTRY1 pDirDstStream = NULL;
PSHOPENINFO pDirSrcSHInfo = NULL, pDirDstSHInfo = NULL;
//BYTE     szSrcLongName[ FAT32MAXPATH ];
PSZ      szSrcLongName = NULL;
//BYTE     szDstLongName[ FAT32MAXPATH ];
PSZ      szDstLongName = NULL;

   _asm push es;

   MessageL(LOG_FS, "FS_MOVE%m %s to %s", 0x002c, pSrc, pDst);

   pVolInfo = GetVolInfoX(pSrc);

   if (pVolInfo != GetVolInfoX(pDst))
      {
      rc = ERROR_CANNOT_COPY;
      goto FS_MOVEEXIT;
      }

   if (! pVolInfo)
      {
      pVolInfo = GetVolInfo(pcdfsi->cdi_hVPB);
      }

   if (! pVolInfo)
      {
      rc = ERROR_INVALID_DRIVE;
      goto FS_MOVEEXIT;
      }

   if (pVolInfo->fFormatInProgress)
      {
      rc = ERROR_ACCESS_DENIED;
      goto FS_MOVEEXIT;
      }

   if (IsDriveLocked(pVolInfo))
      {
      rc = ERROR_DRIVE_LOCKED;
      goto FS_MOVEEXIT;
      }
   if (!pVolInfo->fDiskCleanOnMount)
      {
      rc = ERROR_VOLUME_DIRTY;
      goto FS_MOVEEXIT;
      }
   if (pVolInfo->fWriteProtected)
      {
      rc = ERROR_WRITE_PROTECT;
      goto FS_MOVEEXIT;
      }

   pDirEntry = (PDIRENTRY)malloc((size_t)sizeof(DIRENTRY));
   if (!pDirEntry)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto FS_MOVEEXIT;
      }
#ifdef EXFAT
   pDirEntry1 = (PDIRENTRY1)pDirEntry;

   pDirEntryStream = (PDIRENTRY1)malloc((size_t)sizeof(DIRENTRY1));
   if (!pDirEntryStream)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto FS_MOVEEXIT;
      }
   pDirEntryStreamOld = (PDIRENTRY1)malloc((size_t)sizeof(DIRENTRY1));
   if (!pDirEntryStreamOld)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto FS_MOVEEXIT;
      }
   pDirSrcStream = (PDIRENTRY1)malloc((size_t)sizeof(DIRENTRY1));
   if (!pDirSrcStream)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto FS_MOVEEXIT;
      }
   pDirDstStream = (PDIRENTRY1)malloc((size_t)sizeof(DIRENTRY1));
   if (!pDirDstStream)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto FS_MOVEEXIT;
      }
   pDirSrcSHInfo = (PSHOPENINFO)malloc((size_t)sizeof(SHOPENINFO));
   if (!pDirSrcSHInfo)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto FS_MOVEEXIT;
      }
   pDirDstSHInfo = (PSHOPENINFO)malloc((size_t)sizeof(SHOPENINFO));
   if (!pDirDstSHInfo)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto FS_MOVEEXIT;
      }
#endif

   szSrcLongName = (PSZ)malloc((size_t)FAT32MAXPATH);
   if (!szSrcLongName)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto FS_MOVEEXIT;
      }

   szDstLongName = (PSZ)malloc((size_t)FAT32MAXPATH);
   if (!szDstLongName)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto FS_MOVEEXIT;
      }

   if( TranslateName(pVolInfo, 0L, NULL, pSrc, szSrcLongName, TRANSLATE_SHORT_TO_LONG ))
      strcpy( szSrcLongName, pSrc );

   if( TranslateName(pVolInfo, 0L, NULL, pDst, szDstLongName, TRANSLATE_SHORT_TO_LONG ))
      strcpy( szDstLongName, pDst );

#ifdef EXFAT
   if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
      {
      pszSrc = szSrcLongName;
      pszDst = szDstLongName;
      }
   else
#endif
      {
      pszSrc = pSrc;
      pszDst = pDst;
      }

   if (usSrcCurDirEnd == (USHORT)(strrchr(pSrc, '\\') - pSrc + 1))
      {
      usSrcCurDirEnd = strrchr(pszSrc, '\\') - pszSrc + 1;
      }

   if (usDstCurDirEnd == (USHORT)(strrchr(pDst, '\\') - pDst + 1))
      {
      usDstCurDirEnd = strrchr(pszDst, '\\') - pszDst + 1;
      }

   pOISrc = malloc(sizeof (OPENINFO));
   if (!pOISrc)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto FS_MOVEEXIT;
      }

   pOISrc->pSHInfo = GetSH( szSrcLongName, pOISrc);
   if (!pOISrc->pSHInfo)
      {
      rc = ERROR_TOO_MANY_OPEN_FILES;
      goto FS_MOVEEXIT;
      }
   //pOISrc->pSHInfo->sOpenCount++; //
   if (pOISrc->pSHInfo->sOpenCount > 1)
      {
      rc = ERROR_ACCESS_DENIED;
      goto FS_MOVEEXIT;
      }
   pOISrc->pSHInfo->fLock = TRUE;

   if( stricmp( szSrcLongName, szDstLongName ))
   {
        pOIDst = malloc(sizeof (OPENINFO));
        if (!pOIDst)
        {
            rc = ERROR_NOT_ENOUGH_MEMORY;
            goto FS_MOVEEXIT;
        }

        pOIDst->pSHInfo = GetSH( szDstLongName, pOIDst);
        if (!pOIDst->pSHInfo)
        {
            rc = ERROR_TOO_MANY_OPEN_FILES;
            goto FS_MOVEEXIT;
        }
        //pOIDst->pSHInfo->sOpenCount++; //
        if (pOIDst->pSHInfo->sOpenCount > 1)
        {
            rc = ERROR_ACCESS_DENIED;
            goto FS_MOVEEXIT;
        }
        pOIDst->pSHInfo->fLock = TRUE;
   }


   /*
      Check destination
   */
   ulDstDirCluster = FindDirCluster(pVolInfo,
      pcdfsi,
      pcdfsd,
      pszDst,
      usDstCurDirEnd,
      RETURN_PARENT_DIR,
      &pszDstFile,
      pDirDstStream);

   if (ulDstDirCluster == pVolInfo->ulFatEof)
      {
      rc = ERROR_PATH_NOT_FOUND;
      goto FS_MOVEEXIT;
      }

#ifdef EXFAT
   if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
      {
      SetSHInfo1(pVolInfo, pDirDstStream, pDirDstSHInfo);
      }
#endif

   ulTarCluster = FindPathCluster(pVolInfo, ulDstDirCluster, pszDstFile, pDirDstSHInfo, pDirEntry, NULL, NULL);

   /*
      Check source
   */
   ulSrcDirCluster = FindDirCluster(pVolInfo,
      pcdfsi,
      pcdfsd,
      pszSrc,
      usSrcCurDirEnd,
      RETURN_PARENT_DIR,
      &pszSrcFile,
      pDirSrcStream);

   if (ulSrcDirCluster == pVolInfo->ulFatEof)
      {
      rc = ERROR_PATH_NOT_FOUND;
      goto FS_MOVEEXIT;
      }

#ifdef EXFAT
   if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
      {
      SetSHInfo1(pVolInfo, pDirSrcStream, pDirSrcSHInfo);
      }
#endif

   ulCluster = FindPathCluster(pVolInfo, ulSrcDirCluster, pszSrcFile, pDirSrcSHInfo, pDirEntry, pDirEntryStream, NULL);
   if (ulCluster == pVolInfo->ulFatEof)
      {
      rc = ERROR_FILE_NOT_FOUND; //////
      goto FS_MOVEEXIT;
      }

   if (!(ulTarCluster == pVolInfo->ulFatEof || ulTarCluster == ulCluster))
      {
      rc = ERROR_ACCESS_DENIED;
      goto FS_MOVEEXIT;
      }

   if (ulCluster == pVolInfo->BootSect.bpb.RootDirStrtClus)
      {
      rc = ERROR_ACCESS_DENIED;
      goto FS_MOVEEXIT;
      }

#if 0
   if (pDirEntry->bAttr & FILE_READONLY)
      {
      rc = ERROR_ACCESS_DENIED;
      goto FS_MOVEEXIT;
      }
#endif

#ifdef EXFAT
   if ( ((pVolInfo->bFatType <  FAT_TYPE_EXFAT) && (pDirEntry->bAttr & FILE_DIRECTORY)) ||
        ((pVolInfo->bFatType == FAT_TYPE_EXFAT) && (pDirEntry1->u.File.usFileAttr & FILE_DIRECTORY)) )
#else
   if ( pDirEntry->bAttr & FILE_DIRECTORY )
#endif
      {
#if 1
      int iLen = strlen( szSrcLongName );

      if ( !strnicmp(szSrcLongName, szDstLongName, iLen ) &&
           ( szDstLongName[ iLen ] == '\\' ))
      {
        rc = ERROR_CIRCULARITY_REQUESTED;
        goto FS_MOVEEXIT;
      }

      ReleaseSH( pOISrc );
      pOISrc = NULL;

      rc = MY_ISCURDIRPREFIX( szSrcLongName );
      if( rc )
        goto FS_MOVEEXIT;

      pOISrc = malloc(sizeof (OPENINFO));
      if (!pOISrc)
      {
        rc = ERROR_NOT_ENOUGH_MEMORY;
        goto FS_MOVEEXIT;
      }

      pOISrc->pSHInfo = GetSH( szSrcLongName, pOISrc);
      if (!pOISrc->pSHInfo)
      {
        rc = ERROR_TOO_MANY_OPEN_FILES;
        goto FS_MOVEEXIT;
      }

      //pOISrc->pSHInfo->sOpenCount++; //
      if (pOISrc->pSHInfo->sOpenCount > 1)
      {
        rc = ERROR_ACCESS_DENIED;
        goto FS_MOVEEXIT;
      }

      pOISrc->pSHInfo->fLock = TRUE;
#else
      //BYTE szName[FAT32MAXPATH];
      PSZ szName = (PSZ)malloc((size_t)FAT32MAXPATH);
      if (!szName)
         {
         rc = ERROR_NOT_ENOUGH_MEMORY;
         goto FS_MOVEEXIT;
         }
      rc = FSH_ISCURDIRPREFIX(pSrc);
      if (rc)
         {
         free(szName);
         goto FS_MOVEEXIT;
         }
      rc = TranslateName(pVolInfo, 0L, NULL, pSrc, szName, TRANSLATE_AUTO);
      if (rc)
         {
         free(szName);
         goto FS_MOVEEXIT;
         }
      rc = FSH_ISCURDIRPREFIX(szName);
      if (rc)
         {
         free(szName);
         goto FS_MOVEEXIT;
         }
      if (szName)
         free(szName);
#endif
      }

   /*
        rename EA file
   */

   if (f32Parms.fEAS)
      {
      rc = usMoveEAS(pVolInfo, ulSrcDirCluster, pszSrcFile, pDirSrcSHInfo,
                               ulDstDirCluster, pszDstFile, pDirDstSHInfo);
      if( rc )
         goto FS_MOVEEXIT;
      }

   if (ulSrcDirCluster == ulDstDirCluster)
      {
      PDIRENTRY pDirOld;

      pDirOld = (PDIRENTRY)malloc((size_t)sizeof(DIRENTRY));
      if (!pDirOld)
         {
         rc = ERROR_NOT_ENOUGH_MEMORY;
         goto FS_MOVEEXIT;
         }

      memcpy(pDirOld, pDirEntry, sizeof(DIRENTRY));
#ifdef EXFAT
      memcpy(pDirEntryStreamOld, pDirEntryStream, sizeof(DIRENTRY1));
#endif

      rc = ModifyDirectory(pVolInfo, ulSrcDirCluster, pDirSrcSHInfo,
         MODIFY_DIR_RENAME, pDirOld, pDirEntry, pDirEntryStreamOld, pDirEntryStream, pszSrcFile, pszDstFile, 0);
      //rc = ModifyDirectory(pVolInfo, ulSrcDirCluster, pDirSrcSHInfo,
      //   MODIFY_DIR_RENAME, pDirOld, pDirEntry, pDirEntryStream, pDirEntryStreamOld, pszDstFile, 0);
      free(pDirOld);
      goto FS_MOVEEXIT;
      }

   /*
      First delete old
   */

   rc = ModifyDirectory(pVolInfo, ulSrcDirCluster, pDirSrcSHInfo, MODIFY_DIR_DELETE, pDirEntry, NULL, pDirEntryStream, NULL, pszSrcFile, pszSrcFile, 0); // was pszDstFile (originally, NULL)
   //rc = ModifyDirectory(pVolInfo, ulSrcDirCluster, pDirSrcSHInfo, MODIFY_DIR_DELETE, pDirEntry, NULL, pDirEntryStream, NULL, NULL, 0);
   if (rc)
      goto FS_MOVEEXIT;

   /*
      Then insert new
   */

   rc = ModifyDirectory(pVolInfo, ulDstDirCluster, pDirDstSHInfo, MODIFY_DIR_INSERT, NULL, pDirEntry, NULL, pDirEntryStream, pszDstFile, pszDstFile, 0);
   if (rc)
      goto FS_MOVEEXIT;

   /*
      If a directory was moved, the .. entry in the dir itself must
      be modified as well.
   */
#ifdef EXFAT
   if ( (pVolInfo->bFatType < FAT_TYPE_EXFAT) && (pDirEntry->bAttr & FILE_DIRECTORY) )
#else
   if ( pDirEntry->bAttr & FILE_DIRECTORY )
#endif
      {
      ULONG ulTmp;
      ulTmp = FindPathCluster(pVolInfo, ulCluster, "..", NULL, pDirEntry, pDirEntryStream, NULL);
      if (ulTmp != pVolInfo->ulFatEof)
         {
         PDIRENTRY pDirNew;

         pDirNew = (PDIRENTRY)malloc((size_t)sizeof(DIRENTRY));
         if (!pDirNew)
            {
            rc = ERROR_NOT_ENOUGH_MEMORY;
            goto FS_MOVEEXIT;
            }

         memcpy(pDirNew, pDirEntry, sizeof (DIRENTRY));
         pDirNew->wCluster = LOUSHORT(ulDstDirCluster);
         pDirNew->wClusterHigh = HIUSHORT(ulDstDirCluster);

         rc = ModifyDirectory(pVolInfo, ulCluster, NULL,
            MODIFY_DIR_UPDATE, pDirEntry, pDirNew, NULL, NULL, "..", "..", 0);
         //rc = ModifyDirectory(pVolInfo, ulCluster, NULL,
         //   MODIFY_DIR_UPDATE, pDirEntry, pDirNew, NULL, NULL, NULL, 0);
         free(pDirNew);
         if (rc)
            goto FS_MOVEEXIT;
         }
      else
         {
         rc = ERROR_PATH_NOT_FOUND;
         goto FS_MOVEEXIT;
         }
      }

FS_MOVEEXIT:

   if (pOISrc)
      {
      if (pOISrc->pSHInfo)
         ReleaseSH(pOISrc);
      else
         free(pOISrc);
      }

   if (pOIDst)
      {
      if (pOIDst->pSHInfo)
         ReleaseSH(pOIDst);
      else
         free(pOIDst);
      }

   if (szSrcLongName)
      free(szSrcLongName);
   if (szDstLongName)
      free(szDstLongName);
   if (pDirEntry)
      free(pDirEntry);
#ifdef EXFAT
   if (pDirEntryStream)
      free(pDirEntryStream);
   if (pDirEntryStreamOld)
      free(pDirEntryStreamOld);
   if (pDirSrcStream)
      free(pDirSrcStream);
   if (pDirDstStream)
      free(pDirDstStream);
   if (pDirSrcSHInfo)
      free(pDirSrcSHInfo);
   if (pDirDstSHInfo)
      free(pDirDstSHInfo);
#endif

   MessageL(LOG_FS, "FS_MOVE%m returned %d", 0x802c, rc);

   _asm pop es;

   return rc;

   usFlags = usFlags;
}

/******************************************************************
*
******************************************************************/
int far pascal _loadds FS_PROCESSNAME(
    char far *  pNameBuf        /* pNameBuf */
)
{
#if 1
     USHORT usUniCh;
     USHORT usLen;
     char   far *p;

     _asm push es;

     MessageL(LOG_FS, "FS_PROCESSNAME%m for %s", 0x002d, pNameBuf);

     for( p = pNameBuf; *p; p += usLen )
     {
        usLen = Translate2Win( p, &usUniCh, 1 );
        if( usUniCh == 0xFFFD )
        {
            *p = '_';
            if( usLen == 2 )
                p[ 1 ] = '_';
        }
     }
#endif
   MessageL(LOG_FS, " FS_PROCESSNAME%m returned filename: %s", 0x802d, pNameBuf);

   _asm pop es;

   return 0;
}


/******************************************************************
*
******************************************************************/
int far pascal _loadds FS_SHUTDOWN(
    unsigned short usType,      /* usType   */
    unsigned long    ulReserved /* ulReserved   */
)
{
PVOLINFO pVolInfo;
USHORT rc = 0;

   _asm push es;

   ulReserved = ulReserved;

   MessageL(LOG_FS, "FS_SHUTDOWN%m, Type = %d", 0x002e, usType);
   f32Parms.fInShutDown = TRUE;
   f32Parms.fLW = FALSE;

   if (usType == SD_BEGIN)
      {
      for (pVolInfo = pGlobVolInfo; pVolInfo;
           pVolInfo = (PVOLINFO)pVolInfo->pNextVolInfo)
         {
         if (pVolInfo->hVBP)
            // physical disk
            Message("disk: %c: fDiskCleanOnMount=%u", 
               pVolInfo->bDrive + 'a', pVolInfo->fDiskCleanOnMount);
         else
            // mounted image
            Message("disk: %s: fDiskCleanOnMount=%u", 
               pVolInfo->pbFilename, pVolInfo->fDiskCleanOnMount);

         if (pVolInfo->fWriteProtected)
            continue;

         if (pVolInfo->fFormatInProgress)
            continue;

         if (! pVolInfo->hVBP)
            continue;

         usFlushVolume( pVolInfo, FLUSH_DISCARD, TRUE, PRIO_URGENT );

         UpdateFSInfo(pVolInfo);
         if (! pVolInfo->fRemovable && (pVolInfo->bFatType != FAT_TYPE_FAT12) )
            {
            MarkDiskStatus(pVolInfo, pVolInfo->fDiskCleanOnMount);
            }
         }
      }
   else /* usType == SD_COMPLETE */
      TranslateFreeBuffer();

//FS_SHUTDOWNEXIT:

   MessageL(LOG_FS, "FS_SHUTDOWN%m returned %d", 0x802e, rc);

   _asm pop es;

   return rc;
}

/******************************************************************
*
******************************************************************/
int far pascal _loadds FS_VERIFYUNCNAME(
    unsigned short usFlag,      /* flag     */
    char far *  pName       /* pName    */
)
{
   _asm push es;

   MessageL(LOG_FS, "FS_VERIFYUNCNAME%m - NOT SUPPORTED", 0x002f);
   usFlag = usFlag;
   pName = pName;

   _asm pop es;

   return ERROR_NOT_SUPPORTED;
}



/******************************************************************
*
******************************************************************/
void * gdtAlloc(ULONG tSize, BOOL fSwap)
{
INT rc;
USHORT usSel;
PVOID pRet;

   MessageL(LOG_FUNCS, "gdtAlloc%m for %lu bytes", 0x0037, tSize);

   if (fSwap)
      rc = FSH_SEGALLOC(SA_FRING0|SA_FSWAP, tSize, &usSel);
   else
      rc = FSH_SEGALLOC(SA_FRING0, tSize, &usSel);
   if (rc)
      {
      Message("FSH_SEGALLOC (gdtAlloc %ld bytes) failed, rc = %d",
         tSize, rc);
      return NULL;
      }

   pRet = NULL;
   pRet = MAKEP(usSel, 0);

   f32Parms.usSegmentsAllocated++;
   return pRet;
}

/******************************************************************
*
******************************************************************/
void * ldtAlloc(ULONG tSize)
{
INT rc;
USHORT usSel;
PVOID pRet;

   MessageL(LOG_FUNCS, "ldtAlloc%m for %D bytes", 0x0038, tSize);

   rc = FSH_SEGALLOC(SA_FLDT | SA_FSWAP| SA_FRING3, tSize, &usSel);
   if (rc)
      {
      Message("FSH_SEGALLOC (ldtAlloc %ld bytes) failed, rc = %d",
         tSize, rc);
      return gdtAlloc(tSize, TRUE);
      }

   pRet = NULL;
   pRet = MAKEP(usSel, 0);
   f32Parms.usSegmentsAllocated++;
   return pRet;
}


/******************************************************************
*
******************************************************************/
/*
ULONG linalloc(ULONG tSize, BOOL fHighMem, BOOL fIgnore)
{
ULONG rc;
ULONG ulFlags;
ULONG ulAddress;
ULONG ulReserved;
PVOID pv = &ulReserved;

   MessageL(LOG_FUNCS, "linAlloc%m for %u bytes", 0x0039, tSize);

   ulFlags = VMDHA_FIXED;
   if( fHighMem )
      ulFlags |= VMDHA_USEHIGHMEM;

   rc = DevHelp_VMAlloc( ulFlags,
                tSize,
                -1,
                &ulAddress,
                (VOID **)&pv);
   if (rc)
      {
      MessageL(LOG_FUNCS, "ERROR: linalloc failed%m, rc = %d", 0x4015, rc);

      if( !fIgnore )
        Message("linalloc failed, rc = %d", rc);

      return 0xFFFFFFFF;
      }
   return ulAddress;
}
*/

APIRET linalloc(ULONG tSize, BOOL fHighMem, BOOL fIgnore, PULONG pulPhysAddr)
{
APIRET rc=NO_ERROR;
ULONG  ulFlags=0UL;
LIN    lulAddress=0UL;
LIN    lulPhysAddr = 0UL;
PVOID pv=NULL;

    MessageL(LOG_FUNCS, "linAlloc%m for %u bytes", 0x0039, tSize);

    rc = DevHelp_VirtToLin(     SELECTOROF(pulPhysAddr),
                                OFFSETOF(pulPhysAddr),
                                &lulPhysAddr);

    if (rc != NO_ERROR)
    {
        MessageL(LOG_FUNCS, "ERROR: linalloc VirtToLin failed%m, rc = %d", 0x4015, rc);

        if( !fIgnore )
            Message("linalloc VirtToLin failed, rc = %d", rc);

        return rc;
    }

    ulFlags = VMDHA_FIXED | VMDHA_CONTIG;
    if( fHighMem )
        ulFlags |= VMDHA_USEHIGHMEM;

    rc = DevHelp_VMAlloc( ulFlags,
                    tSize,
                    lulPhysAddr,
                    &lulAddress,
                    &pv);

    if (rc)
    {
        MessageL(LOG_FUNCS, "ERROR: linalloc VMAlloc failed%m, rc = %d", 0x4016, rc);

        if( !fIgnore )
            Message("linalloc VMAlloc failed, rc = %d", rc);

    }
    return rc;
}

/******************************************************************
*
******************************************************************/
void freeseg(void *p)
{
USHORT usSel;
USHORT rc;

   MessageL(LOG_FUNCS, "freeseg%m", 0x0040);

   if (!p)
      return;

   rc = MY_PROBEBUF(PB_OPREAD, p, 1);
   if (rc)
      {
      //CritMessage("FAT32: freeseg: protection violation!");
      Message("ERROR: freeseg: protection violation");
      return;
      }

   usSel = SELECTOROF(p);
   rc = FSH_SEGFREE(usSel);
   if (rc)
      {
      Message("FAT32: FSH_SEGFREE failed for selector %X", usSel);
      }
   else
      f32Parms.usSegmentsAllocated--;
}



/******************************************************************
*
******************************************************************/
USHORT ReadBlock(PVOLINFO pVolInfo, ULONG ulCluster, ULONG ulBlock, PVOID pbCluster, USHORT usIOMode)
{
ULONG  ulSector;
ULONG  ulNextCluster;
USHORT usSectorsPerBlock = (USHORT)(pVolInfo->ulBlockSize / pVolInfo->BootSect.bpb.BytesPerSector);
USHORT rc;

   MessageL(LOG_FUNCS, "ReadBlock%m %lu", 0x0041, ulCluster);

   if (ulCluster < 2 || ulCluster >= pVolInfo->ulTotalClusters + 2)
      {
      //CritMessage("ERROR: Cluster %lu does not exist on disk %c:",
      //   ulCluster, pVolInfo->bDrive + 'A');
      Message("ERROR: Cluster %lu (%lX) does not exist on disk %c:",
         ulCluster, ulCluster, pVolInfo->bDrive + 'A');
      return ERROR_SECTOR_NOT_FOUND;
      }

   // check for bad cluster
   ulNextCluster = GetNextCluster(pVolInfo, NULL, ulCluster);
   if (ulNextCluster == pVolInfo->ulFatBad)
      return ERROR_SECTOR_NOT_FOUND;

   ulSector = pVolInfo->ulStartOfData +
      (ulCluster - 2) * pVolInfo->SectorsPerCluster;

   rc = ReadSector(pVolInfo, ulSector + ulBlock * usSectorsPerBlock,
      usSectorsPerBlock,
      pbCluster, usIOMode);
   if (rc)
      {
      Message("ReadBlock: Cluster %lu, block %lu failed!", ulCluster, ulBlock);
      return rc;
      }

   return 0;
}

/******************************************************************
*
******************************************************************/
USHORT WriteBlock(PVOLINFO pVolInfo, ULONG ulCluster, ULONG ulBlock, PVOID pbCluster, USHORT usIOMode)
{
ULONG  ulSector;
ULONG  ulNextCluster;
USHORT usSectorsPerBlock = (USHORT)(pVolInfo->ulBlockSize / pVolInfo->BootSect.bpb.BytesPerSector);
USHORT rc;

   MessageL(LOG_FUNCS, "WriteBlock%m, %lu", 0x0042, ulCluster);

   if (ulCluster < 2 || ulCluster >= pVolInfo->ulTotalClusters + 2)
      {
      //CritMessage("ERROR: Cluster %ld does not exist on disk %c:",
      //   ulCluster, pVolInfo->bDrive + 'A');
      Message("ERROR: Cluster %ld does not exist on disk %c:",
         ulCluster, pVolInfo->bDrive + 'A');
      return ERROR_SECTOR_NOT_FOUND;
      }

   // check for bad cluster
   ulNextCluster = GetNextCluster(pVolInfo, NULL, ulCluster);
   if (ulNextCluster == pVolInfo->ulFatBad)
      return ERROR_SECTOR_NOT_FOUND;

   ulSector = pVolInfo->ulStartOfData +
      (ulCluster - 2) * pVolInfo->SectorsPerCluster;

   rc = WriteSector(pVolInfo, ulSector + ulBlock * usSectorsPerBlock,
      usSectorsPerBlock,
      pbCluster, usIOMode);
   if (rc)
      {
      Message("WriteBlock: Cluster %lu, block %lu failed!", ulCluster, ulBlock);
      return rc;
      }

   return 0;
}

/******************************************************************
*
******************************************************************/
USHORT ReadFatSector(PVOLINFO pVolInfo, ULONG ulSector)
{
ULONG  ulSec = ulSector * 3;
USHORT usNumSec = 3;
USHORT rc;

   MessageL(LOG_FUNCS, "ReadFatSector%m", 0x0043);

   // read multiples of three sectors,
   // to fit a whole number of FAT12 entries
   // (ulSector is indeed a number of 3*512
   // bytes blocks, so, it is needed to multiply by 3)

   // A 360 KB diskette has only 2 sectors per FAT
   if (pVolInfo->BootSect.bpb.BigSectorsPerFat < 3)
      {
      if (ulSector > 0)
         return ERROR_SECTOR_NOT_FOUND;
      else
         {
         ulSec = 0;
         usNumSec = (USHORT)pVolInfo->BootSect.bpb.BigSectorsPerFat;
         }
      }

   if (pVolInfo->ulCurFatSector == ulSector)
      return 0;

   if (ulSec >= pVolInfo->BootSect.bpb.BigSectorsPerFat)
      {
      //CritMessage("ERROR: ReadFatSector: Sector %lu too high", ulSec);
      Message("ERROR: ReadFatSector: Sector %lu too high", ulSec);
      return ERROR_SECTOR_NOT_FOUND;
      }

   rc = ReadSector(pVolInfo, pVolInfo->ulActiveFatStart + ulSec, usNumSec,
      pVolInfo->pbFatSector, 0);
   if (rc)
      return rc;

   pVolInfo->ulCurFatSector = ulSector;

   return 0;
}

/******************************************************************
*
******************************************************************/
USHORT WriteFatSector(PVOLINFO pVolInfo, ULONG ulSector)
{
ULONG  ulSec = ulSector * 3;
USHORT usNumSec = 3;
USHORT usFat;
USHORT rc;

   MessageL(LOG_FUNCS, "WriteFatSector%m", 0x0044);

   // write multiples of three sectors,
   // to fit a whole number of FAT12 entries
   // (ulSector is indeed a number of 3*512
   // bytes blocks, so, it is needed to multiply by 3)

   // A 360 KB diskette has only 2 sectors per FAT
   if (pVolInfo->BootSect.bpb.BigSectorsPerFat < 3)
      {
      if (ulSector > 0)
         return ERROR_SECTOR_NOT_FOUND;
      else
         {
         ulSec = 0;
         usNumSec = (USHORT)pVolInfo->BootSect.bpb.BigSectorsPerFat;
         }
      }

   if (pVolInfo->ulCurFatSector != ulSector)
      {
      //CritMessage("FAT32: WriteFatSector: Sectors do not match!");
      Message("ERROR: WriteFatSector: Sectors do not match!");
      return ERROR_SECTOR_NOT_FOUND;
      }

   if (ulSec >= pVolInfo->BootSect.bpb.BigSectorsPerFat)
      {
      //CritMessage("ERROR: WriteFatSector: Sector %ld too high", ulSec);
      Message("ERROR: WriteFatSector: Sector %ld too high", ulSec);
      return ERROR_SECTOR_NOT_FOUND;
      }

   for (usFat = 0; usFat < pVolInfo->BootSect.bpb.NumberOfFATs; usFat++)
      {
      rc = WriteSector(pVolInfo, pVolInfo->ulActiveFatStart + ulSec, usNumSec,
         pVolInfo->pbFatSector, 0);
      if (rc)
         return rc;

      if (pVolInfo->BootSect.bpb.ExtFlags & 0x0080)
         break;
      ulSec += pVolInfo->BootSect.bpb.BigSectorsPerFat; // !!!
      // @todo what if pVolInfo->ulActiveFatStart does not point to 1st FAT?
      }

   return 0;
}

#ifdef EXFAT

/******************************************************************
*
******************************************************************/
USHORT ReadBmpSector(PVOLINFO pVolInfo, ULONG ulSector)
{
// Read exFAT Allocation bitmap sector
USHORT rc;

   MessageL(LOG_FUNCS, "ReadBmpSector%m", 0x0045);

   if (pVolInfo->ulCurBmpSector == ulSector)
      return 0;

   if (ulSector >= (pVolInfo->ulAllocBmpLen + pVolInfo->BootSect.bpb.BytesPerSector - 1)
         / pVolInfo->BootSect.bpb.BytesPerSector)
      {
      //CritMessage("ERROR: ReadBmpSector: Sector %lu too high", ulSector);
      Message("ERROR: ReadBmpSector: Sector %lu too high", ulSector);
      return ERROR_SECTOR_NOT_FOUND;
      }

   rc = ReadSector(pVolInfo, pVolInfo->ulBmpStartSector + ulSector, 1,
      pVolInfo->pbFatBits, 0);
   if (rc)
      return rc;

   pVolInfo->ulCurBmpSector = ulSector;

   return 0;
}

/******************************************************************
*
******************************************************************/
USHORT WriteBmpSector(PVOLINFO pVolInfo, ULONG ulSector)
{
// Write exFAT Allocation bitmap sector
//USHORT usBmp;
USHORT rc;

   MessageL(LOG_FUNCS, "WriteBmpSector%m", 0x0046);

   if (pVolInfo->ulCurBmpSector != ulSector)
      {
      //CritMessage("FAT32: WriteBmpSector: Sectors do not match!");
      Message("ERROR: WriteBmpSector: Sectors do not match!");
      return ERROR_SECTOR_NOT_FOUND;
      }

   if (ulSector >= (pVolInfo->ulAllocBmpLen + pVolInfo->BootSect.bpb.BytesPerSector - 1)
         / pVolInfo->BootSect.bpb.BytesPerSector)
      {
      //CritMessage("ERROR: WriteBmpSector: Sector %ld too high", ulSector);
      Message("ERROR: WriteBmpSector: Sector %ld too high", ulSector);
      return ERROR_SECTOR_NOT_FOUND;
      }

   //for (usFat = 0; usFat < pVolInfo->BootSect.bpb.NumberOfFATs; usFat++)
   //   {
      rc = WriteSector(pVolInfo, pVolInfo->ulBmpStartSector + ulSector, 1,
         pVolInfo->pbFatBits, 0);
      if (rc)
         return rc;

      //if (pVolInfo->BootSect.bpb.ExtFlags & 0x0080)
      //   break;
      //ulSec += pVolInfo->BootSect.bpb.BigSectorsPerFat; // !!!
      // @todo what if pVolInfo->ulActiveFatStart does not point to 1st FAT?
   //   }

   return 0;
}

#endif

/******************************************************************
*
******************************************************************/
static ULONG GetFatEntrySec(PVOLINFO pVolInfo, ULONG ulCluster)
{
   return GetFatEntryBlock(pVolInfo, ulCluster, (ULONG)pVolInfo->BootSect.bpb.BytesPerSector * 3); // in three sector blocks
}

/******************************************************************
*
******************************************************************/
static ULONG GetFatEntryBlock(PVOLINFO pVolInfo, ULONG ulCluster, ULONG ulBlockSize)
{
ULONG  ulSector;

   ulCluster &= pVolInfo->ulFatEof;

   switch (pVolInfo->bFatType)
      {
      case FAT_TYPE_FAT12:
         ulSector = ((ulCluster * 3) / 2) / ulBlockSize;
         break;

      case FAT_TYPE_FAT16:
         ulSector = (ulCluster * 2) / ulBlockSize;
         break;

      case FAT_TYPE_FAT32:
#ifdef EXFAT
      case FAT_TYPE_EXFAT:
#endif
         ulSector = (ulCluster * 4) / ulBlockSize;
      }

   return ulSector;
}

/******************************************************************
*
******************************************************************/
static ULONG GetFatEntry(PVOLINFO pVolInfo, ULONG ulCluster)
{
   return GetFatEntryEx(pVolInfo, pVolInfo->pbFatSector, ulCluster, (ULONG)pVolInfo->BootSect.bpb.BytesPerSector * 3);
}

/******************************************************************
*
******************************************************************/
static ULONG GetFatEntryEx(PVOLINFO pVolInfo, PBYTE pFatStart, ULONG ulCluster, ULONG ulBlockSize)
{
   ulCluster &= pVolInfo->ulFatEof;

   switch (pVolInfo->bFatType)
      {
      case FAT_TYPE_FAT12:
         {
         ULONG   ulOffset;
         PUSHORT pusCluster;

         ulOffset = (ulCluster * 3) / 2;

         if (ulBlockSize)
            ulOffset %= ulBlockSize;

         pusCluster = (PUSHORT)((PBYTE)pFatStart + ulOffset);
         ulCluster = ( ((ulCluster * 3) % 2) ?
            *pusCluster >> 4 : // odd
            *pusCluster )      // even
            & pVolInfo->ulFatEof;
         break;
         }

      case FAT_TYPE_FAT16:
         {
         PUSHORT pusCluster;
         ULONG   ulOffset = ulCluster * 2;

         if (ulBlockSize)
            ulOffset %= ulBlockSize;

         pusCluster = (PUSHORT)((PBYTE)pFatStart + ulOffset);
         ulCluster = *pusCluster & pVolInfo->ulFatEof;
         break;
         }

      case FAT_TYPE_FAT32:
#ifdef EXFAT
      case FAT_TYPE_EXFAT:
#endif
         {
         PULONG pulCluster;
         ULONG   ulOffset = ulCluster * 4;

         if (ulBlockSize)
            ulOffset %= ulBlockSize;

         pulCluster = (PULONG)((PBYTE)pFatStart + ulOffset);
         ulCluster = *pulCluster & pVolInfo->ulFatEof;
         }
      }

   return ulCluster;
}

/******************************************************************
*
******************************************************************/
static void SetFatEntry(PVOLINFO pVolInfo, ULONG ulCluster, ULONG ulValue)
{
   SetFatEntryEx(pVolInfo, pVolInfo->pbFatSector, ulCluster, ulValue, (ULONG)pVolInfo->BootSect.bpb.BytesPerSector * 3);
}

/******************************************************************
*
******************************************************************/
static void SetFatEntryEx(PVOLINFO pVolInfo, PBYTE pFatStart, ULONG ulCluster, ULONG ulValue, ULONG ulBlockSize)
{
//USHORT usPrevValue;

   ulCluster &= pVolInfo->ulFatEof;
   ulValue   &= pVolInfo->ulFatEof;

   switch (pVolInfo->bFatType)
      {
      case FAT_TYPE_FAT12:
         {
         ULONG   ulOffset;
         PUSHORT pusCluster;
         ULONG   ulNewValue;
         ULONG   usPrevValue;

         ulOffset = (ulCluster * 3) / 2;

         if (ulBlockSize)
            ulOffset %= ulBlockSize;

         pusCluster = (PUSHORT)((PBYTE)pFatStart + ulOffset);
         usPrevValue = *pusCluster;
         ulNewValue = ((ulCluster * 3) % 2)  ?
            (usPrevValue & 0xf) | (ulValue << 4) : // odd
            (usPrevValue & 0xf000) | (ulValue);    // even
         *pusCluster = (USHORT)ulNewValue;
         break;
         }

      case FAT_TYPE_FAT16:
         {
         PUSHORT pusCluster;
         ULONG   ulOffset = ulCluster * 2;

         if (ulBlockSize)
            ulOffset %= ulBlockSize;

         pusCluster = (PUSHORT)((PBYTE)pFatStart + ulOffset);
         *pusCluster = (USHORT)ulValue;
         break;
         }

      case FAT_TYPE_FAT32:
#ifdef EXFAT
      case FAT_TYPE_EXFAT:
#endif
         {
         PULONG pulCluster;
         ULONG   ulOffset = ulCluster * 4;

         if (ulBlockSize)
            ulOffset %= ulBlockSize;

         pulCluster = (PULONG)((PBYTE)pFatStart + ulOffset);
         *pulCluster = ulValue;
         }
      }
}

/******************************************************************
*
******************************************************************/
ULONG GetNextCluster(PVOLINFO pVolInfo, PSHOPENINFO pSHInfo, ULONG ulCluster)
{
   MessageL(LOG_FUNCS, "GetNextCluster%m for %lu", 0x0047, ulCluster);

   if (!GetFatAccess(pVolInfo, "GetNextCluster"))
      {
      ulCluster = GetNextCluster2(pVolInfo, pSHInfo, ulCluster);
      ReleaseFat(pVolInfo);
      return ulCluster;
      }
   else
      return pVolInfo->ulFatEof;
}

/******************************************************************
*
******************************************************************/
ULONG GetNextCluster2(PVOLINFO pVolInfo, PSHOPENINFO pSHInfo, ULONG ulCluster)
{
ULONG ulSector;

   if (ulCluster >= pVolInfo->ulTotalClusters + 2 &&
       !(ulCluster >= pVolInfo->ulFatBad && ulCluster <= pVolInfo->ulFatEof))
      return pVolInfo->ulFatEof;

#ifdef EXFAT
   if ( (pVolInfo->bFatType == FAT_TYPE_EXFAT) &&
       pSHInfo && (pSHInfo->fNoFatChain & 1) )
      {
      if (ulCluster < pSHInfo->ulLastCluster)
         return ulCluster + 1;
      else
         return pVolInfo->ulFatEof;
      }
#endif

   ulSector = GetFatEntrySec(pVolInfo, ulCluster);
   if (ReadFatSector(pVolInfo, ulSector))
      {
      Message("GetNextCluster for %ld failed", ulCluster);
      return pVolInfo->ulFatEof;
      }

   ulCluster = GetFatEntry(pVolInfo, ulCluster);
   if (ulCluster >= pVolInfo->ulFatEof2 && ulCluster <= pVolInfo->ulFatEof)
      return pVolInfo->ulFatEof;
   return ulCluster;
}

/******************************************************************
*
******************************************************************/
ULONG GetAllocBitmapSec(PVOLINFO pVolInfo, ULONG ulCluster)
{
ULONG ulOffset;

   ulCluster -= 2;
   ulOffset  = ulCluster / 8;

   return ulOffset / pVolInfo->BootSect.bpb.BytesPerSector;
}

BOOL GetBmpEntry(PVOLINFO pVolInfo, ULONG ulCluster)
{
ULONG ulOffset;
USHORT usShift;
BYTE bMask;

   ulCluster -= 2;
   ulOffset = (ulCluster / 8) % pVolInfo->BootSect.bpb.BytesPerSector;
   usShift = (USHORT)(ulCluster % 8);
   //bMask = (BYTE)(0x80 >> usShift);
   bMask = (BYTE)(1 << usShift);

   if (pVolInfo->pbFatBits[ulOffset] & bMask)
      return TRUE;
   else
      return FALSE;
}

VOID SetBmpEntry(PVOLINFO pVolInfo, ULONG ulCluster, BOOL fState)
{
ULONG ulOffset;
USHORT usShift;
BYTE bMask;

   ulCluster -= 2;
   ulOffset = (ulCluster / 8) % pVolInfo->BootSect.bpb.BytesPerSector;
   usShift = (USHORT)(ulCluster % 8);
   //bMask = (BYTE)(0x80 >> usShift);
   bMask = (BYTE)(1 << usShift);

   if (fState)
      pVolInfo->pbFatBits[ulOffset] |= bMask;
   else
      pVolInfo->pbFatBits[ulOffset] &= ~bMask;
}

/******************************************************************
*
******************************************************************/
BOOL ClusterInUse(PVOLINFO pVolInfo, ULONG ulCluster)
{
#ifdef EXFAT
ULONG ulBmpSector;
#endif
BOOL rc;

   MessageL(LOG_FUNCS, "ClusterInUse%m for %lu", 0x0048, ulCluster);

   //if (GetFatAccess(pVolInfo, "ClusterInUse"))
   //   return FALSE;

   if (ulCluster >= pVolInfo->ulTotalClusters + 2)
      {
      Message("An invalid cluster number %8.8lX was found\n", ulCluster);
      rc = FALSE;
      //rc = TRUE;
      goto ClusterInUse_Exit;
      }

#ifdef EXFAT
   if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
#endif
      {
      ULONG ulNextCluster;
      ulNextCluster = GetNextCluster(pVolInfo, NULL, ulCluster);
      if (ulNextCluster)
         rc = TRUE;
      else
         rc = FALSE;
      goto ClusterInUse_Exit;
      }

#ifdef EXFAT
   ulBmpSector = GetAllocBitmapSec(pVolInfo, ulCluster);

   // sanity check
   if (ulBmpSector > pVolInfo->ulAllocBmpLen ||
       ulBmpSector > pVolInfo->BootSect.bpb.BigTotalSectors)
      {
      rc = TRUE;
      goto ClusterInUse_Exit;
      }

   if ( pVolInfo->ulCurBmpSector != ulBmpSector &&
       ReadBmpSector(pVolInfo, ulBmpSector) )
      return TRUE;

   rc = GetBmpEntry(pVolInfo, ulCluster);
#else
   rc = FALSE;
#endif

ClusterInUse_Exit:
   //ReleaseFat(pVolInfo);
   return rc;
}

#ifdef EXFAT

/******************************************************************
*
******************************************************************/
BOOL MarkCluster(PVOLINFO pVolInfo, ULONG ulCluster, BOOL fState)
{
ULONG ulBmpSector;

   MessageL(LOG_FUNCS, "MarkCluster%m for %lu", 0x0049, ulCluster);

   if (!GetFatAccess(pVolInfo, "MarkCluster"))
      {
      //if (ClusterInUse2(pVolInfo, ulCluster) && fState)
      //   {
      //   return FALSE;
      //   }

      ulBmpSector = GetAllocBitmapSec(pVolInfo, ulCluster);

      if (pVolInfo->ulCurBmpSector != ulBmpSector)
         ReadBmpSector(pVolInfo, ulBmpSector);

      SetBmpEntry(pVolInfo, ulCluster, fState);
      WriteBmpSector(pVolInfo, ulBmpSector);
      ReleaseFat(pVolInfo);
      return TRUE;
      }

   return FALSE;
}

#endif

/******************************************************************
*
******************************************************************/
ULONG GetFreeSpace(PVOLINFO pVolInfo)
{
//ULONG ulSector = 0;
ULONG ulCluster = 0;
ULONG ulNextFree = 0;
ULONG ulTotalFree;

   MessageL(LOG_FUNCS, "GetFreeSpace%m", 0x004a);

   //if (GetFatAccess(pVolInfo, "GetFreeSpace"))
   //   return 0L;

   ulTotalFree = 0;
   for (ulCluster = 2; ulCluster < pVolInfo->ulTotalClusters + 2; ulCluster++)
      {
      //ULONG ulNextCluster = 0;
      //ulSector = GetFatEntrySec(pVolInfo, ulCluster);

      //if (ulSector != pVolInfo->ulCurFatSector)
      //   ReadFatSector(pVolInfo, ulSector);

      //ulNextCluster = GetFatEntry(pVolInfo, ulCluster);
      //if (ulNextCluster == 0)
      if (! ClusterInUse(pVolInfo, ulCluster))
         {
         ulTotalFree++;
         if (! ulNextFree)
            ulNextFree = ulCluster;
         }
      }

   if (pVolInfo->pBootFSInfo->ulFreeClusters != ulTotalFree)
      {
      pVolInfo->pBootFSInfo->ulFreeClusters = ulTotalFree;
      /* UpdateFSInfo(pVolInfo); */
      }
   if (pVolInfo->pBootFSInfo->ulNextFreeCluster != ulNextFree)
      {
      pVolInfo->pBootFSInfo->ulNextFreeCluster = ulNextFree;
      /* UpdateFSInfo(pVolInfo); */
      }

   return ulTotalFree;
}

/******************************************************************
*
******************************************************************/
ULONG MakeFatChain(PVOLINFO pVolInfo, PSHOPENINFO pSHInfo, ULONG ulPrevCluster, ULONG ulClustersRequested, PULONG pulLast)
{
ULONG  ulCluster = 0;
ULONG  ulFirstCluster = 0;
ULONG  ulStartCluster = 0;
ULONG  ulNextCluster = 0;
ULONG  ulLargestChain;
ULONG  ulLargestSize;
ULONG  ulReturn = 0;
ULONG  ulSector = 0;
ULONG  ulBmpSector = 0;
BOOL   fStartAt2;
BOOL   fContiguous;
#ifdef EXFAT
BOOL   fStatus;
#endif
BOOL   fClean;

   MessageL(LOG_FUNCS, "MakeFatChain%m, %lu clusters", 0x004b, ulClustersRequested);

   if (!ulClustersRequested)
      return pVolInfo->ulFatEof;

   /*
      Trick, set fDiskClean to FALSE, so WriteSector
      won't set is back to dirty again
    */
   fClean = pVolInfo->fDiskClean;
   pVolInfo->fDiskClean = FALSE;

   if (pVolInfo->pBootFSInfo->ulFreeClusters < ulClustersRequested)
      return pVolInfo->ulFatEof;

   ulReturn = pVolInfo->ulFatEof;
   fContiguous = TRUE;
   for (;;)
      {
      ulLargestChain = pVolInfo->ulFatEof;
      ulLargestSize = 0;

      //ulFirstCluster = pVolInfo->pBootFSInfo->ulNextFreeCluster + 1;
      ulFirstCluster = pVolInfo->pBootFSInfo->ulNextFreeCluster;
      if (ulFirstCluster < 2 || ulFirstCluster >= pVolInfo->ulTotalClusters + 2)
         {
         fStartAt2 = TRUE;
         ulFirstCluster = 2;
         ulStartCluster = pVolInfo->ulTotalClusters + 3;
         }
      else
         {
         ulStartCluster = ulFirstCluster;
         fStartAt2 = FALSE;
         }

      for (;;)
         {
#ifdef CALL_YIELD
         Yield();
#endif
         /*
            Find first free cluster
         */
#ifdef EXFAT
         if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
            {
#endif
            while (ulFirstCluster < pVolInfo->ulTotalClusters + 2)
               {
               if (!GetFatAccess(pVolInfo, "MakeFatChain"))
                  {
                  ulSector  = GetFatEntrySec(pVolInfo, ulFirstCluster);
                  ulNextCluster = GetFatEntry(pVolInfo, ulFirstCluster);

                  if (ulSector != pVolInfo->ulCurFatSector)
                     ReadFatSector(pVolInfo, ulSector);

                  ReleaseFat(pVolInfo);
                  }

               if (!ulNextCluster)
                  break;
               ulFirstCluster++;
               }
#ifdef EXFAT
            }
         else
            {
            while (ulFirstCluster < pVolInfo->ulTotalClusters + 2)
               {
               if (!GetFatAccess(pVolInfo, "MakeFatChain"))
                  {
                  ulBmpSector = GetAllocBitmapSec(pVolInfo, ulFirstCluster);
                  fStatus = GetBmpEntry(pVolInfo, ulFirstCluster);

                  if (ulBmpSector != pVolInfo->ulCurBmpSector)
                     ReadBmpSector(pVolInfo, ulBmpSector);

                  ReleaseFat(pVolInfo);
                  }

               if (!fStatus)
                  break;
               ulFirstCluster++;
               }
            }
#endif

         if (fStartAt2 && ulFirstCluster >= ulStartCluster)
            break;

         if (ulFirstCluster >= pVolInfo->ulTotalClusters + 2)
            {
            if (fStartAt2)
               break;
            MessageL(LOG_FUNCS, "No contiguous block found, restarting at cluster 2%m", 0x4023);
            ulFirstCluster = 2;
            fStartAt2 = TRUE;
            continue;
            }


         /*
            Check if chain is long enough
         */

#ifdef EXFAT
         if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
            {
#endif
            for (ulCluster = ulFirstCluster ;
                     ulCluster < ulFirstCluster + ulClustersRequested &&
                     ulCluster < pVolInfo->ulTotalClusters + 2;
                           ulCluster++)
               {
               ULONG ulNewCluster = 0;
               if (!GetFatAccess(pVolInfo, "MakeFatChain"))
                  {
                  ulSector = GetFatEntrySec(pVolInfo, ulCluster);

                  ulNewCluster = GetFatEntry(pVolInfo, ulCluster);
                  if (ulSector != pVolInfo->ulCurFatSector)
                     ReadFatSector(pVolInfo, ulSector);

                  ReleaseFat(pVolInfo);
                  }
               if (ulNewCluster)
                  break;
               }
#ifdef EXFAT
            }
         else
            {
            for (ulCluster = ulFirstCluster ;
                     ulCluster < ulFirstCluster + ulClustersRequested &&
                     ulCluster < pVolInfo->ulTotalClusters + 2;
                           ulCluster++)
               {
               BOOL fStatus1;
               if (!GetFatAccess(pVolInfo, "MakeFatChain"))
                  {
                  ulBmpSector = GetAllocBitmapSec(pVolInfo, ulCluster);

                  fStatus1 = GetBmpEntry(pVolInfo, ulCluster);
                  if (ulBmpSector != pVolInfo->ulCurBmpSector)
                     ReadBmpSector(pVolInfo, ulBmpSector);

                  ReleaseFat(pVolInfo);
                  }
               if (fStatus1)
                  break;
               }
            }
#endif

         if (ulCluster != ulFirstCluster + ulClustersRequested)
            {
            /*
               Keep the largests chain found
            */
            if (ulCluster - ulFirstCluster > ulLargestSize)
               {
               ulLargestChain = ulFirstCluster;
               ulLargestSize  = ulCluster - ulFirstCluster;
               }
            ulFirstCluster = ulCluster;
            continue;
            }

         /*
            Chain found long enough
         */
         if (ulReturn == pVolInfo->ulFatEof)
            ulReturn = ulFirstCluster;

         if (MakeChain(pVolInfo, pSHInfo, ulFirstCluster, ulClustersRequested))
            goto MakeFatChain_Error;

         if (ulPrevCluster != pVolInfo->ulFatEof)
            {
            if (SetNextCluster(pVolInfo, ulPrevCluster, ulFirstCluster) == pVolInfo->ulFatEof)
               goto MakeFatChain_Error;
            }

         if (fContiguous)
            MessageL(LOG_FUNCS, "Contiguous chain returned, first%m = %lu", 0x4024, ulReturn);
         else
            MessageL(LOG_FUNCS, "NON Contiguous chain returned, first%m = %lu", 0x4025, ulReturn);
         if (pulLast)
            *pulLast = ulFirstCluster + ulClustersRequested - 1;
         return ulReturn;
         }

      /*
         We get here only if no free chain long enough was found!
      */
      MessageL(LOG_FUNCS, "No contiguous block found, largest found is%m %lu clusters", 0x4026, ulLargestSize);
      fContiguous = FALSE;

      if (ulLargestChain != pVolInfo->ulFatEof)
         {
         ulFirstCluster = ulLargestChain;
         if (ulReturn == pVolInfo->ulFatEof)
            ulReturn = ulFirstCluster;

         if (MakeChain(pVolInfo, pSHInfo, ulFirstCluster, ulLargestSize))
            goto MakeFatChain_Error;

         if (ulPrevCluster != pVolInfo->ulFatEof)
            {
            if (SetNextCluster(pVolInfo, ulPrevCluster, ulFirstCluster) == pVolInfo->ulFatEof)
               goto MakeFatChain_Error;
            }

         ulPrevCluster        = ulFirstCluster + ulLargestSize - 1;
         ulClustersRequested -= ulLargestSize;
         }
      else
         break;
      }

MakeFatChain_Error:

   if (ulReturn != pVolInfo->ulFatEof)
      DeleteFatChain(pVolInfo, ulReturn);

   pVolInfo->fDiskClean = fClean;

   return pVolInfo->ulFatEof;
}

/******************************************************************
*
******************************************************************/
USHORT MakeChain(PVOLINFO pVolInfo, PSHOPENINFO pSHInfo, ULONG ulFirstCluster, ULONG ulSize)
{
ULONG ulSector = 0;
ULONG ulBmpSector = 0;
ULONG ulLastCluster = 0;
ULONG ulCluster = 0;
ULONG ulNewCluster = 0;
#ifdef EXFAT
BOOL fStatus;
#endif
USHORT rc;

   MessageL(LOG_FUNCS, "MakeChain%m", 0x004c);

   ulLastCluster = ulFirstCluster + ulSize - 1;

   if (!GetFatAccess(pVolInfo, "MakeChain"))
      {
      ulSector = GetFatEntrySec(pVolInfo, ulFirstCluster);
      if (ulSector != pVolInfo->ulCurFatSector)
         ReadFatSector(pVolInfo, ulSector);
      ReleaseFat(pVolInfo);
      }

#ifdef EXFAT
   if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
      {
      if (!GetFatAccess(pVolInfo, "MakeChain"))
         {
         ulBmpSector = GetAllocBitmapSec(pVolInfo, ulFirstCluster);
         if (ulBmpSector != pVolInfo->ulCurBmpSector)
            ReadBmpSector(pVolInfo, ulBmpSector);
         ReleaseFat(pVolInfo);
         }
      }
#endif

   for (ulCluster = ulFirstCluster; ulCluster < ulLastCluster; ulCluster++)
      {
      if (! pSHInfo || ! pSHInfo->fNoFatChain)
         {
         if (!GetFatAccess(pVolInfo, "MakeChain"))
            {
            ulSector = GetFatEntrySec(pVolInfo, ulCluster);
            if (ulSector != pVolInfo->ulCurFatSector)
               {
               rc = WriteFatSector(pVolInfo, pVolInfo->ulCurFatSector);
               if (rc)
                  return rc;
               ReadFatSector(pVolInfo, ulSector);
               }
            ulNewCluster = GetFatEntry(pVolInfo, ulCluster);
#ifdef EXFAT
            if (ulNewCluster && pVolInfo->bFatType < FAT_TYPE_EXFAT)
#else
            if (ulNewCluster)
#endif
               {
               //CritMessage("FAT32:MakeChain:Cluster %lx is not free!", ulCluster);
               Message("ERROR:MakeChain:Cluster %lx is not free!", ulCluster);
               return ERROR_SECTOR_NOT_FOUND;
               }
            SetFatEntry(pVolInfo, ulCluster, ulCluster + 1);
            ReleaseFat(pVolInfo);
            }
         }
#ifdef EXFAT
      if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
         {
         if (!GetFatAccess(pVolInfo, "MakeChain"))
            {
            ulBmpSector = GetAllocBitmapSec(pVolInfo, ulCluster);
            if (ulBmpSector != pVolInfo->ulCurBmpSector)
               {
               rc = WriteBmpSector(pVolInfo, pVolInfo->ulCurBmpSector);
               if (rc)
                  return rc;
               ReadBmpSector(pVolInfo, ulBmpSector);
               }
            fStatus = GetBmpEntry(pVolInfo, ulCluster);
            if (fStatus)
               {
               //CritMessage("FAT32:MakeChain:Cluster %lx is not free!", ulCluster);
               Message("ERROR:MakeChain:Cluster %lx is not free!", ulCluster);
               return ERROR_SECTOR_NOT_FOUND;
               }
            SetBmpEntry(pVolInfo, ulCluster, 1);
            ReleaseFat(pVolInfo);
            }
         }
#endif
      }

#ifdef EXFAT
   if (! pSHInfo || ! pSHInfo->fNoFatChain)
#endif
      {
      if (!GetFatAccess(pVolInfo, "MakeChain"))
         {
         ulSector = GetFatEntrySec(pVolInfo, ulCluster);
         if (ulSector != pVolInfo->ulCurFatSector)
            {
            rc = WriteFatSector(pVolInfo, pVolInfo->ulCurFatSector);
            if (rc)
               return rc;
            ReadFatSector(pVolInfo, ulSector);
            }
         ulNewCluster = GetFatEntry(pVolInfo, ulCluster);
         if (ulNewCluster && pVolInfo->bFatType < FAT_TYPE_EXFAT)
            {
            //CritMessage("FAT32:MakeChain:Cluster %lx is not free!", ulCluster);
            Message("ERROR:MakeChain:Cluster %lx is not free!", ulCluster);
            return ERROR_SECTOR_NOT_FOUND;
            }

         SetFatEntry(pVolInfo, ulCluster, pVolInfo->ulFatEof);
         rc = WriteFatSector(pVolInfo, pVolInfo->ulCurFatSector);
         if (rc)
            return rc;
         ReleaseFat(pVolInfo);
         }
      }

#ifdef EXFAT
   if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
      {
      if (!GetFatAccess(pVolInfo, "MakeChain"))
         {
         ulBmpSector = GetAllocBitmapSec(pVolInfo, ulCluster);
         if (ulBmpSector != pVolInfo->ulCurBmpSector)
            {
            rc = WriteBmpSector(pVolInfo, pVolInfo->ulCurBmpSector);
            if (rc)
               return rc;
            ReadBmpSector(pVolInfo, ulBmpSector);
            }
         fStatus = GetBmpEntry(pVolInfo, ulCluster);
         if (fStatus)
            {
            //CritMessage("FAT32:MakeChain:Cluster %lx is not free!", ulCluster);
            Message("ERROR:MakeChain:Cluster %lx is not free!", ulCluster);
            return ERROR_SECTOR_NOT_FOUND;
            }

         SetBmpEntry(pVolInfo, ulCluster, 1);
         rc = WriteBmpSector(pVolInfo, pVolInfo->ulCurBmpSector);
         if (rc)
            return rc;
         ReleaseFat(pVolInfo);
         }
      }
#endif

   //pVolInfo->pBootFSInfo->ulNextFreeCluster = ulCluster;
   pVolInfo->pBootFSInfo->ulNextFreeCluster = ulCluster + 1;
   pVolInfo->pBootFSInfo->ulFreeClusters   -= ulSize;

   return 0;
}

/******************************************************************
*
******************************************************************/
BOOL UpdateFSInfo(PVOLINFO pVolInfo)
{
PBYTE bSector;

   MessageL(LOG_FUNCS, "UpdateFSInfo%m", 0x004d);

   if (pVolInfo->fFormatInProgress)
      return FALSE;

   if (pVolInfo->fWriteProtected)
      return TRUE;

   if (pVolInfo->BootSect.bpb.FSinfoSec == 0xFFFF)
      return TRUE;

   // no FSInfo sector on FAT12/FAT16
   if (pVolInfo->bFatType != FAT_TYPE_FAT32)
      return TRUE;

   bSector = malloc(pVolInfo->BootSect.bpb.BytesPerSector);

   if (!ReadSector(pVolInfo, pVolInfo->BootSect.bpb.FSinfoSec, 1, bSector, DVIO_OPNCACHE))
      {
      memcpy(bSector + FSINFO_OFFSET, pVolInfo->pBootFSInfo, sizeof (BOOTFSINFO));
      if (!WriteSector(pVolInfo, pVolInfo->BootSect.bpb.FSinfoSec, 1, bSector, DVIO_OPNCACHE | DVIO_OPWRTHRU))
         {
         free(bSector);
         return TRUE;
         }
      }
   //CritMessage("UpdateFSInfo for %c: failed!", pVolInfo->bDrive + 'A');
   Message("ERROR: UpdateFSInfo for %c: failed!", pVolInfo->bDrive + 'A');

   free(bSector);
   return FALSE;
}

/******************************************************************
*
******************************************************************/
BOOL MarkDiskStatus(PVOLINFO pVolInfo, BOOL fClean)
{
static BYTE bSector[SECTOR_SIZE * 8] = {0};
ULONG ulNextCluster = 0;
ULONG ulSector;
USHORT usFat;
PBYTE pbSector;

   MessageL(LOG_FUNCS, "MarkDiskStatus%m, %d", 0x004e, fClean);

   if (!pVolInfo->fDiskCleanOnMount && fClean)
      return TRUE;

   if (pVolInfo->fWriteProtected)
      return TRUE;

#ifdef EXFAT
   if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
      {
      // set dirty flag in volume flags of boot sector
      USHORT usVolFlags;
      PBOOTSECT1 pbBoot = (PBOOTSECT1)bSector;

      /*
         Trick, set fDiskClean to FALSE, so WriteSector
         won't set is back to dirty again
      */
      pVolInfo->fDiskClean = FALSE;

      if (ReadSector(pVolInfo, 0, 1, (PBYTE)pbBoot, DVIO_OPNCACHE))
         return FALSE;

      usVolFlags = pbBoot->usVolumeFlags;

      if (fClean)
         usVolFlags &= ~VOL_FLAG_VOLDIRTY;
      else
         usVolFlags |= VOL_FLAG_VOLDIRTY;

      pbBoot->usVolumeFlags = usVolFlags;

      if (WriteSector(pVolInfo, 0, 1, (PBYTE)pbBoot, DVIO_OPNCACHE))
         return FALSE;

      pVolInfo->fDiskClean = fClean;

      return TRUE;
      }
#endif

   if (pVolInfo->ulCurFatSector != 0)
      {
      if (ReadSector(pVolInfo, pVolInfo->ulActiveFatStart, 1,
         bSector, DVIO_OPNCACHE))
         return FALSE;
      pbSector = bSector;
      }
   else
      {
      pbSector = pVolInfo->pbFatSector;
      }

   ulNextCluster = GetFatEntryEx(pVolInfo, pbSector, 1, pVolInfo->BootSect.bpb.BytesPerSector);

   if (fClean)
      {
      ulNextCluster |= pVolInfo->ulFatClean;
      }
   else
      {
      ulNextCluster  &= ~pVolInfo->ulFatClean;
      }

   SetFatEntryEx(pVolInfo, pbSector, 1, ulNextCluster, pVolInfo->BootSect.bpb.BytesPerSector);

   /*
      Trick, set fDiskClean to FALSE, so WriteSector
      won't set is back to dirty again
   */
   pVolInfo->fDiskClean = FALSE;

   ulSector = 0L;
   for (usFat = 0; usFat < pVolInfo->BootSect.bpb.NumberOfFATs; usFat++)
      {
      if (WriteSector(pVolInfo, pVolInfo->ulActiveFatStart + ulSector, 1,
         pbSector, DVIO_OPWRTHRU | DVIO_OPNCACHE))
         {
         ReleaseFat(pVolInfo);
         return FALSE;
         }
      if (pVolInfo->BootSect.bpb.ExtFlags & 0x0080)
         break;
      ulSector += pVolInfo->BootSect.bpb.BigSectorsPerFat;
      }

   pVolInfo->fDiskClean = fClean;

   return TRUE;
}

/******************************************************************
*
******************************************************************/
BOOL GetDiskStatus(PVOLINFO pVolInfo)
{
ULONG ulStatus = 0;
BOOL  fStatus;

#ifdef EXFAT
   if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
      {
      PBOOTSECT1 pbBoot = (PBOOTSECT1)malloc(pVolInfo->BootSect.bpb.BytesPerSector);

      if (ReadSector(pVolInfo, 0, 1, (PBYTE)pbBoot, DVIO_OPNCACHE))
         return FALSE;

      fStatus = ! (pbBoot->usVolumeFlags & VOL_FLAG_VOLDIRTY);
      free(pbBoot);
      return fStatus;
      }
#endif

   if (GetFatAccess(pVolInfo, "GetDiskStatus"))
      return FALSE;

   if (ReadFatSector(pVolInfo, 0L))
      {
      ReleaseFat(pVolInfo);
      return FALSE;
      }

   ulStatus = GetFatEntry(pVolInfo, 1);

   if (ulStatus & pVolInfo->ulFatClean)
      fStatus = TRUE;
   else
      fStatus = FALSE;

   ReleaseFat(pVolInfo);

   return fStatus;
}

/******************************************************************
*
******************************************************************/
ULONG SetNextCluster(PVOLINFO pVolInfo, ULONG ulCluster, ULONG ulNext)
{
ULONG ulNewCluster = 0;
BOOL fUpdateFSInfo, fClean;
ULONG ulReturn = 0;
USHORT rc;

   MessageL(LOG_FUNCS, "SetNextCluster%m", 0x004f);

   ulReturn = ulNext;
   if (ulCluster == FAT_ASSIGN_NEW)
      {
      /*
         A new seperate CHAIN is started.
      */
      ulCluster = GetFreeCluster(pVolInfo);
      if (ulCluster == pVolInfo->ulFatEof)
         return pVolInfo->ulFatEof;
      ulReturn = ulCluster;
      ulNext = pVolInfo->ulFatEof;
      }

   else if (ulNext == FAT_ASSIGN_NEW)
      {
      /*
         An existing chain is extended
      */
      ulNext = SetNextCluster(pVolInfo, FAT_ASSIGN_NEW, pVolInfo->ulFatEof);
      if (ulNext == pVolInfo->ulFatEof)
         return pVolInfo->ulFatEof;
      ulReturn = ulNext;
      }

   /* Trick to avoid deadlock in WriteSector */
   fClean = pVolInfo->fDiskClean;
   pVolInfo->fDiskClean = FALSE;

#ifdef EXFAT
   if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
      {
      // mark cluster in exFAT allocation bitmap
      MarkCluster(pVolInfo, ulCluster, (BOOL)ulNext);
      }
#endif

   if (GetFatAccess(pVolInfo, "SetNextCluster"))
      {
#ifdef EXFAT
      if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
         {
         // mark cluster in exFAT allocation bitmap
         MarkCluster(pVolInfo, ulCluster, FALSE);
         }
#endif
      return pVolInfo->ulFatEof;
      }

   if (ReadFatSector(pVolInfo, GetFatEntrySec(pVolInfo, ulCluster)))
      {
#ifdef EXFAT
      if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
         {
         // mark cluster in exFAT allocation bitmap
         MarkCluster(pVolInfo, ulCluster, FALSE);
         }
#endif
      return pVolInfo->ulFatEof;
      }

   fUpdateFSInfo = FALSE;
   ulNewCluster = GetFatEntry(pVolInfo, ulCluster);
   if (ulNewCluster && !ulNext)
      {
      fUpdateFSInfo = TRUE;
      pVolInfo->pBootFSInfo->ulFreeClusters++;
      }
   if (ulNewCluster == 0 && ulNext)
      {
      fUpdateFSInfo = TRUE;
      //pVolInfo->pBootFSInfo->ulNextFreeCluster = ulCluster;
      pVolInfo->pBootFSInfo->ulNextFreeCluster = ulCluster + 1;
      pVolInfo->pBootFSInfo->ulFreeClusters--;
      }
   SetFatEntry(pVolInfo, ulCluster, ulNext);

   rc = WriteFatSector(pVolInfo, GetFatEntrySec(pVolInfo, ulCluster));
   pVolInfo->fDiskClean = fClean;
   if (rc)
      {
      ReleaseFat(pVolInfo);
#ifdef EXFAT
      if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
         {
         // mark cluster in exFAT allocation bitmap
         MarkCluster(pVolInfo, ulCluster, FALSE);
         }
#endif
      return pVolInfo->ulFatEof;
      }
/*
   if (fUpdateFSInfo)
      UpdateFSInfo(pVolInfo);
*/

   ReleaseFat(pVolInfo);
   return ulReturn;
}


/******************************************************************
*
******************************************************************/
ULONG GetFreeCluster(PVOLINFO pVolInfo)
{
ULONG ulStartCluster;
ULONG ulCluster;
BOOL fStartAt2;

   MessageL(LOG_FUNCS, "GetFreeCluster%m", 0x0050);

   if (pVolInfo->pBootFSInfo->ulFreeClusters == 0L)
      return pVolInfo->ulFatEof;

   fStartAt2 = FALSE;
   //ulCluster = pVolInfo->pBootFSInfo->ulNextFreeCluster + 1;
   ulCluster = pVolInfo->pBootFSInfo->ulNextFreeCluster;
   if (!ulCluster || ulCluster >= pVolInfo->ulTotalClusters + 2)
      {
      fStartAt2 = TRUE;
      ulCluster = 2;
      ulStartCluster = pVolInfo->ulTotalClusters + 2;
      }
   else
      ulStartCluster = ulCluster;

   //while (GetNextCluster2(pVolInfo, NULL, ulCluster))
   while (ClusterInUse(pVolInfo, ulCluster))
      {
      ulCluster++;
      if (fStartAt2 && ulCluster >= ulStartCluster)
         return pVolInfo->ulFatEof;

      if (ulCluster >= pVolInfo->ulTotalClusters + 2)
         {
         if (!fStartAt2)
            {
            ulCluster = 2;
            fStartAt2 = TRUE;
            }
         else
            return pVolInfo->ulFatEof;
         }
      }
   return ulCluster;
}

/******************************************************************
*
******************************************************************/
PVOLINFO GetVolInfo(USHORT hVBP)
{
INT rc;
struct vpfsi far * pvpfsi;
struct vpfsd far * pvpfsd;
PVOLINFO pVolInfo;
//PULONG p;

   rc = FSH_GETVOLPARM(hVBP, &pvpfsi, &pvpfsd);
   if (rc)
      {
      FatalMessage("FAT32:GetVolInfo - Volinfo not found!");
      return pGlobVolInfo;
      }

   pVolInfo = *((PVOLINFO *)(pvpfsd->vpd_work));  /* Get the pointer to the FAT32 Volume structure from the original block */

   if (! pVolInfo)
      return NULL;

   // prevent blocking in FSH_SETVOLUME when remounting after format
   /* if (! pVolInfo->fFormatInProgress && ! pVolInfo->fRemovable)
      {
      // check if volume is present
      Message("FSH_SETVOLUME");
      rc = FSH_SETVOLUME(hVBP, 0);

      if (rc == ERROR_VOLUME_CHANGED)
         return NULL;
      } */

   rc = MY_PROBEBUF(PB_OPWRITE, (PBYTE)pVolInfo, sizeof (VOLINFO));
   if (rc)
      {
      FatalMessage("FAT32: Error VOLINFO invalid in GetVolInfo (ProtViol)");
      return pGlobVolInfo;
      }
   pVolInfo->hVBP = hVBP;
   return pVolInfo;
}

PVOLINFO GetVolInfoCD(struct cdfsi *pcdfsi, struct cdfsd *pcdfsd)
{
PSZ pszCurDir = pcdfsi->cdi_curdir;
POPENINFO pOpenInfo;

   if (pszCurDir[1] == ':')
      pszCurDir += 2;

   if (*pszCurDir == '\\')
      return GetVolInfo(pcdfsi->cdi_hVPB);

   pOpenInfo = (POPENINFO)*((PULONG)pcdfsd + 1);

   return pOpenInfo->pVolInfo;
}

PVOLINFO GetVolInfoSF(struct sffsd *psffsd)
{
POPENINFO pOpenInfo;

   pOpenInfo = *(POPENINFO *)psffsd;
   return pOpenInfo->pVolInfo;
}

PVOLINFO GetVolInfoFS(struct fsfsd *pfsfsd)
{
PVOLINFO pVolInfo;

   pVolInfo = ((PFINDINFO)pfsfsd)->pVolInfo;
   return pVolInfo;
}

PVOLINFO GetVolInfoX(char *pszName)
{
PVOLINFO pVolInfo = NULL;
PVOLINFO pNext = pGlobVolInfo;
APIRET rc;

   do
      {
      if (pNext && pNext->pbMntPoint && strstr(strlwr(pszName), strlwr(pNext->pbMntPoint)))
         break;
      if (pNext && pNext->pbFilename && strstr(strlwr(pszName), strlwr(pNext->pbFilename)))
         break;
      pNext = pNext->pNextVolInfo;
      }
   while (pNext);

   pVolInfo = pNext;

   if (! pVolInfo)
      {
      return NULL;
      }

   rc = MY_PROBEBUF(PB_OPWRITE, (PBYTE)pVolInfo, sizeof (VOLINFO));
   if (rc)
      {
      FatalMessage("FAT32: Error VOLINFO invalid in GetVolInfo (ProtViol)");
      return pGlobVolInfo;
      }

   return pVolInfo;
}

ULONG GetVolDevice(PVOLINFO pVolInfo)
{
INT rc;
struct vpfsi far * pvpfsi;
struct vpfsd far * pvpfsd;

   rc = FSH_GETVOLPARM(pVolInfo->hVBP, &pvpfsi, &pvpfsd);
   if (rc)
      return 0L;
   return pvpfsi->vpi_hDEV;
}

/******************************************************************
*
******************************************************************/
USHORT GetProcInfo(PPROCINFO pProcInfo, USHORT usSize)
{
USHORT rc;

   memset(pProcInfo, 0xFF, usSize);
   rc = FSH_QSYSINFO(2, (PBYTE)pProcInfo, 6);
   if (rc)
      FatalMessage("FAT32: GetProcInfo failed, rc = %d", rc);
   return rc;
}

/******************************************************************
*
******************************************************************/
BOOL RemoveFindEntry(PVOLINFO pVolInfo, PFINFO pFindInfo)
{
PFINFO pNext;
USHORT rc;

   MessageL(LOG_FUNCS, "RemoveFindEntry%m", 0x0051);

   if (pVolInfo->pFindInfo == pFindInfo)
      {
      pVolInfo->pFindInfo = pFindInfo->pNextEntry;
      return TRUE;
      }

   pNext = (PFINFO)pVolInfo->pFindInfo;
   while (pNext)
      {
      rc = MY_PROBEBUF(PB_OPWRITE, (PBYTE)pNext, sizeof (FINDINFO));
      if (rc)
         {
         //CritMessage("FAT32: Protection VIOLATION in RemoveFindEntry! (SYS%d)", rc);
         Message("FAT32: Protection VIOLATION in RemoveFindEntry! (SYS%d)", rc);
         return FALSE;
         }

      if (pNext->pNextEntry == pFindInfo)
         {
         pNext->pNextEntry = pFindInfo->pNextEntry;
         return TRUE;
         }
      pNext = (PFINFO)pNext->pNextEntry;
      }
   //CritMessage("FAT32: RemoveFindEntry: Entry not found!");
   Message("ERROR: RemoveFindEntry: Entry not found!");
   return FALSE;
}

/******************************************************************
*
******************************************************************/
#ifdef __WATCOM
_WCRTLINK int sprintf(char * pszBuffer, const char * pszFormat, ...)
#else
int cdecl sprintf(char * pszBuffer, const char *pszFormat, ...)
#endif
{
va_list va;
int ret;

   va_start(va, pszFormat);
   ret = vsprintf(pszBuffer, pszFormat, va);
   va_end( va);
   return ret;
}

/******************************************************************
*
******************************************************************/
USHORT cdecl CritMessage(PSZ pszMessage, ...)
{
static BYTE szMessage[512];
va_list va;

   memset(szMessage, 0, sizeof szMessage);
   va_start(va, pszMessage);
   vsprintf(szMessage, pszMessage, va);
   va_end(va);
   strcat(szMessage, "\r\n");
   return FSH_CRITERROR(strlen(szMessage) + 1, szMessage, 0, "", CE_ALLABORT | CE_ALLRETRY);
}

VOID cdecl FatalMessage(PSZ pszMessage, ...)
{
static BYTE szMessage[512];
va_list va;

   memset(szMessage, 0, sizeof szMessage);
   va_start(va, pszMessage);
   vsprintf(szMessage, pszMessage, va);
   va_end(va);
   strcat(szMessage, "\r\n");
   FSH_INTERR(szMessage, strlen(szMessage) + 1);
}

VOID InternalError(PSZ pszMessage)
{
   DevHelp_InternalError(pszMessage, strlen(pszMessage) + 1);
}

/******************************************************************
*
******************************************************************/
#ifdef __WATCOM
_WCRTLINK int vsprintf(char * pszBuffer, const char * pszFormat, va_list va)
#else
int cdecl vsprintf(char * pszBuffer, const char * pszFormat, va_list va)
#endif
{
PSZ p;
BOOL fLong = FALSE;
BOOL fLongLong = FALSE;
ULONGLONG ullValue;
ULONG ulValue;
USHORT usValue;
PSZ   pszValue;
BYTE  bToken;


   for (;;)
      {
      p = strchr(pszFormat, '%');
      if (!p)
         break;
      strncpy(pszBuffer, pszFormat, p - pszFormat);
      pszBuffer[p - pszFormat] = 0;
      pszBuffer += strlen(pszBuffer);
      pszFormat = p;
      p++;
      fLong = FALSE;
      fLongLong = FALSE;
      if (*p == 'l')
         {
         if (p[1] == 'l')
            {
            fLongLong = TRUE;
            p += 2;
            }
            else
            {
            fLong = TRUE;
            p++;
            }
         }
      bToken = *p;
      if (*p)
         p++;

      switch (bToken)
         {
         case '%':
            strcpy(pszBuffer, "%");
            break;
         case 'd':
         case 'u':
            if (fLongLong)
               {
               ullValue = va_arg(va, ULONGLONG);
               }
            else if (fLong)
               {
               ulValue = va_arg(va, unsigned long);
#ifdef INCL_LONGLONG
               ullValue = ulValue;
#else
               AssignUL(&ullValue, ulValue);
#endif
               }
            else
               {
               usValue = va_arg(va, unsigned short);
#ifdef INCL_LONGLONG
               ullValue = usValue;
#else
               AssignUS(&ullValue, usValue);
#endif
               }
            if (bToken == 'u')
               ulltoa(ullValue, pszBuffer, 10);
            else
               lltoa(*(PLONGLONG)&ullValue, pszBuffer, 10);
            break;
         case 'x':
         case 'X':
            if (fLongLong)
               {
               ullValue = va_arg(va, ULONGLONG);
               }
            else if (fLong)
               {
               ulValue = va_arg(va, unsigned long);
#ifdef INCL_LONGLONG
               ullValue = ulValue;
#else
               AssignUL(&ullValue, ulValue);
#endif
               }
            else
               {
               usValue = va_arg(va, unsigned short);
#ifdef INCL_LONGLONG
               ullValue = usValue;
#else
               AssignUS(&ullValue, usValue);
#endif
               }
            ulltoa(ullValue, pszBuffer, 16);
            if (bToken == 'X')
               strupr(pszBuffer);
            break;
         case 's':
            pszValue = va_arg(va, char *);
            if (pszValue)
               {
               if (MY_PROBEBUF(PB_OPREAD, pszValue, 1))
                  strcpy(pszBuffer, "(bad address)");
               else
                  strcpy(pszBuffer, pszValue);
               }
            else
               strcpy(pszBuffer, "(null)");
            break;
         case 'c':
            pszBuffer[0] = (char)va_arg(va, USHORT);
            pszBuffer[1] = 0;
            break;
         case 'm':
            // minor ID of system trace, just skip it
            usValue = va_arg(va, USHORT);
            break;
         default :
            strncpy(pszBuffer, pszFormat, p - pszFormat);
            pszBuffer[p - pszFormat] = 0;
            break;
         }
      pszBuffer += strlen(pszBuffer);
      pszFormat = p;
      }
   strcpy(pszBuffer, pszFormat);
   return 1;
}

/******************************************************************
*
******************************************************************/
BYTE GetVFATCheckSum(PDIRENTRY pDir)
{
BYTE bCheck;
INT  iIndex;

   bCheck = 0;
   for (iIndex = 0; iIndex < 11; iIndex++)
      {
      if (bCheck & 0x01)
         {
         bCheck >>=1;
         bCheck |= 0x80;
         }
      else
         bCheck >>=1;
      bCheck += pDir->bFileName[iIndex];
      }

   return bCheck;

}

/******************************************************************
*
******************************************************************/
PDIRENTRY fSetLongName(PDIRENTRY pDir, PSZ pszLongName, BYTE bCheck)
{
// FAT12/FAT16/FAT32 case
USHORT usNeededEntries;
PDIRENTRY pRet;
BYTE bCurEntry;
PLNENTRY pLN;
USHORT usIndex;
USHORT uniName[13];
USHORT uniEnd[13];
PUSHORT p;

   if (!pszLongName || !strlen(pszLongName))
      return pDir;

#if 0
   usNeededEntries = strlen(pszLongName) / 13 +
      (strlen(pszLongName) % 13 ? 1 : 0);
#else
   usNeededEntries = ( DBCSStrlen( pszLongName ) + 12 ) / 13;
#endif

   if (!usNeededEntries)
      return pDir;

   pDir += (usNeededEntries - 1);
   pRet = pDir + 1;
   pLN = (PLNENTRY)pDir;

   bCurEntry = 1;
   while (*pszLongName)
      {
#if 0
      USHORT usLen;
#endif
      pLN->bNumber = bCurEntry;
      if (DBCSStrlen(pszLongName) <= 13)
         pLN->bNumber += 0x40;
      pLN->wCluster = 0L;
      pLN->bAttr = FILE_LONGNAME;
      pLN->bReserved = 0;
      pLN->bVFATCheckSum = bCheck;

#if 0
      usLen = strlen(pszLongName);
      if (usLen > 13)
         usLen = 13;
#endif

      memset(uniEnd, 0xFF, sizeof uniEnd);
      memset(uniName, 0, sizeof uniName);

      pszLongName += Translate2Win(pszLongName, uniName, 13);

      p = uniName;
      for (usIndex = 0; usIndex < 5; usIndex ++)
         {
         pLN->usChar1[usIndex] = *p;
         if (*p == 0)
            p = uniEnd;
         p++;
         }

      for (usIndex = 0; usIndex < 6; usIndex ++)
         {
         pLN->usChar2[usIndex] = *p;
         if (*p == 0)
            p = uniEnd;
         p++;
         }

      for (usIndex = 0; usIndex < 2; usIndex ++)
         {
         pLN->usChar3[usIndex] = *p;
         if (*p == 0)
            p = uniEnd;
         p++;
         }

      pLN--;
      bCurEntry++;
      }

   return pRet;
}

#ifdef EXFAT

/******************************************************************
*
******************************************************************/
PDIRENTRY1 fSetLongName1(PDIRENTRY1 pDir, PSZ pszLongName, PUSHORT pusNameHash)
{
// exFAT case
USHORT usNeededEntries;
BYTE bCurEntry;
PDIRENTRY1 pLN;
USHORT usIndex;
//UCHAR  szLongName1[FAT32MAXPATH];
PSZ pszLongName1;
PSZ pszLongName2 = NULL;
//USHORT pusUniName[256];
PUSHORT pusUniName = NULL;
USHORT uniName[15];
USHORT uniName1[15];
PUSHORT p, q, p1;
PSZ     r;

   if (!pszLongName || !strlen(pszLongName))
      return pDir;

   pszLongName2 = (PSZ)malloc((size_t)FAT32MAXPATH);
   if (!pszLongName2)
      return pDir;

   pszLongName1 = pszLongName2;

   pusUniName = (PUSHORT)malloc(256 * sizeof(USHORT));
   if (!pusUniName)
      {
      free(pszLongName2);
      return pDir;
      }

   // @todo Use upcase table
   strcpy(pszLongName1, pszLongName);
   FSH_UPPERCASE(pszLongName1, FAT32MAXPATH, pszLongName1);

   usNeededEntries = ( DBCSStrlen( pszLongName ) + 14 ) / 15;

   if (!usNeededEntries)
      {
      free(pszLongName2);
      free(pusUniName);
      return pDir;
      }

   pLN = (PDIRENTRY1)pDir;
   q = pusUniName;
   r = pszLongName;

   bCurEntry = 1;
   while (*pszLongName)
      {
      pLN->bEntryType = ENTRY_TYPE_FILE_NAME;
      pLN->u.FileName.bAllocPossible = 0;
      pLN->u.FileName.bNoFatChain = 0;
      pLN->u.FileName.bCustomDefined1 = 0;

      memset(uniName, 0, sizeof uniName);
      memset(uniName1, 0, sizeof uniName1);

      pszLongName += Translate2Win(pszLongName, uniName, 15);
      pszLongName1 += Translate2Win(pszLongName1, uniName1, 15);

      p = uniName;
      p1 = uniName1;

      for (usIndex = 0; usIndex < 15; usIndex ++)
         {
         pLN->u.FileName.bFileName[usIndex] = *p++;
         *q++ = *p1++;
         }

      pLN++;
      bCurEntry++;
      }

   if (pusNameHash)
      *pusNameHash = NameHash(pusUniName, DBCSStrlen(r));


   if (pusUniName)
      free(pusUniName);
   if (pszLongName2)
      free(pszLongName2);

   return pLN;
}

#endif

/******************************************************************
*
******************************************************************/
USHORT MakeDirEntry(PVOLINFO pVolInfo, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo,
                    PDIRENTRY pNew, PDIRENTRY1 pNewStream, PSZ pszName)
{
   MessageL(LOG_FUNCS, "MakeDirEntry%m %s", 0x0052, pszName);

   if (pGI)
      {
#ifdef EXFAT
      if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
         {
#endif
         pNew->wLastWriteDate.year = pGI->year - 1980;
         pNew->wLastWriteDate.month = pGI->month;
         pNew->wLastWriteDate.day = pGI->day;
         pNew->wLastWriteTime.hours = pGI->hour;
         pNew->wLastWriteTime.minutes = pGI->minutes;
         pNew->wLastWriteTime.twosecs = pGI->seconds / 2;

         pNew->wCreateDate = pNew->wLastWriteDate;
         pNew->wCreateTime = pNew->wLastWriteTime;
         pNew->wAccessDate = pNew->wLastWriteDate;
#ifdef EXFAT
         }
      else
         {
         PDIRENTRY1 pNew1 = (PDIRENTRY1)pNew;
         pNew1->u.File.ulLastModifiedTimestp.year = pGI->year - 1980;
         pNew1->u.File.ulLastModifiedTimestp.month = pGI->month;
         pNew1->u.File.ulLastModifiedTimestp.day = pGI->day;
         pNew1->u.File.ulLastModifiedTimestp.hour = pGI->hour;
         pNew1->u.File.ulLastModifiedTimestp.minutes = pGI->minutes;
         pNew1->u.File.ulLastModifiedTimestp.twosecs = pGI->seconds / 2;

         pNew1->u.File.ulCreateTimestp = pNew1->u.File.ulLastModifiedTimestp;
         pNew1->u.File.ulLastAccessedTimestp = pNew1->u.File.ulLastModifiedTimestp;
         pNew1->u.File.bCreate10msIncrement = 0;
         pNew1->u.File.bLastModified10msIncrement = 0;
         pNew1->u.File.bCreateTimezoneOffset = 0;
         pNew1->u.File.bLastModifiedTimezoneOffset = 0;
         pNew1->u.File.bLastAccessedTimezoneOffset = 0;
         memset(pNew1->u.File.bResvd2, 0, sizeof(pNew1->u.File.bResvd2));
         }
#endif
      }

   return ModifyDirectory(pVolInfo, ulDirCluster, pDirSHInfo, MODIFY_DIR_INSERT,
      NULL, pNew, NULL, pNewStream, pszName, pszName, 0);
}



/******************************************************************
*
******************************************************************/
USHORT MakeShortName(PVOLINFO pVolInfo, ULONG ulDirCluster, ULONG ulFileNo,
                     PSZ pszLongName, PSZ pszShortName)
{
USHORT usLongName;
PSZ pLastDot;
PSZ pFirstDot;
PSZ p;
USHORT usIndex;
BYTE szShortName[12];
BYTE szFileName[25];
PSZ  pszUpper;
USHORT rc;

   MessageL(LOG_FUNCS, "MakeShortName%m for %s, dircluster %lu",
            0x0053, pszLongName, ulDirCluster);

   usLongName = LONGNAME_OFF;
   memset(szShortName, 0x20, 11);
   szShortName[11] = 0;

   /*
      Uppercase the longname
   */
   usIndex = strlen(pszLongName) + 5;
   pszUpper = malloc(usIndex);
   if (!pszUpper)
      return LONGNAME_ERROR;

   rc = FSH_UPPERCASE(pszLongName, usIndex, pszUpper);
   if (rc)
      {
      free(pszUpper);
      return LONGNAME_ERROR;
      }

   /* Skip all leading dots */
   p = pszUpper;
   while (*p == '.')
      p++;

   pLastDot  = strrchr(p, '.');
   pFirstDot = strchr(p, '.');

   if (!pLastDot)
      pLastDot = pszUpper + strlen(pszUpper);
   if (!pFirstDot)
      pFirstDot = pLastDot;

   /*
      Is the name a valid 8.3 name ?
   */
   if ((!strcmp(pszLongName, pszUpper) || IsDosSession()) &&
      pFirstDot == pLastDot &&
      pLastDot - pszUpper <= 8 &&
      strlen(pLastDot) <= 4)
      {
      PSZ p = pszUpper;

      if (*p != '.')
         {
         while (*p)
            {
            if (*p < 128 && !strchr(rgValidChars, *p) && *p != '.')
               break;
            p++;
            }
         }

      if (!(*p))
         {
         memset(szShortName, 0x20, sizeof szShortName);
         szShortName[11] = 0;
         memcpy(szShortName, pszUpper, pLastDot - pszUpper);
         if (*pLastDot)
            memcpy(szShortName + 8, pLastDot + 1, strlen(pLastDot + 1));

         memcpy(pszShortName, szShortName, 11);
         free(pszUpper);
         if (ulFileNo != 0xffffffff)
            {
            p = szShortName + 7;
            while (*p == ' ') p--;
            p++;
            memset(szFileName, 0, sizeof(szFileName));
            memcpy(szFileName, szShortName, p - szShortName);
            if (memcmp(szShortName + 8, "   ", 3))
               {
               szFileName[p - szShortName] = '.';
               memcpy(szFileName + (p - szShortName) + 1, szShortName + 8, 3);
               }
            strcpy(pszShortName, szFileName);
            return usLongName;
            }
         return usLongName;
         }
      }

#if 0
   if (IsDosSession())
      {
      free(pszUpper);
      return LONGNAME_ERROR;
      }
#endif

   usLongName = LONGNAME_OK;

   if (pLastDot - pszUpper > 8)
      usLongName = LONGNAME_MAKE_UNIQUE;

   szShortName[11] = 0;

   usIndex = 0;
   p = pszUpper;
   while (usIndex < 11)
      {
      if (!(*p))
         break;

      if (usIndex == 8 && p <= pLastDot)
         {
         if (p < pLastDot)
            usLongName = LONGNAME_MAKE_UNIQUE;
         if (*pLastDot)
            p = pLastDot + 1;
         else
            break;
         }

      while (*p == 0x20)
         {
         usLongName = LONGNAME_MAKE_UNIQUE;
         p++;
         }
      if (!(*p))
         break;

      if (*p >= 128)
         {
         szShortName[usIndex++] = *p;
         }
      else if (*p == '.')
         {
         /*
            Skip all dots, if multiple dots are in the
            name create an unique name
         */
         if (p == pLastDot)
            usIndex = 8;
         else
            usLongName = LONGNAME_MAKE_UNIQUE;
         }
      else if (strchr(rgValidChars, *p))
         szShortName[usIndex++] = *p;
      else
         {
         szShortName[usIndex++] = '_';
         usLongName = LONGNAME_MAKE_UNIQUE;
         }
      p++;
      }
   if (strlen(p))
      usLongName = LONGNAME_MAKE_UNIQUE;

   free(pszUpper);

   p = szShortName;
   for( usIndex = 0; usIndex < 8; usIndex++ )
      if( IsDBCSLead( p[ usIndex ]))
         usIndex++;

   if( usIndex > 8 )
      p[ 7 ] = 0x20;

   p = szShortName + 8;
   for( usIndex = 0; usIndex < 3; usIndex++ )
      if( IsDBCSLead( p[ usIndex ]))
         usIndex++;

   if( usIndex > 3 )
      p[ 2 ] = 0x20;

   if (usLongName == LONGNAME_MAKE_UNIQUE)
      {
      USHORT usNum;
      BYTE   szNumber[18];
      ULONG ulCluster;
      PSZ p;
      USHORT usPos1, usPos2;

      for (usPos1 = 8; usPos1 > 0; usPos1--)
         if (szShortName[usPos1 - 1] != 0x20)
            break;

      for (usNum = 1; usNum < 32000; usNum++)
         {
         memset(szFileName, 0, sizeof szFileName);
         memcpy(szFileName, szShortName, 8);

         /*
            Find last blank in filename before dot.
         */

         memset(szNumber, 0, sizeof szNumber);

         if (ulFileNo != 0xffffffff)
            // if DirEntry number is specified (non-zero), use it
            itoa((USHORT)ulFileNo, szNumber, 16);
         else
            itoa(usNum, szNumber, 10);

         p = szNumber;
         while (*p) *p++ = (char)toupper(*p);

         usPos2 = 7 - (strlen(szNumber));
         if (usPos1 && usPos1 < usPos2)
            usPos2 = usPos1;

         for( usIndex = 0; usIndex < usPos2; usIndex++ )
            if( IsDBCSLead( szShortName[ usIndex ]))
               usIndex++;

         if( usIndex > usPos2 )
            usPos2--;

         strcpy(szFileName + usPos2, "~");
         strcat(szFileName, szNumber);

         if (memcmp(szShortName + 8, "   ", 3))
            {
            strcat(szFileName, ".");
            memcpy(szFileName + strlen(szFileName), szShortName + 8, 3);
            p = szFileName + strlen(szFileName);
            while (p > szFileName && *(p-1) == 0x20)
               p--;
            *p = 0;
            }
         if (ulFileNo != 0xffffffff)
            {
            // if DirEntry number is specified, use it as NNN in file~NNN.ext,
            // no need to try over all values < 32000
            break;
            }
         else
            {
            ulCluster = FindPathCluster(pVolInfo, ulDirCluster, szFileName, NULL, NULL, NULL, NULL);
            if (ulCluster == pVolInfo->ulFatEof)
               {
               break;
               }
            }
         }
      if ( (ulFileNo == 0xffffffff && usNum < 32000) || 
           (ulFileNo != 0xffffffff && ulFileNo < 32000) )
         {
         p = strchr(szFileName, '.');
#if 0
         if (p && p - szFileName < 8)
            memcpy(szShortName, szFileName, p - szFileName);
         else
            memccpy(szShortName, szFileName, 0, 8 );
         }
#else
         if( !p )
            p = szFileName + strlen( szFileName );

         memcpy(szShortName, szFileName, p - szFileName);
         memset( szShortName + ( p - szFileName ), 0x20, 8 - ( p - szFileName ));
#endif
         }
      else
         {
         return LONGNAME_ERROR;
         }
      }

   if (ulFileNo != 0xffffffff)
      {
      strcpy(pszShortName, szFileName);
      }
   else
      {
      memcpy(pszShortName, szShortName, 11);
      }
   return usLongName;
}

/******************************************************************
*
******************************************************************/
BOOL DeleteFatChain(PVOLINFO pVolInfo, ULONG ulCluster)
{
ULONG ulNextCluster = 0;
ULONG ulSector = 0;
ULONG ulBmpSector = 0;
ULONG ulClustersFreed;
USHORT rc;

   if (!ulCluster)
      return TRUE;

   if (ulCluster >= 2 && ulCluster < pVolInfo->ulTotalClusters + 2)
      {
      MessageL(LOG_FUNCS, "DeleteFatChain%m for cluster %lu", 0x0054, ulCluster);
      }
   else
      {
      MessageL(LOG_FUNCS, "DeleteFatChain%m for invalid cluster %lu (ERROR)", 0x0055, ulCluster);
      return FALSE;
      }

   if (!GetFatAccess(pVolInfo, "DeleteFatChain"))
      {
      ulSector = GetFatEntrySec(pVolInfo, ulCluster);
      ReadFatSector(pVolInfo, ulSector);
      ReleaseFat(pVolInfo);
      }

#ifdef EXFAT
   if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
      {
      if (!GetFatAccess(pVolInfo, "DeleteFatChain"))
         {
         ulBmpSector = GetAllocBitmapSec(pVolInfo, ulCluster);
         ReadBmpSector(pVolInfo, ulBmpSector);
         ReleaseFat(pVolInfo);
         }
      }
#endif

   ulClustersFreed = 0;
   while (!(ulCluster >= pVolInfo->ulFatEof2 && ulCluster <= pVolInfo->ulFatEof))
      {
#ifdef CALL_YIELD
      Yield();
#endif

      if (!ulCluster || ulCluster == pVolInfo->ulFatBad)
         {
         Message("DeleteFatChain: Bad Chain (Cluster %lu)",
            ulCluster);
         break;
         }

      if (!GetFatAccess(pVolInfo, "DeleteFatChain"))
         {
         ulSector = GetFatEntrySec(pVolInfo, ulCluster);
         if (ulSector != pVolInfo->ulCurFatSector)
            {
            rc = WriteFatSector(pVolInfo, pVolInfo->ulCurFatSector);

            if (rc)
               return FALSE;
            ReadFatSector(pVolInfo, ulSector);
            }
         ulNextCluster = GetFatEntry(pVolInfo, ulCluster);
         SetFatEntry(pVolInfo, ulCluster, 0L);
         ReleaseFat(pVolInfo);
         }

#ifdef EXFAT
      if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
         {
         if (!GetFatAccess(pVolInfo, "DeleteFatChain"))
            {
            // modify exFAT allocation bitmap as well
            ulBmpSector = GetAllocBitmapSec(pVolInfo, ulCluster);
            if (ulBmpSector != pVolInfo->ulCurBmpSector)
               {
               rc = WriteBmpSector(pVolInfo, pVolInfo->ulCurBmpSector);

               if (rc)
                  return FALSE;
               ReadBmpSector(pVolInfo, ulBmpSector);
               }
            SetBmpEntry(pVolInfo, ulCluster, 0);
            ReleaseFat(pVolInfo);
            }
         }
#endif

      ulClustersFreed++;
      ulCluster = ulNextCluster;
      }

   if (!GetFatAccess(pVolInfo, "DeleteFatChain"))
      {
      rc = WriteFatSector(pVolInfo, pVolInfo->ulCurFatSector);

      if (rc)
         return FALSE;

#ifdef EXFAT
      if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
         {
         rc = WriteBmpSector(pVolInfo, pVolInfo->ulCurBmpSector);

         if (rc)
            return FALSE;
         }
#endif
      ReleaseFat(pVolInfo);
      }

   pVolInfo->pBootFSInfo->ulFreeClusters += ulClustersFreed;
/*   UpdateFSInfo(pVolInfo);*/

   return TRUE;
}

ULONG SeekToCluster(PVOLINFO pVolInfo, PSHOPENINFO pSHInfo, ULONG ulCluster, LONGLONG llPosition)
{
ULONG  ulSector = 0;

   MessageL(LOG_FUNCS, "SeekToCluster%m", 0x0056);

#ifdef EXFAT
   if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
      {
      if (pSHInfo->fNoFatChain & 1)
         {
#ifdef INCL_LONGLONG
         while (llPosition >= (LONGLONG)pVolInfo->ulClusterSize)
            {
            llPosition -= pVolInfo->ulClusterSize;
            ulCluster++;
            }
#else
         while (iGreaterEUL(llPosition, pVolInfo->ulClusterSize))
            {
            llPosition = iSubUL(llPosition, pVolInfo->ulClusterSize);
            ulCluster++;
            }
#endif
         if (ulCluster <= pSHInfo->ulLastCluster)
            return ulCluster;
         else
            return pVolInfo->ulFatEof;
         }
      }
#endif

   while (ulCluster != pVolInfo->ulFatEof &&
#ifdef INCL_LONGLONG
          llPosition >= (LONGLONG)pVolInfo->ulClusterSize)
#else
          iGreaterEUL(llPosition, pVolInfo->ulClusterSize))
#endif
      {
      if (GetFatAccess(pVolInfo, "SeekToCluster"))
         return pVolInfo->ulFatEof;

      ulSector = GetFatEntrySec(pVolInfo, ulCluster);

      if (ulSector != pVolInfo->ulCurFatSector)
         ReadFatSector(pVolInfo, ulSector);

      ulCluster = GetFatEntry(pVolInfo, ulCluster);

      if (ulCluster >= pVolInfo->ulFatEof2 && ulCluster <= pVolInfo->ulFatEof)
         ulCluster = pVolInfo->ulFatEof;

#ifdef INCL_LONGLONG
      llPosition -= pVolInfo->ulClusterSize;
#else
      llPosition = iSubUL(llPosition, pVolInfo->ulClusterSize);
#endif
      ReleaseFat(pVolInfo);
      }

   return ulCluster;
}

#ifdef EXFAT

void SetSHInfo1(PVOLINFO pVolInfo, PDIRENTRY1 pStreamEntry, PSHOPENINFO pSHInfo)
{
   memset(pSHInfo, 0, sizeof(PSHOPENINFO));
   pSHInfo->fNoFatChain = pStreamEntry->u.Stream.bNoFatChain & 1;
   pSHInfo->ulStartCluster = pStreamEntry->u.Stream.ulFirstClus;
   pSHInfo->ulLastCluster = GetLastCluster(pVolInfo, pStreamEntry->u.Stream.ulFirstClus, pStreamEntry);
}

#endif

ULONG GetLastCluster(PVOLINFO pVolInfo, ULONG ulCluster, PDIRENTRY1 pDirEntryStream)
{
ULONG  ulReturn = 0;

   MessageL(LOG_FUNCS, "GetLastCluster%m", 0x0057);

   if (!ulCluster)
      return pVolInfo->ulFatEof;

#ifdef EXFAT
   if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
      {
      if (pDirEntryStream->u.Stream.bNoFatChain & 1)
         {
#ifdef INCL_LONGLONG
         return ulCluster + (pDirEntryStream->u.Stream.ullValidDataLen / pVolInfo->ulClusterSize) +
             ((pDirEntryStream->u.Stream.ullValidDataLen % pVolInfo->ulClusterSize) ? 1 : 0) - 1;
#else
         {
         ULONGLONG ullLen;
         ullLen = ModUL(pDirEntryStream->u.Stream.ullDataLen, pVolInfo->ulClusterSize);

         if (GreaterUL(ullLen, 0))
            AddUL(ullLen, 1);

         return ulCluster + DivUL(pDirEntryStream->u.Stream.ullDataLen, pVolInfo->ulClusterSize).ulLo +
            ullLen.ulLo - 1;
         }
#endif
         }
      }
#endif

   //if (GetFatAccess(pVolInfo, "GetLastCluster"))
   //   return pVolInfo->ulFatEof;

   ulReturn = ulCluster;
   while (ulCluster != pVolInfo->ulFatEof)
      {
      ulReturn = ulCluster;
      ulCluster = GetNextCluster(pVolInfo, NULL, ulCluster);
      //ulSector = GetFatEntrySec(pVolInfo, ulCluster);

      //if (ulSector != pVolInfo->ulCurFatSector)
      //   ReadFatSector(pVolInfo, ulSector);

      //ulCluster = GetFatEntry(pVolInfo, ulCluster);

      if (ulCluster >= pVolInfo->ulFatEof2 && ulCluster <= pVolInfo->ulFatEof)
         break;
      if (! ulCluster)
         {
         Message("The file ends with NULL FAT entry!");
         break;
         }
      }
   //ReleaseFat(pVolInfo);
   return ulReturn;
}

USHORT CopyChain(PVOLINFO pVolInfo, PSHOPENINFO pSHInfo, ULONG ulCluster, PULONG pulNew)
{
USHORT rc;
ULONG  ulNext;
ULONG  ulNext2;
ULONG  ulCount;
ULONG  ulBlock;
PBYTE  pbCluster;
ULONG  tStart = GetCurTime();


   MessageL(LOG_FUNCS, "CopyChain%m, cluster %lu", 0x0058, ulCluster);

   if (!ulCluster)
      {
      *pulNew = 0L;
      return 0;
      }

   *pulNew = pVolInfo->ulFatEof;

   pbCluster = malloc((size_t)pVolInfo->ulBlockSize);
   if (!pbCluster)
      return ERROR_NOT_ENOUGH_MEMORY;

   ulCount = GetChainSize(pVolInfo, pSHInfo, ulCluster);
   if (!ulCount)
      {
      free(pbCluster);
      return 1340;
      }

   *pulNew = MakeFatChain(pVolInfo, pSHInfo, pVolInfo->ulFatEof, ulCount, NULL);
   if (*pulNew == pVolInfo->ulFatEof)
      {
      free(pbCluster);
      return ERROR_DISK_FULL;
      }

   ulNext = *pulNew;
   rc = 0;
   while (ulCluster != pVolInfo->ulFatEof && ulNext != pVolInfo->ulFatEof)
      {
      ULONG tNow = GetCurTime();
      if (tStart + 3 > tNow)
         {
         Yield();
         tStart = tNow;
         }

      for (ulBlock = 0; ulBlock < pVolInfo->ulClusterSize / pVolInfo->ulBlockSize; ulBlock++)
         {
         rc = ReadBlock(pVolInfo, ulCluster, ulBlock, pbCluster, DVIO_OPNCACHE);
         if (rc)
            break;
         rc = WriteBlock(pVolInfo, ulNext, ulBlock, pbCluster, 0);
         if (rc)
            break;
         }
      if (rc)
         break;
      ulCluster = GetNextCluster(pVolInfo, pSHInfo, ulCluster);
      if (!ulCluster)
         ulCluster = pVolInfo->ulFatEof;
      if (ulCluster == pVolInfo->ulFatEof)
         break;
      ulNext2 = GetNextCluster(pVolInfo, pSHInfo, ulNext);
      if (ulNext2 == pVolInfo->ulFatEof)
         ulNext2 = SetNextCluster(pVolInfo, ulNext, FAT_ASSIGN_NEW); //// should we pass pSHInfo to it too?
      ulNext = ulNext2;
      }
   if (ulCluster != pVolInfo->ulFatEof)
      rc = ERROR_DISK_FULL;

   free(pbCluster);
   if (rc)
      {
      DeleteFatChain(pVolInfo, *pulNew);
      *pulNew = pVolInfo->ulFatEof;
      }
   return rc;
}

ULONG GetChainSize(PVOLINFO pVolInfo, PSHOPENINFO pSHInfo, ULONG ulCluster)
{
ULONG ulCount;
ULONG ulSector;
USHORT usSectorsRead;
USHORT usSectorsPerBlock;

   MessageL(LOG_FUNCS, "GetChainSize%m", 0x0059);

   if (ulCluster == 1)
      {
      // FAT12/FAT16 root directory starting sector
      ulSector = pVolInfo->BootSect.bpb.ReservedSectors +
         (ULONG)pVolInfo->BootSect.bpb.SectorsPerFat * pVolInfo->BootSect.bpb.NumberOfFATs;
      usSectorsPerBlock = (USHORT)((ULONG)pVolInfo->SectorsPerCluster /
         ((ULONG)pVolInfo->ulClusterSize / pVolInfo->ulBlockSize));
      usSectorsRead = 0;
      }

   if (GetFatAccess(pVolInfo, "GetChainCount"))
      return 0L;

   ulCount = 0;
   while (ulCluster && ulCluster != pVolInfo->ulFatEof)
      {
      ulCount++;
      if (ulCluster == 1)
         {
         // reading the root directory in case of FAT12/FAT16
         ulSector += pVolInfo->SectorsPerCluster;
         usSectorsRead += pVolInfo->SectorsPerCluster;
         if ((ULONG)usSectorsRead * pVolInfo->BootSect.bpb.BytesPerSector >=
            (ULONG)pVolInfo->BootSect.bpb.RootDirEntries * sizeof(DIRENTRY))
            // root directory ended
            ulCluster = 0;
         }
      else
         ulCluster = GetNextCluster2(pVolInfo, pSHInfo, ulCluster);
      }
   ReleaseFat(pVolInfo);
   return ulCount;
}

VOID MarkFreeEntries(PDIRENTRY pDirBlock, ULONG ulSize)
{
PDIRENTRY pMax, pDirBlock2 = pDirBlock;

   pMax = (PDIRENTRY)((PBYTE)pDirBlock + ulSize);
   while (pDirBlock != pMax)
      {
      if (!pDirBlock->bFileName[0])
         pDirBlock->bFileName[0] = DELETED_ENTRY;
      pDirBlock++;
      }
}

#ifdef EXFAT

VOID MarkFreeEntries1(PDIRENTRY1 pDirBlock, ULONG ulSize)
{
PDIRENTRY1 pMax, pDirBlock2 = pDirBlock;

   pMax = (PDIRENTRY1)((PBYTE)pDirBlock + ulSize);
   while (pDirBlock != pMax)
      {
      if (pDirBlock->bEntryType == ENTRY_TYPE_EOD)
         // Mark to something non-zero without ENTRY_TYPE_IN_USE_STATUS bit
         pDirBlock->bEntryType = 0x05;
      pDirBlock++;
      }
}

#endif

USHORT GetFreeEntries(PDIRENTRY pDirBlock, ULONG ulSize)
{
USHORT usCount;
PDIRENTRY pMax, pDirBlock2 = pDirBlock;
BOOL bLoop;

   pMax = (PDIRENTRY)((PBYTE)pDirBlock + ulSize);
   usCount = 0;
   bLoop = pMax == pDirBlock;
   while (( pDirBlock != pMax ) || bLoop )
      {
      if (!pDirBlock->bFileName[0] || pDirBlock->bFileName[0] == DELETED_ENTRY)
         usCount++;
      bLoop = FALSE;
      pDirBlock++;
      }

   return usCount;
}

#ifdef EXFAT

USHORT GetFreeEntries1(PDIRENTRY1 pDirBlock, ULONG ulSize)
{
// GetFreeEntries version for exFAT
USHORT usCount;
PDIRENTRY1 pMax, pDirBlock2 = pDirBlock;
BOOL bLoop;

   pMax = (PDIRENTRY1)((PBYTE)pDirBlock + ulSize);
   usCount = 0;
   bLoop = pMax == pDirBlock;
   while (( pDirBlock != pMax ) || bLoop )
      {
      if ( pDirBlock->bEntryType == ENTRY_TYPE_EOD ||
           !(pDirBlock->bEntryType & ENTRY_TYPE_IN_USE_STATUS) )
         usCount++;
      bLoop = FALSE;
      pDirBlock++;
      }

   return usCount;
}

#endif

PDIRENTRY CompactDir(PDIRENTRY pStart, ULONG ulSize, USHORT usEntriesNeeded)
{
PDIRENTRY pTar, pMax, pFirstFree;
USHORT usFreeEntries;
BOOL bLoop;


   pMax = (PDIRENTRY)((PBYTE)pStart + ulSize);
   bLoop = pMax == pStart;
   pFirstFree = pMax;
   usFreeEntries = 0;
   while (( pFirstFree != pStart ) || bLoop )
      {
      if (!(pFirstFree - 1)->bFileName[0])
         usFreeEntries++;
      else
         break;
      bLoop = FALSE;
      pFirstFree--;
      }

   if ((( pFirstFree == pStart ) && !bLoop ) || (pFirstFree - 1)->bAttr != FILE_LONGNAME)
      if (usFreeEntries >= usEntriesNeeded)
         return pFirstFree;

   /*
      Leaving longname entries at the end
   */
   while ((( pFirstFree != pStart ) || bLoop ) && (pFirstFree - 1)->bAttr == FILE_LONGNAME)
   {
      bLoop = FALSE;
      pFirstFree--;
   }

   usFreeEntries = 0;
   pTar = pStart;
   while ((( pStart != pFirstFree ) || bLoop ) && usFreeEntries < usEntriesNeeded)
      {
      if (pStart->bFileName[0] && pStart->bFileName[0] != DELETED_ENTRY)
         {
         if (pTar != pStart)
            *pTar = *pStart;
         pTar++;
         }
      else
         usFreeEntries++;

      bLoop = FALSE;
      pStart++;
      }
   if (pTar != pStart)
      {
#if 1
      USHORT usEntries = 0;
      PDIRENTRY p;

      for( p = pStart; ( p != pFirstFree ) /*|| bLoop */; p++ )
        {
            /*bLoop = FALSE;*/
            usEntries++;
        }
      memmove(pTar, pStart, usEntries * sizeof (DIRENTRY));
#else
      memmove(pTar, pStart, (pFirstFree - pStart) * sizeof (DIRENTRY));
#endif
      pFirstFree -= usFreeEntries;
      memset(pFirstFree, DELETED_ENTRY, usFreeEntries * sizeof (DIRENTRY));
      }

   return pFirstFree;
}

#ifdef EXFAT

PDIRENTRY1 CompactDir1(PDIRENTRY1 pStart, ULONG ulSize, USHORT usEntriesNeeded)
{
// CompactDir version for exFAT
PDIRENTRY1 pTar, pMax, pFirstFree, pStart2;
USHORT usFreeEntries;
BOOL bLoop;

   pMax = (PDIRENTRY1)((PBYTE)pStart + ulSize);
   bLoop = pMax == pStart;
   pFirstFree = pMax;
   usFreeEntries = 0;
   while (( pFirstFree != pStart ) || bLoop )
      {
      if ( !((pFirstFree - 1)->bEntryType & ENTRY_TYPE_IN_USE_STATUS) )
         usFreeEntries++;
      else
         break;

      bLoop = FALSE;
      pFirstFree--;
      }

   pStart2 = pStart;
   while (( pStart2 != pFirstFree ))
      {
      if ( pStart2->bEntryType == ENTRY_TYPE_EOD )
         break;

      pStart2++;
      }

   if (pStart2 != pFirstFree)
      return pStart2;

   if ( (( pFirstFree == pStart ) && !bLoop ) || ((pFirstFree - 1)->bEntryType & ENTRY_TYPE_IN_USE_STATUS))
      {
      if (usFreeEntries >= usEntriesNeeded)
         return pFirstFree;
      }

   /*
      Leaving longname entries at the end
   */
   //while ((( pFirstFree != pStart ) || bLoop ) && (pFirstFree - 1)->bAttr == FILE_LONGNAME)
   //{
   //   bLoop = FALSE;
   //   pFirstFree--;
   //}

   usFreeEntries = 0;
   pTar = pStart;
   while ((( pStart != pFirstFree ) || bLoop ) && usFreeEntries < usEntriesNeeded)
      {
      //if (pStart->bFileName[0] && pStart->bFileName[0] != DELETED_ENTRY)
      if ((pStart->bEntryType != ENTRY_TYPE_EOD) && (pStart->bEntryType & ENTRY_TYPE_IN_USE_STATUS))
         {
         if (pTar != pStart)
            *pTar = *pStart;
         pTar++;
         }
      else
         usFreeEntries++;

      bLoop = FALSE;
      pStart++;
      }
   if (pTar != pStart)
      {
#if 1
      USHORT usEntries = 0;
      PDIRENTRY1 p;
      int i;

      for( p = pStart; ( p != pFirstFree ) /*|| bLoop */; p++ )
        {
            /*bLoop = FALSE;*/
            usEntries++;
        }
      memmove(pTar, pStart, usEntries * sizeof (DIRENTRY));
#else
      memmove(pTar, pStart, (pFirstFree - pStart) * sizeof (DIRENTRY));
#endif
      pFirstFree -= usFreeEntries;
      // Mark free entries as deleted
      //memset(pFirstFree, DELETED_ENTRY, usFreeEntries * sizeof (DIRENTRY));
      for (p = pFirstFree, i = 0; i < (int)usFreeEntries; i++, p++)
         {
         p->bEntryType &= ~ENTRY_TYPE_IN_USE_STATUS;
         }
      }

   return pFirstFree;
}

#endif

USHORT SemRequest(void far * hSem, ULONG ulTimeOut, PSZ pszText)
{
USHORT rc;

   if (ulTimeOut == TO_INFINITE)
      ulTimeOut = 60000; /* 1 minute */
   do
      {
      rc = FSH_SEMREQUEST(hSem, ulTimeOut);
      if (rc == ERROR_SEM_TIMEOUT)
         {
         Message("ERROR: Timeout on semaphore for %s", pszText);
         //rc = CritMessage("FAT32: Timeout on semaphore for %s", pszText);
         if (rc != CE_RETRETRY)
            rc = ERROR_SEM_TIMEOUT;
         else
            rc = ERROR_INTERRUPT;
         }
      } while (rc == ERROR_INTERRUPT);

   return rc;
}

BOOL IsDosSession(VOID)
{
PROCINFO pr;

   GetProcInfo(&pr, sizeof pr);
   if (pr.usPdb)
      return TRUE;
   return FALSE;
}

BOOL IsDriveLocked(PVOLINFO pVolInfo)
{
PROCINFO ProcInfo;
USHORT   rc;

   for (;;)
      {
      if (!pVolInfo->fLocked)
         return FALSE;
      GetProcInfo(&ProcInfo, sizeof ProcInfo);
      if (!memcmp(&pVolInfo->ProcLocked, &ProcInfo, sizeof (PROCINFO)))
         return FALSE;

      rc = FSH_CRITERROR(strlen(szDiskLocked) + 1, szDiskLocked, 0, "",
         CE_ALLFAIL | CE_ALLABORT | CE_ALLRETRY);
      if (rc != CE_RETRETRY)
         return TRUE;
      }
}

USHORT GetFatAccess(PVOLINFO pVolInfo, PSZ pszName)
{
USHORT rc;

   pVolInfo = pVolInfo;

   //Message("GetFatAccess: %s", pszName);
   rc = SemRequest(&ulSemRWFat, TO_INFINITE, pszName);
   if (rc)
      {
      Message("ERROR: SemRequest GetFatAccess Failed, rc = %d!", rc);
      //CritMessage("FAT32: SemRequest GetFatAccess Failed, rc = %d!", rc);
      Message("GetFatAccess Failed for %s, rc = %d", pszName, rc);
      return rc;
      }
   return 0;
}

VOID ReleaseFat(PVOLINFO pVolInfo)
{
   pVolInfo = pVolInfo;
   //Message("ReleaseFat");
   FSH_SEMCLEAR(&ulSemRWFat);
}

VOID Yield(void)
{
static PBYTE pYieldFlag = NULL;

   if (!pYieldFlag)
      DevHelp_GetDOSVar(DHGETDOSV_YIELDFLAG, 0, (PPVOID)&pYieldFlag);
   if (*pYieldFlag)
      DevHelp_Yield();
}

USHORT MY_PROBEBUF(USHORT usOperation, char far * pData, USHORT cbData)
{
    USHORT rc;
    rc = DevHelp_VerifyAccess(SELECTOROF(pData),
                                cbData,
                                OFFSETOF(pData),
                                (UCHAR)usOperation);

    if (rc)
    {
        rc = ERROR_PROTECTION_VIOLATION;
    }
    return rc;
}

USHORT DBCSStrlen( const PSZ pszStr )
{
   USHORT usLen;
   USHORT usIndex;
   USHORT usRet;

   usLen = strlen( pszStr );
   usRet = 0;
   for( usIndex = 0; usIndex < usLen; usIndex++ )
      {
         if( IsDBCSLead( pszStr[ usIndex ]))
            usIndex++;

         usRet++;
      }

   return usRet;
}
