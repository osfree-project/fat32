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
BYTE     szDirLongName[ FAT32MAXPATH ];

   _asm push es;

   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_CHDIR, flag %u", usFlag);

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
DIRENTRY DirEntry;
PDIRENTRY pDir;
PDIRENTRY1 pDir1;
DIRENTRY1 DirStream;
SHOPENINFO DirSHInfo;
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

   ulDirCluster = FindDirCluster(pVolInfo,
      pcdfsi,
      pcdfsd,
      pName,
      usCurDirEnd,
      RETURN_PARENT_DIR,
      &pszFile,
      &DirStream);

   if (ulDirCluster == pVolInfo->ulFatEof)
      {
      rc = ERROR_PATH_NOT_FOUND;
      goto FS_MKDIREXIT;
      }

   if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
      {
      pDirSHInfo = &DirSHInfo;
      SetSHInfo1(pVolInfo, &DirStream, pDirSHInfo);
      }

   ulCluster = FindPathCluster(pVolInfo, ulDirCluster, pszFile, pDirSHInfo, &DirEntry, NULL, NULL);
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

   pbCluster = malloc(pVolInfo->ulBlockSize);
   if (!pbCluster)
      {
      SetNextCluster( pVolInfo, ulCluster, 0L);
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto FS_MKDIREXIT;
      }

   memset(pbCluster, 0, pVolInfo->ulBlockSize);

   if (pVolInfo->bFatType < FAT_TYPE_EXFAT)   
      {
      pDir = (PDIRENTRY)pbCluster;

      pDir->wCluster = LOUSHORT(ulCluster);
      pDir->wClusterHigh = HIUSHORT(ulCluster);
      pDir->bAttr = FILE_DIRECTORY;
      }
   else
      {
      pDir1 = (PDIRENTRY1)pbCluster;

      pDir1->bEntryType = ENTRY_TYPE_FILE;
      pDir1->u.File.usFileAttr = FILE_DIRECTORY;
      (pDir1+1)->bEntryType = ENTRY_TYPE_STREAM_EXT;
      (pDir1+1)->u.Stream.bAllocPossible = 1;
      (pDir1+1)->u.Stream.bNoFatChain = 0;
      (pDir1+1)->u.Stream.ullValidDataLen = pVolInfo->ulClusterSize;
      (pDir1+1)->u.Stream.ullDataLen = pVolInfo->ulClusterSize;
      (pDir1+1)->u.Stream.ulFirstClus = ulCluster;
      }

   //rc = MakeDirEntry(pVolInfo, ulDirCluster, (PDIRENTRY)pbCluster, NULL, pszFile);
   rc = MakeDirEntry(pVolInfo, ulDirCluster, pDirSHInfo, (PDIRENTRY)pbCluster,
                     (PDIRENTRY1)((PBYTE)pbCluster+sizeof(DIRENTRY1)), pszFile);

   if (rc)
      {
      free(pbCluster);
      goto FS_MKDIREXIT;
      }

   if (pVolInfo->bFatType < FAT_TYPE_EXFAT)   
      {
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
      }
   else
      memset(pbCluster, 0, pVolInfo->ulBlockSize);

   rc = WriteBlock( pVolInfo, ulCluster, 0, pbCluster, DVIO_OPWRTHRU);

   if (rc)
      goto FS_MKDIREXIT;

   memset(pbCluster, 0, pVolInfo->ulBlockSize);

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
DIRENTRY DirEntry;
PDIRENTRY1 pDirEntry = (PDIRENTRY1)&DirEntry;
PDIRENTRY pDir;
PDIRENTRY pWork, pMax;
USHORT   rc;
USHORT   usFileCount;
BYTE     szLongName[ FAT32MAXPATH ];
DIRENTRY1 StreamEntry, DirStream;
SHOPENINFO DirSHInfo;
PSHOPENINFO pDirSHInfo = NULL;
SHOPENINFO SHInfo;
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
      &DirStream);

   if (ulDirCluster == pVolInfo->ulFatEof)
      {
      rc = ERROR_PATH_NOT_FOUND;
      goto FS_RMDIREXIT;
      }

   if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
      {
      pDirSHInfo = &DirSHInfo;
      SetSHInfo1(pVolInfo, &DirStream, pDirSHInfo);
      }

   ulCluster = FindPathCluster(pVolInfo, ulDirCluster, pszFile, pDirSHInfo, &DirEntry, &StreamEntry, NULL);
   if (ulCluster == pVolInfo->ulFatEof ||
       ((pVolInfo->bFatType <  FAT_TYPE_EXFAT) && !(DirEntry.bAttr & FILE_DIRECTORY)) ||
       ((pVolInfo->bFatType == FAT_TYPE_EXFAT) && !(pDirEntry->u.File.usFileAttr & FILE_DIRECTORY)) )
      {
      rc = ERROR_PATH_NOT_FOUND;
      goto FS_RMDIREXIT;
      }

   if ( ((pVolInfo->bFatType <  FAT_TYPE_EXFAT) && (DirEntry.bAttr & FILE_READONLY)) ||
        ((pVolInfo->bFatType == FAT_TYPE_EXFAT) && (pDirEntry->u.File.usFileAttr & FILE_READONLY)) )
      {
      rc = ERROR_ACCESS_DENIED;
      goto FS_RMDIREXIT;
      }

   pDir = malloc(pVolInfo->ulClusterSize);
   if (!pDir)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto FS_RMDIREXIT;
      }

   if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
      {
      pSHInfo = &SHInfo;
      SetSHInfo1(pVolInfo, &StreamEntry, pSHInfo);
      }

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

         if (pVolInfo->bFatType <  FAT_TYPE_EXFAT)
            {
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
            }
         else
            {
            PDIRENTRY1 pWork1 = (PDIRENTRY1)pDir;
            PDIRENTRY1 pMax1 = (PDIRENTRY1)((PBYTE)pDir + pVolInfo->ulBlockSize);
            BYTE bSecondaryCount;
            while (pWork1 < pMax1)
               {
               if (pWork1->bEntryType == ENTRY_TYPE_EOD)
                  {
                  break;
                  }
               else if (pWork1->bEntryType & ENTRY_TYPE_IN_USE_STATUS)
                  {
                  if (pWork1->bEntryType == ENTRY_TYPE_FILE)
                     {
                     bSecondaryCount = pWork1->u.File.bSecondaryCount;
                     usFileCount++;
                     }
                  }
               pWork1++;
               }
            }
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
      if (DirEntry.fEAS == FILE_HAS_EAS || DirEntry.fEAS == FILE_HAS_CRITICAL_EAS)
         DirEntry.fEAS = FILE_HAS_NO_EAS;
#endif
      }

   rc = ModifyDirectory(pVolInfo, ulDirCluster, pDirSHInfo, MODIFY_DIR_DELETE,
      &DirEntry, NULL, &StreamEntry, NULL, NULL, DVIO_OPWRTHRU);
   if (rc)
      goto FS_RMDIREXIT;

   DeleteFatChain(pVolInfo, ulCluster);

FS_RMDIREXIT:
   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_RMDIR returned %u", rc);

   _asm pop es;

   return rc;
}
