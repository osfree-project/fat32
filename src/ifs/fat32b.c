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

USHORT NameHash(USHORT *pszFilename, int NameLen);
USHORT GetChkSum16(const UCHAR *data, int bytes);
ULONG  GetChkSum32(const UCHAR *data, int bytes);

VOID MarkFreeEntries(PDIRENTRY pDirBlock, ULONG ulSize);
VOID MarkFreeEntries1(PDIRENTRY1 pDirBlock, ULONG ulSize);
PDIRENTRY fSetLongName(PDIRENTRY pDir, PSZ pszName, BYTE bCheck);
PDIRENTRY1 fSetLongName1(PDIRENTRY1 pDir, PSZ pszLongName, PUSHORT pusNameHash);
PDIRENTRY CompactDir(PDIRENTRY pStart, ULONG ulSize, USHORT usNeededEntries);
PDIRENTRY1 CompactDir1(PDIRENTRY1 pStart, ULONG ulSize, USHORT usEntriesNeeded);
USHORT GetFreeEntries(PDIRENTRY pDirBlock, ULONG ulSize);
USHORT GetFreeEntries1(PDIRENTRY1 pDirBlock, ULONG ulSize);
USHORT DBCSStrlen( const PSZ pszStr );


/******************************************************************
*
******************************************************************/
void FileSetSize(PVOLINFO pVolInfo, PDIRENTRY pDirEntry, ULONG ulDirCluster,
                 PSHOPENINFO pDirSHInfo, PSZ pszFile, ULONGLONG ullSize)
{
#ifdef INCL_LONGLONG
   pDirEntry->ulFileSize = ullSize & 0xffffffff;
#else
   pDirEntry->ulFileSize = ullSize.ulLo;
#endif

   if (f32Parms.fFatPlus)
      {
#ifdef INCL_LONGLONG
      {
      ULONGLONG ullTmp;
      ullTmp = (ULONGLONG)(pDirEntry->fEAS);
      ullTmp |= ((ullSize >> 32) & FILE_SIZE_MASK);
      pDirEntry->fEAS = (BYTE)ullTmp;
      }
#else
      {
      ULONG ulTmp;
      ulTmp = (ULONG)(pDirEntry->fEAS);
      ulTmp |= (ullSize.ulHi & FILE_SIZE_MASK_UL);
      pDirEntry->fEAS = (BYTE)ulTmp;
      }
#endif
      pDirEntry->fEAS &= ~FILE_SIZE_EA;
      }

#ifdef INCL_LONGLONG
   if ( f32Parms.fFatPlus && f32Parms.fEAS && (ullSize >> 35) )
#else
   if ( f32Parms.fFatPlus && f32Parms.fEAS && (ullSize.ulHi >> 3) )
#endif
      {
      // write EAs
      EAOP eaop;
      BYTE pBuf[sizeof(FEALIST) + 13 + 4 + sizeof(ULONGLONG)];
      PFEALIST pfealist = (PFEALIST)pBuf;
      APIRET rc;

      pDirEntry->fEAS |= FILE_SIZE_EA | FILE_HAS_EAS;

      memset(&eaop, 0, sizeof(eaop));
      memset(pfealist, 0, sizeof(pBuf));
      eaop.fpFEAList = pfealist;
      pfealist->cbList = sizeof(pBuf);
      pfealist->list[0].fEA = FEA_NEEDEA; // critical EA
      strcpy((PBYTE)((PFEA)pfealist->list + 1), "FAT_PLUS_FSZ");
      pfealist->list[0].cbName  = 13 - 1;
      pfealist->list[0].cbValue = 4 + sizeof(ULONGLONG);
      *(PUSHORT)((PBYTE)((PFEA)pfealist->list + 1) + 13) = EAT_BINARY; // EA type
      *(PUSHORT)((PBYTE)((PFEA)pfealist->list + 1) + 15) = 8;          // length
      *(PULONGLONG)((PBYTE)((PFEA)pfealist->list + 1) + 17) = ullSize; // value

      rc = usModifyEAS(pVolInfo, ulDirCluster, pDirSHInfo, pszFile, &eaop);
      }
}


/******************************************************************
*
******************************************************************/
void FileGetSize(PVOLINFO pVolInfo, PDIRENTRY pDirEntry, ULONG ulDirCluster,
                 PSHOPENINFO pDirSHInfo, PSZ pszFile, PULONGLONG pullSize)
{
#ifdef INCL_LONGLONG
   *pullSize = pDirEntry->ulFileSize;
#else
   AssignUL(pullSize, pDirEntry->ulFileSize);
#endif

   if (f32Parms.fFatPlus)
      {
#ifdef INCL_LONGLONG
      *pullSize |= (((ULONGLONG)(pDirEntry->fEAS) & FILE_SIZE_MASK) << 32);
#else
      pullSize->ulHi |= ((ULONG)(pDirEntry->fEAS) & FILE_SIZE_MASK_UL);
#endif
      }

   if ( f32Parms.fFatPlus && f32Parms.fEAS && (pDirEntry->fEAS & FILE_SIZE_EA) )
      {
      // read EAs
      EAOP eaop;
      BYTE pBuf1[sizeof(GEALIST) + 12];
      PGEALIST pgealist = (PGEALIST)pBuf1;
      BYTE pBuf2[sizeof(FEALIST) + 13 + 4 + sizeof(ULONGLONG)];
      PFEALIST pfealist = (PFEALIST)pBuf2;
      APIRET rc;

      memset(&eaop, 0, sizeof(eaop));
      memset(pgealist, 0, sizeof(pBuf1));
      memset(pfealist, 0, sizeof(pBuf2));
      eaop.fpGEAList = pgealist;
      eaop.fpFEAList = pfealist;
      pgealist->cbList = sizeof(GEALIST) + 12;
      pgealist->list[0].cbName = 12;
      strcpy(pgealist->list[0].szName, "FAT_PLUS_FSZ");
      pfealist->cbList = sizeof(pBuf2);

      rc = usGetEAS(pVolInfo, FIL_QUERYEASFROMLISTL, ulDirCluster, pDirSHInfo, pszFile, &eaop);

      if (rc)
         return;

#ifdef INCL_LONGLONG
      *pullSize = *(PULONGLONG)((PBYTE)((PFEA)pfealist->list + 1) + 17);
#else
      pullSize->ulHi = *((PULONG)((PBYTE)((PFEA)pfealist->list + 1) + 17) + 1);
      pullSize->ulLo = *(PULONG)((PBYTE)((PFEA)pfealist->list + 1) + 17);
#endif
      }
}


/******************************************************************
*
******************************************************************/
ULONG FindDirCluster(PVOLINFO pVolInfo,
   struct cdfsi far * pcdfsi,       /* pcdfsi   */
   struct cdfsd far * pcdfsd,       /* pcdfsd   */
   PSZ pDir,
   USHORT usCurDirEnd,
   USHORT usAttrWanted,
   PSZ *pDirEnd,
   PDIRENTRY1 pStreamEntry)
{
PSZ    szDir;
ULONG  ulCluster;
ULONG  ulCluster2;
PDIRENTRY pDirEntry;
PDIRENTRY1 pDirEntry1;
PSZ    pszPath = NULL;
PSZ    p;

   MessageL(LOG_FUNCS, "FindDirCluster%m for %s, CurDirEnd %u, AttrWanted %u",
            0x0031, pDir, usCurDirEnd, usAttrWanted );

   pszPath = malloc(CCHMAXPATHCOMP + 1);

   if (! pszPath)
      {
      return ERROR_NOT_ENOUGH_MEMORY;
      }

   if (! pVolInfo->hVBP)
      {
      // FAT FS image mounted at some directory
      if (pDir[1] == ':')
         {
         // absolute path starting from a drive letter
         pDir += strlen(pVolInfo->pbMntPoint);
         }
      else if (pDir[0] == '\\')
         {
         // absolute path starting from backslash
         pDir++;
         }
      else
         {
         // relative path
         strcpy(pszPath, pcdfsi->cdi_curdir);
         strcat(pszPath, "\\");
         strcat(pszPath, pDir);
         pDir = pszPath + strlen(pVolInfo->pbMntPoint);
         }
      }

   if (pcdfsi && pVolInfo->hVBP &&
      (pcdfsi->cdi_flags & CDI_ISVALID) &&
      !(pcdfsi->cdi_flags & CDI_ISROOT) &&
      usCurDirEnd != 0xFFFF)
      {
      pDir += usCurDirEnd;
      ulCluster = *(PULONG)pcdfsd;
      }
   else
      {
      ulCluster = pVolInfo->BootSect.bpb.RootDirStrtClus;
#ifdef EXFAT
      if (pStreamEntry)
         {
         // fill the Stream entry for root directory
         memset(pStreamEntry, 0, sizeof(DIRENTRY1));
         if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
            {
            pStreamEntry->u.Stream.bNoFatChain = 0;
            pStreamEntry->u.Stream.ulFirstClus = ulCluster;
            }
         }
#endif
      if (strlen(pDir) >= 2)
         {
         if (pDir[1] == ':')
            pDir += 2;
         }
      }

   pDirEntry = (PDIRENTRY)malloc((size_t)sizeof(DIRENTRY));
   if (!pDirEntry)
      {
      free(pszPath);
      return ERROR_NOT_ENOUGH_MEMORY;
      }
   pDirEntry1 = (PDIRENTRY1)pDirEntry;

   if (*pDir == '\\')
      pDir++;

   p = strrchr(pDir, '\\');
   if (!p)
      p = pDir;
   szDir = (PSZ)malloc((size_t)FAT32MAXPATH);
   if (!szDir)
      {
      free(pszPath);
      free(pDirEntry);
      return pVolInfo->ulFatEof;
      }
   memset(szDir, 0, FAT32MAXPATH);
   memcpy(szDir, pDir, p - pDir);
   if (*p && p != pDir)
      pDir = p + 1;
   else
      pDir = p;

   if (pDirEnd)
      *pDirEnd = pDir;

   ulCluster = FindPathCluster(pVolInfo, ulCluster, szDir, NULL, pDirEntry, pStreamEntry, NULL);
   if (ulCluster == pVolInfo->ulFatEof)
      {
      MessageL(LOG_FUNCS, "FindDirCluster%m for '%s', not found", 0x4002, szDir);
      free(pszPath);
      free(szDir);
      free(pDirEntry);
      return pVolInfo->ulFatEof;
      }
   if ( ulCluster != pVolInfo->ulFatEof &&
#ifdef EXFAT
       ( ( (pVolInfo->bFatType < FAT_TYPE_EXFAT)  && !(pDirEntry->bAttr & FILE_DIRECTORY) ) ||
         ( (pVolInfo->bFatType == FAT_TYPE_EXFAT) && !(pDirEntry1->u.File.usFileAttr & FILE_DIRECTORY) ) ) )
#else
       !(pDirEntry->bAttr & FILE_DIRECTORY) )
#endif
      {
      MessageL(LOG_FUNCS, "FindDirCluster%m for '%s', not a directory", 0x4003, szDir);
      free(pszPath);
      free(szDir);
      free(pDirEntry);
      return pVolInfo->ulFatEof;
      }

   if (*pDir)
      {
      if (usAttrWanted != RETURN_PARENT_DIR && !strpbrk(pDir, "?*"))
         {
         ulCluster2 = FindPathCluster(pVolInfo, ulCluster, pDir, NULL, pDirEntry, pStreamEntry, NULL);
         if ( ulCluster2 != pVolInfo->ulFatEof &&
#ifdef EXFAT
             ( (pVolInfo->bFatType < FAT_TYPE_EXFAT)  && ((pDirEntry->bAttr & usAttrWanted) == usAttrWanted) ) ||
               ( (pVolInfo->bFatType == FAT_TYPE_EXFAT) && ((pDirEntry1->u.File.usFileAttr & usAttrWanted) == usAttrWanted) ) )
#else
             ( (pDirEntry->bAttr & usAttrWanted) == usAttrWanted ) )
#endif
            {
            if (pDirEnd)
               *pDirEnd = pDir + strlen(pDir);
            free(pszPath);
            free(szDir);
            free(pDirEntry);
            return ulCluster2;
            }
         }
      }

   free(pszPath);
   free(szDir);
   free(pDirEntry);
   return ulCluster;
}

#define MODE_START  0
#define MODE_RETURN 1
#define MODE_SCAN   2

/******************************************************************
*
******************************************************************/
ULONG FindPathCluster(PVOLINFO pVolInfo, ULONG ulCluster, PSZ pszPath, PSHOPENINFO pSHInfo,
                      PDIRENTRY pDirEntry, PDIRENTRY1 pDirEntryStream,
                      PSZ pszFullName)
{
PSZ  pszLongName;
PSZ  pszPart;
PSZ  p;
BOOL fFound;
USHORT usMode;
PROCINFO ProcInfo;
USHORT usDirEntries = 0;
USHORT usMaxDirEntries = (USHORT)(pVolInfo->ulBlockSize / sizeof(DIRENTRY));

#ifdef EXFAT
   if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
#endif
      {
      // FAT12/FAT16/FAT32 case
      BYTE szShortName[13];
      PDIRENTRY pDir;
      PDIRENTRY pDirStart;
      PDIRENTRY pDirEnd;
      BYTE   bCheck;
      ULONG  ulSector;
      USHORT usSectorsRead;
      USHORT usSectorsPerBlock;

      MessageL(LOG_FUNCS, "FindPathCluster%m for %s, dircluster %lu", 0x0032, pszPath, ulCluster);

      if (ulCluster == 1)
         {
         // root directory starting sector
         ulSector = pVolInfo->BootSect.bpb.ReservedSectors +
            (ULONG)pVolInfo->BootSect.bpb.SectorsPerFat * pVolInfo->BootSect.bpb.NumberOfFATs;
         usSectorsPerBlock = (USHORT)((ULONG)pVolInfo->SectorsPerCluster /
            ((ULONG)pVolInfo->ulClusterSize / pVolInfo->ulBlockSize));
         usSectorsRead = 0;
         }

      if (pDirEntry)
         {
         memset(pDirEntry, 0, sizeof (DIRENTRY));
         pDirEntry->bAttr = FILE_DIRECTORY;
         }
      if (pszFullName)
         {
         memset(pszFullName, 0, FAT32MAXPATH);
         if (ulCluster == pVolInfo->BootSect.bpb.RootDirStrtClus)
            {
            pszFullName[0] = (BYTE)(pVolInfo->bDrive + 'A');
            pszFullName[1] = ':';
            pszFullName[2] = '\\';
            }
         }

      if (strlen(pszPath) >= 2)
         {
         if (pszPath[1] == ':')
            pszPath += 2;
         }

      pDirStart = malloc((size_t)pVolInfo->ulBlockSize);
      if (!pDirStart)
         {
         Message("FAT32: Not enough memory for cluster in FindPathCluster");
         return pVolInfo->ulFatEof;
         }
      pszLongName = malloc((size_t)FAT32MAXPATHCOMP * 2);
      if (!pszLongName)
         {
         Message("FAT32: Not enough memory for buffers in FindPathCluster");
         free(pDirStart);
         return pVolInfo->ulFatEof;
         }
      memset(pszLongName, 0, FAT32MAXPATHCOMP * 2);
      pszPart = pszLongName + FAT32MAXPATHCOMP;

      usMode = MODE_SCAN;
      GetProcInfo(&ProcInfo, sizeof ProcInfo);
      /*
         Allow EA files to be found!
      */
      if (ProcInfo.usPdb && f32Parms.fEAS && IsEASFile(pszPath))
         ProcInfo.usPdb = 0;

      while (usMode != MODE_RETURN && ulCluster != pVolInfo->ulFatEof)
         {
         usMode = MODE_SCAN;

         if (*pszPath == '\\')
            pszPath++;

         if (!strlen(pszPath))
            break;

         p = strchr(pszPath, '\\');
         if (!p)
            p = pszPath + strlen(pszPath);

         memset(pszPart, 0, FAT32MAXPATHCOMP);
         if (p - pszPath > FAT32MAXPATHCOMP - 1)
            {
            free(pDirStart);
            free(pszLongName);
            return pVolInfo->ulFatEof;
            }

         memcpy(pszPart, pszPath, p - pszPath);
         pszPath = p;

         memset(pszLongName, 0, FAT32MAXPATHCOMP);

         fFound = FALSE;
         while (usMode == MODE_SCAN && ulCluster != pVolInfo->ulFatEof)
            {
            ULONG ulBlock;
            for (ulBlock = 0; ulBlock < pVolInfo->ulClusterSize / pVolInfo->ulBlockSize; ulBlock++)
               {
               if (ulCluster == 1)
                  // reading root directory on FAT12/FAT16
                  ReadSector(pVolInfo, ulSector + ulBlock * usSectorsPerBlock, usSectorsPerBlock, (void *)pDirStart, 0);
               else
                  ReadBlock(pVolInfo, ulCluster, ulBlock, pDirStart, 0);
               pDir    = pDirStart;
               pDirEnd = (PDIRENTRY)((PBYTE)pDirStart + pVolInfo->ulBlockSize);

#ifdef CALL_YIELD
               Yield();
#endif

               usDirEntries = 0;
               //while (usMode == MODE_SCAN && pDir < pDirEnd)
               while (usMode == MODE_SCAN && usDirEntries < usMaxDirEntries)
                  {
                  if (pDir->bAttr == FILE_LONGNAME)
                     {
                     fGetLongName(pDir, pszLongName, FAT32MAXPATHCOMP, &bCheck);
                     }
                  else if ((pDir->bAttr & 0x0F) != FILE_VOLID)
                     {
                     MakeName(pDir, szShortName, sizeof szShortName);
                     FSH_UPPERCASE(szShortName, sizeof szShortName, szShortName);
                     if (strlen(pszLongName) && bCheck != GetVFATCheckSum(pDir))
                        memset(pszLongName, 0, FAT32MAXPATHCOMP);

                      /* support for the FAT32 variation of WinNT family */
                      if( !*pszLongName && HAS_WINNT_EXT( pDir->fEAS ))
                      {
                          PBYTE pDot;

                          MakeName( pDir, pszLongName, sizeof( pszLongName ));
                          pDot = strchr( pszLongName, '.' );

                          if( HAS_WINNT_EXT_NAME( pDir->fEAS )) /* name part is lower case */
                          {
                              if( pDot )
                                  *pDot = 0;

                              strlwr( pszLongName );

                              if( pDot )
                                  *pDot = '.';
                          }

                          if( pDot && HAS_WINNT_EXT_EXT( pDir->fEAS )) /* ext part is lower case */
                              strlwr( pDot + 1 );
                      }

                     if (!strlen(pszLongName))
                        strcpy(pszLongName, szShortName);

                     if (( strlen(pszLongName) && !stricmp(pszPart, pszLongName)) ||
                         !stricmp( pszPart, szShortName ))
                        {
                          if( pszFullName )
                              strcat( pszFullName, pszLongName );
                          fFound = TRUE;
                        }

                     if (fFound)
                        {
                        ulCluster = (ULONG)pDir->wClusterHigh * 0x10000L + pDir->wCluster;
                        ulCluster &= pVolInfo->ulFatEof;
                        if (strlen(pszPath))
                           {
                           if (pDir->bAttr & FILE_DIRECTORY)
                              {
                              if (pszFullName)
                                 strcat(pszFullName, "\\");
                              usMode = MODE_START;
                              break;
                              }
                           ulCluster = pVolInfo->ulFatEof;
                           }
                        else
                           {
                           if (pDirEntry)
                              memcpy(pDirEntry, pDir, sizeof (DIRENTRY));
                           }
                        usMode = MODE_RETURN;
                        break;
                        }
                     memset(pszLongName, 0, FAT32MAXPATHCOMP);
                     }
                  pDir++;
                  usDirEntries++;
                  if (ulCluster == 1 && usDirEntries > pVolInfo->BootSect.bpb.RootDirEntries)
                     break;
                  }
               if (usMode != MODE_SCAN)
                  break;
               }
            if (usMode != MODE_SCAN)
               break;
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
               {
               ulCluster = GetNextCluster(pVolInfo, NULL, ulCluster);
               }
            if (!ulCluster)
               ulCluster = pVolInfo->ulFatEof;
            }
         }
      free(pDirStart);
      free(pszLongName);
      if (ulCluster != pVolInfo->ulFatEof)
         MessageL(LOG_FUNCS, "FindPathCluster%m for %s found cluster %ld", 0x8032, pszPath, ulCluster);
      else
         MessageL(LOG_FUNCS, "FindPathCluster%m for %s returned EOF", 0x8033, pszPath);
      return ulCluster;
      }
#ifdef EXFAT
   else
      {
      // exFAT case
      DIRENTRY1  Dir;
      PDIRENTRY1 pDir;
      PDIRENTRY1 pDirEntry1 = (PDIRENTRY1)pDirEntry;
      PDIRENTRY1 pDirStart;
      PDIRENTRY1 pDirEnd;
      ULONG  ulRet;
      USHORT usNumSecondary;
      USHORT usFileAttr;

      MessageL(LOG_FUNCS, "FindPathCluster%m for %s, dircluster %lu", 0x0032, pszPath, ulCluster);

      if (pDirEntry1)
         {
         memset(pDirEntry1, 0, sizeof (DIRENTRY1));
         pDirEntry1->u.File.usFileAttr = FILE_DIRECTORY;
         }
      if (pszFullName)
         {
         memset(pszFullName, 0, FAT32MAXPATH);
         if (ulCluster == pVolInfo->BootSect.bpb.RootDirStrtClus)
            {
            pszFullName[0] = (BYTE)(pVolInfo->bDrive + 'A');
            pszFullName[1] = ':';
            pszFullName[2] = '\\';
            }
         }

      if (pDirEntryStream)
         memset(pDirEntryStream, 0, sizeof(DIRENTRY1));

      if (strlen(pszPath) >= 2)
         {
         if (pszPath[1] == ':')
            pszPath += 2;
         }

      pDirStart = malloc((size_t)pVolInfo->ulBlockSize);
      if (!pDirStart)
         {
         Message("FAT32: Not enough memory for cluster in FindPathCluster");
         return pVolInfo->ulFatEof;
         }
      pszLongName = malloc((size_t)FAT32MAXPATHCOMP * 2);
      if (!pszLongName)
         {
         Message("FAT32: Not enough memory for buffers in FindPathCluster");
         free(pDirStart);
         return pVolInfo->ulFatEof;
         }
      memset(pszLongName, 0, FAT32MAXPATHCOMP * 2);
      pszPart = pszLongName + FAT32MAXPATHCOMP;

      usMode = MODE_SCAN;
      GetProcInfo(&ProcInfo, sizeof ProcInfo);
      /*
         Allow EA files to be found!
      */
      if (ProcInfo.usPdb && f32Parms.fEAS && IsEASFile(pszPath))
         ProcInfo.usPdb = 0;

      while (usMode != MODE_RETURN && ulCluster != pVolInfo->ulFatEof)
         {
         usMode = MODE_SCAN;

         if (*pszPath == '\\')
            pszPath++;

         if (!strlen(pszPath))
            break;

         p = strchr(pszPath, '\\');
         if (!p)
            p = pszPath + strlen(pszPath);

         memset(pszPart, 0, FAT32MAXPATHCOMP);
         if (p - pszPath > FAT32MAXPATHCOMP - 1)
            {
            free(pDirStart);
            free(pszLongName);
            return pVolInfo->ulFatEof;
            }

         memcpy(pszPart, pszPath, p - pszPath);
         pszPath = p;

         memset(pszLongName, 0, FAT32MAXPATHCOMP);

         fFound = FALSE;
         while (usMode == MODE_SCAN && ulCluster != pVolInfo->ulFatEof)
            {
            ULONG ulBlock;
            for (ulBlock = 0; ulBlock < pVolInfo->ulClusterSize / pVolInfo->ulBlockSize; ulBlock++)
               {
               ReadBlock(pVolInfo, ulCluster, ulBlock, pDirStart, 0);
               pDir    = pDirStart;
               pDirEnd = (PDIRENTRY1)((PBYTE)pDirStart + pVolInfo->ulBlockSize);

#ifdef CALL_YIELD
               Yield();
#endif

               usDirEntries = 0;
               //while (usMode == MODE_SCAN && pDir < pDirEnd)
               while (usMode == MODE_SCAN && usDirEntries < usMaxDirEntries)
                  {
                  if (pDir->bEntryType == ENTRY_TYPE_EOD)
                     {
                     ulCluster = pVolInfo->ulFatEof;
                     usMode = MODE_RETURN;
                     break;
                     }
                  else if (pDir->bEntryType & ENTRY_TYPE_IN_USE_STATUS)
                     {
                     if (pDir->bEntryType == ENTRY_TYPE_FILE_NAME)
                        {
                        usNumSecondary--;
                        fGetLongName1(pDir, pszLongName, FAT32MAXPATHCOMP);

                        if (!usNumSecondary)
                           {
                           //MakeName(pDir, szShortName, sizeof szShortName);
                           //FSH_UPPERCASE(szShortName, sizeof szShortName, szShortName);
                           //if (strlen(pszLongName) && bCheck != GetVFATCheckSum(pDir))
                           //   memset(pszLongName, 0, FAT32MAXPATHCOMP);
#if 0
                           /* support for the FAT32 variation of WinNT family */
                           if( !*pszLongName && HAS_WINNT_EXT( pDir->u.File.fEAS ))
                              {
                              PBYTE pDot;

                              MakeName( pDir, pszLongName, sizeof( pszLongName ));
                              pDot = strchr( pszLongName, '.' );

                              if( HAS_WINNT_EXT_NAME( pDir->u.File.fEAS )) /* name part is lower case */
                                 {
                                 if( pDot )
                                    *pDot = 0;

                                 strlwr( pszLongName );

                                 if( pDot )
                                    *pDot = '.';
                                 }

                              if( pDot && HAS_WINNT_EXT_EXT( pDir->u.File.fEAS )) /* ext part is lower case */
                                 strlwr( pDot + 1 );
                              }
#endif
                           //if (!strlen(pszLongName))
                           //   strcpy(pszLongName, szShortName);

                           if (( strlen(pszLongName) && !stricmp(pszPart, pszLongName))) //||
                           //!stricmp( pszPart, szShortName ))
                              {
                              if( pszFullName )
                                 strcat( pszFullName, pszLongName );
                              fFound = TRUE;
                              }

                           if (fFound)
                              {
                              //ulCluster = (ULONG)pDir->wClusterHigh * 0x10000L + pDir->wCluster;
                              ulCluster = ulRet;
                              if (strlen(pszPath))
                                 {
                                 if (usFileAttr & FILE_DIRECTORY)
                                    {
                                    if (pszFullName)
                                       strcat(pszFullName, "\\");
                                    usMode = MODE_START;
                                    break;
                                    }
                                 ulCluster = pVolInfo->ulFatEof;
                                 }
                              else
                                 {
                                 if (pDirEntry1)
                                    memcpy(pDirEntry1, &Dir, sizeof (DIRENTRY1));
                                 }
                              usMode = MODE_RETURN;
                              break;
                              }
                           }
                        }
                     else if (pDir->bEntryType == ENTRY_TYPE_STREAM_EXT)
                        {
                        usNumSecondary--;
                        ulRet = pDir->u.Stream.ulFirstClus;
                        if (pDirEntryStream)
                           memcpy(pDirEntryStream, pDir, sizeof(DIRENTRY1));
                        }
                     else if (pDir->bEntryType == ENTRY_TYPE_FILE)
                        {
                        usNumSecondary = pDir->u.File.bSecondaryCount;
                        usFileAttr = pDir->u.File.usFileAttr;
                        memcpy(&Dir, pDir, sizeof (DIRENTRY1));
                        memset(pszLongName, 0, FAT32MAXPATHCOMP);
                        }
                     }
                  pDir++;
                  usDirEntries++;
                  }
               if (usMode != MODE_SCAN)
                  break;
               }
            if (usMode != MODE_SCAN)
               break;
            ulCluster = GetNextCluster(pVolInfo, pSHInfo, ulCluster);
            if (!ulCluster)
               ulCluster = pVolInfo->ulFatEof;
            }
         }
      free(pDirStart);
      free(pszLongName);
      if (ulCluster != pVolInfo->ulFatEof)
         MessageL(LOG_FUNCS, "FindPathCluster%m for %s found cluster %ld", 0x8032, pszPath, ulCluster);
      else
         MessageL(LOG_FUNCS, "FindPathCluster%m for %s returned EOF", 0x8033, pszPath);
      return ulCluster;
      }
#endif
}


USHORT TranslateName(PVOLINFO pVolInfo, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo, PSZ pszPath, PSZ pszTarget, USHORT usTranslate)
{
BYTE szShortName[13];
PSZ  pszLongName, pszNumber;
PSZ  pszUpperName;
PSZ  pszUpperPart;
PSZ  pszPart;
PSZ  p;
ULONG  ulFileNo;
USHORT usNum = 0;
PDIRENTRY pDir;
PDIRENTRY pDirStart;
PDIRENTRY pDirEnd;
BOOL fFound;
USHORT usMode;
BYTE   bCheck;
ULONG  ulCluster;
ULONG  ulSector;
USHORT usSectorsRead;
USHORT usSectorsPerBlock;
ULONG  ulDirEntries = 0;
DIRENTRY1 Dir;
USHORT usNumSecondary;
USHORT usFileAttr;
ULONG  ulRet;

   MessageL(LOG_FUNCS, "TranslateName%m: %s", 0x0034, pszPath);

   memset(pszTarget, 0, FAT32MAXPATH);
   if (strlen(pszPath) >= 2)
      {
      if (pszPath[1] == ':')
         {
         memcpy(pszTarget, pszPath, 2);
         pszTarget += 2;
         pszPath += 2;
         }
      }

   pDirStart = malloc((size_t)pVolInfo->ulBlockSize);
   if (!pDirStart)
      {
      Message("FAT32: Not enough memory for cluster in TranslateName");
      return ERROR_NOT_ENOUGH_MEMORY;
      }
   pszLongName = malloc((size_t)FAT32MAXPATHCOMP * 4);
   if (!pszLongName)
      {
      Message("FAT32: Not enough memory for buffers in TranslateName");
      free(pDirStart);
      return ERROR_NOT_ENOUGH_MEMORY;
      }
   memset(pszLongName, 0, FAT32MAXPATHCOMP * 4);

   pszPart      = pszLongName + FAT32MAXPATHCOMP;
   pszUpperPart = pszPart + FAT32MAXPATHCOMP;
   pszUpperName = pszUpperPart + FAT32MAXPATHCOMP;

   if (usTranslate == TRANSLATE_AUTO)
      {
      if (IsDosSession())
         usTranslate = TRANSLATE_SHORT_TO_LONG;
      else
         usTranslate = TRANSLATE_LONG_TO_SHORT;
      }

   if (ulDirCluster)
      ulCluster = ulDirCluster;
   else
      ulCluster = pVolInfo->BootSect.bpb.RootDirStrtClus;

   if (ulCluster == 1)
      {
      // root directory starting sector
      ulSector = pVolInfo->BootSect.bpb.ReservedSectors +
         (ULONG)pVolInfo->BootSect.bpb.SectorsPerFat * pVolInfo->BootSect.bpb.NumberOfFATs;
      usSectorsPerBlock = (USHORT)((ULONG)pVolInfo->SectorsPerCluster /
         ((ULONG)pVolInfo->ulClusterSize / pVolInfo->ulBlockSize));
      usSectorsRead = 0;
      }

   usMode = MODE_SCAN;
   while (usMode != MODE_RETURN && ulCluster != pVolInfo->ulFatEof)
      {
      usMode = MODE_SCAN;

      if (*pszPath == '\\')
         *pszTarget++ = *pszPath++;

      if (!strlen(pszPath))
         break;

      p = strchr(pszPath, '\\');
      if (!p)
         p = pszPath + strlen(pszPath);

      if (p - pszPath > FAT32MAXPATHCOMP - 1)
         {
         free(pDirStart);
         free(pszLongName);
         return ERROR_BUFFER_OVERFLOW;
         }

      memset(pszPart, 0, FAT32MAXPATHCOMP);
      memcpy(pszPart, pszPath, p - pszPath);
      FSH_UPPERCASE(pszPart, FAT32MAXPATHCOMP, pszUpperPart);
      pszPath = p;

      pszNumber = strchr(pszUpperPart, '~');
      if (pszNumber)
         {
         usNum = atoi(pszNumber + 1);
         }

      memset(pszLongName, 0, FAT32MAXPATHCOMP);
      fFound = FALSE;
      ulFileNo = 0;
      while (usMode == MODE_SCAN && ulCluster != pVolInfo->ulFatEof)
         {
         ULONG ulBlock;
         for (ulBlock = 0; ulBlock < pVolInfo->ulClusterSize / pVolInfo->ulBlockSize; ulBlock++)
            {
            if (ulCluster == 1)
               // reading root directory on FAT12/FAT16
               ReadSector(pVolInfo, ulSector + ulBlock * usSectorsPerBlock, usSectorsPerBlock, (void *)pDirStart, 0);
            else
               ReadBlock(pVolInfo, ulCluster, ulBlock, pDirStart, 0);
            pDir    = pDirStart;
            pDirEnd = (PDIRENTRY)((PBYTE)pDirStart + pVolInfo->ulBlockSize);

#ifdef CALL_YIELD
            Yield();
#endif

            while (usMode == MODE_SCAN && pDir < pDirEnd)
               {
#ifdef EXFAT
               if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
                  {
#endif
                  if (pDir->bAttr == FILE_LONGNAME)
                     {
                     fGetLongName(pDir, pszLongName, FAT32MAXPATHCOMP, &bCheck);
                     }
                  else if ((pDir->bAttr & 0x0F) != FILE_VOLID)
                     {
                     MakeName(pDir, szShortName, sizeof szShortName);
                     FSH_UPPERCASE(szShortName, sizeof szShortName, szShortName);

                     if (bCheck != GetVFATCheckSum(pDir))
                        memset(pszLongName, 0, FAT32MAXPATHCOMP);

                     /* support for the FAT32 variation of WinNT family */
                     if ( !*pszLongName && HAS_WINNT_EXT( pDir->fEAS ))
                        {
                        PBYTE pDot;

                        MakeName( pDir, pszLongName, sizeof( pszLongName ));
                        pDot = strchr( pszLongName, '.' );

                        if ( HAS_WINNT_EXT_NAME( pDir->fEAS )) /* name part is lower case */
                           {
                              if( pDot )
                                  *pDot = 0;

                              strlwr( pszLongName );

                              if( pDot )
                                  *pDot = '.';
                           }

                        if( pDot && HAS_WINNT_EXT_EXT( pDir->fEAS )) /* ext part is lower case */
                            strlwr( pDot + 1 );
                        }

                     if (!strlen(pszLongName))
                        strcpy(pszLongName, szShortName);
                     FSH_UPPERCASE(pszLongName, FAT32MAXPATHCOMP, pszUpperName);

                     if (usTranslate == TRANSLATE_LONG_TO_SHORT) /* OS/2 session, translate to DOS */
                        {
                        if (!stricmp(pszUpperName, pszUpperPart) ||
                            !stricmp(szShortName,  pszUpperPart))
                           {
                           strcat(pszTarget, szShortName);
                           pszTarget += strlen(pszTarget);
                           fFound = TRUE;
                           }
                        }
                     else /* translate from DOS to OS/2 */
                        {
                        if (!stricmp(szShortName,  pszUpperPart) ||
                            !stricmp(pszUpperName, pszUpperPart))
                           {
                           strcat(pszTarget, pszLongName);
                           pszTarget += strlen(pszTarget);
                           fFound = TRUE;
                           }
                        }

                     if (fFound)
                        {
                        ulCluster = (ULONG)pDir->wClusterHigh * 0x10000L + pDir->wCluster;
                        ulCluster &= pVolInfo->ulFatEof;
                        if (strlen(pszPath))
                           {
                           if (pDir->bAttr & FILE_DIRECTORY)
                              {
                              usMode = MODE_START;
                              break;
                              }
                           ulCluster = pVolInfo->ulFatEof;
                           }
                        usMode = MODE_RETURN;
                        break;
                        }
                     memset(pszLongName, 0, FAT32MAXPATHCOMP);
                     }
#ifdef EXFAT
                  }
               else
                  {
                  PDIRENTRY1 pDir1 = (PDIRENTRY1)pDir;

                  if (pDir1->bEntryType == ENTRY_TYPE_EOD)
                     {
                     ulCluster = pVolInfo->ulFatEof;
                     usMode = MODE_RETURN;
                     break;
                     }
                  else if (pDir1->bEntryType & ENTRY_TYPE_IN_USE_STATUS)
                     {
                     if (pDir1->bEntryType == ENTRY_TYPE_FILE_NAME)
                        {
                        usNumSecondary--;
                        fGetLongName1(pDir1, pszLongName, FAT32MAXPATHCOMP);

                        if (!usNumSecondary)
                           {
                           //MakeName(pDir1, szShortName, sizeof szShortName);
                           //FSH_UPPERCASE(szShortName, sizeof szShortName, szShortName);

                           //if (bCheck != GetVFATCheckSum(pDir1))
                           //   memset(pszLongName, 0, FAT32MAXPATHCOMP);
#if 0
                           /* support for the FAT32 variation of WinNT family */
                           if ( !*pszLongName && HAS_WINNT_EXT( pDir1->u.File.fEAS ))
                              {
                              PBYTE pDot;

                              MakeName( pDir1, pszLongName, sizeof( pszLongName ));
                              pDot = strchr( pszLongName, '.' );

                              if ( HAS_WINNT_EXT_NAME( pDir->u.File.fEAS )) /* name part is lower case */
                                 {
                                 if( pDot )
                                    *pDot = 0;

                                 strlwr( pszLongName );

                                 if( pDot )
                                    *pDot = '.';
                                 }

                              if ( pDot && HAS_WINNT_EXT_EXT( pDir->u.File.fEAS )) /* ext part is lower case */
                                 strlwr( pDot + 1 );
                              }
#endif
                           //if (!strlen(pszLongName))
                           //   strcpy(pszLongName, szShortName);
                           FSH_UPPERCASE(pszLongName, FAT32MAXPATHCOMP, pszUpperName);

                           if (usTranslate == TRANSLATE_LONG_TO_SHORT) /* OS/2 session, translate to DOS */
                              {
                              MakeShortName(pVolInfo, ulDirCluster, ulFileNo, pszUpperName, szShortName);
                              if (
                                   ( !stricmp(pszUpperName, pszUpperPart) ||
                                     !stricmp(szShortName,  pszUpperPart) ) )
                                 {
                                 strcat(pszTarget, szShortName);
                                 pszTarget += strlen(pszTarget);
                                 fFound = TRUE;
                                 }
                              }
                           else /* translate from DOS to OS/2 */
                              {
                              if ((pszNumber && usNum == (USHORT)ulFileNo) ||
                                  (! pszNumber && ! stricmp(pszUpperName, pszUpperPart)))
                                 {
                                 strcat(pszTarget, pszLongName);
                                 pszTarget += strlen(pszTarget);
                                 fFound = TRUE;
                                 }
                              }

                           if (fFound)
                              {
                              //ulCluster = (ULONG)pDir->wClusterHigh * 0x10000L + pDir->wCluster;
                              //ulCluster &= pVolInfo->ulFatEof;
                              ulCluster = ulRet;
                              if (strlen(pszPath))
                                 {
                                 if (usFileAttr & FILE_DIRECTORY)
                                    {
                                    usMode = MODE_START;
                                    break;
                                    }
                                 ulCluster = pVolInfo->ulFatEof;
                                 }
                              usMode = MODE_RETURN;
                              break;
                              }
                           memset(pszLongName, 0, FAT32MAXPATHCOMP);
                           }
                        }
                     else if (pDir1->bEntryType == ENTRY_TYPE_STREAM_EXT)
                        {
                        usNumSecondary--;
                        ulRet = pDir1->u.Stream.ulFirstClus;
                        //if (pDirEntryStream)
                        //   memcpy(pDirEntryStream, pDir1, sizeof(DIRENTRY1));
                        }
                     else if (pDir1->bEntryType == ENTRY_TYPE_FILE)
                        {
                        usNumSecondary = pDir1->u.File.bSecondaryCount;
                        usFileAttr = pDir1->u.File.usFileAttr;
                        memcpy(&Dir, pDir1, sizeof (DIRENTRY1));
                        memset(pszLongName, 0, FAT32MAXPATHCOMP);
                        ulFileNo++;
                        }
                     }
                  }
#endif
               pDir++;
               ulDirEntries++;
               if (ulCluster == 1 && ulDirEntries > pVolInfo->BootSect.bpb.RootDirEntries)
                  break;
               }
            if (usMode != MODE_SCAN)
               break;
            }
         if (usMode != MODE_SCAN)
            break;
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
            {
            ulCluster = GetNextCluster(pVolInfo, pDirSHInfo, ulCluster);
            }
         if (!ulCluster)
            ulCluster = pVolInfo->ulFatEof;
         //if (ulCluster == pVolInfo->ulFatEof)
         //   {
         //   Message("xxx7");
         //   strcat(pszTarget, pszPart);
         //   }
         }
      }

   free(pDirStart);
   free(pszLongName);
   if (ulCluster == pVolInfo->ulFatEof)
      {
      strcat(pszTarget, pszPath);
      }
   if (! fFound)
      {
      return ERROR_FILE_NOT_FOUND;
      }
   return 0;
}

USHORT GetChkSum16(const UCHAR *data, int bytes)
{
   USHORT chksum = 0;
   int i;

   for (i = 0; i < bytes; i++)
      {
      if (i == 2 || i == 3)
         continue;

      chksum = ((chksum << 15) | (chksum >> 1)) + (USHORT)data[i];
      }

   return chksum;
}

ULONG GetChkSum32(const UCHAR *data, int bytes)
{
   ULONG chksum = 0;
   int i;

   for (i = 0; i < bytes; i++)
      chksum = ((chksum << 31) | (chksum >> 1)) + (ULONG)data[i];

   return chksum;
}

USHORT NameHash(USHORT *pszFilename, int NameLen)
{
   USHORT hash = 0;
   UCHAR  *data = (UCHAR *)pszFilename;
   int i;

   for (i = 0; i < NameLen * 2; i++)
      hash = ((hash << 15) | (hash >> 1)) + data[i];

   return hash;
}


USHORT ModifyDirectory(PVOLINFO pVolInfo, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo,
                       USHORT usMode, PDIRENTRY pOld, PDIRENTRY pNew,
                       PDIRENTRY1 pStreamOld, PDIRENTRY1 pStreamNew,
                       PSZ pszLongNameOld, PSZ pszLongNameNew, USHORT usIOMode)
{
USHORT    usEntriesNeeded;
USHORT    usFreeEntries;
ULONG     ulCluster;
ULONG     ulPrevCluster;
ULONG     ulPrevBlock;
ULONG     ulNextCluster = pVolInfo->ulFatEof;
USHORT    usClusterCount;
BOOL      fNewCluster;
ULONG     ulSector;
ULONG     ulPrevSector;
USHORT    usSectorsRead;
USHORT    usSectorsPerBlock;
ULONG     ulBytesToRead = 0;
ULONG     ulPrevBytesToRead = 0;
ULONG     ulBytesRemained;
USHORT    rc;

#ifdef EXFAT
   if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
#endif
      {
      PDIRENTRY pDirectory;
      PDIRENTRY pDir2;
      PDIRENTRY pWork, pWork2;
      PDIRENTRY pMax;
      PDIRENTRY pDirNew;
      PDIRENTRY pLNStart;

      MessageL(LOG_FUNCS, "ModifyDirectory%m DirCluster %ld, Mode = %d",
               0x0035, ulDirCluster, usMode);

      pDirNew = (PDIRENTRY)malloc((size_t)sizeof(DIRENTRY));
      if (!pDirNew)
         {
         return ERROR_NOT_ENOUGH_MEMORY;
         }

      if (usMode == MODIFY_DIR_RENAME ||
          usMode == MODIFY_DIR_INSERT)
         {
         if (!pNew || !pszLongNameNew)
            {
            Message("Modify directory: Invalid parameters 1");
            free(pDirNew);
            return ERROR_INVALID_PARAMETER;
            }

         memcpy(pDirNew, pNew, sizeof (DIRENTRY));
         if ((pNew->bAttr & 0x0F) != FILE_VOLID)
            {
            rc = MakeShortName(pVolInfo, ulDirCluster, 0xffffffff, pszLongNameNew, pDirNew->bFileName);
            if (rc == LONGNAME_ERROR)
               {
               Message("Modify directory: Longname error");
               free(pDirNew);
               return ERROR_FILE_EXISTS;
               }
            memcpy(pNew, pDirNew, sizeof (DIRENTRY));

            if (rc == LONGNAME_OFF)
               pszLongNameNew = NULL;
            }
         else
            pszLongNameNew = NULL;

         usEntriesNeeded = 1;
         if (pszLongNameNew)
#if 0
            usEntriesNeeded += strlen(pszLongNameNew) / 13 +
               (strlen(pszLongNameNew) % 13 ? 1 : 0);
#else
            usEntriesNeeded += ( DBCSStrlen( pszLongNameNew ) + 12 ) / 13;
#endif
         }

      if (usMode == MODIFY_DIR_RENAME ||
          usMode == MODIFY_DIR_DELETE ||
          usMode == MODIFY_DIR_UPDATE)
         {
         if (!pOld)
            {
            Message("Modify directory: Invalid parameter 2 ");
            free(pDirNew);
            return ERROR_INVALID_PARAMETER;
            }
         }

      pDirectory = (PDIRENTRY)malloc((size_t)(2 * pVolInfo->ulBlockSize));
      if (!pDirectory)
         {
         Message("Modify directory: Not enough memory");
         free(pDirNew);
         return ERROR_NOT_ENOUGH_MEMORY;
         }
      memset(pDirectory, 0, (size_t)pVolInfo->ulBlockSize);
      pDir2 = (PDIRENTRY)((PBYTE)pDirectory + pVolInfo->ulBlockSize);
      memset(pDir2, 0, (size_t)pVolInfo->ulBlockSize);

      ulCluster = ulDirCluster;
      pLNStart = NULL;
      ulPrevCluster = pVolInfo->ulFatEof;
      ulPrevBlock = 0;
      usClusterCount = 0;
      fNewCluster = FALSE;

      if (ulCluster == 1)
         {
         // root directory starting sector
         ulSector = pVolInfo->BootSect.bpb.ReservedSectors +
            (ULONG)pVolInfo->BootSect.bpb.SectorsPerFat * pVolInfo->BootSect.bpb.NumberOfFATs;
         usSectorsPerBlock = (USHORT)((ULONG)pVolInfo->SectorsPerCluster /
            ((ULONG)pVolInfo->ulClusterSize / pVolInfo->ulBlockSize));
         usSectorsRead = 0;
         ulBytesRemained = (ULONG)pVolInfo->BootSect.bpb.RootDirEntries * sizeof(DIRENTRY);
         }

      while (ulCluster != pVolInfo->ulFatEof)
         {
         ULONG ulBlock;
#ifdef CALL_YIELD
         Yield();
#endif
         usClusterCount++;
         for (ulBlock = 0;
              ulBlock < pVolInfo->ulClusterSize / pVolInfo->ulBlockSize &&
              ulCluster != pVolInfo->ulFatEof; ulBlock++)
            {
            if (!fNewCluster)
               {
               if (ulCluster == 1)
                  {
                  // reading root directory on FAT12/FAT16
                  rc = ReadSector(pVolInfo, ulSector + ulBlock * usSectorsPerBlock, usSectorsPerBlock, (void *)pDir2, usIOMode);
                  if (ulBytesRemained >= pVolInfo->ulBlockSize)
                     {
                     ulBytesToRead = pVolInfo->ulBlockSize;
                     ulBytesRemained -= pVolInfo->ulBlockSize;
                     }
                  else
                     {
                     ulBytesToRead = ulBytesRemained;
                     ulBytesRemained = 0;
                     }
                  }
               else
                  {
                  rc = ReadBlock(pVolInfo, ulCluster, ulBlock, pDir2, usIOMode);
                  ulBytesToRead = pVolInfo->ulBlockSize;
                  }
               if (rc)
                  {
                  free(pDirNew);
                  free(pDirectory);
                  return rc;
                  }
               }
            else
               {
               memset(pDir2, 0, (size_t)pVolInfo->ulBlockSize);
               fNewCluster = FALSE;
               }

            pMax = (PDIRENTRY)((PBYTE)pDirectory + pVolInfo->ulBlockSize + ulBytesToRead);

            switch (usMode)
               {
               case MODIFY_DIR_RENAME :
               case MODIFY_DIR_UPDATE :
               case MODIFY_DIR_DELETE :

                  /*
                     Find old entry
                  */

                  pWork = pDir2;
                  while (pWork != pMax)
                     {
                     if (pWork->bFileName[0] && pWork->bFileName[0] != DELETED_ENTRY)
                        {
                        if (pWork->bAttr == FILE_LONGNAME)
                           {
                           if (!pLNStart)
                              pLNStart = pWork;
                           }
                        else if ((pWork->bAttr & 0x0F) != FILE_VOLID)
                           {
                           if (!memcmp(pWork->bFileName, pOld->bFileName, 11) &&
                               pWork->wCluster     == pOld->wCluster &&
                               pWork->wClusterHigh == pOld->wClusterHigh)
                              {
                              if (!pLNStart)
                                 pLNStart = pWork;
                              break;
                              }
                           pLNStart = NULL;
                           }
                        else
                           pLNStart = NULL;
                        }
                     else
                        pLNStart = NULL;
                     pWork++;
                     }

                  if (pWork != pMax)
                     {
                     switch (usMode)
                        {
                        case MODIFY_DIR_UPDATE:
                           MessageL(LOG_FUNCS, " Updating cluster%m", 0x4009);
                           memcpy(pWork, pNew, sizeof (DIRENTRY));
                           if (ulCluster == 1)
                              // reading root directory on FAT12/FAT16
                              rc = WriteSector(pVolInfo, ulSector + ulBlock * usSectorsPerBlock, usSectorsPerBlock, (void *)pDir2, usIOMode);
                           else
                              rc = WriteBlock(pVolInfo, ulCluster, ulBlock, pDir2, usIOMode);
                           if (rc)
                              {
                              free(pDirNew);
                              free(pDirectory);
                              return rc;
                              }
                           ulCluster = pVolInfo->ulFatEof;
                           break;

                        case MODIFY_DIR_DELETE:
                        case MODIFY_DIR_RENAME:
                           MessageL(LOG_FUNCS, " Removing entry from cluster%m", 0x400a);
                           pWork2 = pLNStart;
                           while (pWork2 < pWork)
                              {
                              MessageL(LOG_FUNCS, "Deleting Longname entry.%m", 0x400b);
                              pWork2->bFileName[0] = DELETED_ENTRY;
                              pWork2++;
                              }
                           pWork->bFileName[0] = DELETED_ENTRY;

                           /*
                              Write previous cluster if LN start lies there
                           */
                           if (ulPrevCluster != pVolInfo->ulFatEof &&
                              pLNStart < pDir2)
                              {
                              if (ulPrevCluster == 1)
                                 // reading root directory on FAT12/FAT16
                                 rc = WriteSector(pVolInfo, ulPrevSector + ulPrevBlock * usSectorsPerBlock, usSectorsPerBlock, (void *)pDirectory, usIOMode);
                              else
                                 rc = WriteBlock(pVolInfo, ulPrevCluster, ulPrevBlock, pDirectory, usIOMode);
                              if (rc)
                                 {
                                 free(pDirNew);
                                 free(pDirectory);
                                 return rc;
                                 }
                              }

                           /*
                              Write current cluster
                           */
                           if (ulCluster == 1)
                              // reading root directory on FAT12/FAT16
                              rc = WriteSector(pVolInfo, ulSector + ulBlock * usSectorsPerBlock, usSectorsPerBlock, (void *)pDir2, usIOMode);
                           else
                              rc = WriteBlock(pVolInfo, ulCluster, ulBlock, pDir2, usIOMode);
                           if (rc)
                              {
                              free(pDirNew);
                              free(pDirectory);
                              return rc;
                              }

                           if (usMode == MODIFY_DIR_DELETE)
                              ulCluster = pVolInfo->ulFatEof;
                           else
                              {
                              usMode = MODIFY_DIR_INSERT;

                              pszLongNameOld = pszLongNameNew; //////

                              ulCluster = ulDirCluster;
                              ulBytesRemained = pVolInfo->BootSect.bpb.RootDirEntries * sizeof(DIRENTRY);
                              ulPrevCluster = pVolInfo->ulFatEof;
                              ulPrevSector = 0;
                              ulPrevBlock = 0;
                              usClusterCount = 0;
                              pLNStart = NULL;
                              continue;
                              }
                           break;
                        }
                     }

                  break;

               case MODIFY_DIR_INSERT:
                  if (ulPrevCluster != pVolInfo->ulFatEof && GetFreeEntries(pDirectory, ulPrevBytesToRead + ulBytesToRead) >= usEntriesNeeded)
                     {
                     BYTE bCheck = GetVFATCheckSum(pDirNew);

                     MessageL(LOG_FUNCS, " Inserting entry into 2 clusters%m", 0x400c);

                     pWork = CompactDir(pDirectory, ulPrevBytesToRead + ulBytesToRead, usEntriesNeeded);
                     pWork = fSetLongName(pWork, pszLongNameNew, bCheck);
                     memcpy(pWork, pDirNew, sizeof (DIRENTRY));

                     if (ulPrevCluster == 1)
                        // reading root directory on FAT12/FAT16
                        rc = WriteSector(pVolInfo, ulPrevSector + ulPrevBlock * usSectorsPerBlock, usSectorsPerBlock, (void *)pDirectory, usIOMode);
                     else
                        rc = WriteBlock(pVolInfo, ulPrevCluster, ulPrevBlock, pDirectory, usIOMode);
                     if (rc)
                        {
                        free(pDirNew);
                        free(pDirectory);
                        return rc;
                        }

                     if (ulCluster == 1)
                        // reading root directory on FAT12/FAT16
                        rc = WriteSector(pVolInfo, ulSector + ulBlock * usSectorsPerBlock, usSectorsPerBlock, (void *)pDir2, usIOMode);
                     else
                        rc = WriteBlock(pVolInfo, ulCluster, ulBlock, pDir2, usIOMode);
                     if (rc)
                        {
                        free(pDirNew);
                        free(pDirectory);
                        return rc;
                        }
                     ulCluster = pVolInfo->ulFatEof;
                     break;
                     }

                  usFreeEntries = GetFreeEntries(pDir2, ulBytesToRead);
                  if (usFreeEntries >= usEntriesNeeded)
                     {
                     BYTE bCheck = GetVFATCheckSum(pDirNew);

                     MessageL(LOG_FUNCS, " Inserting entry into 1 cluster%m", 0x400d);

                     pWork = CompactDir(pDir2, ulBytesToRead, usEntriesNeeded);
                     pWork = fSetLongName(pWork, pszLongNameNew, bCheck);
                     memcpy(pWork, pDirNew, sizeof (DIRENTRY));
                     if (ulCluster == 1)
                        // reading root directory on FAT12/FAT16
                        rc = WriteSector(pVolInfo, ulSector + ulBlock * usSectorsPerBlock, usSectorsPerBlock, (void *)pDir2, usIOMode);
                     else
                        rc = WriteBlock(pVolInfo, ulCluster, ulBlock, pDir2, usIOMode);
                     if (rc)
                        {
                        free(pDirNew);
                        free(pDirectory);
                        return rc;
                        }
                     ulCluster = pVolInfo->ulFatEof;
                     break;
                     }
                  else if (usFreeEntries > 0)
                     {
                     MarkFreeEntries(pDir2, ulBytesToRead);
                     if (ulCluster == 1)
                        // reading root directory on FAT12/FAT16
                        rc = WriteSector(pVolInfo, ulSector + ulBlock * usSectorsPerBlock, usSectorsPerBlock, (void *)pDir2, usIOMode);
                     else
                        rc = WriteBlock(pVolInfo, ulCluster, ulBlock, pDir2, usIOMode);
                     if (rc)
                        {
                        free(pDirNew);
                        free(pDirectory);
                        return rc;
                        }
                     }

                  break;
               }

            if (ulCluster != pVolInfo->ulFatEof)
               {
               ulPrevBytesToRead = ulBytesToRead;
               ulPrevCluster = ulCluster;
               ulPrevSector = ulSector;
               ulPrevBlock = ulBlock;
               memset(pDirectory, 0, (size_t)pVolInfo->ulBlockSize);
               memmove(pDirectory, pDir2, (size_t)ulBytesToRead);
               if (pLNStart)
                  pLNStart = (PDIRENTRY)((PBYTE)pLNStart - pVolInfo->ulBlockSize);

               if (ulCluster == 1)
                  {
                  // reading the root directory in case of FAT12/FAT16
                  ulSector += pVolInfo->SectorsPerCluster;
                  usSectorsRead += pVolInfo->SectorsPerCluster;
                  if ((ULONG)usSectorsRead * pVolInfo->BootSect.bpb.BytesPerSector >=
                      (ULONG)pVolInfo->BootSect.bpb.RootDirEntries * sizeof(DIRENTRY))
                     // root directory ended
                     ulNextCluster = 0;
                  else
                     ulNextCluster = 1;
                  }
               else
                  ulNextCluster = GetNextCluster(pVolInfo, NULL, ulCluster);
               if (!ulNextCluster)
                  ulNextCluster = pVolInfo->ulFatEof;
               if (ulNextCluster == pVolInfo->ulFatEof)
                  {
                  if (usMode == MODIFY_DIR_UPDATE ||
                      usMode == MODIFY_DIR_DELETE ||
                      usMode == MODIFY_DIR_RENAME)
                     {
                     if (ulBlock == pVolInfo->ulClusterSize / pVolInfo->ulBlockSize - 1)
                        {
                        free(pDirNew);
                        free(pDirectory);
                        return ERROR_FILE_NOT_FOUND;
                        }
                     else
                        {
                        if (ulBlock == pVolInfo->ulClusterSize / pVolInfo->ulBlockSize - 1)
                           ulCluster = ulNextCluster;
                        continue;
                        }
                     }
                  else
                     {
                     if (ulCluster == 1)
                        {
                        // no expanding for root directory in case of FAT12/FAT16
                        ulNextCluster = pVolInfo->ulFatEof;
                        }
                     else
                        ulNextCluster = SetNextCluster(pVolInfo, ulCluster, FAT_ASSIGN_NEW);
                     if (ulNextCluster == pVolInfo->ulFatEof)
                        {
                        free(pDirNew);
                        free(pDirectory);
                        Message("Modify Directory: Disk Full!");
                        return ERROR_DISK_FULL;
                        }
                     fNewCluster = TRUE;
                     }
                  }
               ulCluster = ulNextCluster;

               // clear the new cluster
               if (fNewCluster)
                  {
                  ULONG ulBlock2;
                  memset(pDir2, 0, (size_t)pVolInfo->ulBlockSize);
                  for (ulBlock2 = 0; ulBlock2 < pVolInfo->ulClusterSize / pVolInfo->ulBlockSize; ulBlock2++)
                     {
                     rc = WriteBlock(pVolInfo, ulCluster, ulBlock2, pDir2, usIOMode);
                     if (rc)
                        {
                        free(pDirNew);
                        free(pDirectory);
                        return rc;
                        }
                     }
                  }
               break;
               }
            if (ulCluster == pVolInfo->ulFatEof)
               break;
            if (ulBlock == pVolInfo->ulClusterSize / pVolInfo->ulBlockSize - 1)
               ulCluster = ulNextCluster;
            }
         if (ulCluster == pVolInfo->ulFatEof)
            break;
         if (ulBlock == pVolInfo->ulClusterSize / pVolInfo->ulBlockSize - 1)
            ulCluster = ulNextCluster;
         }

      free(pDirNew);
      free(pDirectory);
      return 0;
      }
#ifdef EXFAT
   else
      {
      // exFAT case
      PDIRENTRY1 pDirectory;
      PDIRENTRY1 pDir2;
      PDIRENTRY1 pWork, pWork2;
      PDIRENTRY1 pWorkStream, pWorkFile, pWorkFileName;
      PDIRENTRY1 pMax;
      PDIRENTRY1 pNew1 = (PDIRENTRY1)pNew;
      PDIRENTRY1 pLNStart;
      PDIRENTRY1 pDirNew;
      PSZ       szLongName;
      USHORT    usNumSecondary  = 0;
      USHORT    usNumSecondary2 = 0;
      USHORT    usFileName, usIndex;
      USHORT    usNameHash;
      ULONG     ulBlock2, ulCluster2;
      BOOL      fFound;
      BOOL      fCrossBorder;

      MessageL(LOG_FUNCS, "ModifyDirectory%m DirCluster %ld, Mode = %d",
               0x0035, ulDirCluster, usMode);

      szLongName = (PSZ)malloc(FAT32MAXPATHCOMP);
      if (!szLongName)
         {
         return ERROR_NOT_ENOUGH_MEMORY;
         }

      pDirNew = (PDIRENTRY1)malloc((size_t)sizeof(DIRENTRY1));
      if (!pDirNew)
         {
         free(szLongName);
         return ERROR_NOT_ENOUGH_MEMORY;
         }

      if (usMode == MODIFY_DIR_RENAME ||
          usMode == MODIFY_DIR_INSERT)
         {
         if (!pNew || !pszLongNameNew)
            {
            free(szLongName);
            free(pDirNew);
            return ERROR_INVALID_PARAMETER;
            }

         memcpy(pDirNew, pNew, sizeof (DIRENTRY1));
         /* if ((pNew->bAttr & 0x0F) != FILE_VOLID)
            {
            rc = MakeShortName(pVolInfo, ulDirCluster, 0xffffffff, pszLongNameNew, pDirNew->bFileName);
            if (rc == LONGNAME_ERROR)
               {
               Message("Modify directory: Longname error");
               free(szLongName);
               free(pDirNew);
               return ERROR_FILE_EXISTS;
               }
            memcpy(pNew, pDirNew, sizeof (DIRENTRY));

            if (rc == LONGNAME_OFF)
               pszLongNameNew = NULL;
            }
         else
            pszLongNameNew = NULL; */

         if (pStreamNew)
            {
            //usEntriesNeeded = 1;
            usEntriesNeeded = 2;
            if (pszLongNameNew)
#if 0
               usEntriesNeeded += strlen(pszLongNameNew) / 13 +
                  (strlen(pszLongNameNew) % 13 ? 1 : 0);
#else
               //usEntriesNeeded += ( DBCSStrlen( pszLongNameNew ) + 12 ) / 13;
               usEntriesNeeded += ( DBCSStrlen( pszLongNameNew ) + 14 ) / 15;
#endif
            }
         else
            {
            usEntriesNeeded = 1;
            }
         }

      if (usMode == MODIFY_DIR_RENAME ||
          usMode == MODIFY_DIR_DELETE ||
          usMode == MODIFY_DIR_UPDATE)
         {
         if (!pOld)
            {
            free(szLongName);
            free(pDirNew);
            return ERROR_INVALID_PARAMETER;
            }
         }

      pDirectory = (PDIRENTRY1)malloc((size_t)(2 * pVolInfo->ulBlockSize));
      if (!pDirectory)
         {
         free(szLongName);
         free(pDirNew);
         return ERROR_NOT_ENOUGH_MEMORY;
         }
      memset(pDirectory, 0, (size_t)pVolInfo->ulBlockSize);
      pDir2 = (PDIRENTRY1)((PBYTE)pDirectory + pVolInfo->ulBlockSize);
      memset(pDir2, 0, (size_t)pVolInfo->ulBlockSize);

      ulCluster = ulDirCluster;
      pLNStart = NULL;
      ulPrevCluster = pVolInfo->ulFatEof;
      ulPrevBlock = 0;
      usClusterCount = 0;
      fNewCluster = FALSE;

      //if (ulCluster == 1)
      //   {
      //   // root directory starting sector
      //   ulSector = pVolInfo->BootSect.bpb.ReservedSectors +
      //      (ULONG)pVolInfo->BootSect.bpb.SectorsPerFat * pVolInfo->BootSect.bpb.NumberOfFATs;
      //   usSectorsPerBlock = (ULONG)pVolInfo->SectorsPerCluster /
      //      ((ULONG)pVolInfo->ulClusterSize / pVolInfo->ulBlockSize);
      //   usSectorsRead = 0;
      //   ulBytesRemained = (ULONG)pVolInfo->BootSect.bpb.RootDirEntries * sizeof(DIRENTRY);
      //   }

      pWorkFile = NULL;
      pWorkFileName = NULL;
      while (ulCluster != pVolInfo->ulFatEof)
         {
         ULONG ulBlock;
#ifdef CALL_YIELD
         Yield();
#endif
         usClusterCount++;
         for (ulBlock = 0;
              ulBlock < pVolInfo->ulClusterSize / pVolInfo->ulBlockSize &&
              ulCluster != pVolInfo->ulFatEof; ulBlock++)
            {
            if (!fNewCluster)
               {
               //if (ulCluster == 1)
               //   {
               //   // reading root directory on FAT12/FAT16
               //   rc = ReadSector(pVolInfo, ulSector + ulBlock * usSectorsPerBlock, usSectorsPerBlock, (void *)pDir2, usIOMode);
               //   if (ulBytesRemained >= pVolInfo->ulBlockSize)
               //      {
               //      ulBytesToRead = pVolInfo->ulBlockSize;
               //      ulBytesRemained -= pVolInfo->ulBlockSize;
               //      }
               //   else
               //      {
               //      ulBytesToRead = ulBytesRemained;
               //      ulBytesRemained = 0;
               //      }
               //   }
               //else
               //   {
               //Message("md000aa: ulCluster=%lx, ulBlock=%lu", ulCluster, ulBlock);
                  rc = ReadBlock(pVolInfo, ulCluster, ulBlock, pDir2, usIOMode);
                  ulBytesToRead = pVolInfo->ulBlockSize;
               //   }
               if (rc)
                  {
                  free(szLongName);
                  free(pDirNew);
                  free(pDirectory);
                  return rc;
                  }
               }
            else
               {
               memset(pDir2, 0, (size_t)pVolInfo->ulBlockSize);
               fNewCluster = FALSE;
               }

            pMax = (PDIRENTRY1)((PBYTE)pDirectory + pVolInfo->ulBlockSize + ulBytesToRead);

            switch (usMode)
               {
               case MODIFY_DIR_RENAME :
               case MODIFY_DIR_UPDATE :
               case MODIFY_DIR_DELETE :
                  memcpy(pDirectory, pDir2, (size_t)pVolInfo->ulBlockSize);
                  if (ulBlock == pVolInfo->ulClusterSize / pVolInfo->ulBlockSize - 1)
                     {
                     ulBlock2 = 0;
                     ulCluster2 = GetNextCluster(pVolInfo, pDirSHInfo, ulCluster);
                     }
                  else
                     {
                     ulBlock2 = ulBlock + 1;
                     ulCluster2 = ulCluster;
                     }
                  if (ulCluster2 != pVolInfo->ulFatEof)
                     {
                     rc = ReadBlock(pVolInfo, ulCluster2, ulBlock2, pDir2, usIOMode);
                     if (rc)
                        {
                        free(szLongName);
                        free(pDirNew);
                        free(pDirectory);
                        return rc;
                        }
                     }
                  else
                     memset(pDir2, 0, (size_t)pVolInfo->ulBlockSize);

                  /*
                     Find old entry
                  */

                  fFound = FALSE;
                  fCrossBorder = FALSE;
                  pWork = pDirectory;
                  pMax = (PDIRENTRY1)((PBYTE)pDirectory + ulBytesToRead);
                  while (!pLNStart)
                     {
                     if (pWork->bEntryType == ENTRY_TYPE_EOD)
                        break;
                     //if (pWork->bFileName[0] && pWork->bFileName[0] != DELETED_ENTRY)
                     if (pWork->bEntryType & ENTRY_TYPE_IN_USE_STATUS)
                        {
                        //if (pWork->bAttr == FILE_LONGNAME)
                        if (pWork->bEntryType == ENTRY_TYPE_FILE_NAME)
                           {
                           usNumSecondary2--;
                           usFileName++;
                           fGetLongName1(pWork, szLongName, FAT32MAXPATHCOMP);

                           if (pWork == pMax - 1)
                              fCrossBorder = TRUE;

                           if ( !pLNStart && !usNumSecondary2 && pWorkStream && pStreamOld &&
                                //pWorkStream->u.Stream.ulFirstClus == pStreamOld->u.Stream.ulFirstClus &&
                                ( !stricmp(szLongName, pszLongNameOld) ) )
                           //if (!pLNStart && !usFileName && fFound)
                              {
                              pWorkFileName = pWork;
                              pLNStart = pWork - usFileName + 1;
                              fFound = TRUE;
                              //pLNStart = pWork;
                              break;
                              }
                           //usFileName++;
                           }
                        else if (pWork->bEntryType == ENTRY_TYPE_FILE)
                           {
                           usFileName = 0;
                           usNumSecondary = usNumSecondary2 = pWork->u.File.bSecondaryCount;

                           if (pWork == pMax - 1)
                              fCrossBorder = TRUE;
                           else if (pWork >= pMax)
                              break;

                           pWorkFile = pWork;
                           //*szLongName = '\0';
                           memset(szLongName, 0, FAT32MAXPATHCOMP);
                           }
                        //else if ((pWork->bAttr & 0x0F) != FILE_VOLID)
                        else if (pWork->bEntryType == ENTRY_TYPE_STREAM_EXT)
                           {
                           usNumSecondary2--;
                           //if (!memcmp(pWork->bFileName, pOld->bFileName, 11) &&
                           //    pWork->wCluster     == pOld->wCluster &&
                           //    pWork->wClusterHigh == pOld->wClusterHigh)

                           if (pWork == pMax - 1)
                              fCrossBorder = TRUE;

                           pWorkStream = pWork;
                           pLNStart = NULL;
                           }
                        else
                           pLNStart = NULL;
                        }
                     else
                        pLNStart = NULL;
                     if (fCrossBorder && !usNumSecondary2)
                        break;
                     pWork++;
                     }

                  if (!fFound)
                     {
                     if (usMode == MODIFY_DIR_UPDATE ||
                         usMode == MODIFY_DIR_DELETE ||
                         usMode == MODIFY_DIR_RENAME)
                        // copy it back
                        memmove(pDir2, pDirectory, (size_t)ulBytesToRead);
                     }
                  else
                  //if (pWork != pMax)
                     {
                     switch (usMode)
                        {
                        case MODIFY_DIR_UPDATE:
                           MessageL(LOG_FUNCS, " Updating cluster%m", 0x4009);
                           pNew1->bEntryType = ENTRY_TYPE_FILE;
                           memcpy(pWorkFile, pNew, sizeof (DIRENTRY1));
                           if (pStreamNew)
                              {
                              pStreamNew->bEntryType = ENTRY_TYPE_STREAM_EXT;
                              memcpy(pWorkStream, pStreamNew, sizeof (DIRENTRY1));
                              }
                           pWorkFile->u.File.usSetCheckSum = GetChkSum16((UCHAR *)pWorkFile,
                              sizeof(DIRENTRY1) * (pWorkFile->u.File.bSecondaryCount + 1));
                           rc = WriteBlock(pVolInfo, ulCluster, ulBlock, pDirectory, usIOMode);
                           if (rc)
                              {
                              free(szLongName);
                              free(pDirNew);
                              free(pDirectory);
                              return rc;
                              }
                           if (ulCluster2 != pVolInfo->ulFatEof && pWorkFileName > pMax)
                              {
                              //if (ulCluster == 1)
                              //   // reading root directory on FAT12/FAT16
                              //   rc = WriteSector(pVolInfo, ulSector + ulBlock * usSectorsPerBlock, usSectorsPerBlock, (void *)pDir2, usIOMode);
                              //else
                                 rc = WriteBlock(pVolInfo, ulCluster2, ulBlock2, pDir2, usIOMode);
                              if (rc)
                                 {
                                 free(szLongName);
                                 free(pDirNew);
                                 free(pDirectory);
                                 return rc;
                                 }
                              }
                           ulCluster = pVolInfo->ulFatEof;
                           break;

                        case MODIFY_DIR_DELETE:
                        case MODIFY_DIR_RENAME:
                           MessageL(LOG_FUNCS, " Removing entry from cluster%m", 0x400a);
                           pWork2 = pLNStart;
                           for (usIndex = 0; usIndex < usNumSecondary - 1; usIndex++)
                              {
                              MessageL(LOG_FUNCS, "Deleting Longname entry.%m", 0x400b);
                              //pWork2->bFileName[0] = DELETED_ENTRY;
                              pWork2->bEntryType &= ~ENTRY_TYPE_IN_USE_STATUS;
                              pWork2++;
                              }
                           //pWork->bFileName[0] = DELETED_ENTRY;
                           pWorkFile->bEntryType &= ~ENTRY_TYPE_IN_USE_STATUS;
                           pWorkStream->bEntryType &= ~ENTRY_TYPE_IN_USE_STATUS;

                           /*
                              Write current cluster
                           */
                           rc = WriteBlock(pVolInfo, ulCluster, ulBlock, pDirectory, usIOMode);
                           if (rc)
                              {
                              free(szLongName);
                              free(pDirNew);
                              free(pDirectory);
                              return rc;
                              }
                           if (ulCluster2 != pVolInfo->ulFatEof && pWorkFileName > pMax)
                              {
                              //if (ulCluster == 1)
                              //   // reading root directory on FAT12/FAT16
                              //   rc = WriteSector(pVolInfo, ulSector + ulBlock * usSectorsPerBlock, usSectorsPerBlock, (void *)pDir2, usIOMode);
                              //else
                                 rc = WriteBlock(pVolInfo, ulCluster2, ulBlock2, pDir2, usIOMode);
                              if (rc)
                                 {
                                 free(szLongName);
                                 free(pDirNew);
                                 free(pDirectory);
                                 return rc;
                                 }
                              }

                           if (usMode == MODIFY_DIR_DELETE)
                              ulCluster = pVolInfo->ulFatEof;
                           else
                              {
                              usMode = MODIFY_DIR_INSERT;
                              pszLongNameOld = pszLongNameNew;
                              ulCluster = ulDirCluster;
                              ulPrevCluster = pVolInfo->ulFatEof;
                              ulPrevSector = 0;
                              ulPrevBlock = 0;
                              usClusterCount = 0;
                              pLNStart = NULL;
                              continue;
                              }
                           break;
                        }
                     }

                  break;

               case MODIFY_DIR_INSERT:
                  if (pStreamNew)
                  {
                     pNew1->bEntryType = ENTRY_TYPE_FILE;
                     pStreamNew->bEntryType = ENTRY_TYPE_STREAM_EXT;
                  }

                  if (ulPrevCluster != pVolInfo->ulFatEof && GetFreeEntries1(pDirectory, ulPrevBytesToRead + ulBytesToRead) >= usEntriesNeeded)
                     {
                     PDIRENTRY1 pWork3;
                     //BYTE bCheck = GetVFATCheckSum(pDirNew);

                     MessageL(LOG_FUNCS, " Inserting entry into 2 clusters%m", 0x400c);

                     pWork = CompactDir1(pDirectory, ulPrevBytesToRead + ulBytesToRead, usEntriesNeeded);
                     //pWork = fSetLongName(pWork, pszLongNameNew, bCheck);
                     //memcpy(pWork, pDirNew, sizeof (DIRENTRY));
                     if (pStreamNew)
                        {
                        pWork3 = fSetLongName1(pWork+2, pszLongNameNew, &usNameHash);
                        pNew1->u.File.bSecondaryCount = (BYTE)(pWork3 - pWork - 1);
                        memcpy(pWork++, pNew, sizeof (DIRENTRY1));
                        pStreamNew->u.Stream.usNameHash = usNameHash;
                        pStreamNew->u.Stream.bNameLen = (BYTE)strlen(pszLongNameNew);
                        memcpy(pWork++, pStreamNew, sizeof (DIRENTRY1));
                        (pWork-2)->u.File.usSetCheckSum = GetChkSum16((UCHAR *)(pWork-2),
                           sizeof(DIRENTRY1) * ((pWork-2)->u.File.bSecondaryCount + 1));
                        }
                     else
                        {
                        memcpy(pWork, pNew, sizeof (DIRENTRY));
                        }

                     //if (ulPrevCluster == 1)
                     //   // reading root directory on FAT12/FAT16
                     //   rc = WriteSector(pVolInfo, ulPrevSector + ulPrevBlock * usSectorsPerBlock, usSectorsPerBlock, (void *)pDirectory, usIOMode);
                     //else
                        rc = WriteBlock(pVolInfo, ulPrevCluster, ulPrevBlock, pDirectory, usIOMode);
                     if (rc)
                        {
                        free(szLongName);
                        free(pDirNew);
                        free(pDirectory);
                        return rc;
                        }

                     //if (ulCluster == 1)
                     //   // reading root directory on FAT12/FAT16
                     //   rc = WriteSector(pVolInfo, ulSector + ulBlock * usSectorsPerBlock, usSectorsPerBlock, (void *)pDir2, usIOMode);
                     //else
                        rc = WriteBlock(pVolInfo, ulCluster, ulBlock, pDir2, usIOMode);
                     if (rc)
                        {
                        free(szLongName);
                        free(pDirNew);
                        free(pDirectory);
                        return rc;
                        }
                     ulCluster = pVolInfo->ulFatEof;
                     break;
                     }

                  usFreeEntries = GetFreeEntries1(pDir2, ulBytesToRead);
                  if (usFreeEntries >= usEntriesNeeded)
                     {
                     PDIRENTRY1 pWork3;
                     //BYTE bCheck = GetVFATCheckSum(pDirNew);

                     MessageL(LOG_FUNCS, " Inserting entry into 1 cluster%m", 0x400d);

                     //pWork = CompactDir1(pDirectory, ulBytesToRead, usEntriesNeeded);
                     pWork = CompactDir1(pDir2, ulBytesToRead, usEntriesNeeded);
                     
                     if (pStreamNew)
                        {
                        pWork3 = fSetLongName1(pWork+2, pszLongNameNew, &usNameHash);
                        pNew1->u.File.bSecondaryCount = (BYTE)(pWork3 - pWork - 1);
                        memcpy(pWork++, pNew, sizeof (DIRENTRY1));
                        pStreamNew->u.Stream.usNameHash = usNameHash;
                        pStreamNew->u.Stream.bNameLen = (BYTE)strlen(pszLongNameNew);
                        memcpy(pWork++, pStreamNew, sizeof (DIRENTRY1));
                        (pWork-2)->u.File.usSetCheckSum = GetChkSum16((UCHAR *)(pWork-2),
                           sizeof(DIRENTRY1) * ((pWork-2)->u.File.bSecondaryCount + 1));
                        }
                     else
                        {
                        memcpy(pWork, pNew, sizeof (DIRENTRY));
                        }
                     //if (ulCluster == 1)
                     //   // reading root directory on FAT12/FAT16
                     //   rc = WriteSector(pVolInfo, ulSector + ulBlock * usSectorsPerBlock, usSectorsPerBlock, (void *)pDir2, usIOMode);
                     //else
                     //   rc = WriteBlock(pVolInfo, ulCluster, ulBlock, pDirectory, usIOMode);
                        rc = WriteBlock(pVolInfo, ulCluster, ulBlock, pDir2, usIOMode);
                     if (rc)
                        {
                        free(szLongName);
                        free(pDirNew);
                        free(pDirectory);
                        return rc;
                        }
                     ulCluster = pVolInfo->ulFatEof;
                     break;
                     }
                  else if (usFreeEntries > 0)
                     {
                     MarkFreeEntries1(pDir2, ulBytesToRead);
                     //if (ulCluster == 1)
                     //   // reading root directory on FAT12/FAT16
                     //   rc = WriteSector(pVolInfo, ulSector + ulBlock * usSectorsPerBlock, usSectorsPerBlock, (void *)pDir2, usIOMode);
                     //else
                        rc = WriteBlock(pVolInfo, ulCluster, ulBlock, pDir2, usIOMode);
                     if (rc)
                        {
                        free(szLongName);
                        free(pDirNew);
                        free(pDirectory);
                        return rc;
                        }
                     }

                  break;
               }

            if (ulCluster != pVolInfo->ulFatEof)
               {
               ulPrevBytesToRead = ulBytesToRead;
               ulPrevCluster = ulCluster;
               ulPrevSector = ulSector;
               ulPrevBlock = ulBlock;
               memset(pDirectory, 0, (size_t)pVolInfo->ulBlockSize);
               memmove(pDirectory, pDir2, (size_t)ulBytesToRead);
               if (pLNStart)
                  pLNStart = (PDIRENTRY1)((PBYTE)pLNStart - pVolInfo->ulBlockSize);

               //if (ulCluster == 1)
               //   {
               //   // reading the root directory in case of FAT12/FAT16
               //   ulSector += pVolInfo->SectorsPerCluster;
               //   usSectorsRead += pVolInfo->SectorsPerCluster;
               //   if ((ULONG)usSectorsRead * pVolInfo->BootSect.bpb.BytesPerSector >=
               //       (ULONG)pVolInfo->BootSect.bpb.RootDirEntries * sizeof(DIRENTRY))
               //      // root directory ended
               //      ulNextCluster = 0;
               //   else
               //      ulNextCluster = 1;
               //   }
               //else
                  ulNextCluster = GetNextCluster(pVolInfo, pDirSHInfo, ulCluster);
               if (!ulNextCluster)
                  ulNextCluster = pVolInfo->ulFatEof;
               if (ulNextCluster == pVolInfo->ulFatEof)
                  {
                  if (usMode == MODIFY_DIR_UPDATE ||
                      usMode == MODIFY_DIR_DELETE ||
                      usMode == MODIFY_DIR_RENAME)
                     {
                     if (ulBlock == pVolInfo->ulClusterSize / pVolInfo->ulBlockSize - 1)
                        {
                        free(szLongName);
                        free(pDirNew);
                        free(pDirectory);
                        return ERROR_FILE_NOT_FOUND;
                        }
                     else
                        {
                        if (ulBlock == pVolInfo->ulClusterSize / pVolInfo->ulBlockSize - 1)
                           ulCluster = ulNextCluster;
                        continue;
                        }
                     }
                  else
                     {
                     //if (ulCluster == 1)
                     //   {
                     //   // no expanding for root directory in case of FAT12/FAT16
                     //   ulNextCluster = pVolInfo->ulFatEof;
                     //   }
                     //else
                        ulNextCluster = SetNextCluster(pVolInfo, ulCluster, FAT_ASSIGN_NEW);
                     if (ulNextCluster == pVolInfo->ulFatEof)
                        {
                        free(szLongName);
                        free(pDirNew);
                        free(pDirectory);
                        return ERROR_DISK_FULL;
                        }
                     fNewCluster = TRUE;
                     }
                  }
               ulCluster = ulNextCluster;

               // clear the new cluster
               if (fNewCluster)
                  {
                  ULONG ulBlock2;
                  memset(pDir2, 0, (size_t)pVolInfo->ulBlockSize);
                  for (ulBlock2 = 0; ulBlock2 < pVolInfo->ulClusterSize / pVolInfo->ulBlockSize; ulBlock2++)
                     {
                     rc = WriteBlock(pVolInfo, ulCluster, ulBlock2, pDir2, usIOMode);
                     if (rc)
                        {
                        free(szLongName);
                        free(pDirNew);
                        free(pDirectory);
                        return rc;
                        }
                     }
                  }
               break;
               }
            if (ulCluster == pVolInfo->ulFatEof)
               break;
            if (ulBlock == pVolInfo->ulClusterSize / pVolInfo->ulBlockSize - 1)
               ulCluster = ulNextCluster;
            }
         if (ulCluster == pVolInfo->ulFatEof)
            break;
         if (ulBlock == pVolInfo->ulClusterSize / pVolInfo->ulBlockSize - 1)
            ulCluster = ulNextCluster;
         }

      free(szLongName);
      free(pDirNew);
      free(pDirectory);
      return 0;
      }
#endif
}
