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

/******************************************************************
*
******************************************************************/
int far pascal _loadds FS_FILEATTRIBUTE(
    unsigned short usFlag,      /* flag     */
    struct cdfsi far * pcdfsi,      /* pcdfsi   */
    struct cdfsd far * pcdfsd,      /* pcdfsd   */
    char far * pName,           /* pName    */
    unsigned short usCurDirEnd,     /* iCurDirEnd   */
    unsigned short far * pAttr  /* pAttr    */
)
{
PVOLINFO pVolInfo;
ULONG ulCluster;
ULONG ulDirCluster;
PSZ   pszFile;
DIRENTRY DirEntry;
DIRENTRY DirNew;
DIRENTRY1 DirStream, DirEntryStream, DirEntryStreamNew;
SHOPENINFO DirSHInfo;
PSHOPENINFO pDirSHInfo = NULL;
USHORT rc;

   _asm push es;

   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_FILEATTRIBUTE, Flag = %X for %s", usFlag, pName);

   pVolInfo = GetVolInfo(pcdfsi->cdi_hVPB);

   if (! pVolInfo)
      {
      rc = ERROR_INVALID_DRIVE;
      goto FS_FILEATTRIBUTEEXIT;
      }

   if (pVolInfo->fFormatInProgress)
      {
      rc = ERROR_ACCESS_DENIED;
      goto FS_FILEATTRIBUTEEXIT;
      }

   if (IsDriveLocked(pVolInfo))
      {
      rc = ERROR_DRIVE_LOCKED;
      goto FS_FILEATTRIBUTEEXIT;
      }

   if (strlen(pName) > FAT32MAXPATH)
      {
      rc = ERROR_FILENAME_EXCED_RANGE;
      goto FS_FILEATTRIBUTEEXIT;
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
      goto FS_FILEATTRIBUTEEXIT;
      }

   if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
      {
      pDirSHInfo = &DirSHInfo;
      SetSHInfo1(pVolInfo, &DirStream, pDirSHInfo);
      }

   ulCluster = FindPathCluster(pVolInfo, ulDirCluster, pszFile, pDirSHInfo, &DirEntry, &DirEntryStream, NULL);
   if (ulCluster == pVolInfo->ulFatEof)
      {
      rc = ERROR_FILE_NOT_FOUND;
      goto FS_FILEATTRIBUTEEXIT;
      }

   switch (usFlag)
      {
      case FA_RETRIEVE :
         if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
            *pAttr = DirEntry.bAttr;
         else
            {
            PDIRENTRY1 pDirEntry = (PDIRENTRY1)&DirEntry;
            *pAttr = pDirEntry->u.File.usFileAttr;
            }
         rc = 0;
         break;

      case FA_SET     :
         {
         USHORT usMask;

         if (!pVolInfo->fDiskCleanOnMount)
            {
            rc = ERROR_VOLUME_DIRTY;
            goto FS_FILEATTRIBUTEEXIT;
            }

         if (pVolInfo->fWriteProtected)
            {
            rc = ERROR_WRITE_PROTECT;
            goto FS_FILEATTRIBUTEEXIT;
            }

         usMask = ~(FILE_READONLY | FILE_HIDDEN | FILE_SYSTEM | FILE_ARCHIVED);
         if (*pAttr & usMask)
            {
            rc = ERROR_ACCESS_DENIED;
            goto FS_FILEATTRIBUTEEXIT;
            }

         if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
            {
            memcpy(&DirNew, &DirEntry, sizeof (DIRENTRY));

            if (DirNew.bAttr & FILE_DIRECTORY)
               DirNew.bAttr = (BYTE)(*pAttr | FILE_DIRECTORY);
            else
               DirNew.bAttr = (BYTE)*pAttr;
            rc = ModifyDirectory(pVolInfo, ulDirCluster, pDirSHInfo, MODIFY_DIR_UPDATE,
               &DirEntry, &DirNew, NULL, NULL, NULL, 0);
            }
         else
            {
            DIRENTRY1 DirNew1;
            memcpy(&DirNew1, &DirEntry, sizeof (DIRENTRY));

            if (DirNew1.u.File.usFileAttr & FILE_DIRECTORY)
               DirNew1.u.File.usFileAttr = (BYTE)(*pAttr | FILE_DIRECTORY);
            else
               DirNew1.u.File.usFileAttr = (BYTE)*pAttr;
            rc = ModifyDirectory(pVolInfo, ulDirCluster, pDirSHInfo, MODIFY_DIR_UPDATE,
               &DirEntry, (PDIRENTRY)&DirNew1, &DirEntryStream, NULL, NULL, 0);
            }
         break;
         }
      default:
         rc = ERROR_INVALID_FUNCTION;
         break;
      }

FS_FILEATTRIBUTEEXIT:

   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_FILEATTRIBUTE returned %d", rc);

   _asm pop es;

   return rc;
}


/******************************************************************
*
******************************************************************/
int far pascal _loadds FS_PATHINFO(
    unsigned short usFlag,      /* flag     */
    struct cdfsi far * pcdfsi,      /* pcdfsi   */
    struct cdfsd far * pcdfsd,      /* pcdfsd   */
    char far * pName,           /* pName    */
    unsigned short usCurDirEnd,     /* iCurDirEnd   */
    unsigned short usLevel,     /* level    */
    char far * pData,           /* pData    */
    unsigned short cbData       /* cbData   */
)
{
PVOLINFO pVolInfo;
ULONG ulCluster;
ULONG ulDirCluster;
PSZ   pszFile;
DIRENTRY DirEntry;
DIRENTRY1 DirEntryStream;
DIRENTRY1 DirStream;
SHOPENINFO DirSHInfo;
PSHOPENINFO pDirSHInfo = NULL;
USHORT usNeededSize;
USHORT rc;

   _asm push es;

   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_PATHINFO Flag = %d, Level = %d called for %s, cbData = %u",
          usFlag, usLevel, pName, cbData);

   pVolInfo = GetVolInfo(pcdfsi->cdi_hVPB);

   if (! pVolInfo)
      {
      rc = ERROR_INVALID_DRIVE;
      goto FS_PATHINFOEXIT;
      }

   if (pVolInfo->fFormatInProgress)
      {
      rc = ERROR_ACCESS_DENIED;
      goto FS_PATHINFOEXIT;
      }

   if (IsDriveLocked(pVolInfo))
      {
      rc = ERROR_DRIVE_LOCKED;
      goto FS_PATHINFOEXIT;
      }

   if (strlen(pName) > FAT32MAXPATH)
      {
      rc = ERROR_FILENAME_EXCED_RANGE;
      goto FS_PATHINFOEXIT;
      }

   if (usLevel != FIL_NAMEISVALID)
      {
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
         goto FS_PATHINFOEXIT;
         }

      if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
         {
         pDirSHInfo = &DirSHInfo;
         SetSHInfo1(pVolInfo, &DirStream, pDirSHInfo);
         }
      }

   if (usFlag == PI_RETRIEVE)
      {
      BYTE szFullName[FAT32MAXPATH];

      if (usLevel != FIL_NAMEISVALID)
         {
         ulCluster = FindPathCluster(pVolInfo, ulDirCluster, pszFile, pDirSHInfo, &DirEntry, &DirEntryStream, NULL);
         if (ulCluster == pVolInfo->ulFatEof)
            {
            rc = ERROR_FILE_NOT_FOUND;
            goto FS_PATHINFOEXIT;
            }

         if( TranslateName( pVolInfo, 0L, pName, szFullName, TRANSLATE_SHORT_TO_LONG ))
            strcpy( szFullName, pName );
         }

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
         case FIL_QUERYALLEAS:
            usNeededSize = sizeof (EAOP);
            break;
         case FIL_NAMEISVALID:
            rc = 0;
            goto FS_PATHINFOEXIT;
         case 7:
            usNeededSize = strlen(szFullName) + 1;
            break;
         default                   :
            rc = ERROR_INVALID_LEVEL;
            goto FS_PATHINFOEXIT;
         }

      if (cbData < usNeededSize)
         {
         rc = ERROR_BUFFER_OVERFLOW;
         goto FS_PATHINFOEXIT;
         }

      rc = MY_PROBEBUF(PB_OPWRITE, pData, cbData);
      if (rc)
         {
         Message("Protection VIOLATION in FS_PATHINFO!\n");
         goto FS_PATHINFOEXIT;
         }

      switch (usLevel)
         {
         case FIL_STANDARD         :
            {
            PFILESTATUS pfStatus = (PFILESTATUS)pData;

            memset(pfStatus, 0, sizeof (FILESTATUS));

            if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
               {
               pfStatus->fdateCreation = DirEntry.wCreateDate;
               pfStatus->ftimeCreation = DirEntry.wCreateTime;
               pfStatus->fdateLastAccess = DirEntry.wAccessDate;
               pfStatus->fdateLastWrite = DirEntry.wLastWriteDate;
               pfStatus->ftimeLastWrite = DirEntry.wLastWriteTime;

               if (!(DirEntry.bAttr & FILE_DIRECTORY))
                  {
                  pfStatus->cbFile = DirEntry.ulFileSize;
                  pfStatus->cbFileAlloc =
                     (pfStatus->cbFile / pVolInfo->ulClusterSize) * pVolInfo->ulClusterSize +
                     (pfStatus->cbFile % pVolInfo->ulClusterSize ? pVolInfo->ulClusterSize : 0);
                  }

               pfStatus->attrFile = (USHORT)DirEntry.bAttr;
               }
            else
               {
               PDIRENTRY1 pDirEntry = (PDIRENTRY1)&DirEntry;
               pfStatus->fdateCreation = GetDate1(pDirEntry->u.File.ulCreateTimestp);
               pfStatus->ftimeCreation = GetTime1(pDirEntry->u.File.ulCreateTimestp);
               pfStatus->fdateLastAccess = GetDate1(pDirEntry->u.File.ulLastAccessedTimestp);
               pfStatus->fdateLastWrite = GetDate1(pDirEntry->u.File.ulLastModifiedTimestp);
               pfStatus->ftimeLastWrite = GetTime1(pDirEntry->u.File.ulLastModifiedTimestp);

               if (!(pDirEntry->u.File.usFileAttr & FILE_DIRECTORY))
                  {
                  pfStatus->cbFile = DirEntryStream.u.Stream.ullValidDataLen;
                  pfStatus->cbFileAlloc =
                     (pfStatus->cbFile / pVolInfo->ulClusterSize) * pVolInfo->ulClusterSize +
                     (pfStatus->cbFile % pVolInfo->ulClusterSize ? pVolInfo->ulClusterSize : 0);
                  }

               pfStatus->attrFile = (USHORT)pDirEntry->u.File.usFileAttr;
               }
            rc = 0;
            break;
            }

         case FIL_STANDARDL        :
            {
            PFILESTATUS3L pfStatus = (PFILESTATUS3L)pData;

            memset(pfStatus, 0, sizeof (FILESTATUS3L));

            if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
               {
               pfStatus->fdateCreation = DirEntry.wCreateDate;
               pfStatus->ftimeCreation = DirEntry.wCreateTime;
               pfStatus->fdateLastAccess = DirEntry.wAccessDate;
               pfStatus->fdateLastWrite = DirEntry.wLastWriteDate;
               pfStatus->ftimeLastWrite = DirEntry.wLastWriteTime;

               if (!(DirEntry.bAttr & FILE_DIRECTORY))
                  {
                  pfStatus->cbFile = DirEntry.ulFileSize;
                  pfStatus->cbFileAlloc =
                     (pfStatus->cbFile / pVolInfo->ulClusterSize) * pVolInfo->ulClusterSize +
                     (pfStatus->cbFile % pVolInfo->ulClusterSize ? pVolInfo->ulClusterSize : 0);
                  }

               pfStatus->attrFile = (USHORT)DirEntry.bAttr;
               }
            else
               {
               PDIRENTRY1 pDirEntry = (PDIRENTRY1)&DirEntry;
               pfStatus->fdateCreation = GetDate1(pDirEntry->u.File.ulCreateTimestp);
               pfStatus->ftimeCreation = GetTime1(pDirEntry->u.File.ulCreateTimestp);
               pfStatus->fdateLastAccess = GetDate1(pDirEntry->u.File.ulLastAccessedTimestp);
               pfStatus->fdateLastWrite = GetDate1(pDirEntry->u.File.ulLastModifiedTimestp);
               pfStatus->ftimeLastWrite = GetTime1(pDirEntry->u.File.ulLastModifiedTimestp);

               if (!(pDirEntry->u.File.usFileAttr & FILE_DIRECTORY))
                  {
                  pfStatus->cbFile = DirEntryStream.u.Stream.ullValidDataLen;
                  pfStatus->cbFileAlloc =
                     (pfStatus->cbFile / pVolInfo->ulClusterSize) * pVolInfo->ulClusterSize +
                     (pfStatus->cbFile % pVolInfo->ulClusterSize ? pVolInfo->ulClusterSize : 0);
                  }

               pfStatus->attrFile = (USHORT)pDirEntry->u.File.usFileAttr;
               }
            rc = 0;
            break;
            }

         case FIL_QUERYEASIZE      :
            {
            PFILESTATUS2 pfStatus = (PFILESTATUS2)pData;

            memset(pfStatus, 0, sizeof (FILESTATUS2));

            if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
               {
               pfStatus->fdateCreation = DirEntry.wCreateDate;
               pfStatus->ftimeCreation = DirEntry.wCreateTime;
               pfStatus->fdateLastAccess = DirEntry.wAccessDate;
               pfStatus->fdateLastWrite = DirEntry.wLastWriteDate;
               pfStatus->ftimeLastWrite = DirEntry.wLastWriteTime;
               if (!(DirEntry.bAttr & FILE_DIRECTORY))
                  {
                  pfStatus->cbFile = DirEntry.ulFileSize;
                  pfStatus->cbFileAlloc =
                     (pfStatus->cbFile / pVolInfo->ulClusterSize) * pVolInfo->ulClusterSize +
                     (pfStatus->cbFile % pVolInfo->ulClusterSize ? pVolInfo->ulClusterSize : 0);
                  }

               pfStatus->attrFile = (USHORT)DirEntry.bAttr;
               }
            else
               {
               PDIRENTRY1 pDirEntry = (PDIRENTRY1)&DirEntry;
               pfStatus->fdateCreation = GetDate1(pDirEntry->u.File.ulCreateTimestp);
               pfStatus->ftimeCreation = GetTime1(pDirEntry->u.File.ulCreateTimestp);
               pfStatus->fdateLastAccess = GetDate1(pDirEntry->u.File.ulLastAccessedTimestp);
               pfStatus->fdateLastWrite = GetDate1(pDirEntry->u.File.ulLastModifiedTimestp);
               pfStatus->ftimeLastWrite = GetTime1(pDirEntry->u.File.ulLastModifiedTimestp);

               if (!(pDirEntry->u.File.usFileAttr & FILE_DIRECTORY))
                  {
                  pfStatus->cbFile = DirEntryStream.u.Stream.ullValidDataLen;
                  pfStatus->cbFileAlloc =
                     (pfStatus->cbFile / pVolInfo->ulClusterSize) * pVolInfo->ulClusterSize +
                     (pfStatus->cbFile % pVolInfo->ulClusterSize ? pVolInfo->ulClusterSize : 0);
                  }

               pfStatus->attrFile = (USHORT)pDirEntry->u.File.usFileAttr;
               }
            if (!f32Parms.fEAS)
               {
               pfStatus->cbList = sizeof pfStatus->cbList;
               rc = 0;
               }
            else
               {
               rc = usGetEASize(pVolInfo, ulDirCluster, pDirSHInfo, pszFile, &pfStatus->cbList);
               }
            break;
            }

         case FIL_QUERYEASIZEL     :
            {
            PFILESTATUS4L pfStatus = (PFILESTATUS4L)pData;

            memset(pfStatus, 0, sizeof (FILESTATUS4L));

            if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
               {
               pfStatus->fdateCreation = DirEntry.wCreateDate;
               pfStatus->ftimeCreation = DirEntry.wCreateTime;
               pfStatus->fdateLastAccess = DirEntry.wAccessDate;
               pfStatus->fdateLastWrite = DirEntry.wLastWriteDate;
               pfStatus->ftimeLastWrite = DirEntry.wLastWriteTime;
               if (!(DirEntry.bAttr & FILE_DIRECTORY))
                  {
                  pfStatus->cbFile = DirEntry.ulFileSize;
                  pfStatus->cbFileAlloc =
                     (pfStatus->cbFile / pVolInfo->ulClusterSize) * pVolInfo->ulClusterSize +
                     (pfStatus->cbFile % pVolInfo->ulClusterSize ? pVolInfo->ulClusterSize : 0);
                  }

               pfStatus->attrFile = (USHORT)DirEntry.bAttr;
               }
            else
               {
               PDIRENTRY1 pDirEntry = (PDIRENTRY1)&DirEntry;
               pfStatus->fdateCreation = GetDate1(pDirEntry->u.File.ulCreateTimestp);
               pfStatus->ftimeCreation = GetTime1(pDirEntry->u.File.ulCreateTimestp);
               pfStatus->fdateLastAccess = GetDate1(pDirEntry->u.File.ulLastAccessedTimestp);
               pfStatus->fdateLastWrite = GetDate1(pDirEntry->u.File.ulLastModifiedTimestp);
               pfStatus->ftimeLastWrite = GetTime1(pDirEntry->u.File.ulLastModifiedTimestp);

               if (!(pDirEntry->u.File.usFileAttr & FILE_DIRECTORY))
                  {
                  pfStatus->cbFile = DirEntryStream.u.Stream.ullValidDataLen;
                  pfStatus->cbFileAlloc =
                     (pfStatus->cbFile / pVolInfo->ulClusterSize) * pVolInfo->ulClusterSize +
                     (pfStatus->cbFile % pVolInfo->ulClusterSize ? pVolInfo->ulClusterSize : 0);
                  }

               pfStatus->attrFile = (USHORT)pDirEntry->u.File.usFileAttr;
               }
            if (!f32Parms.fEAS)
               {
               pfStatus->cbList = sizeof pfStatus->cbList;
               rc = 0;
               }
            else
               {
               rc = usGetEASize(pVolInfo, ulDirCluster, pDirSHInfo, pszFile, &pfStatus->cbList);
               }
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
               Message("Protection VIOLATION in FS_PATHINFO!\n");
               goto FS_PATHINFOEXIT;
               }
            rc = MY_PROBEBUF(PB_OPWRITE, (PBYTE)pFEA, (USHORT)pFEA->cbList);
            if (rc)
               {
               Message("Protection VIOLATION in FS_PILEINFO!\n");
               goto FS_PATHINFOEXIT;
               }
            if (!f32Parms.fEAS)
               {
               rc = usGetEmptyEAS(pszFile,pEA);
               }
            else
               {
               rc = usGetEAS(pVolInfo, usLevel, ulDirCluster, pDirSHInfo, pszFile, pEA);
               }

            break;
            }

         case FIL_QUERYALLEAS:
            {
            PEAOP pEA = (PEAOP)pData;
            PFEALIST pFEA = pEA->fpFEAList;
            rc = MY_PROBEBUF(PB_OPWRITE, (PBYTE)pFEA, sizeof pFEA->cbList);
            if (rc)
               {
               Message("Protection VIOLATION in FS_PATHINFO!\n");
               goto FS_PATHINFOEXIT;
               }
            rc = MY_PROBEBUF(PB_OPWRITE, (PBYTE)pFEA, (USHORT)pFEA->cbList);
            if (rc)
               {
               Message("Protection VIOLATION in FS_PATHINFO!\n");
               goto FS_PATHINFOEXIT;
               }
            if (!f32Parms.fEAS)
               {
               memset(pFEA, 0, (USHORT)pFEA->cbList);
               pFEA->cbList = sizeof pFEA->cbList;
               rc = 0;
               }
            else
               {
               rc = usGetEAS(pVolInfo, usLevel, ulDirCluster, pDirSHInfo, pszFile, pEA);
               }

            break;
            }

         case FIL_NAMEISVALID :
            rc = 0;
            break;

         case 7:
            strcpy(pData, szFullName);
            rc = 0;
            break;

         default :
            rc = ERROR_INVALID_LEVEL;
            break;
         }
      goto FS_PATHINFOEXIT;
      }


   if (usFlag & PI_SET)
      {
      if (!pVolInfo->fDiskCleanOnMount)
         {
         rc = ERROR_VOLUME_DIRTY;
         goto FS_PATHINFOEXIT;
         }
      if (pVolInfo->fWriteProtected)
         {
         rc = ERROR_WRITE_PROTECT;
         goto FS_PATHINFOEXIT;
         }

      rc = MY_PROBEBUF(PB_OPREAD, pData, cbData);
      if (rc)
         {
         Message("Protection VIOLATION in FS_PATHINFO!\n");
         goto FS_PATHINFOEXIT;
         }

      if (usLevel == FIL_STANDARD  || usLevel == FIL_QUERYEASIZE ||
          usLevel == FIL_STANDARDL || usLevel == FIL_QUERYEASIZEL)
         {
         ulCluster = FindPathCluster(pVolInfo, ulDirCluster, pszFile, pDirSHInfo, &DirEntry, &DirEntryStream, NULL);
         if (ulCluster == pVolInfo->ulFatEof)
            {
            rc = ERROR_FILE_NOT_FOUND;
            goto FS_PATHINFOEXIT;
            }
         }

      switch (usLevel)
         {
         case FIL_STANDARD:
            {
            PFILESTATUS pfStatus = (PFILESTATUS)pData;
            USHORT usMask;
            DIRENTRY DirNew;
            PDIRENTRY1 pDirNew = (PDIRENTRY1)&DirNew;
            PDIRENTRY1 pDirEntry = (PDIRENTRY1)&DirEntry;

            if (cbData < sizeof (FILESTATUS))
               {
               rc = ERROR_INSUFFICIENT_BUFFER;
               goto FS_PATHINFOEXIT;
               }

            usMask = FILE_READONLY | FILE_HIDDEN | FILE_SYSTEM | FILE_ARCHIVED;
            if ( ((pVolInfo->bFatType <  FAT_TYPE_EXFAT) && (DirEntry.bAttr & FILE_DIRECTORY)) ||
                 ((pVolInfo->bFatType == FAT_TYPE_EXFAT) && (pDirEntry->u.File.usFileAttr & FILE_DIRECTORY)) )
               usMask |= FILE_DIRECTORY;
            usMask = ~usMask;

            if (pfStatus->attrFile & usMask)
               {
               if (f32Parms.fMessageActive & LOG_FS)
                  Message("Trying to set invalid attr bits: %X", pfStatus->attrFile);
               rc = ERROR_ACCESS_DENIED;
               goto FS_PATHINFOEXIT;
               }

            memcpy(&DirNew, &DirEntry, sizeof (DIRENTRY));

            usMask = 0;
            if (memcmp(&pfStatus->fdateCreation, &usMask, sizeof usMask) ||
                memcmp(&pfStatus->ftimeCreation, &usMask, sizeof usMask))
               {
               if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
                  {
                  DirNew.wCreateDate = pfStatus->fdateCreation;
                  DirNew.wCreateTime = pfStatus->ftimeCreation;
                  }
               else
                  {
                  pDirNew->u.File.ulCreateTimestp = SetTimeStamp(pfStatus->fdateCreation, pfStatus->ftimeCreation);
                  }
               }

            if (memcmp(&pfStatus->fdateLastWrite, &usMask, sizeof usMask) ||
                memcmp(&pfStatus->ftimeLastWrite, &usMask, sizeof usMask))
               {
               if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
                  {
                  DirNew.wLastWriteDate = pfStatus->fdateLastWrite;
                  DirNew.wLastWriteTime = pfStatus->ftimeLastWrite;
                  }
               else
                  {
                  pDirNew->u.File.ulLastModifiedTimestp = SetTimeStamp(pfStatus->fdateLastWrite, pfStatus->ftimeLastWrite);
                  }
               }

            if (memcmp(&pfStatus->fdateLastAccess, &usMask, sizeof usMask))
               {
               if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
                  {
                  DirNew.wAccessDate = pfStatus->fdateLastAccess;
                  }
               else
                  {
                  pDirNew->u.File.ulLastAccessedTimestp = SetTimeStamp(pfStatus->fdateLastAccess, pfStatus->ftimeLastAccess);
                  }
               }

            if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
               {
               if (DirNew.bAttr & FILE_DIRECTORY)
                  DirNew.bAttr = (BYTE)(pfStatus->attrFile | FILE_DIRECTORY);
               else
                  DirNew.bAttr = (BYTE)pfStatus->attrFile;
               }
            else
               {
               if (pDirNew->u.File.usFileAttr & FILE_DIRECTORY)
                  pDirNew->u.File.usFileAttr = (BYTE)(pfStatus->attrFile | FILE_DIRECTORY);
               else
                  pDirNew->u.File.usFileAttr = (BYTE)pfStatus->attrFile;
               }

            rc = ModifyDirectory(pVolInfo, ulDirCluster, pDirSHInfo, MODIFY_DIR_UPDATE,
               &DirEntry, &DirNew, &DirEntryStream, NULL, NULL, 0);
            break;
            }

         case FIL_STANDARDL:
            {
            PFILESTATUS3L pfStatus = (PFILESTATUS3L)pData;
            USHORT usMask;
            DIRENTRY DirNew;
            PDIRENTRY1 pDirNew = (PDIRENTRY1)&DirNew;
            PDIRENTRY1 pDirEntry = (PDIRENTRY1)&DirEntry;

            if (cbData < sizeof (FILESTATUS3L))
               {
               rc = ERROR_INSUFFICIENT_BUFFER;
               goto FS_PATHINFOEXIT;
               }

            usMask = FILE_READONLY | FILE_HIDDEN | FILE_SYSTEM | FILE_ARCHIVED;
            if ( ((pVolInfo->bFatType <  FAT_TYPE_EXFAT) && (DirEntry.bAttr & FILE_DIRECTORY)) ||
                 ((pVolInfo->bFatType == FAT_TYPE_EXFAT) && (pDirEntry->u.File.usFileAttr & FILE_DIRECTORY)) )
               usMask |= FILE_DIRECTORY;
            usMask = ~usMask;

            if (pfStatus->attrFile & usMask)
               {
               if (f32Parms.fMessageActive & LOG_FS)
                  Message("Trying to set invalid attr bits: %X", pfStatus->attrFile);
               rc = ERROR_ACCESS_DENIED;
               goto FS_PATHINFOEXIT;
               }

            memcpy(&DirNew, &DirEntry, sizeof (DIRENTRY));

            usMask = 0;
            if (memcmp(&pfStatus->fdateCreation, &usMask, sizeof usMask) ||
                memcmp(&pfStatus->ftimeCreation, &usMask, sizeof usMask))
               {
               if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
                  {
                  DirNew.wCreateDate = pfStatus->fdateCreation;
                  DirNew.wCreateTime = pfStatus->ftimeCreation;
                  }
               else
                  {
                  pDirNew->u.File.ulCreateTimestp = SetTimeStamp(pfStatus->fdateCreation, pfStatus->ftimeCreation);
                  }
               }

            if (memcmp(&pfStatus->fdateLastWrite, &usMask, sizeof usMask) ||
                memcmp(&pfStatus->ftimeLastWrite, &usMask, sizeof usMask))
               {
               if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
                  {
                  DirNew.wLastWriteDate = pfStatus->fdateLastWrite;
                  DirNew.wLastWriteTime = pfStatus->ftimeLastWrite;
                  }
               else
                  {
                  pDirNew->u.File.ulLastModifiedTimestp = SetTimeStamp(pfStatus->fdateLastWrite, pfStatus->ftimeLastWrite);
                  }
               }

            if (memcmp(&pfStatus->fdateLastAccess, &usMask, sizeof usMask))
               {
               if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
                  {
                  DirNew.wAccessDate = pfStatus->fdateLastAccess;
                  }
               else
                  {
                  pDirNew->u.File.ulLastAccessedTimestp = SetTimeStamp(pfStatus->fdateLastAccess, pfStatus->ftimeLastAccess);
                  }
               }

            if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
               {
               if (DirNew.bAttr & FILE_DIRECTORY)
                  DirNew.bAttr = (BYTE)(pfStatus->attrFile | FILE_DIRECTORY);
               else
                  DirNew.bAttr = (BYTE)pfStatus->attrFile;
               }
            else
               {
               if (pDirNew->u.File.usFileAttr & FILE_DIRECTORY)
                  pDirNew->u.File.usFileAttr = (BYTE)(pfStatus->attrFile | FILE_DIRECTORY);
               else
                  pDirNew->u.File.usFileAttr = (BYTE)pfStatus->attrFile;
               }

            rc = ModifyDirectory(pVolInfo, ulDirCluster, pDirSHInfo, MODIFY_DIR_UPDATE,
               &DirEntry, &DirNew, &DirEntryStream, NULL, NULL, 0);
            break;
            }

         case FIL_QUERYEASIZE      :
         case FIL_QUERYEASIZEL     :
            if (!f32Parms.fEAS)
               rc = 0;
            else
               {
#if 0
               DIRENTRY DirNew;
#endif
               rc = usModifyEAS(pVolInfo, ulDirCluster, pDirSHInfo, pszFile, (PEAOP)pData);
               if (rc)
                  goto FS_PATHINFOEXIT;
#if 0
               memcpy(&DirNew, &DirEntry, sizeof (DIRENTRY));
               DirNew.wLastWriteDate.year = pGI->year - 1980;
               DirNew.wLastWriteDate.month = pGI->month;
               DirNew.wLastWriteDate.day = pGI->day;
               DirNew.wLastWriteTime.hours = pGI->hour;
               DirNew.wLastWriteTime.minutes = pGI->minutes;
               DirNew.wLastWriteTime.twosecs = pGI->seconds / 2;
               rc = ModifyDirectory(pVolInfo, ulDirCluster, pDirSHInfo, MODIFY_DIR_UPDATE,
                  &DirEntry, &DirNew, &DirEntryStream, NULL, NULL, 0);
#endif
               }
            break;

         default          :
            rc = ERROR_INVALID_LEVEL;
            break;
         }
      }
   else
      rc = ERROR_INVALID_FUNCTION;

FS_PATHINFOEXIT:

   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_PATHINFO returned %u", rc);

   _asm pop es;

   return rc;
}
