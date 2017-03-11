#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>

#define INCL_DOS
#define INCL_DOSDEVIOCTL
#define INCL_DOSDEVICES
#define INCL_DOSERRORS
#define INCL_LONGLONG

#include "os2.h"
#include "portable.h"
#include "fat32ifs.h"

#define SECTORS_OF_2GB  (( LONG )( 2UL * 1024 * 1024 * 1024 / SECTOR_SIZE ))

PRIVATE volatile PSHOPENINFO pGlobSH = NULL;

PRIVATE VOID ResetAllCurrents(POPENINFO pOI);

ULONG PositionToOffset(PVOLINFO pVolInfo, POPENINFO pOpenInfo, ULONGLONG ullOffset);
PRIVATE USHORT NewSize(PVOLINFO pVolInfo,
    struct sffsi far * psffsi,      /* psffsi   */
    struct sffsd far * psffsd,      /* psffsd   */
    ULONGLONG ullLen,
    USHORT usIOFlag);

/******************************************************************
*
******************************************************************/
int far pascal _loadds FS_OPENCREATE(
    struct cdfsi far * pcdfsi,      /* pcdfsi       */
    void far * pcdfsd,     /* pcdfsd      */
    char far * pName,           /* pName        */
    unsigned short usCurDirEnd,     /* iCurDirEnd       */
    struct sffsi far * psffsi,      /* psffsi       */
    struct sffsd far * psffsd,      /* psffsd       */
    unsigned long ulOpenMode,       /* fhandflag/openmode   */
    unsigned short usOpenFlag,      /* openflag     */
    unsigned short far * pAction,   /* pAction      */
    unsigned short usAttr,      /* attr         */
    char far * pEABuf,          /* pEABuf       */
    unsigned short far * pfGenFlag  /* pfgenFlag        */
)
{
PVOLINFO pVolInfo;
ULONG    ulCluster;
ULONG    ulDirCluster;
PSZ      pszFile;
DIRENTRY DirEntry;
POPENINFO pOpenInfo = NULL;
USHORT   usIOMode;
ULONGLONG size;
USHORT rc;

   _asm push es;

   size = psffsi->sfi_size;

   if (f32Parms.fLargeFiles)
      size = psffsi->sfi_sizel;

   usIOMode = 0;
   if (ulOpenMode & OPEN_FLAGS_NO_CACHE)
      usIOMode |= DVIO_OPNCACHE;
   if (ulOpenMode & OPEN_FLAGS_WRITE_THROUGH)
      usIOMode |= DVIO_OPWRTHRU;

   if (f32Parms.fMessageActive & LOG_FS)
      {
      Message("FS_OPENCREATE for %s mode %lX, Flag %X, IOMode %X, selfsfn=%u",
         pName, ulOpenMode, usOpenFlag, usIOMode, psffsi->sfi_selfsfn);
      Message("              attribute %X, pEABuf %lX", usAttr, pEABuf);
      }

   pVolInfo = GetVolInfo(pcdfsi->cdi_hVPB);

   if (! pVolInfo)
      {
      rc = ERROR_INVALID_DRIVE;
      goto FS_OPENCREATEEXIT;
      }

   if (pVolInfo->fFormatInProgress && !(ulOpenMode & OPEN_FLAGS_DASD))
      {
      rc = ERROR_ACCESS_DENIED;
      goto FS_OPENCREATEEXIT;
      }

   if (IsDriveLocked(pVolInfo))
      {
      rc = ERROR_DRIVE_LOCKED;
      goto FS_OPENCREATEEXIT;
      }

   *pAction = 0;

   if (strlen(pName) > FAT32MAXPATH)
      {
      rc = ERROR_FILENAME_EXCED_RANGE;
      goto FS_OPENCREATEEXIT;
      }

   pOpenInfo = malloc(sizeof (OPENINFO));
   if (!pOpenInfo)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto FS_OPENCREATEEXIT;
      }
   memset(pOpenInfo, 0, sizeof (OPENINFO));
   *(POPENINFO *)psffsd = pOpenInfo;

   if ((ulOpenMode & OPEN_ACCESS_EXECUTE) == OPEN_ACCESS_EXECUTE)
      {
      ulOpenMode &= ~OPEN_ACCESS_EXECUTE;
      ulOpenMode |= OPEN_ACCESS_READONLY;
      }

   if (!(ulOpenMode & OPEN_FLAGS_DASD))
      {
      BYTE szLongName[ FAT32MAXPATH ];

      if( TranslateName(pVolInfo, 0L, pName, szLongName, TRANSLATE_SHORT_TO_LONG ))
         strcpy( szLongName, pName );

      pOpenInfo->pSHInfo = GetSH( szLongName, pOpenInfo);
      if (!pOpenInfo->pSHInfo)
         {
         rc = ERROR_TOO_MANY_OPEN_FILES;
         goto FS_OPENCREATEEXIT;
         }
      //pOpenInfo->pSHInfo->sOpenCount++; //
      if (pOpenInfo->pSHInfo->fLock)
         {
         rc = ERROR_ACCESS_DENIED;
         goto FS_OPENCREATEEXIT;
         }
      ulDirCluster = FindDirCluster(pVolInfo,
         pcdfsi,
         pcdfsd,
         pName,
         usCurDirEnd,
         RETURN_PARENT_DIR,
         &pszFile);
      if (ulDirCluster == FAT_EOF)
         {
         rc = ERROR_PATH_NOT_FOUND;
         goto FS_OPENCREATEEXIT;
         }

#if 0
      if (f32Parms.fEAS)
         {
         if (IsEASFile(pszFile))
            {
            rc = ERROR_ACCESS_DENIED;
            goto FS_OPENCREATEEXIT;
            }
         }
#endif

      ulCluster = FindPathCluster(pVolInfo, ulDirCluster, pszFile, &DirEntry, NULL);
      if (pOpenInfo->pSHInfo->sOpenCount > 1)
         {
         if (pOpenInfo->pSHInfo->bAttr & FILE_DIRECTORY)
            {
            rc = ERROR_ACCESS_DENIED;
            goto FS_OPENCREATEEXIT;
            }

         if (f32Parms.fMessageActive & LOG_FS)
            Message("File has been previously opened!");
         if (ulCluster != pOpenInfo->pSHInfo->ulStartCluster)
            Message("ulCluster with this open and the previous one are not the same!");
         //ulCluster    = pOpenInfo->pSHInfo->ulStartCluster;
         //DirEntry.bAttr = pOpenInfo->pSHInfo->bAttr;
         }

      if (ulCluster == FAT_EOF)
         {
         if (!(usOpenFlag & FILE_CREATE))
            {
            rc = ERROR_OPEN_FAILED;
            goto FS_OPENCREATEEXIT;
            }

         if (pVolInfo->fWriteProtected)
            {
            rc = ERROR_WRITE_PROTECT;
            goto FS_OPENCREATEEXIT;
            }

         }
      else
         {
         if (DirEntry.bAttr & FILE_DIRECTORY)
            {
            rc = ERROR_ACCESS_DENIED;
            goto FS_OPENCREATEEXIT;
            }

         if (!(usOpenFlag & (FILE_OPEN | FILE_TRUNCATE)))
            {
            rc = ERROR_OPEN_FAILED;
            goto FS_OPENCREATEEXIT;
            }

         if (DirEntry.bAttr & FILE_READONLY &&
           (ulOpenMode & OPEN_ACCESS_WRITEONLY ||
               ulOpenMode & OPEN_ACCESS_READWRITE))
            {
            if ((psffsi->sfi_type & STYPE_FCB) && (ulOpenMode & OPEN_ACCESS_READWRITE))
               {
               ulOpenMode &= ~(OPEN_ACCESS_WRITEONLY | OPEN_ACCESS_READWRITE);
               psffsi->sfi_mode &= ~(OPEN_ACCESS_WRITEONLY | OPEN_ACCESS_READWRITE);
               }
            else
               {
               rc = ERROR_ACCESS_DENIED;
               goto FS_OPENCREATEEXIT;
               }
            }
         }

      if (!pVolInfo->fDiskCleanOnMount &&
        (ulOpenMode & OPEN_ACCESS_WRITEONLY ||
         ulOpenMode & OPEN_ACCESS_READWRITE))
         {
         rc = ERROR_VOLUME_DIRTY;
         goto FS_OPENCREATEEXIT;
         }

      if ( !pVolInfo->fDiskCleanOnMount && !f32Parms.fReadonly &&
           !(ulOpenMode & OPEN_FLAGS_DASD) )
         {
         rc = ERROR_VOLUME_DIRTY;
         goto FS_OPENCREATEEXIT;
         }

      if (ulCluster == FAT_EOF)
         {
         memset(&DirEntry, 0, sizeof (DIRENTRY));
         DirEntry.bAttr = (BYTE)(usAttr & (FILE_READONLY | FILE_HIDDEN | FILE_SYSTEM | FILE_ARCHIVED));
         ulCluster = 0;

         if (f32Parms.fLargeFiles)
            {
            if (size > (ULONGLONG)ULONG_MAX)
               size = (ULONGLONG)ULONG_MAX;
            }
         else
            {
            if (size > (ULONGLONG)LONG_MAX)
               size = (ULONGLONG)LONG_MAX;
            }

         if (size > 0)
            {
            ULONG ulClustersNeeded = size / pVolInfo->ulClusterSize +
                  (size % pVolInfo->ulClusterSize ? 1:0);
            ulCluster = MakeFatChain(pVolInfo, FAT_EOF, ulClustersNeeded, NULL);
            if (ulCluster != FAT_EOF)
               {
               DirEntry.wCluster = LOUSHORT(ulCluster);
               DirEntry.wClusterHigh = HIUSHORT(ulCluster);
               DirEntry.ulFileSize = size;
               }
            else
               {
               rc = ERROR_DISK_FULL;
               goto FS_OPENCREATEEXIT;
               }
            }

         rc = MakeDirEntry(pVolInfo, ulDirCluster, &DirEntry, pszFile);
         if (rc)
            goto FS_OPENCREATEEXIT;

         memcpy(&psffsi->sfi_ctime, &DirEntry.wCreateTime, sizeof (USHORT));
         memcpy(&psffsi->sfi_cdate, &DirEntry.wCreateDate, sizeof (USHORT));
         psffsi->sfi_atime = 0;
         memcpy(&psffsi->sfi_adate, &DirEntry.wAccessDate, sizeof (USHORT));
         memcpy(&psffsi->sfi_mtime, &DirEntry.wLastWriteTime, sizeof (USHORT));
         memcpy(&psffsi->sfi_mdate, &DirEntry.wLastWriteDate, sizeof (USHORT));

         pOpenInfo->pSHInfo->fMustCommit = TRUE;

         psffsi->sfi_tstamp = ST_SCREAT | ST_PCREAT | ST_SREAD | ST_PREAD | ST_SWRITE | ST_PWRITE;

         *pfGenFlag = 0;
         if (f32Parms.fEAS && pEABuf && pEABuf != MYNULL)
            {
            rc = usModifyEAS(pVolInfo, ulDirCluster, pszFile, (PEAOP)pEABuf);
            if (rc)
               goto FS_OPENCREATEEXIT;
            }
         *pAction   = FILE_CREATED;
         }
      else if (usOpenFlag & FILE_TRUNCATE)
         {
         DIRENTRY DirOld;

         if (pVolInfo->fWriteProtected)
            {
            rc = ERROR_WRITE_PROTECT;
            goto FS_OPENCREATEEXIT;
            }

         if ((DirEntry.bAttr & FILE_HIDDEN) && !(usAttr & FILE_HIDDEN))
            {
            rc = ERROR_ACCESS_DENIED;
            goto FS_OPENCREATEEXIT;
            }

         if ((DirEntry.bAttr & FILE_SYSTEM) && !(usAttr & FILE_SYSTEM))
            {
            rc = ERROR_ACCESS_DENIED;
            goto FS_OPENCREATEEXIT;
            }

         memcpy(&DirOld, &DirEntry, sizeof (DIRENTRY));

         DirEntry.bAttr = (BYTE)(usAttr & (FILE_READONLY | FILE_HIDDEN | FILE_SYSTEM | FILE_ARCHIVED));
         DirEntry.wCluster     = 0;
         DirEntry.wClusterHigh = 0;
         DirEntry.ulFileSize   = 0;
         DirEntry.fEAS = FILE_HAS_NO_EAS;

         rc = usDeleteEAS(pVolInfo, ulDirCluster, pszFile);
         if (rc)
            goto FS_OPENCREATEEXIT;
         DirOld.fEAS = FILE_HAS_NO_EAS;

         if (ulCluster)
            DeleteFatChain(pVolInfo, ulCluster);
         pOpenInfo->pSHInfo->ulLastCluster = FAT_EOF;
         ResetAllCurrents(pOpenInfo);
         ulCluster = 0;

         if (f32Parms.fLargeFiles)
            {
            if (size > (ULONGLONG)ULONG_MAX)
               size = (ULONGLONG)ULONG_MAX;
            }
         else
            {
            if (size > (ULONGLONG)LONG_MAX)
               size = (ULONGLONG)LONG_MAX;
            }

         if (size > 0)
            {
            ULONG ulClustersNeeded = size / pVolInfo->ulClusterSize +
                  (size % pVolInfo->ulClusterSize ? 1:0);
            ulCluster = MakeFatChain(pVolInfo, FAT_EOF, ulClustersNeeded, &pOpenInfo->pSHInfo->ulLastCluster);
            if (ulCluster != FAT_EOF)
               {
               DirEntry.wCluster = LOUSHORT(ulCluster);
               DirEntry.wClusterHigh = HIUSHORT(ulCluster);
               DirEntry.ulFileSize = size;
               }
            else
               {
               ulCluster = 0;
               rc = ModifyDirectory(pVolInfo, ulDirCluster, MODIFY_DIR_UPDATE,
                  &DirOld, &DirEntry, NULL, usIOMode);
               if (!rc)
                  rc = ERROR_DISK_FULL;
               goto FS_OPENCREATEEXIT;
               }
            }
         rc = ModifyDirectory(pVolInfo, ulDirCluster, MODIFY_DIR_UPDATE,
            &DirOld, &DirEntry, NULL, usIOMode);
         if (rc)
            goto FS_OPENCREATEEXIT;

         memcpy(&psffsi->sfi_ctime, &DirEntry.wCreateTime, sizeof (USHORT));
         memcpy(&psffsi->sfi_cdate, &DirEntry.wCreateDate, sizeof (USHORT));
         psffsi->sfi_atime = 0;
         memcpy(&psffsi->sfi_adate, &DirEntry.wAccessDate, sizeof (USHORT));
         memcpy(&psffsi->sfi_mtime, &DirEntry.wLastWriteTime, sizeof (USHORT));
         memcpy(&psffsi->sfi_mdate, &DirEntry.wLastWriteDate, sizeof (USHORT));

         pOpenInfo->pSHInfo->fMustCommit = TRUE;

         psffsi->sfi_tstamp = ST_SWRITE | ST_PWRITE | ST_SREAD | ST_PREAD;
         *pfGenFlag = 0;
         if (f32Parms.fEAS && pEABuf && pEABuf != MYNULL)
            {
            rc = usModifyEAS(pVolInfo, ulDirCluster, pszFile, (PEAOP)pEABuf);
            if (rc)
               goto FS_OPENCREATEEXIT;
            }

         *pAction = FILE_TRUNCATED;
         }
      else
         {
         memcpy(&psffsi->sfi_ctime, &DirEntry.wCreateTime, sizeof (USHORT));
         memcpy(&psffsi->sfi_cdate, &DirEntry.wCreateDate, sizeof (USHORT));
         psffsi->sfi_atime = 0;
         memcpy(&psffsi->sfi_adate, &DirEntry.wAccessDate, sizeof (USHORT));
         memcpy(&psffsi->sfi_mtime, &DirEntry.wLastWriteTime, sizeof (USHORT));
         memcpy(&psffsi->sfi_mdate, &DirEntry.wLastWriteDate, sizeof (USHORT));

         psffsi->sfi_tstamp = 0;
         *pAction = FILE_EXISTED;
         *pfGenFlag = ( HAS_CRITICAL_EAS( DirEntry.fEAS ) ? 1 : 0);
         }

#if 0
      rc = FSH_UPPERCASE(pName, sizeof pOpenInfo->pSHInfo->szFileName, pOpenInfo->pSHInfo->szFileName);
      if (rc || !strlen(pOpenInfo->pSHInfo->szFileName))
         {
         Message("OpenCreate: FSH_UPPERCASE failed!, rc = %d", rc);
         strncpy(pOpenInfo->pSHInfo->szFileName, pName, sizeof pOpenInfo->pSHInfo->szFileName);
         rc = 0;
         }
#endif

      if (pOpenInfo->pSHInfo->sOpenCount == 1)
         {
         pOpenInfo->pSHInfo->ulLastCluster = GetLastCluster(pVolInfo, ulCluster);
         pOpenInfo->pSHInfo->bAttr = DirEntry.bAttr;
         }

      pOpenInfo->pSHInfo->ulDirCluster = ulDirCluster;
      pOpenInfo->pSHInfo->ulStartCluster = ulCluster;
      if (ulCluster)
         pOpenInfo->ulCurCluster = ulCluster;
      else
         pOpenInfo->ulCurCluster = FAT_EOF;

      pOpenInfo->ulCurBlock = 0;

      size = DirEntry.ulFileSize;
      psffsi->sfi_DOSattr = DirEntry.bAttr;
      }
   else /* OPEN_FLAGS_DASD */
      {
      if (!usOpenFlag)
         {
         rc = ERROR_FILE_EXISTS;
         goto FS_OPENCREATEEXIT;
         }

      if (usOpenFlag != FILE_OPEN)
         {
         rc = ERROR_ACCESS_DENIED;
         goto FS_OPENCREATEEXIT;
         }

      //size = pVolInfo->BootSect.bpb.BigTotalSectors;
      /* if a less volume than 2GB, do normal IO else sector IO */
      size = pVolInfo->BootSect.bpb.BigTotalSectors * SECTOR_SIZE;
      //if( size < SECTORS_OF_2GB )
      //   size *= SECTOR_SIZE;
      //else
      //   pOpenInfo->fLargeVolume = TRUE;

      psffsi->sfi_ctime = 0;
      psffsi->sfi_cdate = 0;
      psffsi->sfi_atime = 0;
      psffsi->sfi_adate = 0;
      psffsi->sfi_mtime = 0;
      psffsi->sfi_mdate = 0;

      *pAction = FILE_EXISTED;
      }

   psffsi->sfi_position = 0L;

   if (f32Parms.fLargeFiles)
      psffsi->sfi_positionl = 0LL;

   psffsi->sfi_type &= ~STYPE_FCB;
   psffsi->sfi_mode = ulOpenMode;
   pVolInfo->ulOpenFiles++;

   rc = 0;

FS_OPENCREATEEXIT:
   psffsi->sfi_size = size;

   if (f32Parms.fLargeFiles)
      psffsi->sfi_sizel = size;

   if (rc && pOpenInfo)
      {
      if (pOpenInfo->pSHInfo)
         ReleaseSH(pOpenInfo);
      else
         free(pOpenInfo);
      }

   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_OPENCREATE returned %u (Action = %u, OI=%lX)", rc, *pAction, pOpenInfo);

   _asm pop es;

   return rc;
}

/******************************************************************
* GetSH
******************************************************************/
PSHOPENINFO GetSH(PSZ pszFileName, POPENINFO pOI)
{
PSHOPENINFO pSH;

   pSH = pGlobSH;
   while (pSH)
      {
      if (!stricmp(pSH->szFileName, pszFileName))
         break;
      pSH = (PSHOPENINFO)pSH->pNext;
      }

   if (!pSH)
      {
      pSH = malloc(sizeof (SHOPENINFO));
      if (!pSH)
         return NULL;
      memset(pSH, 0, sizeof (SHOPENINFO));
      strcpy(pSH->szFileName, pszFileName);

      pSH->pNext = (PVOID)pGlobSH;
      pGlobSH = pSH;
      }

   pSH->sOpenCount++;

   pOI->pNext = pSH->pChild;
   pSH->pChild = pOI;

   return pSH;
}

/******************************************************************
*  Release shared info
******************************************************************/
BOOL ReleaseSH(POPENINFO pOI)
{
PSHOPENINFO pSH2;
PSHOPENINFO pSH = pOI->pSHInfo;
POPENINFO pOI2;
USHORT rc;

   /*
      Remove the openinfo from the chain
   */

   if ((POPENINFO)pSH->pChild == pOI)
      pSH->pChild = pOI->pNext;
   else
      {
      pOI2 = pSH->pChild;
      while (pOI2)
         {
         rc = MY_PROBEBUF(PB_OPREAD, (PBYTE)pOI2, sizeof (OPENINFO));
         if (rc)
            {
            CritMessage("FAT32: Protection VIOLATION (OpenInfo) in ReleaseSH! (SYS%d)", rc);
            Message("FAT32: Protection VIOLATION (OpenInfo) in ReleaseSH! (SYS%d)", rc);
            return FALSE;
            }
         if ((POPENINFO)pOI2->pNext == pOI)
            {
            pOI2->pNext = pOI->pNext;
            break;
            }
         pOI2 = (POPENINFO)pOI2->pNext;
         }
      if (!pOI2)
         {
         CritMessage("FAT32: ReleaseSH: Error cannot find OI for %s!", pSH->szFileName);
         Message("FAT32: ReleaseSH: Error cannot find OI for %s!", pSH->szFileName);
         }
      }
   free(pOI);

   /*
      Now release the SHOPENINFO if needed
   */

   pSH->sOpenCount--;
   if (pSH->sOpenCount > 0)
      return TRUE;

   if (pGlobSH == pSH)
      pGlobSH = pSH->pNext;
   else
      {
      pSH2 = pGlobSH;
      while (pSH2)
         {
         rc = MY_PROBEBUF(PB_OPREAD, (PBYTE)pSH2, sizeof (SHOPENINFO));
         if (rc)
            {
            CritMessage("FAT32: Protection VIOLATION (SHOpenInfo) in ReleaseSH! (SYS%d)", rc);
            Message("FAT32: Protection VIOLATION (SHOpenInfo) in ReleaseSH! (SYS%d)", rc);
            return FALSE;
            }
         if ((PSHOPENINFO)pSH2->pNext == pSH)
            {
            pSH2->pNext = pSH->pNext;
            break;
            }
         pSH2 = (PSHOPENINFO)pSH2->pNext;
         }
      if (!pSH2)
         {
         CritMessage("FAT32: ReleaseSH: Error cannot find SH for %s!", pSH->szFileName);
         Message("FAT32: ReleaseSH: Error cannot find SH for %s!", pSH->szFileName);
         }
      }

   free(pSH);
   return TRUE;
}

VOID ResetAllCurrents(POPENINFO pOI)
{
PSHOPENINFO pSH = pOI->pSHInfo;

   pOI = (POPENINFO)pSH->pChild;
   while (pOI)
      {
      pOI->ulCurCluster = FAT_EOF;
      pOI = (POPENINFO)pOI->pNext;
      }
}

/******************************************************************
*
******************************************************************/
int far pascal _loadds FS_CLOSE(
    unsigned short usType,      /* close type   */
    unsigned short IOFlag,      /* IOflag   */
    struct sffsi far * psffsi,      /* psffsi   */
    struct sffsd far * psffsd       /* psffsd   */
)
{
POPENINFO pOpenInfo;
PVOLINFO pVolInfo;
USHORT  rc = 0;

   _asm push es;

   pOpenInfo = GetOpenInfo(psffsd);
   pVolInfo = GetVolInfo(psffsi->sfi_hVPB);

   if (! pVolInfo)
      {
      rc = ERROR_INVALID_DRIVE;
      goto FS_CLOSEEXIT;
      }

   if (f32Parms.fMessageActive & LOG_FS)
      {
      if (psffsi->sfi_mode & OPEN_FLAGS_DASD)
         Message("FS_CLOSE (DASD) type %u:", usType);
      else
         Message("FS_CLOSE of %s, type = %u OI=%lX",
            pOpenInfo->pSHInfo->szFileName,
            usType,
            pOpenInfo);
      }

   if (IsDriveLocked(pVolInfo))
      {
      rc = ERROR_DRIVE_LOCKED;
      goto FS_CLOSEEXIT;
      }

   if (usType == FS_CL_FORSYS)
      {
      if (!pVolInfo->ulOpenFiles)
         {
         Message("FAT32 - FS_CLOSE: Error openfile count would become negative!");
         CritMessage("FAT32 - FS_CLOSE: Error openfile count would become negative!");
         }
      else
         pVolInfo->ulOpenFiles--;
      }

   if (psffsi->sfi_mode & OPEN_FLAGS_DASD)
      {
      rc = 0;
      goto FS_CLOSEEXIT;
      }

   if (pOpenInfo->pSHInfo->fMustCommit && !pVolInfo->fWriteProtected)
      {
      if (usType != FS_CL_ORDINARY || IOFlag & DVIO_OPWRTHRU)
         rc = FS_COMMIT(FS_COMMIT_ONE, IOFlag, psffsi, psffsd);
      }

   if (usType == FS_CL_FORSYS)
      ReleaseSH(pOpenInfo);

FS_CLOSEEXIT:
   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_CLOSE returned %u", rc);

   _asm pop es;

   return rc;
}

/******************************************************************
*
******************************************************************/
int far pascal _loadds FS_READ(
    struct sffsi far * psffsi,  /* psffsi   */
    struct sffsd far * psffsd,  /* psffsd   */
    char far * pData,           /* pData    */
    unsigned short far * pLen,  /* pLen     */
    unsigned short usIOFlag     /* IOflag   */
)
{
USHORT rc;
PVOLINFO pVolInfo;
POPENINFO pOpenInfo;
USHORT usBytesRead;
USHORT usBytesToRead;
PBYTE  pbCluster;
ULONG  ulClusterSector;
USHORT usClusterOffset;
USHORT usBlockOffset;
ULONG  ulBytesPerCluster;
ULONG  ulBytesPerBlock;
LONGLONG pos;
ULONGLONG size;

   _asm push es;

   pOpenInfo = GetOpenInfo(psffsd);

   usBytesToRead = *pLen;
   usBytesRead = 0;
   *pLen = 0;
   pbCluster = NULL;

   size = (ULONGLONG)psffsi->sfi_size;
   pos  = (LONGLONG)psffsi->sfi_position;

   if (f32Parms.fLargeFiles)
      {
      size = (ULONGLONG)psffsi->sfi_sizel;
      pos  = (LONGLONG)psffsi->sfi_positionl;
      }

   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_READ, %u bytes at offset %lld",
         usBytesToRead, pos);

   pVolInfo = GetVolInfo(psffsi->sfi_hVPB);

   if (! pVolInfo)
      {
      rc = ERROR_INVALID_DRIVE;
      goto FS_READEXIT;
      }

   if (pVolInfo->fFormatInProgress && !(psffsi->sfi_mode & OPEN_FLAGS_DASD))
      {
      rc = ERROR_ACCESS_DENIED;
      goto FS_READEXIT;
      }

   if (IsDriveLocked(pVolInfo))
   {
      rc = ERROR_DRIVE_LOCKED;
      goto FS_READEXIT;
   }

   if (!((psffsi->sfi_mode & 0xFUL) == OPEN_ACCESS_READONLY) &&
       !((psffsi->sfi_mode & 0xFUL) == OPEN_ACCESS_READWRITE))
      {
      rc = ERROR_ACCESS_DENIED;
      goto FS_READEXIT;
      }

   //if ((psffsi->sfi_mode & OPEN_FLAGS_DASD ) &&
   //    pOpenInfo->fLargeVolume && !pOpenInfo->fSectorMode )
   //   {
   //   /* User didn't enable sector IO on the larger volume than 2GB */
   //   rc = ERROR_ACCESS_DENIED;
   //   goto FS_READEXIT;
   //   }

   if (!usBytesToRead)
      {
      rc = NO_ERROR;
      goto FS_READEXIT;
      }

   pbCluster = malloc(pVolInfo->ulBlockSize);
   if (!pbCluster)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto FS_READEXIT;
      }

    if (psffsi->sfi_mode & OPEN_FLAGS_DASD)
    {
        ULONG   ulSector;
        USHORT  usSectorOffset;
        USHORT  usRemaining;
        USHORT  usBytesPerSector;
        USHORT  usNumSectors;

        char far *pBufPosition = pData;

        usBytesPerSector    = pVolInfo->BootSect.bpb.BytesPerSector;
        usBytesRead         = 0;

        if (pOpenInfo->fSectorMode)
        {
            USHORT usTotNumBytes;

            if (usBytesToRead > (USHRT_MAX/usBytesPerSector))
            {
                rc = ERROR_TRANSFER_TOO_LONG;
                goto FS_READEXIT;
            }

            usTotNumBytes = usBytesToRead * usBytesPerSector;

            rc = MY_PROBEBUF(PB_OPWRITE, pBufPosition, usTotNumBytes);
            if (rc)
            {
                Message("Protection VIOLATION in FS_READ! (SYS%d)", rc);
                goto FS_READEXIT;
            }

            /*
                for sector mode, offsets are actually sectors
                and Number of Bytes to read are actually
                Number of sectors to read
            */
            ulSector        = (ULONG)pos;
            usSectorOffset  = 0;
            usRemaining     = 0;
            usNumSectors    = usBytesToRead;
            if (ulSector > (pVolInfo->BootSect.bpb.BigTotalSectors - (ULONG)usNumSectors))
            {
                usNumSectors    = (USHORT)(pVolInfo->BootSect.bpb.BigTotalSectors - ulSector);
            }
            usBytesToRead   = usNumSectors;
        }
        else
        {
            rc = MY_PROBEBUF(PB_OPWRITE, pData, usBytesToRead);
            if (rc)
            {
                Message("Protection VIOLATION in FS_READ! (SYS%d)", rc);
                goto FS_READEXIT;
            }

            ulSector        = (ULONG)(pos / (ULONG)usBytesPerSector);
            usSectorOffset  = (USHORT)(pos % (ULONG)usBytesPerSector);
            usRemaining     = (usSectorOffset + usBytesToRead) > usBytesPerSector ? ((usSectorOffset + usBytesToRead) % usBytesPerSector) : 0;
            usNumSectors    = (usBytesToRead - usRemaining)/usBytesPerSector;

            if (ulSector > (pVolInfo->BootSect.bpb.BigTotalSectors - (ULONG)(usSectorOffset ? 1 : 0) - (ULONG)usNumSectors - (ULONG)(usRemaining ? 1 : 0)))
            {
                usNumSectors    = (USHORT)(pVolInfo->BootSect.bpb.BigTotalSectors - (ULONG)(usSectorOffset ? 1 : 0) - ulSector);
                usRemaining     = 0;
                usBytesToRead   = usNumSectors*usBytesPerSector + ( usSectorOffset ? ( usBytesPerSector - usSectorOffset ) : 0 );
            }
        }

        if (ulSector >= pVolInfo->BootSect.bpb.BigTotalSectors)
        {
            rc = ERROR_SECTOR_NOT_FOUND;
            goto FS_READEXIT;
        }

        if (usSectorOffset)
        {
            USHORT usCurBytesToRead = min(usBytesToRead, usBytesPerSector - usSectorOffset);

            rc = ReadSector(pVolInfo, ulSector, 1, pbCluster, usIOFlag);
            if (rc)
            {
                goto FS_READEXIT;
            }
            memcpy(pBufPosition,pbCluster + usSectorOffset,usCurBytesToRead);
            pBufPosition            += usCurBytesToRead;
            pos                     += usCurBytesToRead;
            usBytesRead             += usCurBytesToRead;
            usBytesToRead           -= usCurBytesToRead;
            ulSector++;
        }

        if (usNumSectors)
        {
            rc = ReadSector(pVolInfo, ulSector, usNumSectors, pBufPosition, usIOFlag);
            if (rc)
            {
                goto FS_READEXIT;
            }
            pBufPosition            += (ULONG)((ULONG)usNumSectors*(ULONG)usBytesPerSector);
            pos                     += usBytesToRead - usRemaining;
            usBytesRead             += usBytesToRead - usRemaining;
            ulSector                += usNumSectors;
        }

        if (usRemaining)
        {
            rc = ReadSector(pVolInfo, ulSector, 1, pbCluster, usIOFlag);
            if (rc)
            {
                goto FS_READEXIT;
            }
            memcpy(pBufPosition,pbCluster,usRemaining);
            pBufPosition            += usRemaining;
            pos                     += usRemaining;
            usBytesRead             += usRemaining;
        }

        *pLen                       = usBytesRead;
        rc = NO_ERROR;
    }
    else
    {
        char far *pBufPosition = pData;

        rc = MY_PROBEBUF(PB_OPWRITE, pData, usBytesToRead);
        if (rc)
        {
            Message("Protection VIOLATION in FS_READ! (SYS%d)", rc);
            goto FS_READEXIT;
        }

        pOpenInfo->pSHInfo->fMustCommit = TRUE;
        if (pOpenInfo->ulCurCluster == FAT_EOF)
           {
           pOpenInfo->ulCurCluster = PositionToOffset(pVolInfo, pOpenInfo, pos);
           pOpenInfo->ulCurBlock = pOpenInfo->ulCurBlock = (pos / pVolInfo->ulBlockSize) % (pVolInfo->ulClusterSize / pVolInfo->ulBlockSize);
           }

        /*
            First, handle the first part that does not align on a cluster border
        */
        ulBytesPerCluster = pVolInfo->ulClusterSize;
        ulBytesPerBlock = pVolInfo->ulBlockSize;
        usClusterOffset   = (USHORT)(pos % ulBytesPerCluster); /* get remainder */
        usBlockOffset   = (USHORT)(pos % ulBytesPerBlock); /* get remainder */
        if
            (
                (pOpenInfo->ulCurCluster != FAT_EOF) &&
                (pos < size) &&
                (usBytesToRead) &&
                (usBlockOffset)
            )
        {
            ULONG   ulCurrBytesToRead;
            USHORT  usSectorsToRead;
            USHORT  usSectorsPerCluster;
            USHORT  usSectorsPerBlock;

            usSectorsPerCluster = pVolInfo->BootSect.bpb.SectorsPerCluster;
            usSectorsPerBlock = ulBytesPerBlock / pVolInfo->BootSect.bpb.BytesPerSector;
            usSectorsToRead = usSectorsPerBlock;

            /* compute the number of bytes
                to the cluster end or
                as much as fits into the user buffer
                or how much remains to be read until file end
               whatever is the smallest
            */
            ulCurrBytesToRead   = (ulBytesPerBlock - (ULONG)usBlockOffset);
            ulCurrBytesToRead   = min(ulCurrBytesToRead,(ULONG)usBytesToRead);
            ulCurrBytesToRead   = (ULONG)min((ULONG)ulCurrBytesToRead,size - pos);
            if (ulCurrBytesToRead)
            {
                ulClusterSector = pVolInfo->ulStartOfData + (pOpenInfo->ulCurCluster-2)*usSectorsPerCluster;

                rc = ReadBlock(pVolInfo, pOpenInfo->ulCurCluster, pOpenInfo->ulCurBlock, pbCluster, usIOFlag);
                if (rc)
                {
                    goto FS_READEXIT;
                }
                memcpy(pBufPosition, pbCluster + usBlockOffset, (USHORT)ulCurrBytesToRead);

                pBufPosition            += (USHORT)ulCurrBytesToRead;
                pos                     += (USHORT)ulCurrBytesToRead;
                usBytesRead             += (USHORT)ulCurrBytesToRead;
                usBytesToRead           -= (USHORT)ulCurrBytesToRead;
            }

            if (((ULONG)usClusterOffset + ulCurrBytesToRead) >= ulBytesPerCluster)
            {
                pOpenInfo->ulCurCluster     = GetNextCluster(pVolInfo, pOpenInfo->ulCurCluster);
                if (!pOpenInfo->ulCurCluster)
                {
                    pOpenInfo->ulCurCluster = FAT_EOF;
                }
            }
            pOpenInfo->ulCurBlock = (pos / pVolInfo->ulBlockSize) % (pVolInfo->ulClusterSize / pVolInfo->ulBlockSize);
        }


        /*
            Second, handle the part that aligns on a cluster border and is a multiple of the cluster size
        */
        if
            (
                (pOpenInfo->ulCurCluster != FAT_EOF) &&
                (pos < size) &&
                (usBytesToRead)
            )
        {
            ULONG   ulCurrCluster       = pOpenInfo->ulCurCluster;
            ULONG   ulNextCluster       = ulCurrCluster;
            ULONG   ulCurrBytesToRead   = 0;
            USHORT  usSectorsPerCluster = pVolInfo->BootSect.bpb.SectorsPerCluster;
            USHORT  usSectorsPerBlock   = ulBytesPerBlock / pVolInfo->BootSect.bpb.BytesPerSector;
            USHORT  usClustersToProcess = 0;
            USHORT  usBlocksToProcess   = 0;
            USHORT  usAdjacentClusters  = 1;
            USHORT  usAdjacentBlocks;
            USHORT  usBlocks;

            ulCurrBytesToRead           = (ULONG)min((ULONGLONG)usBytesToRead,size - pos);

            usClustersToProcess         = (USHORT)(ulCurrBytesToRead / ulBytesPerCluster); /* get the number of full clusters */
            usBlocksToProcess           = (USHORT)(ulCurrBytesToRead / ulBytesPerBlock);   /* get the number of full clusters */
            usAdjacentBlocks            = (usBlocksToProcess % (pVolInfo->ulClusterSize / pVolInfo->ulBlockSize));
            usBlocks                    = usBlocksToProcess;

            if (ulCurrBytesToRead < pVolInfo->ulClusterSize)
               usAdjacentBlocks = ulCurrBytesToRead / pVolInfo->ulBlockSize;

            while (usBlocks && (ulCurrCluster != FAT_EOF))
            {
                ulNextCluster       = GetNextCluster(pVolInfo, ulCurrCluster);
                if (!ulNextCluster)
                {
                    ulNextCluster = FAT_EOF;
                }

                if  (
                        (ulNextCluster != FAT_EOF) &&
                        (ulNextCluster == (ulCurrCluster+1)) &&
                        (usClustersToProcess)
                    )
                {
                    usAdjacentClusters  += 1;
                    usAdjacentBlocks    += pVolInfo->ulClusterSize / pVolInfo->ulBlockSize;
                    usClustersToProcess -= 1;
                }
                else
                {
                    ULONG ulCurrBytesToRead;
                    USHORT usSectorsToRead;

                    if (!usAdjacentBlocks)
                       usAdjacentBlocks = 1;

                    ulCurrBytesToRead = usAdjacentBlocks * ulBytesPerBlock;
                    usSectorsToRead   = usAdjacentBlocks * usSectorsPerBlock;

                    ulClusterSector = pVolInfo->ulStartOfData + (pOpenInfo->ulCurCluster-2)*usSectorsPerCluster;
#if 0
                    /*
                        The following code is fast, but is not compatible
                        with OBJ_ANY attribute
                    */
                    rc = ReadSector(pVolInfo, ulClusterSector,usSectorsToRead,pBufPosition, usIOFlag);
                    if (rc)
                    {
                        goto FS_READEXIT;
                    }
                    pBufPosition                    += (USHORT)ulCurrBytesToRead;
                    pos                             += (USHORT)ulCurrBytesToRead;
                    usBytesRead                     += (USHORT)ulCurrBytesToRead;
                    usBytesToRead                   -= (USHORT)ulCurrBytesToRead;
                    usAdjacentClusters              = 1;
                    pOpenInfo->ulCurCluster         = ulNextCluster;
#else
                    {
                        ULONG ulBlock;

                        while ( usBlocks )
                        {
                           rc = ReadBlock(pVolInfo, pOpenInfo->ulCurCluster, pOpenInfo->ulCurBlock, pbCluster, usIOFlag);
                           if (rc)
                           {
                               goto FS_READEXIT;
                           }

                           pOpenInfo->ulCurBlock++;
                           memcpy( pBufPosition, pbCluster, (USHORT)ulBytesPerBlock );

                           if (pOpenInfo->ulCurBlock >= (pVolInfo->ulClusterSize / pVolInfo->ulBlockSize))
                             {
                             pOpenInfo->ulCurBlock   = 0;
                             ulCurrCluster           = GetNextCluster(pVolInfo, ulCurrCluster);
                             pOpenInfo->ulCurCluster = ulCurrCluster;
                             }

                           usBlocks -= 1;

                           pBufPosition                += ulBytesPerBlock;
                           ulClusterSector             += usSectorsPerBlock;
                           usAdjacentBlocks--;
                        }
                    }

                    pos                             += (USHORT)ulCurrBytesToRead;
                    usBytesRead                     += (USHORT)ulCurrBytesToRead;
                    usBytesToRead                   -= (USHORT)ulCurrBytesToRead;
                    usAdjacentBlocks                = pVolInfo->ulClusterSize / pVolInfo->ulBlockSize;
                    usAdjacentClusters              = 1;

                    if (pOpenInfo->ulCurBlock >= pVolInfo->ulClusterSize / pVolInfo->ulBlockSize)
                       {
                       pOpenInfo->ulCurBlock           = 0;
                       ulCurrCluster                   = ulNextCluster;
                       pOpenInfo->ulCurCluster         = ulNextCluster;
                       }
#endif
                }
            }
        }

        /*
            Third, handle the part that aligns on a cluster border but does not make up a complete cluster
        */
        if
            (
                (pOpenInfo->ulCurCluster != FAT_EOF) &&
                (pos < size) &&
                (usBytesToRead)
            )
        {
            ULONG  ulCurrBytesToRead;
            USHORT usSectorsToRead;
            USHORT usSectorsPerCluster;
            USHORT usSectorsPerBlock;

            usSectorsToRead = ulBytesPerBlock / pVolInfo->BootSect.bpb.BytesPerSector;
            usSectorsPerCluster = pVolInfo->BootSect.bpb.SectorsPerCluster;
            usSectorsPerBlock = usSectorsToRead;

            ulCurrBytesToRead = (ULONG)min((ULONGLONG)usBytesToRead,size - pos);
            if (ulCurrBytesToRead)
            {
                ulClusterSector = pVolInfo->ulStartOfData + (pOpenInfo->ulCurCluster-2)*usSectorsPerCluster;
                rc = ReadBlock(pVolInfo, pOpenInfo->ulCurCluster, pOpenInfo->ulCurBlock, pbCluster, usIOFlag);
                if (rc)
                {
                    goto FS_READEXIT;
                }
                Message("3: read %lu, curblk: %lu", ulCurrBytesToRead, pOpenInfo->ulCurBlock);
                memcpy(pBufPosition,pbCluster, (USHORT)ulCurrBytesToRead);

                pos                     += (USHORT)ulCurrBytesToRead;
                usBytesRead             += (USHORT)ulCurrBytesToRead;
                usBytesToRead           -= (USHORT)ulCurrBytesToRead;
            }
        }

        pOpenInfo->ulCurBlock = (pos / pVolInfo->ulBlockSize) % (pVolInfo->ulClusterSize / pVolInfo->ulBlockSize);

        *pLen = usBytesRead;
        psffsi->sfi_tstamp  |= ST_SREAD | ST_PREAD;
        rc = NO_ERROR;
    }

FS_READEXIT:
   psffsi->sfi_size = (ULONG)size;
   psffsi->sfi_position = (LONG)pos;

   if (f32Parms.fLargeFiles)
      {
      psffsi->sfi_sizel = size;
      psffsi->sfi_positionl = pos;
      }

    if( pbCluster )
        free( pbCluster );

    if (f32Parms.fMessageActive & LOG_FS)
        Message("FS_READ returned %u (%u bytes read)", rc, *pLen);

   _asm pop es;

    return rc;
}

/******************************************************************
*
******************************************************************/
int far pascal _loadds FS_WRITE(
    struct sffsi far * psffsi,      /* psffsi   */
    struct sffsd far * psffsd,      /* psffsd   */
    char far * pData,           /* pData    */
    unsigned short far * pLen,  /* pLen     */
    unsigned short usIOFlag     /* IOflag   */
)
{
USHORT rc;
PVOLINFO pVolInfo;
POPENINFO pOpenInfo;
USHORT usBytesWritten;
USHORT usBytesToWrite;
PBYTE  pbCluster;
ULONG  ulClusterSector;
USHORT usClusterOffset;
USHORT usBlockOffset;
ULONG  ulBytesPerCluster;
ULONG  ulBytesPerBlock;
LONGLONG pos;
ULONGLONG size;

   _asm push es;

   pOpenInfo = GetOpenInfo(psffsd);

   usBytesToWrite = *pLen;
   usBytesWritten = 0;
   *pLen = 0;
   pbCluster = NULL;

   size = (ULONGLONG)psffsi->sfi_size;
   pos  = (LONGLONG)psffsi->sfi_position;

   if (f32Parms.fLargeFiles)
      {
      size = psffsi->sfi_sizel;
      pos  = psffsi->sfi_positionl;
      }

   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_WRITE, %u bytes at offset %lld, pData=%lx, Len=%u, ioflag %X, size = %llu",
      usBytesToWrite, pos, pData, *pLen, usIOFlag, size);

   pVolInfo = GetVolInfo(psffsi->sfi_hVPB);

   if (! pVolInfo)
      {
      rc = ERROR_INVALID_DRIVE;
      goto FS_WRITEEXIT;
      }

   if (pVolInfo->fFormatInProgress && !(psffsi->sfi_mode & OPEN_FLAGS_DASD))
      {
      rc = ERROR_ACCESS_DENIED;
      goto FS_WRITEEXIT;
      }

   if (IsDriveLocked(pVolInfo))
   {
      rc = ERROR_DRIVE_LOCKED;
      goto FS_WRITEEXIT;
   }


   if (pVolInfo->fWriteProtected)
   {
      rc = ERROR_WRITE_PROTECT;
      goto FS_WRITEEXIT;
   }

   if (!((psffsi->sfi_mode & 0xFUL) == OPEN_ACCESS_WRITEONLY) &&
       !((psffsi->sfi_mode & 0xFUL) == OPEN_ACCESS_READWRITE))
      {
      rc = ERROR_ACCESS_DENIED;
      goto FS_WRITEEXIT;
      }

   //if ((psffsi->sfi_mode & OPEN_FLAGS_DASD ) &&
   //    pOpenInfo->fLargeVolume && !pOpenInfo->fSectorMode )
   //   {
   //   /* User didn't enable sector IO on the larger volume than 2GB */
   //   rc = ERROR_ACCESS_DENIED;
   //   goto FS_WRITEEXIT;
   //   }

   if (!usBytesToWrite)
      {
      rc = NO_ERROR;
      goto FS_WRITEEXIT;
      }

   pbCluster = malloc(pVolInfo->ulBlockSize);
   if (!pbCluster)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto FS_WRITEEXIT;
      }

    if (psffsi->sfi_mode & OPEN_FLAGS_DASD)
    {
        ULONG   ulSector;
        USHORT  usSectorOffset;
        USHORT  usRemaining;
        USHORT  usBytesPerSector;
        USHORT  usNumSectors;

        char far *pBufPosition = pData;

        usBytesPerSector    = pVolInfo->BootSect.bpb.BytesPerSector;
        usBytesWritten      = 0;

        if (pOpenInfo->fSectorMode)
        {
            USHORT usTotNumBytes;

            if (usBytesToWrite > (USHRT_MAX/usBytesPerSector))
            {
                rc = ERROR_TRANSFER_TOO_LONG;
                goto FS_WRITEEXIT;
            }

            usTotNumBytes = usBytesToWrite * usBytesPerSector;

            rc = MY_PROBEBUF(PB_OPREAD, pBufPosition, usTotNumBytes);
            if (rc)
            {
                Message("Protection VIOLATION in FS_WRITE! (SYS%d)", rc);
                goto FS_WRITEEXIT;
            }

            /*
                for sector mode, offsets are actually sectors
                and Number of Bytes to write are actually
                Number of sectors to write
            */
            ulSector        = (ULONG)pos;
            usSectorOffset  = 0;
            usRemaining     = 0;
            usNumSectors    = usBytesToWrite;
            if (ulSector > (pVolInfo->BootSect.bpb.BigTotalSectors - (ULONG)usNumSectors))
            {
                usNumSectors    = (USHORT)(pVolInfo->BootSect.bpb.BigTotalSectors - ulSector);
            }
            usBytesToWrite   = usNumSectors;
        }
        else
        {
            rc = MY_PROBEBUF(PB_OPREAD, pData, usBytesToWrite);
            if (rc)
            {
                Message("Protection VIOLATION in FS_WRITE! (SYS%d)", rc);
                goto FS_WRITEEXIT;
            }

            ulSector        = (ULONG)(pos / (ULONG)usBytesPerSector);
            usSectorOffset  = (USHORT)(pos % (ULONG)usBytesPerSector);
            usRemaining     = (usSectorOffset + usBytesToWrite) > usBytesPerSector ? ((usSectorOffset + usBytesToWrite) % usBytesPerSector) : 0;
            usNumSectors    = (usBytesToWrite - usRemaining)/usBytesPerSector;

            if (ulSector > (pVolInfo->BootSect.bpb.BigTotalSectors - (ULONG)(usSectorOffset ? 1 : 0) - (ULONG)usNumSectors - (ULONG)(usRemaining ? 1 : 0)))
               {
               usNumSectors    = (USHORT)(pVolInfo->BootSect.bpb.BigTotalSectors - (ULONG)(usSectorOffset ? 1 : 0) - ulSector);
               usRemaining     = 0;
               usBytesToWrite  = usNumSectors*usBytesPerSector + ( usSectorOffset ? ( usBytesPerSector - usSectorOffset ) : 0 );
               }
        }

        if (ulSector >= pVolInfo->BootSect.bpb.BigTotalSectors)
        {
            rc = ERROR_SECTOR_NOT_FOUND;
            goto FS_WRITEEXIT;
        }

        if (ulSector > (pVolInfo->BootSect.bpb.BigTotalSectors - (ULONG)(usSectorOffset ? 1 : 0) - (ULONG)usNumSectors - (ULONG)(usRemaining ? 1 : 0)))
        {
            rc = ERROR_SECTOR_NOT_FOUND;
            goto FS_WRITEEXIT;
        }

        if (usSectorOffset)
        {
            ULONG ulCurBytesToWrite = min(usBytesToWrite, usBytesPerSector - usSectorOffset);

            rc = ReadSector(pVolInfo, ulSector, 1, pbCluster, usIOFlag);
            if (rc)
            {
                goto FS_WRITEEXIT;
            }
            memcpy(pbCluster + usSectorOffset, pBufPosition, (USHORT)ulCurBytesToWrite);
            rc = WriteSector(pVolInfo, ulSector, 1, pbCluster, usIOFlag);
            if (rc)
            {
                goto FS_WRITEEXIT;
            }

            pBufPosition            += (USHORT)ulCurBytesToWrite;
            pos                     += (USHORT)ulCurBytesToWrite;
            usBytesWritten          += (USHORT)ulCurBytesToWrite;
            usBytesToWrite          -= (USHORT)ulCurBytesToWrite;
            ulSector++;
        }

        if (usNumSectors)
        {
            rc = WriteSector(pVolInfo, ulSector, usNumSectors, pBufPosition, usIOFlag);
            if (rc)
            {
                goto FS_WRITEEXIT;
            }
            pBufPosition            += (ULONG)((ULONG)usNumSectors*(ULONG)usBytesPerSector);
            pos                     += usBytesToWrite - usRemaining;
            usBytesWritten          += usBytesToWrite - usRemaining;
            ulSector                += usNumSectors;
        }

        if (usRemaining)
        {
            rc = ReadSector(pVolInfo, ulSector, 1, pbCluster, usIOFlag);
            if (rc)
            {
                goto FS_WRITEEXIT;
            }
            memcpy(pbCluster, pBufPosition, usRemaining);
            rc = WriteSector(pVolInfo, ulSector, 1, pbCluster, usIOFlag);
            if (rc)
            {
                goto FS_WRITEEXIT;
            }

            pBufPosition            += usRemaining;
            pos                     += usRemaining;
            usBytesWritten          += usRemaining;
        }

        *pLen                       = usBytesWritten;
        rc = NO_ERROR;
    }
    else
    {
        char far *pBufPosition = pData;

        /*
            No writes if file is larger than 7FFFFFFF (= 2Gb, or = 4 Gb with /largefiles)
        */
        if ( (! f32Parms.fLargeFiles && (pos + usBytesToWrite >= (LONGLONG)LONG_MAX   ||
                                        size + usBytesToWrite >= (LONGLONG)LONG_MAX)) ||
            (f32Parms.fLargeFiles && (pos + usBytesToWrite >= (LONGLONG)ULONG_MAX     ||
                                      size + usBytesToWrite >= (LONGLONG)ULONG_MAX)) )
        {
            rc = ERROR_INVALID_PARAMETER;
            goto FS_WRITEEXIT;
        }

        rc = MY_PROBEBUF(PB_OPREAD, pData, usBytesToWrite);
        if (rc)
        {
            Message("Protection VIOLATION in FS_WRITE! (SYS%d)", rc);
            goto FS_WRITEEXIT;
        }

        pOpenInfo->pSHInfo->fMustCommit = TRUE;


        if (! f32Parms.fLargeFiles && (LONGLONG)LONG_MAX - pos < (LONGLONG)usBytesToWrite)
           usBytesToWrite = (USHORT)((LONGLONG)LONG_MAX - pos);

        if (f32Parms.fLargeFiles && (LONGLONG)ULONG_MAX - pos < (LONGLONG)usBytesToWrite)
           usBytesToWrite = (USHORT)((LONGLONG)ULONG_MAX - pos);

        if (pos + usBytesToWrite > size)
        {
            ULONG ulLast = FAT_EOF;

            if (
                    pOpenInfo->ulCurCluster == FAT_EOF &&
                    pos == size &&
                    !(size % pVolInfo->ulBlockSize)
                )
                ulLast = pOpenInfo->pSHInfo->ulLastCluster;

            rc = NewSize(pVolInfo, psffsi, psffsd,
            pos + usBytesToWrite, usIOFlag);

            if (rc)
                goto FS_WRITEEXIT;

            size = psffsi->sfi_size;

            if (f32Parms.fLargeFiles)
               size = psffsi->sfi_sizel;

            if (ulLast != FAT_EOF)
            {
                pOpenInfo->ulCurCluster = GetNextCluster(pVolInfo, ulLast);
                if (!pOpenInfo->ulCurCluster)
                    pOpenInfo->ulCurCluster = FAT_EOF;

                if (pOpenInfo->ulCurCluster == FAT_EOF)
                {
                    Message("FS_WRITE (INIT) No next cluster available!");
                    CritMessage("FAT32: FS_WRITE (INIT) No next cluster available!");
                }
            }
        }

        if (pOpenInfo->ulCurCluster == FAT_EOF)
           {
           pOpenInfo->ulCurCluster = PositionToOffset(pVolInfo, pOpenInfo, pos);
           pOpenInfo->ulCurBlock = pOpenInfo->ulCurBlock = (pos / pVolInfo->ulBlockSize) % (pVolInfo->ulClusterSize / pVolInfo->ulBlockSize);
           }

        if (pOpenInfo->ulCurCluster == FAT_EOF)
        {
            Message("FS_WRITE (INIT2) No next cluster available!");
            CritMessage("FAT32: FS_WRITE (INIT2) No next cluster available!");
            rc = ERROR_INVALID_HANDLE;
            goto FS_WRITEEXIT;
        }

        /*
            First, handle the first part that does not align on a cluster border
        */
        ulBytesPerCluster   = pVolInfo->ulClusterSize;
        ulBytesPerBlock   = pVolInfo->ulBlockSize;
        usClusterOffset     = (USHORT)(pos % ulBytesPerCluster);
        usBlockOffset     = (USHORT)(pos % ulBytesPerBlock);
        if
            (
                (pOpenInfo->ulCurCluster != FAT_EOF) &&
                (pos < size) &&
                (usBytesToWrite) &&
                (usBlockOffset)
            )
        {
            ULONG   ulCurrBytesToWrite;
            USHORT  usSectorsToWrite;
            USHORT  usSectorsPerCluster;
            USHORT  usSectorsPerBlock;

            usSectorsPerCluster = pVolInfo->BootSect.bpb.SectorsPerCluster;
            usSectorsPerBlock = ulBytesPerBlock / pVolInfo->BootSect.bpb.BytesPerSector;
            usSectorsToWrite    = usSectorsPerBlock;

            /* compute the number of bytes
                to the cluster end or
                as much as fits into the user buffer
                or how much remains to be read until file end
               whatever is the smallest
            */
            ulCurrBytesToWrite  = (ulBytesPerBlock - (ULONG)usBlockOffset);
            ulCurrBytesToWrite  = min(ulCurrBytesToWrite,(ULONG)usBytesToWrite);
            ulCurrBytesToWrite  = (ULONG)min((ULONGLONG)ulCurrBytesToWrite,size - pos);
            if (ulCurrBytesToWrite)
            {
                ulClusterSector = pVolInfo->ulStartOfData + (pOpenInfo->ulCurCluster-2)*usSectorsPerCluster;

                rc = ReadBlock(pVolInfo, pOpenInfo->ulCurCluster, pOpenInfo->ulCurBlock, pbCluster, usIOFlag);
                if (rc)
                {
                    goto FS_WRITEEXIT;
                }
                memcpy(pbCluster + usBlockOffset, pBufPosition, (USHORT)ulCurrBytesToWrite);

                rc = WriteBlock(pVolInfo, pOpenInfo->ulCurCluster, pOpenInfo->ulCurBlock, pbCluster, usIOFlag);
                if (rc)
                {
                    goto FS_WRITEEXIT;
                }

                pBufPosition            += (USHORT)ulCurrBytesToWrite;
                pos                     += (USHORT)ulCurrBytesToWrite;
                usBytesWritten          += (USHORT)ulCurrBytesToWrite;
                usBytesToWrite          -= (USHORT)ulCurrBytesToWrite;
            }

            if (((ULONG)usClusterOffset + ulCurrBytesToWrite) >= ulBytesPerCluster)
            {
                pOpenInfo->ulCurCluster     = GetNextCluster(pVolInfo, pOpenInfo->ulCurCluster);
                if (!pOpenInfo->ulCurCluster)
                {
                    pOpenInfo->ulCurCluster = FAT_EOF;
                }
            }
            pOpenInfo->ulCurBlock = (pos / pVolInfo->ulBlockSize) % (pVolInfo->ulClusterSize / pVolInfo->ulBlockSize);
        }

        /*
            Second, handle the part that aligns on a cluster border and is a multiple of the cluster size
        */
        if
            (
                (pOpenInfo->ulCurCluster != FAT_EOF) &&
                (pos < size) &&
                (usBytesToWrite)
            )
        {
            ULONG   ulCurrCluster       = pOpenInfo->ulCurCluster;
            ULONG   ulNextCluster       = ulCurrCluster;
            ULONG   ulCurrBytesToWrite  = 0;
            USHORT  usSectorsPerCluster = pVolInfo->BootSect.bpb.SectorsPerCluster;
            USHORT  usSectorsPerBlock   = ulBytesPerBlock / pVolInfo->BootSect.bpb.BytesPerSector;
            USHORT  usClustersToProcess = 0;
            USHORT  usBlocksToProcess   = 0;
            USHORT  usAdjacentClusters  = 1;
            USHORT  usAdjacentBlocks;
            USHORT  usBlocks;

            ulCurrBytesToWrite          = (ULONG)min((ULONGLONG)usBytesToWrite,size - pos);

            usClustersToProcess         = (USHORT)(ulCurrBytesToWrite / ulBytesPerCluster); /* get the number of full clusters */
            usBlocksToProcess           = (USHORT)(ulCurrBytesToWrite / ulBytesPerBlock);   /* get the number of full blocks   */
            usAdjacentBlocks            = (usBlocksToProcess % (pVolInfo->ulClusterSize / pVolInfo->ulBlockSize));
            usBlocks = usBlocksToProcess;

            if (ulCurrBytesToWrite < pVolInfo->ulClusterSize)
               usAdjacentBlocks = ulCurrBytesToWrite / pVolInfo->ulBlockSize;

            while (usBlocks && (ulCurrCluster != FAT_EOF))
            {
                ulNextCluster       = GetNextCluster(pVolInfo, ulCurrCluster);
                if (!ulNextCluster)
                {
                    ulNextCluster = FAT_EOF;
                }

                if  (
                        (ulNextCluster != FAT_EOF) &&
                        (ulNextCluster == (ulCurrCluster+1)) &&
                        (usClustersToProcess)
                    )
                {
                    usAdjacentClusters  += 1;
                    usAdjacentBlocks    += pVolInfo->ulClusterSize / pVolInfo->ulBlockSize;
                    usClustersToProcess -= 1;
                }
                else
                {
                    ULONG  ulCurrBytesToWrite;
                    USHORT usSectorsToWrite;

                    if (!usAdjacentBlocks)
                       usAdjacentBlocks = 1;

                    ulCurrBytesToWrite = usAdjacentBlocks * ulBytesPerBlock;
                    usSectorsToWrite   = usAdjacentBlocks * usSectorsPerBlock;

                    ulClusterSector = pVolInfo->ulStartOfData + (pOpenInfo->ulCurCluster-2)*usSectorsPerCluster;
#if 0
                    /*
                        The following code is fast, but is not compatible
                        with OBJ_ANY attribute
                    */
                    rc = WriteSector(pVolInfo, ulClusterSector,usSectorsToWrite,pBufPosition, usIOFlag);
                    if (rc)
                    {
                        goto FS_WRITEEXIT;
                    }

                    pBufPosition                    += (USHORT)ulCurrBytesToWrite;
                    pos                             += (USHORT)ulCurrBytesToWrite;
                    usBytesWritten                  += (USHORT)ulCurrBytesToWrite;
                    usBytesToWrite                  -= (USHORT)ulCurrBytesToWrite;
                    usAdjacentClusters              = 1;
                    pOpenInfo->ulCurCluster         = ulNextCluster;
#else
                    {
                        ULONG ulBlock;
                        while ( usBlocks )
                        {
                           memcpy(pbCluster,pBufPosition,(USHORT)ulBytesPerBlock);
                           rc = WriteBlock(pVolInfo, pOpenInfo->ulCurCluster, pOpenInfo->ulCurBlock, pbCluster, usIOFlag);
                           if (rc)
                           {
                               goto FS_WRITEEXIT;
                           }

                           pOpenInfo->ulCurBlock++;

                           if (pOpenInfo->ulCurBlock >= (pVolInfo->ulClusterSize / pVolInfo->ulBlockSize))
                             {
                             pOpenInfo->ulCurBlock   = 0;
                             ulCurrCluster           = GetNextCluster(pVolInfo, ulCurrCluster);
                             pOpenInfo->ulCurCluster = ulCurrCluster;
                             }

                           usBlocks -= 1;

                           pBufPosition                += ulBytesPerBlock;
                           ulClusterSector             += usSectorsPerBlock;
                           usAdjacentBlocks--;
                        }
                    }

                    pos                             += (USHORT)ulCurrBytesToWrite;
                    usBytesWritten                  += (USHORT)ulCurrBytesToWrite;
                    usBytesToWrite                  -= (USHORT)ulCurrBytesToWrite;
                    usAdjacentBlocks                = pVolInfo->ulClusterSize / pVolInfo->ulBlockSize;
                    usAdjacentClusters              = 1;

                    if (pOpenInfo->ulCurBlock >= (pVolInfo->ulClusterSize / pVolInfo->ulBlockSize))
                       {
                       pOpenInfo->ulCurBlock = 0;
                       ulCurrCluster         = ulNextCluster;
                       pOpenInfo->ulCurCluster = ulNextCluster;
                       }
#endif
                }
            }
        }


        /*
            Third, handle the part that aligns on a cluster border but does not make up a complete cluster
        */
        if
            (
                (pOpenInfo->ulCurCluster != FAT_EOF) &&
                (pos < size) &&
                (usBytesToWrite)
            )
        {
            ULONG  ulCurrBytesToWrite;
            USHORT usSectorsToWrite;
            USHORT usSectorsPerCluster;
            USHORT usSectorsPerBlock;

            usSectorsToWrite = ulBytesPerBlock / pVolInfo->BootSect.bpb.BytesPerSector;
            usSectorsPerCluster = pVolInfo->BootSect.bpb.SectorsPerCluster;
            usSectorsPerBlock = usSectorsToWrite;

            ulCurrBytesToWrite = (ULONG)min((ULONGLONG)usBytesToWrite,size - pos);
            if (ulCurrBytesToWrite)
            {
                ulClusterSector = pVolInfo->ulStartOfData + (pOpenInfo->ulCurCluster-2)*usSectorsPerCluster;
                rc = ReadBlock(pVolInfo, pOpenInfo->ulCurCluster, pOpenInfo->ulCurBlock, pbCluster, usIOFlag);
                if (rc)
                {
                    goto FS_WRITEEXIT;
                }
                memcpy(pbCluster, pBufPosition, (USHORT)ulCurrBytesToWrite);

                rc = WriteBlock(pVolInfo, pOpenInfo->ulCurCluster, pOpenInfo->ulCurBlock, pbCluster, usIOFlag);
                if (rc)
                {
                    goto FS_WRITEEXIT;
                }

                pos                     += (USHORT)ulCurrBytesToWrite;
                usBytesWritten          += (USHORT)ulCurrBytesToWrite;
                usBytesToWrite          -= (USHORT)ulCurrBytesToWrite;
            }
        }

        pOpenInfo->ulCurBlock = (pos / pVolInfo->ulBlockSize) % (pVolInfo->ulClusterSize / pVolInfo->ulBlockSize);

        *pLen = usBytesWritten;
        if (usBytesWritten)
        {
            psffsi->sfi_tstamp |= ST_SWRITE | ST_PWRITE;
            pOpenInfo->fCommitAttr = TRUE;
            pOpenInfo->pSHInfo->bAttr |= FILE_ARCHIVED;
            psffsi->sfi_DOSattr |= FILE_ARCHIVED;
        }


        if (usIOFlag & DVIO_OPWRTHRU)
            rc = FS_COMMIT(FS_COMMIT_ONE, usIOFlag, psffsi, psffsd);
        else
            rc = NO_ERROR;
    }

FS_WRITEEXIT:
   psffsi->sfi_size = (ULONG)size;
   psffsi->sfi_position = (LONG)pos;

   if (f32Parms.fLargeFiles)
      {
      psffsi->sfi_sizel = size;
      psffsi->sfi_positionl = pos;
      }

   if( pbCluster )
      free( pbCluster );

   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_WRITE returned %u (%u bytes written)", rc, *pLen);

   _asm pop es;

   return rc;
}

/******************************************************************
* Positition to offset
******************************************************************/
ULONG PositionToOffset(PVOLINFO pVolInfo, POPENINFO pOpenInfo, ULONGLONG ullOffset)
{
ULONG ulCurCluster;


   ulCurCluster = pOpenInfo->pSHInfo->ulStartCluster;
   if (!ulCurCluster)
      return FAT_EOF;

   if (ullOffset < pVolInfo->ulClusterSize)
      return ulCurCluster;

   return SeekToCluster(pVolInfo, ulCurCluster, ullOffset);
}

/******************************************************************
*
******************************************************************/
int far pascal _loadds FS_CANCELLOCKREQUESTL(
    struct sffsi far * psffsi,      /* psffsi   */
    struct sffsd far * psffsd,      /* psffsd   */
    void far * pLockRang            /* pLockRang    */
)
{
   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_CANCELLOCKREQUESTL - NOT SUPPORTED");
   return ERROR_NOT_SUPPORTED;

   psffsi = psffsi;
   psffsd = psffsd;
   pLockRang = pLockRang;
}

/******************************************************************
*
******************************************************************/
int far pascal _loadds FS_CANCELLOCKREQUEST(
    struct sffsi far * psffsi,      /* psffsi   */
    struct sffsd far * psffsd,      /* psffsd   */
    void far * pLockRang            /* pLockRang    */
)
{
   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_CANCELLOCKREQUEST");

   return FS_CANCELLOCKREQUESTL(psffsi, psffsd, pLockRang);
}

/******************************************************************
*
******************************************************************/
int far pascal _loadds FS_CHGFILEPTRL(
    struct sffsi far * psffsi,      /* psffsi   */
    struct sffsd far * psffsd,      /* psffsd   */
    long long llOffset,             /* offset   */
    unsigned short usType,          /* type     */
    unsigned short IOFlag           /* IOflag   */
)
{
PVOLINFO pVolInfo;
POPENINFO pOpenInfo;
LONGLONG  llNewOffset = 0;
LONGLONG  pos;
ULONGLONG size;
USHORT rc;

   _asm push es;

   pOpenInfo = GetOpenInfo(psffsd);

   size = (ULONGLONG)psffsi->sfi_size;
   pos  = (LONGLONG)psffsi->sfi_position;

   if (f32Parms.fLargeFiles)
      {
      size = psffsi->sfi_sizel;
      pos  = psffsi->sfi_positionl;
      }

   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_CHGFILEPTRL, Mode %d - offset %lld, current offset=%lld",
      usType, llOffset, pos);

   pVolInfo = GetVolInfo(psffsi->sfi_hVPB);

   if (! pVolInfo)
      {
      rc = ERROR_INVALID_DRIVE;
      goto FS_CHGFILEPTRLEXIT;
      }

   if (pVolInfo->fFormatInProgress && !(psffsi->sfi_mode & OPEN_FLAGS_DASD))
      {
      rc = ERROR_ACCESS_DENIED;
      goto FS_CHGFILEPTRLEXIT;
      }

   if (IsDriveLocked(pVolInfo))
      {
      rc = ERROR_DRIVE_LOCKED;
      goto FS_CHGFILEPTRLEXIT;
      }

   switch (usType)
      {
      case CFP_RELBEGIN :
         if (llOffset < 0)
            {
            rc = ERROR_NEGATIVE_SEEK;
            goto FS_CHGFILEPTRLEXIT;
            }
         llNewOffset = llOffset;
         break;
      case CFP_RELCUR  :
         llNewOffset = pos + llOffset;
         break;
      case CFP_RELEND  :
         llNewOffset = size + llOffset;
         break;
      }
   if (!IsDosSession() && llNewOffset < 0)
      {
      rc = ERROR_NEGATIVE_SEEK;
      goto FS_CHGFILEPTRLEXIT;
      }

   if (pos != (ULONG)llNewOffset)
      {
      pos = (ULONG)llNewOffset;
      pOpenInfo->ulCurCluster = FAT_EOF;
      }
   rc = 0;

FS_CHGFILEPTRLEXIT:
   psffsi->sfi_size = (ULONG)size;
   psffsi->sfi_position = (LONG)pos;

   if (f32Parms.fLargeFiles)
      {
      psffsi->sfi_sizel = size;
      psffsi->sfi_positionl = pos;
      }

   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_CHGFILEPTRL returned %u", rc);

   _asm pop es;

   return rc;

   IOFlag = IOFlag;
}


/******************************************************************
*
******************************************************************/
int far pascal _loadds FS_CHGFILEPTR(
    struct sffsi far * psffsi,      /* psffsi   */
    struct sffsd far * psffsd,      /* psffsd   */
    long lOffset,                   /* offset   */
    unsigned short usType,          /* type     */
    unsigned short IOFlag           /* IOflag   */
)
{
   APIRET rc;
   LONGLONG pos;

   pos = (LONGLONG)psffsi->sfi_position;

   if (f32Parms.fLargeFiles)
      pos = psffsi->sfi_positionl;

   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_CHGFILEPTR, Mode %d - offset %ld, current offset=%lld",
      usType, lOffset, pos);

   rc = FS_CHGFILEPTRL(psffsi, psffsd,
                       (long long)lOffset, usType,
                       IOFlag);

   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_CHGFILEPTR returned %u", rc);

   return rc;
}

/******************************************************************
*
******************************************************************/
int far pascal _loadds FS_COMMIT(
    unsigned short usType,      /* commit type  */
    unsigned short usIOFlag,        /* IOflag   */
    struct sffsi far * psffsi,      /* psffsi   */
    struct sffsd far * psffsd       /* psffsd   */
)
{
USHORT rc;

   _asm push es;

   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_COMMIT, type %d", usType);

   switch (usType)
      {
      case FS_COMMIT_ONE:
      case FS_COMMIT_ALL:
         {
         PVOLINFO pVolInfo;
         POPENINFO pOpenInfo;
         PSZ  pszFile;
         DIRENTRY DirEntry;
         DIRENTRY DirOld;
         ULONG  ulCluster;

         pVolInfo = GetVolInfo(psffsi->sfi_hVPB);

         if (! pVolInfo)
            {
            rc = ERROR_INVALID_DRIVE;
            goto FS_COMMITEXIT;
            }

         if (pVolInfo->fFormatInProgress)
            {
            rc = ERROR_ACCESS_DENIED;
            goto FS_COMMITEXIT;
            }

         pOpenInfo = GetOpenInfo(psffsd);

         if (IsDriveLocked(pVolInfo))
            {
            rc = ERROR_DRIVE_LOCKED;
            goto FS_COMMITEXIT;
            }

         if (psffsi->sfi_mode & OPEN_FLAGS_DASD)
            {
            rc = ERROR_NOT_SUPPORTED;
            goto FS_COMMITEXIT;
            }

         if (!pOpenInfo->pSHInfo->fMustCommit)
            {
            rc = 0;
            goto FS_COMMITEXIT;
            }

         pszFile = strrchr(pOpenInfo->pSHInfo->szFileName, '\\');
         if (!pszFile)
            {
            Message("FS_COMMIT, cannot find \\ in '%s'", pOpenInfo->pSHInfo->szFileName);
            CritMessage("FAT32:FS_COMMIT, cannot find \\ in '%s'!", pOpenInfo->pSHInfo->szFileName);
            return 1;
            }
         pszFile++;

         ulCluster = FindPathCluster(pVolInfo, pOpenInfo->pSHInfo->ulDirCluster, pszFile, &DirOld, NULL);
         if (ulCluster == FAT_EOF)
            {
            rc = ERROR_FILE_NOT_FOUND;
            goto FS_COMMITEXIT;
            }

         memcpy(&DirEntry, &DirOld, sizeof (DIRENTRY));
         memcpy(&DirEntry.wCreateTime,    &psffsi->sfi_ctime, sizeof (USHORT));
         memcpy(&DirEntry.wCreateDate,    &psffsi->sfi_cdate, sizeof (USHORT));
         memcpy(&DirEntry.wAccessDate,    &psffsi->sfi_adate, sizeof (USHORT));
         memcpy(&DirEntry.wLastWriteTime, &psffsi->sfi_mtime, sizeof (USHORT));
         memcpy(&DirEntry.wLastWriteDate, &psffsi->sfi_mdate, sizeof (USHORT));

         DirEntry.ulFileSize  = psffsi->sfi_size;

         if (f32Parms.fLargeFiles)
            DirEntry.ulFileSize  = (ULONG)psffsi->sfi_sizel;

         if (pOpenInfo->fCommitAttr || psffsi->sfi_DOSattr != pOpenInfo->pSHInfo->bAttr)
            {
            DirEntry.bAttr = pOpenInfo->pSHInfo->bAttr = psffsi->sfi_DOSattr;
            pOpenInfo->fCommitAttr = FALSE;
            }

         if (pOpenInfo->pSHInfo->ulStartCluster)
            {
            DirEntry.wCluster     = LOUSHORT(pOpenInfo->pSHInfo->ulStartCluster);
            DirEntry.wClusterHigh = HIUSHORT(pOpenInfo->pSHInfo->ulStartCluster);
            }
         else
            {
            DirEntry.wCluster = 0;
            DirEntry.wClusterHigh = 0;
            }

         rc = ModifyDirectory(pVolInfo, pOpenInfo->pSHInfo->ulDirCluster, MODIFY_DIR_UPDATE,
            &DirOld, &DirEntry, NULL, usIOFlag);
         if (!rc)
            psffsi->sfi_tstamp = 0;
         goto FS_COMMITEXIT;
         }
      }
   rc = ERROR_NOT_SUPPORTED;

FS_COMMITEXIT:

   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_COMMIT returned %u", rc);

   _asm pop es;

   return rc;
}

/******************************************************************
*
******************************************************************/
int far pascal _loadds FS_FILELOCKSL(
    struct sffsi far * psffsi,      /* psffsi   */
    struct sffsd far * psffsd,      /* psffsd   */
    void far * pUnlockRange,        /* pUnLockRange */
    void far * pLockRange,          /* pLockRange   */
    unsigned long ulTimeOut,        /* timeout  */
    unsigned long   ulFlags         /* flags    */
)
{
   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_FILELOCKSL");
   return ERROR_NOT_SUPPORTED;

   psffsi = psffsi;
   psffsd = psffsd;
   pUnlockRange = pUnlockRange;
   pLockRange = pLockRange;
   ulTimeOut = ulTimeOut;
   ulFlags = ulFlags;
}

/******************************************************************
*
******************************************************************/
int far pascal _loadds FS_FILELOCKS(
    struct sffsi far * psffsi,      /* psffsi   */
    struct sffsd far * psffsd,      /* psffsd   */
    void far * pUnlockRange,        /* pUnLockRange */
    void far * pLockRange,          /* pLockRange   */
    unsigned long ulTimeOut,        /* timeout  */
    unsigned long   ulFlags         /* flags    */
)
{
   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_FILELOCKS");

   return FS_FILELOCKSL(psffsi, psffsd,
                        pUnlockRange, pLockRange,
                        ulTimeOut, ulFlags);
}

/******************************************************************
*
******************************************************************/
int far pascal _loadds FS_NEWSIZEL(
    struct sffsi far * psffsi,      /* psffsi   */
    struct sffsd far * psffsd,      /* psffsd   */
    unsigned long long ullLen,      /* len      */
    unsigned short usIOFlag         /* IOflag   */
)
{
PVOLINFO pVolInfo;
POPENINFO pOpenInfo;
USHORT rc;

   _asm push es;

   pOpenInfo = GetOpenInfo(psffsd);

   pOpenInfo->pSHInfo->fMustCommit = TRUE;

   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_NEWSIZEL newsize = %llu", ullLen);

   if (psffsi->sfi_mode & OPEN_FLAGS_DASD)
      {
      rc = ERROR_NOT_SUPPORTED;
      goto FS_NEWSIZELEXIT;
      }

   pVolInfo = GetVolInfo(psffsi->sfi_hVPB);

   if (! pVolInfo)
      {
      rc = ERROR_INVALID_DRIVE;
      goto FS_NEWSIZELEXIT;
      }
   if (pVolInfo->fFormatInProgress)
      {
      rc = ERROR_ACCESS_DENIED;
      goto FS_NEWSIZELEXIT;
      }
   if (IsDriveLocked(pVolInfo))
      {
      rc = ERROR_DRIVE_LOCKED;
      goto FS_NEWSIZELEXIT;
      }
   if (pVolInfo->fWriteProtected)
      {
      rc = ERROR_WRITE_PROTECT;
      goto FS_NEWSIZELEXIT;
      }

   rc = NewSize(pVolInfo, psffsi, psffsd, ullLen, usIOFlag);
   if (!rc)
      psffsi->sfi_tstamp |= ST_SWRITE | ST_PWRITE;

   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_NEWSIZEL returned %u", rc);

FS_NEWSIZELEXIT:
   _asm pop es;

   return rc;
}


/******************************************************************
*
******************************************************************/
int far pascal _loadds FS_NEWSIZE(
    struct sffsi far * psffsi,      /* psffsi   */
    struct sffsd far * psffsd,      /* psffsd   */
    unsigned long ulLen,        /* len      */
    unsigned short usIOFlag     /* IOflag   */
)
{
   APIRET rc;

   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_NEWSIZEL newsize = %lu", ulLen);

   rc = FS_NEWSIZEL(psffsi, psffsd,
                    ulLen, usIOFlag);

   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_NEWSIZEL returned %u", rc);

   return rc;
}


/******************************************************************
*
******************************************************************/
USHORT NewSize(PVOLINFO pVolInfo,
    struct sffsi far * psffsi,      /* psffsi   */
    struct sffsd far * psffsd,      /* psffsd   */
    ULONGLONG ullLen,
    USHORT usIOFlag)
{
POPENINFO pOpenInfo = GetOpenInfo(psffsd);
ULONG ulClustersNeeded;
ULONG ulClusterCount;
ULONG ulCluster, ulNextCluster;
ULONGLONG size;

   size = (ULONGLONG)psffsi->sfi_size;

   if (f32Parms.fLargeFiles)
      size = psffsi->sfi_sizel;

   if (ullLen == size)
      return 0;

   if (!ullLen)
      {
      if (pOpenInfo->pSHInfo->ulStartCluster)
         {
         DeleteFatChain(pVolInfo, pOpenInfo->pSHInfo->ulStartCluster);
         pOpenInfo->pSHInfo->ulStartCluster = 0L;
         pOpenInfo->pSHInfo->ulLastCluster = FAT_EOF;
         }

      ResetAllCurrents(pOpenInfo);

      psffsi->sfi_size = (ULONG)ullLen;

      if (f32Parms.fLargeFiles)
         psffsi->sfi_sizel = ullLen;

      if (usIOFlag & DVIO_OPWRTHRU)
         return FS_COMMIT(FS_COMMIT_ONE, usIOFlag, psffsi, psffsd);

      return 0;
      }

   /*
      Calculate number of needed clusters
   */
   ulClustersNeeded = ullLen / pVolInfo->ulClusterSize;
   if (ullLen % pVolInfo->ulClusterSize)
      ulClustersNeeded ++;

   /*
      if file didn't have any clusters
   */

   if (!pOpenInfo->pSHInfo->ulStartCluster)
      {
      ulCluster = MakeFatChain(pVolInfo, FAT_EOF, ulClustersNeeded, &pOpenInfo->pSHInfo->ulLastCluster);
      if (ulCluster == FAT_EOF)
         return ERROR_DISK_FULL;
      pOpenInfo->pSHInfo->ulStartCluster = ulCluster;
      }

   /*
      If newsize < current size
   */

   else if (ullLen < size)
      {
      if (!(ullLen % pVolInfo->ulClusterSize))
         ulCluster = PositionToOffset(pVolInfo, pOpenInfo, ullLen - 1);
      else
         ulCluster = PositionToOffset(pVolInfo, pOpenInfo, ullLen);

      if (ulCluster == FAT_EOF)
         return ERROR_SECTOR_NOT_FOUND;

      ulNextCluster = GetNextCluster(pVolInfo, ulCluster);
      if (ulNextCluster != FAT_EOF)
         {
         SetNextCluster( pVolInfo, ulCluster, FAT_EOF);
         DeleteFatChain(pVolInfo, ulNextCluster);
         }
      pOpenInfo->pSHInfo->ulLastCluster = ulCluster;

      ResetAllCurrents(pOpenInfo);
      }
   else
      {
      /*
         If newsize > current size
      */

      ulCluster = pOpenInfo->pSHInfo->ulLastCluster;
      if (ulCluster == FAT_EOF)
         {
         CritMessage("FAT32: Lastcluster empty in NewSize!");
         Message("FAT32: Lastcluster empty in NewSize!");
         return ERROR_SECTOR_NOT_FOUND;
         }

      ulClusterCount = size / pVolInfo->ulClusterSize;
      if (size % pVolInfo->ulClusterSize)
         ulClusterCount ++;

      if (ulClustersNeeded > ulClusterCount)
         {
         ulNextCluster = MakeFatChain(pVolInfo, ulCluster, ulClustersNeeded - ulClusterCount, &pOpenInfo->pSHInfo->ulLastCluster);
         if (ulNextCluster == FAT_EOF)
            return ERROR_DISK_FULL;
         }
      }

   psffsi->sfi_size = (ULONG)ullLen;

   if (f32Parms.fLargeFiles)
      psffsi->sfi_sizel = ullLen;

   if (usIOFlag & DVIO_OPWRTHRU)
      return FS_COMMIT(FS_COMMIT_ONE, usIOFlag, psffsi, psffsd);

   return 0;
}

/******************************************************************
*
******************************************************************/
int far pascal _loadds FS_FILEINFO(unsigned short usFlag,       /* flag     */
    struct sffsi far * psffsi,      /* psffsi   */
    struct sffsd far * psffsd,      /* psffsd   */
    unsigned short usLevel,     /* level    */
    char far * pData,           /* pData    */
    unsigned short cbData,      /* cbData   */
    unsigned short IOFlag       /* IOflag   */
)
{
PVOLINFO pVolInfo;
POPENINFO pOpenInfo;
USHORT usNeededSize;
USHORT rc;
PSZ  pszFile;
ULONGLONG size;

   _asm push es;

   pOpenInfo = GetOpenInfo(psffsd);

   size = (ULONGLONG)psffsi->sfi_size;

   if (f32Parms.fLargeFiles)
      size = psffsi->sfi_sizel;

   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_FILEINFO for %s, usFlag = %X, level %d",
         pOpenInfo->pSHInfo->szFileName,
         usFlag, usLevel);

   pVolInfo = GetVolInfo(psffsi->sfi_hVPB);

   if (! pVolInfo)
      {
      rc = ERROR_INVALID_DRIVE;
      goto FS_FILEINFOEXIT;
      }

   if (pVolInfo->fFormatInProgress)
      {
      rc = ERROR_ACCESS_DENIED;
      goto FS_FILEINFOEXIT;
      }

   if (psffsi->sfi_mode & OPEN_FLAGS_DASD)
      {
      rc = ERROR_NOT_SUPPORTED;
      goto FS_FILEINFOEXIT;
      }
   if (IsDriveLocked(pVolInfo))
      {
      rc = ERROR_DRIVE_LOCKED;
      goto FS_FILEINFOEXIT;
      }

   pszFile = strrchr(pOpenInfo->pSHInfo->szFileName, '\\');
   if (!pszFile)
      {
      Message("FS_FILEINFO, cannot find \\!");
      CritMessage("FAT32:FS_FILEINFO, cannot find \\!");
      return 1;
      }
   pszFile++;


   psffsd = psffsd;
   IOFlag = IOFlag;

   if (usFlag == FI_RETRIEVE)
      {
      switch (usLevel)
         {
         case FIL_STANDARD         :
            usNeededSize = sizeof (FILESTATUS);
            break;
         case FIL_STANDARDL        :
            usNeededSize = sizeof (FILESTATUS3L);
            break;
         case FIL_QUERYEASIZE      :
            usNeededSize = sizeof (FILESTATUS2);
            break;
         case FIL_QUERYEASIZEL     :
            usNeededSize = sizeof (FILESTATUS4L);
            break;
         case FIL_QUERYEASFROMLIST :
         case 4:
            usNeededSize = sizeof (EAOP);
            break;
         default                   :
            rc = ERROR_INVALID_LEVEL;
            goto FS_FILEINFOEXIT;
         }
      if (cbData < usNeededSize)
         {
         rc = ERROR_BUFFER_OVERFLOW;
         goto FS_FILEINFOEXIT;
         }

      rc = MY_PROBEBUF(PB_OPWRITE, pData, cbData);
      if (rc)
         {
         Message("Protection VIOLATION in FS_FILEINFO!\n");
         goto FS_FILEINFOEXIT;
         }

      if (usLevel == FIL_STANDARD  || usLevel == FIL_QUERYEASIZE ||
          usLevel == FIL_STANDARDL || usLevel == FIL_QUERYEASIZEL)
         {
         DIRENTRY DirEntry;
         ULONG ulCluster;

         ulCluster = FindPathCluster(pVolInfo, pOpenInfo->pSHInfo->ulDirCluster, pszFile, &DirEntry, NULL);
         if (ulCluster == FAT_EOF)
            {
            rc = ERROR_FILE_NOT_FOUND;
            goto FS_FILEINFOEXIT;
            }

         memcpy(&psffsi->sfi_ctime, &DirEntry.wCreateTime, sizeof (USHORT));
         memcpy(&psffsi->sfi_cdate, &DirEntry.wCreateDate, sizeof (USHORT));
         psffsi->sfi_atime = 0;
         memcpy(&psffsi->sfi_adate, &DirEntry.wAccessDate, sizeof (USHORT));
         memcpy(&psffsi->sfi_mtime, &DirEntry.wLastWriteTime, sizeof (USHORT));
         memcpy(&psffsi->sfi_mdate, &DirEntry.wLastWriteDate, sizeof (USHORT));
         psffsi->sfi_DOSattr = DirEntry.bAttr = pOpenInfo->pSHInfo->bAttr;
         }

      switch (usLevel)
         {
         case FIL_STANDARD         :
            {
            PFILESTATUS pfStatus = (PFILESTATUS)pData;
            memset(pfStatus, 0, sizeof (FILESTATUS));

            memcpy(&pfStatus->fdateCreation, &psffsi->sfi_cdate, sizeof (USHORT));
            memcpy(&pfStatus->ftimeCreation, &psffsi->sfi_ctime, sizeof (USHORT));
            memcpy(&pfStatus->fdateLastAccess, &psffsi->sfi_adate, sizeof (USHORT));
            memcpy(&pfStatus->fdateLastWrite, &psffsi->sfi_mdate, sizeof (USHORT));
            memcpy(&pfStatus->ftimeLastWrite, &psffsi->sfi_mtime, sizeof (USHORT));
            pfStatus->cbFile = size;
            pfStatus->cbFileAlloc =
               (pfStatus->cbFile / pVolInfo->ulClusterSize) * pVolInfo->ulClusterSize +
               (pfStatus->cbFile % pVolInfo->ulClusterSize ? pVolInfo->ulClusterSize : 0);

            pfStatus->attrFile = psffsi->sfi_DOSattr = pOpenInfo->pSHInfo->bAttr;
            rc = 0;
            break;
            }
         case FIL_STANDARDL        :
            {
            PFILESTATUS3L pfStatus = (PFILESTATUS3L)pData;
            memset(pfStatus, 0, sizeof (FILESTATUS3L));

            memcpy(&pfStatus->fdateCreation, &psffsi->sfi_cdate, sizeof (USHORT));
            memcpy(&pfStatus->ftimeCreation, &psffsi->sfi_ctime, sizeof (USHORT));
            memcpy(&pfStatus->fdateLastAccess, &psffsi->sfi_adate, sizeof (USHORT));
            memcpy(&pfStatus->fdateLastWrite, &psffsi->sfi_mdate, sizeof (USHORT));
            memcpy(&pfStatus->ftimeLastWrite, &psffsi->sfi_mtime, sizeof (USHORT));
            pfStatus->cbFile = size;
            pfStatus->cbFileAlloc =
               (pfStatus->cbFile / pVolInfo->ulClusterSize) * pVolInfo->ulClusterSize +
               (pfStatus->cbFile % pVolInfo->ulClusterSize ? pVolInfo->ulClusterSize : 0);

            pfStatus->attrFile = psffsi->sfi_DOSattr = pOpenInfo->pSHInfo->bAttr;
            rc = 0;
            break;
            }
         case FIL_QUERYEASIZE      :
            {
            PFILESTATUS2 pfStatus = (PFILESTATUS2)pData;
            memset(pfStatus, 0, sizeof (FILESTATUS2));

            memcpy(&pfStatus->fdateCreation, &psffsi->sfi_cdate, sizeof (USHORT));
            memcpy(&pfStatus->ftimeCreation, &psffsi->sfi_ctime, sizeof (USHORT));
            memcpy(&pfStatus->fdateLastAccess, &psffsi->sfi_adate, sizeof (USHORT));
            memcpy(&pfStatus->fdateLastWrite, &psffsi->sfi_mdate, sizeof (USHORT));
            memcpy(&pfStatus->ftimeLastWrite, &psffsi->sfi_mtime, sizeof (USHORT));
            pfStatus->cbFile = size;
            pfStatus->cbFileAlloc =
               (pfStatus->cbFile / pVolInfo->ulClusterSize) * pVolInfo->ulClusterSize +
               (pfStatus->cbFile % pVolInfo->ulClusterSize ? pVolInfo->ulClusterSize : 0);

            pfStatus->attrFile = psffsi->sfi_DOSattr = pOpenInfo->pSHInfo->bAttr;

            if (!f32Parms.fEAS)
               {
               pfStatus->cbList = sizeof pfStatus->cbList;
               rc = 0;
               }
            else
               rc = usGetEASize(pVolInfo, pOpenInfo->pSHInfo->ulDirCluster, pszFile, &pfStatus->cbList);
            break;
            }
         case FIL_QUERYEASIZEL     :
            {
            PFILESTATUS4L pfStatus = (PFILESTATUS4L)pData;
            memset(pfStatus, 0, sizeof (FILESTATUS4L));

            memcpy(&pfStatus->fdateCreation, &psffsi->sfi_cdate, sizeof (USHORT));
            memcpy(&pfStatus->ftimeCreation, &psffsi->sfi_ctime, sizeof (USHORT));
            memcpy(&pfStatus->fdateLastAccess, &psffsi->sfi_adate, sizeof (USHORT));
            memcpy(&pfStatus->fdateLastWrite, &psffsi->sfi_mdate, sizeof (USHORT));
            memcpy(&pfStatus->ftimeLastWrite, &psffsi->sfi_mtime, sizeof (USHORT));
            pfStatus->cbFile = size;
            pfStatus->cbFileAlloc =
               (pfStatus->cbFile / pVolInfo->ulClusterSize) * pVolInfo->ulClusterSize +
               (pfStatus->cbFile % pVolInfo->ulClusterSize ? pVolInfo->ulClusterSize : 0);

            pfStatus->attrFile = psffsi->sfi_DOSattr = pOpenInfo->pSHInfo->bAttr;

            if (!f32Parms.fEAS)
               {
               pfStatus->cbList = sizeof pfStatus->cbList;
               rc = 0;
               }
            else
               rc = usGetEASize(pVolInfo, pOpenInfo->pSHInfo->ulDirCluster, pszFile, &pfStatus->cbList);
            break;
            }
         case FIL_QUERYEASFROMLIST:
            {
            PEAOP pEA = (PEAOP)pData;
            PFEALIST pFEA = pEA->fpFEAList;
            rc = MY_PROBEBUF(PB_OPWRITE, (PBYTE)pFEA, sizeof pFEA->cbList);
            if (rc)
               {
               Message("FAT32: Protection VIOLATION in FS_FILEINFO!\n");
               goto FS_FILEINFOEXIT;
               }

            rc = MY_PROBEBUF(PB_OPWRITE, (PBYTE)pFEA, (USHORT)pFEA->cbList);
            if (rc)
               {
               Message("FAT32: Protection VIOLATION in FS_FILEINFO!\n");
               goto FS_FILEINFOEXIT;
               }

            if (!f32Parms.fEAS)
               {
               rc = usGetEmptyEAS(pszFile,pEA);
               }
            else
               rc = usGetEAS(pVolInfo, usLevel, pOpenInfo->pSHInfo->ulDirCluster, pszFile, pEA);
            break;
            }

         case 4:
            {
            PEAOP pEA = (PEAOP)pData;
            PFEALIST pFEA = pEA->fpFEAList;
            rc = MY_PROBEBUF(PB_OPWRITE, (PBYTE)pFEA, sizeof pFEA->cbList);
            if (rc)
               {
               Message("FAT32: Protection VIOLATION in FS_FILEINFO!\n");
               goto FS_FILEINFOEXIT;
               }

            rc = MY_PROBEBUF(PB_OPWRITE, (PBYTE)pFEA, (USHORT)pFEA->cbList);
            if (rc)
               {
               Message("FAT32: Protection VIOLATION in FS_FILEINFO!\n");
               goto FS_FILEINFOEXIT;
               }
            if (!f32Parms.fEAS)
               {
               memset(pFEA, 0, (USHORT)pFEA->cbList);
               pFEA->cbList = sizeof pFEA->cbList;
               rc = 0;
               }
            else
               rc = usGetEAS(pVolInfo, usLevel, pOpenInfo->pSHInfo->ulDirCluster, pszFile, pEA);
            break;
            }
         default :
            rc = ERROR_INVALID_LEVEL;
            break;
         }
      goto FS_FILEINFOEXIT;
      }


   if (usFlag & FI_SET)
      {
      rc = MY_PROBEBUF(PB_OPREAD, pData, cbData);
      if (rc)
         {
         Message("FAT32: Protection VIOLATION in FS_FILEINFO!\n");
         goto FS_FILEINFOEXIT;
         }

      if (!(psffsi->sfi_mode & OPEN_ACCESS_WRITEONLY) &&
         !(psffsi->sfi_mode & OPEN_ACCESS_READWRITE))
         {
         rc = ERROR_ACCESS_DENIED;
         goto FS_FILEINFOEXIT;
         }

      switch (usLevel)
         {
         case FIL_STANDARD:
            {
            USHORT usMask;
            PFILESTATUS pfStatus = (PFILESTATUS)pData;

            if (cbData < sizeof (FILESTATUS))
               {
               rc = ERROR_INSUFFICIENT_BUFFER;
               goto FS_FILEINFOEXIT;
               }

            usMask = ~(FILE_READONLY | FILE_HIDDEN | FILE_SYSTEM | FILE_ARCHIVED);
            if (pfStatus->attrFile & usMask)
               {
               rc = ERROR_ACCESS_DENIED;
               goto FS_FILEINFOEXIT;
               }

            usMask = 0;
            if (memcmp(&pfStatus->fdateCreation, &usMask, sizeof usMask) ||
                memcmp(&pfStatus->ftimeCreation, &usMask, sizeof usMask))
               {
               psffsi->sfi_tstamp &= ~ST_SCREAT;
               psffsi->sfi_tstamp |= ST_PCREAT;
               memcpy(&psffsi->sfi_ctime, &pfStatus->ftimeCreation, sizeof (USHORT));
               memcpy(&psffsi->sfi_cdate, &pfStatus->fdateCreation, sizeof (USHORT));
               pOpenInfo->pSHInfo->fMustCommit = TRUE;
               }

            if (memcmp(&pfStatus->fdateLastWrite, &usMask, sizeof usMask) ||
                memcmp(&pfStatus->ftimeLastWrite, &usMask, sizeof usMask))
               {
               psffsi->sfi_tstamp &= ~ST_SWRITE;
               psffsi->sfi_tstamp |= ST_PWRITE;
               memcpy(&psffsi->sfi_mdate, &pfStatus->fdateLastWrite, sizeof (USHORT));
               memcpy(&psffsi->sfi_mtime, &pfStatus->ftimeLastWrite, sizeof (USHORT));
               pOpenInfo->pSHInfo->fMustCommit = TRUE;
               }

            if (memcmp(&pfStatus->fdateLastAccess, &usMask, sizeof usMask))
               {
               psffsi->sfi_tstamp &= ~ST_SREAD;
               psffsi->sfi_tstamp |= ST_PREAD;
               memcpy(&psffsi->sfi_adate, &pfStatus->fdateLastAccess, sizeof (USHORT));
               psffsi->sfi_atime = 0;
               pOpenInfo->pSHInfo->fMustCommit = TRUE;
               }
            if (psffsi->sfi_DOSattr       != (BYTE)pfStatus->attrFile ||
                pOpenInfo->pSHInfo->bAttr != (BYTE)pfStatus->attrFile)
               {
               psffsi->sfi_DOSattr = (BYTE)pfStatus->attrFile;
               pOpenInfo->pSHInfo->bAttr = (BYTE)pfStatus->attrFile;
               pOpenInfo->fCommitAttr = TRUE;
               pOpenInfo->pSHInfo->fMustCommit = TRUE;
               }

            if (IOFlag & DVIO_OPWRTHRU)
               rc = FS_COMMIT(FS_COMMIT_ONE, IOFlag, psffsi, psffsd);
            else
               rc = 0;
            break;
            }

         case FIL_STANDARDL:
            {
            USHORT usMask;
            PFILESTATUS3L pfStatus = (PFILESTATUS3L)pData;

            if (cbData < sizeof (FILESTATUS3L))
               {
               rc = ERROR_INSUFFICIENT_BUFFER;
               goto FS_FILEINFOEXIT;
               }

            usMask = ~(FILE_READONLY | FILE_HIDDEN | FILE_SYSTEM | FILE_ARCHIVED);
            if (pfStatus->attrFile & usMask)
               {
               rc = ERROR_ACCESS_DENIED;
               goto FS_FILEINFOEXIT;
               }

            usMask = 0;
            if (memcmp(&pfStatus->fdateCreation, &usMask, sizeof usMask) ||
                memcmp(&pfStatus->ftimeCreation, &usMask, sizeof usMask))
               {
               psffsi->sfi_tstamp &= ~ST_SCREAT;
               psffsi->sfi_tstamp |= ST_PCREAT;
               memcpy(&psffsi->sfi_ctime, &pfStatus->ftimeCreation, sizeof (USHORT));
               memcpy(&psffsi->sfi_cdate, &pfStatus->fdateCreation, sizeof (USHORT));
               pOpenInfo->pSHInfo->fMustCommit = TRUE;
               }

            if (memcmp(&pfStatus->fdateLastWrite, &usMask, sizeof usMask) ||
                memcmp(&pfStatus->ftimeLastWrite, &usMask, sizeof usMask))
               {
               psffsi->sfi_tstamp &= ~ST_SWRITE;
               psffsi->sfi_tstamp |= ST_PWRITE;
               memcpy(&psffsi->sfi_mdate, &pfStatus->fdateLastWrite, sizeof (USHORT));
               memcpy(&psffsi->sfi_mtime, &pfStatus->ftimeLastWrite, sizeof (USHORT));
               pOpenInfo->pSHInfo->fMustCommit = TRUE;
               }

            if (memcmp(&pfStatus->fdateLastAccess, &usMask, sizeof usMask))
               {
               psffsi->sfi_tstamp &= ~ST_SREAD;
               psffsi->sfi_tstamp |= ST_PREAD;
               memcpy(&psffsi->sfi_adate, &pfStatus->fdateLastAccess, sizeof (USHORT));
               psffsi->sfi_atime = 0;
               pOpenInfo->pSHInfo->fMustCommit = TRUE;
               }
            if (psffsi->sfi_DOSattr       != (BYTE)pfStatus->attrFile ||
                pOpenInfo->pSHInfo->bAttr != (BYTE)pfStatus->attrFile)
               {
               psffsi->sfi_DOSattr = (BYTE)pfStatus->attrFile;
               pOpenInfo->pSHInfo->bAttr = (BYTE)pfStatus->attrFile;
               pOpenInfo->fCommitAttr = TRUE;
               pOpenInfo->pSHInfo->fMustCommit = TRUE;
               }

            if (IOFlag & DVIO_OPWRTHRU)
               rc = FS_COMMIT(FS_COMMIT_ONE, IOFlag, psffsi, psffsd);
            else
               rc = 0;
            break;
            }

         case FIL_QUERYEASIZE      :
         case FIL_QUERYEASIZEL     :
            if (!f32Parms.fEAS)
               rc = 0;
            else
               {
               rc = usModifyEAS(pVolInfo, pOpenInfo->pSHInfo->ulDirCluster,
                  pszFile, (PEAOP)pData);
               psffsi->sfi_tstamp = ST_SWRITE | ST_PWRITE;
               }
            break;

         default          :
            rc = ERROR_INVALID_LEVEL;
            break;
         }
      }
   else
      rc = ERROR_INVALID_FUNCTION;

FS_FILEINFOEXIT:
   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_FILEINFO returned %u", rc);

   _asm pop es;

   return rc;
}

/******************************************************************
*
******************************************************************/
int far pascal _loadds FS_FILEIO(
    struct sffsi far * psffsi,      /* psffsi   */
    struct sffsd far * psffsd,      /* psffsd   */
    char far * cbCmdList,           /* cbCmdList    */
    unsigned short pCmdLen,     /* pCmdLen  */
    unsigned short far * poError,   /* poError  */
    unsigned short IOFlag       /* IOflag   */
)
{
   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_FILEIO - NOT SUPPORTED");
   return ERROR_NOT_SUPPORTED;

   psffsi = psffsi;
   psffsd = psffsd;
   cbCmdList = cbCmdList;
   pCmdLen = pCmdLen;
   poError = poError;
   IOFlag = IOFlag;
}


/******************************************************************
*
******************************************************************/
int far pascal _loadds FS_NMPIPE(
    struct sffsi far * psffsi,      /* psffsi   */
    struct sffsd far * psffsd,      /* psffsd   */
    unsigned short usOpType,        /* OpType   */
    union npoper far * pOpRec,      /* pOpRec   */
    char far * pData,           /* pData    */
    char far *  pName       /* pName    */
)
{
   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_NMPIPE - NOT SUPPORTED");
   return ERROR_NOT_SUPPORTED;

   psffsi = psffsi;
   psffsd = psffsd;
   usOpType = usOpType;
   pOpRec = pOpRec;
   pData = pData;
   pName = pName;
}

POPENINFO GetOpenInfo(struct sffsd far * psffsd)
{
POPENINFO pOpenInfo = *(POPENINFO *)psffsd;
USHORT rc;

   rc = MY_PROBEBUF(PB_OPREAD, (PBYTE)pOpenInfo, sizeof (OPENINFO));
   if (rc)
      {
      CritMessage("FAT32: Protection VIOLATION (OpenInfo) in GetOpenInfo! (SYS%d)", rc);
      Message("FAT32: Protection VIOLATION (OpenInfo) in GetOpenInfo!(SYS%d)", rc);
      return NULL;
      }
   return pOpenInfo;
}

USHORT MY_ISCURDIRPREFIX( PSZ pszName )
{
PSHOPENINFO pSH;
int iLength = strlen( pszName );

   pSH = pGlobSH;
   while (pSH)
      {
      if ( !strnicmp(pSH->szFileName, pszName, iLength ) &&
           (( pSH->szFileName[ iLength ] == '\\' ) || ( pSH->szFileName[ iLength ] == '\0' )))
         return ERROR_CURRENT_DIRECTORY;

      pSH = (PSHOPENINFO)pSH->pNext;
      }

   return 0;
}
