#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#define INCL_DOS
#define INCL_DOSDEVIOCTL
#define INCL_DOSDEVICES
#define INCL_DOSERRORS

#include "os2.h"
#include "portable.h"
#include "fat32ifs.h"

int far pascal _loadds FS_CHDIR(
    unsigned short usFlag,      /* flag     */
    struct cdfsi far * pcdfsi,      /* pcdfsi   */
    struct cdfsd far * pcdfsd,      /* pcdfsd   */
    char far * pDir,            /* pDir     */
    unsigned short usCurDirEnd      /* iCurDirEnd   */
)
{
PVOLINFO pVolInfo;
POPENINFO pOpenInfo = NULL;
ULONG ulCluster;
PSZ   pszFile;
USHORT rc;
//BYTE     szDirLongName[ FAT32MAXPATH ];
PSZ      szDirLongName;

   _asm push es;

   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_CHDIR, flag %u", usFlag);

   szDirLongName = (PSZ)malloc((size_t)FAT32MAXPATH);
   if (!szDirLongName)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto FS_CHDIREXIT;
      }

   switch (usFlag)
      {
      case CD_VERIFY   :
         pDir = pcdfsi->cdi_curdir;
         usCurDirEnd = 0xFFFF;

      case CD_EXPLICIT :
         if (f32Parms.fMessageActive & LOG_FS)
            Message("CHDIR to %s", pDir);
         if (strlen(pDir) > FAT32MAXPATH)
            {
            rc = ERROR_FILENAME_EXCED_RANGE;
            goto FS_CHDIREXIT;
            }

         pVolInfo = GetVolInfo(pcdfsi->cdi_hVPB);

         if (! pVolInfo)
            {
            rc = ERROR_INVALID_DRIVE;
            goto FS_CHDIREXIT;
            }

         if (pVolInfo->fFormatInProgress)
            {
            rc = ERROR_ACCESS_DENIED;
            goto FS_CHDIREXIT;
            }

         if (IsDriveLocked(pVolInfo))
            {
            rc = ERROR_DRIVE_LOCKED;
            goto FS_CHDIREXIT;
            }

         if( usFlag == CD_EXPLICIT )
         {
            pOpenInfo = malloc(sizeof (OPENINFO));
            if (!pOpenInfo)
            {
                rc = ERROR_NOT_ENOUGH_MEMORY;
                goto FS_CHDIREXIT;
            }
            memset(pOpenInfo, 0, sizeof (OPENINFO));

            if( TranslateName(pVolInfo, 0L, pDir, szDirLongName, TRANSLATE_SHORT_TO_LONG ))
               strcpy( szDirLongName, pDir );

            pOpenInfo->pSHInfo = GetSH( szDirLongName, pOpenInfo);
            if (!pOpenInfo->pSHInfo)
            {
                rc = ERROR_TOO_MANY_OPEN_FILES;
                goto FS_CHDIREXIT;
            }
            //pOpenInfo->pSHInfo->sOpenCount++; //
            if (pOpenInfo->pSHInfo->fLock)
            {
                rc = ERROR_ACCESS_DENIED;
                goto FS_CHDIREXIT;
            }
         }

         ulCluster = FindDirCluster(pVolInfo,
            pcdfsi,
            pcdfsd,
            pDir,
            usCurDirEnd,
            FILE_DIRECTORY,
            &pszFile,
            NULL);

         if (ulCluster == pVolInfo->ulFatEof || *pszFile)
            {
            rc = ERROR_PATH_NOT_FOUND;
            goto FS_CHDIREXIT;
            }

         if (ulCluster == pVolInfo->BootSect.bpb.RootDirStrtClus)
            {
            rc = 0;
            goto FS_CHDIREXIT;
            }

         *(PULONG)pcdfsd = ulCluster;


         if( usFlag == CD_EXPLICIT )
         {
            pOpenInfo->pSHInfo->bAttr = FILE_DIRECTORY;
            *((PULONG)pcdfsd + 1 ) = ( ULONG )pOpenInfo;
         }
         rc = 0;
         break;

      case CD_FREE     :
         pOpenInfo = ( POPENINFO )*(( PULONG )pcdfsd + 1 );
         ReleaseSH( pOpenInfo );
         pOpenInfo = NULL;
         rc = 0;
         break;
      default :
         rc = ERROR_INVALID_FUNCTION;
         break;
      }

FS_CHDIREXIT:

   if (rc && pOpenInfo)
      {
      if (pOpenInfo->pSHInfo)
         ReleaseSH(pOpenInfo);
      else
         free(pOpenInfo);
      }

   if (szDirLongName)
      free(szDirLongName);

   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_CHDIR returned %u", rc);

   _asm pop es;

   return rc;
}

int far pascal _loadds FS_MKDIR(
    struct cdfsi far * pcdfsi,      /* pcdfsi   */
    struct cdfsd far * pcdfsd,      /* pcdfsd   */
    char far * pName,           /* pName    */
    unsigned short usCurDirEnd,     /* iCurDirEnd   */
    char far * pEABuf,          /* pEABuf   */
    unsigned short usFlags      /* flags    */
)
{
PVOLINFO pVolInfo;
ULONG    ulCluster;
ULONG    ulDirCluster;
PSZ      pszFile;
PDIRENTRY pDirEntry;
PDIRENTRY pDir;
#ifdef EXFAT
PDIRENTRY1 pDir1;
#endif
PDIRENTRY1 pDirStream = NULL;
PSHOPENINFO pDirSHInfo = NULL;
USHORT   rc;
PBYTE    pbCluster;
ULONG    ulBlock;

   _asm push es;

   usFlags = usFlags;

   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_MKDIR - %s", pName);

   pVolInfo = GetVolInfo(pcdfsi->cdi_hVPB);

   if (! pVolInfo)
      {
      rc = ERROR_INVALID_DRIVE;
      goto FS_MKDIREXIT;
      }

   if (pVolInfo->fFormatInProgress)
      {
      rc = ERROR_ACCESS_DENIED;
      goto FS_MKDIREXIT;
      }

   if (IsDriveLocked(pVolInfo))
      {
      rc = ERROR_DRIVE_LOCKED;
      goto FS_MKDIREXIT;
      }
   if (!pVolInfo->fDiskCleanOnMount)
      {
      rc = ERROR_VOLUME_DIRTY;
      goto FS_MKDIREXIT;
      }
   if (pVolInfo->fWriteProtected)
      {
      rc = ERROR_WRITE_PROTECT;
      goto FS_MKDIREXIT;
      }

   if (strlen(pName) > FAT32MAXPATH)
      {
      rc = ERROR_FILENAME_EXCED_RANGE;
      goto FS_MKDIREXIT;
      }

   pDirEntry = (PDIRENTRY)malloc((size_t)sizeof(DIRENTRY));
   if (!pDirEntry)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto FS_MKDIREXIT;
      }
#ifdef EXFAT
   pDirStream = (PDIRENTRY1)malloc((size_t)sizeof(DIRENTRY1));
   if (!pDirStream)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto FS_MKDIREXIT;
      }
   pDirSHInfo = (PSHOPENINFO)malloc((size_t)sizeof(SHOPENINFO));
   if (!pDirSHInfo)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto FS_MKDIREXIT;
      }
#endif

   ulDirCluster = FindDirCluster(pVolInfo,
      pcdfsi,
      pcdfsd,
      pName,
      usCurDirEnd,
      RETURN_PARENT_DIR,
      &pszFile,
      pDirStream);

   if (ulDirCluster == pVolInfo->ulFatEof)
      {
      rc = ERROR_PATH_NOT_FOUND;
      goto FS_MKDIREXIT;
      }

#ifdef EXFAT
   if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
      {
      SetSHInfo1(pVolInfo, pDirStream, pDirSHInfo);
      }
#endif

   ulCluster = FindPathCluster(pVolInfo, ulDirCluster, pszFile, pDirSHInfo, pDirEntry, NULL, NULL);
   if (ulCluster != pVolInfo->ulFatEof)
      {
      rc = ERROR_ACCESS_DENIED;
      goto FS_MKDIREXIT;
      }

   ulCluster = SetNextCluster( pVolInfo, FAT_ASSIGN_NEW, pVolInfo->ulFatEof);
   if (ulCluster == pVolInfo->ulFatEof)
      {
      rc = ERROR_DISK_FULL;
      goto FS_MKDIREXIT;
      }

   pbCluster = malloc((size_t)pVolInfo->ulBlockSize);
   if (!pbCluster)
      {
      SetNextCluster( pVolInfo, ulCluster, 0L);
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto FS_MKDIREXIT;
      }

   memset(pbCluster, 0, (size_t)pVolInfo->ulBlockSize);

#ifdef EXFAT
   if (pVolInfo->bFatType < FAT_TYPE_EXFAT)   
      {
#endif
      pDir = (PDIRENTRY)pbCluster;

      pDir->wCluster = LOUSHORT(ulCluster);
      pDir->wClusterHigh = HIUSHORT(ulCluster);
      pDir->bAttr = FILE_DIRECTORY;
#ifdef EXFAT
      }
   else
      {
      pDir1 = (PDIRENTRY1)pbCluster;

      pDir1->bEntryType = ENTRY_TYPE_FILE;
      pDir1->u.File.usFileAttr = FILE_DIRECTORY;
      (pDir1+1)->bEntryType = ENTRY_TYPE_STREAM_EXT;
      (pDir1+1)->u.Stream.bAllocPossible = 1;
      (pDir1+1)->u.Stream.bNoFatChain = 0;
#ifdef INCL_LONGLONG
      (pDir1+1)->u.Stream.ullValidDataLen = pVolInfo->ulClusterSize;
      (pDir1+1)->u.Stream.ullDataLen = pVolInfo->ulClusterSize;
#else
      AssignUL(&(pDir1+1)->u.Stream.ullValidDataLen, pVolInfo->ulClusterSize);
      AssignUL(&(pDir1+1)->u.Stream.ullDataLen, pVolInfo->ulClusterSize);
#endif
      (pDir1+1)->u.Stream.ulFirstClus = ulCluster;
      }
#endif

   //rc = MakeDirEntry(pVolInfo, ulDirCluster, (PDIRENTRY)pbCluster, NULL, pszFile);
   rc = MakeDirEntry(pVolInfo, ulDirCluster, pDirSHInfo, (PDIRENTRY)pbCluster,
                     (PDIRENTRY1)((PBYTE)pbCluster+sizeof(DIRENTRY1)), pszFile);

   if (rc)
      {
      free(pbCluster);
      goto FS_MKDIREXIT;
      }

#ifdef EXFAT
   if (pVolInfo->bFatType < FAT_TYPE_EXFAT)   
      {
#endif
      memset(pDir->bFileName, 0x20, 11);
      memcpy(pDir->bFileName, ".", 1);

      memcpy(pDir + 1, pDir, sizeof (DIRENTRY));
      pDir++;

      memcpy(pDir->bFileName, "..", 2);
      if (ulDirCluster == pVolInfo->BootSect.bpb.RootDirStrtClus)
         {
         pDir->wCluster = 0;
         pDir->wClusterHigh = 0;
         }
      else
         {
         pDir->wCluster = LOUSHORT(ulDirCluster);
         pDir->wClusterHigh = HIUSHORT(ulDirCluster);
         }
      pDir->bAttr = FILE_DIRECTORY;
#ifdef EXFAT
      }
   else
      memset(pbCluster, 0, (size_t)pVolInfo->ulBlockSize);
#endif

   rc = WriteBlock( pVolInfo, ulCluster, 0, pbCluster, DVIO_OPWRTHRU);

   if (rc)
      goto FS_MKDIREXIT;

   memset(pbCluster, 0, (size_t)pVolInfo->ulBlockSize);

   // zero-out remaining blocks
   for (ulBlock = 1; ulBlock < pVolInfo->ulClusterSize / pVolInfo->ulBlockSize; ulBlock++)
      {
      rc = WriteBlock( pVolInfo, ulCluster, ulBlock, pbCluster, DVIO_OPWRTHRU);
      if (rc)
         goto FS_MKDIREXIT;
      }

   free(pbCluster);

   if (f32Parms.fEAS && pEABuf && pEABuf != MYNULL)
      rc = usModifyEAS(pVolInfo, ulDirCluster, pDirSHInfo, pszFile, (PEAOP)pEABuf);

FS_MKDIREXIT:
   if (pDirEntry)
      free(pDirEntry);
#ifdef EXFAT
   if (pDirStream)
      free(pDirStream);
   if (pDirSHInfo)
      free(pDirSHInfo);
#endif

   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_MKDIR returned %u", rc);

   _asm pop es;

   return rc;
}

int far pascal _loadds FS_RMDIR(
    struct cdfsi far * pcdfsi,      /* pcdfsi   */
    struct cdfsd far * pcdfsd,      /* pcdfsd   */
    char far * pName,           /* pName    */
    unsigned short usCurDirEnd      /* iCurDirEnd   */
)
{
PVOLINFO pVolInfo;
ULONG    ulCluster;
ULONG    ulNextCluster;
ULONG    ulDirCluster;
PSZ      pszFile;
PDIRENTRY pDirEntry;
PDIRENTRY pDir;
PDIRENTRY pWork, pMax;
USHORT   rc;
USHORT   usFileCount;
//BYTE     szLongName[ FAT32MAXPATH ];
PSZ     szLongName;
PDIRENTRY1 pStreamEntry = NULL, pDirStream = NULL;
PSHOPENINFO pDirSHInfo = NULL;
PSHOPENINFO pSHInfo = NULL;

   _asm push es;

   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_RMDIR %s", pName);

   pVolInfo = GetVolInfo(pcdfsi->cdi_hVPB);

   if (! pVolInfo)
      {
      rc = ERROR_INVALID_DRIVE;
      goto FS_RMDIREXIT;
      }
   if (pVolInfo->fFormatInProgress)
      {
      rc = ERROR_ACCESS_DENIED;
      goto FS_RMDIREXIT;
      }
   if (IsDriveLocked(pVolInfo))
      {
      rc = ERROR_DRIVE_LOCKED;
      goto FS_RMDIREXIT;
      }
   if (!pVolInfo->fDiskCleanOnMount)
      {
      rc = ERROR_VOLUME_DIRTY;
      goto FS_RMDIREXIT;
      }
   if (pVolInfo->fWriteProtected)
      {
      rc = ERROR_WRITE_PROTECT;
      goto FS_RMDIREXIT;
      }
   if (strlen(pName) > FAT32MAXPATH)
      {
      rc = ERROR_FILENAME_EXCED_RANGE;
      goto FS_RMDIREXIT;
      }

   pDirEntry = (PDIRENTRY)malloc((size_t)sizeof(DIRENTRY));
   if (!pDirEntry)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto FS_RMDIREXIT;
      }
#ifdef EXFAT
   pStreamEntry = (PDIRENTRY1)malloc((size_t)sizeof(DIRENTRY1));
   if (!pStreamEntry)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto FS_RMDIREXIT;
      }
   pDirStream = (PDIRENTRY1)malloc((size_t)sizeof(DIRENTRY1));
   if (!pDirStream)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto FS_RMDIREXIT;
      }
   pSHInfo = (PSHOPENINFO)malloc((size_t)sizeof(SHOPENINFO));
   if (!pSHInfo)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto FS_RMDIREXIT;
      }
   pDirSHInfo = (PSHOPENINFO)malloc((size_t)sizeof(SHOPENINFO));
   if (!pDirSHInfo)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto FS_RMDIREXIT;
      }
#endif

   szLongName = (PSZ)malloc((size_t)FAT32MAXPATH);
   if (!szLongName)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto FS_RMDIREXIT;
      }

#if 1
   if( TranslateName(pVolInfo, 0L, pName, szLongName, TRANSLATE_SHORT_TO_LONG ))
      strcpy( szLongName, pName );

   rc = MY_ISCURDIRPREFIX( szLongName );
   if( rc )
     goto FS_RMDIREXIT;
#else
   rc = FSH_ISCURDIRPREFIX(pName);
   if (rc)
      goto FS_RMDIREXIT;
   rc = TranslateName(pVolInfo, 0L, pName, szName, TRANSLATE_AUTO);
   if (rc)
      goto FS_RMDIREXIT;
   rc = FSH_ISCURDIRPREFIX(szName);
   if (rc)
      goto FS_RMDIREXIT;
#endif

   ulDirCluster = FindDirCluster(pVolInfo,
      pcdfsi,
      pcdfsd,
      pName,
      usCurDirEnd,
      RETURN_PARENT_DIR,
      &pszFile,
      pDirStream);

   if (ulDirCluster == pVolInfo->ulFatEof)
      {
      rc = ERROR_PATH_NOT_FOUND;
      goto FS_RMDIREXIT;
      }

#ifdef EXFAT
   if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
      {
      SetSHInfo1(pVolInfo, pDirStream, pDirSHInfo);
      }
#endif

   ulCluster = FindPathCluster(pVolInfo, ulDirCluster, pszFile, pDirSHInfo, pDirEntry, pStreamEntry, NULL);
   if ( ulCluster == pVolInfo->ulFatEof ||
#ifdef EXFAT
       ((pVolInfo->bFatType <  FAT_TYPE_EXFAT) && !(pDirEntry->bAttr & FILE_DIRECTORY)) ||
       ((pVolInfo->bFatType == FAT_TYPE_EXFAT) && !(((PDIRENTRY1)pDirEntry)->u.File.usFileAttr & FILE_DIRECTORY)) )
#else
       !(pDirEntry->bAttr & FILE_DIRECTORY) )
#endif
      {
      rc = ERROR_PATH_NOT_FOUND;
      goto FS_RMDIREXIT;
      }

#ifdef EXFAT
   if ( ((pVolInfo->bFatType <  FAT_TYPE_EXFAT) && (pDirEntry->bAttr & FILE_READONLY)) ||
        ((pVolInfo->bFatType == FAT_TYPE_EXFAT) && (((PDIRENTRY1)pDirEntry)->u.File.usFileAttr & FILE_READONLY)) )
#else
   if ( pDirEntry->bAttr & FILE_READONLY )
#endif
      {
      rc = ERROR_ACCESS_DENIED;
      goto FS_RMDIREXIT;
      }

   pDir = malloc((size_t)pVolInfo->ulClusterSize);
   if (!pDir)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto FS_RMDIREXIT;
      }

#ifdef EXFAT
   if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
      {
      SetSHInfo1(pVolInfo, pStreamEntry, pSHInfo);
      }
#endif

   ulNextCluster = ulCluster;
   usFileCount = 0;
   while (ulNextCluster != pVolInfo->ulFatEof)
      {
      ULONG ulBlock;
      for (ulBlock = 0; ulBlock < pVolInfo->ulClusterSize / pVolInfo->ulBlockSize; ulBlock++)
         {
         rc = ReadBlock( pVolInfo, ulNextCluster, ulBlock, pDir, 0);
         if (rc)
            {
            free(pDir);
            goto FS_RMDIREXIT;
            }

#ifdef EXFAT
         if (pVolInfo->bFatType <  FAT_TYPE_EXFAT)
            {
#endif
            pWork = pDir;
            pMax = (PDIRENTRY)((PBYTE)pDir + pVolInfo->ulBlockSize);
            while (pWork < pMax)
               {
               if (pWork->bFileName[0] && pWork->bFileName[0] != DELETED_ENTRY &&
                   pWork->bAttr != FILE_LONGNAME)
                  {
                  if (memcmp(pWork->bFileName, ".       ", 8) &&
                      memcmp(pWork->bFileName, "..      ", 8))
                     usFileCount++;
                  }
               pWork++;
               }
#ifdef EXFAT
            }
         else
            {
            PDIRENTRY1 pWork1 = (PDIRENTRY1)pDir;
            PDIRENTRY1 pMax1 = (PDIRENTRY1)((PBYTE)pDir + pVolInfo->ulBlockSize);
            BYTE bSecondaryCount;
            while (pWork1 < pMax1)
               {
               BYTE bSecondaries = 0;
               if (pWork1->bEntryType == ENTRY_TYPE_EOD)
                  {
                  break;
                  }
               else if (pWork1->bEntryType & ENTRY_TYPE_IN_USE_STATUS)
                  {
                  if (pWork1->bEntryType == ENTRY_TYPE_FILE)
                     {
                     bSecondaryCount = pWork1->u.File.bSecondaryCount;
                     }
                  if (pWork1->bEntryType == ENTRY_TYPE_STREAM_EXT)
                     {
                     bSecondaries++;
                     }
                  if (pWork1->bEntryType == ENTRY_TYPE_FILE_NAME)
                     {
                     bSecondaries++;
                     }
                  }
               if (bSecondaries == bSecondaryCount)
                  usFileCount++;
               pWork1++;
               }
            }
#endif
         }
      ulNextCluster = GetNextCluster(pVolInfo, pSHInfo, ulNextCluster);
      if (!ulNextCluster)
         ulNextCluster = pVolInfo->ulFatEof;
      }
   free(pDir);
   if (usFileCount)
      {
      Message("Cannot RMDIR, contains %u files", usFileCount);
      rc = ERROR_ACCESS_DENIED;
      goto FS_RMDIREXIT;
      }

   if (f32Parms.fEAS)
      {
      rc = usDeleteEAS(pVolInfo, ulDirCluster, pDirSHInfo, pszFile);
      if (rc)
         goto FS_RMDIREXIT;
#if 0
      if (pDirEntry->fEAS == FILE_HAS_EAS || pDirEntry->fEAS == FILE_HAS_CRITICAL_EAS)
         pDirEntry->fEAS = FILE_HAS_NO_EAS;
#endif
      }

   rc = ModifyDirectory(pVolInfo, ulDirCluster, pDirSHInfo, MODIFY_DIR_DELETE,
      pDirEntry, NULL, pStreamEntry, NULL, NULL, DVIO_OPWRTHRU);
   if (rc)
      goto FS_RMDIREXIT;

   DeleteFatChain(pVolInfo, ulCluster);

FS_RMDIREXIT:
   if (pDirEntry)
      free(pDirEntry);
#ifdef EXFAT
   if (pStreamEntry)
      free(pStreamEntry);
   if (pDirStream)
      free(pDirStream);
   if (pDirSHInfo)
      free(pDirSHInfo);
   if (pSHInfo)
      free(pSHInfo);
#endif
   if (szLongName)
      free(szLongName);

   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_RMDIR returned %u", rc);

   _asm pop es;

   return rc;
}
