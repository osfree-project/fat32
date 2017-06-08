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
USHORT GetChkSum16(const char *data, int bytes);
ULONG GetChkSum32(const char *data, int bytes);

VOID MarkFreeEntries(PDIRENTRY pDirBlock, ULONG ulSize);
VOID MarkFreeEntries1(PDIRENTRY1 pDirBlock, ULONG ulSize);
PDIRENTRY fSetLongName(PDIRENTRY pDir, PSZ pszName, BYTE bCheck);
PDIRENTRY1 fSetLongName1(PDIRENTRY1 pDir, PSZ pszLongName, PUSHORT pusNameHash);
PDIRENTRY CompactDir(PDIRENTRY pStart, ULONG ulSize, USHORT usNeededEntries);
PDIRENTRY1 CompactDir1(PDIRENTRY1 pStart, ULONG ulSize, USHORT usEntriesNeeded);
USHORT GetFreeEntries(PDIRENTRY pDirBlock, ULONG ulSize);
USHORT DBCSStrlen( const PSZ pszStr );

USHORT GetBuf1Access(PVOLINFO pVolInfo, PSZ pszName);
VOID   ReleaseBuf1(PVOLINFO pVolInfo);
USHORT GetBuf2Access(PVOLINFO pVolInfo, PSZ pszName);
VOID   ReleaseBuf2(PVOLINFO pVolInfo);
USHORT GetBuf3Access(PVOLINFO pVolInfo, PSZ pszName);
VOID   ReleaseBuf3(PVOLINFO pVolInfo);

static ULONG ulSemRWBuf1 = 0UL;
static ULONG ulSemRWBuf2 = 0UL;
static ULONG ulSemRWBuf3 = 0UL;

#ifdef USE_STATIC_BUFS
static BYTE _based(_segname("fat32_buf1_DATA")) pbDirBuf1[0x10000] = {0};
static BYTE _based(_segname("fat32_buf2_DATA")) pbDirBuf2[0x8000]  = {0};
static BYTE _based(_segname("fat32_buf2_DATA")) pbDirBuf3[0x8000]  = {0};
#endif

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
BYTE   szDir[FAT32MAXPATH];
ULONG  ulCluster;
ULONG  ulCluster2;
DIRENTRY DirEntry;
PSZ    p;

   if (f32Parms.fMessageActive & LOG_FUNCS)
      Message("FindDirCluster for %s, CurDirEnd %u, AttrWanted %u", pDir, usCurDirEnd, usAttrWanted );

   if (pcdfsi &&
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
      if (pStreamEntry)
         {
         // fill the Stream entry for root directory
         memset(pStreamEntry, 0, sizeof(DIRENTRY1));
#ifdef EXFAT
         if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
            {
            pStreamEntry->u.Stream.bNoFatChain = 0;
            pStreamEntry->u.Stream.ulFirstClus = ulCluster;
            }
#endif
         }
      if (strlen(pDir) >= 2)
         {
         if (pDir[1] == ':')
            pDir += 2;
         }
      }

   if (*pDir == '\\')
      pDir++;

   p = strrchr(pDir, '\\');
   if (!p)
      p = pDir;
   memset(szDir, 0, sizeof szDir);
   memcpy(szDir, pDir, p - pDir);
   if (*p && p != pDir)
      pDir = p + 1;
   else
      pDir = p;

   if (pDirEnd)
      *pDirEnd = pDir;

   ulCluster = FindPathCluster(pVolInfo, ulCluster, szDir, NULL, &DirEntry, pStreamEntry, NULL); ////
   if (ulCluster == pVolInfo->ulFatEof)
      {
      if (f32Parms.fMessageActive & LOG_FUNCS)
         Message("FindDirCluster for '%s', not found", szDir);
      return pVolInfo->ulFatEof;
      }
   if ( ulCluster != pVolInfo->ulFatEof &&
#ifdef EXFAT
       ( ( (pVolInfo->bFatType < FAT_TYPE_EXFAT)  && !(DirEntry.bAttr & FILE_DIRECTORY) ) ||
         ( (pVolInfo->bFatType == FAT_TYPE_EXFAT) && !(((PDIRENTRY1)&DirEntry)->u.File.usFileAttr & FILE_DIRECTORY) ) ) )
#else
       !(DirEntry.bAttr & FILE_DIRECTORY) )
#endif
      {
      if (f32Parms.fMessageActive & LOG_FUNCS)
         Message("FindDirCluster for '%s', not a directory", szDir);
      return pVolInfo->ulFatEof;
      }

   if (*pDir)
      {
      if (usAttrWanted != RETURN_PARENT_DIR && !strpbrk(pDir, "?*"))
         {
         ulCluster2 = FindPathCluster(pVolInfo, ulCluster, pDir, NULL, &DirEntry, pStreamEntry, NULL); ////
         if ( ulCluster2 != pVolInfo->ulFatEof &&
#ifdef EXFAT
             ( (pVolInfo->bFatType < FAT_TYPE_EXFAT)  && ((DirEntry.bAttr & usAttrWanted) == usAttrWanted) ) ||
               ( (pVolInfo->bFatType == FAT_TYPE_EXFAT) && ((((PDIRENTRY1)&DirEntry)->u.File.usFileAttr & usAttrWanted) == usAttrWanted) ) )
#else
             ( (DirEntry.bAttr & usAttrWanted) == usAttrWanted ) )
#endif
            {
            if (pDirEnd)
               *pDirEnd = pDir + strlen(pDir);
            return ulCluster2;
            }
         }
      }

   return ulCluster;
}

#define MODE_START  0
#define MODE_RETURN 1
#define MODE_SCAN   2

/******************************************************************
*
******************************************************************/
ULONG FindPathCluster0(PVOLINFO pVolInfo, ULONG ulCluster, PSZ pszPath, PDIRENTRY pDirEntry, PSZ pszFullName)
{
// FAT12/FAT16/FAT32 case
BYTE szShortName[13];
#ifdef USE_STATIC_BUFS
static BYTE szLongName[FAT32MAXPATHCOMP * 2];
PSZ  pszLongName = szLongName;
#else
PSZ  pszLongName;
#endif
PSZ  pszPart;
PSZ  p;
PDIRENTRY pDir;
#ifdef USE_STATIC_BUFS
PDIRENTRY pDirStart = (PDIRENTRY)pbDirBuf3;
#else
PDIRENTRY pDirStart;
#endif
PDIRENTRY pDirEnd;
BOOL fFound;
USHORT usMode;
BYTE   bCheck;
PROCINFO ProcInfo;
ULONG  ulSector;
USHORT usSectorsRead;
USHORT usSectorsPerBlock;
USHORT usDirEntries = 0;
USHORT usMaxDirEntries = (USHORT)(pVolInfo->ulBlockSize / sizeof(DIRENTRY));

   if (f32Parms.fMessageActive & LOG_FUNCS)
      Message("FindPathCluster for %s, dircluster %lu", pszPath, ulCluster);

   if (ulCluster == 1)
      {
      // root directory starting sector
      ulSector = pVolInfo->BootSect.bpb.ReservedSectors +
         pVolInfo->BootSect.bpb.SectorsPerFat * pVolInfo->BootSect.bpb.NumberOfFATs;
      usSectorsPerBlock = (USHORT)pVolInfo->SectorsPerCluster /
         (USHORT)(pVolInfo->ulClusterSize / pVolInfo->ulBlockSize);
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

#ifndef USE_STATIC_BUFS
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
#endif
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
#ifndef USE_STATIC_BUFS
         free(pDirStart);
         free(pszLongName);
#endif
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
               ReadBlock(pVolInfo, ulCluster, ulBlock, pDirStart, 0); ////
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
      }
#ifndef USE_STATIC_BUFS
   free(pDirStart);
   free(pszLongName);
#endif
   if (f32Parms.fMessageActive & LOG_FUNCS)
      {
      if (ulCluster != pVolInfo->ulFatEof)
         Message("FindPathCluster for %s found cluster %ld", pszPath, ulCluster);
      else
         Message("FindPathCluster for %s returned EOF", pszPath);
      }
   return ulCluster;
}

#ifdef EXFAT

/******************************************************************
*
******************************************************************/
ULONG FindPathCluster1(PVOLINFO pVolInfo, ULONG ulCluster, PSZ pszPath, PSHOPENINFO pSHInfo,
                       PDIRENTRY1 pDirEntry, PDIRENTRY1 pDirEntryStream,
                       PSZ pszFullName)
{
// exFAT case
#ifdef USE_STATIC_BUFS
static BYTE szLongName[FAT32MAXPATHCOMP * 2];
PSZ  pszLongName = szLongName;
#else
PSZ  pszLongName;
#endif
PSZ  pszPart;
PSZ  p;
DIRENTRY1  Dir;
PDIRENTRY1 pDir;
#ifdef USE_STATIC_BUFS
PDIRENTRY1 pDirStart = (PDIRENTRY1)pbDirBuf3;
#else
PDIRENTRY1 pDirStart;
#endif
PDIRENTRY1 pDirEnd;
BOOL fFound;
USHORT usMode;
PROCINFO ProcInfo;
USHORT usDirEntries = 0;
USHORT usMaxDirEntries = (USHORT)(pVolInfo->ulBlockSize / sizeof(DIRENTRY1));
ULONG  ulRet;
USHORT usNumSecondary;
USHORT usFileAttr;

   if (f32Parms.fMessageActive & LOG_FUNCS)
      Message("FindPathCluster for %s, dircluster %lu", pszPath, ulCluster);

   if (pDirEntry)
      {
      memset(pDirEntry, 0, sizeof (DIRENTRY1));
      pDirEntry->u.File.usFileAttr = FILE_DIRECTORY;
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

#ifndef USE_STATIC_BUFS
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
#endif
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
#ifndef USE_STATIC_BUFS
         free(pDirStart);
         free(pszLongName);
#endif
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
                              if (pDirEntry)
                                 memcpy(pDirEntry, &Dir, sizeof (DIRENTRY1));
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
#ifndef USE_STATIC_BUFS
   free(pDirStart);
   free(pszLongName);
#endif
   if (f32Parms.fMessageActive & LOG_FUNCS)
      {
      if (ulCluster != pVolInfo->ulFatEof)
         Message("FindPathCluster for %s found cluster %ld", pszPath, ulCluster);
      else
         Message("FindPathCluster for %s returned EOF", pszPath);
      }
   return ulCluster;
}

#endif

/******************************************************************
*
******************************************************************/
ULONG FindPathCluster(PVOLINFO pVolInfo, ULONG ulCluster, PSZ pszPath, PSHOPENINFO pSHInfo,
                      PDIRENTRY pDirEntry, PDIRENTRY1 pDirEntryStream,
                      PSZ pszFullName)
{
ULONG rc;

#ifdef USE_STATIC_BUFS
   if (!GetBuf3Access(pVolInfo, "FindPathCluster"))
      {
#endif
#ifdef EXFAT
      if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
#endif
         rc = FindPathCluster0(pVolInfo, ulCluster, pszPath, pDirEntry, pszFullName);
#ifdef EXFAT
      else
         rc = FindPathCluster1(pVolInfo, ulCluster, pszPath, pSHInfo,
            (PDIRENTRY1)pDirEntry, (PDIRENTRY1)pDirEntryStream, pszFullName);
#endif

#ifdef USE_STATIC_BUFS
      ReleaseBuf3(pVolInfo);
      }
#endif

   return rc;
}

USHORT TranslateName2(PVOLINFO pVolInfo, ULONG ulDirCluster, PSZ pszPath, PSZ pszTarget, USHORT usTranslate)
{
BYTE szShortName[13];
#ifdef USE_STATIC_BUFS
static BYTE szLongName[FAT32MAXPATHCOMP * 4];
PSZ  pszLongName = szLongName;
#else
PSZ  pszLongName;
#endif
PSZ  pszUpperName;
PSZ  pszUpperPart;
PSZ  pszPart;
PSZ  p;
/* PSZ  pTar = pszTarget; */
PDIRENTRY pDir;
#ifdef USE_STATIC_BUFS
PDIRENTRY pDirStart = (PDIRENTRY)pbDirBuf2;
#else
PDIRENTRY pDirStart;
#endif
PDIRENTRY pDirEnd;
BOOL fFound;
USHORT usMode;
BYTE   bCheck;
ULONG  ulCluster;
ULONG  ulSector;
USHORT usSectorsRead;
USHORT usSectorsPerBlock;
ULONG  ulDirEntries = 0;

   if (f32Parms.fMessageActive & LOG_FUNCS)
      Message("TranslateName: %s", pszPath);

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

#ifndef USE_STATIC_BUFS
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
#endif
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
         pVolInfo->BootSect.bpb.SectorsPerFat * pVolInfo->BootSect.bpb.NumberOfFATs;
      usSectorsPerBlock = (USHORT)pVolInfo->SectorsPerCluster /
         (USHORT)(pVolInfo->ulClusterSize / pVolInfo->ulBlockSize);
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
#ifndef USE_STATIC_BUFS
         free(pDirStart);
         free(pszLongName);
#endif
         return ERROR_BUFFER_OVERFLOW;
         }

      memset(pszPart, 0, FAT32MAXPATHCOMP);
      memcpy(pszPart, pszPath, p - pszPath);
      FSH_UPPERCASE(pszPart, FAT32MAXPATHCOMP, pszUpperPart);
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

            while (usMode == MODE_SCAN && pDir < pDirEnd)
               {
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
            if (usSectorsRead * pVolInfo->BootSect.bpb.BytesPerSector >=
                pVolInfo->BootSect.bpb.RootDirEntries * sizeof(DIRENTRY))
               // root directory ended
               ulCluster = 0;
            }
         else
            ulCluster = GetNextCluster(pVolInfo, NULL, ulCluster);
         if (!ulCluster)
            ulCluster = pVolInfo->ulFatEof;
         if (ulCluster == pVolInfo->ulFatEof)
            strcat(pszTarget, pszPart);
         }
      }

#ifndef USE_STATIC_BUFS
   free(pDirStart);
   free(pszLongName);
#endif
   if (ulCluster == pVolInfo->ulFatEof)
      strcat(pszTarget, pszPath);
   return 0;
}


USHORT TranslateName(PVOLINFO pVolInfo, ULONG ulDirCluster, PSZ pszPath, PSZ pszTarget, USHORT usTranslate)
{
USHORT rc;

#ifdef USE_STATIC_BUFS
   if (!GetBuf2Access(pVolInfo, "TranslateName"))
      {
#endif
      rc = TranslateName2(pVolInfo, ulDirCluster, pszPath, pszTarget, usTranslate);
#ifdef USE_STATIC_BUFS
      ReleaseBuf2(pVolInfo);
      }
#endif

   return rc;
}

USHORT ModifyDirectory0(PVOLINFO pVolInfo, ULONG ulDirCluster,
                       USHORT usMode, PDIRENTRY pOld, PDIRENTRY pNew,
                       PSZ pszLongName, USHORT usIOMode)
{
#ifdef USE_STATIC_BUFS
PDIRENTRY pDirectory = (PDIRENTRY)pbDirBuf1;
#else
PDIRENTRY pDirectory;
#endif
PDIRENTRY pDir2;
PDIRENTRY pWork, pWork2;
PDIRENTRY pMax;
USHORT    usEntriesNeeded;
USHORT    usFreeEntries;
DIRENTRY  DirNew;
ULONG     ulCluster;
ULONG     ulPrevCluster;
ULONG     ulPrevBlock;
ULONG     ulNextCluster = pVolInfo->ulFatEof;
PDIRENTRY pLNStart;
USHORT    rc;
USHORT    usClusterCount;
BOOL      fNewCluster;
ULONG     ulSector;
ULONG     ulPrevSector;
USHORT    usSectorsRead;
USHORT    usSectorsPerBlock;
ULONG     ulBytesToRead;
ULONG     ulPrevBytesToRead = 0;
ULONG     ulBytesRemained;

   if (f32Parms.fMessageActive & LOG_FUNCS)
      Message("ModifyDirectory DirCluster %ld, Mode = %d",
      ulDirCluster, usMode);

   if (usMode == MODIFY_DIR_RENAME ||
       usMode == MODIFY_DIR_INSERT)
      {
      if (!pNew || !pszLongName)
         {
         Message("Modify directory: Invalid parameters 1");
         return ERROR_INVALID_PARAMETER;
         }

      memcpy(&DirNew, pNew, sizeof (DIRENTRY));
      if ((pNew->bAttr & 0x0F) != FILE_VOLID)
         {
         rc = MakeShortName(pVolInfo, ulDirCluster, pszLongName, DirNew.bFileName);
         if (rc == LONGNAME_ERROR)
            {
            Message("Modify directory: Longname error");
            return ERROR_FILE_EXISTS;
            }
         memcpy(pNew, &DirNew, sizeof (DIRENTRY));

         if (rc == LONGNAME_OFF)
            pszLongName = NULL;
         }
      else
         pszLongName = NULL;

      usEntriesNeeded = 1;
      if (pszLongName)
#if 0
         usEntriesNeeded += strlen(pszLongName) / 13 +
            (strlen(pszLongName) % 13 ? 1 : 0);
#else
         usEntriesNeeded += ( DBCSStrlen( pszLongName ) + 12 ) / 13;
#endif
      }

   if (usMode == MODIFY_DIR_RENAME ||
       usMode == MODIFY_DIR_DELETE ||
       usMode == MODIFY_DIR_UPDATE)
      {
      if (!pOld)
         {
         Message("Modify directory: Invalid parameter 2 ");
         return ERROR_INVALID_PARAMETER;
         }
      }

#ifndef USE_STATIC_BUFS
   pDirectory = (PDIRENTRY)malloc(2 * (size_t)pVolInfo->ulBlockSize);
   if (!pDirectory)
      {
      Message("Modify directory: Not enough memory");
      return ERROR_NOT_ENOUGH_MEMORY;
      }
#endif
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
         pVolInfo->BootSect.bpb.SectorsPerFat * pVolInfo->BootSect.bpb.NumberOfFATs;
      usSectorsPerBlock = (USHORT)pVolInfo->SectorsPerCluster /
         (USHORT)(pVolInfo->ulClusterSize / pVolInfo->ulBlockSize);
      usSectorsRead = 0;
      ulBytesRemained = pVolInfo->BootSect.bpb.RootDirEntries * sizeof(DIRENTRY);
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
#ifndef USE_STATIC_BUFS
               free(pDirectory);
#endif
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
                        if (f32Parms.fMessageActive & LOG_FUNCS)
                           Message(" Updating cluster");
                        memcpy(pWork, pNew, sizeof (DIRENTRY));
                        if (ulCluster == 1)
                           // reading root directory on FAT12/FAT16
                           rc = WriteSector(pVolInfo, ulSector + ulBlock * usSectorsPerBlock, usSectorsPerBlock, (void *)pDir2, usIOMode);
                        else
                           rc = WriteBlock(pVolInfo, ulCluster, ulBlock, pDir2, usIOMode);
                        if (rc)
                           {
#ifndef USE_STATIC_BUFS
                           free(pDirectory);
#endif
                           return rc;
                           }
                        ulCluster = pVolInfo->ulFatEof;
                        break;

                     case MODIFY_DIR_DELETE:
                     case MODIFY_DIR_RENAME:
                        if (f32Parms.fMessageActive & LOG_FUNCS)
                           Message(" Removing entry from cluster");
                        pWork2 = pLNStart;
                        while (pWork2 < pWork)
                           {
                           if (f32Parms.fMessageActive & LOG_FUNCS)
                              Message("Deleting Longname entry.");
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
#ifndef USE_STATIC_BUFS
                              free(pDirectory);
#endif
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
#ifndef USE_STATIC_BUFS
                           free(pDirectory);
#endif
                           return rc;
                           }

                        if (usMode == MODIFY_DIR_DELETE)
                           ulCluster = pVolInfo->ulFatEof;
                        else
                           {
                           usMode = MODIFY_DIR_INSERT;
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
                  BYTE bCheck = GetVFATCheckSum(&DirNew);

                  if (f32Parms.fMessageActive & LOG_FUNCS)
                     Message(" Inserting entry into 2 clusters");

                  pWork = CompactDir(pDirectory, ulPrevBytesToRead + ulBytesToRead, usEntriesNeeded);
                  pWork = fSetLongName(pWork, pszLongName, bCheck);
                  memcpy(pWork, &DirNew, sizeof (DIRENTRY));

                  if (ulPrevCluster == 1)
                     // reading root directory on FAT12/FAT16
                     rc = WriteSector(pVolInfo, ulPrevSector + ulPrevBlock * usSectorsPerBlock, usSectorsPerBlock, (void *)pDirectory, usIOMode);
                  else
                     rc = WriteBlock(pVolInfo, ulPrevCluster, ulPrevBlock, pDirectory, usIOMode);
                  if (rc)
                     {
#ifndef USE_STATIC_BUFS
                     free(pDirectory);
#endif
                     return rc;
                     }

                  if (ulCluster == 1)
                     // reading root directory on FAT12/FAT16
                     rc = WriteSector(pVolInfo, ulSector + ulBlock * usSectorsPerBlock, usSectorsPerBlock, (void *)pDir2, usIOMode);
                  else
                     rc = WriteBlock(pVolInfo, ulCluster, ulBlock, pDir2, usIOMode);
                  if (rc)
                     {
#ifndef USE_STATIC_BUFS
                     free(pDirectory);
#endif
                     return rc;
                     }
                  ulCluster = pVolInfo->ulFatEof;
                  break;
                  }

               usFreeEntries = GetFreeEntries(pDir2, ulBytesToRead);
               if (usFreeEntries >= usEntriesNeeded)
                  {
                  BYTE bCheck = GetVFATCheckSum(&DirNew);

                  if (f32Parms.fMessageActive & LOG_FUNCS)
                     Message(" Inserting entry into 1 cluster");

                  pWork = CompactDir(pDir2, ulBytesToRead, usEntriesNeeded);
                  pWork = fSetLongName(pWork, pszLongName, bCheck);
                  memcpy(pWork, &DirNew, sizeof (DIRENTRY));
                  if (ulCluster == 1)
                     // reading root directory on FAT12/FAT16
                     rc = WriteSector(pVolInfo, ulSector + ulBlock * usSectorsPerBlock, usSectorsPerBlock, (void *)pDir2, usIOMode);
                  else
                     rc = WriteBlock(pVolInfo, ulCluster, ulBlock, pDir2, usIOMode);
                  if (rc)
                     {
#ifndef USE_STATIC_BUFS
                     free(pDirectory);
#endif
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
#ifndef USE_STATIC_BUFS
                     free(pDirectory);
#endif
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
            memset(pDirectory, 0, (size_t)pVolInfo->ulClusterSize);
            memmove(pDirectory, pDir2, (size_t)ulBytesToRead);
            if (pLNStart)
               pLNStart = (PDIRENTRY)((PBYTE)pLNStart - pVolInfo->ulBlockSize);

            if (ulCluster == 1)
               {
               // reading the root directory in case of FAT12/FAT16
               ulSector += pVolInfo->SectorsPerCluster;
               usSectorsRead += pVolInfo->SectorsPerCluster;
               if (usSectorsRead * pVolInfo->BootSect.bpb.BytesPerSector >=
                   pVolInfo->BootSect.bpb.RootDirEntries * sizeof(DIRENTRY))
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
#ifndef USE_STATIC_BUFS
                     free(pDirectory);
#endif
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
#ifndef USE_STATIC_BUFS
                     free(pDirectory);
#endif
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
#ifndef USE_STATIC_BUFS
                     free(pDirectory);
#endif
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

#ifndef USE_STATIC_BUFS
   free(pDirectory);
#endif
   return 0;
}

#ifdef EXFAT

USHORT GetChkSum16(const char *data, int bytes)
{
   USHORT chksum = 0;
   int i;

   for (i = 0; i < bytes; i++)
      {
      if (i == 2)
         // skip checksum field
         i++;
      else
         chksum = (chksum << 15) | (chksum >> 1) + data[i];
      }

   return chksum;
}

ULONG GetChkSum32(const char *data, int bytes)
{
   ULONG chksum = 0;
   int i;

   for (i = 0; i < bytes; i++)
      chksum = (chksum << 31) | (chksum >> 1) + data[i];

   return chksum;
}

USHORT NameHash(USHORT *pszFilename, int NameLen)
{
   USHORT hash = 0;
   UCHAR  *data = (UCHAR *)pszFilename;
   int i;

   for (i = 0; i < NameLen * 2; i++)
      hash = (hash << 15) | (hash >> 1) + data[i];

   return hash;
}


USHORT ModifyDirectory1(PVOLINFO pVolInfo, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo,
                       USHORT usMode, PDIRENTRY1 pOld, PDIRENTRY1 pNew,
                       PDIRENTRY1 pStreamOld, PDIRENTRY1 pStreamNew,
                       PSZ pszLongName, USHORT usIOMode)
{
#ifdef USE_STATIC_BUFS
PDIRENTRY1 pDirectory = (PDIRENTRY1)pbDirBuf1;
#else
PDIRENTRY1 pDirectory;
#endif
PDIRENTRY1 pDir2, pDir1;
PDIRENTRY1 pWork, pWork2, pWorkStream, pWorkFile;
PDIRENTRY1 pMax;
USHORT    usEntriesNeeded;
USHORT    usFreeEntries;
DIRENTRY1  DirNew;
ULONG     ulCluster;
ULONG     ulPrevCluster;
ULONG     ulPrevBlock;
ULONG     ulNextCluster = pVolInfo->ulFatEof;
PDIRENTRY1 pLNStart;
USHORT    rc;
USHORT    usClusterCount;
BOOL      fNewCluster;
ULONG     ulSector;
ULONG     ulPrevSector;
//USHORT    usSectorsRead;
//USHORT    usSectorsPerBlock;
ULONG     ulBytesToRead;
ULONG     ulPrevBytesToRead = 0;
ULONG     ulBytesRemained;
USHORT    usNumSecondary;
USHORT    usFileName, usIndex;
USHORT    usNameHash;
BOOL      fFound;

   if (f32Parms.fMessageActive & LOG_FUNCS)
      Message("ModifyDirectory DirCluster %ld, Mode = %d",
      ulDirCluster, usMode);

   if (usMode == MODIFY_DIR_RENAME ||
       usMode == MODIFY_DIR_INSERT)
      {
      if (!pNew || !pszLongName)
         {
         return ERROR_INVALID_PARAMETER;
         }

      memcpy(&DirNew, pNew, sizeof (DIRENTRY1));
      /* if ((pNew->bAttr & 0x0F) != FILE_VOLID)
         {
         rc = MakeShortName(pVolInfo, ulDirCluster, pszLongName, DirNew.bFileName);
         if (rc == LONGNAME_ERROR)
            {
            Message("Modify directory: Longname error");
            return ERROR_FILE_EXISTS;
            }
         memcpy(pNew, &DirNew, sizeof (DIRENTRY));

         if (rc == LONGNAME_OFF)
            pszLongName = NULL;
         }
      else
         pszLongName = NULL; */

      //usEntriesNeeded = 1;
      usEntriesNeeded = 2;
      if (pszLongName)
#if 0
         usEntriesNeeded += strlen(pszLongName) / 13 +
            (strlen(pszLongName) % 13 ? 1 : 0);
#else
         //usEntriesNeeded += ( DBCSStrlen( pszLongName ) + 12 ) / 13;
         usEntriesNeeded += ( DBCSStrlen( pszLongName ) + 14 ) / 15;
#endif
      }

   if (usMode == MODIFY_DIR_RENAME ||
       usMode == MODIFY_DIR_DELETE ||
       usMode == MODIFY_DIR_UPDATE)
      {
      if (!pOld)
         {
         return ERROR_INVALID_PARAMETER;
         }
      }

#ifndef USE_STATIC_BUFS
   pDirectory = (PDIRENTRY1)malloc(2 * (size_t)pVolInfo->ulBlockSize);
   if (!pDirectory)
      {
      return ERROR_NOT_ENOUGH_MEMORY;
      }
#endif
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
   //      pVolInfo->BootSect.bpb.SectorsPerFat * pVolInfo->BootSect.bpb.NumberOfFATs;
   //   usSectorsPerBlock = pVolInfo->SectorsPerCluster /
   //      (pVolInfo->ulClusterSize / pVolInfo->ulBlockSize);
   //   usSectorsRead = 0;
   //   ulBytesRemained = pVolInfo->BootSect.bpb.RootDirEntries * sizeof(DIRENTRY);
   //   }

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
               rc = ReadBlock(pVolInfo, ulCluster, ulBlock, pDir2, usIOMode);
               ulBytesToRead = pVolInfo->ulBlockSize;
            //   }
            if (rc)
               {
#ifndef USE_STATIC_BUFS
               free(pDirectory);
#endif
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

               /*
                  Find old entry
               */

               fFound = FALSE;
               pWork = pDir2;
               pDir1 = NULL;
               while (pWork != pMax)
                  {
                  //if (pWork->bFileName[0] && pWork->bFileName[0] != DELETED_ENTRY)
                  if ((pWork->bEntryType != ENTRY_TYPE_EOD) && (pWork->bEntryType & ENTRY_TYPE_IN_USE_STATUS))
                     {
                     //if (pWork->bAttr == FILE_LONGNAME)
                     if (pWork->bEntryType == ENTRY_TYPE_FILE_NAME)
                        {
                        //usNumSecondary--;
                        //fGetLongName1(pDir, szLongName, sizeof szLongName);
                        if (!pLNStart && !usFileName && fFound)
                           {
                           pLNStart = pWork;
                           break;
                           }
                        usFileName++;
                        }
                     else if (pWork->bEntryType == ENTRY_TYPE_FILE)
                        {
                        usFileName = 0;
                        usNumSecondary = pWork->u.File.bSecondaryCount;
                        pWorkFile = pWork;
                        }
                     //else if ((pWork->bAttr & 0x0F) != FILE_VOLID)
                     else if (pWork->bEntryType == ENTRY_TYPE_STREAM_EXT)
                        {
                        //usNumSecondary--;
                        //if (!memcmp(pWork->bFileName, pOld->bFileName, 11) &&
                        //    pWork->wCluster     == pOld->wCluster &&
                        //    pWork->wClusterHigh == pOld->wClusterHigh)
                        pWorkStream = pWork;
                        if (pWork->u.Stream.ulFirstClus == pStreamOld->u.Stream.ulFirstClus)
                           {
                           fFound = TRUE;
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
                        if (f32Parms.fMessageActive & LOG_FUNCS)
                           Message(" Updating cluster");
                        memcpy(pWorkFile, pNew, sizeof (DIRENTRY1));
                        if (pStreamNew)
                           {
                           memcpy(pWorkStream, pStreamNew, sizeof (DIRENTRY1));
                           }
                        pWorkFile->u.File.usSetCheckSum = GetChkSum16((char *)pWorkFile,
                           sizeof(DIRENTRY1) * (pWorkFile->u.File.bSecondaryCount + 1));
                        //if (ulCluster == 1)
                        //   // reading root directory on FAT12/FAT16
                        //   rc = WriteSector(pVolInfo, ulSector + ulBlock * usSectorsPerBlock, usSectorsPerBlock, (void *)pDir2, usIOMode);
                        //else
                           rc = WriteBlock(pVolInfo, ulCluster, ulBlock, pDir2, usIOMode);
                        if (rc)
                           {
#ifndef USE_STATIC_BUFS
                           free(pDirectory);
#endif
                           return rc;
                           }
                        ulCluster = pVolInfo->ulFatEof;
                        break;

                     case MODIFY_DIR_DELETE:
                     case MODIFY_DIR_RENAME:
                        if (f32Parms.fMessageActive & LOG_FUNCS)
                           Message(" Removing entry from cluster");
                        pWork2 = pLNStart;
                        //while (pWork2 < pWork)
                        for (usIndex = 0; usIndex < usNumSecondary - 1; usIndex++)
                           {
                           if (f32Parms.fMessageActive & LOG_FUNCS)
                              Message("Deleting Longname entry.");
                           //pWork2->bFileName[0] = DELETED_ENTRY;
                           pWork2->bEntryType &= ~ENTRY_TYPE_IN_USE_STATUS;
                           pWork2++;
                           }
                        //pWork->bFileName[0] = DELETED_ENTRY;
                        pWorkFile->bEntryType &= ~ENTRY_TYPE_IN_USE_STATUS;
                        pWorkStream->bEntryType &= ~ENTRY_TYPE_IN_USE_STATUS;

                        /*
                           Write previous cluster if LN start lies there
                        */
                        //if (ulPrevCluster != pVolInfo->ulFatEof &&
                        //   pLNStart < pDir2)
                        //   {
                        //   if (ulPrevCluster == 1)
                        //      // reading root directory on FAT12/FAT16
                        //      rc = WriteSector(pVolInfo, ulPrevSector + ulPrevBlock * usSectorsPerBlock, usSectorsPerBlock, (void *)pDirectory, usIOMode);
                        //   else
                        //      rc = WriteBlock(pVolInfo, ulPrevCluster, ulPrevBlock, pDirectory, usIOMode);
                        //   if (rc)
                        //      {
#ifndef USE_STATIC_BUFS
                        //      free(pDirectory);
#endif
                        //      return rc;
                        //      }
                        //   }

                        /*
                           Write current cluster
                        */
                        //if (ulCluster == 1)
                        //   // reading root directory on FAT12/FAT16
                        //   rc = WriteSector(pVolInfo, ulSector + ulBlock * usSectorsPerBlock, usSectorsPerBlock, (void *)pDir2, usIOMode);
                        //else
                           rc = WriteBlock(pVolInfo, ulCluster, ulBlock, pDir2, usIOMode);
                        if (rc)
                           {
#ifndef USE_STATIC_BUFS
                           free(pDirectory);
#endif
                           return rc;
                           }

                        if (usMode == MODIFY_DIR_DELETE)
                           ulCluster = pVolInfo->ulFatEof;
                        else
                           {
                           usMode = MODIFY_DIR_INSERT;
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
               pNew->bEntryType = ENTRY_TYPE_FILE;
               pStreamNew->bEntryType = ENTRY_TYPE_STREAM_EXT;

               if (ulPrevCluster != pVolInfo->ulFatEof && GetFreeEntries((PDIRENTRY)pDirectory, ulPrevBytesToRead + ulBytesToRead) >= usEntriesNeeded)
                  {
                  PDIRENTRY1 pWork3;
                  //BYTE bCheck = GetVFATCheckSum(&DirNew);

                  if (f32Parms.fMessageActive & LOG_FUNCS)
                     Message(" Inserting entry into 2 clusters");

                  pWork = CompactDir1(pDirectory, ulPrevBytesToRead + ulBytesToRead, usEntriesNeeded);
                  //pWork = fSetLongName(pWork, pszLongName, bCheck);
                  //memcpy(pWork, &DirNew, sizeof (DIRENTRY));
                  pWork3 = fSetLongName1(pWork+2, pszLongName, &usNameHash);
                  pNew->u.File.bSecondaryCount = (BYTE)(pWork3 - pWork - 1);
                  memcpy(pWork++, pNew, sizeof (DIRENTRY1));
                  pStreamNew->u.Stream.usNameHash = usNameHash;
                  pStreamNew->u.Stream.bNameLen = (BYTE)strlen(pszLongName);
                  memcpy(pWork++, pStreamNew, sizeof (DIRENTRY1));
                  (pWork-2)->u.File.usSetCheckSum = GetChkSum16((char *)(pWork-2),
                     sizeof(DIRENTRY1) * ((pWork-2)->u.File.bSecondaryCount + 1));

                  //if (ulPrevCluster == 1)
                  //   // reading root directory on FAT12/FAT16
                  //   rc = WriteSector(pVolInfo, ulPrevSector + ulPrevBlock * usSectorsPerBlock, usSectorsPerBlock, (void *)pDirectory, usIOMode);
                  //else
                     rc = WriteBlock(pVolInfo, ulPrevCluster, ulPrevBlock, pDirectory, usIOMode);
                  if (rc)
                     {
#ifndef USE_STATIC_BUFS
                     free(pDirectory);
#endif
                     return rc;
                     }

                  //if (ulCluster == 1)
                  //   // reading root directory on FAT12/FAT16
                  //   rc = WriteSector(pVolInfo, ulSector + ulBlock * usSectorsPerBlock, usSectorsPerBlock, (void *)pDir2, usIOMode);
                  //else
                     rc = WriteBlock(pVolInfo, ulCluster, ulBlock, pDir2, usIOMode);
                  if (rc)
                     {
#ifndef USE_STATIC_BUFS
                     free(pDirectory);
#endif
                     return rc;
                     }
                  ulCluster = pVolInfo->ulFatEof;
                  break;
                  }

               usFreeEntries = GetFreeEntries((PDIRENTRY)pDir2, ulBytesToRead);
               if (usFreeEntries >= usEntriesNeeded)
                  {
                  PDIRENTRY1 pWork3;
                  //BYTE bCheck = GetVFATCheckSum(&DirNew);

                  if (f32Parms.fMessageActive & LOG_FUNCS)
                     Message(" Inserting entry into 1 cluster");

                  pWork = CompactDir1(pDir2, ulBytesToRead, usEntriesNeeded);
                  pWork3 = fSetLongName1(pWork+2, pszLongName, &usNameHash);
                  pNew->u.File.bSecondaryCount = (BYTE)(pWork3 - pWork - 1);
                  memcpy(pWork++, pNew, sizeof (DIRENTRY1));
                  pStreamNew->u.Stream.usNameHash = usNameHash;
                  pStreamNew->u.Stream.bNameLen = (BYTE)strlen(pszLongName);
                  memcpy(pWork++, pStreamNew, sizeof (DIRENTRY1));
                  (pWork-2)->u.File.usSetCheckSum = GetChkSum16((char *)(pWork-2),
                     sizeof(DIRENTRY1) * ((pWork-2)->u.File.bSecondaryCount + 1));
                  //if (ulCluster == 1)
                  //   // reading root directory on FAT12/FAT16
                  //   rc = WriteSector(pVolInfo, ulSector + ulBlock * usSectorsPerBlock, usSectorsPerBlock, (void *)pDir2, usIOMode);
                  //else
                     rc = WriteBlock(pVolInfo, ulCluster, ulBlock, pDir2, usIOMode);
                  if (rc)
                     {
#ifndef USE_STATIC_BUFS
                     free(pDirectory);
#endif
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
#ifndef USE_STATIC_BUFS
                     free(pDirectory);
#endif
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
            memset(pDirectory, 0, (size_t)pVolInfo->ulClusterSize);
            memmove(pDirectory, pDir2, (size_t)ulBytesToRead);
            if (pLNStart)
               pLNStart = (PDIRENTRY1)((PBYTE)pLNStart - pVolInfo->ulBlockSize);

            //if (ulCluster == 1)
            //   {
            //   // reading the root directory in case of FAT12/FAT16
            //   ulSector += pVolInfo->SectorsPerCluster;
            //   usSectorsRead += pVolInfo->SectorsPerCluster;
            //   if (usSectorsRead * pVolInfo->BootSect.bpb.BytesPerSector >=
            //       pVolInfo->BootSect.bpb.RootDirEntries * sizeof(DIRENTRY))
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
#ifndef USE_STATIC_BUFS
                     free(pDirectory);
#endif
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
#ifndef USE_STATIC_BUFS
                     free(pDirectory);
#endif
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
#ifndef USE_STATIC_BUFS
                     free(pDirectory);
#endif
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

#ifndef USE_STATIC_BUFS
   free(pDirectory);
#endif
   return 0;
}

#endif

USHORT ModifyDirectory(PVOLINFO pVolInfo, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo,
                       USHORT usMode, PDIRENTRY pOld, PDIRENTRY pNew,
                       PDIRENTRY1 pStreamOld, PDIRENTRY1 pStreamNew,
                       PSZ pszLongName, USHORT usIOMode)
{
USHORT rc;

#ifdef USE_STATIC_BUFS
   if (!GetBuf1Access(pVolInfo, "ModifyDirectory"))
      {
#endif
#ifdef EXFAT
      if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
#endif
         rc = ModifyDirectory0(pVolInfo, ulDirCluster, usMode, pOld, pNew,
                               pszLongName, usIOMode);
#ifdef EXFAT
      else
         rc = ModifyDirectory1(pVolInfo, ulDirCluster, pDirSHInfo, usMode,
                               (PDIRENTRY1)pOld, (PDIRENTRY1)pNew,
                               pStreamOld, pStreamNew, pszLongName, usIOMode);
#endif
#ifdef USE_STATIC_BUFS
      ReleaseBuf1(pVolInfo);
      }
#endif

   return rc;
}

USHORT GetBuf1Access(PVOLINFO pVolInfo, PSZ pszName)
{
USHORT rc;

   pVolInfo = pVolInfo;

   Message("GetBuf1Access: %s", pszName);
   rc = SemRequest(&ulSemRWBuf1, TO_INFINITE, pszName);
   if (rc)
      {
      Message("ERROR: SemRequest GetBuf1Access Failed, rc = %d!", rc);
      CritMessage("FAT32: SemRequest GetBuf1Access Failed, rc = %d!", rc);
      Message("GetBuf1Access Failed for %s, rc = %d", pszName, rc);
      return rc;
      }
   return 0;
}

VOID ReleaseBuf1(PVOLINFO pVolInfo)
{
   pVolInfo = pVolInfo;
   Message("ReleaseBuf1");
   FSH_SEMCLEAR(&ulSemRWBuf1);
}

USHORT GetBuf2Access(PVOLINFO pVolInfo, PSZ pszName)
{
USHORT rc;

   pVolInfo = pVolInfo;

   Message("GetBuf2Access: %s", pszName);
   rc = SemRequest(&ulSemRWBuf2, TO_INFINITE, pszName);
   if (rc)
      {
      Message("ERROR: SemRequest GetBuf2Access Failed, rc = %d!", rc);
      CritMessage("FAT32: SemRequest GetBuf2Access Failed, rc = %d!", rc);
      Message("GetBuf2Access Failed for %s, rc = %d", pszName, rc);
      return rc;
      }
   return 0;
}

VOID ReleaseBuf2(PVOLINFO pVolInfo)
{
   pVolInfo = pVolInfo;
   Message("ReleaseBuf2");
   FSH_SEMCLEAR(&ulSemRWBuf2);
}

USHORT GetBuf3Access(PVOLINFO pVolInfo, PSZ pszName)
{
USHORT rc;

   pVolInfo = pVolInfo;

   Message("GetBuf3Access: %s", pszName);
   rc = SemRequest(&ulSemRWBuf3, TO_INFINITE, pszName);
   if (rc)
      {
      Message("ERROR: SemRequest GetBuf3Access Failed, rc = %d!", rc);
      CritMessage("FAT32: SemRequest GetBuf3Access Failed, rc = %d!", rc);
      Message("GetBuf3Access Failed for %s, rc = %d", pszName, rc);
      return rc;
      }
   return 0;
}

VOID ReleaseBuf3(PVOLINFO pVolInfo)
{
   pVolInfo = pVolInfo;
   Message("ReleaseBuf3");
   FSH_SEMCLEAR(&ulSemRWBuf3);
}
