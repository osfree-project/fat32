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

USHORT NameHash(USHORT *pszFilename, int NameLen);
USHORT GetChkSum16(const char *data, int bytes);
ULONG GetChkSum32(const char *data, int bytes);
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
static USHORT RecoverChain(PVOLINFO pVolInfo, ULONG ulCluster, PBYTE pData, USHORT cbData);
static USHORT WriteFatSector(PVOLINFO pVolInfo, ULONG ulSector);
static USHORT ReadFatSector(PVOLINFO pVolInfo, ULONG ulSector);
static ULONG  GetVolDevice(PVOLINFO pVolInfo);
static USHORT SetFileSize(PVOLINFO pVolInfo, PFILESIZEDATA pFileSize);
static ULONG GetChainSize(PVOLINFO pVolInfo, PSHOPENINFO pSHInfo, ULONG ulCluster);
static USHORT MakeChain(PVOLINFO pVolInfo, PSHOPENINFO pSHInfo, ULONG ulFirstCluster, ULONG ulSize);
static USHORT GetSetFileEAS(PVOLINFO pVolInfo, USHORT usFunc, PMARKFILEEASBUF pMark);
USHORT DBCSStrlen( const PSZ pszStr );

static ULONG GetFatEntrySec(PVOLINFO pVolInfo, ULONG ulCluster);
static ULONG GetFatEntryBlock(PVOLINFO pVolInfo, ULONG ulCluster, USHORT usBlockSize);
static ULONG GetFatEntry(PVOLINFO pVolInfo, ULONG ulCluster);
static ULONG GetFatEntryEx(PVOLINFO pVolInfo, PBYTE pFatStart, ULONG ulCluster, USHORT usBlockSize);
static void SetFatEntry(PVOLINFO pVolInfo, ULONG ulCluster, ULONG ulValue);
static void SetFatEntryEx(PVOLINFO pVolInfo, PBYTE pFatStart, ULONG ulCluster, ULONG ulValue, USHORT usBlockSize);

extern ULONG autocheck_mask;
extern ULONG force_mask;
extern ULONG fat_mask;
extern ULONG fat32_mask;
#ifdef EXFAT
extern ULONG exfat_mask;
#endif

void _cdecl autocheck(char *args);

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
   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_ATTACH - NOT SUPPORTED");
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
DIRENTRY DirEntry;
PDIRENTRY1 pDirEntry = (PDIRENTRY1)&DirEntry;
DIRENTRY TarEntry;
PDIRENTRY1 pTarEntry = (PDIRENTRY1)&TarEntry;
DIRENTRY1 DirStreamEntry;
DIRENTRY1 TarStreamEntry;
DIRENTRY1 DirSrcStream;
DIRENTRY1 DirDstStream;
#ifdef EXFAT
SHOPENINFO DirSrcSHInfo, DirDstSHInfo;
SHOPENINFO SrcSHInfo;
#endif
PSHOPENINFO pSrcSHInfo = NULL;
PSHOPENINFO pDirSrcSHInfo = NULL, pDirDstSHInfo = NULL;
ULONG    ulSrcCluster;
ULONG    ulDstCluster;
USHORT   rc, rc2;
POPENINFO pOpenInfo = NULL;
BYTE     szSrcLongName[ FAT32MAXPATH ];
BYTE     szDstLongName[ FAT32MAXPATH ];

   _asm push es;

   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_COPY %s to %s, mode %d", pSrc, pDst, usMode);

   pVolInfo = GetVolInfo(pcdfsi->cdi_hVPB);

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

   /*
      Not on the same drive: cannot copy
   */
   if (*pSrc != *pDst)
      {
      rc = ERROR_CANNOT_COPY;
      goto FS_COPYEXIT;
      }

   if( TranslateName(pVolInfo, 0L, pSrc, szSrcLongName, TRANSLATE_SHORT_TO_LONG ))
      strcpy( szSrcLongName, pSrc );

   if( TranslateName(pVolInfo, 0L, pDst, szDstLongName, TRANSLATE_SHORT_TO_LONG ))
      strcpy( szDstLongName, pDst );

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
      pSrc,
      usSrcCurDirEnd,
      RETURN_PARENT_DIR,
      &pszSrcFile,
      &DirSrcStream);

   if (ulSrcDirCluster == pVolInfo->ulFatEof)
      {
      rc = ERROR_PATH_NOT_FOUND;
      goto FS_COPYEXIT;
      }

#ifdef EXFAT
   if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
      {
      pDirSrcSHInfo = &DirSrcSHInfo;
      SetSHInfo1(pVolInfo, &DirSrcStream, pDirSrcSHInfo);
      }
#endif

   ulSrcCluster = FindPathCluster(pVolInfo, ulSrcDirCluster, pszSrcFile,
      pDirSrcSHInfo, &DirEntry, &DirStreamEntry, NULL);
   if (ulSrcCluster == pVolInfo->ulFatEof)
      {
      rc = ERROR_FILE_NOT_FOUND;
      goto FS_COPYEXIT;
      }
   /*
      Do not allow directories to be copied
   */
#ifdef EXFAT
   if ( ((pVolInfo->bFatType <  FAT_TYPE_EXFAT) && (DirEntry.bAttr & FILE_DIRECTORY)) ||
        ((pVolInfo->bFatType == FAT_TYPE_EXFAT) && (pDirEntry->u.File.usFileAttr & FILE_DIRECTORY)) )
#else
   if ( DirEntry.bAttr & FILE_DIRECTORY )
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
      pDst,
      usDstCurDirEnd,
      RETURN_PARENT_DIR,
      &pszDstFile,
      &DirDstStream);

   if (ulDstDirCluster == pVolInfo->ulFatEof)
      {
      rc = ERROR_PATH_NOT_FOUND;
      goto FS_COPYEXIT;
      }

#ifdef EXFAT
   if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
      {
      pDirDstSHInfo = &DirDstSHInfo;
      SetSHInfo1(pVolInfo, &DirDstStream, pDirDstSHInfo);
      }
#endif

   pszDstFile = strrchr( szDstLongName, '\\' ) + 1; /* To preserve long name */

   ulDstCluster = FindPathCluster(pVolInfo, ulDstDirCluster, pszDstFile,
      pDirDstSHInfo, &TarEntry, &TarStreamEntry, NULL);
   if (ulDstCluster != pVolInfo->ulFatEof)
      {
#ifdef EXFAT
      if ( ((pVolInfo->bFatType <  FAT_TYPE_EXFAT) && (TarEntry.bAttr & FILE_DIRECTORY)) ||
           ((pVolInfo->bFatType == FAT_TYPE_EXFAT) && (pTarEntry->u.File.usFileAttr & FILE_DIRECTORY)) )
#else
      if ( TarEntry.bAttr & FILE_DIRECTORY )
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
         if (TarEntry.fEAS == FILE_HAS_EAS || TarEntry.fEAS == FILE_HAS_CRITICAL_EAS)
            TarEntry.fEAS = FILE_HAS_NO_EAS;
#endif
         }

      rc = ModifyDirectory(pVolInfo, ulDstDirCluster, pDirDstSHInfo, MODIFY_DIR_DELETE,
                           &TarEntry, NULL, &TarStreamEntry, NULL, NULL, 0);
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

   memcpy(&TarEntry, &DirEntry, sizeof TarEntry);
#ifdef EXFAT
   if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
      {
#endif
      TarEntry.wCluster = 0;
      TarEntry.wClusterHigh = 0;
      TarEntry.ulFileSize = 0L;
#ifdef EXFAT
      }
   else
      {
      TarStreamEntry.u.Stream.ulFirstClus = 0;
#ifdef INCL_LONGLONG
      TarStreamEntry.u.Stream.ullValidDataLen = 0;
      TarStreamEntry.u.Stream.ullDataLen = 0;
#else
      AssignUL(&TarStreamEntry.u.Stream.ullValidDataLen, 0);
      AssignUL(&TarStreamEntry.u.Stream.ullDataLen, 0);
#endif
      }
#endif
   rc = ModifyDirectory(pVolInfo, ulDstDirCluster, pDirDstSHInfo, MODIFY_DIR_INSERT, NULL, &TarEntry, NULL, &TarStreamEntry, pszDstFile, 0);
   if (rc)
      goto FS_COPYEXIT;
#ifdef EXFAT
   if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
#endif
      memcpy(DirEntry.bFileName, TarEntry.bFileName, 11); ////

#ifdef EXFAT
   if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
      {
      pSrcSHInfo = &SrcSHInfo;
      SetSHInfo1(pVolInfo, &DirStreamEntry, pSrcSHInfo);
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
         DirEntry.wCluster = LOUSHORT(ulDstCluster);
         DirEntry.wClusterHigh = HIUSHORT(ulDstCluster);
         }
      else
         {
         DirEntry.wCluster = 0;
         DirEntry.wClusterHigh = 0;
         DirEntry.ulFileSize = 0L;
         }
#ifdef EXFAT
      }
   else
      {
      if (ulDstCluster != pVolInfo->ulFatEof)
         {
         DirStreamEntry.u.Stream.ulFirstClus = ulDstCluster;
         }
      else
         {
         DirStreamEntry.u.Stream.ulFirstClus = 0;
#ifdef INCL_LONGLONG
         DirStreamEntry.u.Stream.ullValidDataLen = 0;
         DirStreamEntry.u.Stream.ullDataLen = 0;
#else
         AssignUL(&DirStreamEntry.u.Stream.ullValidDataLen, 0);
         AssignUL(&DirStreamEntry.u.Stream.ullDataLen, 0);
#endif
         }
      }
#endif
   /*
      modify new entry
   */
   rc2 = ModifyDirectory(pVolInfo, ulDstDirCluster, pDirDstSHInfo, MODIFY_DIR_UPDATE, &TarEntry, &DirEntry, &TarStreamEntry, &DirStreamEntry, NULL, 0);
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

   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_COPY returned %u", rc);

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
PSZ      pszFile;
USHORT   rc;
DIRENTRY DirEntry;
DIRENTRY1 DirEntryStream;
PDIRENTRY1 pDirEntry = (PDIRENTRY1)&DirEntry;
POPENINFO pOpenInfo = NULL;
DIRENTRY1 DirStream;
#ifdef EXFAT
SHOPENINFO DirSHInfo;
#endif
PSHOPENINFO pDirSHInfo = NULL;
BYTE     szLongName[ FAT32MAXPATH ];

   _asm push es;

   if (f32Parms.fMessageActive & LOG_FS)
    Message("FS_DELETE for %s", pFile);

   pVolInfo = GetVolInfo(pcdfsi->cdi_hVPB);

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

   if( TranslateName(pVolInfo, 0L, pFile, szLongName, TRANSLATE_SHORT_TO_LONG ))
      strcpy( szLongName, pFile );

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
      pFile,
      usCurDirEnd,
      RETURN_PARENT_DIR,
      &pszFile,
      &DirStream);

   if (ulDirCluster == pVolInfo->ulFatEof)
      {
      rc = ERROR_PATH_NOT_FOUND;
      goto FS_DELETEEXIT;
      }

#ifdef EXFAT
   if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
      {
      pDirSHInfo = &DirSHInfo;
      SetSHInfo1(pVolInfo, &DirStream, pDirSHInfo);
      }
#endif

   ulCluster = FindPathCluster(pVolInfo, ulDirCluster, pszFile, pDirSHInfo,
                               &DirEntry, &DirEntryStream, NULL);
   if (ulCluster == pVolInfo->ulFatEof)
      {
      rc = ERROR_FILE_NOT_FOUND;
      goto FS_DELETEEXIT;
      }


#ifdef EXFAT
   if ( ((pVolInfo->bFatType <  FAT_TYPE_EXFAT) && (DirEntry.bAttr & FILE_DIRECTORY)) ||
        ((pVolInfo->bFatType == FAT_TYPE_EXFAT) && (pDirEntry->u.File.usFileAttr & FILE_DIRECTORY)) )
#else
   if ( DirEntry.bAttr & FILE_DIRECTORY )
#endif
      {
      rc = ERROR_ACCESS_DENIED;
      goto FS_DELETEEXIT;
      }

#ifdef EXFAT
   if ( ((pVolInfo->bFatType <  FAT_TYPE_EXFAT) && (DirEntry.bAttr & FILE_READONLY)) ||
        ((pVolInfo->bFatType == FAT_TYPE_EXFAT) && (pDirEntry->u.File.usFileAttr & FILE_READONLY)) )
#else
   if ( DirEntry.bAttr & FILE_READONLY )
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
      if (DirEntry.fEAS == FILE_HAS_EAS || DirEntry.fEAS == FILE_HAS_CRITICAL_EAS)
         DirEntry.fEAS = FILE_HAS_NO_EAS;
#endif
      }

   rc = ModifyDirectory(pVolInfo, ulDirCluster, pDirSHInfo, MODIFY_DIR_DELETE,
                        &DirEntry, NULL, &DirEntryStream, NULL, NULL, 0);
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

   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_DELETE returned %u", rc);

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

   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_EXIT for PID: %X, PDB %X",
         usPid, usPdb);

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
         if (f32Parms.fMessageActive & LOG_FUNCS)
            Message("Still findinfo's allocated");
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
            if (f32Parms.fMessageActive & LOG_FUNCS)
               Message("Removing a FINDINFO");
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

   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_FLUSHBUF, flag = %d", usFlag);

   if (pVolInfo->fWriteProtected)
      {
      rc = 0;
      goto FS_FLUSHEXIT;
      }

   rc = usFlushVolume(pVolInfo, usFlag, TRUE, PRIO_URGENT);

   if (rc)
      goto FS_FLUSHEXIT;

   if (!f32Parms.usDirtySectors) // vs
      goto FS_FLUSHEXIT;         //

   if (!UpdateFSInfo(pVolInfo))
      {
      rc = ERROR_SECTOR_NOT_FOUND;
      goto FS_FLUSHEXIT;
      }

   if (!MarkDiskStatus(pVolInfo, pVolInfo->fDiskCleanOnMount))
      {
      rc = ERROR_SECTOR_NOT_FOUND;
      goto FS_FLUSHEXIT;
      }
FS_FLUSHEXIT:

   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_FLUSHBUF returned %u", rc);

   _asm pop es;

   return rc;
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

   if (usFunc != FAT32_GETLOGDATA && f32Parms.fMessageActive & LOG_FS)
      Message("FS_FSCTL, Func = %Xh", usFunc);

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
         //f32Parms.fLW = TRUE;
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
         char szShortPath[ FAT32MAXPATH ] = { 0, };
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
            rc = ERROR_ACCESS_DENIED;
            goto FS_FSCTLEXIT;
            }
         if (pVolInfo)
         {
            TranslateName(pVolInfo, 0L, (PSZ)pParm, szShortPath, TRANSLATE_LONG_TO_SHORT);
            if( strlen( szShortPath ) >= cbData )
            {
                rc = ERROR_BUFFER_OVERFLOW;
                goto FS_FSCTLEXIT;
            }

            strcpy((PSZ)pData, szShortPath );
         }

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

      default :
         rc = ERROR_INVALID_FUNCTION;
         break;
      }

FS_FSCTLEXIT:

   if (usFunc != FAT32_GETLOGDATA && f32Parms.fMessageActive & LOG_FS)
      Message("FS_FSCTL returned %u", rc);

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

   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_FSINFO, Flag = %d, Level = %d", usFlag, usLevel);

   pVolInfo = GetVolInfo(hVBP);

   if (! pVolInfo)
      {
      rc = ERROR_INVALID_DRIVE;
      goto FS_FSINFOEXIT;
      }

   if (pVolInfo->fFormatInProgress)
      {
      rc = ERROR_ACCESS_DENIED;
      goto FS_FSINFOEXIT;
      }

   if (IsDriveLocked(pVolInfo))
      {
      rc = ERROR_DRIVE_LOCKED;
      goto FS_FSINFOEXIT;
      }

   rc = MY_PROBEBUF(PB_OPWRITE, pData, cbData);
   if (rc)
      {
      Message("Protection VIOLATION in FS_FSINFO!");
      goto FS_FSINFOEXIT;
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
               goto FS_FSINFOEXIT;
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

               if (f32Parms.fMessageActive & LOG_FUNCS)
                  Message("DOS Free space: sc: %lu tc: %lu fc: %lu",
                     pAlloc->cSectorUnit, pAlloc->cUnit, pAlloc->cUnitAvail);
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
               goto FS_FSINFOEXIT;
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
               goto FS_FSINFOEXIT;
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

FS_FSINFOEXIT:

   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_FSINFO returned %u", rc);

   _asm pop es;

   return rc;
}

USHORT fGetSetVolLabel(PVOLINFO pVolInfo, USHORT usFlag, PSZ pszVolLabel, PUSHORT pusSize)
{
struct vpfsi far * pvpfsi;
struct vpfsd far * pvpfsd;
PDIRENTRY pDirStart, pDir, pDirEnd;
ULONG ulCluster;
ULONG ulBlock;
DIRENTRY DirEntry;
BOOL     fFound;
USHORT   rc;
BYTE     bAttr = FILE_VOLID | FILE_ARCHIVED;
USHORT   usIndex;
PBOOTSECT pBootSect;
ULONG  ulSector;
USHORT usSectorsRead;
USHORT usSectorsPerBlock;
ULONG  ulDirEntries = 0;

   pDir = NULL;

   pDirStart = (PDIRENTRY)malloc((size_t)pVolInfo->ulBlockSize);
   if (!pDirStart)
      return ERROR_NOT_ENOUGH_MEMORY;

   fFound = FALSE;
   ulCluster = pVolInfo->BootSect.bpb.RootDirStrtClus;
   if (ulCluster == 1)
      {
      // FAT12/FAT16 root directory starting sector
      ulSector = pVolInfo->BootSect.bpb.ReservedSectors +
         pVolInfo->BootSect.bpb.SectorsPerFat * pVolInfo->BootSect.bpb.NumberOfFATs;
      usSectorsPerBlock = (USHORT)pVolInfo->SectorsPerCluster /
         (USHORT)(pVolInfo->ulClusterSize / pVolInfo->ulBlockSize);
      usSectorsRead = 0;
      }
   while (!fFound && ulCluster != pVolInfo->ulFatEof)
      {
      for (ulBlock = 0; ulBlock < pVolInfo->ulClusterSize / pVolInfo->ulBlockSize; ulBlock++)
         {
         if (ulCluster == 1)
            // reading root directory on FAT12/FAT16
            ReadSector(pVolInfo, ulSector + ulBlock * usSectorsPerBlock, usSectorsPerBlock, (void *)pDirStart, 0);
         else
            ReadBlock(pVolInfo, ulCluster, ulBlock, pDirStart, 0);
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
                  memcpy(&DirEntry, pDir, sizeof (DIRENTRY));
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
                  memcpy(&DirEntry, pDir, sizeof (DIRENTRY));
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
               if (usSectorsRead * pVolInfo->BootSect.bpb.BytesPerSector >=
                   pVolInfo->BootSect.bpb.RootDirEntries * sizeof(DIRENTRY))
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
         return 0;
         }
      *pusSize = 11;
#ifdef EXFAT
      if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
#endif
         memcpy(pszVolLabel, DirEntry.bFileName, 11);
#ifdef EXFAT
      else
         {
         // exFAT case
         USHORT pVolLabel[11];
         memcpy(pVolLabel, ((PDIRENTRY1)&DirEntry)->u.VolLbl.usChars,
            ((PDIRENTRY1)&DirEntry)->u.VolLbl.bCharCount * sizeof(USHORT));
         pVolLabel[((PDIRENTRY1)&DirEntry)->u.VolLbl.bCharCount] = 0;
         Translate2OS2(pVolLabel, pszVolLabel, ((PDIRENTRY1)&DirEntry)->u.VolLbl.bCharCount);
         }
#endif
      while (*pusSize > 0 && pszVolLabel[(*pusSize)-1] == 0x20)
         {
         (*pusSize)--;
         pszVolLabel[*pusSize] = 0;
         }
      return 0;
      }

   strupr(pszVolLabel);
   for (usIndex = 0; usIndex < *pusSize; usIndex++)
      {
      if (!strchr(rgValidChars, pszVolLabel[usIndex]))
         {
         free(pDirStart);
         return ERROR_INVALID_NAME;
         }
      }

   memset(&DirEntry, 0, sizeof DirEntry);

#ifdef EXFAT
   if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
      {
#endif
      memset(DirEntry.bFileName, 0x20, 11);
      memcpy(DirEntry.bFileName, pszVolLabel, min(11, *pusSize));
      DirEntry.bAttr = bAttr;
#ifdef EXFAT
      }
   else
      {
      // exFAT case
      USHORT pVolLabel[11];
      USHORT usChars = min(11, *pusSize);
      Translate2Win(pszVolLabel, pVolLabel, usChars);
      ((PDIRENTRY1)&DirEntry)->u.VolLbl.bCharCount = (BYTE)usChars;
      memcpy(((PDIRENTRY1)&DirEntry)->u.VolLbl.usChars, pVolLabel, usChars * sizeof(USHORT));
      ((PDIRENTRY1)&DirEntry)->bEntryType = ENTRY_TYPE_VOLUME_LABEL;
      }
#endif

   if (fFound)
      {
      memcpy(pDir, &DirEntry, sizeof (DIRENTRY));
      if (ulCluster == 1)
         // reading root directory on FAT12/FAT16
         rc = WriteSector(pVolInfo, ulSector + ulBlock * usSectorsPerBlock, usSectorsPerBlock, (void *)pDirStart, DVIO_OPWRTHRU);
      else
         rc = WriteBlock(pVolInfo, ulCluster, ulBlock, (PBYTE)pDirStart, DVIO_OPWRTHRU);
      }
   else
      {
      rc = ModifyDirectory(pVolInfo, pVolInfo->BootSect.bpb.RootDirStrtClus, NULL,
         MODIFY_DIR_INSERT, NULL, &DirEntry, NULL, NULL, pszVolLabel, DVIO_OPWRTHRU);
      }
   if (rc)
      {
      free(pDirStart);
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
         memcpy(pBootSect->VolumeLabel, DirEntry.bFileName, 11);
         rc = WriteSector(pVolInfo, 0L, 1, (PBYTE)pBootSect, DVIO_OPWRTHRU | DVIO_OPNCACHE);
         }
      free(pDirStart);
      }

   if (!rc)
      {
      memcpy(pVolInfo->BootSect.VolumeLabel, DirEntry.bFileName, 11);
      rc = FSH_GETVOLPARM(pVolInfo->hVBP, &pvpfsi, &pvpfsd);
      if (!rc)
         memcpy(pvpfsi->vpi_text, DirEntry.bFileName, 11);
      }

   return rc;
}

#ifdef EXFAT

USHORT fGetAllocBitmap(PVOLINFO pVolInfo, PULONG pulFirstCluster, PULONGLONG pullLen)
{
// This is relevant for exFAT only
PDIRENTRY1 pDirStart, pDir, pDirEnd;
DIRENTRY1 DirEntry;
ULONG ulCluster;
ULONG ulBlock;
BOOL fFound;

   pDir = NULL;

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
            if (pDir->bEntryType == ENTRY_TYPE_ALLOC_BITMAP)
               {
               fFound = TRUE;
               memcpy(&DirEntry, pDir, sizeof (DIRENTRY1));
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

   *pulFirstCluster = DirEntry.u.AllocBmp.ulFirstCluster;
   *pullLen = DirEntry.u.AllocBmp.ullDataLength;
   return 0;
}


USHORT fGetUpCaseTbl(PVOLINFO pVolInfo, PULONG pulFirstCluster, PULONGLONG pullLen, PULONG pulChecksum)
{
// This is relevant for exFAT only
PDIRENTRY1 pDirStart, pDir, pDirEnd;
DIRENTRY1 DirEntry;
ULONG ulCluster;
ULONG ulBlock;
BOOL fFound;

   pDir = NULL;

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
               memcpy(&DirEntry, pDir, sizeof (DIRENTRY1));
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

   *pulFirstCluster = DirEntry.u.UpCaseTbl.ulFirstCluster;
   *pullLen = DirEntry.u.UpCaseTbl.ullDataLength;
   *pulChecksum = DirEntry.u.UpCaseTbl.ulTblCheckSum;
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
//PFN pfn;
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

      p = strstr(szArguments, "/fat:");
      if (!p)
         p = strstr(szArguments, "-fat:");
      if (p)
         {
         // mount FAT12/FAT16 disks
         f32Parms.fFat = TRUE;
         p += 5;

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

   if (!ulCacheSectors)
      InitMessage("FAT32: Warning CACHE size is zero!");
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
   if (f32Parms.fFat || f32Parms.fExFat)
#else
   if (f32Parms.fFat)
#endif
      {
      // report itself as "UNIFAT" driver, instead of "FAT32"
      // (to avoid confusion, when FAT16 drives are shown as FAT32)
      strcpy(FS_NAME, "UNIFAT");
      }

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

   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_IOCTL, Cat %Xh, Func %Xh, File# %u",
         usCat, usFunc, psffsi->sfi_selfsfn);

   pVolInfo = GetVolInfo(psffsi->sfi_hVPB);

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
                  Message("Drive locked, pVolInfo->fLocked=%lu", pVolInfo->fLocked);
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
               rc = FSH_DOVOLIO2(hDEV, psffsi->sfi_selfsfn,
                  usCat, usFunc, pParm, cbParm, pData, cbData);

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
               rc = FSH_DOVOLIO2(hDEV, psffsi->sfi_selfsfn,
                  usCat, usFunc, pParm, cbParm, pData, cbData);

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
               rc = FSH_DOVOLIO2(hDEV, psffsi->sfi_selfsfn,
                  usCat, usFunc, pParm, cbParm, pData, cbData);

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

            case DSK_GETDEVICEPARAMS :
               if (pcbData)
                  *pcbData = sizeof (BIOSPARAMETERBLOCK);
               hDEV = GetVolDevice(pVolInfo);
               rc = FSH_DOVOLIO2(hDEV, psffsi->sfi_selfsfn,
                  usCat, usFunc, pParm, cbParm, pData, cbData);
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
               if (pcbData)
                  *pcbData = sizeof (BIOSPARAMETERBLOCK);
               hDEV = GetVolDevice(pVolInfo);

               pBPB = (PBIOSPARAMETERBLOCK)pData;

               pVolInfo->SectorsPerCluster = pBPB->bSectorsPerCluster;
               pVolInfo->BootSect.bpb.ReservedSectors = pBPB->usReservedSectors;
               pVolInfo->BootSect.bpb.NumberOfFATs = pBPB->cFATs;
               if (pVolInfo->bFatType < FAT_TYPE_FAT32)
                  {
                  pVolInfo->BootSect.bpb.RootDirEntries = pBPB->cRootEntries;
                  pVolInfo->BootSect.bpb.BigSectorsPerFat = ((PBPB0)pBPB)->SectorsPerFat;
                  }
               else if (pVolInfo->bFatType == FAT_TYPE_FAT32)
                  {
                     pVolInfo->BootSect.bpb.BigSectorsPerFat = ((PBPB)pBPB)->BigSectorsPerFat;
                  }

               rc = FSH_DOVOLIO2(hDEV, psffsi->sfi_selfsfn,
                  usCat, usFunc, pParm, cbParm, pData, cbData);
               if (!rc) {

                  if (pcbData) {
                    *pcbData = sizeof (BIOSPARAMETERBLOCK);
                  }

                  if (pcbParm) {
                    *pcbParm = cbParm;
                  }
               }

               break;

            default  :
               hDEV = GetVolDevice(pVolInfo);
               rc = FSH_DOVOLIO2(hDEV, psffsi->sfi_selfsfn,
                  usCat, usFunc, pParm, cbParm, pData, cbData);

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
               if (pVolInfo->usRASectors > MAX_RASECTORS)
                  pVolInfo->usRASectors = MAX_RASECTORS;
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
            /* case FAT32_READCLUSTER:
               {
               ULONG ulCluster;
               if (cbParm < sizeof(ULONG))
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
               rc = ReadCluster(pVolInfo, ulCluster, pData, 0);
               break;
               } */
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
            /* case FAT32_WRITECLUSTER:
               {
               ULONG ulCluster;
               if (cbParm < sizeof(ULONG))
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
               rc = WriteCluster(pVolInfo, ulCluster, pData, 0);
               break;
               } */
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
            default :
               rc = ERROR_BAD_COMMAND;
               break;
            }
         break;

      default:
         hDEV = GetVolDevice(pVolInfo);
         rc = FSH_DOVOLIO2(hDEV, psffsi->sfi_selfsfn,
            usCat, usFunc, pParm, cbParm, pData, cbData);

         if (!rc) {
           if (pcbData) {
             *pcbData = cbData;
           }

           if (pcbParm) {
             *pcbParm = cbParm;
           }
         }

         break;
      }

FS_IOCTLEXIT:
   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_IOCTL returned %u", rc);

   _asm pop es;

   return rc;
}


USHORT GetSetFileEAS(PVOLINFO pVolInfo, USHORT usFunc, PMARKFILEEASBUF pMark)
{
ULONG ulDirCluster;
DIRENTRY1 DirStream;
#ifdef EXFAT
SHOPENINFO DirSHInfo;
#endif
PSHOPENINFO pDirSHInfo = NULL;
PSZ   pszFile;

   if (f32Parms.fMessageActive & LOG_FUNCS)
      Message("GetSetFileEAS");

   ulDirCluster = FindDirCluster(pVolInfo,
      NULL,
      NULL,
      pMark->szFileName,
      0xFFFF,
      RETURN_PARENT_DIR,
      &pszFile,
      &DirStream);

   if (ulDirCluster == pVolInfo->ulFatEof)
      return ERROR_PATH_NOT_FOUND;

#ifdef EXFAT
   if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
      {
      pDirSHInfo = &DirSHInfo;
      SetSHInfo1(pVolInfo, &DirStream, pDirSHInfo);
      }
#endif

   if (usFunc == FAT32_QUERYEAS)
      {
      ULONG ulCluster;
      DIRENTRY DirEntry;
      PDIRENTRY1 pDirEntry = (PDIRENTRY1)&DirEntry;

      ulCluster = FindPathCluster(pVolInfo, ulDirCluster, pszFile, pDirSHInfo, &DirEntry, NULL, NULL);
      if (ulCluster == pVolInfo->ulFatEof)
         return ERROR_FILE_NOT_FOUND;
#ifdef EXFAT
      if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
#endif
         pMark->fEAS = DirEntry.fEAS;
#ifdef EXFAT
      else
         pMark->fEAS = pDirEntry->u.File.fEAS;
#endif
      return 0;
      }

   return MarkFileEAS(pVolInfo, ulDirCluster, pDirSHInfo, pszFile, pMark->fEAS);
}

USHORT RecoverChain(PVOLINFO pVolInfo, ULONG ulCluster, PBYTE pData, USHORT cbData)
{
DIRENTRY DirEntry;
PDIRENTRY1 pDirEntry = (PDIRENTRY1)&DirEntry;
DIRENTRY1 DirStream;
BYTE     szFileName[14];
USHORT   usNr;

   memset(&DirEntry, 0, sizeof (DIRENTRY));
   memset(&DirStream, 0, sizeof (DIRENTRY1));

#ifdef EXFAT
   if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
#endif
      {
      memcpy(DirEntry.bFileName, "FILE0000CHK", 11);
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
      return ERROR_FILE_EXISTS;
#ifdef EXFAT
   if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
#endif
      memcpy(DirEntry.bFileName, szFileName, 8);
   if (pData)
      strncpy(pData, szFileName, cbData);

#ifdef EXFAT
   if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
      {
#endif
      DirEntry.wCluster = LOUSHORT(ulCluster);
      DirEntry.wClusterHigh = HIUSHORT(ulCluster);
#ifdef EXFAT
      }
   else
      {
      DirStream.u.Stream.ulFirstClus = ulCluster;
      }
#endif
   while (ulCluster != pVolInfo->ulFatEof)
      {
      ULONG ulNextCluster;
#ifdef EXFAT
      if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
#endif
         DirEntry.ulFileSize += pVolInfo->ulClusterSize;
#ifdef EXFAT
      else
         {
#ifdef INCL_LONGLONG
         DirStream.u.Stream.ullValidDataLen += pVolInfo->ulClusterSize;
         DirStream.u.Stream.ullDataLen += pVolInfo->ulClusterSize;
#else
         DirStream.u.Stream.ullValidDataLen = AddUL(DirStream.u.Stream.ullValidDataLen, pVolInfo->ulClusterSize);
         DirStream.u.Stream.ullDataLen = AddUL(DirStream.u.Stream.ullDataLen, pVolInfo->ulClusterSize);
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

   return MakeDirEntry(pVolInfo,
      pVolInfo->BootSect.bpb.RootDirStrtClus, NULL,
      &DirEntry, &DirStream, szFileName);
}

USHORT SetFileSize(PVOLINFO pVolInfo, PFILESIZEDATA pFileSize)
{
ULONG ulDirCluster;
PSZ   pszFile;
ULONG ulCluster;
DIRENTRY DirEntry;
DIRENTRY DirNew;
DIRENTRY1 DirStream, DirStreamNew, DirStreamEntry;
ULONG ulClustersNeeded;
ULONG ulClustersUsed;
#ifdef EXFAT
SHOPENINFO SHInfo;
#endif
PSHOPENINFO pSHInfo = NULL;
SHOPENINFO DirSHInfo;
PSHOPENINFO pDirSHInfo = NULL;
USHORT rc;

   ulDirCluster = FindDirCluster(pVolInfo,
      NULL,
      NULL,
      pFileSize->szFileName,
      0xFFFF,
      RETURN_PARENT_DIR,
      &pszFile,
      &DirStreamEntry);

   if (ulDirCluster == pVolInfo->ulFatEof)
      return ERROR_PATH_NOT_FOUND;

#ifdef EXFAT
   if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
#endif
      {
      pDirSHInfo = &DirSHInfo;
      SetSHInfo1(pVolInfo, (PDIRENTRY1)&DirStreamEntry, pDirSHInfo);
      }

   ulCluster = FindPathCluster(pVolInfo, ulDirCluster, pszFile,
      pDirSHInfo, &DirEntry, &DirStream, NULL);
   if (ulCluster == pVolInfo->ulFatEof)
      return ERROR_FILE_NOT_FOUND;
   if (!ulCluster)
      pFileSize->ulFileSize = 0L;

   ulClustersNeeded = pFileSize->ulFileSize / pVolInfo->ulClusterSize;
   if (pFileSize->ulFileSize % pVolInfo->ulClusterSize)
      ulClustersNeeded++;

#ifdef EXFAT
   if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
      {
      pSHInfo = &SHInfo;
      SetSHInfo1(pVolInfo, (PDIRENTRY1)&DirStream, pSHInfo);
      }
#endif

   if (pFileSize->ulFileSize > 0 )
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
         pFileSize->ulFileSize = ulClustersUsed * pVolInfo->ulClusterSize;
      else
         SetNextCluster(pVolInfo, ulCluster, pVolInfo->ulFatEof);
      }

   memcpy(&DirNew, &DirEntry, sizeof (DIRENTRY));
   memcpy(&DirStreamNew, &DirStream, sizeof (DIRENTRY1));

#ifdef EXFAT
   if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
      {
#endif
      DirNew.ulFileSize = pFileSize->ulFileSize;
#ifdef EXFAT
      }
   else
      {
#ifdef INCL_LONGLONG
      DirStreamNew.u.Stream.ullValidDataLen = pFileSize->ulFileSize;
      DirStreamNew.u.Stream.ullDataLen =
         (pFileSize->ulFileSize / pVolInfo->ulClusterSize) * pVolInfo->ulClusterSize +
         ((pFileSize->ulFileSize % pVolInfo->ulClusterSize) ? pVolInfo->ulClusterSize : 0);
#else
      AssignUL(&DirStreamNew.u.Stream.ullValidDataLen, pFileSize->ulFileSize);
      AssignUL(&DirStreamNew.u.Stream.ullDataLen,
         (pFileSize->ulFileSize / pVolInfo->ulClusterSize) * pVolInfo->ulClusterSize +
         ((pFileSize->ulFileSize % pVolInfo->ulClusterSize) ? pVolInfo->ulClusterSize : 0));
#endif
      }
#endif

   if (!pFileSize->ulFileSize)
      {
#ifdef EXFAT
      if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
         {
#endif
         DirNew.wCluster = 0;
         DirNew.wClusterHigh = 0;
#ifdef EXFAT
         }
      else
         {
         DirStreamNew.u.Stream.ulFirstClus = 0;
         }
#endif
      }

   rc = ModifyDirectory(pVolInfo, ulDirCluster, pDirSHInfo, MODIFY_DIR_UPDATE,
      &DirEntry, &DirNew, &DirStream, &DirStreamNew, NULL, 0);

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
DIRENTRY DirEntry;
DIRENTRY1 DirEntryStream, DirEntryStreamOld;
PDIRENTRY1 pDirEntry = (PDIRENTRY1)&DirEntry;
ULONG    ulCluster;
ULONG    ulTarCluster;
USHORT   rc;
POPENINFO pOISrc = NULL;
POPENINFO pOIDst = NULL;
DIRENTRY1 DirSrcStream;
DIRENTRY1 DirDstStream;
#ifdef EXFAT
SHOPENINFO DirSrcSHInfo, DirDstSHInfo;
#endif
PSHOPENINFO pDirSrcSHInfo = NULL, pDirDstSHInfo = NULL;
BYTE     szSrcLongName[ FAT32MAXPATH ];
BYTE     szDstLongName[ FAT32MAXPATH ];

   _asm push es;

   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_MOVE %s to %s", pSrc, pDst);

   pVolInfo = GetVolInfo(pcdfsi->cdi_hVPB);

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

   if( TranslateName(pVolInfo, 0L, pSrc, szSrcLongName, TRANSLATE_SHORT_TO_LONG ))
      strcpy( szSrcLongName, pSrc );

   if( TranslateName(pVolInfo, 0L, pDst, szDstLongName, TRANSLATE_SHORT_TO_LONG ))
      strcpy( szDstLongName, pDst );

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
      pDst,
      usDstCurDirEnd,
      RETURN_PARENT_DIR,
      &pszDstFile,
      &DirDstStream);

   if (ulDstDirCluster == pVolInfo->ulFatEof)
      {
      rc = ERROR_PATH_NOT_FOUND;
      goto FS_MOVEEXIT;
      }

#ifdef EXFAT
   if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
      {
      pDirDstSHInfo = &DirDstSHInfo;
      SetSHInfo1(pVolInfo, (PDIRENTRY1)&DirDstStream, pDirDstSHInfo);
      }
#endif

   ulTarCluster = FindPathCluster(pVolInfo, ulDstDirCluster, pszDstFile, pDirDstSHInfo, &DirEntry, NULL, NULL);

   /*
      Check source
   */
   ulSrcDirCluster = FindDirCluster(pVolInfo,
      pcdfsi,
      pcdfsd,
      pSrc,
      usSrcCurDirEnd,
      RETURN_PARENT_DIR,
      &pszSrcFile,
      &DirSrcStream);

   if (ulSrcDirCluster == pVolInfo->ulFatEof)
      {
      rc = ERROR_PATH_NOT_FOUND;
      goto FS_MOVEEXIT;
      }

#ifdef EXFAT
   if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
      {
      pDirSrcSHInfo = &DirSrcSHInfo;
      SetSHInfo1(pVolInfo, (PDIRENTRY1)&DirSrcStream, pDirSrcSHInfo);
      }
#endif

   ulCluster = FindPathCluster(pVolInfo, ulSrcDirCluster, pszSrcFile, pDirSrcSHInfo, &DirEntry, &DirEntryStream, NULL);
   if (ulCluster == pVolInfo->ulFatEof)
      {
      rc = ERROR_FILE_NOT_FOUND;
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
   if (DirEntry.bAttr & FILE_READONLY)
      {
      rc = ERROR_ACCESS_DENIED;
      goto FS_MOVEEXIT;
      }
#endif

#ifdef EXFAT
   if ( ((pVolInfo->bFatType <  FAT_TYPE_EXFAT) && (DirEntry.bAttr & FILE_DIRECTORY)) ||
        ((pVolInfo->bFatType == FAT_TYPE_EXFAT) && (pDirEntry->u.File.usFileAttr & FILE_DIRECTORY)) )
#else
   if ( DirEntry.bAttr & FILE_DIRECTORY )
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
      BYTE szName[FAT32MAXPATH];
      rc = FSH_ISCURDIRPREFIX(pSrc);
      if (rc)
         goto FS_MOVEEXIT;
      rc = TranslateName(pVolInfo, 0L, pSrc, szName, TRANSLATE_AUTO);
      if (rc)
         goto FS_MOVEEXIT;
      rc = FSH_ISCURDIRPREFIX(szName);
      if (rc)
         goto FS_MOVEEXIT;
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
      DIRENTRY DirOld;

      memcpy(&DirOld, &DirEntry, sizeof DirEntry);
      memcpy(&DirEntryStreamOld, &DirEntryStream, sizeof DirEntry);

      rc = ModifyDirectory(pVolInfo, ulSrcDirCluster, pDirSrcSHInfo,
         MODIFY_DIR_RENAME, &DirOld, &DirEntry, &DirEntryStream, &DirEntryStreamOld, pszDstFile, 0);
      goto FS_MOVEEXIT;
      }

   /*
      First delete old
   */

   rc = ModifyDirectory(pVolInfo, ulSrcDirCluster, pDirSrcSHInfo, MODIFY_DIR_DELETE, &DirEntry, NULL, &DirEntryStream, NULL, NULL, 0);
   if (rc)
      goto FS_MOVEEXIT;

   /*
      Then insert new
   */

   rc = ModifyDirectory(pVolInfo, ulDstDirCluster, pDirDstSHInfo, MODIFY_DIR_INSERT, NULL, &DirEntry, NULL, &DirEntryStream, pszDstFile, 0);
   if (rc)
      goto FS_MOVEEXIT;

   /*
      If a directory was moved, the .. entry in the dir itself must
      be modified as well.
   */
#ifdef EXFAT
   if ( (pVolInfo->bFatType < FAT_TYPE_EXFAT) && (DirEntry.bAttr & FILE_DIRECTORY) )
#else
   if ( DirEntry.bAttr & FILE_DIRECTORY )
#endif
      {
      ULONG ulTmp;
      ulTmp = FindPathCluster(pVolInfo, ulCluster, "..", NULL, &DirEntry, &DirEntryStream, NULL); ////
      if (ulTmp != pVolInfo->ulFatEof)
         {
         DIRENTRY DirNew;

         memcpy(&DirNew, &DirEntry, sizeof (DIRENTRY));
         DirNew.wCluster = LOUSHORT(ulDstDirCluster);
         DirNew.wClusterHigh = HIUSHORT(ulDstDirCluster);

         rc = ModifyDirectory(pVolInfo, ulCluster, NULL,
            MODIFY_DIR_UPDATE, &DirEntry, &DirNew, NULL, NULL, NULL, 0);
         if (rc)
            goto FS_MOVEEXIT;
         }
      else
         {
         Message("FS_MOVE: .. entry of moved directory not found!");
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

   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_MOVE returned %d", rc);

   return rc;

   _asm pop es;

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

     if (f32Parms.fMessageActive & LOG_FS)
        Message("FS_PROCESSNAME for %s", pNameBuf);

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

     if (f32Parms.fMessageActive & LOG_FS)
        Message(" FS_PROCESSNAME returned filename: %s", pNameBuf);

     _asm pop es;
#endif
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

   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_SHUTDOWN, Type = %d", usType);
   f32Parms.fInShutDown = TRUE;
   f32Parms.fLW = FALSE;

   if (usType == SD_BEGIN)
      {
      for (pVolInfo = pGlobVolInfo; pVolInfo;
           pVolInfo = (PVOLINFO)pVolInfo->pNextVolInfo)
         {
            if (pVolInfo->fWriteProtected)
               continue;

            if (pVolInfo->fFormatInProgress)
               continue;

            usFlushVolume( pVolInfo, FLUSH_DISCARD, TRUE, PRIO_URGENT );

            //if (!f32Parms.usDirtySectors)
            //   continue;

            UpdateFSInfo(pVolInfo);
            MarkDiskStatus(pVolInfo, pVolInfo->fDiskCleanOnMount);
         }
      }
   else /* usType == SD_COMPLETE */
      TranslateFreeBuffer();

//FS_SHUTDOWNEXIT:

   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_SHUTDOWN returned %d", rc);

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

   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_VERIFYUNCNAME - NOT SUPPORTED");
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

   if (f32Parms.fMessageActive & LOG_FUNCS)
      Message("gdtAlloc for %lu bytes", tSize);

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

   if (f32Parms.fMessageActive & LOG_FUNCS)
      Message("ldtAlloc for %lu bytes", tSize);

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

   if (f32Parms.fMessageActive & LOG_FUNCS)
      Message("linAlloc");

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
      if (f32Parms.fMessageActive & LOG_FUNCS)
         Message("ERROR: linalloc failed, rc = %d", rc);

      if( !fIgnore )
        CritMessage("linalloc failed, rc = %d", rc);

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

    if (f32Parms.fMessageActive & LOG_FUNCS)
        Message("linAlloc");

    rc = DevHelp_VirtToLin(     SELECTOROF(pulPhysAddr),
                                OFFSETOF(pulPhysAddr),
                                &lulPhysAddr);

    if (rc != NO_ERROR)
    {
        if (f32Parms.fMessageActive & LOG_FUNCS)
            Message("ERROR: linalloc VirtToLin failed, rc = %d", rc);

        if( !fIgnore )
            CritMessage("linalloc VirtToLin failed, rc = %d", rc);

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
        if (f32Parms.fMessageActive & LOG_FUNCS)
            Message("ERROR: linalloc VMAlloc failed, rc = %d", rc);

        if( !fIgnore )
            CritMessage("linalloc VMAlloc failed, rc = %d", rc);

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

   if (f32Parms.fMessageActive & LOG_FUNCS)
      Message("freeseg");

   if (!p)
      return;

   rc = MY_PROBEBUF(PB_OPREAD, p, 1);
   if (rc)
      {
      CritMessage("FAT32: freeseg: protection violation!");
      Message("ERROR: freeseg: protection violation");
      return;
      }

   usSel = SELECTOROF(p);
   rc = FSH_SEGFREE(usSel);
   if (rc)
      {
      CritMessage("FAT32: FSH_SEGFREE failed for selector %X", usSel);
      Message("ERROR: FSH_SEGFREE failed");
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
//ULONG  ulNextCluster;
USHORT usSectorsPerBlock = (USHORT)(pVolInfo->ulBlockSize / pVolInfo->BootSect.bpb.BytesPerSector);
USHORT rc;

   if (f32Parms.fMessageActive & LOG_FUNCS)
      Message("ReadBlock %lu", ulCluster);

   if (ulCluster < 2 || ulCluster >= pVolInfo->ulTotalClusters + 2)
      {
      CritMessage("ERROR: Cluster %lu does not exist on disk %c:",
         ulCluster, pVolInfo->bDrive + 'A');
      Message("ERROR: Cluster %lu (%lX) does not exist on disk %c:",
         ulCluster, ulCluster, pVolInfo->bDrive + 'A');
      return ERROR_SECTOR_NOT_FOUND;
      }

   // check for bad cluster
   //ulNextCluster = GetNextCluster(pVolInfo, NULL, ulCluster);
   //if (ulNextCluster == pVolInfo->ulFatBad)
   //   return ERROR_SECTOR_NOT_FOUND;

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
//ULONG  ulNextCluster;
USHORT usSectorsPerBlock = (USHORT)(pVolInfo->ulBlockSize / pVolInfo->BootSect.bpb.BytesPerSector);
USHORT rc;

   if (f32Parms.fMessageActive & LOG_FUNCS)
      Message("WriteBlock");

   if (ulCluster < 2 || ulCluster >= pVolInfo->ulTotalClusters + 2)
      {
      CritMessage("ERROR: Cluster %ld does not exist on disk %c:",
         ulCluster, pVolInfo->bDrive + 'A');
      Message("ERROR: Cluster %ld does not exist on disk %c:",
         ulCluster, pVolInfo->bDrive + 'A');
      return ERROR_SECTOR_NOT_FOUND;
      }

   // check for bad cluster
   //ulNextCluster = GetNextCluster(pVolInfo, NULL, ulCluster);
   //if (ulNextCluster == pVolInfo->ulFatBad)
   //   return ERROR_SECTOR_NOT_FOUND;

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
USHORT rc;

   if (f32Parms.fMessageActive & LOG_FUNCS)
      Message("ReadFatSector");

   // read multiples of three sectors,
   // to fit a whole number of FAT12 entries
   // (ulSector is indeed a number of 3*512
   // bytes blocks, so, it is needed to multiply by 3)

   if (pVolInfo->ulCurFatSector == ulSector)
      return 0;

   if (ulSec >= pVolInfo->BootSect.bpb.BigSectorsPerFat)
      {
      CritMessage("ERROR: ReadFatSector: Sector %lu too high", ulSec);
      Message("ERROR: ReadFatSector: Sector %lu too high", ulSec);
      return ERROR_SECTOR_NOT_FOUND;
      }

   rc = ReadSector2(pVolInfo, pVolInfo->ulActiveFatStart + ulSec, 3,
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
USHORT usFat;
USHORT rc;

   if (f32Parms.fMessageActive & LOG_FUNCS)
      Message("WriteFatSector");

   // write multiples of three sectors,
   // to fit a whole number of FAT12 entries
   // (ulSector is indeed a number of 3*512
   // bytes blocks, so, it is needed to multiply by 3)

   if (pVolInfo->ulCurFatSector != ulSector)
      {
      CritMessage("FAT32: WriteFatSector: Sectors do not match!");
      Message("ERROR: WriteFatSector: Sectors do not match!");
      return ERROR_SECTOR_NOT_FOUND;
      }

   if (ulSec >= pVolInfo->BootSect.bpb.BigSectorsPerFat)
      {
      CritMessage("ERROR: WriteFatSector: Sector %ld too high", ulSec);
      Message("ERROR: WriteFatSector: Sector %ld too high", ulSec);
      return ERROR_SECTOR_NOT_FOUND;
      }

   for (usFat = 0; usFat < pVolInfo->BootSect.bpb.NumberOfFATs; usFat++)
      {
      rc = WriteSector(pVolInfo, pVolInfo->ulActiveFatStart + ulSec, 3,
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

   if (f32Parms.fMessageActive & LOG_FUNCS)
      Message("ReadBmpSector");

   if (pVolInfo->ulCurBmpSector == ulSector)
      return 0;

   if (ulSector >= (pVolInfo->ulAllocBmpLen + pVolInfo->BootSect.bpb.BytesPerSector - 1)
         / pVolInfo->BootSect.bpb.BytesPerSector)
      {
      CritMessage("ERROR: ReadBmpSector: Sector %lu too high", ulSector);
      Message("ERROR: ReadBmpSector: Sector %lu too high", ulSector);
      return ERROR_SECTOR_NOT_FOUND;
      }

   rc = ReadSector2(pVolInfo, pVolInfo->ulBmpStartSector + ulSector, 1,
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

   if (f32Parms.fMessageActive & LOG_FUNCS)
      Message("WriteBmpSector");

   if (pVolInfo->ulCurBmpSector != ulSector)
      {
      CritMessage("FAT32: WriteBmpSector: Sectors do not match!");
      Message("ERROR: WriteBmpSector: Sectors do not match!");
      return ERROR_SECTOR_NOT_FOUND;
      }

   if (ulSector >= (pVolInfo->ulAllocBmpLen + pVolInfo->BootSect.bpb.BytesPerSector - 1)
         / pVolInfo->BootSect.bpb.BytesPerSector)
      {
      CritMessage("ERROR: WriteBmpSector: Sector %ld too high", ulSector);
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
   return GetFatEntryBlock(pVolInfo, ulCluster, pVolInfo->BootSect.bpb.BytesPerSector * 3); // in three sector blocks
}

/******************************************************************
*
******************************************************************/
static ULONG GetFatEntryBlock(PVOLINFO pVolInfo, ULONG ulCluster, USHORT usBlockSize)
{
ULONG  ulSector;

   ulCluster &= pVolInfo->ulFatEof;

   switch (pVolInfo->bFatType)
      {
      case FAT_TYPE_FAT12:
         ulSector = ((ulCluster * 3) / 2) / usBlockSize;
         break;

      case FAT_TYPE_FAT16:
         ulSector = (ulCluster * 2) / usBlockSize;
         break;

      case FAT_TYPE_FAT32:
#ifdef EXFAT
      case FAT_TYPE_EXFAT:
#endif
         ulSector = (ulCluster * 4) / usBlockSize;
      }

   return ulSector;
}

/******************************************************************
*
******************************************************************/
static ULONG GetFatEntry(PVOLINFO pVolInfo, ULONG ulCluster)
{
   return GetFatEntryEx(pVolInfo, pVolInfo->pbFatSector, ulCluster, pVolInfo->BootSect.bpb.BytesPerSector * 3);
}

/******************************************************************
*
******************************************************************/
static ULONG GetFatEntryEx(PVOLINFO pVolInfo, PBYTE pFatStart, ULONG ulCluster, USHORT usBlockSize)
{
   ulCluster &= pVolInfo->ulFatEof;

   switch (pVolInfo->bFatType)
      {
      case FAT_TYPE_FAT12:
         {
         ULONG   ulOffset;
         PUSHORT pusCluster;

         ulOffset = (ulCluster * 3) / 2;

         if (usBlockSize)
            ulOffset %= usBlockSize;

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

         if (usBlockSize)
            ulOffset %= usBlockSize;

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

         if (usBlockSize)
            ulOffset %= usBlockSize;

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
   SetFatEntryEx(pVolInfo, pVolInfo->pbFatSector, ulCluster, ulValue, pVolInfo->BootSect.bpb.BytesPerSector * 3);
}

/******************************************************************
*
******************************************************************/
static void SetFatEntryEx(PVOLINFO pVolInfo, PBYTE pFatStart, ULONG ulCluster, ULONG ulValue, USHORT usBlockSize)
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

         if (usBlockSize)
            ulOffset %= usBlockSize;

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

         if (usBlockSize)
            ulOffset %= usBlockSize;

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

         if (usBlockSize)
            ulOffset %= usBlockSize;

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
   if (f32Parms.fMessageActive & LOG_FUNCS)
      Message("GetNextCluster for %lu", ulCluster);

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
BOOL ClusterInUse2(PVOLINFO pVolInfo, ULONG ulCluster)
{
#ifdef EXFAT
ULONG ulBmpSector;
#endif

   if (ulCluster >= pVolInfo->ulTotalClusters + 2)
      {
      Message("An invalid cluster number %8.8lX was found\n", ulCluster);
      return TRUE;
      }

#ifdef EXFAT
   if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
#endif
      {
      ULONG ulNextCluster;
      ulNextCluster = GetNextCluster2(pVolInfo, NULL, ulCluster);
      if (ulNextCluster)
         return TRUE;
      else
         return FALSE;
      }

#ifdef EXFAT
   ulBmpSector = GetAllocBitmapSec(pVolInfo, ulCluster);

   if (pVolInfo->ulCurBmpSector != ulBmpSector)
      ReadBmpSector(pVolInfo, ulBmpSector);

   return GetBmpEntry(pVolInfo, ulCluster);
#else
   return FALSE;
#endif
}

/******************************************************************
*
******************************************************************/
BOOL ClusterInUse(PVOLINFO pVolInfo, ULONG ulCluster)
{
   if (f32Parms.fMessageActive & LOG_FUNCS)
      Message("ClusterInUse for %lu", ulCluster);

   if (!GetFatAccess(pVolInfo, "ClusterInUse"))
      {
      ulCluster = ClusterInUse2(pVolInfo, ulCluster);
      ReleaseFat(pVolInfo);
      return TRUE;
      }

   return FALSE;
}

#ifdef EXFAT

/******************************************************************
*
******************************************************************/
BOOL MarkCluster2(PVOLINFO pVolInfo, ULONG ulCluster, BOOL fState)
{
ULONG ulBmpSector;

   //if (ClusterInUse2(pVolInfo, ulCluster) && fState)
   //   {
   //   return FALSE;
   //   }

   ulBmpSector = GetAllocBitmapSec(pVolInfo, ulCluster);

   if (pVolInfo->ulCurBmpSector != ulBmpSector)
      ReadBmpSector(pVolInfo, ulBmpSector);

   SetBmpEntry(pVolInfo, ulCluster, fState);
   WriteBmpSector(pVolInfo, ulBmpSector);

   return TRUE;
}

/******************************************************************
*
******************************************************************/
BOOL MarkCluster(PVOLINFO pVolInfo, ULONG ulCluster, BOOL fState)
{
   if (f32Parms.fMessageActive & LOG_FUNCS)
      Message("MarkCluster for %lu", ulCluster);

   if (!GetFatAccess(pVolInfo, "MarkCluster"))
      {
      ulCluster = MarkCluster2(pVolInfo, ulCluster, fState);
      ReleaseFat(pVolInfo);
      return TRUE;
      }

   return FALSE;
}

#endif

/******************************************************************
*
******************************************************************/
ULONG GetFreeSpace2(PVOLINFO pVolInfo)
{
//ULONG ulSector = 0;
ULONG ulCluster = 0;
ULONG ulNextFree = 0;
ULONG ulTotalFree;

   if (f32Parms.fMessageActive & LOG_FUNCS)
      Message("GetFreeSpace");

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
      if (!ClusterInUse2(pVolInfo, ulCluster))
         {
         ulTotalFree++;
         if (!ulNextFree)
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
ULONG GetFreeSpace(PVOLINFO pVolInfo)
{
ULONG rc;

   if (f32Parms.fMessageActive & LOG_FUNCS)
      Message("GetFreeSpace");

   if (!GetFatAccess(pVolInfo, "GetFreeSpace"))
      {
      rc = GetFreeSpace2(pVolInfo);
      ReleaseFat(pVolInfo);
      }

   return rc;
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

   if (f32Parms.fMessageActive & LOG_FUNCS)
      Message("MakeFatChain, %lu clusters", ulClustersRequested);

   if (!ulClustersRequested)
      return pVolInfo->ulFatEof;

   /*
      Trick, set fDiskClean to FALSE, so WriteSector
      won't set is back to dirty again
    */
   fClean = pVolInfo->fDiskClean;
   pVolInfo->fDiskClean = FALSE;

   if (GetFatAccess(pVolInfo, "MakeFatChain"))
      return pVolInfo->ulFatEof;

   if (pVolInfo->pBootFSInfo->ulFreeClusters < ulClustersRequested)
      {
      ReleaseFat(pVolInfo);
      return pVolInfo->ulFatEof;
      }

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
               ulSector  = GetFatEntrySec(pVolInfo, ulFirstCluster);
               ulNextCluster = GetFatEntry(pVolInfo, ulFirstCluster);

               if (ulSector != pVolInfo->ulCurFatSector)
                  ReadFatSector(pVolInfo, ulSector);

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
               ulBmpSector = GetAllocBitmapSec(pVolInfo, ulFirstCluster);
               fStatus = GetBmpEntry(pVolInfo, ulFirstCluster);

               if (ulBmpSector != pVolInfo->ulCurBmpSector)
                  ReadBmpSector(pVolInfo, ulBmpSector);

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
            if (f32Parms.fMessageActive & LOG_FUNCS)
               Message("No contiguous block found, restarting at cluster 2");
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
               ulSector = GetFatEntrySec(pVolInfo, ulCluster);
               ulNewCluster = GetFatEntry(pVolInfo, ulCluster);
               if (ulSector != pVolInfo->ulCurFatSector)
                  ReadFatSector(pVolInfo, ulSector);
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
               ulBmpSector = GetAllocBitmapSec(pVolInfo, ulCluster);
               fStatus1 = GetBmpEntry(pVolInfo, ulCluster);
               if (ulBmpSector != pVolInfo->ulCurBmpSector)
                  ReadBmpSector(pVolInfo, ulBmpSector);
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

         if (MakeChain(pVolInfo, pSHInfo, ulFirstCluster, ulClustersRequested)) ////
            goto MakeFatChain_Error;

         if (ulPrevCluster != pVolInfo->ulFatEof)
            {
            if (SetNextCluster2(pVolInfo, ulPrevCluster, ulFirstCluster) == pVolInfo->ulFatEof)
               goto MakeFatChain_Error;
            }

         ReleaseFat(pVolInfo);
         if (f32Parms.fMessageActive & LOG_FUNCS)
            {
            if (fContiguous)
               Message("Contiguous chain returned, first = %lu", ulReturn);
            else
               Message("NON Contiguous chain returned, first = %lu", ulReturn);
            }
         if (pulLast)
            *pulLast = ulFirstCluster + ulClustersRequested - 1;
         return ulReturn;
         }

      /*
         We get here only if no free chain long enough was found!
      */
      if (f32Parms.fMessageActive & LOG_FUNCS)
         Message("No contiguous block found, largest found is %lu clusters", ulLargestSize);
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
            if (SetNextCluster2(pVolInfo, ulPrevCluster, ulFirstCluster) == pVolInfo->ulFatEof)
               goto MakeFatChain_Error;
            }

         ulPrevCluster        = ulFirstCluster + ulLargestSize - 1;
         ulClustersRequested -= ulLargestSize;
         }
      else
         break;
      }

MakeFatChain_Error:

   ReleaseFat(pVolInfo);
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
ULONG  ulCluster = 0;
ULONG ulNewCluster = 0;
#ifdef EXFAT
BOOL fStatus;
#endif
USHORT rc;

   if (f32Parms.fMessageActive & LOG_FUNCS)
      Message("MakeChain");

   ulLastCluster = ulFirstCluster + ulSize - 1;

   ulSector = GetFatEntrySec(pVolInfo, ulFirstCluster);
   if (ulSector != pVolInfo->ulCurFatSector)
      ReadFatSector(pVolInfo, ulSector);

#ifdef EXFAT
   if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
      {
      ulBmpSector = GetAllocBitmapSec(pVolInfo, ulFirstCluster);
      if (ulBmpSector != pVolInfo->ulCurBmpSector)
         ReadBmpSector(pVolInfo, ulBmpSector);
      }
#endif

   for (ulCluster = ulFirstCluster; ulCluster < ulLastCluster; ulCluster++)
      {
      if (! pSHInfo || ! pSHInfo->fNoFatChain)
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
#endif
            {
            CritMessage("FAT32:MakeChain:Cluster %lx is not free!", ulCluster);
            Message("ERROR:MakeChain:Cluster %lx is not free!", ulCluster);
            return ERROR_SECTOR_NOT_FOUND;
            }
         SetFatEntry(pVolInfo, ulCluster, ulCluster + 1);
         }
#ifdef EXFAT
      if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
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
            CritMessage("FAT32:MakeChain:Cluster %lx is not free!", ulCluster);
            Message("ERROR:MakeChain:Cluster %lx is not free!", ulCluster);
            return ERROR_SECTOR_NOT_FOUND;
            }
         SetBmpEntry(pVolInfo, ulCluster, 1);
         }
#endif
      }

#ifdef EXFAT
   if (! pSHInfo || ! pSHInfo->fNoFatChain)
#endif
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
         CritMessage("FAT32:MakeChain:Cluster %lx is not free!", ulCluster);
         Message("ERROR:MakeChain:Cluster %lx is not free!", ulCluster);
         return ERROR_SECTOR_NOT_FOUND;
         }

      SetFatEntry(pVolInfo, ulCluster, pVolInfo->ulFatEof);
      rc = WriteFatSector(pVolInfo, pVolInfo->ulCurFatSector);
      if (rc)
         return rc;
      }

#ifdef EXFAT
   if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
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
         CritMessage("FAT32:MakeChain:Cluster %lx is not free!", ulCluster);
         Message("ERROR:MakeChain:Cluster %lx is not free!", ulCluster);
         return ERROR_SECTOR_NOT_FOUND;
         }

      SetBmpEntry(pVolInfo, ulCluster, 1);
      rc = WriteBmpSector(pVolInfo, pVolInfo->ulCurBmpSector);
      if (rc)
         return rc;
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

   if (f32Parms.fMessageActive & LOG_FUNCS)
      Message("UpdateFSInfo");

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
   CritMessage("UpdateFSInfo for %c: failed!", pVolInfo->bDrive + 'A');
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

   if (f32Parms.fMessageActive & LOG_FUNCS)
      Message("MarkDiskStatus, %d", fClean);

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
   if (f32Parms.fMessageActive & LOG_FUNCS)
      Message("SetNextCluster");


   if (GetFatAccess(pVolInfo, "SetNextCluster"))
      return pVolInfo->ulFatEof;

   ulCluster = SetNextCluster2(pVolInfo, ulCluster, ulNext);
   ReleaseFat(pVolInfo);
   return ulCluster;
}

/******************************************************************
*
******************************************************************/
ULONG SetNextCluster2(PVOLINFO pVolInfo, ULONG ulCluster, ULONG ulNext)
{
ULONG ulNewCluster = 0;
BOOL fUpdateFSInfo, fClean;
ULONG ulReturn = 0;
USHORT rc;

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
      ulNext = SetNextCluster2(pVolInfo, FAT_ASSIGN_NEW, pVolInfo->ulFatEof);
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
      MarkCluster2(pVolInfo, ulCluster, (BOOL)ulNext);
      }
#endif

   if (ReadFatSector(pVolInfo, GetFatEntrySec(pVolInfo, ulCluster)))
      return pVolInfo->ulFatEof;

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
      return pVolInfo->ulFatEof;

/*
   if (fUpdateFSInfo)
      UpdateFSInfo(pVolInfo);
*/

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

   if (f32Parms.fMessageActive & LOG_FUNCS)
      Message("GetFreeCluster");

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
   while (ClusterInUse2(pVolInfo, ulCluster))
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

   if (f32Parms.fMessageActive & LOG_FUNCS)
      Message("RemoveFindEntry");

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
         CritMessage("FAT32: Protection VIOLATION in RemoveFindEntry! (SYS%d)", rc);
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
   CritMessage("FAT32: RemoveFindEntry: Entry not found!");
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
UCHAR  szLongName1[FAT32MAXPATH];
USHORT pusUniName[256];
USHORT uniName[15];
USHORT uniName1[15];
PSZ pszLongName1 = szLongName1;
PUSHORT p, q, p1;
PSZ     r;

   if (!pszLongName || !strlen(pszLongName))
      return pDir;

   // @todo Use upcase table
   strcpy(pszLongName1, pszLongName);
   FSH_UPPERCASE(pszLongName1, FAT32MAXPATH, pszLongName1);

   //pusUniName = malloc(256 * sizeof(USHORT));

   usNeededEntries = ( DBCSStrlen( pszLongName ) + 14 ) / 15;

   if (!usNeededEntries)
      return pDir;

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

   //free(pusUniName);

   return pLN;
}

#endif

/******************************************************************
*
******************************************************************/
USHORT MakeDirEntry(PVOLINFO pVolInfo, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo,
                    PDIRENTRY pNew, PDIRENTRY1 pNewStream, PSZ pszName)
{
   if (f32Parms.fMessageActive & LOG_FUNCS)
      Message("MakeDirEntry %s", pszName);

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
         pNew1->u.File.ulLastModifiedTimestp.hour = pGI->day;
         pNew1->u.File.ulLastModifiedTimestp.minutes = pGI->minutes;
         pNew1->u.File.ulLastModifiedTimestp.seconds = pGI->seconds;

         pNew1->u.File.ulCreateTimestp = pNew1->u.File.ulLastModifiedTimestp;
         pNew1->u.File.ulLastAccessedTimestp = pNew1->u.File.ulLastModifiedTimestp;
         }
#endif
      }

   return ModifyDirectory(pVolInfo, ulDirCluster, pDirSHInfo, MODIFY_DIR_INSERT,
      NULL, pNew, NULL, pNewStream, pszName, 0);
}



/******************************************************************
*
******************************************************************/
USHORT MakeShortName(PVOLINFO pVolInfo, ULONG ulDirCluster, PSZ pszLongName, PSZ pszShortName)
{
USHORT usLongName;
PSZ pLastDot;
PSZ pFirstDot;
PSZ p;
USHORT usIndex;
BYTE szShortName[12];
PSZ  pszUpper;
USHORT rc;

   if (f32Parms.fMessageActive & LOG_FUNCS)
      Message("MakeShortName for %s, dircluster %lu",
         pszLongName, ulDirCluster);

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
      BYTE   szFileName[25];
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
         itoa(usNum, szNumber, 10);

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
         ulCluster = FindPathCluster(pVolInfo, ulDirCluster, szFileName, NULL, NULL, NULL, NULL); ////
         if (ulCluster == pVolInfo->ulFatEof)
            break;
         }
      if (usNum < 32000)
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
         return LONGNAME_ERROR;
      }
   memcpy(pszShortName, szShortName, 11);
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
      if (f32Parms.fMessageActive  & LOG_FUNCS)
         Message("DeleteFatChain for cluster %lu", ulCluster);
      }
   else
      {
      Message("DeleteFatChain for invalid cluster %lu (ERROR)", ulCluster);
      return FALSE;
      }

   if (GetFatAccess(pVolInfo, "DeleteFatChain"))
      return FALSE;

   ulSector = GetFatEntrySec(pVolInfo, ulCluster);
   ReadFatSector(pVolInfo, ulSector);

#ifdef EXFAT
   if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
      {
      ulBmpSector = GetAllocBitmapSec(pVolInfo, ulCluster);
      ReadBmpSector(pVolInfo, ulSector);
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

      ulSector = GetFatEntrySec(pVolInfo, ulCluster);
      if (ulSector != pVolInfo->ulCurFatSector)
         {
         rc = WriteFatSector(pVolInfo, pVolInfo->ulCurFatSector);
         if (rc)
            {
            ReleaseFat(pVolInfo);
            return FALSE;
            }
         ReadFatSector(pVolInfo, ulSector);
         }
      ulNextCluster = GetFatEntry(pVolInfo, ulCluster);
      SetFatEntry(pVolInfo, ulCluster, 0L);

#ifdef EXFAT
      if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
         {
         // modify exFAT allocation bitmap as well
         ulBmpSector = GetAllocBitmapSec(pVolInfo, ulCluster);
         if (ulBmpSector != pVolInfo->ulCurBmpSector)
            {
            rc = WriteBmpSector(pVolInfo, pVolInfo->ulCurBmpSector);
            if (rc)
               {
               ReleaseFat(pVolInfo);
               return FALSE;
               }
            ReadBmpSector(pVolInfo, ulBmpSector);
            }
         SetBmpEntry(pVolInfo, ulCluster, 0);
         }
#endif

      ulClustersFreed++;
      ulCluster = ulNextCluster;
      }
   rc = WriteFatSector(pVolInfo, pVolInfo->ulCurFatSector);

   if (rc)
      {
      ReleaseFat(pVolInfo);
      return FALSE;
      }

#ifdef EXFAT
   if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
      {
      rc = WriteBmpSector(pVolInfo, pVolInfo->ulCurBmpSector);

      if (rc)
         {
         ReleaseFat(pVolInfo);
         return FALSE;
         }
      }
#endif

   pVolInfo->pBootFSInfo->ulFreeClusters += ulClustersFreed;
/*   UpdateFSInfo(pVolInfo);*/

   ReleaseFat(pVolInfo);

   return TRUE;
}

ULONG SeekToCluster(PVOLINFO pVolInfo, PSHOPENINFO pSHInfo, ULONG ulCluster, LONGLONG llPosition)
{
ULONG  ulSector = 0;

   if (f32Parms.fMessageActive & LOG_FUNCS)
      Message("SeekToCluster");

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
         if (ulCluster < pSHInfo->ulLastCluster)
            return ulCluster;
         else
            return pVolInfo->ulFatEof;
         }
      }
#endif

   if (GetFatAccess(pVolInfo, "SeekToCluster"))
      return pVolInfo->ulFatEof;

   while (ulCluster != pVolInfo->ulFatEof &&
#ifdef INCL_LONGLONG
          llPosition >= (LONGLONG)pVolInfo->ulClusterSize)
#else
          iGreaterEUL(llPosition, pVolInfo->ulClusterSize))
#endif
      {
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
      }
   ReleaseFat(pVolInfo);

   return ulCluster;
}

void SetSHInfo1(PVOLINFO pVolInfo, PDIRENTRY1 pStreamEntry, PSHOPENINFO pSHInfo)
{
   memset(pSHInfo, 0, sizeof(PSHOPENINFO));
   pSHInfo->fNoFatChain = pStreamEntry->u.Stream.bNoFatChain & 1;
   pSHInfo->ulStartCluster = pStreamEntry->u.Stream.ulFirstClus;
   pSHInfo->ulLastCluster = GetLastCluster(pVolInfo, pStreamEntry->u.Stream.ulFirstClus, pStreamEntry);
}

ULONG GetLastCluster(PVOLINFO pVolInfo, ULONG ulCluster, PDIRENTRY1 pDirEntryStream)
{
ULONG  ulReturn = 0;

   if (f32Parms.fMessageActive & LOG_FUNCS)
      Message("GetLastCluster");

   if (!ulCluster)
      return pVolInfo->ulFatEof;

#ifdef EXFAT
   if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
      {
      if (pDirEntryStream->u.Stream.bNoFatChain & 1)
         {
#ifdef INCL_LONGLONG
         return ulCluster + pDirEntryStream->u.Stream.ullDataLen / pVolInfo->ulClusterSize - 1;
#else
         return ulCluster + DivUL(pDirEntryStream->u.Stream.ullDataLen, pVolInfo->ulClusterSize).ulLo - 1;
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


   if (f32Parms.fMessageActive & LOG_FUNCS)
      Message("CopyChain, cluster %lu", ulCluster);

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
      return 1340;

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

   if (f32Parms.fMessageActive & LOG_FUNCS)
      Message("GetChainSize");

   if (ulCluster == 1)
      {
      // FAT12/FAT16 root directory starting sector
      ulSector = pVolInfo->BootSect.bpb.ReservedSectors +
         pVolInfo->BootSect.bpb.SectorsPerFat * pVolInfo->BootSect.bpb.NumberOfFATs;
      usSectorsPerBlock = (USHORT)pVolInfo->SectorsPerCluster /
         (USHORT)(pVolInfo->ulClusterSize / pVolInfo->ulBlockSize);
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
         if (usSectorsRead * pVolInfo->BootSect.bpb.BytesPerSector >=
            pVolInfo->BootSect.bpb.RootDirEntries * sizeof(DIRENTRY))
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
         pDirBlock->bEntryType &= ~ENTRY_TYPE_IN_USE_STATUS;
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
PDIRENTRY1 pTar, pMax, pFirstFree;
USHORT usFreeEntries;
BOOL bLoop;


   pMax = (PDIRENTRY1)((PBYTE)pStart + ulSize);
   bLoop = pMax == pStart;
   pFirstFree = pMax;
   usFreeEntries = 0;
   while (( pFirstFree != pStart ) || bLoop )
      {
      //if (!(pFirstFree - 1)->bFileName[0])
      if ((pFirstFree - 1)->bEntryType == ENTRY_TYPE_EOD)
         usFreeEntries++;
      else
         break;
      bLoop = FALSE;
      pFirstFree--;
      }

   //if ((( pFirstFree == pStart ) && !bLoop ) || (pFirstFree - 1)->bAttr != FILE_LONGNAME)
   if ( (( pFirstFree == pStart ) && !bLoop ) )
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
      if (pStart->bEntryType & ENTRY_TYPE_IN_USE_STATUS)
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
      //memset(pFirstFree, DELETED_ENTRY, usFreeEntries * sizeof (DIRENTRY));
      memset(pFirstFree, ENTRY_TYPE_EOD, usFreeEntries * sizeof (DIRENTRY));
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
         rc = CritMessage("FAT32: Timeout on semaphore for %s", pszText);
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

   Message("GetFatAccess: %s", pszName);
   rc = SemRequest(&ulSemRWFat, TO_INFINITE, pszName);
   if (rc)
      {
      Message("ERROR: SemRequest GetFatAccess Failed, rc = %d!", rc);
      CritMessage("FAT32: SemRequest GetFatAccess Failed, rc = %d!", rc);
      Message("GetFatAccess Failed for %s, rc = %d", pszName, rc);
      return rc;
      }
   return 0;
}

VOID ReleaseFat(PVOLINFO pVolInfo)
{
   pVolInfo = pVolInfo;
   Message("ReleaseFat");
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
