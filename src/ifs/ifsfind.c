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

static USHORT FillDirEntry(PVOLINFO pVolInfo, PBYTE * ppData, PUSHORT pcbData, PFINDINFO pFindInfo, USHORT usLevel);
static BOOL GetBlock(PVOLINFO pVolInfo, PFINDINFO pFindInfo, USHORT usBlockIndex);

/******************************************************************
*
******************************************************************/
int far pascal _loadds FS_FINDCLOSE(struct fsfsi far * pfsfsi,
                            struct fsfsd far * pfsfsd)
{
PVOLINFO pVolInfo;
PFINDINFO pFindInfo = (PFINDINFO)pfsfsd;
APIRET rc = 0;

   _asm push es;

   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_FINDCLOSE");

   pVolInfo = GetVolInfo(pfsfsi->fsi_hVPB);

   if (! pVolInfo)
      {
      rc = ERROR_INVALID_DRIVE;
      goto FS_FINDCLOSEEXIT;
      }

   if (pVolInfo->fFormatInProgress)
      {
      rc = ERROR_ACCESS_DENIED;
      goto FS_FINDCLOSEEXIT;
      }

   if (pFindInfo->pSHInfo)
      {
      free(pFindInfo->pSHInfo);
      }

   if (pFindInfo->pInfo)
      {
      if (RemoveFindEntry(pVolInfo, pFindInfo->pInfo))
         free(pFindInfo->pInfo);

      pFindInfo->pInfo = NULL;
      }

FS_FINDCLOSEEXIT:
   _asm pop es;

   return rc;
}

/******************************************************************
*
******************************************************************/
int far pascal _loadds FS_FINDFIRST(struct cdfsi far * pcdfsi,      /* pcdfsi   */
                            struct cdfsd far * pcdfsd,      /* pcdfsd   */
                            char far * pName,           /* pName    */
                            unsigned short usCurDirEnd,     /* iCurDirEnd   */
                            unsigned short usAttr,      /* attr     */
                            struct fsfsi far * pfsfsi,      /* pfsfsi   */
                            struct fsfsd far * pfsfsd,      /* pfsfsd   */
                            char far * pData,           /* pData    */
                            unsigned short cbData,      /* cbData   */
                            unsigned short far * pcMatch,   /* pcMatch  */
                            unsigned short usLevel,     /* level    */
                            unsigned short usFlags)     /* flags    */
{
PVOLINFO pVolInfo;
PFINDINFO pFindInfo = (PFINDINFO)pfsfsd;
USHORT rc;
USHORT usIndex;
USHORT usNeededLen;
USHORT usNumClusters;
USHORT usNumBlocks;
ULONG  ulCluster;
ULONG  ulDirCluster;
PSZ    pSearch;
PFINFO pNext;
ULONG ulNeededSpace;
USHORT usEntriesWanted;
EAOP   EAOP;
PROCINFO ProcInfo;
ULONG  ulSector;
USHORT usSectorsRead;
USHORT usSectorsPerBlock;
DIRENTRY1 StreamEntry;

   _asm push es;

   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_FINDFIRST for %s attr %X, Level %d, cbData %u, MaxEntries %u", pName, usAttr, usLevel, cbData, *pcMatch);

   usEntriesWanted = *pcMatch;
   *pcMatch  = 0;

   if (strlen(pName) > FAT32MAXPATH)
      {
      rc = ERROR_FILENAME_EXCED_RANGE;
      goto FS_FINDFIRSTEXIT;
      }

   memset(pfsfsd, 0, sizeof (struct fsfsd));

   pVolInfo = GetVolInfo(pfsfsi->fsi_hVPB);

   if (! pVolInfo)
      {
      rc = ERROR_INVALID_DRIVE;
      goto FS_FINDFIRSTEXIT;
      }

   if (pVolInfo->fFormatInProgress)
      {
      rc = ERROR_ACCESS_DENIED;
      goto FS_FINDFIRSTEXIT;
      }

   if (IsDriveLocked(pVolInfo))
      {
      rc = ERROR_DRIVE_LOCKED;
      goto FS_FINDFIRSTEXIT;
      }

   if (!pVolInfo->fDiskCleanOnMount && !f32Parms.fReadonly)
      {
      rc = ERROR_VOLUME_DIRTY;
      goto FS_FINDFIRSTEXIT;
      }

   switch (usLevel)
      {
      case FIL_STANDARD         :
         usNeededLen = sizeof (FILEFNDBUF) - CCHMAXPATHCOMP;
         break;
      case FIL_STANDARDL        :
         usNeededLen = sizeof (FILEFNDBUF3L) - CCHMAXPATHCOMP;
         break;
      case FIL_QUERYEASIZE      :
         usNeededLen = sizeof (FILEFNDBUF2) - CCHMAXPATHCOMP;
         break;
      case FIL_QUERYEASIZEL     :
         usNeededLen = sizeof (FILEFNDBUF4L) - CCHMAXPATHCOMP;
         break;
      case FIL_QUERYEASFROMLIST :
         usNeededLen = sizeof (EAOP) + sizeof (FILEFNDBUF3) + sizeof (ULONG);
         break;
      case FIL_QUERYEASFROMLISTL :
         usNeededLen = sizeof (EAOP) + sizeof (FILEFNDBUF3L) + sizeof (ULONG);
         break;
      default                   :
         rc = ERROR_INVALID_FUNCTION;
         goto FS_FINDFIRSTEXIT;
      }

   if (usFlags == FF_GETPOS)
      usNeededLen += sizeof (ULONG);

   if (cbData < usNeededLen)
      {
      rc = ERROR_BUFFER_OVERFLOW;
      goto FS_FINDFIRSTEXIT;
      }

   rc = MY_PROBEBUF(PB_OPWRITE, pData, cbData);
   if (rc)
      {
      Message("FAT32: Protection VIOLATION in FS_FINDFIRST! (SYS%d)", rc);
      goto FS_FINDFIRSTEXIT;
      }

   if (usLevel == FIL_QUERYEASFROMLIST || usLevel == FIL_QUERYEASFROMLISTL)
      {
      memcpy(&EAOP, pData, sizeof (EAOP));
      rc = MY_PROBEBUF(PB_OPREAD,
         (PBYTE)EAOP.fpGEAList,
         (USHORT)EAOP.fpGEAList->cbList);
      if (rc)
         goto FS_FINDFIRSTEXIT;
      }
   memset(pData, 0, cbData);

   pFindInfo->pSHInfo = NULL;

   usNumClusters = 0;
   ulDirCluster = FindDirCluster(pVolInfo,
      pcdfsi,
      pcdfsd,
      pName,
      usCurDirEnd,
      RETURN_PARENT_DIR,
      &pSearch,
      &StreamEntry);

   if (ulDirCluster == pVolInfo->ulFatEof)
      {
      rc = ERROR_PATH_NOT_FOUND;
      goto FS_FINDFIRSTEXIT;
      }

   ulCluster = ulDirCluster;

   if (ulCluster == 1)
      {
      // FAT12/FAT16 root directory size (contiguous)
      USHORT usRootDirSize = pVolInfo->BootSect.bpb.RootDirEntries * sizeof(DIRENTRY) /
         (pVolInfo->SectorsPerCluster * pVolInfo->BootSect.bpb.BytesPerSector);
      usNumClusters = (pVolInfo->BootSect.bpb.RootDirEntries * sizeof(DIRENTRY) %
         (pVolInfo->SectorsPerCluster * pVolInfo->BootSect.bpb.BytesPerSector)) ?
         usRootDirSize + 1 : usRootDirSize;
      }
   else
      {
      if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
         {
         PSHOPENINFO pSHInfo = malloc(sizeof(SHOPENINFO));
         SetSHInfo1(pVolInfo, (PDIRENTRY1)&StreamEntry, pSHInfo);
         pFindInfo->pSHInfo = pSHInfo;
         }

      while (ulCluster && ulCluster != pVolInfo->ulFatEof)
         {
         usNumClusters++;
         //ulCluster = GetNextCluster(pVolInfo, pFindInfo->pSHInfo, ulCluster);
         ulCluster = GetNextCluster(pVolInfo, NULL, ulCluster);
         }
      }

   usNumBlocks = usNumClusters * (pVolInfo->ulClusterSize / pVolInfo->ulBlockSize);
   ulNeededSpace = sizeof (FINFO) + (usNumBlocks - 1) * sizeof (ULONG);
   ulNeededSpace += pVolInfo->ulBlockSize;

   GetProcInfo(&ProcInfo, sizeof ProcInfo);


   pFindInfo->pInfo = (PFINFO)malloc((size_t)ulNeededSpace);
   if (!pFindInfo->pInfo)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto FS_FINDFIRSTEXIT;
      }

   memset(pFindInfo->pInfo, 0, (size_t)ulNeededSpace);

   if (!pVolInfo->pFindInfo)
      pVolInfo->pFindInfo = pFindInfo->pInfo;
   else
      {
      pNext = (PFINFO)pVolInfo->pFindInfo;
      while (pNext->pNextEntry)
         pNext = (PFINFO)pNext->pNextEntry;
      pNext->pNextEntry = pFindInfo->pInfo;
      }

   memcpy(&pFindInfo->pInfo->EAOP, &EAOP, sizeof (EAOP));
   pFindInfo->usEntriesPerBlock = pVolInfo->ulBlockSize / sizeof (DIRENTRY);
   pFindInfo->usBlockIndex = 0;
   pFindInfo->pInfo->rgClusters[0] = ulDirCluster;
   pFindInfo->usTotalBlocks = usNumBlocks;
   pFindInfo->pInfo->pDirEntries =
      (PDIRENTRY)(&pFindInfo->pInfo->rgClusters[usNumClusters]);

   if (f32Parms.fMessageActive & LOG_FIND)
      Message("pInfo at %lX, pDirEntries at %lX",
         pFindInfo->pInfo, pFindInfo->pInfo->pDirEntries);

   pFindInfo->pInfo->pNextEntry = NULL;
   memcpy(&pFindInfo->pInfo->ProcInfo, &ProcInfo, sizeof (PROCINFO));

   strcpy(pFindInfo->pInfo->szSearch, pSearch);
   FSH_UPPERCASE(pFindInfo->pInfo->szSearch, sizeof pFindInfo->pInfo->szSearch, pFindInfo->pInfo->szSearch);

   if (ulDirCluster == 1)
      // FAT12/FAT16 root directory case
      pFindInfo->ulMaxEntry = pVolInfo->BootSect.bpb.RootDirEntries;
   else
      pFindInfo->ulMaxEntry = ((ULONG)pVolInfo->ulClusterSize / sizeof (DIRENTRY)) * usNumClusters;

   if (!GetBlock(pVolInfo, pFindInfo, 0))
      {
      rc = ERROR_SYS_INTERNAL;
      goto FS_FINDFIRSTEXIT;
      }

   pFindInfo->ulCurEntry = 0;

   if (usAttr & 0x0040)
      {
      pFindInfo->fLongNames = TRUE;
      usAttr &= ~0x0040;
      }
   else
      pFindInfo->fLongNames = FALSE;

   pFindInfo->bMustAttr = (BYTE)(usAttr >> 8);
   usAttr |= (FILE_READONLY | FILE_ARCHIVED);
   usAttr &= (FILE_READONLY | FILE_HIDDEN | FILE_SYSTEM | FILE_DIRECTORY | FILE_ARCHIVED);
   pFindInfo->bAttr = (BYTE)~usAttr;

   if (usLevel == FIL_QUERYEASFROMLIST || usLevel == FIL_QUERYEASFROMLISTL)
      {
      memcpy(pData, &pFindInfo->pInfo->EAOP, sizeof (EAOP));
      pData += sizeof (EAOP);
      cbData -= sizeof (EAOP);
      }

   rc = 0;
   for (usIndex = 0; usIndex < usEntriesWanted; usIndex++)
      {
      PULONG pulOrdinal;

      if (usFlags == FF_GETPOS)
         {
         if (cbData < sizeof (ULONG))
            {
            rc = ERROR_BUFFER_OVERFLOW;
            break;
            }
         pulOrdinal = (PULONG)pData;
         pData += sizeof (ULONG);
         cbData -= sizeof (ULONG);
         }

      rc = FillDirEntry(pVolInfo, &pData, &cbData, pFindInfo, usLevel);
      if (!rc || (rc == ERROR_EAS_DIDNT_FIT && usIndex == 0))
         {
         if (usFlags == FF_GETPOS)
            *pulOrdinal = pFindInfo->ulCurEntry - 1;
         }
      if (rc)
         break;
      }


   if ((rc == ERROR_NO_MORE_FILES ||
        rc == ERROR_BUFFER_OVERFLOW ||
        rc == ERROR_EAS_DIDNT_FIT)
       && usIndex > 0)
      rc = 0;

   if (rc == ERROR_EAS_DIDNT_FIT && usIndex == 0)
      usIndex = 1;

   *pcMatch = usIndex;


FS_FINDFIRSTEXIT:

   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_FINDFIRST returned %d (%d entries)",
         rc, *pcMatch);

   if (rc && rc != ERROR_EAS_DIDNT_FIT)
      {
      FS_FINDCLOSE(pfsfsi, pfsfsd);
      }

   _asm pop es;

   return rc;
}


/******************************************************************
*
******************************************************************/
int far pascal _loadds FS_FINDFROMNAME(
    struct fsfsi far * pfsfsi,      /* pfsfsi   */
    struct fsfsd far * pfsfsd,      /* pfsfsd   */
    char far * pData,           /* pData    */
    unsigned short cbData,      /* cbData   */
    unsigned short far * pcMatch,   /* pcMatch  */
    unsigned short usLevel,     /* level    */
    unsigned long ulPosition,       /* position */
    char far * pName,           /* pName    */
    unsigned short usFlags      /* flags    */
)
{
PFINDINFO pFindInfo = (PFINDINFO)pfsfsd;

   pName = pName;

   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_FINDFROMNAME, curpos = %lu, requested %lu",
         pFindInfo->ulCurEntry, ulPosition);

   pFindInfo->ulCurEntry = ulPosition + 1;
   return FS_FINDNEXT(pfsfsi, pfsfsd, pData, cbData, pcMatch, usLevel, usFlags);
}

/******************************************************************
*
******************************************************************/
int far pascal _loadds FS_FINDNEXT(
    struct fsfsi far * pfsfsi,      /* pfsfsi   */
    struct fsfsd far * pfsfsd,      /* pfsfsd   */
    char far * pData,           /* pData    */
    unsigned short cbData,      /* cbData   */
    unsigned short far * pcMatch,   /* pcMatch  */
    unsigned short usLevel,     /* level    */
    unsigned short usFlags      /* flag     */
)
{
PVOLINFO pVolInfo;
PFINDINFO pFindInfo = (PFINDINFO)pfsfsd;
USHORT rc;
USHORT usIndex;
USHORT usNeededLen;
USHORT usEntriesWanted;

   _asm push es;

   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_FINDNEXT, level %u, cbData %u, MaxEntries %u", usLevel, cbData, *pcMatch);

   usEntriesWanted = *pcMatch;
   *pcMatch = 0;

   pVolInfo = GetVolInfo(pfsfsi->fsi_hVPB);

   if (! pVolInfo)
      {
      rc = ERROR_INVALID_DRIVE;
      goto FS_FINDNEXTEXIT;
      }

   if (pVolInfo->fFormatInProgress)
      {
      rc = ERROR_ACCESS_DENIED;
      goto FS_FINDNEXTEXIT;
      }

   if (IsDriveLocked(pVolInfo))
      {
      rc = ERROR_DRIVE_LOCKED;
      goto FS_FINDNEXTEXIT;
      }

   switch (usLevel)
      {
      case FIL_STANDARD         :
         usNeededLen = sizeof (FILEFNDBUF) - CCHMAXPATHCOMP;
         break;
      case FIL_STANDARDL        :
         usNeededLen = sizeof (FILEFNDBUF3L) - CCHMAXPATHCOMP;
         break;
      case FIL_QUERYEASIZE      :
         usNeededLen = sizeof (FILEFNDBUF2) - CCHMAXPATHCOMP;
         break;
      case FIL_QUERYEASIZEL     :
         usNeededLen = sizeof (FILEFNDBUF4L) - CCHMAXPATHCOMP;
         break;
      case FIL_QUERYEASFROMLIST :
         usNeededLen = sizeof (EAOP) + sizeof (FILEFNDBUF3) + sizeof (ULONG);
         break;
      case FIL_QUERYEASFROMLISTL :
         usNeededLen = sizeof (EAOP) + sizeof (FILEFNDBUF3L) + sizeof (ULONG);
         break;
      default                   :
         rc = ERROR_INVALID_FUNCTION;
         goto FS_FINDNEXTEXIT;
      }

   if (usFlags == FF_GETPOS)
      usNeededLen += sizeof (ULONG);

   if (cbData < usNeededLen)
      {
      rc = ERROR_BUFFER_OVERFLOW;
      goto FS_FINDNEXTEXIT;
      }

   rc = MY_PROBEBUF(PB_OPWRITE, pData, cbData);
   if (rc)
      {
      Message("FAT32: Protection VIOLATION in FS_FINDNEXT!");
      goto FS_FINDNEXTEXIT;
      }

   if (usLevel == FIL_QUERYEASFROMLIST || usLevel == FIL_QUERYEASFROMLISTL)
      {
      memcpy(&pFindInfo->pInfo->EAOP, pData, sizeof (EAOP));
      rc = MY_PROBEBUF(PB_OPREAD,
         (PBYTE)pFindInfo->pInfo->EAOP.fpGEAList,
         (USHORT)pFindInfo->pInfo->EAOP.fpGEAList->cbList);
      if (rc)
         goto FS_FINDNEXTEXIT;
      }

   memset(pData, 0, cbData);

   if (usLevel == FIL_QUERYEASFROMLIST || usLevel == FIL_QUERYEASFROMLISTL)
      {
      memcpy(pData, &pFindInfo->pInfo->EAOP, sizeof (EAOP));
      pData += sizeof (EAOP);
      cbData -= sizeof (EAOP);
      }

   rc = 0;
   for (usIndex = 0; usIndex < usEntriesWanted; usIndex++)
      {
      PULONG pulOrdinal = NULL;

      if (usFlags == FF_GETPOS)
         {
         if (cbData < sizeof (ULONG))
            {
            rc = ERROR_BUFFER_OVERFLOW;
            break;
            }

         pulOrdinal = (PULONG)pData;
         pData += sizeof (ULONG);
         cbData -= sizeof (ULONG);
         }

      rc = FillDirEntry(pVolInfo, &pData, &cbData, pFindInfo, usLevel);
      if (!rc || (rc == ERROR_EAS_DIDNT_FIT && usIndex == 0))
         {
         if (usFlags == FF_GETPOS)
            *pulOrdinal = pFindInfo->ulCurEntry - 1;
         }
      if (rc)
         break;
      }

   if ((rc == ERROR_NO_MORE_FILES ||
        rc == ERROR_BUFFER_OVERFLOW ||
        rc == ERROR_EAS_DIDNT_FIT)
      && usIndex > 0)
      rc = 0;
   if (rc == ERROR_EAS_DIDNT_FIT && usIndex == 0)
      usIndex = 1;

   *pcMatch = usIndex;


FS_FINDNEXTEXIT:

   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_FINDNEXT returned %d (%d entries)",
         rc, *pcMatch);

   _asm pop es;

   return rc;
}


/******************************************************************
*
******************************************************************/
USHORT FillDirEntry0(PVOLINFO pVolInfo, PBYTE * ppData, PUSHORT pcbData, PFINDINFO pFindInfo, USHORT usLevel)
{
// FAT12/FAT16/FAT32 case
BYTE szLongName[FAT32MAXPATHCOMP];
BYTE szUpperName[FAT32MAXPATHCOMP];
BYTE szShortName[14];
PBYTE pStart = *ppData;
USHORT rc;
DIRENTRY _huge * pDir;
BYTE bCheck1;
USHORT usBlockIndex;

   memset(szLongName, 0, sizeof szLongName);
   pDir = &pFindInfo->pInfo->pDirEntries[pFindInfo->ulCurEntry % pFindInfo->usEntriesPerBlock];
   bCheck1 = 0;
   while (pFindInfo->ulCurEntry < pFindInfo->ulMaxEntry)
      {
      memset(szShortName, 0, sizeof(szShortName)); // vs

      usBlockIndex = (USHORT)(pFindInfo->ulCurEntry / pFindInfo->usEntriesPerBlock);
      if (usBlockIndex != pFindInfo->usBlockIndex)
         {
         if (!GetBlock(pVolInfo, pFindInfo, usBlockIndex))
            return ERROR_SYS_INTERNAL;
         pDir = &pFindInfo->pInfo->pDirEntries[pFindInfo->ulCurEntry % pFindInfo->usEntriesPerBlock];
         }

      if (pDir->bFileName[0] && pDir->bFileName[0] != DELETED_ENTRY)
         {
         if (pDir->bAttr == FILE_LONGNAME)
            {
            fGetLongName(pDir, szLongName, sizeof szLongName, &bCheck1);
            }
         else if ((pDir->bAttr & FILE_VOLID) != FILE_VOLID)
            {
            if (!(pDir->bAttr & pFindInfo->bAttr))
               {
               BYTE bCheck2 = GetVFATCheckSum(pDir);
               MakeName(pDir, szShortName, sizeof szShortName);
               FSH_UPPERCASE(szShortName, sizeof szShortName, szShortName);

               rc = 0;

               if (f32Parms.fEAS && bCheck2 == bCheck1 && strlen(szLongName))
                  if (IsEASFile(szLongName))
                     rc = 1;

               if (f32Parms.fMessageActive & LOG_FIND)
                  {
                  if (bCheck2 != bCheck1 && strlen(szLongName))
                     Message("Invalid LFN entry found: %s", szLongName);
                  }

               if (bCheck2 != bCheck1 ||
                  !strlen(szLongName))
               {
                  strcpy(szLongName, szShortName);

                  /* support for the FAT32 variation of WinNT family */
                  if( HAS_WINNT_EXT( pDir->fEAS ))
                  {
                        PBYTE pDot = strchr( szLongName, '.' );

                        if( HAS_WINNT_EXT_NAME( pDir->fEAS )) /* name part is lower case */
                        {
                            if( pDot )
                                *pDot = 0;

                            strlwr( szLongName );

                            if( pDot )
                                *pDot = '.';
                        }

                        if( pDot && HAS_WINNT_EXT_EXT( pDir->fEAS )) /* ext part is lower case */
                            strlwr( pDot + 1 );
                  }
               }

               if (f32Parms.fEAS && IsEASFile(szLongName))
                  rc = 1;

               strcpy(szUpperName, szLongName);
               FSH_UPPERCASE(szUpperName, sizeof szUpperName, szUpperName);

               if( !pFindInfo->fLongNames )
                  strcpy( szLongName, szShortName );

               /*
                  Check for MUST HAVE attributes
               */
               if (!rc && pFindInfo->bMustAttr)
                  {
                  if ((pDir->bAttr & pFindInfo->bMustAttr) != pFindInfo->bMustAttr)
                     rc = 1;
                  }

               if (!rc && strlen(pFindInfo->pInfo->szSearch))
                  {
                  rc = FSH_WILDMATCH(pFindInfo->pInfo->szSearch, szUpperName);
                  if (rc && stricmp(szShortName, szUpperName))
                     rc = FSH_WILDMATCH(pFindInfo->pInfo->szSearch, szShortName);
                  }
               if (!rc && f32Parms.fMessageActive & LOG_FIND)
                  Message("%lu : %s, %s", pFindInfo->ulCurEntry, szLongName, szShortName );

               if (!rc && usLevel == FIL_STANDARD)
                  {
                  PFILEFNDBUF pfFind = (PFILEFNDBUF)*ppData;

                  if (*pcbData < sizeof (FILEFNDBUF) - CCHMAXPATHCOMP + strlen(szLongName) + 1)
                     return ERROR_BUFFER_OVERFLOW;

                  pfFind->fdateCreation = pDir->wCreateDate;
                  pfFind->ftimeCreation = pDir->wCreateTime;
                  pfFind->fdateLastAccess = pDir->wAccessDate;
                  pfFind->fdateLastWrite = pDir->wLastWriteDate;
                  pfFind->ftimeLastWrite = pDir->wLastWriteTime;
                  pfFind->cbFile = pDir->ulFileSize;
                  pfFind->cbFileAlloc =
                     (pfFind->cbFile / pVolInfo->ulClusterSize) * pVolInfo->ulClusterSize +
                     (pfFind->cbFile % pVolInfo->ulClusterSize ? pVolInfo->ulClusterSize : 0);

                  pfFind->attrFile = (USHORT)pDir->bAttr;
                  pfFind->cchName = (BYTE)strlen(szLongName);
                  strcpy(pfFind->achName, szLongName);
                  *ppData = pfFind->achName + pfFind->cchName + 1;
                  (*pcbData) -= *ppData - pStart;
                  pFindInfo->ulCurEntry++;
                  return 0;
                  }
               else if (!rc && usLevel == FIL_STANDARDL)
                  {
                  PFILEFNDBUF3L pfFind = (PFILEFNDBUF3L)*ppData;

                  if (*pcbData < sizeof (FILEFNDBUF3L) - CCHMAXPATHCOMP + strlen(szLongName) + 1)
                     return ERROR_BUFFER_OVERFLOW;

                  pfFind->fdateCreation = pDir->wCreateDate;
                  pfFind->ftimeCreation = pDir->wCreateTime;
                  pfFind->fdateLastAccess = pDir->wAccessDate;
                  pfFind->fdateLastWrite = pDir->wLastWriteDate;
                  pfFind->ftimeLastWrite = pDir->wLastWriteTime;
                  pfFind->cbFile = pDir->ulFileSize;
                  pfFind->cbFileAlloc =
                     (pfFind->cbFile / pVolInfo->ulClusterSize) * pVolInfo->ulClusterSize +
                     (pfFind->cbFile % pVolInfo->ulClusterSize ? pVolInfo->ulClusterSize : 0);

                  pfFind->attrFile = (USHORT)pDir->bAttr;
                  pfFind->cchName = (BYTE)strlen(szLongName);
                  strcpy(pfFind->achName, szLongName);
                  *ppData = pfFind->achName + pfFind->cchName + 1;
                  (*pcbData) -= *ppData - pStart;
                  pFindInfo->ulCurEntry++;
                  return 0;
                  }
               else if (!rc && usLevel == FIL_QUERYEASIZE)
                  {
                  PFILEFNDBUF2 pfFind = (PFILEFNDBUF2)*ppData;

                  if (*pcbData < sizeof (FILEFNDBUF2) - CCHMAXPATHCOMP + strlen(szLongName) + 1)
                     return ERROR_BUFFER_OVERFLOW;

                  pfFind->fdateCreation = pDir->wCreateDate;
                  pfFind->ftimeCreation = pDir->wCreateTime;
                  pfFind->fdateLastAccess = pDir->wAccessDate;
                  pfFind->fdateLastWrite = pDir->wLastWriteDate;
                  pfFind->ftimeLastWrite = pDir->wLastWriteTime;
                  pfFind->cbFile = pDir->ulFileSize;
                  pfFind->cbFileAlloc =
                     (pfFind->cbFile / pVolInfo->ulClusterSize)  +
                     (pfFind->cbFile % pVolInfo->ulClusterSize ? 1 : 0);
                  if (!f32Parms.fEAS || !HAS_EAS( pDir->fEAS ))
                     pfFind->cbList = sizeof pfFind->cbList;
                  else
                     {
                     rc = usGetEASize(pVolInfo, pFindInfo->pInfo->rgClusters[0], NULL,
                        szLongName, &pfFind->cbList);
                     if (rc)
                        pfFind->cbList = 4;
                     rc = 0;
                     }
                  pfFind->attrFile = (USHORT)pDir->bAttr;
                  pfFind->cchName = (BYTE)strlen(szLongName);
                  strcpy(pfFind->achName, szLongName);
                  *ppData = pfFind->achName + pfFind->cchName + 1;
                  (*pcbData) -= *ppData - pStart;
                  pFindInfo->ulCurEntry++;
                  return 0;
                  }
               else if (!rc && usLevel == FIL_QUERYEASIZEL)
                  {
                  PFILEFNDBUF4L pfFind = (PFILEFNDBUF4L)*ppData;

                  if (*pcbData < sizeof (FILEFNDBUF4L) - CCHMAXPATHCOMP + strlen(szLongName) + 1)
                     return ERROR_BUFFER_OVERFLOW;

                  pfFind->fdateCreation = pDir->wCreateDate;
                  pfFind->ftimeCreation = pDir->wCreateTime;
                  pfFind->fdateLastAccess = pDir->wAccessDate;
                  pfFind->fdateLastWrite = pDir->wLastWriteDate;
                  pfFind->ftimeLastWrite = pDir->wLastWriteTime;
                  pfFind->cbFile = pDir->ulFileSize;
                  pfFind->cbFileAlloc =
                     (pfFind->cbFile / pVolInfo->ulClusterSize)  +
                     (pfFind->cbFile % pVolInfo->ulClusterSize ? 1 : 0);
                  if (!f32Parms.fEAS || !HAS_EAS( pDir->fEAS ))
                     pfFind->cbList = sizeof pfFind->cbList;
                  else
                     {
                     rc = usGetEASize(pVolInfo, pFindInfo->pInfo->rgClusters[0], NULL,
                        szLongName, &pfFind->cbList);
                     if (rc)
                        pfFind->cbList = 4;
                     rc = 0;
                     }
                  pfFind->attrFile = (USHORT)pDir->bAttr;
                  pfFind->cchName = (BYTE)strlen(szLongName);
                  strcpy(pfFind->achName, szLongName);
                  *ppData = pfFind->achName + pfFind->cchName + 1;
                  (*pcbData) -= *ppData - pStart;
                  pFindInfo->ulCurEntry++;
                  return 0;
                  }
               else if (!rc && usLevel == FIL_QUERYEASFROMLIST)
                  {
                  PFILEFNDBUF3 pfFind = (PFILEFNDBUF3)*ppData;
                  ULONG ulFeaSize;

                  if (*pcbData < sizeof (FILEFNDBUF3) + sizeof (ULONG) + strlen(szLongName) + 2)
                     return ERROR_BUFFER_OVERFLOW;

                  pfFind->fdateCreation = pDir->wCreateDate;
                  pfFind->ftimeCreation = pDir->wCreateTime;
                  pfFind->fdateLastAccess = pDir->wAccessDate;
                  pfFind->fdateLastWrite = pDir->wLastWriteDate;
                  pfFind->ftimeLastWrite = pDir->wLastWriteTime;
                  pfFind->cbFile = pDir->ulFileSize;
                  pfFind->cbFileAlloc =
                     (pfFind->cbFile / pVolInfo->ulClusterSize) * pVolInfo->ulClusterSize +
                     (pfFind->cbFile % pVolInfo->ulClusterSize ? pVolInfo->ulClusterSize : 0);
                  pfFind->attrFile = (USHORT)pDir->bAttr;
                  *ppData = (PBYTE)(pfFind + 1);
                  (*pcbData) -= *ppData - pStart;

                  if (f32Parms.fEAS && HAS_EAS( pDir->fEAS ))
                     {
                     pFindInfo->pInfo->EAOP.fpFEAList = (PFEALIST)*ppData;
                     pFindInfo->pInfo->EAOP.fpFEAList->cbList =
                        *pcbData - (strlen(szLongName) + 2);

                     rc = usGetEAS(pVolInfo, FIL_QUERYEASFROMLIST,
                        pFindInfo->pInfo->rgClusters[0], NULL,
                        szLongName, &pFindInfo->pInfo->EAOP);
                     if (rc && rc != ERROR_BUFFER_OVERFLOW)
                        return rc;
                     if (rc)
                        {
                        rc = ERROR_EAS_DIDNT_FIT;
                        ulFeaSize = sizeof (ULONG);
                        }
                     else
                        ulFeaSize = pFindInfo->pInfo->EAOP.fpFEAList->cbList;
                     }
                  else
                     {
                     pFindInfo->pInfo->EAOP.fpFEAList = (PFEALIST)*ppData;
                     pFindInfo->pInfo->EAOP.fpFEAList->cbList =
                        *pcbData - (strlen(szLongName) + 2);

                     rc = usGetEmptyEAS(szLongName,&pFindInfo->pInfo->EAOP);

                     if (rc && (rc != ERROR_EAS_DIDNT_FIT))
                        return rc;
                     else if (rc == ERROR_EAS_DIDNT_FIT)
                        ulFeaSize = sizeof(pFindInfo->pInfo->EAOP.fpFEAList->cbList);
                     else
                        ulFeaSize = pFindInfo->pInfo->EAOP.fpFEAList->cbList;
                     }
                  (*ppData) += ulFeaSize;
                  (*pcbData) -= ulFeaSize;

                  /*
                     Length and longname
                  */

                  *(*ppData)++ = (BYTE)strlen(szLongName);
                  (*pcbData)--;
                  strcpy(*ppData, szLongName);

                  (*ppData) += strlen(szLongName) + 1;
                  (*pcbData) -= (strlen(szLongName) + 1);

                  pFindInfo->ulCurEntry++;
                  return rc;
                  }
               else if (!rc && usLevel == FIL_QUERYEASFROMLISTL)
                  {
                  PFILEFNDBUF3L pfFind = (PFILEFNDBUF3L)*ppData;
                  ULONG ulFeaSize;

                  if (*pcbData < sizeof (FILEFNDBUF3L) + sizeof (ULONG) + strlen(szLongName) + 2)
                     return ERROR_BUFFER_OVERFLOW;

                  pfFind->fdateCreation = pDir->wCreateDate;
                  pfFind->ftimeCreation = pDir->wCreateTime;
                  pfFind->fdateLastAccess = pDir->wAccessDate;
                  pfFind->fdateLastWrite = pDir->wLastWriteDate;
                  pfFind->ftimeLastWrite = pDir->wLastWriteTime;
                  pfFind->cbFile = pDir->ulFileSize;
                  pfFind->cbFileAlloc =
                     (pfFind->cbFile / pVolInfo->ulClusterSize) * pVolInfo->ulClusterSize +
                     (pfFind->cbFile % pVolInfo->ulClusterSize ? pVolInfo->ulClusterSize : 0);
                  pfFind->attrFile = (USHORT)pDir->bAttr;
                  *ppData = (PBYTE)(pfFind + 1);
                  (*pcbData) -= *ppData - pStart;

                  if (f32Parms.fEAS && HAS_EAS( pDir->fEAS ))
                     {
                     pFindInfo->pInfo->EAOP.fpFEAList = (PFEALIST)*ppData;
                     pFindInfo->pInfo->EAOP.fpFEAList->cbList =
                        *pcbData - (strlen(szLongName) + 2);

                     rc = usGetEAS(pVolInfo, FIL_QUERYEASFROMLISTL,
                        pFindInfo->pInfo->rgClusters[0], NULL,
                        szLongName, &pFindInfo->pInfo->EAOP);
                     if (rc && rc != ERROR_BUFFER_OVERFLOW)
                        return rc;
                     if (rc)
                        {
                        rc = ERROR_EAS_DIDNT_FIT;
                        ulFeaSize = sizeof (ULONG);
                        }
                     else
                        ulFeaSize = pFindInfo->pInfo->EAOP.fpFEAList->cbList;
                     }
                  else
                     {
                     pFindInfo->pInfo->EAOP.fpFEAList = (PFEALIST)*ppData;
                     pFindInfo->pInfo->EAOP.fpFEAList->cbList =
                        *pcbData - (strlen(szLongName) + 2);

                     rc = usGetEmptyEAS(szLongName,&pFindInfo->pInfo->EAOP);

                     if (rc && (rc != ERROR_EAS_DIDNT_FIT))
                        return rc;
                     else if (rc == ERROR_EAS_DIDNT_FIT)
                        ulFeaSize = sizeof(pFindInfo->pInfo->EAOP.fpFEAList->cbList);
                     else
                        ulFeaSize = pFindInfo->pInfo->EAOP.fpFEAList->cbList;
                     }
                  (*ppData) += ulFeaSize;
                  (*pcbData) -= ulFeaSize;

                  /*
                     Length and longname
                  */

                  *(*ppData)++ = (BYTE)strlen(szLongName);
                  (*pcbData)--;
                  strcpy(*ppData, szLongName);

                  (*ppData) += strlen(szLongName) + 1;
                  (*pcbData) -= (strlen(szLongName) + 1);

                  pFindInfo->ulCurEntry++;
                  return rc;
                  }
               }
            memset(szLongName, 0, sizeof szLongName);
            }
         }
      pFindInfo->ulCurEntry++;
      pDir++;
      }
   return ERROR_NO_MORE_FILES;
}

/******************************************************************
*
******************************************************************/
USHORT FillDirEntry1(PVOLINFO pVolInfo, PBYTE * ppData, PUSHORT pcbData, PFINDINFO pFindInfo, USHORT usLevel)
{
// exFAT case
BYTE szLongName[FAT32MAXPATHCOMP];
BYTE szUpperName[FAT32MAXPATHCOMP];
PBYTE pStart = *ppData;
USHORT rc;
DIRENTRY1 _huge * pDir;
USHORT usNameLen;
USHORT usNameHash;
USHORT usBlockIndex;
USHORT usNumSecondary;
ULONGLONG cbFile;
ULONGLONG cbFileAlloc;

   memset(szLongName, 0, sizeof szLongName);
   pDir = (PDIRENTRY1)&pFindInfo->pInfo->pDirEntries[pFindInfo->ulCurEntry % pFindInfo->usEntriesPerBlock];
   while (pFindInfo->ulCurEntry < pFindInfo->ulMaxEntry)
      {
      usBlockIndex = (USHORT)(pFindInfo->ulCurEntry / pFindInfo->usEntriesPerBlock);
      if (usBlockIndex != pFindInfo->usBlockIndex)
         {
         if (!GetBlock(pVolInfo, pFindInfo, usBlockIndex))
            return ERROR_SYS_INTERNAL;
         pDir = (PDIRENTRY1)&pFindInfo->pInfo->pDirEntries[pFindInfo->ulCurEntry % pFindInfo->usEntriesPerBlock];
         }

      if (pDir->bEntryType == ENTRY_TYPE_EOD)
         {
         // end of directory reached
         pFindInfo->ulMaxEntry = pFindInfo->ulCurEntry;
         return ERROR_NO_MORE_FILES;
         }
      else if (pDir->bEntryType & ENTRY_TYPE_IN_USE_STATUS)
         {
         if (pDir->bEntryType == ENTRY_TYPE_FILE_NAME)
            {
            usNumSecondary--;
            fGetLongName1(pDir, szLongName, sizeof szLongName);

            if (!usNumSecondary)
               {
               // last file name entry
               strcpy(szUpperName, szLongName);
               FSH_UPPERCASE(szUpperName, sizeof szUpperName, szUpperName);

               //if (!rc && strlen(pFindInfo->pInfo->szSearch))
               if (strlen(pFindInfo->pInfo->szSearch))
                  {
                  rc = FSH_WILDMATCH(pFindInfo->pInfo->szSearch, szUpperName);
                  //if (rc && stricmp(szShortName, szUpperName))
                  //   rc = FSH_WILDMATCH(pFindInfo->pInfo->szSearch, szShortName);
                  }

               if (!rc && usLevel == FIL_STANDARD)
                  {
                  PFILEFNDBUF pfFind = (PFILEFNDBUF)*ppData;

                  if (*pcbData < sizeof (FILEFNDBUF) - CCHMAXPATHCOMP + strlen(szLongName) + 1)
                     return ERROR_BUFFER_OVERFLOW;

                  pfFind->cchName = (BYTE)usNameLen;
                  strncpy(pfFind->achName, szLongName, usNameLen);
                  *ppData = pfFind->achName + pfFind->cchName + 1;
                  (*pcbData) -= *ppData - pStart;
                  pFindInfo->ulCurEntry++;
                  return 0;
                  }
               else if (!rc && usLevel == FIL_STANDARDL)
                  {
                  PFILEFNDBUF3L pfFind = (PFILEFNDBUF3L)*ppData;

                  if (*pcbData < sizeof (FILEFNDBUF3L) - CCHMAXPATHCOMP + strlen(szLongName) + 1)
                     return ERROR_BUFFER_OVERFLOW;

                  pfFind->cchName = (BYTE)usNameLen;
                  strncpy(pfFind->achName, szLongName, usNameLen);
                  *ppData = pfFind->achName + pfFind->cchName + 1;
                  (*pcbData) -= *ppData - pStart;
                  pFindInfo->ulCurEntry++;
                  return 0;
                  }
               else if (!rc && usLevel == FIL_QUERYEASIZE)
                  {
                  PFILEFNDBUF2 pfFind = (PFILEFNDBUF2)*ppData;

                  if (*pcbData < sizeof (FILEFNDBUF2) - CCHMAXPATHCOMP + strlen(szLongName) + 1)
                     return ERROR_BUFFER_OVERFLOW;

                  pfFind->cchName = (BYTE)usNameLen;
                  strncpy(pfFind->achName, szLongName, usNameLen);
                  *ppData = pfFind->achName + pfFind->cchName + 1;
                  (*pcbData) -= *ppData - pStart;
                  pFindInfo->ulCurEntry++;
                  return 0;
                  }
               else if (!rc && usLevel == FIL_QUERYEASIZEL)
                  {
                  PFILEFNDBUF4L pfFind = (PFILEFNDBUF4L)*ppData;

                  if (*pcbData < sizeof (FILEFNDBUF4L) - CCHMAXPATHCOMP + strlen(szLongName) + 1)
                     return ERROR_BUFFER_OVERFLOW;

                  pfFind->cchName = (BYTE)usNameLen;
                  strncpy(pfFind->achName, szLongName, usNameLen);
                  *ppData = pfFind->achName + pfFind->cchName + 1;
                  (*pcbData) -= *ppData - pStart;
                  pFindInfo->ulCurEntry++;
                  return 0;
                  }
               else if (!rc && usLevel == FIL_QUERYEASFROMLIST)
                  {
                  PFILEFNDBUF3 pfFind = (PFILEFNDBUF3)*ppData;
                  ULONG ulFeaSize;

                  if (*pcbData < sizeof (FILEFNDBUF3) + sizeof (ULONG) + strlen(szLongName) + 2)
                     return ERROR_BUFFER_OVERFLOW;

                  *ppData = (PBYTE)(pfFind + 1);
                  (*pcbData) -= *ppData - pStart;
                  *(*ppData)++ = (BYTE)usNameLen;
                  (*pcbData)--;
                  strncpy(*ppData, szLongName, usNameLen);

                  (*ppData) += usNameLen + 1;
                  (*pcbData) -= (usNameLen + 1);

                  pFindInfo->ulCurEntry++;
                  return rc;
                  }
               else if (!rc && usLevel == FIL_QUERYEASFROMLISTL)
                  {
                  PFILEFNDBUF3L pfFind = (PFILEFNDBUF3L)*ppData;
                  ULONG ulFeaSize;

                  if (*pcbData < sizeof (FILEFNDBUF3L) + sizeof (ULONG) + strlen(szLongName) + 2)
                     return ERROR_BUFFER_OVERFLOW;

                  *ppData = (PBYTE)(pfFind + 1);
                  (*pcbData) -= *ppData - pStart;
                  *(*ppData)++ = (BYTE)usNameLen;
                  (*pcbData)--;
                  strncpy(*ppData, szLongName, usNameLen);

                  (*ppData) += usNameLen + 1;
                  (*pcbData) -= (usNameLen + 1);

                  pFindInfo->ulCurEntry++;
                  return rc;
                  }
               }
            }
         else if (pDir->bEntryType == ENTRY_TYPE_STREAM_EXT)
            {
            usNumSecondary--;
            usNameLen = pDir->u.Stream.bNameLen;
            usNameHash = pDir->u.Stream.usNameHash;

            if (!rc && usLevel == FIL_STANDARD)
               {
               PFILEFNDBUF pfFind = (PFILEFNDBUF)*ppData;
               pfFind->cbFile = pDir->u.Stream.ullValidDataLen;
               pfFind->cbFileAlloc = pDir->u.Stream.ullDataLen;
               }
            else if (!rc && usLevel == FIL_STANDARDL)
               {
               PFILEFNDBUF3L pfFind = (PFILEFNDBUF3L)*ppData;
               pfFind->cbFile = pDir->u.Stream.ullValidDataLen;
               pfFind->cbFileAlloc = pDir->u.Stream.ullDataLen;
               }
            else if (!rc && usLevel == FIL_QUERYEASIZE)
               {
               PFILEFNDBUF2 pfFind = (PFILEFNDBUF2)*ppData;
               pfFind->cbFile = pDir->u.Stream.ullValidDataLen;
               pfFind->cbFileAlloc = pDir->u.Stream.ullDataLen;
               }
            else if (!rc && usLevel == FIL_QUERYEASIZEL)
               {
               PFILEFNDBUF4L pfFind = (PFILEFNDBUF4L)*ppData;
               pfFind->cbFile = pDir->u.Stream.ullValidDataLen;
               pfFind->cbFileAlloc = pDir->u.Stream.ullDataLen;
               }
            else if (!rc && usLevel == FIL_QUERYEASFROMLIST)
               {
               PFILEFNDBUF3 pfFind = (PFILEFNDBUF3)*ppData;
               pfFind->cbFile = pDir->u.Stream.ullValidDataLen;
               pfFind->cbFileAlloc = pDir->u.Stream.ullDataLen;
               }
            else if (!rc && usLevel == FIL_QUERYEASFROMLISTL)
               {
               PFILEFNDBUF3L pfFind = (PFILEFNDBUF3L)*ppData;
               pfFind->cbFile = cbFile;
               pfFind->cbFileAlloc = cbFileAlloc;
               }
            }
         else if (pDir->bEntryType == ENTRY_TYPE_FILE)
            {
            usNumSecondary = pDir->u.File.bSecondaryCount;
            if (!(pDir->u.File.usFileAttr & pFindInfo->bAttr))
               {
               rc = 0;

               //if (f32Parms.fEAS && bCheck2 == bCheck1 && strlen(szLongName))
               if (f32Parms.fEAS && strlen(szLongName))
                  if (IsEASFile(szLongName))
                     rc = 1;

               if (f32Parms.fEAS && IsEASFile(szLongName))
                  rc = 1;

               /*
                  Check for MUST HAVE attributes
               */
               if (!rc && pFindInfo->bMustAttr)
                  {
                  if ((pDir->u.File.usFileAttr & pFindInfo->bMustAttr) != pFindInfo->bMustAttr)
                     rc = 1;
                  }

               if (!rc && usLevel == FIL_STANDARD)
                  {
                  PFILEFNDBUF pfFind = (PFILEFNDBUF)*ppData;

                  pfFind->fdateCreation = GetDate1(pDir->u.File.ulCreateTimestp);
                  pfFind->ftimeCreation = GetTime1(pDir->u.File.ulCreateTimestp);
                  pfFind->fdateLastAccess = GetDate1(pDir->u.File.ulLastAccessedTimestp);
                  pfFind->fdateLastWrite = GetDate1(pDir->u.File.ulLastModifiedTimestp);
                  pfFind->ftimeLastWrite = GetTime1(pDir->u.File.ulLastModifiedTimestp);

                  pfFind->attrFile = pDir->u.File.usFileAttr;
                  }
               else if (!rc && usLevel == FIL_STANDARDL)
                  {
                  PFILEFNDBUF3L pfFind = (PFILEFNDBUF3L)*ppData;

                  pfFind->fdateCreation = GetDate1(pDir->u.File.ulCreateTimestp);
                  pfFind->ftimeCreation = GetTime1(pDir->u.File.ulCreateTimestp);
                  pfFind->fdateLastAccess = GetDate1(pDir->u.File.ulLastAccessedTimestp);
                  pfFind->fdateLastWrite = GetDate1(pDir->u.File.ulLastModifiedTimestp);
                  pfFind->ftimeLastWrite = GetTime1(pDir->u.File.ulLastModifiedTimestp);

                  pfFind->attrFile = pDir->u.File.usFileAttr;
                  }
               else if (!rc && usLevel == FIL_QUERYEASIZE)
                  {
                  PFILEFNDBUF2 pfFind = (PFILEFNDBUF2)*ppData;

                  pfFind->fdateCreation = GetDate1(pDir->u.File.ulCreateTimestp);
                  pfFind->ftimeCreation = GetTime1(pDir->u.File.ulCreateTimestp);
                  pfFind->fdateLastAccess = GetDate1(pDir->u.File.ulLastAccessedTimestp);
                  pfFind->fdateLastWrite = GetDate1(pDir->u.File.ulLastModifiedTimestp);
                  pfFind->ftimeLastWrite = GetTime1(pDir->u.File.ulLastModifiedTimestp);
#if 0
                  if (!f32Parms.fEAS || !HAS_EAS( pDir->fEAS ))
                     pfFind->cbList = sizeof pfFind->cbList;
                  else
                     {
                     rc = usGetEASize(pVolInfo, pFindInfo->pInfo->rgClusters[0], NULL,
                        szLongName, &pfFind->cbList);
                     if (rc)
                        pfFind->cbList = 4;
                     rc = 0;
                     }
#endif
                  pfFind->attrFile = pDir->u.File.usFileAttr;
                  }
               else if (!rc && usLevel == FIL_QUERYEASIZEL)
                  {
                  PFILEFNDBUF4L pfFind = (PFILEFNDBUF4L)*ppData;

                  pfFind->fdateCreation = GetDate1(pDir->u.File.ulCreateTimestp);
                  pfFind->ftimeCreation = GetTime1(pDir->u.File.ulCreateTimestp);
                  pfFind->fdateLastAccess = GetDate1(pDir->u.File.ulLastAccessedTimestp);
                  pfFind->fdateLastWrite = GetDate1(pDir->u.File.ulLastModifiedTimestp);
                  pfFind->ftimeLastWrite = GetTime1(pDir->u.File.ulLastModifiedTimestp);
#if 0
                  if (!f32Parms.fEAS || !HAS_EAS( pDir->fEAS ))
                     pfFind->cbList = sizeof pfFind->cbList;
                  else
                     {
                     rc = usGetEASize(pVolInfo, pFindInfo->pInfo->rgClusters[0], NULL,
                        szLongName, &pfFind->cbList);
                     if (rc)
                        pfFind->cbList = 4;
                     rc = 0;
                     }
#endif
                  pfFind->attrFile = pDir->u.File.usFileAttr;
                  }
               else if (!rc && usLevel == FIL_QUERYEASFROMLIST)
                  {
                  PFILEFNDBUF3 pfFind = (PFILEFNDBUF3)*ppData;
                  ULONG ulFeaSize;

                  pfFind->fdateCreation = GetDate1(pDir->u.File.ulCreateTimestp);
                  pfFind->ftimeCreation = GetTime1(pDir->u.File.ulCreateTimestp);
                  pfFind->fdateLastAccess = GetDate1(pDir->u.File.ulLastAccessedTimestp);
                  pfFind->fdateLastWrite = GetDate1(pDir->u.File.ulLastModifiedTimestp);
                  pfFind->ftimeLastWrite = GetTime1(pDir->u.File.ulLastModifiedTimestp);

                  pfFind->attrFile = pDir->u.File.usFileAttr;
#if 0
                  if (f32Parms.fEAS && HAS_EAS( pDir->fEAS ))
                     {
                     pFindInfo->pInfo->EAOP.fpFEAList = (PFEALIST)*ppData;
                     pFindInfo->pInfo->EAOP.fpFEAList->cbList =
                        *pcbData - (strlen(szLongName) + 2);

                     rc = usGetEAS(pVolInfo, FIL_QUERYEASFROMLIST,
                        pFindInfo->pInfo->rgClusters[0], NULL,
                        szLongName, &pFindInfo->pInfo->EAOP);
                     if (rc && rc != ERROR_BUFFER_OVERFLOW)
                        return rc;
                     if (rc)
                        {
                        rc = ERROR_EAS_DIDNT_FIT;
                        ulFeaSize = sizeof (ULONG);
                        }
                     else
                        ulFeaSize = pFindInfo->pInfo->EAOP.fpFEAList->cbList;
                     }
                  else
                     {
                     pFindInfo->pInfo->EAOP.fpFEAList = (PFEALIST)*ppData;
                     pFindInfo->pInfo->EAOP.fpFEAList->cbList =
                        *pcbData - (strlen(szLongName) + 2);

                     rc = usGetEmptyEAS(szLongName,&pFindInfo->pInfo->EAOP);

                     if (rc && (rc != ERROR_EAS_DIDNT_FIT))
                        return rc;
                     else if (rc == ERROR_EAS_DIDNT_FIT)
                        ulFeaSize = sizeof(pFindInfo->pInfo->EAOP.fpFEAList->cbList);
                     else
                        ulFeaSize = pFindInfo->pInfo->EAOP.fpFEAList->cbList;
                     }
                  (*ppData) += ulFeaSize;
                  (*pcbData) -= ulFeaSize;
#endif
                  }
               else if (!rc && usLevel == FIL_QUERYEASFROMLISTL)
                  {
                  PFILEFNDBUF3L pfFind = (PFILEFNDBUF3L)*ppData;
                  ULONG ulFeaSize;

                  pfFind->fdateCreation = GetDate1(pDir->u.File.ulCreateTimestp);
                  pfFind->ftimeCreation = GetTime1(pDir->u.File.ulCreateTimestp);
                  pfFind->fdateLastAccess = GetDate1(pDir->u.File.ulLastAccessedTimestp);
                  pfFind->fdateLastWrite = GetDate1(pDir->u.File.ulLastModifiedTimestp);
                  pfFind->ftimeLastWrite = GetTime1(pDir->u.File.ulLastModifiedTimestp);

                  pfFind->attrFile = pDir->u.File.usFileAttr;
#if 0
                  if (f32Parms.fEAS && HAS_EAS( pDir->fEAS ))
                     {
                     pFindInfo->pInfo->EAOP.fpFEAList = (PFEALIST)*ppData;
                     pFindInfo->pInfo->EAOP.fpFEAList->cbList =
                        *pcbData - (strlen(szLongName) + 2);

                     rc = usGetEAS(pVolInfo, FIL_QUERYEASFROMLISTL,
                        pFindInfo->pInfo->rgClusters[0], NULL,
                        szLongName, &pFindInfo->pInfo->EAOP);
                     if (rc && rc != ERROR_BUFFER_OVERFLOW)
                        return rc;
                     if (rc)
                        {
                        rc = ERROR_EAS_DIDNT_FIT;
                        ulFeaSize = sizeof (ULONG);
                        }
                     else
                        ulFeaSize = pFindInfo->pInfo->EAOP.fpFEAList->cbList;
                     }
                  else
                     {
                     pFindInfo->pInfo->EAOP.fpFEAList = (PFEALIST)*ppData;
                     pFindInfo->pInfo->EAOP.fpFEAList->cbList =
                        *pcbData - (strlen(szLongName) + 2);

                     rc = usGetEmptyEAS(szLongName,&pFindInfo->pInfo->EAOP);

                     if (rc && (rc != ERROR_EAS_DIDNT_FIT))
                        return rc;
                     else if (rc == ERROR_EAS_DIDNT_FIT)
                        ulFeaSize = sizeof(pFindInfo->pInfo->EAOP.fpFEAList->cbList);
                     else
                        ulFeaSize = pFindInfo->pInfo->EAOP.fpFEAList->cbList;
                     }
                  (*ppData) += ulFeaSize;
                  (*pcbData) -= ulFeaSize;
#endif
                  }
               }
            memset(szLongName, 0, sizeof szLongName);
            }
         }
      pFindInfo->ulCurEntry++;
      pDir++;
      }
   return ERROR_NO_MORE_FILES;
}

/******************************************************************
*
******************************************************************/
USHORT FillDirEntry(PVOLINFO pVolInfo, PBYTE * ppData, PUSHORT pcbData, PFINDINFO pFindInfo, USHORT usLevel)
{
   if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
      return FillDirEntry0(pVolInfo, ppData, pcbData, pFindInfo, usLevel);
   else
      return FillDirEntry1(pVolInfo, ppData, pcbData, pFindInfo, usLevel);
}

VOID MakeName(PDIRENTRY pDir, PSZ pszName, USHORT usMax)
{
PSZ p;
BYTE szExtention[4];

   memset(pszName, 0, usMax);
   strncpy(pszName, pDir->bFileName, 8);
   p = pszName + strlen(pszName);
   while (p > pszName && *(p-1) == 0x20)
      p--;
   *p = 0;

   memset(szExtention, 0, sizeof szExtention);
   strncpy(szExtention, pDir->bExtention, 3);
   p = szExtention + strlen(szExtention);
   while (p > szExtention && *(p-1) == 0x20)
      p--;
   *p = 0;
   if (strlen(szExtention))
      {
      strcat(pszName, ".");
      strcat(pszName, szExtention);
      }
}


BOOL fGetLongName(PDIRENTRY pDir, PSZ pszName, USHORT wMax, PBYTE pbCheck)
{
BYTE szLongName[30] = "";
USHORT uniName[15] = {0};
USHORT wNameSize;
USHORT usIndex;
PLNENTRY pLN = (PLNENTRY)pDir;

   memset(szLongName, 0, sizeof szLongName);
   memset(uniName, 0, sizeof uniName);

   wNameSize = 0;
   if (pLN->bVFATCheckSum != *pbCheck)
      {
      memset(pszName, 0, wMax);
      *pbCheck = pLN->bVFATCheckSum;
      }

   for (usIndex = 0; usIndex < 5; usIndex ++)
      {
      if (pLN->usChar1[usIndex] != 0xFFFF)
         uniName[wNameSize++] = pLN->usChar1[usIndex];
      }
   for (usIndex = 0; usIndex < 6; usIndex ++)
      {
      if (pLN->usChar2[usIndex] != 0xFFFF)
         uniName[wNameSize++] = pLN->usChar2[usIndex];
      }
   for (usIndex = 0; usIndex < 2; usIndex ++)
      {
      if (pLN->usChar3[usIndex] != 0xFFFF)
         uniName[wNameSize++] = pLN->usChar3[usIndex];
      }

   Translate2OS2(uniName, szLongName, sizeof szLongName);

   wNameSize = strlen( szLongName );
   if (strlen(pszName) + wNameSize > wMax)
      return FALSE;

   memmove(pszName + wNameSize, pszName, strlen(pszName) + 1);
   memcpy(pszName, szLongName, wNameSize);
   return TRUE;
}

BOOL fGetLongName1(PDIRENTRY1 pDir, PSZ pszName, USHORT wMax)
{
BYTE szLongName[30] = {0};
USHORT uniName[15] = {0};
USHORT wNameSize;
USHORT usIndex;

   memset(szLongName, 0, sizeof szLongName);
   memset(uniName, 0, sizeof uniName);

   wNameSize = 0;

   for (usIndex = 0; (usIndex < 15) && pDir->u.FileName.bFileName[usIndex]; usIndex ++)
      {
      uniName[wNameSize++] = pDir->u.FileName.bFileName[usIndex];
      }

   Translate2OS2(uniName, szLongName, sizeof(szLongName) / 2);

   wNameSize = min(15, strlen( szLongName ));
   if (strlen(pszName) + wNameSize > wMax)
      return FALSE;

   strncpy(pszName + strlen(pszName), szLongName, wNameSize);
   return TRUE;
}

FDATE GetDate1(TIMESTAMP ts)
{
   FDATE date = {0};

   date.day = ts.day;
   date.month = ts.month;
   date.year = ts.year;

   return date;
}

FTIME GetTime1(TIMESTAMP ts)
{
   FTIME time = {0};

   time.twosecs = ts.seconds / 2;
   time.minutes = ts.minutes;
   time.hours = ts.hour;

   return time;
}

TIMESTAMP SetTimeStamp(FDATE date, FTIME time)
{
   TIMESTAMP ts;

   ts.seconds = time.twosecs * 2;
   ts.minutes = time.minutes;
   ts.hour = time.hours;

   ts.day = date.day;
   ts.month = date.month;
   ts.year = date.year;

   return ts;
}


/******************************************************************
*
******************************************************************/
int far pascal _loadds FS_FINDNOTIFYCLOSE( unsigned short usHandle)
{
   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_FINDNOTIFYCLOSE - NOT SUPPORTED");
   return ERROR_NOT_SUPPORTED;

   usHandle = usHandle;
}

/******************************************************************
*
******************************************************************/
int far pascal _loadds FS_FINDNOTIFYFIRST(
    struct cdfsi far * pcdfsi,      /* pcdfsi   */
    struct cdfsd far * pcdfsd,      /* pcdfsd   */
    char far * pName,           /* pName    */
    unsigned short usCurDirEnd,     /* iCurDirEnd   */
    unsigned short usAttr,      /* attr     */
    unsigned short far * pHandle,   /* pHandle  */
    char far * pData,           /* pData    */
    unsigned short cbData,      /* cbData   */
    unsigned short far * pcMatch,   /* pcMatch  */
    unsigned short usLevel,     /* level    */
    unsigned long   ulTimeOut   /* timeout  */
)
{
   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_FINDNOTIFYFIRST - NOT SUPPORTED");

   return ERROR_NOT_SUPPORTED;

   pcdfsi = pcdfsi;
   pcdfsd = pcdfsd;
   pName = pName;
   usCurDirEnd = usCurDirEnd;
   usAttr = usAttr;
   pHandle = pHandle;
   pData = pData;
   cbData = cbData;
   pcMatch = pcMatch;
   usLevel = usLevel;
   ulTimeOut =ulTimeOut;
}

/******************************************************************
*
******************************************************************/
int far pascal _loadds FS_FINDNOTIFYNEXT(
    unsigned short usHandle,        /* handle   */
    char far * pData,           /* pData    */
    unsigned short cbData,      /* cbData   */
    unsigned short far * pcMatch,   /* pcMatch  */
    unsigned short usInfoLevel,     /* infolevel    */
    unsigned long    ulTimeOut  /* timeout  */
)
{
   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_FINDNOTIFYNEXT - NOT SUPPORTED");

   return ERROR_NOT_SUPPORTED;

   usHandle = usHandle;
   pData = pData;
   cbData = cbData;
   pcMatch = pcMatch;
   usInfoLevel = usInfoLevel;
   ulTimeOut = ulTimeOut;
}

BOOL GetBlock(PVOLINFO pVolInfo, PFINDINFO pFindInfo, USHORT usBlockIndex)
{
USHORT usIndex;
USHORT usBlocksPerCluster = pVolInfo->ulClusterSize / pVolInfo->ulBlockSize;
USHORT usClusterIndex = usBlockIndex / usBlocksPerCluster;
ULONG  ulBlock = usBlockIndex % usBlocksPerCluster;
USHORT usSectorsPerBlock = pVolInfo->SectorsPerCluster /
         (pVolInfo->ulClusterSize / pVolInfo->ulBlockSize);
USHORT usSectorsRead;
ULONG  ulSector;
CHAR   fRootDir = FALSE;

   if (pFindInfo->pInfo->rgClusters[0] == 1)
      {
      // FAT12/FAT16 root directory, special case
      fRootDir = TRUE;
      }

   if (usBlockIndex >= pFindInfo->usTotalBlocks)
      return FALSE;

   if (!pFindInfo->pInfo->rgClusters[usClusterIndex])
      {
      if (fRootDir)
         {
         usIndex = pFindInfo->usBlockIndex / usBlocksPerCluster;
         ulSector = pVolInfo->BootSect.bpb.ReservedSectors +
            pVolInfo->BootSect.bpb.SectorsPerFat * pVolInfo->BootSect.bpb.NumberOfFATs +
            usIndex * pVolInfo->SectorsPerCluster;
         usSectorsRead = usIndex * pVolInfo->SectorsPerCluster;
         }
      for (usIndex = pFindInfo->usBlockIndex / usBlocksPerCluster; usIndex < usClusterIndex; usIndex++)
         {
         if (fRootDir)
            {
            // reading the root directory in case of FAT12/FAT16
            ulSector += pVolInfo->SectorsPerCluster;
            usSectorsRead += pVolInfo->SectorsPerCluster;
            // put sector numbers instead of cluster numbers in case of FAT root dir
            if (usSectorsRead * pVolInfo->BootSect.bpb.BytesPerSector >=
                pVolInfo->BootSect.bpb.RootDirEntries * sizeof(DIRENTRY))
               // root directory ended
               pFindInfo->pInfo->rgClusters[usIndex + 1] = 0;
            else
               // put starting sector numbers, instead of
               // cluster numbers, in case of FAT12/FAT16 root directory
               pFindInfo->pInfo->rgClusters[usIndex + 1] = ulSector;
            }
         else
            {
            pFindInfo->pInfo->rgClusters[usIndex + 1] =
               //GetNextCluster( pVolInfo, pFindInfo->pSHInfo, pFindInfo->pInfo->rgClusters[usIndex] );
               GetNextCluster( pVolInfo, NULL, pFindInfo->pInfo->rgClusters[usIndex] );
            }

         if (!pFindInfo->pInfo->rgClusters[usIndex + 1])
            pFindInfo->pInfo->rgClusters[usIndex + 1] = pVolInfo->ulFatEof;

         if (pFindInfo->pInfo->rgClusters[usIndex + 1] == pVolInfo->ulFatEof)
            return FALSE;
         }
      }

   if (pFindInfo->pInfo->rgClusters[usClusterIndex] == pVolInfo->ulFatEof)
      return FALSE;

   if (fRootDir)
      {
      // FAT12/FAT16 root directory, special case
      ulSector = pVolInfo->BootSect.bpb.ReservedSectors +
         pVolInfo->BootSect.bpb.SectorsPerFat * pVolInfo->BootSect.bpb.NumberOfFATs +
         usClusterIndex * pVolInfo->SectorsPerCluster;
      // use sector read instead, in case of FAT12/FAT16 root directory
      // (because it is not associated with any cluster in this case)
      if (ReadSector( pVolInfo,
         ulSector + ulBlock * usSectorsPerBlock, usSectorsPerBlock, (char *)pFindInfo->pInfo->pDirEntries, 0 ))
         return FALSE;
      }
   else
      {
      if (ReadBlock( pVolInfo,
         pFindInfo->pInfo->rgClusters[usClusterIndex], ulBlock, pFindInfo->pInfo->pDirEntries, 0))
         return FALSE;
      }

   if (fRootDir)
      pFindInfo->pInfo->rgClusters[0] = 1;

   pFindInfo->usBlockIndex = usBlockIndex;
   return TRUE;
}
