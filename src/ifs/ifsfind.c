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
static BOOL GetBlock(PVOLINFO pVolInfo, PFINDINFO pFindInfo, ULONG ulBlockIndex);

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

   MessageL(LOG_FS, "FS_FINDCLOSE%m", 0x0005);

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
ULONG  ulNumClusters;
ULONG  ulNumBlocks;
ULONG  ulCluster;
ULONG  ulDirCluster;
PSZ    pSearch;
PFINFO pNext;
ULONG ulNeededSpace;
USHORT usEntriesWanted;
EAOP   EAOP;
PROCINFO ProcInfo;
PDIRENTRY1 pStreamEntry = NULL;

   _asm push es;

   MessageL(LOG_FS, "FS_FINDFIRST%m for %s attr %X, Level %d, cbData %u, MaxEntries %u",
            0x0006, pName, usAttr, usLevel, cbData, *pcMatch);

   usEntriesWanted = *pcMatch;
   *pcMatch  = 0;

#ifdef EXFAT
   pStreamEntry = (PDIRENTRY1)malloc((size_t)sizeof(DIRENTRY1));
   if (!pStreamEntry)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto FS_FINDFIRSTEXIT;
      }
#endif

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
         /* bare minimum, zero terminator only */
         usNeededLen = sizeof (FILEFNDBUF);
         break;
      case FIL_STANDARDL        :
         /* bare minimum, zero terminator only */
         usNeededLen = sizeof (FILEFNDBUF3L);
         break;
      case FIL_QUERYEASIZE      :
         /* bare minimum, zero terminator only */
         usNeededLen = sizeof (FILEFNDBUF2);
         break;
      case FIL_QUERYEASIZEL     :
         /* bare minimum, zero terminator only */
         usNeededLen = sizeof (FILEFNDBUF4L);
         break;
      case FIL_QUERYEASFROMLIST :
         /* bare minimum, zero terminator only */
         usNeededLen = sizeof (EAOP) + sizeof (FILEFNDBUF3) + EAMINSIZE;
         break;
      case FIL_QUERYEASFROMLISTL :
         /* bare minimum, zero terminator only */
         usNeededLen = sizeof (EAOP) + sizeof (FILEFNDBUF3L) + EAMINSIZE;
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

   /*
      for FIL_QUERYEASFROMLIST and FIL_QUERYEASFROMLISTL
      it is ESSENTIAL to zero out the EAOP occupied structure
      at the beginning, but we have to skip it when we return
      file find data, see further below
   */   
   memset(pData, 0, cbData);

   pFindInfo->pSHInfo = NULL;

   ulNumClusters = 0;
   ulDirCluster = FindDirCluster(pVolInfo,
      pcdfsi,
      pcdfsd,
      pName,
      usCurDirEnd,
      RETURN_PARENT_DIR,
      &pSearch,
      pStreamEntry);

   if (ulDirCluster == pVolInfo->ulFatEof)
      {
      rc = ERROR_PATH_NOT_FOUND;
      goto FS_FINDFIRSTEXIT;
      }

   ulCluster = ulDirCluster;

   if (ulCluster == 1)
      {
      // FAT12/FAT16 root directory size (contiguous)
      ULONG ulRootDirSize = ((ULONG)pVolInfo->BootSect.bpb.RootDirEntries * sizeof(DIRENTRY)) /
         ((ULONG)pVolInfo->SectorsPerCluster * pVolInfo->BootSect.bpb.BytesPerSector);
      ulNumClusters = (((ULONG)pVolInfo->BootSect.bpb.RootDirEntries * sizeof(DIRENTRY)) %
         ((ULONG)pVolInfo->SectorsPerCluster * pVolInfo->BootSect.bpb.BytesPerSector)) ?
         ulRootDirSize + 1 : ulRootDirSize;
      }
   else
      {
#ifdef EXFAT
      if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
         {
         PSHOPENINFO pSHInfo = malloc(sizeof(SHOPENINFO));
         SetSHInfo1(pVolInfo, pStreamEntry, pSHInfo);
         pFindInfo->pSHInfo = pSHInfo;
         }
#endif

      while (ulCluster && ulCluster != pVolInfo->ulFatEof)
         {
         ulNumClusters++;
         ulCluster = GetNextCluster(pVolInfo, pFindInfo->pSHInfo, ulCluster);
         //ulCluster = GetNextCluster(pVolInfo, NULL, ulCluster);
         }
      }

   ulNumBlocks = ulNumClusters * ((ULONG)pVolInfo->ulClusterSize / pVolInfo->ulBlockSize);
   ulNeededSpace = sizeof (FINFO) + (ulNumBlocks - 1) * sizeof (ULONG);
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
   pFindInfo->pInfo->usEntriesPerBlock = (USHORT)(pVolInfo->ulBlockSize / sizeof (DIRENTRY));
   pFindInfo->pInfo->ulBlockIndex = 0;
   pFindInfo->pInfo->rgClusters[0] = ulDirCluster;
   pFindInfo->pInfo->ulTotalBlocks = ulNumBlocks;
   pFindInfo->pInfo->pDirEntries =
      (PDIRENTRY)(&pFindInfo->pInfo->rgClusters[ulNumClusters]);

   MessageL(LOG_FIND, "pInfo%m at %lX, pDirEntries at %lX",
            0x406d, pFindInfo->pInfo, pFindInfo->pInfo->pDirEntries);

   pFindInfo->pInfo->pNextEntry = NULL;
   memcpy(&pFindInfo->pInfo->ProcInfo, &ProcInfo, sizeof (PROCINFO));

   strcpy(pFindInfo->pInfo->szSearch, pSearch);
   FSH_UPPERCASE(pFindInfo->pInfo->szSearch, sizeof pFindInfo->pInfo->szSearch, pFindInfo->pInfo->szSearch);

   if (ulDirCluster == 1)
      // FAT12/FAT16 root directory case
      pFindInfo->pInfo->ulMaxEntry = pVolInfo->BootSect.bpb.RootDirEntries;
   else
      pFindInfo->pInfo->ulMaxEntry = ((ULONG)pVolInfo->ulClusterSize / sizeof (DIRENTRY)) * ulNumClusters;

   if (!GetBlock(pVolInfo, pFindInfo, 0))
      {
      rc = ERROR_SYS_INTERNAL;
      goto FS_FINDFIRSTEXIT;
      }

   pFindInfo->pInfo->ulCurEntry = 0;

   if (usAttr & 0x0040)
      {
      pFindInfo->pInfo->fLongNames = TRUE;
      usAttr &= ~0x0040;
      }
   else
      pFindInfo->pInfo->fLongNames = FALSE;

   pFindInfo->pInfo->bMustAttr = (BYTE)(usAttr >> 8);
   usAttr |= (FILE_READONLY | FILE_ARCHIVED);
   usAttr &= (FILE_READONLY | FILE_HIDDEN | FILE_SYSTEM | FILE_DIRECTORY | FILE_ARCHIVED);
   pFindInfo->pInfo->bAttr = (BYTE)~usAttr;

   if (usLevel == FIL_QUERYEASFROMLIST || usLevel == FIL_QUERYEASFROMLISTL)
      {
      memcpy(pData, &pFindInfo->pInfo->EAOP, sizeof (EAOP));
      /*
         skip the EAOP occupied room
         when returning data
      */   
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
            *pulOrdinal = pFindInfo->pInfo->ulCurEntry - 1;
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
#ifdef EXFAT
   if (pStreamEntry)
      free(pStreamEntry);
#endif

   if (rc && rc != ERROR_EAS_DIDNT_FIT)
      {
      FS_FINDCLOSE(pfsfsi, pfsfsd);
      }

   MessageL(LOG_FS, "FS_FINDFIRST%m returned %d (%d entries)",
            0x8006, rc, *pcMatch);

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

   MessageL(LOG_FS, "FS_FINDFROMNAME%m, curpos = %lu, requested %lu",
            0x0009, pFindInfo->pInfo->ulCurEntry, ulPosition);

   pFindInfo->pInfo->ulCurEntry = ulPosition + 1;
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

   MessageL(LOG_FS, "FS_FINDNEXT%m, level %u, cbData %u, MaxEntries %u",
            0x0008, usLevel, cbData, *pcMatch);

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
         /* bare minimum, zero terminator only */
         usNeededLen = sizeof (FILEFNDBUF);
         break;
      case FIL_STANDARDL        :
         /* bare minimum, zero terminator only */
         usNeededLen = sizeof (FILEFNDBUF3L);
         break;
      case FIL_QUERYEASIZE      :
         /* bare minimum, zero terminator only */
         usNeededLen = sizeof (FILEFNDBUF2);
         break;
      case FIL_QUERYEASIZEL     :
         /* bare minimum, zero terminator only */
         usNeededLen = sizeof (FILEFNDBUF4L);
         break;
      case FIL_QUERYEASFROMLIST :
         /* bare minimum, zero terminator only */
         usNeededLen = sizeof (EAOP) + sizeof (FILEFNDBUF3) + EAMINSIZE;
         break;
      case FIL_QUERYEASFROMLISTL :
         /* bare minimum, zero terminator only */
         usNeededLen = sizeof (EAOP) + sizeof (FILEFNDBUF3L) + EAMINSIZE;
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

   /*
      for FIL_QUERYEASFROMLIST and FIL_QUERYEASFROMLISTL
      it is ESSENTIAL to zero out the EAOP occupied structure
      at the beginning, but we have to skip it when we return
      file find data, see further below
   */   
   memset(pData, 0, cbData);

   if (usLevel == FIL_QUERYEASFROMLIST || usLevel == FIL_QUERYEASFROMLISTL)
      {
      memcpy(pData, &pFindInfo->pInfo->EAOP, sizeof (EAOP));
      /*
         skip the EAOP occupied room
         when returning data
      */   
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
            *pulOrdinal = pFindInfo->pInfo->ulCurEntry - 1;
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

   MessageL(LOG_FS, "FS_FINDNEXT%m returned %d (%d entries)",
            0x8008, rc, *pcMatch);

   _asm pop es;

   return rc;
}


/******************************************************************
*
******************************************************************/
USHORT FillDirEntry(PVOLINFO pVolInfo, PBYTE * ppData, PUSHORT pcbData, PFINDINFO pFindInfo, USHORT usLevel)
{
//BYTE szLongName[FAT32MAXPATHCOMP];
PSZ szLongName;
//BYTE szUpperName[FAT32MAXPATHCOMP];
PSZ szUpperName;
PBYTE pStart = *ppData;
USHORT rc;
ULONG  ulBlockIndex;
ULONGLONG ullSize;

   szLongName = (PSZ)malloc((size_t)FAT32MAXPATHCOMP);
   if (!szLongName)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto FillDirEntryExit;
      }

   szUpperName = (PSZ)malloc((size_t)FAT32MAXPATHCOMP);
   if (!szUpperName)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto FillDirEntryExit;
      }

#ifdef EXFAT
   if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
#endif
      {
      // FAT12/FAT16/FAT32 case
      DIRENTRY _huge * pDir;
      BYTE szShortName[14];
      BYTE bCheck1;

      //memset(szLongName, 0, sizeof szLongName);
      memset(szLongName, 0, FAT32MAXPATHCOMP);
      pDir = &pFindInfo->pInfo->pDirEntries[pFindInfo->pInfo->ulCurEntry % pFindInfo->pInfo->usEntriesPerBlock];
      bCheck1 = 0;
      while (pFindInfo->pInfo->ulCurEntry < pFindInfo->pInfo->ulMaxEntry)
         {
         memset(szShortName, 0, sizeof(szShortName)); // vs

         ulBlockIndex = (USHORT)(pFindInfo->pInfo->ulCurEntry / pFindInfo->pInfo->usEntriesPerBlock);
         if (ulBlockIndex != pFindInfo->pInfo->ulBlockIndex)
            {
            if (!GetBlock(pVolInfo, pFindInfo, ulBlockIndex))
               {
               rc = ERROR_SYS_INTERNAL;
               goto FillDirEntryExit;
               }
            pDir = &pFindInfo->pInfo->pDirEntries[pFindInfo->pInfo->ulCurEntry % pFindInfo->pInfo->usEntriesPerBlock];
            }

         if (pDir->bFileName[0] && pDir->bFileName[0] != DELETED_ENTRY)
            {
            if (pDir->bAttr == FILE_LONGNAME)
               {
               //fGetLongName(pDir, szLongName, sizeof szLongName, &bCheck1);
               fGetLongName(pDir, szLongName, FAT32MAXPATHCOMP, &bCheck1);
               }
            else if ((pDir->bAttr & FILE_VOLID) != FILE_VOLID)
               {
               if (!(pDir->bAttr & pFindInfo->pInfo->bAttr))
                  {
                  BYTE bCheck2 = GetVFATCheckSum(pDir);
                  MakeName(pDir, szShortName, sizeof szShortName);
                  FSH_UPPERCASE(szShortName, sizeof szShortName, szShortName);

                  rc = 0;

                  if (f32Parms.fEAS && bCheck2 == bCheck1 && strlen(szLongName))
                     if (IsEASFile(szLongName))
                        rc = 1;

                  if (bCheck2 != bCheck1 && strlen(szLongName))
                     MessageL(LOG_FIND, "Invalid LFN entry found%m: %s", 0x406e, szLongName);

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
                  //FSH_UPPERCASE(szUpperName, sizeof szUpperName, szUpperName);
                  FSH_UPPERCASE(szUpperName, FAT32MAXPATHCOMP, szUpperName);

                  if( !pFindInfo->pInfo->fLongNames )
                     strcpy( szLongName, szShortName );

                  /*
                     Check for MUST HAVE attributes
                  */
                  if (!rc && pFindInfo->pInfo->bMustAttr)
                     {
                     if ((pDir->bAttr & pFindInfo->pInfo->bMustAttr) != pFindInfo->pInfo->bMustAttr)
                        rc = 1;
                     }

                  if (!rc && strlen(pFindInfo->pInfo->szSearch))
                     {
                     rc = FSH_WILDMATCH(pFindInfo->pInfo->szSearch, szUpperName);
                     if (rc && stricmp(szShortName, szUpperName))
                        rc = FSH_WILDMATCH(pFindInfo->pInfo->szSearch, szShortName);
                     }
                  if (!rc)
                     MessageL(LOG_FIND, "%m %lu : %s, %s", 0x406f, pFindInfo->pInfo->ulCurEntry, szLongName, szShortName );

                  if (!rc && usLevel == FIL_STANDARD)
                     {
                     PFILEFNDBUF pfFind = (PFILEFNDBUF)*ppData;

                     //if (*pcbData < sizeof (FILEFNDBUF) - CCHMAXPATHCOMP + strlen(szLongName) + 1)
                     if (*pcbData < sizeof(FILEFNDBUF) + strlen(szLongName))
                        {
                        rc = ERROR_BUFFER_OVERFLOW;
                        goto FillDirEntryExit;
                        }

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
                     pFindInfo->pInfo->ulCurEntry++;
                     rc = 0;
                     goto FillDirEntryExit;
                     }
                  else if (!rc && usLevel == FIL_STANDARDL)
                     {
                     PFILEFNDBUF3L pfFind = (PFILEFNDBUF3L)*ppData;

                     //if (*pcbData < sizeof (FILEFNDBUF3L) - CCHMAXPATHCOMP + strlen(szLongName) + 1)
                     if (*pcbData < sizeof(FILEFNDBUF3L) + strlen(szLongName))
                        {
                        rc = ERROR_BUFFER_OVERFLOW;
                        goto FillDirEntryExit;
                        }

                     pfFind->fdateCreation = pDir->wCreateDate;
                     pfFind->ftimeCreation = pDir->wCreateTime;
                     pfFind->fdateLastAccess = pDir->wAccessDate;
                     pfFind->fdateLastWrite = pDir->wLastWriteDate;
                     pfFind->ftimeLastWrite = pDir->wLastWriteTime;

                     FileGetSize(pVolInfo, pDir, pFindInfo->pInfo->rgClusters[0], pFindInfo->pSHInfo, szLongName, &ullSize);
#ifdef INCL_LONGLONG
                     pfFind->cbFile = ullSize;
                     pfFind->cbFileAlloc =
                        (pfFind->cbFile / pVolInfo->ulClusterSize) * pVolInfo->ulClusterSize +
                        (pfFind->cbFile % pVolInfo->ulClusterSize ? pVolInfo->ulClusterSize : 0);
#else
                     {
                     LONGLONG llRest;

                     iAssignUL(&pfFind->cbFile, ullSize);
                     pfFind->cbFileAlloc = iDivUL(pfFind->cbFile, pVolInfo->ulClusterSize);
                     pfFind->cbFileAlloc = iMulUL(pfFind->cbFileAlloc, pVolInfo->ulClusterSize);
                     llRest = iModUL(pfFind->cbFile, pVolInfo->ulClusterSize);

                     if (iNeqUL(llRest, 0))
                        iAssignUL(&llRest, pVolInfo->ulClusterSize);
                     else
                        iAssignUL(&llRest, 0);

                     pfFind->cbFileAlloc = iAdd(pfFind->cbFileAlloc, llRest);
                     }
#endif

                     pfFind->attrFile = (USHORT)pDir->bAttr;
                     pfFind->cchName = (BYTE)strlen(szLongName);
                     strcpy(pfFind->achName, szLongName);
                     *ppData = pfFind->achName + pfFind->cchName + 1;
                     (*pcbData) -= *ppData - pStart;
                     pFindInfo->pInfo->ulCurEntry++;
                     rc = 0;
                     goto FillDirEntryExit;
                     }
                  else if (!rc && usLevel == FIL_QUERYEASIZE)
                     {
                     PFILEFNDBUF2 pfFind = (PFILEFNDBUF2)*ppData;

                     //if (*pcbData < sizeof (FILEFNDBUF2) - CCHMAXPATHCOMP + strlen(szLongName) + 1)
                     if (*pcbData < sizeof (FILEFNDBUF2) + strlen(szLongName))
                        {
                        rc = ERROR_BUFFER_OVERFLOW;
                        goto FillDirEntryExit;
                        }

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
                        /* HACK: what we need to return here
                           is the FEALIST size of the list
                           that would be produced by usGetEmptyEAs !
                           for the time being: just tell the user
                           to allocate some reasonably sized amount of memory
                        */   
                        pfFind->cbList = EAMINSIZE;
                        //pfFind->cbList = sizeof pfFind->cbList;
                     else
                        {
                        rc = usGetEASize(pVolInfo, pFindInfo->pInfo->rgClusters[0], NULL,
                                         szLongName, &pfFind->cbList);
                        if (rc)
                           /* HACK: what we need to return here
                              is the FEALIST size of the list
                              that would be produced by usGetEmptyEAs !
                              for the time being: just tell the user
                              to allocate some reasonably sized amount of memory
                           */   
                           pfFind->cbList = EAMINSIZE;
                           //pfFind->cbList = 4;
                        rc = 0;
                        }
                     pfFind->attrFile = (USHORT)pDir->bAttr;
                     pfFind->cchName = (BYTE)strlen(szLongName);
                     strcpy(pfFind->achName, szLongName);
                     *ppData = pfFind->achName + pfFind->cchName + 1;
                     (*pcbData) -= *ppData - pStart;
                     pFindInfo->pInfo->ulCurEntry++;
                     rc = 0;
                     goto FillDirEntryExit;
                     }
                  else if (!rc && usLevel == FIL_QUERYEASIZEL)
                     {
                     PFILEFNDBUF4L pfFind = (PFILEFNDBUF4L)*ppData;

                     //if (*pcbData < sizeof (FILEFNDBUF4L) - CCHMAXPATHCOMP + strlen(szLongName) + 1)
                     if (*pcbData < sizeof (FILEFNDBUF4L) + strlen(szLongName))
                        {
                        return ERROR_BUFFER_OVERFLOW;
                        goto FillDirEntryExit;
                        }

                     pfFind->fdateCreation = pDir->wCreateDate;
                     pfFind->ftimeCreation = pDir->wCreateTime;
                     pfFind->fdateLastAccess = pDir->wAccessDate;
                     pfFind->fdateLastWrite = pDir->wLastWriteDate;
                     pfFind->ftimeLastWrite = pDir->wLastWriteTime;

                     FileGetSize(pVolInfo, pDir, pFindInfo->pInfo->rgClusters[0], pFindInfo->pSHInfo, szLongName, &ullSize);
#ifdef INCL_LONGLONG
                     pfFind->cbFile = ullSize;
                     pfFind->cbFileAlloc =
                        (pfFind->cbFile / pVolInfo->ulClusterSize) * pVolInfo->ulClusterSize +
                        (pfFind->cbFile % pVolInfo->ulClusterSize ? pVolInfo->ulClusterSize : 0);
#else
                     {
                     LONGLONG llRest;

                     iAssignUL(&pfFind->cbFile, ullSize);
                     pfFind->cbFileAlloc = iDivUL(pfFind->cbFile, pVolInfo->ulClusterSize);
                     pfFind->cbFileAlloc = iMulUL(pfFind->cbFileAlloc, pVolInfo->ulClusterSize);
                     llRest = iModUL(pfFind->cbFile, pVolInfo->ulClusterSize);

                     if (iNeqUL(llRest, 0))
                        iAssignUL(&llRest, pVolInfo->ulClusterSize);
                     else
                        iAssignUL(&llRest, 0);

                     pfFind->cbFileAlloc = iAdd(pfFind->cbFileAlloc, llRest);
                     }
#endif
                     if (!f32Parms.fEAS || !HAS_EAS( pDir->fEAS ))
                        /* HACK: what we need to return here
                           is the FEALIST size of the list
                           that would be produced by usGetEmptyEAs !
                           for the time being: just tell the user
                           to allocate some reasonably sized amount of memory
                        */   
                        pfFind->cbList = EAMINSIZE;
                        //pfFind->cbList = sizeof pfFind->cbList;
                     else
                        {
                        rc = usGetEASize(pVolInfo, pFindInfo->pInfo->rgClusters[0], NULL,
                           szLongName, &pfFind->cbList);
                        if (rc)
                           /* HACK: what we need to return here
                              is the FEALIST size of the list
                              that would be produced by usGetEmptyEAs !
                              for the time being: just tell the user
                              to allocate some reasonably sized amount of memory
                           */   
                           pfFind->cbList = EAMINSIZE;
                           //pfFind->cbList = 4;
                        rc = 0;
                        }
                     pfFind->attrFile = (USHORT)pDir->bAttr;
                     pfFind->cchName = (BYTE)strlen(szLongName);
                     strcpy(pfFind->achName, szLongName);
                     *ppData = pfFind->achName + pfFind->cchName + 1;
                     (*pcbData) -= *ppData - pStart;
                     pFindInfo->pInfo->ulCurEntry++;
                     rc = 0;
                     goto FillDirEntryExit;
                     }
                  else if (!rc && usLevel == FIL_QUERYEASFROMLIST)
                     {
                     PFILEFNDBUF3 pfFind = (PFILEFNDBUF3)*ppData;
                     ULONG ulFeaSize;

                     //if (*pcbData < sizeof (FILEFNDBUF3) + sizeof (ULONG) + strlen(szLongName) + 2)
                     if (*pcbData < sizeof (FILEFNDBUF3) + EAMINSIZE + strlen(szLongName))
                        {
                        rc = ERROR_BUFFER_OVERFLOW;
                        goto FillDirEntryExit;
                        }

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
                     //*ppData = (PBYTE)(pfFind + 1);
                     *ppData = ((PBYTE)pfFind + FIELDOFFSET(FILEFNDBUF3,cchName));
                     (*pcbData) -= *ppData - pStart;

                     //
                     if (usGetEASize(pVolInfo, pFindInfo->pInfo->rgClusters[0], NULL,
                                     szLongName, &ulFeaSize))
                     if ((ULONG)*pcbData < (ulFeaSize + strlen(szLongName) + 2))
                        {
                        rc = ERROR_BUFFER_OVERFLOW;
                        goto FillDirEntryExit;
                        }
   
                     pFindInfo->pInfo->EAOP.fpFEAList = (PFEALIST)*ppData;
                     pFindInfo->pInfo->EAOP.fpFEAList->cbList =
                        *pcbData - (strlen(szLongName) + 2);
                     //

                     if (f32Parms.fEAS && HAS_EAS( pDir->fEAS ))
                        {
                        //pFindInfo->pInfo->EAOP.fpFEAList = (PFEALIST)*ppData;
                        //pFindInfo->pInfo->EAOP.fpFEAList->cbList =
                        //   *pcbData - (strlen(szLongName) + 2);

                        rc = usGetEAS(pVolInfo, FIL_QUERYEASFROMLIST,
                                      pFindInfo->pInfo->rgClusters[0], NULL,
                                      szLongName, &pFindInfo->pInfo->EAOP);
                        if (rc && rc != ERROR_BUFFER_OVERFLOW)
                           goto FillDirEntryExit;
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
                        //pFindInfo->pInfo->EAOP.fpFEAList = (PFEALIST)*ppData;
                        //pFindInfo->pInfo->EAOP.fpFEAList->cbList =
                        //   *pcbData - (strlen(szLongName) + 2);

                        rc = usGetEmptyEAS(szLongName,&pFindInfo->pInfo->EAOP);

                        if (rc && (rc != ERROR_EAS_DIDNT_FIT))
                           goto FillDirEntryExit;
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

                     pFindInfo->pInfo->ulCurEntry++;
                     goto FillDirEntryExit;
                     }
                  else if (!rc && usLevel == FIL_QUERYEASFROMLISTL)
                     {
                     PFILEFNDBUF3L pfFind = (PFILEFNDBUF3L)*ppData;
                     ULONG ulFeaSize;

                     //if (*pcbData < sizeof (FILEFNDBUF3L) + sizeof (ULONG) + strlen(szLongName) + 2)
                     if (*pcbData < sizeof (FILEFNDBUF3L) + EAMINSIZE + strlen(szLongName))
                        {
                        rc = ERROR_BUFFER_OVERFLOW;
                        goto FillDirEntryExit;
                        }

                     pfFind->fdateCreation = pDir->wCreateDate;
                     pfFind->ftimeCreation = pDir->wCreateTime;
                     pfFind->fdateLastAccess = pDir->wAccessDate;
                     pfFind->fdateLastWrite = pDir->wLastWriteDate;
                     pfFind->ftimeLastWrite = pDir->wLastWriteTime;

                     FileGetSize(pVolInfo, pDir, pFindInfo->pInfo->rgClusters[0], pFindInfo->pSHInfo, szLongName, &ullSize);
#ifdef INCL_LONGLONG
                     pfFind->cbFile = ullSize;
                     pfFind->cbFileAlloc =
                        (pfFind->cbFile / pVolInfo->ulClusterSize) * pVolInfo->ulClusterSize +
                        (pfFind->cbFile % pVolInfo->ulClusterSize ? pVolInfo->ulClusterSize : 0);
#else
                     {
                     LONGLONG llRest;

                     iAssignUL(&pfFind->cbFile, ullSize);
                     pfFind->cbFileAlloc = iDivUL(pfFind->cbFile, pVolInfo->ulClusterSize);
                     pfFind->cbFileAlloc = iMulUL(pfFind->cbFileAlloc, pVolInfo->ulClusterSize);
                     llRest = iModUL(pfFind->cbFile, pVolInfo->ulClusterSize);

                     if (iNeqUL(llRest, 0))
                        iAssignUL(&llRest, pVolInfo->ulClusterSize);
                     else
                        iAssignUL(&llRest, 0);

                     pfFind->cbFileAlloc = iAdd(pfFind->cbFileAlloc, llRest);
                     }
#endif
                     pfFind->attrFile = (USHORT)pDir->bAttr;
                     //*ppData = (PBYTE)(pfFind + 1);
                     *ppData = ((PBYTE)pfFind + FIELDOFFSET(FILEFNDBUF3L,cchName));
                     (*pcbData) -= *ppData - pStart;

                     //
                     if (usGetEASize(pVolInfo, pFindInfo->pInfo->rgClusters[0], NULL,
                                     szLongName, &ulFeaSize))
                     if ((ULONG)*pcbData < (ulFeaSize + strlen(szLongName) + 2))
                        {
                        rc = ERROR_BUFFER_OVERFLOW;
                        goto FillDirEntryExit;
                        }

                     pFindInfo->pInfo->EAOP.fpFEAList = (PFEALIST)*ppData;
                     pFindInfo->pInfo->EAOP.fpFEAList->cbList =
                        *pcbData - (strlen(szLongName) + 2);
                     //
                     
                     if (f32Parms.fEAS && HAS_EAS( pDir->fEAS ))
                        {
                        //pFindInfo->pInfo->EAOP.fpFEAList = (PFEALIST)*ppData;
                        //pFindInfo->pInfo->EAOP.fpFEAList->cbList =
                        //   *pcbData - (strlen(szLongName) + 2);

                        rc = usGetEAS(pVolInfo, FIL_QUERYEASFROMLISTL,
                           pFindInfo->pInfo->rgClusters[0], NULL,
                           szLongName, &pFindInfo->pInfo->EAOP);
                        if (rc && rc != ERROR_BUFFER_OVERFLOW)
                           goto FillDirEntryExit;
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
                        //pFindInfo->pInfo->EAOP.fpFEAList = (PFEALIST)*ppData;
                        //pFindInfo->pInfo->EAOP.fpFEAList->cbList =
                        //   *pcbData - (strlen(szLongName) + 2);

                        rc = usGetEmptyEAS(szLongName,&pFindInfo->pInfo->EAOP);

                        if (rc && (rc != ERROR_EAS_DIDNT_FIT))
                           goto FillDirEntryExit;
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

                     pFindInfo->pInfo->ulCurEntry++;
                     goto FillDirEntryExit;
                     }
                  }
               //memset(szLongName, 0, sizeof szLongName);
               memset(szLongName, 0, FAT32MAXPATHCOMP);
               }
            }
         pFindInfo->pInfo->ulCurEntry++;
         pDir++;
         }
      rc = ERROR_NO_MORE_FILES;
      goto FillDirEntryExit;
      }
#ifdef EXFAT
   else
      {
      // exFAT case
      DIRENTRY1 _huge * pDir;
      USHORT usNameLen;
      USHORT usNameHash;
      USHORT usNumSecondary;
      BYTE fEAS;
      USHORT attrFile;

      //memset(szLongName, 0, sizeof szLongName);
      memset(szLongName, 0, FAT32MAXPATHCOMP);
      pDir = (PDIRENTRY1)&pFindInfo->pInfo->pDirEntries[pFindInfo->pInfo->ulCurEntry % pFindInfo->pInfo->usEntriesPerBlock];
      while (pFindInfo->pInfo->ulCurEntry < pFindInfo->pInfo->ulMaxEntry)
         {
         ulBlockIndex = (USHORT)(pFindInfo->pInfo->ulCurEntry / pFindInfo->pInfo->usEntriesPerBlock);
         if (ulBlockIndex != pFindInfo->pInfo->ulBlockIndex)
            {
            if (!GetBlock(pVolInfo, pFindInfo, ulBlockIndex))
               {
               rc = ERROR_SYS_INTERNAL;
               goto FillDirEntryExit;
               }
            pDir = (PDIRENTRY1)&pFindInfo->pInfo->pDirEntries[pFindInfo->pInfo->ulCurEntry % pFindInfo->pInfo->usEntriesPerBlock];
            }

         if (pDir->bEntryType == ENTRY_TYPE_EOD)
            {
            // end of directory reached
            pFindInfo->pInfo->ulMaxEntry = pFindInfo->pInfo->ulCurEntry;
            rc = ERROR_NO_MORE_FILES;
            goto FillDirEntryExit;
            }
         else if (pDir->bEntryType & ENTRY_TYPE_IN_USE_STATUS)
            {
            if (pDir->bEntryType == ENTRY_TYPE_FILE_NAME)
               {
               usNumSecondary--;
               //fGetLongName1(pDir, szLongName, sizeof szLongName);
               fGetLongName1(pDir, szLongName, FAT32MAXPATHCOMP);

               if (!usNumSecondary)
                  {
                  // last file name entry
                  strcpy(szUpperName, szLongName);
                  //FSH_UPPERCASE(szUpperName, sizeof szUpperName, szUpperName);
                  FSH_UPPERCASE(szUpperName, FAT32MAXPATHCOMP, szUpperName);

                  rc = 0;

                  //if (!rc && strlen(pFindInfo->pInfo->szSearch))
                  if (strlen(pFindInfo->pInfo->szSearch))
                     {
                     rc = FSH_WILDMATCH(pFindInfo->pInfo->szSearch, szUpperName);
                     //if (rc && stricmp(szShortName, szUpperName))
                     //   rc = FSH_WILDMATCH(pFindInfo->pInfo->szSearch, szShortName);
                     }

                  //if (f32Parms.fEAS && bCheck2 == bCheck1 && strlen(szLongName))
                  if (f32Parms.fEAS && strlen(szLongName))
                     if (IsEASFile(szLongName))
                        rc = 1;

                  if (f32Parms.fEAS && IsEASFile(szLongName))
                     rc = 1;

                  /*
                     Check for MUST HAVE attributes
                  */
                  if (!rc && pFindInfo->pInfo->bMustAttr)
                     {
                     if ((attrFile & pFindInfo->pInfo->bMustAttr) != pFindInfo->pInfo->bMustAttr)
                        rc = 1;
                     }

                  if (!rc && usLevel == FIL_STANDARD)
                     {
                     PFILEFNDBUF pfFind = (PFILEFNDBUF)*ppData;

                     //if (*pcbData < sizeof (FILEFNDBUF) - CCHMAXPATHCOMP + strlen(szLongName) + 1)
                     if (*pcbData < sizeof(FILEFNDBUF) + strlen(szLongName))
                        {
                        rc = ERROR_BUFFER_OVERFLOW;
                        goto FillDirEntryExit;
                        }

                     pfFind->cchName = (BYTE)usNameLen;
                     strncpy(pfFind->achName, szLongName, usNameLen);
                     *ppData = pfFind->achName + pfFind->cchName + 1;
                     (*pcbData) -= *ppData - pStart;
                     pFindInfo->pInfo->ulCurEntry++;
                     rc = 0;
                     goto FillDirEntryExit;
                     }
                  else if (!rc && usLevel == FIL_STANDARDL)
                     {
                     PFILEFNDBUF3L pfFind = (PFILEFNDBUF3L)*ppData;

                     //if (*pcbData < sizeof (FILEFNDBUF3L) - CCHMAXPATHCOMP + strlen(szLongName) + 1)
                     if (*pcbData < sizeof(FILEFNDBUF3L) + strlen(szLongName))
                        {
                        rc = ERROR_BUFFER_OVERFLOW;
                        goto FillDirEntryExit;
                        }

                     pfFind->cchName = (BYTE)usNameLen;
                     strncpy(pfFind->achName, szLongName, usNameLen);
                     *ppData = pfFind->achName + pfFind->cchName + 1;
                     (*pcbData) -= *ppData - pStart;
                     pFindInfo->pInfo->ulCurEntry++;
                     rc = 0;
                     goto FillDirEntryExit;
                     }
                  else if (!rc && usLevel == FIL_QUERYEASIZE)
                     {
                     PFILEFNDBUF2 pfFind = (PFILEFNDBUF2)*ppData;

                     //if (*pcbData < sizeof (FILEFNDBUF2) - CCHMAXPATHCOMP + strlen(szLongName) + 1)
                     if (*pcbData < sizeof (FILEFNDBUF2) + strlen(szLongName))
                        {
                        rc = ERROR_BUFFER_OVERFLOW;
                        goto FillDirEntryExit;
                        }

                     if (!f32Parms.fEAS || !HAS_EAS( fEAS ))
                        /* HACK: what we need to return here
                           is the FEALIST size of the list
                           that would be produced by usGetEmptyEAs !
                           for the time being: just tell the user
                           to allocate some reasonably sized amount of memory
                        */   
                        pfFind->cbList = EAMINSIZE;
                        //pfFind->cbList = sizeof pfFind->cbList;
                     else
                        {
                        rc = usGetEASize(pVolInfo, pFindInfo->pInfo->rgClusters[0], NULL,
                                         szLongName, &pfFind->cbList);
                        if (rc)
                           /* HACK: what we need to return here
                              is the FEALIST size of the list
                              that would be produced by usGetEmptyEAs !
                              for the time being: just tell the user
                              to allocate some reasonably sized amount of memory
                           */   
                           pfFind->cbList = EAMINSIZE;
                           //pfFind->cbList = 4;
                        rc = 0;
                        }

                     pfFind->cchName = (BYTE)usNameLen;
                     strncpy(pfFind->achName, szLongName, usNameLen);
                     *ppData = pfFind->achName + pfFind->cchName + 1;
                     (*pcbData) -= *ppData - pStart;
                     pFindInfo->pInfo->ulCurEntry++;
                     rc = 0;
                     goto FillDirEntryExit;
                     }
                  else if (!rc && usLevel == FIL_QUERYEASIZEL)
                     {
                     PFILEFNDBUF4L pfFind = (PFILEFNDBUF4L)*ppData;

                     //if (*pcbData < sizeof (FILEFNDBUF4L) - CCHMAXPATHCOMP + strlen(szLongName) + 1)
                     if (*pcbData < sizeof (FILEFNDBUF4L) + strlen(szLongName))
                        {
                        rc = ERROR_BUFFER_OVERFLOW;
                        goto FillDirEntryExit;
                        }

                     if (!f32Parms.fEAS || !HAS_EAS( fEAS ))
                        /* HACK: what we need to return here
                           is the FEALIST size of the list
                           that would be produced by usGetEmptyEAs !
                           for the time being: just tell the user
                           to allocate some reasonably sized amount of memory
                        */   
                        pfFind->cbList = EAMINSIZE;
                        //pfFind->cbList = sizeof pfFind->cbList;
                     else
                        {
                        rc = usGetEASize(pVolInfo, pFindInfo->pInfo->rgClusters[0], NULL,
                                         szLongName, &pfFind->cbList);
                        if (rc)
                           /* HACK: what we need to return here
                              is the FEALIST size of the list
                              that would be produced by usGetEmptyEAs !
                              for the time being: just tell the user
                              to allocate some reasonably sized amount of memory
                           */   
                           pfFind->cbList = EAMINSIZE;
                           //pfFind->cbList = 4;
                        rc = 0;
                        }

                     pfFind->cchName = (BYTE)usNameLen;
                     strncpy(pfFind->achName, szLongName, usNameLen);
                     *ppData = pfFind->achName + pfFind->cchName + 1;
                     (*pcbData) -= *ppData - pStart;
                     pFindInfo->pInfo->ulCurEntry++;
                     rc = 0;
                     goto FillDirEntryExit;
                     }
                  else if (!rc && usLevel == FIL_QUERYEASFROMLIST)
                     {
                     PFILEFNDBUF3 pfFind = (PFILEFNDBUF3)*ppData;
                     ULONG ulFeaSize;

                     //if (*pcbData < sizeof (FILEFNDBUF3) + sizeof (ULONG) + strlen(szLongName) + 2)
                     if (*pcbData < sizeof (FILEFNDBUF3) + EAMINSIZE + strlen(szLongName))
                        {
                        rc = ERROR_BUFFER_OVERFLOW;
                        goto FillDirEntryExit;
                        }

                     //*ppData = (PBYTE)(pfFind + 1);
                     *ppData = ((PBYTE)pfFind + FIELDOFFSET(FILEFNDBUF3,cchName));
                     (*pcbData) -= *ppData - pStart;

                     //
                     if (usGetEASize(pVolInfo, pFindInfo->pInfo->rgClusters[0], NULL,
                                     szLongName, &ulFeaSize))
                     if ((ULONG)*pcbData < (ulFeaSize + strlen(szLongName) + 2))
                        {
                        rc = ERROR_BUFFER_OVERFLOW;
                        goto FillDirEntryExit;
                        }

                     pFindInfo->pInfo->EAOP.fpFEAList = (PFEALIST)*ppData;
                     pFindInfo->pInfo->EAOP.fpFEAList->cbList =
                        *pcbData - (strlen(szLongName) + 2);
                     //

                     if (f32Parms.fEAS && HAS_EAS( fEAS ))
                        {
                        //pFindInfo->pInfo->EAOP.fpFEAList = (PFEALIST)*ppData;
                        //pFindInfo->pInfo->EAOP.fpFEAList->cbList =
                        //   *pcbData - (strlen(szLongName) + 2);

                        rc = usGetEAS(pVolInfo, FIL_QUERYEASFROMLIST,
                                      pFindInfo->pInfo->rgClusters[0], NULL,
                                      szLongName, &pFindInfo->pInfo->EAOP);
                        if (rc && rc != ERROR_BUFFER_OVERFLOW)
                           goto FillDirEntryExit;
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
                        //pFindInfo->pInfo->EAOP.fpFEAList = (PFEALIST)*ppData;
                        //pFindInfo->pInfo->EAOP.fpFEAList->cbList =
                        //   *pcbData - (strlen(szLongName) + 2);

                        rc = usGetEmptyEAS(szLongName, &pFindInfo->pInfo->EAOP);

                        if (rc && (rc != ERROR_EAS_DIDNT_FIT))
                           goto FillDirEntryExit;
                        else if (rc == ERROR_EAS_DIDNT_FIT)
                           ulFeaSize = sizeof(pFindInfo->pInfo->EAOP.fpFEAList->cbList);
                        else
                           ulFeaSize = pFindInfo->pInfo->EAOP.fpFEAList->cbList;
                        }
                     (*ppData) += ulFeaSize;
                     (*pcbData) -= ulFeaSize;

                     *(*ppData)++ = (BYTE)usNameLen;
                     (*pcbData)--;

                     strncpy(*ppData, szLongName, usNameLen);

                     (*ppData) += usNameLen + 1;
                     (*pcbData) -= (usNameLen + 1);

                     pFindInfo->pInfo->ulCurEntry++;
                     goto FillDirEntryExit;
                     }
                  else if (!rc && usLevel == FIL_QUERYEASFROMLISTL)
                     {
                     PFILEFNDBUF3L pfFind = (PFILEFNDBUF3L)*ppData;
                     ULONG ulFeaSize;

                     //if (*pcbData < sizeof (FILEFNDBUF3L) + sizeof (ULONG) + strlen(szLongName) + 2)
                     if (*pcbData < sizeof (FILEFNDBUF3L) + EAMINSIZE + strlen(szLongName))
                        {
                        rc = ERROR_BUFFER_OVERFLOW;
                        goto FillDirEntryExit;
                        }

                     //*ppData = (PBYTE)(pfFind + 1);
                     *ppData = ((PBYTE)pfFind + FIELDOFFSET(FILEFNDBUF3L,cchName));
                     (*pcbData) -= *ppData - pStart;

                     //
                     if (usGetEASize(pVolInfo, pFindInfo->pInfo->rgClusters[0], NULL,
                                     szLongName, &ulFeaSize))
                     if ((ULONG)*pcbData < (ulFeaSize + strlen(szLongName) + 2))
                        {
                        rc = ERROR_BUFFER_OVERFLOW;
                        goto FillDirEntryExit;
                        }

                     pFindInfo->pInfo->EAOP.fpFEAList = (PFEALIST)*ppData;
                     pFindInfo->pInfo->EAOP.fpFEAList->cbList =
                        *pcbData - (strlen(szLongName) + 2);
                     //

                     if (f32Parms.fEAS && HAS_EAS( fEAS ))
                        {
                        //pFindInfo->pInfo->EAOP.fpFEAList = (PFEALIST)*ppData;
                        //pFindInfo->pInfo->EAOP.fpFEAList->cbList =
                        //   *pcbData - (strlen(szLongName) + 2);

                        rc = usGetEAS(pVolInfo, FIL_QUERYEASFROMLISTL,
                                      pFindInfo->pInfo->rgClusters[0], NULL,
                                      szLongName, &pFindInfo->pInfo->EAOP);
                        if (rc && rc != ERROR_BUFFER_OVERFLOW)
                           goto FillDirEntryExit;
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
                        //pFindInfo->pInfo->EAOP.fpFEAList = (PFEALIST)*ppData;
                        //pFindInfo->pInfo->EAOP.fpFEAList->cbList =
                        //   *pcbData - (strlen(szLongName) + 2);

                        rc = usGetEmptyEAS(szLongName,&pFindInfo->pInfo->EAOP);

                        if (rc && (rc != ERROR_EAS_DIDNT_FIT))
                           goto FillDirEntryExit;
                        else if (rc == ERROR_EAS_DIDNT_FIT)
                           ulFeaSize = sizeof(pFindInfo->pInfo->EAOP.fpFEAList->cbList);
                        else
                           ulFeaSize = pFindInfo->pInfo->EAOP.fpFEAList->cbList;
                        }
                     (*ppData) += ulFeaSize;
                     (*pcbData) -= ulFeaSize;

                     *(*ppData)++ = (BYTE)usNameLen;
                     (*pcbData)--;

                     strncpy(*ppData, szLongName, usNameLen);

                     (*ppData) += usNameLen + 1;
                     (*pcbData) -= (usNameLen + 1);

                     pFindInfo->pInfo->ulCurEntry++;
                     goto FillDirEntryExit;
                     }
                  }
               }
            else if (pDir->bEntryType == ENTRY_TYPE_STREAM_EXT)
               {
               usNumSecondary--;

               usNameLen = pDir->u.Stream.bNameLen;
               usNameHash = pDir->u.Stream.usNameHash;

               if (usLevel == FIL_STANDARD)
                  {
                  PFILEFNDBUF pfFind = (PFILEFNDBUF)*ppData;
#ifdef INCL_LONGLONG
                  pfFind->cbFile = pDir->u.Stream.ullValidDataLen;
                  pfFind->cbFileAlloc = 
                     (pDir->u.Stream.ullValidDataLen / pVolInfo->ulClusterSize) * pVolInfo->ulClusterSize +
                     ((pDir->u.Stream.ullValidDataLen % pVolInfo->ulClusterSize) ? pVolInfo->ulClusterSize : 0);
#else
                  pfFind->cbFile = pDir->u.Stream.ullValidDataLen.ulLo;
                  pfFind->cbFileAlloc = 
                     (pDir->u.Stream.ullValidDataLen.ulLo / pVolInfo->ulClusterSize) * pVolInfo->ulClusterSize +
                     ((pDir->u.Stream.ullValidDataLen.ulLo % pVolInfo->ulClusterSize) ? pVolInfo->ulClusterSize : 0);
#endif
                  }
               else if (usLevel == FIL_STANDARDL)
                  {
                  PFILEFNDBUF3L pfFind = (PFILEFNDBUF3L)*ppData;
#ifdef INCL_LONGLONG
                  pfFind->cbFile = pDir->u.Stream.ullValidDataLen;
                  pfFind->cbFileAlloc = pDir->u.Stream.ullValidDataLen;
#else
                  iAssign(&pfFind->cbFile, *(PLONGLONG)&pDir->u.Stream.ullValidDataLen);
                  iAssign(&pfFind->cbFileAlloc, *(PLONGLONG)&pDir->u.Stream.ullValidDataLen);
#endif
                  }
               else if (usLevel == FIL_QUERYEASIZE)
                  {
                  PFILEFNDBUF2 pfFind = (PFILEFNDBUF2)*ppData;
#ifdef INCL_LONGLONG
                  pfFind->cbFile = pDir->u.Stream.ullValidDataLen;
                  pfFind->cbFileAlloc = pDir->u.Stream.ullValidDataLen;
#else
                  pfFind->cbFile = pDir->u.Stream.ullValidDataLen.ulLo;
                  pfFind->cbFileAlloc = 
                     (pDir->u.Stream.ullValidDataLen.ulLo / pVolInfo->ulClusterSize) * pVolInfo->ulClusterSize +
                     ((pDir->u.Stream.ullValidDataLen.ulLo % pVolInfo->ulClusterSize) ? pVolInfo->ulClusterSize : 0);
#endif
                  }
               else if (usLevel == FIL_QUERYEASIZEL)
                  {
                  PFILEFNDBUF4L pfFind = (PFILEFNDBUF4L)*ppData;
#ifdef INCL_LONGLONG
                  pfFind->cbFile = pDir->u.Stream.ullValidDataLen;
                  pfFind->cbFileAlloc = pDir->u.Stream.ullValidDataLen;
#else
                  iAssign(&pfFind->cbFile, *(PLONGLONG)&pDir->u.Stream.ullValidDataLen);
                  iAssign(&pfFind->cbFileAlloc, *(PLONGLONG)&pDir->u.Stream.ullValidDataLen);
#endif
                  }
               else if (usLevel == FIL_QUERYEASFROMLIST)
                  {
                  PFILEFNDBUF3 pfFind = (PFILEFNDBUF3)*ppData;
#ifdef INCL_LONGLONG
                  pfFind->cbFile = pDir->u.Stream.ullValidDataLen;
                  pfFind->cbFileAlloc = pDir->u.Stream.ullValidDataLen;
#else
                  pfFind->cbFile = pDir->u.Stream.ullValidDataLen.ulLo;
                  pfFind->cbFileAlloc = 
                     (pDir->u.Stream.ullValidDataLen.ulLo / pVolInfo->ulClusterSize) * pVolInfo->ulClusterSize +
                     ((pDir->u.Stream.ullValidDataLen.ulLo % pVolInfo->ulClusterSize) ? pVolInfo->ulClusterSize : 0);
#endif
                  }
               else if (usLevel == FIL_QUERYEASFROMLISTL)
                  {
                  PFILEFNDBUF3L pfFind = (PFILEFNDBUF3L)*ppData;
#ifdef INCL_LONGLONG
                  pfFind->cbFile = pDir->u.Stream.ullValidDataLen;
                  pfFind->cbFileAlloc = pDir->u.Stream.ullValidDataLen;
#else
                  iAssign(&pfFind->cbFile, *(PLONGLONG)&pDir->u.Stream.ullValidDataLen);
                  iAssign(&pfFind->cbFileAlloc, *(PLONGLONG)&pDir->u.Stream.ullValidDataLen);
#endif
                  }
               }
            else if (pDir->bEntryType == ENTRY_TYPE_FILE)
               {
               usNumSecondary = pDir->u.File.bSecondaryCount;
               fEAS = pDir->u.File.fEAS;

               if (!(pDir->u.File.usFileAttr & pFindInfo->pInfo->bAttr))
                  {
                  if (usLevel == FIL_STANDARD)
                     {
                     PFILEFNDBUF pfFind = (PFILEFNDBUF)*ppData;

                     pfFind->fdateCreation = GetDate1(pDir->u.File.ulCreateTimestp);
                     pfFind->ftimeCreation = GetTime1(pDir->u.File.ulCreateTimestp);
                     pfFind->fdateLastAccess = GetDate1(pDir->u.File.ulLastAccessedTimestp);
                     pfFind->fdateLastWrite = GetDate1(pDir->u.File.ulLastModifiedTimestp);
                     pfFind->ftimeLastWrite = GetTime1(pDir->u.File.ulLastModifiedTimestp);

                     pfFind->attrFile = pDir->u.File.usFileAttr;
                     attrFile = pfFind->attrFile;
                     }
                  else if (usLevel == FIL_STANDARDL)
                     {
                     PFILEFNDBUF3L pfFind = (PFILEFNDBUF3L)*ppData;

                     pfFind->fdateCreation = GetDate1(pDir->u.File.ulCreateTimestp);
                     pfFind->ftimeCreation = GetTime1(pDir->u.File.ulCreateTimestp);
                     pfFind->fdateLastAccess = GetDate1(pDir->u.File.ulLastAccessedTimestp);
                     pfFind->fdateLastWrite = GetDate1(pDir->u.File.ulLastModifiedTimestp);
                     pfFind->ftimeLastWrite = GetTime1(pDir->u.File.ulLastModifiedTimestp);

                     pfFind->attrFile = pDir->u.File.usFileAttr;
                     attrFile = (USHORT)pfFind->attrFile;
                     }
                  else if (usLevel == FIL_QUERYEASIZE)
                     {
                     PFILEFNDBUF2 pfFind = (PFILEFNDBUF2)*ppData;

                     pfFind->fdateCreation = GetDate1(pDir->u.File.ulCreateTimestp);
                     pfFind->ftimeCreation = GetTime1(pDir->u.File.ulCreateTimestp);
                     pfFind->fdateLastAccess = GetDate1(pDir->u.File.ulLastAccessedTimestp);
                     pfFind->fdateLastWrite = GetDate1(pDir->u.File.ulLastModifiedTimestp);
                     pfFind->ftimeLastWrite = GetTime1(pDir->u.File.ulLastModifiedTimestp);

                     pfFind->attrFile = pDir->u.File.usFileAttr;
                     attrFile = pfFind->attrFile;
                     }
                  else if (usLevel == FIL_QUERYEASIZEL)
                     {
                     PFILEFNDBUF4L pfFind = (PFILEFNDBUF4L)*ppData;

                     pfFind->fdateCreation = GetDate1(pDir->u.File.ulCreateTimestp);
                     pfFind->ftimeCreation = GetTime1(pDir->u.File.ulCreateTimestp);
                     pfFind->fdateLastAccess = GetDate1(pDir->u.File.ulLastAccessedTimestp);
                     pfFind->fdateLastWrite = GetDate1(pDir->u.File.ulLastModifiedTimestp);
                     pfFind->ftimeLastWrite = GetTime1(pDir->u.File.ulLastModifiedTimestp);

                     pfFind->attrFile = pDir->u.File.usFileAttr;
                     attrFile = (USHORT)pfFind->attrFile;
                     }
                  else if (usLevel == FIL_QUERYEASFROMLIST)
                     {
                     PFILEFNDBUF3 pfFind = (PFILEFNDBUF3)*ppData;

                     pfFind->fdateCreation = GetDate1(pDir->u.File.ulCreateTimestp);
                     pfFind->ftimeCreation = GetTime1(pDir->u.File.ulCreateTimestp);
                     pfFind->fdateLastAccess = GetDate1(pDir->u.File.ulLastAccessedTimestp);
                     pfFind->fdateLastWrite = GetDate1(pDir->u.File.ulLastModifiedTimestp);
                     pfFind->ftimeLastWrite = GetTime1(pDir->u.File.ulLastModifiedTimestp);

                     pfFind->attrFile = pDir->u.File.usFileAttr;
                     attrFile = pfFind->attrFile;
                     }
                  else if (usLevel == FIL_QUERYEASFROMLISTL)
                     {
                     PFILEFNDBUF3L pfFind = (PFILEFNDBUF3L)*ppData;

                     pfFind->fdateCreation = GetDate1(pDir->u.File.ulCreateTimestp);
                     pfFind->ftimeCreation = GetTime1(pDir->u.File.ulCreateTimestp);
                     pfFind->fdateLastAccess = GetDate1(pDir->u.File.ulLastAccessedTimestp);
                     pfFind->fdateLastWrite = GetDate1(pDir->u.File.ulLastModifiedTimestp);
                     pfFind->ftimeLastWrite = GetTime1(pDir->u.File.ulLastModifiedTimestp);

                     pfFind->attrFile = pDir->u.File.usFileAttr;
                     attrFile = (USHORT)pfFind->attrFile;
                     }
                  }
               //memset(szLongName, 0, sizeof szLongName);
               memset(szLongName, 0, FAT32MAXPATHCOMP);
               }
            }
         pFindInfo->pInfo->ulCurEntry++;
         pDir++;
         }
      rc = ERROR_NO_MORE_FILES;
      goto FillDirEntryExit;
      }
#endif

FillDirEntryExit:
   if (szLongName)
      free(szLongName);

   if (szUpperName)
      free(szUpperName);

   return rc;
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

#ifdef EXFAT

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

   date.day = (USHORT)ts.day;
   date.month = (USHORT)ts.month;
   date.year = (USHORT)ts.year;

   return date;
}

FTIME GetTime1(TIMESTAMP ts)
{
   FTIME time = {0};

   time.twosecs = (USHORT)ts.twosecs;
   time.minutes = (USHORT)ts.minutes;
   time.hours = (USHORT)ts.hour;

   return time;
}

TIMESTAMP SetTimeStamp(FDATE date, FTIME time)
{
   TIMESTAMP ts;
   memset(&ts, 0, sizeof(ts));

   ts.twosecs = time.twosecs;
   ts.minutes = time.minutes;
   ts.hour = time.hours;

   ts.day = date.day;
   ts.month = date.month;
   ts.year = date.year;

   return ts;
}

#endif


/******************************************************************
*
******************************************************************/
int far pascal _loadds FS_FINDNOTIFYCLOSE( unsigned short usHandle)
{
   MessageL(LOG_FS, "FS_FINDNOTIFYCLOSE%m - NOT SUPPORTED", 0x000a);
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
   MessageL(LOG_FS, "FS_FINDNOTIFYFIRST%m - NOT SUPPORTED", 0x000b);

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
   MessageL(LOG_FS, "FS_FINDNOTIFYNEXT%m - NOT SUPPORTED", 0x000c);

   return ERROR_NOT_SUPPORTED;

   usHandle = usHandle;
   pData = pData;
   cbData = cbData;
   pcMatch = pcMatch;
   usInfoLevel = usInfoLevel;
   ulTimeOut = ulTimeOut;
}

BOOL GetBlock(PVOLINFO pVolInfo, PFINDINFO pFindInfo, ULONG ulBlockIndex)
{
USHORT usIndex;
USHORT usBlocksPerCluster = (USHORT)(pVolInfo->ulClusterSize / pVolInfo->ulBlockSize);
USHORT usClusterIndex = ulBlockIndex / usBlocksPerCluster;
ULONG  ulBlock = ulBlockIndex % usBlocksPerCluster;
USHORT usSectorsPerBlock = (USHORT)pVolInfo->SectorsPerCluster /
         (USHORT)(pVolInfo->ulClusterSize / pVolInfo->ulBlockSize);
USHORT usSectorsRead;
ULONG  ulSector;
CHAR   fRootDir = FALSE;

   if (pFindInfo->pInfo->rgClusters[0] == 1)
      {
      // FAT12/FAT16 root directory, special case
      fRootDir = TRUE;
      }

   if (ulBlockIndex >= pFindInfo->pInfo->ulTotalBlocks)
      return FALSE;

   if (!pFindInfo->pInfo->rgClusters[usClusterIndex])
      {
      if (fRootDir)
         {
         usIndex = pFindInfo->pInfo->ulBlockIndex / usBlocksPerCluster;
         ulSector = pVolInfo->BootSect.bpb.ReservedSectors +
            pVolInfo->BootSect.bpb.SectorsPerFat * pVolInfo->BootSect.bpb.NumberOfFATs +
            usIndex * pVolInfo->SectorsPerCluster;
         usSectorsRead = usIndex * (USHORT)pVolInfo->SectorsPerCluster;
         }
      for (usIndex = pFindInfo->pInfo->ulBlockIndex / usBlocksPerCluster; usIndex < usClusterIndex; usIndex++)
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
               GetNextCluster( pVolInfo, pFindInfo->pSHInfo, pFindInfo->pInfo->rgClusters[usIndex] );
               //GetNextCluster( pVolInfo, NULL, pFindInfo->pInfo->rgClusters[usIndex] );
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

   pFindInfo->pInfo->ulBlockIndex = ulBlockIndex;
   return TRUE;
}
