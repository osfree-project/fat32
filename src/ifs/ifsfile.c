#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>

#define INCL_DOS
#define INCL_DOSDEVIOCTL
#define INCL_DOSDEVICES
#define INCL_DOSERRORS

#include "os2.h"
#include "portable.h"
#include "fat32ifs.h"

#define SECTORS_OF_2GB  (( LONG )( 2UL * 1024 * 1024 * 1024 / SECTOR_SIZE ))

PRIVATE volatile PSHOPENINFO pGlobSH = NULL;

PRIVATE VOID ResetAllCurrents(PVOLINFO pVolInfo, POPENINFO pOI);

ULONG PositionToOffset(PVOLINFO pVolInfo, POPENINFO pOpenInfo, LONGLONG llOffset);
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
PDIRENTRY pDirEntry;
PDIRENTRY1 pDirEntryStream = NULL;
POPENINFO pOpenInfo = NULL;
PDIRENTRY1 pDirStream = NULL;
PSHOPENINFO pDirSHInfo = NULL;
USHORT   usIOMode;
ULONGLONG size;
USHORT rc;

   _asm push es;

#ifdef INCL_LONGLONG
   size = psffsi->sfi_size;

   if (f32Parms.fLargeFiles)
      size = psffsi->sfi_sizel;
#else
   AssignUL(&size, psffsi->sfi_size);

   if (f32Parms.fLargeFiles)
      Assign(&size, *(PULONGLONG)&psffsi->sfi_sizel);
#endif

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

   pDirEntry = (PDIRENTRY)malloc((size_t)sizeof(DIRENTRY));
   if (!pDirEntry)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto FS_OPENCREATEEXIT;
      }
#ifdef EXFAT
   pDirEntryStream = (PDIRENTRY1)malloc((size_t)sizeof(DIRENTRY1));
   if (!pDirEntryStream)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto FS_OPENCREATEEXIT;
      }
   pDirStream = (PDIRENTRY1)malloc((size_t)sizeof(DIRENTRY1));
   if (!pDirStream)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto FS_OPENCREATEEXIT;
      }
   pDirSHInfo = (PSHOPENINFO)malloc((size_t)sizeof(SHOPENINFO));
   if (!pDirSHInfo)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto FS_OPENCREATEEXIT;
      }
#endif

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
      //BYTE szLongName[ FAT32MAXPATH ];
      PSZ szLongName = (PSZ)malloc((size_t)FAT32MAXPATH);
      if (!szLongName)
         {
         rc = ERROR_NOT_ENOUGH_MEMORY;
         goto FS_OPENCREATEEXIT;
         }

      if( TranslateName(pVolInfo, 0L, pName, szLongName, TRANSLATE_SHORT_TO_LONG )) ////
         strcpy( szLongName, pName );

      pOpenInfo->pSHInfo = GetSH( szLongName, pOpenInfo);
      if (!pOpenInfo->pSHInfo)
         {
         free(szLongName);
         rc = ERROR_TOO_MANY_OPEN_FILES;
         goto FS_OPENCREATEEXIT;
         }
      //pOpenInfo->pSHInfo->sOpenCount++; //
      if (pOpenInfo->pSHInfo->fLock)
         {
         free(szLongName);
         rc = ERROR_ACCESS_DENIED;
         goto FS_OPENCREATEEXIT;
         }

      ulDirCluster = FindDirCluster(pVolInfo, ////
         pcdfsi,
         pcdfsd,
         pName,
         usCurDirEnd,
         RETURN_PARENT_DIR,
         &pszFile,
         pDirStream);

      if (ulDirCluster == pVolInfo->ulFatEof)
         {
         free(szLongName);
         rc = ERROR_PATH_NOT_FOUND;
         goto FS_OPENCREATEEXIT;
         }

#ifdef EXFAT
      if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
         {
         SetSHInfo1(pVolInfo, pDirStream, pDirSHInfo);
         }
#endif

      pOpenInfo->pDirSHInfo = pDirSHInfo;
#if 0
      if (f32Parms.fEAS)
         {
         if (IsEASFile(pszFile))
            {
            free(szLongName);
            rc = ERROR_ACCESS_DENIED;
            goto FS_OPENCREATEEXIT;
            }
         }
#endif
      ulCluster = FindPathCluster(pVolInfo, ulDirCluster, pszFile, pDirSHInfo,
         pDirEntry, pDirEntryStream, NULL);

      if (pOpenInfo->pSHInfo->sOpenCount > 1)
         {
         if (pOpenInfo->pSHInfo->bAttr & FILE_DIRECTORY)
            {
            free(szLongName);
            rc = ERROR_ACCESS_DENIED;
            goto FS_OPENCREATEEXIT;
            }

         if (f32Parms.fMessageActive & LOG_FS)
            Message("File has been previously opened!");
         if (ulCluster != pOpenInfo->pSHInfo->ulStartCluster)
            Message("ulCluster with this open and the previous one are not the same!");
         //ulCluster    = pOpenInfo->pSHInfo->ulStartCluster;
         //pDirEntry->bAttr = pOpenInfo->pSHInfo->bAttr;
         }

      if (ulCluster == pVolInfo->ulFatEof)
         {
         if (!(usOpenFlag & FILE_CREATE))
            {
            free(szLongName);
            rc = ERROR_OPEN_FAILED;
            goto FS_OPENCREATEEXIT;
            }

         if (pVolInfo->fWriteProtected)
            {
            free(szLongName);
            rc = ERROR_WRITE_PROTECT;
            goto FS_OPENCREATEEXIT;
            }
         }
      else
         {
#ifdef EXFAT
         if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
            {
#endif
            if (pDirEntry->bAttr & FILE_DIRECTORY)
               {
               free(szLongName);
               rc = ERROR_ACCESS_DENIED;
               goto FS_OPENCREATEEXIT;
               }
#ifdef EXFAT
            }
         else
            {
            if (((PDIRENTRY1)pDirEntry)->u.File.usFileAttr & FILE_DIRECTORY)
               {
               free(szLongName);
               rc = ERROR_ACCESS_DENIED;
               goto FS_OPENCREATEEXIT;
               }

            pOpenInfo->pSHInfo->fNoFatChain = pDirEntryStream->u.Stream.bNoFatChain;
            }
#endif

         if (!(usOpenFlag & (FILE_OPEN | FILE_TRUNCATE)))
            {
            free(szLongName);
            rc = ERROR_OPEN_FAILED;
            goto FS_OPENCREATEEXIT;
            }

#ifdef EXFAT
         if ( ((pVolInfo->bFatType < FAT_TYPE_EXFAT) && pDirEntry->bAttr & FILE_READONLY &&
              (ulOpenMode & OPEN_ACCESS_WRITEONLY || ulOpenMode & OPEN_ACCESS_READWRITE)) ||
              ((pVolInfo->bFatType == FAT_TYPE_EXFAT) && ((PDIRENTRY1)pDirEntry)->u.File.usFileAttr & FILE_READONLY &&
              (ulOpenMode & OPEN_ACCESS_WRITEONLY || ulOpenMode & OPEN_ACCESS_READWRITE)) )
#else
         if ( pDirEntry->bAttr & FILE_READONLY &&
              (ulOpenMode & OPEN_ACCESS_WRITEONLY || ulOpenMode & OPEN_ACCESS_READWRITE) )
#endif
            {
            if ((psffsi->sfi_type & STYPE_FCB) && (ulOpenMode & OPEN_ACCESS_READWRITE))
               {
               ulOpenMode &= ~(OPEN_ACCESS_WRITEONLY | OPEN_ACCESS_READWRITE);
               psffsi->sfi_mode &= ~(OPEN_ACCESS_WRITEONLY | OPEN_ACCESS_READWRITE);
               }
            else
               {
               free(szLongName);
               rc = ERROR_ACCESS_DENIED;
               goto FS_OPENCREATEEXIT;
               }
            }
         }

      if (!pVolInfo->fDiskCleanOnMount &&
        (ulOpenMode & OPEN_ACCESS_WRITEONLY ||
         ulOpenMode & OPEN_ACCESS_READWRITE))
         {
         free(szLongName);
         rc = ERROR_VOLUME_DIRTY;
         goto FS_OPENCREATEEXIT;
         }

      if ( !pVolInfo->fDiskCleanOnMount && !f32Parms.fReadonly &&
           !(ulOpenMode & OPEN_FLAGS_DASD) )
         {
         free(szLongName);
         rc = ERROR_VOLUME_DIRTY;
         goto FS_OPENCREATEEXIT;
         }

      if (ulCluster == pVolInfo->ulFatEof)
         {
         memset(pDirEntry, 0, sizeof (DIRENTRY));
#ifdef EXFAT
         if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
#endif
            pDirEntry->bAttr = (BYTE)(usAttr & (FILE_READONLY | FILE_HIDDEN | FILE_SYSTEM | FILE_ARCHIVED));
#ifdef EXFAT
         else
            ((PDIRENTRY1)pDirEntry)->u.File.usFileAttr = (BYTE)(usAttr & (FILE_READONLY | FILE_HIDDEN | FILE_SYSTEM | FILE_ARCHIVED));
#endif
         ulCluster = 0;

#ifdef EXFAT
         if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
#endif
            {
#ifdef INCL_LONGLONG
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
#else
            if (f32Parms.fLargeFiles)
               {
               if (GreaterUL(size, ULONG_MAX))
                  AssignUL(&size, ULONG_MAX);
               }
            else
               {
               if (GreaterUL(size, (ULONG)LONG_MAX))
                  AssignUL(&size, (ULONG)LONG_MAX);
               }
#endif
            }

#ifdef EXFAT
         if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
            {
            PDIRENTRY1 pDirEntry1 = (PDIRENTRY1)pDirEntry;
            pDirEntryStream->u.Stream.bAllocPossible = 1;
            pDirEntryStream->u.Stream.bNoFatChain = (BYTE)pOpenInfo->pSHInfo->fNoFatChain;
            pDirEntryStream->u.Stream.ulFirstClus = ulCluster;
#ifdef INCL_LONGLONG
            pDirEntryStream->u.Stream.ullValidDataLen = 0;
            pDirEntryStream->u.Stream.ullDataLen = 0;
#else
            AssignUL(pDirEntryStream->u.Stream.ullValidDataLen, 0);
            AssignUL(pDirEntryStream->u.Stream.ullDataLen, 0);
#endif
            }
#endif

#ifdef INCL_LONGLONG
         if (size > 0)
#else
         if (GreaterUL(size, 0))
#endif
            {
            ULONG ulClustersNeeded;
#ifdef INCL_LONGLONG
            ulClustersNeeded = size / pVolInfo->ulClusterSize +
                  (size % pVolInfo->ulClusterSize ? 1:0);
#else
            {
            ULONGLONG ullSize, ullRest;

            ullSize = DivUL(size, pVolInfo->ulClusterSize);
            ullRest = ModUL(size, pVolInfo->ulClusterSize);

            if (NeqUL(ullRest, 0))
               AssignUL(&ullRest, 1);
            else
               AssignUL(&ullRest, 0);

            ullSize = Add(ullSize, ullRest);
            ulClustersNeeded = ullSize.ulLo;
            }
#endif
            ulCluster = MakeFatChain(pVolInfo, pOpenInfo->pSHInfo, pVolInfo->ulFatEof, ulClustersNeeded, NULL);
            if (ulCluster != pVolInfo->ulFatEof)
               {
#ifdef EXFAT
               if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
                  {
#endif
                  pDirEntry->wCluster = LOUSHORT(ulCluster);
                  pDirEntry->wClusterHigh = HIUSHORT(ulCluster);
#ifdef INCL_LONGLONG
                  pDirEntry->ulFileSize = size;
#else
                  pDirEntry->ulFileSize = size.ulLo;
#endif
#ifdef EXFAT
                  }
               else
                  {
                  //PDIRENTRY1 pDirEntry1 = (PDIRENTRY1)pDirEntry;
                  pDirEntryStream->u.Stream.ulFirstClus = ulCluster;
#ifdef INCL_LONGLONG
                  pDirEntryStream->u.Stream.ullValidDataLen = size;
                  pDirEntryStream->u.Stream.ullDataLen =
                     (size / pVolInfo->ulClusterSize) * pVolInfo->ulClusterSize +
                     (size % pVolInfo->ulClusterSize ? pVolInfo->ulClusterSize : 0);
#else
                  {
                  ULONGLONG ullRest;

                  Assign(pDirEntryStream->u.Stream.ullValidDataLen, size);
                  pDirEntryStream->u.Stream.ullDataLen = DivUL(size, pVolInfo->ulClusterSize);
                  pDirEntryStream->u.Stream.ullDataLen = MulUL(pDirEntryStream->u.Stream.ullDataLen, pVolInfo->ulClusterSize);
                  ullRest = ModUL(size, pVolInfo->ulClusterSize);

                  if (NeqUL(ullRest, 0))
                     AssignUL(&ullRest, pVolInfo->ulClusterSize);
                  else
                     AssignUL(&ullRest, 0);

                  pDirEntryStream->u.Stream.ullDataLen = Add(DirEntryStream.u.Stream.ullDataLen, ullRest);
                  }
#endif
                  pDirEntryStream->u.Stream.bAllocPossible = 1;
                  pDirEntryStream->u.Stream.bNoFatChain = (BYTE)pOpenInfo->pSHInfo->fNoFatChain;
                  }
#endif
               }
            else
               {
               free(szLongName);
               rc = ERROR_DISK_FULL;
               goto FS_OPENCREATEEXIT;
               }
            }

         rc = MakeDirEntry(pVolInfo, ulDirCluster, pDirSHInfo, pDirEntry, pDirEntryStream, pszFile);
         if (rc)
            {
            free(szLongName);
            goto FS_OPENCREATEEXIT;
            }

#ifdef EXFAT
         if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
            {
#endif
            memcpy(&psffsi->sfi_ctime, &pDirEntry->wCreateTime, sizeof (USHORT));
            memcpy(&psffsi->sfi_cdate, &pDirEntry->wCreateDate, sizeof (USHORT));
            psffsi->sfi_atime = 0;
            memcpy(&psffsi->sfi_adate, &pDirEntry->wAccessDate, sizeof (USHORT));
            memcpy(&psffsi->sfi_mtime, &pDirEntry->wLastWriteTime, sizeof (USHORT));
            memcpy(&psffsi->sfi_mdate, &pDirEntry->wLastWriteDate, sizeof (USHORT));
#ifdef EXFAT
            }
         else
            {
            PDIRENTRY1 pDirEntry1 = (PDIRENTRY1)pDirEntry;
            FDATE date = GetDate1(pDirEntry1->u.File.ulCreateTimestp);
            FTIME time = GetTime1(pDirEntry1->u.File.ulCreateTimestp);
            memcpy(&psffsi->sfi_ctime, &time, sizeof (USHORT));
            memcpy(&psffsi->sfi_cdate, &date, sizeof (USHORT));
            date = GetDate1(pDirEntry1->u.File.ulLastAccessedTimestp);
            memcpy(&psffsi->sfi_adate, &date, sizeof (USHORT));
            time = GetTime1(pDirEntry1->u.File.ulLastAccessedTimestp);
            memcpy(&psffsi->sfi_atime, &time, sizeof (USHORT));
            date = GetDate1(pDirEntry1->u.File.ulLastModifiedTimestp);
            time = GetTime1(pDirEntry1->u.File.ulLastModifiedTimestp);
            memcpy(&psffsi->sfi_mtime, &time, sizeof (USHORT));
            memcpy(&psffsi->sfi_mdate, &date, sizeof (USHORT));
            }
#endif

         pOpenInfo->pSHInfo->fMustCommit = TRUE;

         psffsi->sfi_tstamp = ST_SCREAT | ST_PCREAT | ST_SREAD | ST_PREAD | ST_SWRITE | ST_PWRITE;

         *pfGenFlag = 0;
         if (f32Parms.fEAS && pEABuf && pEABuf != MYNULL)
            {
            rc = usModifyEAS(pVolInfo, ulDirCluster, pDirSHInfo, pszFile, (PEAOP)pEABuf);
            if (rc)
               {
               free(szLongName);
               goto FS_OPENCREATEEXIT;
               }
            }
         *pAction   = FILE_CREATED;
         }
      else if (usOpenFlag & FILE_TRUNCATE)
         {
         PDIRENTRY pDirOld;
         PDIRENTRY1 pDirOldStream = NULL;
         PDIRENTRY1 pDirEntry1 = (PDIRENTRY1)pDirEntry;

         pDirOld = (PDIRENTRY)malloc((size_t)sizeof(DIRENTRY));
         if (!pDirOld)
            {
            free(szLongName);
            rc = ERROR_NOT_ENOUGH_MEMORY;
            goto FS_OPENCREATEEXIT;
            }
#ifdef EXFAT
         pDirOldStream = (PDIRENTRY1)malloc((size_t)sizeof(DIRENTRY1));
         if (!pDirOldStream)
            {
            free(szLongName);
            rc = ERROR_NOT_ENOUGH_MEMORY;
            goto FS_OPENCREATEEXIT;
            }
#endif

         if (pVolInfo->fWriteProtected)
            {
            free(pDirOld);
#ifdef EXFAT
            free(pDirOldStream);
#endif
            free(szLongName);
            rc = ERROR_WRITE_PROTECT;
            goto FS_OPENCREATEEXIT;
            }

#ifdef EXFAT
         if ( ((pVolInfo->bFatType <  FAT_TYPE_EXFAT) && (pDirEntry->bAttr & FILE_HIDDEN) && !(usAttr & FILE_HIDDEN)) ||
              ((pVolInfo->bFatType == FAT_TYPE_EXFAT) && (pDirEntry1->u.File.usFileAttr & FILE_HIDDEN) && !(usAttr & FILE_HIDDEN)) )
#else
         if ( (pDirEntry->bAttr & FILE_HIDDEN) && !(usAttr & FILE_HIDDEN) )
#endif
            {
            free(pDirOld);
#ifdef EXFAT
            free(pDirOldStream);
#endif
            free(szLongName);
            rc = ERROR_ACCESS_DENIED;
            goto FS_OPENCREATEEXIT;
            }

#ifdef EXFAT
         if ( ((pVolInfo->bFatType <  FAT_TYPE_EXFAT) && (pDirEntry->bAttr & FILE_SYSTEM) && !(usAttr & FILE_SYSTEM)) ||
              ((pVolInfo->bFatType == FAT_TYPE_EXFAT) && (pDirEntry1->u.File.usFileAttr & FILE_SYSTEM) && !(usAttr & FILE_SYSTEM)) )
#else
         if ( (pDirEntry->bAttr & FILE_SYSTEM) && !(usAttr & FILE_SYSTEM) )
#endif
            {
            free(pDirOld);
#ifdef EXFAT
            free(pDirOldStream);
#endif
            free(szLongName);
            rc = ERROR_ACCESS_DENIED;
            goto FS_OPENCREATEEXIT;
            }

         memcpy(pDirOld, pDirEntry, sizeof (DIRENTRY));
#ifdef EXFAT
         memcpy(pDirOldStream, pDirEntryStream, sizeof (DIRENTRY1));

         if (pVolInfo->bFatType <  FAT_TYPE_EXFAT)
            {
#endif
            pDirEntry->bAttr = (BYTE)(usAttr & (FILE_READONLY | FILE_HIDDEN | FILE_SYSTEM | FILE_ARCHIVED));
            pDirEntry->wCluster     = 0;
            pDirEntry->wClusterHigh = 0;
            pDirEntry->ulFileSize   = 0;
            pDirEntry->fEAS = FILE_HAS_NO_EAS;
#ifdef EXFAT
            }
         else
            {
            PDIRENTRY1 pDirEntry1 = (PDIRENTRY1)pDirEntry;
            pDirEntry1->bEntryType = ENTRY_TYPE_FILE;
            pDirEntryStream->bEntryType = ENTRY_TYPE_STREAM_EXT;
            pDirEntry1->u.File.usFileAttr = (BYTE)(usAttr & (FILE_READONLY | FILE_HIDDEN | FILE_SYSTEM | FILE_ARCHIVED));
            pDirEntry1->u.File.fEAS = FILE_HAS_NO_EAS;
            pDirEntryStream->u.Stream.ulFirstClus = 0;
#ifdef INCL_LONGLONG
            pDirEntryStream->u.Stream.ullValidDataLen = 0;
            pDirEntryStream->u.Stream.ullDataLen = 0;
#else
            AssignUL(pDirEntryStream->u.Stream.ullValidDataLen, 0);
            AssignUL(pDirEntryStream->u.Stream.ullDataLen, 0);
#endif
            pDirEntryStream->u.Stream.bAllocPossible = 1;
            pDirEntryStream->u.Stream.bNoFatChain = (BYTE)pOpenInfo->pSHInfo->fNoFatChain;
            }
#endif

         rc = usDeleteEAS(pVolInfo, ulDirCluster, pDirSHInfo, pszFile);
         if (rc)
            {
            free(pDirOld);
#ifdef EXFAT
            free(pDirOldStream);
#endif
            free(szLongName);
            goto FS_OPENCREATEEXIT;
            }
#ifdef EXFAT
         if (pVolInfo->bFatType <  FAT_TYPE_EXFAT)
#endif
            pDirOld->fEAS = FILE_HAS_NO_EAS;
#ifdef EXFAT
         else
            {
            PDIRENTRY1 pDirOld1 = (PDIRENTRY1)pDirOld;
            pDirOld1->u.File.fEAS = FILE_HAS_NO_EAS;
            }
#endif

         if (ulCluster)
            DeleteFatChain(pVolInfo, ulCluster);
         pOpenInfo->pSHInfo->ulLastCluster = pVolInfo->ulFatEof;
         ResetAllCurrents(pVolInfo, pOpenInfo);
         ulCluster = 0;

#ifdef EXFAT
         if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
#endif
            {
#ifdef INCL_LONGLONG
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
#else
            if (f32Parms.fLargeFiles)
               {
               if (GreaterUL(size, ULONG_MAX))
                  AssignUL(&size, ULONG_MAX);
               }
            else
               {
               if (GreaterUL(size, (ULONG)LONG_MAX))
                  AssignUL(&size, (ULONG)LONG_MAX);
               }
#endif
            }

#ifdef INCL_LONGLONG
         if (size > 0)
#else
         if (GreaterUL(size, 0))
#endif
            {
            ULONG ulClustersNeeded;
#ifdef INCL_LONGLONG
            ulClustersNeeded = size / pVolInfo->ulClusterSize +
                  (size % pVolInfo->ulClusterSize ? 1:0);
#else
            {
            ULONGLONG ullSize, ullRest;

            ullSize = DivUL(size, pVolInfo->ulClusterSize);
            ullRest = ModUL(size, pVolInfo->ulClusterSize);

            if (NeqUL(ullRest, 0))
               AssignUL(&ullRest, 1);
            else
               AssignUL(&ullRest, 0);

            ullSize = Add(ullSize, ullRest);
            ulClustersNeeded = ullSize.ulLo;
            }
#endif
            ulCluster = MakeFatChain(pVolInfo, pOpenInfo->pSHInfo, pVolInfo->ulFatEof, ulClustersNeeded, &pOpenInfo->pSHInfo->ulLastCluster);
            if (ulCluster != pVolInfo->ulFatEof)
               {
#ifdef EXFAT
               if (pVolInfo->bFatType <  FAT_TYPE_EXFAT)
                  {
#endif
                  pDirEntry->wCluster = LOUSHORT(ulCluster);
                  pDirEntry->wClusterHigh = HIUSHORT(ulCluster);
#ifdef INCL_LONGLONG
                  pDirEntry->ulFileSize = size;
#else
                  pDirEntry->ulFileSize = size.ulLo;
#endif
#ifdef EXFAT
                  }
               else
                  {
                  PDIRENTRY1 pDirEntry1 = (PDIRENTRY1)pDirEntry;
                  pDirEntry1->bEntryType = ENTRY_TYPE_FILE;
                  pDirEntryStream->bEntryType = ENTRY_TYPE_STREAM_EXT;
                  pDirEntryStream->u.Stream.ulFirstClus = ulCluster;
#ifdef INCL_LONGLONG
                  pDirEntryStream->u.Stream.ullValidDataLen = size;
                  pDirEntryStream->u.Stream.ullDataLen =
                     (size / pVolInfo->ulClusterSize) * pVolInfo->ulClusterSize +
                     (size % pVolInfo->ulClusterSize ? pVolInfo->ulClusterSize : 0);
#else
                  {

                  ULONGLONG ullRest;

                  Assign(pDirEntryStream->u.Stream.ullValidDataLen, size);
                  pDirEntryStream->u.Stream.ullDataLen = DivUL(size, pVolInfo->ulClusterSize);
                  pDirEntryStream->u.Stream.ullDataLen = MulUL(pDirEntryStream->u.Stream.ullDataLen, pVolInfo->ulClusterSize);
                  ullRest = ModUL(size, pVolInfo->ulClusterSize);

                  if (NeqUL(ullRest, 0))
                     AssignUL(&ullRest, pVolInfo->ulClusterSize);
                  else
                     AssignUL(&ullRest, 0);

                  pDirEntryStream->u.Stream.ullDataLen = Add(pDirEntryStream->u.Stream.ullDataLen, ullRest);
                  }
#endif
                  pDirEntryStream->u.Stream.bAllocPossible = 1;
                  pDirEntryStream->u.Stream.bNoFatChain = (BYTE)pOpenInfo->pSHInfo->fNoFatChain;
                  }
#endif
               }
            else
               {
               ulCluster = 0;
               rc = ModifyDirectory(pVolInfo, ulDirCluster, pDirSHInfo, MODIFY_DIR_UPDATE,
                  pDirOld, pDirEntry, pDirOldStream, pDirEntryStream, NULL, usIOMode);
               if (!rc)
                  rc = ERROR_DISK_FULL;
               free(pDirOld);
#ifdef EXFAT
               free(pDirOldStream);
#endif
               free(szLongName);
               goto FS_OPENCREATEEXIT;
               }
            }
         rc = ModifyDirectory(pVolInfo, ulDirCluster, pDirSHInfo, MODIFY_DIR_UPDATE,
            pDirOld, pDirEntry, pDirOldStream, pDirEntryStream, NULL, usIOMode);
         if (rc)
            {
            free(pDirOld);
#ifdef EXFAT
            free(pDirOldStream);
#endif
            free(szLongName);
            goto FS_OPENCREATEEXIT;
            }

#ifdef EXFAT
         if (pVolInfo->bFatType <  FAT_TYPE_EXFAT)
            {
#endif
            memcpy(&psffsi->sfi_ctime, &pDirEntry->wCreateTime, sizeof (USHORT));
            memcpy(&psffsi->sfi_cdate, &pDirEntry->wCreateDate, sizeof (USHORT));
            psffsi->sfi_atime = 0;
            memcpy(&psffsi->sfi_adate, &pDirEntry->wAccessDate, sizeof (USHORT));
            memcpy(&psffsi->sfi_mtime, &pDirEntry->wLastWriteTime, sizeof (USHORT));
            memcpy(&psffsi->sfi_mdate, &pDirEntry->wLastWriteDate, sizeof (USHORT));
#ifdef EXFAT
            }
         else
            {
            PDIRENTRY1 pDirEntry1 = (PDIRENTRY1)pDirEntry;
            FDATE date = GetDate1(pDirEntry1->u.File.ulCreateTimestp);
            FTIME time = GetTime1(pDirEntry1->u.File.ulCreateTimestp);
            memcpy(&psffsi->sfi_ctime, &time, sizeof (USHORT));
            memcpy(&psffsi->sfi_cdate, &date, sizeof (USHORT));
            date = GetDate1(pDirEntry1->u.File.ulLastAccessedTimestp);
            memcpy(&psffsi->sfi_adate, &date, sizeof (USHORT));
            time = GetTime1(pDirEntry1->u.File.ulLastAccessedTimestp);
            memcpy(&psffsi->sfi_atime, &time, sizeof (USHORT));
            date = GetDate1(pDirEntry1->u.File.ulLastModifiedTimestp);
            time = GetTime1(pDirEntry1->u.File.ulLastModifiedTimestp);
            memcpy(&psffsi->sfi_mtime, &time, sizeof (USHORT));
            memcpy(&psffsi->sfi_mdate, &date, sizeof (USHORT));
            }
#endif

         pOpenInfo->pSHInfo->fMustCommit = TRUE;

         psffsi->sfi_tstamp = ST_SWRITE | ST_PWRITE | ST_SREAD | ST_PREAD;
         *pfGenFlag = 0;
         if (f32Parms.fEAS && pEABuf && pEABuf != MYNULL)
            {
            rc = usModifyEAS(pVolInfo, ulDirCluster, pDirSHInfo, pszFile, (PEAOP)pEABuf);
            if (rc)
               {
               free(pDirOld);
#ifdef EXFAT
               free(pDirOldStream);
#endif
               free(szLongName);
               goto FS_OPENCREATEEXIT;
               }
            }

         *pAction = FILE_TRUNCATED;
         free(pDirOld);
#ifdef EXFAT
         free(pDirOldStream);
#endif
         free(szLongName);
         }
      else
         {
#ifdef EXFAT
         if (pVolInfo->bFatType <  FAT_TYPE_EXFAT)
            {
#endif
            memcpy(&psffsi->sfi_ctime, &pDirEntry->wCreateTime, sizeof (USHORT));
            memcpy(&psffsi->sfi_cdate, &pDirEntry->wCreateDate, sizeof (USHORT));
            psffsi->sfi_atime = 0;
            memcpy(&psffsi->sfi_adate, &pDirEntry->wAccessDate, sizeof (USHORT));
            memcpy(&psffsi->sfi_mtime, &pDirEntry->wLastWriteTime, sizeof (USHORT));
            memcpy(&psffsi->sfi_mdate, &pDirEntry->wLastWriteDate, sizeof (USHORT));
#ifdef EXFAT
            }
         else
            {
            PDIRENTRY1 pDirEntry1 = (PDIRENTRY1)pDirEntry;
            FDATE date = GetDate1(pDirEntry1->u.File.ulCreateTimestp);
            FTIME time = GetTime1(pDirEntry1->u.File.ulCreateTimestp);
            memcpy(&psffsi->sfi_ctime, &time, sizeof (USHORT));
            memcpy(&psffsi->sfi_cdate, &date, sizeof (USHORT));
            date = GetDate1(pDirEntry1->u.File.ulLastAccessedTimestp);
            memcpy(&psffsi->sfi_adate, &date, sizeof (USHORT));
            time = GetTime1(pDirEntry1->u.File.ulLastAccessedTimestp);
            memcpy(&psffsi->sfi_atime, &time, sizeof (USHORT));
            date = GetDate1(pDirEntry1->u.File.ulLastModifiedTimestp);
            time = GetTime1(pDirEntry1->u.File.ulLastModifiedTimestp);
            memcpy(&psffsi->sfi_mtime, &time, sizeof (USHORT));
            memcpy(&psffsi->sfi_mdate, &date, sizeof (USHORT));
            }
#endif

         psffsi->sfi_tstamp = 0;
         *pAction = FILE_EXISTED;
         *pfGenFlag = ( HAS_CRITICAL_EAS( pDirEntry->fEAS ) ? 1 : 0);
         }

#if 0
      rc = FSH_UPPERCASE(pName, sizeof pOpenInfo->pSHInfo->szFileName, pOpenInfo->pSHInfo->szFileName);
      if (rc || !strlen(pOpenInfo->pSHInfo->szFileName))
         {
         strncpy(pOpenInfo->pSHInfo->szFileName, pName, sizeof pOpenInfo->pSHInfo->szFileName);
         rc = 0;
         }
#endif

      if (pOpenInfo->pSHInfo->sOpenCount == 1)
         {
#ifdef EXFAT
         PDIRENTRY1 pDirEntry1 = (PDIRENTRY1)pDirEntry;
#endif
         pOpenInfo->pSHInfo->ulLastCluster = GetLastCluster(pVolInfo, ulCluster, pDirEntryStream);
#ifdef EXFAT
         if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
#endif
            pOpenInfo->pSHInfo->bAttr = pDirEntry->bAttr;
#ifdef EXFAT
         else
            pOpenInfo->pSHInfo->bAttr = (BYTE)pDirEntry1->u.File.usFileAttr;
#endif
         }

      pOpenInfo->pSHInfo->ulDirCluster = ulDirCluster;
      pOpenInfo->pSHInfo->ulStartCluster = ulCluster;
      if (ulCluster)
         pOpenInfo->ulCurCluster = ulCluster;
      else
         pOpenInfo->ulCurCluster = pVolInfo->ulFatEof;

      pOpenInfo->ulCurBlock = 0;

#ifdef EXFAT
      if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
         {
#endif
#ifdef INCL_LONGLONG
         size = pDirEntry->ulFileSize;
#else
         AssignUL(&size, pDirEntry->ulFileSize);
#endif
         psffsi->sfi_DOSattr = pDirEntry->bAttr;
#ifdef EXFAT
         }
      else
         {
         PDIRENTRY1 pDirEntry1 = (PDIRENTRY1)pDirEntry;
#ifdef INCL_LONGLONG
         size = pDirEntryStream->u.Stream.ullValidDataLen;
#else
         Assign(&size, pDirEntryStream->u.Stream.ullValidDataLen);
#endif
         psffsi->sfi_DOSattr = (UCHAR)pDirEntry1->u.File.usFileAttr;
         }
#endif
      free(szLongName);
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
#ifdef INCL_LONGLONG
      size = pVolInfo->BootSect.bpb.BigTotalSectors * pVolInfo->BootSect.bpb.BytesPerSector;
#else
      AssignUL(&size, pVolInfo->BootSect.bpb.BigTotalSectors);
      size = MulUS(size, pVolInfo->BootSect.bpb.BytesPerSector);
#endif
      //if( size < SECTORS_OF_2GB )
      //   size *= pVolInfo->BootSect.bpb.BytesPerSector;
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
#ifdef INCL_LONGLONG
      psffsi->sfi_positionl = 0;
#else
      iAssignL(&psffsi->sfi_positionl, 0);
#endif

   psffsi->sfi_type &= ~STYPE_FCB;
   psffsi->sfi_mode = ulOpenMode;
   pVolInfo->ulOpenFiles++;

   rc = 0;

FS_OPENCREATEEXIT:
#ifdef INCL_LONGLONG
   psffsi->sfi_size = size;

   if (f32Parms.fLargeFiles)
      psffsi->sfi_sizel = size;
#else
   psffsi->sfi_size = size.ulLo;

   if (f32Parms.fLargeFiles)
      iAssign(&psffsi->sfi_sizel, *(PLONGLONG)&size);
#endif
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

VOID ResetAllCurrents(PVOLINFO pVolInfo, POPENINFO pOI)
{
PSHOPENINFO pSH = pOI->pSHInfo;

   pOI = (POPENINFO)pSH->pChild;
   while (pOI)
      {
      pOI->ulCurCluster = pVolInfo->ulFatEof;
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
ULONG  ulClusterOffset;
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

#ifdef INCL_LONGLONG
   size = (ULONGLONG)psffsi->sfi_size;
   pos  = (LONGLONG)psffsi->sfi_position;

   if (f32Parms.fLargeFiles)
      {
      size = (ULONGLONG)psffsi->sfi_sizel;
      pos  = (LONGLONG)psffsi->sfi_positionl;
      }
#else
   AssignUL(&size, psffsi->sfi_size);
   iAssignL(&pos , psffsi->sfi_position);

   if (f32Parms.fLargeFiles)
      {
      Assign(&size,  *(PULONGLONG)&psffsi->sfi_sizel);
      iAssign(&pos , psffsi->sfi_positionl);
      }
#endif

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

   pbCluster = malloc((size_t)pVolInfo->ulBlockSize);
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
#ifdef INCL_LONGLONG
            ulSector        = (ULONG)pos;
#else
            ulSector        = pos.ulLo;
#endif
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

#ifdef INCL_LONGLONG
            ulSector        = (ULONG)(pos / (ULONG)usBytesPerSector);
            usSectorOffset  = (USHORT)(pos % (ULONG)usBytesPerSector);
#else
            {
            LONGLONG llRes;

            llRes = iDivUL(pos, (ULONG)usBytesPerSector);
            ulSector = llRes.ulLo;
            llRes = iModUL(pos, (ULONG)usBytesPerSector);
            usSectorOffset = (USHORT)llRes.ulLo;
            }
#endif
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
#ifdef INCL_LONGLONG
            pos                     += usCurBytesToRead;
#else
            pos                     =  iAddUS(pos, usCurBytesToRead);
#endif
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
#ifdef INCL_LONGLONG
            pos                     += usBytesToRead - usRemaining;
#else
            pos                     =  iAddUS(pos, usBytesToRead - usRemaining);
#endif
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
#ifdef INCL_LONGLONG
            pos                     += usRemaining;
#else
            pos                     =  iAddUS(pos, usRemaining);
#endif
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
        if (pOpenInfo->ulCurCluster == pVolInfo->ulFatEof)
           {
           pOpenInfo->ulCurCluster = PositionToOffset(pVolInfo, pOpenInfo, pos);
#ifdef INCL_LONGLONG
           pOpenInfo->ulCurBlock = pOpenInfo->ulCurBlock = (pos / pVolInfo->ulBlockSize) % (pVolInfo->ulClusterSize / pVolInfo->ulBlockSize);
#else
           {
           LONGLONG llRes;

           llRes = iModUL(iDivUL(pos, pVolInfo->ulBlockSize), pVolInfo->ulClusterSize / pVolInfo->ulBlockSize);
           pOpenInfo->ulCurBlock = pOpenInfo->ulCurBlock = llRes.ulLo;
           }
#endif
           }

        /*
            First, handle the first part that does not align on a cluster border
        */
        ulBytesPerCluster = pVolInfo->ulClusterSize;
        ulBytesPerBlock   = pVolInfo->ulBlockSize;
#ifdef INCL_LONGLONG
        ulClusterOffset   = (ULONG)(pos % ulBytesPerCluster); /* get remainder */
        usBlockOffset     = (USHORT)(pos % ulBytesPerBlock);   /* get remainder */
#else
        {
        LONGLONG llRes;

        llRes = iModUL(pos, ulBytesPerCluster);
        ulClusterOffset = (ULONG)llRes.ulLo;
        llRes = iModUL(pos, ulBytesPerBlock);
        usBlockOffset   = (USHORT)llRes.ulLo; 
        }
#endif
        if
            (
                (pOpenInfo->ulCurCluster != pVolInfo->ulFatEof) &&
#ifdef INCL_LONGLONG
                (pos < (LONGLONG)size) &&
#else
                (iLess(pos, *(PLONGLONG)&size)) &&
#endif
                (usBytesToRead) &&
                (usBlockOffset)
            )
        {
            ULONG   ulCurrBytesToRead;
            USHORT  usSectorsToRead;
            USHORT  usSectorsPerCluster;
            USHORT  usSectorsPerBlock;

            usSectorsPerCluster = (USHORT)pVolInfo->SectorsPerCluster;
            usSectorsPerBlock = (USHORT)(ulBytesPerBlock / pVolInfo->BootSect.bpb.BytesPerSector);
            usSectorsToRead = usSectorsPerBlock;

            /* compute the number of bytes
                to the cluster end or
                as much as fits into the user buffer
                or how much remains to be read until file end
               whatever is the smallest
            */
            ulCurrBytesToRead   = (ulBytesPerBlock - (ULONG)usBlockOffset);
            ulCurrBytesToRead   = min(ulCurrBytesToRead,(ULONG)usBytesToRead);
#ifdef INCL_LONGLONG
            ulCurrBytesToRead   = (ULONG)min((ULONG)ulCurrBytesToRead,size - pos);
#else
            {
            ULONGLONG ullRes;
            ullRes = Sub(size, *(PULONGLONG)&pos);
            ulCurrBytesToRead   = (ULONG)min((ULONG)ulCurrBytesToRead, ullRes.ulLo);
            }
#endif
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
#ifdef INCL_LONGLONG
                pos                     += (USHORT)ulCurrBytesToRead;
#else
                pos                     =  iAddUS(pos, (USHORT)ulCurrBytesToRead);
#endif
                usBytesRead             += (USHORT)ulCurrBytesToRead;
                usBytesToRead           -= (USHORT)ulCurrBytesToRead;
            }

            if (ulClusterOffset + ulCurrBytesToRead >= ulBytesPerCluster)
            {
                pOpenInfo->ulCurCluster     = GetNextCluster(pVolInfo, pOpenInfo->pSHInfo, pOpenInfo->ulCurCluster);
                if (!pOpenInfo->ulCurCluster)
                {
                    pOpenInfo->ulCurCluster = pVolInfo->ulFatEof;
                }
            }
#ifdef INCL_LONGLONG
            pOpenInfo->ulCurBlock = (pos / pVolInfo->ulBlockSize) % (pVolInfo->ulClusterSize / pVolInfo->ulBlockSize);
#else
            {
            LONGLONG llRes;

            llRes = iDivUL(pos, pVolInfo->ulBlockSize);
            llRes = iModUL(llRes, pVolInfo->ulClusterSize / pVolInfo->ulBlockSize);
            pOpenInfo->ulCurBlock = llRes.ulLo;
            }
#endif
        }


        /*
            Second, handle the part that aligns on a cluster border and is a multiple of the cluster size
        */
        if
            (
                (pOpenInfo->ulCurCluster != pVolInfo->ulFatEof) &&
#ifdef INCL_LONGLONG
                (pos < (LONGLONG)size) &&
#else
                (iLess(pos, *(PLONGLONG)&size)) &&
#endif
                (usBytesToRead)
            )
        {
            ULONG   ulCurrCluster       = pOpenInfo->ulCurCluster;
            ULONG   ulNextCluster       = ulCurrCluster;
            ULONG   ulCurrBytesToRead   = 0;
            USHORT  usSectorsPerCluster = (USHORT)pVolInfo->SectorsPerCluster;
            USHORT  usSectorsPerBlock   = (USHORT)(ulBytesPerBlock / pVolInfo->BootSect.bpb.BytesPerSector);
            USHORT  usClustersToProcess = 0;
            USHORT  usBlocksToProcess   = 0;
            USHORT  usAdjacentClusters  = 1;
            USHORT  usAdjacentBlocks;
            USHORT  usBlocks;

#ifdef INCL_LONGLONG
            ulCurrBytesToRead           = (ULONG)min((ULONG)usBytesToRead,size - pos);
#else
            {
            ULONGLONG ullRes;
            ullRes = Sub(size, *(PULONGLONG)&pos);
            ulCurrBytesToRead           = (ULONG)min((ULONG)usBytesToRead, ullRes.ulLo);
            }
#endif

            usClustersToProcess         = (USHORT)(ulCurrBytesToRead / ulBytesPerCluster); /* get the number of full clusters */
            usBlocksToProcess           = (USHORT)(ulCurrBytesToRead / ulBytesPerBlock);   /* get the number of full clusters */
            usAdjacentBlocks            = (USHORT)(usBlocksToProcess % (pVolInfo->ulClusterSize / pVolInfo->ulBlockSize));
            usBlocks                    = usBlocksToProcess;

            if (ulCurrBytesToRead < pVolInfo->ulClusterSize)
               usAdjacentBlocks = (USHORT)(ulCurrBytesToRead / pVolInfo->ulBlockSize);

            while (usBlocks && (ulCurrCluster != pVolInfo->ulFatEof))
            {
                ulNextCluster       = GetNextCluster(pVolInfo, pOpenInfo->pSHInfo, ulCurrCluster);
                if (!ulNextCluster)
                {
                    ulNextCluster = pVolInfo->ulFatEof;
                }

                if  (
                        (ulNextCluster != pVolInfo->ulFatEof) &&
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
                        //ULONG ulBlock;

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
                             ulCurrCluster           = GetNextCluster(pVolInfo, pOpenInfo->pSHInfo, ulCurrCluster);
                             pOpenInfo->ulCurCluster = ulCurrCluster;
                             }

                           usBlocks -= 1;

                           pBufPosition                += ulBytesPerBlock;
                           ulClusterSector             += usSectorsPerBlock;
                           usAdjacentBlocks--;
                        }
                    }

#ifdef INCL_LONGLONG
                    pos                             += (USHORT)ulCurrBytesToRead;
#else
                    pos                             =  iAddUS(pos, (USHORT)ulCurrBytesToRead);
#endif
                    usBytesRead                     += (USHORT)ulCurrBytesToRead;
                    usBytesToRead                   -= (USHORT)ulCurrBytesToRead;
                    usAdjacentBlocks                =  (USHORT)(pVolInfo->ulClusterSize / pVolInfo->ulBlockSize);
                    usAdjacentClusters              =  1;

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
                (pOpenInfo->ulCurCluster != pVolInfo->ulFatEof) &&
#ifdef INCL_LONGLONG
                (pos < (LONGLONG)size) &&
#else
                (iLess(pos, *(PLONGLONG)&size)) &&
#endif
                (usBytesToRead)
            )
        {
            ULONG  ulCurrBytesToRead;
            USHORT usSectorsToRead;
            USHORT usSectorsPerCluster;
            USHORT usSectorsPerBlock;

            usSectorsToRead = (USHORT)(ulBytesPerBlock / pVolInfo->BootSect.bpb.BytesPerSector);
            usSectorsPerCluster = (USHORT)pVolInfo->SectorsPerCluster;
            usSectorsPerBlock = usSectorsToRead;

#ifdef INCL_LONGLONG
            ulCurrBytesToRead = (ULONG)min((ULONG)usBytesToRead,size - pos);
#else
            {
            ULONGLONG ullRes;
            ullRes = Sub(size, *(PULONGLONG)&pos);
            ulCurrBytesToRead  = (ULONG)min((ULONG)usBytesToRead, ullRes.ulLo);
            }
#endif
            if (ulCurrBytesToRead)
            {
                ulClusterSector = pVolInfo->ulStartOfData + (pOpenInfo->ulCurCluster-2)*usSectorsPerCluster;
                rc = ReadBlock(pVolInfo, pOpenInfo->ulCurCluster, pOpenInfo->ulCurBlock, pbCluster, usIOFlag);
                if (rc)
                {
                    goto FS_READEXIT;
                }
                memcpy(pBufPosition,pbCluster, (USHORT)ulCurrBytesToRead);

#ifdef INCL_LONGLONG
                pos                     += (USHORT)ulCurrBytesToRead;
#else
                pos                     =  iAddUS(pos, (USHORT)ulCurrBytesToRead);
#endif
                usBytesRead             += (USHORT)ulCurrBytesToRead;
                usBytesToRead           -= (USHORT)ulCurrBytesToRead;
            }
        }

#ifdef INCL_LONGLONG
        pOpenInfo->ulCurBlock = (pos / pVolInfo->ulBlockSize) % (pVolInfo->ulClusterSize / pVolInfo->ulBlockSize);
#else
        {
        LONGLONG llRes;

        llRes = iDivL(pos, pVolInfo->ulBlockSize);
        llRes = iModL(llRes, pVolInfo->ulClusterSize / pVolInfo->ulBlockSize);
        pOpenInfo->ulCurBlock = llRes.ulLo;
        }
#endif

        *pLen = usBytesRead;
        psffsi->sfi_tstamp  |= ST_SREAD | ST_PREAD;
        rc = NO_ERROR;
    }

FS_READEXIT:
#ifdef INCL_LONGLONG
   psffsi->sfi_size = (ULONG)size;
   psffsi->sfi_position = (LONG)pos;

   if (f32Parms.fLargeFiles)
      {
      psffsi->sfi_sizel = size;
      psffsi->sfi_positionl = pos;
      }
#else
   psffsi->sfi_size = size.ulLo;
   psffsi->sfi_position = pos.ulLo;

   if (f32Parms.fLargeFiles)
      {
      iAssign(&psffsi->sfi_sizel, *(PLONGLONG)&size);
      iAssign(&psffsi->sfi_positionl, pos);
      }
#endif

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
ULONG  ulClusterOffset;
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

#ifdef INCL_LONGLONG
   size = (ULONGLONG)psffsi->sfi_size;
   pos  = (LONGLONG)psffsi->sfi_position;

   if (f32Parms.fLargeFiles)
      {
      size = psffsi->sfi_sizel;
      pos  = psffsi->sfi_positionl;
      }
#else
   AssignUL(&size, psffsi->sfi_size);
   iAssignL(&pos, psffsi->sfi_position);

   if (f32Parms.fLargeFiles)
      {
      Assign(&size, *(PULONGLONG)&psffsi->sfi_sizel);
      iAssign(&pos, psffsi->sfi_positionl);
      }
#endif

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

   pbCluster = malloc((size_t)pVolInfo->ulBlockSize);
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
#ifdef INCL_LONGLONG
            ulSector        = (ULONG)pos;
#else
            ulSector        = pos.ulLo;
#endif
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

#ifdef INCL_LONGLONG
            ulSector        = (ULONG)(pos / (ULONG)usBytesPerSector);
            usSectorOffset  = (USHORT)(pos % (ULONG)usBytesPerSector);
#else
            {
            LONGLONG llRes;

            llRes = iDivUL(pos, (ULONG)usBytesPerSector);
            ulSector = llRes.ulLo;
            llRes = iModUL(pos, (ULONG)usBytesPerSector);
            usSectorOffset = (USHORT)llRes.ulLo;
            }
#endif
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
#ifdef INCL_LONGLONG
            pos                     += (USHORT)ulCurBytesToWrite;
#else
            pos                     = iAddUS(pos, (USHORT)ulCurBytesToWrite);
#endif
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
#ifdef INCL_LONGLONG
            pos                     += usBytesToWrite - usRemaining;
#else
            pos                     = iAddUS(pos, usBytesToWrite - usRemaining);
#endif
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
#ifdef INCL_LONGLONG
            pos                     += usRemaining;
#else
            pos                     = iAddUS(pos, usRemaining);
#endif
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
#ifdef INCL_LONGLONG
        if ( (! f32Parms.fLargeFiles && (pos + usBytesToWrite >= (LONGLONG)LONG_MAX   ||
                                        size + usBytesToWrite >= (LONGLONG)LONG_MAX)) ||
             (f32Parms.fLargeFiles   && (pos + usBytesToWrite >= (LONGLONG)ULONG_MAX  ||
                                        size + usBytesToWrite >= (LONGLONG)ULONG_MAX) &&
#else
        LONGLONG llPos, llSize;

        llPos  = iAddUS(pos, usBytesToWrite);
        llSize = iAddUS(*(PLONGLONG)&size, usBytesToWrite);

        if ( (! f32Parms.fLargeFiles && (iGreaterEUL(llPos, LONG_MAX)   ||
                                        iGreaterEUL(llSize, LONG_MAX))) ||
             (f32Parms.fLargeFiles   && (iGreaterEUL(llPos, ULONG_MAX)  ||
                                        iGreaterEUL(llSize, ULONG_MAX)) &&
#endif
#ifdef EXFAT
             (pVolInfo->bFatType < FAT_TYPE_EXFAT)) )
#else
             TRUE) )
#endif
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

#ifdef INCL_LONGLONG
        if (! f32Parms.fLargeFiles && (LONGLONG)LONG_MAX - pos < (LONGLONG)usBytesToWrite)
           usBytesToWrite = (USHORT)((LONGLONG)LONG_MAX - pos);

#ifdef EXFAT
        if (f32Parms.fLargeFiles && (pVolInfo->bFatType < FAT_TYPE_EXFAT) &&
            (LONGLONG)ULONG_MAX - pos < (LONGLONG)usBytesToWrite)
#else
        if (f32Parms.fLargeFiles &&
            (LONGLONG)ULONG_MAX - pos < (LONGLONG)usBytesToWrite)
#endif
           usBytesToWrite = (USHORT)((LONGLONG)ULONG_MAX - pos);
#else
        iAssignUL(&llPos, LONG_MAX);
        llPos = iSub(llPos, pos);

        if (! f32Parms.fLargeFiles && iLessUS(llPos, usBytesToWrite))
           usBytesToWrite = (USHORT)llPos.ulLo;

        iAssignUL(&llPos, ULONG_MAX);
        llPos = iSub(llPos, pos);

#ifdef EXFAT
        if (f32Parms.fLargeFiles && (pVolInfo->bFatType < FAT_TYPE_EXFAT) &&
            iLessUL(llPos, usBytesToWrite))
#else
        if (f32Parms.fLargeFiles &&
            iLessUL(llPos, usBytesToWrite))
#endif
           usBytesToWrite = (USHORT)llPos.ulLo;
#endif

#ifdef INCL_LONGLONG
        if (pos + usBytesToWrite > size)
#else
        llPos = iAddUS(pos, usBytesToWrite);

        if (iGreater(llPos, *(PLONGLONG)&size))
#endif
        {
            ULONG ulLast = pVolInfo->ulFatEof;

#ifdef INCL_LONGLONG
            if (
                    pOpenInfo->ulCurCluster == pVolInfo->ulFatEof &&
                    pos == size &&
                    !(size % pVolInfo->ulBlockSize)
                )
#else
            ULONGLONG ullSize = ModUL(size, pVolInfo->ulBlockSize);

            if (
                    pOpenInfo->ulCurCluster == pVolInfo->ulFatEof &&
                    iEq(pos, *(PLONGLONG)&size) &&
                    EqUL(ullSize, 0)
                )
#endif
                ulLast = pOpenInfo->pSHInfo->ulLastCluster;

#ifdef INCL_LONGLONG
            rc = NewSize(pVolInfo, psffsi, psffsd,
               pos + usBytesToWrite, usIOFlag);
#else
            rc = NewSize(pVolInfo, psffsi, psffsd,
               AddUS(*(PULONGLONG)&pos, usBytesToWrite), usIOFlag);
#endif

            if (rc)
                goto FS_WRITEEXIT;

#ifdef INCL_LONGLONG
            size = psffsi->sfi_size;

            if (f32Parms.fLargeFiles)
               size = psffsi->sfi_sizel;
#else
            AssignUL(&size, psffsi->sfi_size);

            if (f32Parms.fLargeFiles)
               Assign(&size, *(PULONGLONG)&psffsi->sfi_sizel);
#endif

            if (ulLast != pVolInfo->ulFatEof)
            {
                pOpenInfo->ulCurCluster = GetNextCluster(pVolInfo, pOpenInfo->pSHInfo, ulLast);
                if (!pOpenInfo->ulCurCluster)
                    pOpenInfo->ulCurCluster = pVolInfo->ulFatEof;

                if (pOpenInfo->ulCurCluster == pVolInfo->ulFatEof)
                {
                    Message("FS_WRITE (INIT) No next cluster available!");
                    CritMessage("FAT32: FS_WRITE (INIT) No next cluster available!");
                }
            }
        }

        if (pOpenInfo->ulCurCluster == pVolInfo->ulFatEof)
           {
           pOpenInfo->ulCurCluster = PositionToOffset(pVolInfo, pOpenInfo, pos);
#ifdef INCL_LONGLONG
           pOpenInfo->ulCurBlock = pOpenInfo->ulCurBlock = (pos / pVolInfo->ulBlockSize) % (pVolInfo->ulClusterSize / pVolInfo->ulBlockSize);
#else
           {
           LONGLONG llRes;

           llRes = iDivUL(pos, pVolInfo->ulBlockSize);
           llRes = iModUL(llRes, pVolInfo->ulClusterSize / pVolInfo->ulBlockSize);
           pOpenInfo->ulCurBlock = pOpenInfo->ulCurBlock = llRes.ulLo;
           }
#endif
           }

        if (pOpenInfo->ulCurCluster == pVolInfo->ulFatEof)
        {
            Message("FS_WRITE (INIT2) No next cluster available!");
            CritMessage("FAT32: FS_WRITE (INIT2) No next cluster available!");
            rc = ERROR_INVALID_HANDLE;
            goto FS_WRITEEXIT;
        }

        /*
            First, handle the first part that does not align on a cluster border
        */
        ulBytesPerCluster = pVolInfo->ulClusterSize;
        ulBytesPerBlock   = pVolInfo->ulBlockSize;
#ifdef INCL_LONGLONG
        ulClusterOffset   = (ULONG)(pos % ulBytesPerCluster); /* get remainder */
        usBlockOffset     = (USHORT)(pos % ulBytesPerBlock);   /* get remainder */
#else
        {
        LONGLONG llRes;

        llRes = iModUL(pos, ulBytesPerCluster);
        ulClusterOffset   = (ULONG)llRes.ulLo;
        llRes = iModUL(pos, ulBytesPerBlock);
        usBlockOffset     = (USHORT)llRes.ulLo; 
        }
#endif
        if
            (
                (pOpenInfo->ulCurCluster != pVolInfo->ulFatEof) &&
#ifdef INCL_LONGLONG
                (pos < (LONGLONG)size) &&
#else
                (iLess(pos, *(PLONGLONG)&size)) &&
#endif
                (usBytesToWrite) &&
                (usBlockOffset)
            )
        {
            ULONG   ulCurrBytesToWrite;
            USHORT  usSectorsToWrite;
            USHORT  usSectorsPerCluster;
            USHORT  usSectorsPerBlock;

            usSectorsPerCluster = (USHORT)pVolInfo->SectorsPerCluster;
            usSectorsPerBlock   = (USHORT)(ulBytesPerBlock / pVolInfo->BootSect.bpb.BytesPerSector);
            usSectorsToWrite    = usSectorsPerBlock;

            /* compute the number of bytes
                to the cluster end or
                as much as fits into the user buffer
                or how much remains to be read until file end
               whatever is the smallest
            */
            ulCurrBytesToWrite  = (ulBytesPerBlock - (ULONG)usBlockOffset);
            ulCurrBytesToWrite  = min(ulCurrBytesToWrite,(ULONG)usBytesToWrite);
#ifdef INCL_LONGLONG
            ulCurrBytesToWrite  = (ULONG)min((ULONG)ulCurrBytesToWrite, size - pos);
#else
            {
            ULONGLONG ullRes;
            ullRes = Sub(size, *(PULONGLONG)&pos);
            ulCurrBytesToWrite  = (ULONG)min((ULONG)ulCurrBytesToWrite, ullRes.ulLo);
            }
#endif
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
#ifdef INCL_LONGLONG
                pos                     += (USHORT)ulCurrBytesToWrite;
#else
                pos                     =  iAddUS(pos, (USHORT)ulCurrBytesToWrite);
#endif
                usBytesWritten          += (USHORT)ulCurrBytesToWrite;
                usBytesToWrite          -= (USHORT)ulCurrBytesToWrite;
            }

            if (ulClusterOffset + ulCurrBytesToWrite >= ulBytesPerCluster)
            {
                pOpenInfo->ulCurCluster     = GetNextCluster(pVolInfo, pOpenInfo->pSHInfo, pOpenInfo->ulCurCluster);
                if (!pOpenInfo->ulCurCluster)
                {
                    pOpenInfo->ulCurCluster = pVolInfo->ulFatEof;
                }
            }
#ifdef INCL_LONGLONG
            pOpenInfo->ulCurBlock = (pos / pVolInfo->ulBlockSize) % (pVolInfo->ulClusterSize / pVolInfo->ulBlockSize);
#else
            {
            LONGLONG llRes;

            llRes = iDivUL(pos, pVolInfo->ulBlockSize);
            llRes = iModUL(llRes, pVolInfo->ulClusterSize / pVolInfo->ulBlockSize);
            pOpenInfo->ulCurBlock = llRes.ulLo;
            }
#endif
        }

        /*
            Second, handle the part that aligns on a cluster border and is a multiple of the cluster size
        */
        if
            (
                (pOpenInfo->ulCurCluster != pVolInfo->ulFatEof) &&
#ifdef INCL_LONGLONG
                (pos < (LONGLONG)size) &&
#else
                (iLess(pos, *(PLONGLONG)&size)) &&
#endif
                (usBytesToWrite)
            )
        {
            ULONG   ulCurrCluster       = pOpenInfo->ulCurCluster;
            ULONG   ulNextCluster       = ulCurrCluster;
            ULONG   ulCurrBytesToWrite  = 0;
            USHORT  usSectorsPerCluster = (USHORT)pVolInfo->SectorsPerCluster;
            USHORT  usSectorsPerBlock   = (USHORT)(ulBytesPerBlock / pVolInfo->BootSect.bpb.BytesPerSector);
            USHORT  usClustersToProcess = 0;
            USHORT  usBlocksToProcess   = 0;
            USHORT  usAdjacentClusters  = 1;
            USHORT  usAdjacentBlocks;
            USHORT  usBlocks;

#ifdef INCL_LONGLONG
            ulCurrBytesToWrite  = (ULONG)min((ULONG)usBytesToWrite, size - pos);
#else
            {
            ULONGLONG ullRes = Sub(size, *(PULONGLONG)&pos);
            ulCurrBytesToWrite  = (ULONG)min((ULONG)usBytesToWrite, ullRes.ulLo);
            }
#endif

            usClustersToProcess         = (USHORT)(ulCurrBytesToWrite / ulBytesPerCluster); /* get the number of full clusters */
            usBlocksToProcess           = (USHORT)(ulCurrBytesToWrite / ulBytesPerBlock);   /* get the number of full blocks   */
            usAdjacentBlocks            = (USHORT)(usBlocksToProcess % (pVolInfo->ulClusterSize / pVolInfo->ulBlockSize));
            usBlocks = usBlocksToProcess;

            if (ulCurrBytesToWrite < pVolInfo->ulClusterSize)
               usAdjacentBlocks = (USHORT)(ulCurrBytesToWrite / pVolInfo->ulBlockSize);

            while (usBlocks && (ulCurrCluster != pVolInfo->ulFatEof))
            {
                ulNextCluster       = GetNextCluster(pVolInfo, pOpenInfo->pSHInfo, ulCurrCluster);
                if (!ulNextCluster)
                {
                    ulNextCluster = pVolInfo->ulFatEof;
                }

                if  (
                        (ulNextCluster != pVolInfo->ulFatEof) &&
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
                             ulCurrCluster           = GetNextCluster(pVolInfo, pOpenInfo->pSHInfo, ulCurrCluster);
                             pOpenInfo->ulCurCluster = ulCurrCluster;
                             }

                           usBlocks -= 1;

                           pBufPosition                += ulBytesPerBlock;
                           ulClusterSector             += usSectorsPerBlock;
                           usAdjacentBlocks--;
                        }
                    }

#ifdef INCL_LONGLONG
                    pos                             += (USHORT)ulCurrBytesToWrite;
#else
                    pos                             =  iAddUS(pos, (USHORT)ulCurrBytesToWrite);
#endif
                    usBytesWritten                  += (USHORT)ulCurrBytesToWrite;
                    usBytesToWrite                  -= (USHORT)ulCurrBytesToWrite;
                    usAdjacentBlocks                =  (USHORT)(pVolInfo->ulClusterSize / pVolInfo->ulBlockSize);
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
                (pOpenInfo->ulCurCluster != pVolInfo->ulFatEof) &&
#ifdef INCL_LONGLONG
                (pos < (LONGLONG)size) &&
#else
                (iLess(pos, *(PLONGLONG)&size)) &&
#endif
                (usBytesToWrite)
            )
        {
            ULONG  ulCurrBytesToWrite;
            USHORT usSectorsToWrite;
            USHORT usSectorsPerCluster;
            USHORT usSectorsPerBlock;

            usSectorsToWrite = (USHORT)(ulBytesPerBlock / pVolInfo->BootSect.bpb.BytesPerSector);
            usSectorsPerCluster = (USHORT)pVolInfo->SectorsPerCluster;
            usSectorsPerBlock = usSectorsToWrite;

#ifdef INCL_LONGLONG
            ulCurrBytesToWrite  = (ULONG)min((ULONG)usBytesToWrite, size - pos);
#else
            {
            ULONGLONG ullRes = Sub(size, *(PULONGLONG)&pos);
            ulCurrBytesToWrite  = (ULONG)min((ULONG)usBytesToWrite, ullRes.ulLo);
            }
#endif
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

#ifdef INCL_LONGLONG
                pos                     += (USHORT)ulCurrBytesToWrite;
#else
                pos                     =  iAddUS(pos, (USHORT)ulCurrBytesToWrite);
#endif
                usBytesWritten          += (USHORT)ulCurrBytesToWrite;
                usBytesToWrite          -= (USHORT)ulCurrBytesToWrite;
            }
        }

#ifdef INCL_LONGLONG
        pOpenInfo->ulCurBlock = (pos / pVolInfo->ulBlockSize) % (pVolInfo->ulClusterSize / pVolInfo->ulBlockSize);
#else
        {
        LONGLONG llRes;

        llRes = iDivUL(pos, pVolInfo->ulBlockSize);
        llRes = iModUL(llRes, pVolInfo->ulClusterSize / pVolInfo->ulBlockSize);
        pOpenInfo->ulCurBlock = llRes.ulLo;
        }
#endif

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
#ifdef INCL_LONGLONG
   psffsi->sfi_size = (ULONG)size;
   psffsi->sfi_position = (LONG)pos;

   if (f32Parms.fLargeFiles)
      {
      psffsi->sfi_sizel = size;
      psffsi->sfi_positionl = pos;
      }
#else
   psffsi->sfi_size = size.ulLo;
   psffsi->sfi_position = pos.ulLo;

   if (f32Parms.fLargeFiles)
      {
      iAssign(&psffsi->sfi_sizel, *(PLONGLONG)&size);
      iAssign(&psffsi->sfi_positionl, pos);
      }
#endif

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
ULONG PositionToOffset(PVOLINFO pVolInfo, POPENINFO pOpenInfo, LONGLONG llOffset)
{
ULONG ulCurCluster;

   ulCurCluster = pOpenInfo->pSHInfo->ulStartCluster;
   if (!ulCurCluster)
      return pVolInfo->ulFatEof;

#ifdef INCL_LONGLONG
   if (llOffset < pVolInfo->ulClusterSize)
#else
   if (iLessUL(llOffset, pVolInfo->ulClusterSize))
#endif
      return ulCurCluster;

   return SeekToCluster(pVolInfo, pOpenInfo->pSHInfo, ulCurCluster, llOffset);
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
    LONGLONG llOffset,              /* offset   */
    unsigned short usType,          /* type     */
    unsigned short IOFlag           /* IOflag   */
)
{
PVOLINFO pVolInfo;
POPENINFO pOpenInfo;
#ifdef INCL_LONGLONG
LONGLONG  llNewOffset = 0;
#else
LONGLONG  llNewOffset = {0};
#endif
LONGLONG  pos;
ULONGLONG size;
USHORT rc;

   _asm push es;

   pOpenInfo = GetOpenInfo(psffsd);

#ifdef INCL_LONGLONG
   size = (ULONGLONG)psffsi->sfi_size;
   pos  = (LONGLONG)psffsi->sfi_position;

   if (f32Parms.fLargeFiles)
      {
      size = psffsi->sfi_sizel;
      pos  = psffsi->sfi_positionl;
      }
#else
   AssignUL(&size, psffsi->sfi_size);
   iAssignL(&pos, psffsi->sfi_position);

   if (f32Parms.fLargeFiles)
      {
      Assign(&size, *(PULONGLONG)&psffsi->sfi_sizel);
      iAssign(&pos, psffsi->sfi_positionl);
      }
#endif

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
#ifdef INCL_LONGLONG
         if (llOffset < 0)
#else
         if (iLessL(llOffset, 0))
#endif
            {
            rc = ERROR_NEGATIVE_SEEK;
            goto FS_CHGFILEPTRLEXIT;
            }
#ifdef INCL_LONGLONG
         llNewOffset = llOffset;
#else
         iAssign(&llNewOffset, llOffset);
#endif
         break;
      case CFP_RELCUR  :
#ifdef INCL_LONGLONG
         llNewOffset = pos + llOffset;
#else
         llNewOffset = iAdd(pos, llOffset);
#endif
         break;
      case CFP_RELEND  :
#ifdef INCL_LONGLONG
         llNewOffset = size + llOffset;
#else
         llNewOffset = iAdd(*(PLONGLONG)&size, llOffset);
#endif
         break;
      }
#ifdef INCL_LONGLONG
   if (!IsDosSession() && llNewOffset < 0)
#else
   if (!IsDosSession() && iLessL(llNewOffset, 0))
#endif
      {
      rc = ERROR_NEGATIVE_SEEK;
      goto FS_CHGFILEPTRLEXIT;
      }

#ifdef INCL_LONGLONG
   if (pos != (ULONG)llNewOffset)
      {
      pos = (ULONG)llNewOffset;
      pOpenInfo->ulCurCluster = pVolInfo->ulFatEof;
      }
#else
   if (iNeqUL(pos, *(PULONG)&llNewOffset))
      {
      iAssignUL(&pos, *(PULONG)&llNewOffset);
      pOpenInfo->ulCurCluster = pVolInfo->ulFatEof;
      }
#endif
   rc = 0;

FS_CHGFILEPTRLEXIT:
#ifdef INCL_LONGLONG
   psffsi->sfi_size = (ULONG)size;
   psffsi->sfi_position = (LONG)pos;

   if (f32Parms.fLargeFiles)
      {
      psffsi->sfi_sizel = size;
      psffsi->sfi_positionl = pos;
      }
#else
   psffsi->sfi_size = size.ulLo;
   psffsi->sfi_position = pos.ulLo;

   if (f32Parms.fLargeFiles)
      {
      iAssign(&psffsi->sfi_sizel, *(PLONGLONG)&size);
      iAssign(&psffsi->sfi_positionl, pos);
      }
#endif

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
   LONGLONG llOffset;

#ifdef INCL_LONGLONG
   llOffset = (LONGLONG)lOffset;

   pos = (LONGLONG)psffsi->sfi_position;

   if (f32Parms.fLargeFiles)
      pos = psffsi->sfi_positionl;
#else
   iAssignL(&llOffset, lOffset);

   iAssignL(&pos, psffsi->sfi_position);

   if (f32Parms.fLargeFiles)
      iAssign(&pos, psffsi->sfi_positionl);
#endif

   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_CHGFILEPTR, Mode %d - offset %ld, current offset=%lld",
      usType, lOffset, pos);

   rc = FS_CHGFILEPTRL(psffsi, psffsd,
                       llOffset, usType,
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
ULONGLONG size;
USHORT rc;

   _asm push es;

   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_COMMIT, type %d", usType);

#ifdef INCL_LONGLONG
   size = (ULONGLONG)psffsi->sfi_size;

   if (f32Parms.fLargeFiles)
      {
      size = psffsi->sfi_sizel;
      }
#else
   AssignUL(&size, psffsi->sfi_size);

   if (f32Parms.fLargeFiles)
      {
      Assign(&size, *(PULONGLONG)&psffsi->sfi_sizel);
      }
#endif

   switch (usType)
      {
      case FS_COMMIT_ONE:
      case FS_COMMIT_ALL:
         {
         PVOLINFO pVolInfo;
         POPENINFO pOpenInfo;
         PSZ  pszFile;
         PDIRENTRY pDirEntry;
         PDIRENTRY pDirOld;
         PDIRENTRY1 pDirEntryStream = NULL;
         PDIRENTRY1 pDirOldStream = NULL;
#ifdef EXFAT
         PDIRENTRY1 pDirEntry1;
#endif
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

         pDirEntry = (PDIRENTRY)malloc((size_t)sizeof(DIRENTRY));
         if (!pDirEntry)
            {
            rc = ERROR_NOT_ENOUGH_MEMORY;
            goto FS_COMMITEXIT;
            }
         pDirOld = (PDIRENTRY)malloc((size_t)sizeof(DIRENTRY));
         if (!pDirOld)
            {
            rc = ERROR_NOT_ENOUGH_MEMORY;
            goto FS_COMMITEXIT;
            }
#ifdef EXFAT
         pDirEntryStream = (PDIRENTRY1)malloc((size_t)sizeof(DIRENTRY1));
         if (!pDirEntryStream)
            {
            rc = ERROR_NOT_ENOUGH_MEMORY;
            goto FS_COMMITEXIT;
            }
         pDirOldStream = (PDIRENTRY1)malloc((size_t)sizeof(DIRENTRY1));
         if (!pDirOldStream)
            {
            rc = ERROR_NOT_ENOUGH_MEMORY;
            goto FS_COMMITEXIT;
            }

         pDirEntry1 = (PDIRENTRY1)pDirEntry;
#endif

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

         ulCluster = FindPathCluster(pVolInfo, pOpenInfo->pSHInfo->ulDirCluster, pszFile, pOpenInfo->pDirSHInfo,
            pDirOld, pDirOldStream, NULL);
         if (ulCluster == pVolInfo->ulFatEof)
            {
            rc = ERROR_FILE_NOT_FOUND;
            goto FS_COMMITEXIT;
            }

         memcpy(pDirEntry, pDirOld, sizeof (DIRENTRY));
#ifdef EXFAT
         memcpy(pDirEntryStream, pDirOldStream, sizeof (DIRENTRY));

         if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
            {
#endif
            memcpy(&pDirEntry->wCreateTime,    &psffsi->sfi_ctime, sizeof (USHORT));
            memcpy(&pDirEntry->wCreateDate,    &psffsi->sfi_cdate, sizeof (USHORT));
            memcpy(&pDirEntry->wAccessDate,    &psffsi->sfi_adate, sizeof (USHORT));
            memcpy(&pDirEntry->wLastWriteTime, &psffsi->sfi_mtime, sizeof (USHORT));
            memcpy(&pDirEntry->wLastWriteDate, &psffsi->sfi_mdate, sizeof (USHORT));

#ifdef INCL_LONGLONG
            pDirEntry->ulFileSize  = (ULONG)size;
#else
            pDirEntry->ulFileSize  = size.ulLo;
#endif
#ifdef EXFAT
            }
         else
            {
            TIMESTAMP ts = SetTimeStamp(*(FDATE *)&psffsi->sfi_cdate, *(FTIME *)&psffsi->sfi_ctime);

            memcpy(&pDirEntry1->u.File.ulCreateTimestp, &ts, sizeof(TIMESTAMP));
            ts = SetTimeStamp(*(FDATE *)&psffsi->sfi_adate, *(FTIME *)&psffsi->sfi_atime);
            memcpy(&pDirEntry1->u.File.ulLastAccessedTimestp, &ts, sizeof(TIMESTAMP));
            ts = SetTimeStamp(*(FDATE *)&psffsi->sfi_mdate, *(FTIME *)&psffsi->sfi_mtime);
            memcpy(&pDirEntry1->u.File.ulLastModifiedTimestp, &ts, sizeof(TIMESTAMP));

#ifdef INCL_LONGLONG
            pDirEntryStream->u.Stream.ullValidDataLen = size;
            pDirEntryStream->u.Stream.ullDataLen =
               (size / pVolInfo->ulClusterSize) * pVolInfo->ulClusterSize +
               (size % pVolInfo->ulClusterSize ? pVolInfo->ulClusterSize : 0);
#else
            {
            ULONGLONG ullRest;

            Assign(pDirEntryStream->u.Stream.ullValidDataLen, size);
            pDirEntryStream->u.Stream.ullDataLen = DivUL(size, pVolInfo->ulClusterSize);
            pDirEntryStream->u.Stream.ullDataLen = MulUL(pDirEntryStream->u.Stream.ullDataLen, pVolInfo->ulClusterSize);
            ullRest = ModUL(size, pVolInfo->ulClusterSize);

            if (NeqUL(ullRest, 0))
               AssignUL(&ullRest, pVolInfo->ulClusterSize);
            else
               AssignUL(&ullRest, 0);

            pDirEntryStream->u.Stream.ullDataLen = Add(pDirEntryStream->u.Stream.ullDataLen, ullRest);
            }
#endif
            pDirEntryStream->u.Stream.bAllocPossible = 1;
            pDirEntryStream->u.Stream.bNoFatChain = (BYTE)(pOpenInfo->pSHInfo->fNoFatChain & 1);
            }
#endif

         if (pOpenInfo->fCommitAttr || psffsi->sfi_DOSattr != pOpenInfo->pSHInfo->bAttr)
            {
#ifdef EXFAT
            if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
#endif
               pDirEntry->bAttr = pOpenInfo->pSHInfo->bAttr = psffsi->sfi_DOSattr;
#ifdef EXFAT
            else
               pDirEntry1->u.File.usFileAttr = pOpenInfo->pSHInfo->bAttr = psffsi->sfi_DOSattr;
#endif
            pOpenInfo->fCommitAttr = FALSE;
            }

         if (pOpenInfo->pSHInfo->ulStartCluster)
            {
#ifdef EXFAT
            if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
               {
#endif
               pDirEntry->wCluster     = LOUSHORT(pOpenInfo->pSHInfo->ulStartCluster);
               pDirEntry->wClusterHigh = HIUSHORT(pOpenInfo->pSHInfo->ulStartCluster);
#ifdef EXFAT
               }
            else
               {
               pDirEntryStream->u.Stream.ulFirstClus = pOpenInfo->pSHInfo->ulStartCluster;
               }
#endif
            }
         else
            {
#ifdef EXFAT
            if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
               {
#endif
               pDirEntry->wCluster = 0;
               pDirEntry->wClusterHigh = 0;
#ifdef EXFAT
               }
            else
               {
               pDirEntryStream->u.Stream.ulFirstClus = 0;
               }
#endif
            }

         rc = ModifyDirectory(pVolInfo, pOpenInfo->pSHInfo->ulDirCluster, pOpenInfo->pDirSHInfo, MODIFY_DIR_UPDATE,
            pDirOld, pDirEntry, pDirOldStream, pDirEntryStream, NULL, usIOFlag);
         if (!rc)
            psffsi->sfi_tstamp = 0;
         if (pDirEntry)
            free(pDirEntry);
         if (pDirOld)
            free(pDirOld);
#ifdef EXFAT
         if (pDirEntryStream)
            free(pDirEntryStream);
         if (pDirOldStream)
            free(pDirOldStream);
#endif
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
    ULONGLONG ullLen,      /* len      */
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
   ULONGLONG ullLen;
   APIRET rc;
#ifdef INCL_LONGLONG
   ullLen = (ULONGLONG)ulLen;
#else
   AssignUL(&ullLen, ulLen);
#endif

   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_NEWSIZEL newsize = %lu", ulLen);

   rc = FS_NEWSIZEL(psffsi, psffsd,
                    ullLen, usIOFlag);

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

#ifdef INCL_LONGLONG
   size = (ULONGLONG)psffsi->sfi_size;

   if (f32Parms.fLargeFiles)
      size = psffsi->sfi_sizel;

   if (ullLen == size)
      return 0;

   if (!ullLen)
#else
   ULONGLONG ullClusters, ullRest;

   AssignUL(&size, psffsi->sfi_size);

   if (f32Parms.fLargeFiles)
      Assign(&size, *(PULONGLONG)&psffsi->sfi_sizel);

   if (Eq(ullLen, size))
      return 0;

   if (EqUL(ullLen, 0))
#endif
      {
      if (pOpenInfo->pSHInfo->ulStartCluster)
         {
         DeleteFatChain(pVolInfo, pOpenInfo->pSHInfo->ulStartCluster);
         pOpenInfo->pSHInfo->ulStartCluster = 0L;
         pOpenInfo->pSHInfo->ulLastCluster = pVolInfo->ulFatEof;
         }

      ResetAllCurrents(pVolInfo, pOpenInfo);

#ifdef INCL_LONGLONG
      psffsi->sfi_size = (ULONG)ullLen;

      if (f32Parms.fLargeFiles)
         psffsi->sfi_sizel = ullLen;
#else
      psffsi->sfi_size = ullLen.ulLo;

      if (f32Parms.fLargeFiles)
         iAssign(&psffsi->sfi_sizel, *(PLONGLONG)&ullLen);
#endif

      if (usIOFlag & DVIO_OPWRTHRU)
         return FS_COMMIT(FS_COMMIT_ONE, usIOFlag, psffsi, psffsd);

      return 0;
      }

   /*
      Calculate number of needed clusters
   */
#ifdef INCL_LONGLONG
   ulClustersNeeded = ullLen / pVolInfo->ulClusterSize;
   if (ullLen % pVolInfo->ulClusterSize)
      ulClustersNeeded ++;
#else
   ullClusters = DivUL(ullLen, pVolInfo->ulClusterSize);
   ulClustersNeeded = ullClusters.ulLo;
   ullRest = ModUL(ullLen, pVolInfo->ulClusterSize);

   if (NeqUL(ullRest, 0))
      ulClustersNeeded ++;
#endif

   /*
      if file didn't have any clusters
   */

   if (!pOpenInfo->pSHInfo->ulStartCluster)
      {
      ulCluster = MakeFatChain(pVolInfo, pOpenInfo->pSHInfo, pVolInfo->ulFatEof, ulClustersNeeded, &pOpenInfo->pSHInfo->ulLastCluster);
      if (ulCluster == pVolInfo->ulFatEof)
         return ERROR_DISK_FULL;
      pOpenInfo->pSHInfo->ulStartCluster = ulCluster;
      }

   /*
      If newsize < current size
   */

#ifdef INCL_LONGLONG
   else if (ullLen < size)
      {
      if (!(ullLen % pVolInfo->ulClusterSize))
         ulCluster = PositionToOffset(pVolInfo, pOpenInfo, ullLen - 1);
      else
         ulCluster = PositionToOffset(pVolInfo, pOpenInfo, ullLen);
#else
   else if (Less(ullLen, size))
      {
      ULONGLONG ullRest;
      LONGLONG llLen2;

      ullRest = ModUL(ullLen, pVolInfo->ulClusterSize);
      iAssign(&llLen2, *(PLONGLONG)&ullLen);

      if (EqUL(ullRest, 0))
         llLen2 = iSubL(llLen2, 1);

      ulCluster = PositionToOffset(pVolInfo, pOpenInfo, llLen2);
#endif

      if (ulCluster == pVolInfo->ulFatEof)
         return ERROR_SECTOR_NOT_FOUND;

      ulNextCluster = GetNextCluster(pVolInfo, pOpenInfo->pSHInfo, ulCluster);
      if (ulNextCluster != pVolInfo->ulFatEof)
         {
         SetNextCluster( pVolInfo, ulCluster, pVolInfo->ulFatEof);
         DeleteFatChain(pVolInfo, ulNextCluster);
         }
      pOpenInfo->pSHInfo->ulLastCluster = ulCluster;

      ResetAllCurrents(pVolInfo, pOpenInfo);
      }
   else
      {
      /*
         If newsize > current size
      */

      ulCluster = pOpenInfo->pSHInfo->ulLastCluster;
      if (ulCluster == pVolInfo->ulFatEof)
         {
         CritMessage("FAT32: Lastcluster empty in NewSize!");
         Message("FAT32: Lastcluster empty in NewSize!");
         return ERROR_SECTOR_NOT_FOUND;
         }

#ifdef INCL_LONGLONG
      ulClusterCount = size / pVolInfo->ulClusterSize;
      if (size % pVolInfo->ulClusterSize)
         ulClusterCount ++;
#else
      ullClusters = DivUL(size, pVolInfo->ulClusterSize);
      ulClusterCount = ullClusters.ulLo;
      ullRest = ModUL(size, pVolInfo->ulClusterSize);

      if (NeqUL(ullRest, 0))
         ulClusterCount ++;
#endif

      if (ulClustersNeeded > ulClusterCount)
         {
         ulNextCluster = MakeFatChain(pVolInfo, pOpenInfo->pSHInfo, ulCluster, ulClustersNeeded - ulClusterCount, &pOpenInfo->pSHInfo->ulLastCluster);
         if (ulNextCluster == pVolInfo->ulFatEof)
            return ERROR_DISK_FULL;
         }
      }

#ifdef INCL_LONGLONG
   psffsi->sfi_size = (ULONG)ullLen;

   if (f32Parms.fLargeFiles)
      psffsi->sfi_sizel = ullLen;
#else
   psffsi->sfi_size = ullLen.ulLo;

   if (f32Parms.fLargeFiles)
      iAssign(&psffsi->sfi_sizel, *(PLONGLONG)&ullLen);
#endif

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

#ifdef INCL_LONGLONG
   size = (ULONGLONG)psffsi->sfi_size;

   if (f32Parms.fLargeFiles)
      size = psffsi->sfi_sizel;
#else
   AssignUL(&size, psffsi->sfi_size);

   if (f32Parms.fLargeFiles)
      Assign(&size, *(PULONGLONG)&psffsi->sfi_sizel);
#endif

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
         case FIL_QUERYEASFROMLISTL:
         case FIL_QUERYALLEAS:
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
         PDIRENTRY pDirEntry;
         PDIRENTRY1 pDirEntry1;
         ULONG ulCluster;

         pDirEntry = (PDIRENTRY)malloc((size_t)sizeof(DIRENTRY));
         if (!pDirEntry)
            {
            rc = ERROR_NOT_ENOUGH_MEMORY;
            goto FS_FILEINFOEXIT;
            }

         pDirEntry1 = (PDIRENTRY1)pDirEntry;

         ulCluster = FindPathCluster(pVolInfo, pOpenInfo->pSHInfo->ulDirCluster,
            pszFile, pOpenInfo->pDirSHInfo, pDirEntry, NULL, NULL);

         if (ulCluster == pVolInfo->ulFatEof)
            {
            rc = ERROR_FILE_NOT_FOUND;
            goto FS_FILEINFOEXIT;
            }

#ifdef EXFAT
         if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
            {
#endif
            memcpy(&psffsi->sfi_ctime, &pDirEntry->wCreateTime, sizeof (USHORT));
            memcpy(&psffsi->sfi_cdate, &pDirEntry->wCreateDate, sizeof (USHORT));
            psffsi->sfi_atime = 0;
            memcpy(&psffsi->sfi_adate, &pDirEntry->wAccessDate, sizeof (USHORT));
            memcpy(&psffsi->sfi_mtime, &pDirEntry->wLastWriteTime, sizeof (USHORT));
            memcpy(&psffsi->sfi_mdate, &pDirEntry->wLastWriteDate, sizeof (USHORT));
            psffsi->sfi_DOSattr = pDirEntry->bAttr = pOpenInfo->pSHInfo->bAttr;
#ifdef EXFAT
            }
         else
            {
            FDATE date = GetDate1(pDirEntry1->u.File.ulCreateTimestp);
            FTIME time = GetTime1(pDirEntry1->u.File.ulCreateTimestp);
            memcpy(&psffsi->sfi_ctime, &time, sizeof (USHORT));
            memcpy(&psffsi->sfi_cdate, &date, sizeof (USHORT));
            date = GetDate1(pDirEntry1->u.File.ulLastAccessedTimestp);
            time = GetTime1(pDirEntry1->u.File.ulLastAccessedTimestp);
            memcpy(&psffsi->sfi_atime, &time, sizeof (USHORT));
            memcpy(&psffsi->sfi_adate, &date, sizeof (USHORT));
            date = GetDate1(pDirEntry1->u.File.ulLastModifiedTimestp);
            time = GetTime1(pDirEntry1->u.File.ulLastModifiedTimestp);
            memcpy(&psffsi->sfi_mtime, &time, sizeof (USHORT));
            memcpy(&psffsi->sfi_mdate, &date, sizeof (USHORT));
            psffsi->sfi_DOSattr = pOpenInfo->pSHInfo->bAttr;
            pDirEntry1->u.File.usFileAttr = (USHORT)pOpenInfo->pSHInfo->bAttr;
            }
#endif
         if (pDirEntry)
            free(pDirEntry);
         }

      switch (usLevel)
         {
         case FIL_STANDARD         :
            {
            PFILESTATUS pfStatus = (PFILESTATUS)pData;
            memset(pfStatus, 0, sizeof (FILESTATUS));

            memcpy(&pfStatus->fdateCreation, &psffsi->sfi_cdate, sizeof (USHORT));
            memcpy(&pfStatus->ftimeCreation, &psffsi->sfi_ctime, sizeof (USHORT));
            memcpy(&pfStatus->ftimeLastAccess, &psffsi->sfi_atime, sizeof (USHORT));
            memcpy(&pfStatus->fdateLastAccess, &psffsi->sfi_adate, sizeof (USHORT));
            memcpy(&pfStatus->fdateLastWrite, &psffsi->sfi_mdate, sizeof (USHORT));
            memcpy(&pfStatus->ftimeLastWrite, &psffsi->sfi_mtime, sizeof (USHORT));
#ifdef INCL_LONGLONG
            pfStatus->cbFile = size;
#else
            pfStatus->cbFile = size.ulLo;
#endif
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
            memcpy(&pfStatus->ftimeLastAccess, &psffsi->sfi_atime, sizeof (USHORT));
            memcpy(&pfStatus->fdateLastAccess, &psffsi->sfi_adate, sizeof (USHORT));
            memcpy(&pfStatus->fdateLastWrite, &psffsi->sfi_mdate, sizeof (USHORT));
            memcpy(&pfStatus->ftimeLastWrite, &psffsi->sfi_mtime, sizeof (USHORT));
#ifdef INCL_LONGLONG
            pfStatus->cbFile = size;
            pfStatus->cbFileAlloc =
               (pfStatus->cbFile / pVolInfo->ulClusterSize) * pVolInfo->ulClusterSize +
               (pfStatus->cbFile % pVolInfo->ulClusterSize ? pVolInfo->ulClusterSize : 0);
#else
            {
            LONGLONG llRest;

            iAssign(&pfStatus->cbFile, *(PLONGLONG)&size);
            pfStatus->cbFileAlloc = iDivUL(pfStatus->cbFile, pVolInfo->ulClusterSize);
            pfStatus->cbFileAlloc = iMulUL(pfStatus->cbFileAlloc, pVolInfo->ulClusterSize);
            llRest = iModUL(pfStatus->cbFile, pVolInfo->ulClusterSize);

            if (iNeqUL(llRest, 0))
               iAssignUL(&llRest, pVolInfo->ulClusterSize);
            else
               iAssignUL(&llRest, 0);

            pfStatus->cbFileAlloc = iAdd(pfStatus->cbFileAlloc, llRest);
            }
#endif

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
            memcpy(&pfStatus->ftimeLastAccess, &psffsi->sfi_atime, sizeof (USHORT));
            memcpy(&pfStatus->fdateLastAccess, &psffsi->sfi_adate, sizeof (USHORT));
            memcpy(&pfStatus->fdateLastWrite, &psffsi->sfi_mdate, sizeof (USHORT));
            memcpy(&pfStatus->ftimeLastWrite, &psffsi->sfi_mtime, sizeof (USHORT));
#ifdef INCL_LONGLONG
            pfStatus->cbFile = size;
#else
            pfStatus->cbFile = size.ulLo;
#endif
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
               rc = usGetEASize(pVolInfo, pOpenInfo->pSHInfo->ulDirCluster, pOpenInfo->pDirSHInfo, pszFile, &pfStatus->cbList);
            break;
            }
         case FIL_QUERYEASIZEL     :
            {
            PFILESTATUS4L pfStatus = (PFILESTATUS4L)pData;
            memset(pfStatus, 0, sizeof (FILESTATUS4L));

            memcpy(&pfStatus->fdateCreation, &psffsi->sfi_cdate, sizeof (USHORT));
            memcpy(&pfStatus->ftimeCreation, &psffsi->sfi_ctime, sizeof (USHORT));
            memcpy(&pfStatus->ftimeLastAccess, &psffsi->sfi_atime, sizeof (USHORT));
            memcpy(&pfStatus->fdateLastAccess, &psffsi->sfi_adate, sizeof (USHORT));
            memcpy(&pfStatus->fdateLastWrite, &psffsi->sfi_mdate, sizeof (USHORT));
            memcpy(&pfStatus->ftimeLastWrite, &psffsi->sfi_mtime, sizeof (USHORT));
#ifdef INCL_LONGLONG
            pfStatus->cbFile = size;
            pfStatus->cbFileAlloc =
               (pfStatus->cbFile / pVolInfo->ulClusterSize) * pVolInfo->ulClusterSize +
               (pfStatus->cbFile % pVolInfo->ulClusterSize ? pVolInfo->ulClusterSize : 0);
#else
            {
            LONGLONG llRest;

            iAssign(&pfStatus->cbFile, *(PLONGLONG)&size);
            pfStatus->cbFileAlloc = iDivUL(pfStatus->cbFile, pVolInfo->ulClusterSize);
            pfStatus->cbFileAlloc = iMulUL(pfStatus->cbFileAlloc, pVolInfo->ulClusterSize);
            llRest = iModUL(pfStatus->cbFile, pVolInfo->ulClusterSize);

            if (iNeqUL(llRest, 0))
               iAssignUL(&llRest, pVolInfo->ulClusterSize);
            else
               iAssignUL(&llRest, 0);

            pfStatus->cbFileAlloc = iAdd(pfStatus->cbFileAlloc, llRest);
            }
#endif

            pfStatus->attrFile = psffsi->sfi_DOSattr = pOpenInfo->pSHInfo->bAttr;

            if (!f32Parms.fEAS)
               {
               pfStatus->cbList = sizeof pfStatus->cbList;
               rc = 0;
               }
            else
               rc = usGetEASize(pVolInfo, pOpenInfo->pSHInfo->ulDirCluster, pOpenInfo->pDirSHInfo, pszFile, &pfStatus->cbList);
            break;
            }
         case FIL_QUERYEASFROMLIST:
         case FIL_QUERYEASFROMLISTL:
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
               rc = usGetEAS(pVolInfo, usLevel, pOpenInfo->pSHInfo->ulDirCluster, pOpenInfo->pDirSHInfo, pszFile, pEA);
            break;
            }

         case FIL_QUERYALLEAS:
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
               rc = usGetEAS(pVolInfo, usLevel, pOpenInfo->pSHInfo->ulDirCluster, pOpenInfo->pDirSHInfo, pszFile, pEA);
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
               rc = usModifyEAS(pVolInfo, pOpenInfo->pSHInfo->ulDirCluster, pOpenInfo->pDirSHInfo,
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
