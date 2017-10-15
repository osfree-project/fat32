#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

#include "fat32c.h"
#include "portable.h"

#define RETURN_PARENT_DIR 0xFFFF

ULONG ReadCluster(PCDINFO pCD, ULONG ulCluster, PVOID pbCluster);
ULONG FindDirCluster(PCDINFO pCD, PSZ pDir, USHORT usCurDirEnd, USHORT usAttrWanted, PSZ *pDirEnd, PDIRENTRY1 pStreamEntry);
ULONG FindPathCluster(PCDINFO pCD, ULONG ulCluster, PSZ pszPath, PSHOPENINFO pSHInfo,
                      PDIRENTRY pDirEntry, PDIRENTRY1 pDirEntryStream, PSZ pszFullName);
ULONG GetNextCluster(PCDINFO pCD, PSHOPENINFO pSHInfo, ULONG ulCluster, BOOL fAllowBad);

UCHAR GetFatType(PBOOTSECT pSect);
ULONG GetFatEntrySec(PCDINFO pCD, ULONG ulCluster);
ULONG GetFatEntriesPerSec(PCDINFO pCD);
ULONG GetFatEntry(PCDINFO pCD, ULONG ulCluster);
void  SetFatEntry(PCDINFO pCD, ULONG ulCluster, ULONG ulValue);
ULONG GetFatEntryBlock(PCDINFO pCD, ULONG ulCluster, USHORT usBlockSize);
ULONG GetFatEntriesPerBlock(PCDINFO pCD, USHORT usBlockSize);
ULONG GetFatSize(PCDINFO pCD);
ULONG GetFatEntryEx(PCDINFO pCD, PBYTE pFatStart, ULONG ulCluster, USHORT usBlockSize);
void  SetFatEntryEx(PCDINFO pCD, PBYTE pFatStart, ULONG ulCluster, ULONG ulValue, USHORT usBlockSize);
VOID Translate2OS2(PUSHORT pusUni, PSZ pszName, USHORT usLen);


/******************************************************************
*
******************************************************************/
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

/************************************************************************
*
************************************************************************/
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

/******************************************************************
*
******************************************************************/
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

#endif

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

BOOL ReadFATSector(PCDINFO pCD, ULONG ulSector)
{
ULONG  ulSec = ulSector * 3;
USHORT usNumSec = 3;
ULONG rc;

   // read multiples of three sectors,
   // to fit a whole number of FAT12 entries
   // (ulSector is indeed a number of 3*512
   // bytes blocks, so, it is needed to multiply by 3)

   // A 360 KB diskette has only 2 sectors per FAT
   if (pCD->BootSect.bpb.BigSectorsPerFat < 3)
      {
      if (ulSector > 0)
         return ERROR_SECTOR_NOT_FOUND;
      else
         {
         ulSec = 0;
         usNumSec = pCD->BootSect.bpb.BigSectorsPerFat;
         }
      }

   if (pCD->ulCurFATSector == ulSector)
      return TRUE;

   rc = ReadSector(pCD, pCD->ulActiveFatStart + ulSec, usNumSec,
      (PBYTE)pCD->pbFATSector);
   if (rc)
      return FALSE;

   pCD->ulCurFATSector = ulSector;

   return TRUE;
}

ULONG GetNextCluster(PCDINFO pCD, PSHOPENINFO pSHInfo, ULONG ulCluster, BOOL fAllowBad)
{
ULONG  ulSector = 0;
ULONG  ulRet = 0;

#ifdef EXFAT
   if ( (pCD->bFatType == FAT_TYPE_EXFAT) &&
       pSHInfo && (pSHInfo->fNoFatChain & 1) )
      {
      if (ulCluster < pSHInfo->ulLastCluster)
         return ulCluster + 1;
      else
         return pCD->ulFatEof;
      }
#endif

   ulSector = GetFatEntrySec(pCD, ulCluster);
   if (!ReadFATSector(pCD, ulSector))
      return pCD->ulFatEof;

   ulRet = GetFatEntry(pCD, ulCluster);

   if (ulRet >= pCD->ulFatEof2 && ulRet <= pCD->ulFatEof)
      return pCD->ulFatEof;

   if (ulRet == pCD->ulFatBad && fAllowBad)
      return ulRet;

   if (ulRet >= pCD->ulTotalClusters  + 2)
      {
#ifdef EXFAT
      if (pCD->bFatType < FAT_TYPE_EXFAT)
#endif
         show_message("Error: Next cluster for %lu = %8.8lX\n", 0, 0, 2,
            ulCluster, ulRet);
      return pCD->ulFatEof;
      }

   return ulRet;
}

UCHAR GetFatType(PBOOTSECT pSect)
{
   /*
    *  check for FAT32 according to the Microsoft FAT32 specification
    */
   PBPB  pbpb;
   ULONG FATSz;
   ULONG TotSec;
   ULONG RootDirSectors;
   ULONG NonDataSec;
   ULONG DataSec;
   ULONG CountOfClusters;

#ifdef EXFAT
   if (!memcmp(pSect->oemID, "EXFAT   ", 8))
      {
      return FAT_TYPE_EXFAT;
      } /* endif */
#endif

   if (!pSect)
      {
      return FAT_TYPE_NONE;
      } /* endif */

   pbpb = &pSect->bpb;

   if (!pbpb->BytesPerSector)
      {
      return FAT_TYPE_NONE;
      }

   //if (pbpb->BytesPerSector != SECTOR_SIZE)
   //   {
   //   return FAT_TYPE_NONE;
   //   }

   if (! pbpb->SectorsPerCluster)
      {
      // this could be the case with a JFS partition, for example
      return FAT_TYPE_NONE;
      }

   if(( ULONG )pbpb->BytesPerSector * pbpb->SectorsPerCluster > MAX_CLUSTER_SIZE )
      {
      return FAT_TYPE_NONE;
      }

   RootDirSectors = ((pbpb->RootDirEntries * 32UL) + (pbpb->BytesPerSector-1UL)) / pbpb->BytesPerSector;

   if (pbpb->SectorsPerFat)
      {
      FATSz = pbpb->SectorsPerFat;
      }
   else
      {
      FATSz = pbpb->BigSectorsPerFat;
      } /* endif */

   if (pbpb->TotalSectors)
      {
      TotSec = pbpb->TotalSectors;
      }
   else
      {
      TotSec = pbpb->BigTotalSectors;
      } /* endif */

   NonDataSec = pbpb->ReservedSectors
                   +  (pbpb->NumberOfFATs * FATSz)
                   +  RootDirSectors;

   if (TotSec < NonDataSec)
      {
      return FAT_TYPE_NONE;
      } /* endif */

   DataSec = TotSec - NonDataSec;
   CountOfClusters = DataSec / pbpb->SectorsPerCluster;

   if ((CountOfClusters >= 65525UL) && !memcmp(pSect->FileSystem, "FAT32   ", 8))
      {
      return FAT_TYPE_FAT32;
      } /* endif */

   if (!memcmp(((PBOOTSECT0)pSect)->FileSystem, "FAT12   ", 8))
      {
      return FAT_TYPE_FAT12;
      } /* endif */

   if (!memcmp(((PBOOTSECT0)pSect)->FileSystem, "FAT16   ", 8))
      {
      return FAT_TYPE_FAT16;
      } /* endif */

   if (!memcmp(((PBOOTSECT0)pSect)->FileSystem, "FAT", 3))
      {
      ULONG TotClus = (TotSec - NonDataSec) / pbpb->SectorsPerCluster;

      if (TotClus < 0xff6)
         return FAT_TYPE_FAT12;

      return FAT_TYPE_FAT16;
      }

   return FAT_TYPE_NONE;
}

/******************************************************************
*
******************************************************************/
ULONG GetFatEntrySec(PCDINFO pCD, ULONG ulCluster)
{
   return GetFatEntryBlock(pCD, ulCluster, pCD->BootSect.bpb.BytesPerSector * 3); // in three sector blocks
}

/******************************************************************
*
******************************************************************/
ULONG GetFatEntryBlock(PCDINFO pCD, ULONG ulCluster, USHORT usBlockSize)
{
ULONG  ulSector;

   ulCluster &= pCD->ulFatEof;

   switch (pCD->bFatType)
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
ULONG GetFatEntriesPerSec(PCDINFO pCD)
{
   return GetFatEntriesPerBlock(pCD, pCD->BootSect.bpb.BytesPerSector * 3);
}

/******************************************************************
*
******************************************************************/
ULONG GetFatEntriesPerBlock(PCDINFO pCD, USHORT usBlockSize)
{
   switch (pCD->bFatType)
      {
      case FAT_TYPE_FAT12:
         return (ULONG)usBlockSize * 2 / 3;

      case FAT_TYPE_FAT16:
         return (ULONG)usBlockSize / 2;

      case FAT_TYPE_FAT32:
#ifdef EXFAT
      case FAT_TYPE_EXFAT:
#endif
         return (ULONG)usBlockSize / 4;
      }

   return 0;
}

/******************************************************************
*
******************************************************************/
ULONG GetFatSize(PCDINFO pCD)
{
ULONG ulFatSize = pCD->ulTotalClusters;

   switch (pCD->bFatType)
      {
      case FAT_TYPE_FAT12:
         ulFatSize *= 3;
         ulFatSize /= 2;
         break;

      case FAT_TYPE_FAT16:
         ulFatSize *= 2;
         break;

      case FAT_TYPE_FAT32:
#ifdef EXFAT
      case FAT_TYPE_EXFAT:
#endif
         ulFatSize *= 4;
      }

   return ulFatSize;
}

/******************************************************************
*
******************************************************************/
ULONG GetFatEntry(PCDINFO pCD, ULONG ulCluster)
{
   return GetFatEntryEx(pCD, pCD->pbFATSector, ulCluster, pCD->BootSect.bpb.BytesPerSector * 3);
}

/******************************************************************
*
******************************************************************/
ULONG GetFatEntryEx(PCDINFO pCD, PBYTE pFatStart, ULONG ulCluster, USHORT usBlockSize)
{
   ulCluster &= pCD->ulFatEof;

   switch (pCD->bFatType)
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
            & pCD->ulFatEof;
         break;
         }

      case FAT_TYPE_FAT16:
         {
         ULONG   ulOffset;
         PUSHORT pusCluster;
         ulOffset = ulCluster * 2;

         if (usBlockSize)
            ulOffset %= usBlockSize;

         pusCluster = (PUSHORT)((PBYTE)pFatStart + ulOffset);
         ulCluster = *pusCluster & pCD->ulFatEof;
         break;
         }

      case FAT_TYPE_FAT32:
#ifdef EXFAT
      case FAT_TYPE_EXFAT:
#endif
         {
         ULONG   ulOffset;
         PULONG pulCluster;
         ulOffset = ulCluster * 4;

         if (usBlockSize)
            ulOffset %= usBlockSize;

         pulCluster = (PULONG)((PBYTE)pFatStart + ulOffset);
         ulCluster = *pulCluster & pCD->ulFatEof;
         }
      }

   return ulCluster;
}

/******************************************************************
*
******************************************************************/
void SetFatEntry(PCDINFO pCD, ULONG ulCluster, ULONG ulValue)
{
   SetFatEntryEx(pCD, pCD->pbFATSector, ulCluster, ulValue, pCD->BootSect.bpb.BytesPerSector * 3);
}

/******************************************************************
*
******************************************************************/
void SetFatEntryEx(PCDINFO pCD, PBYTE pFatStart, ULONG ulCluster, ULONG ulValue, USHORT usBlockSize)
{
   ulCluster &= pCD->ulFatEof;
   ulValue   &= pCD->ulFatEof;

   switch (pCD->bFatType)
      {
      case FAT_TYPE_FAT12:
         {
         ULONG   ulOffset;
         PUSHORT pusCluster;
         USHORT  usPrevValue;
         ULONG   ulNewValue;
         ulOffset = (ulCluster * 3) / 2;

         if (usBlockSize)
            ulOffset %= usBlockSize;

         pusCluster = (PUSHORT)((PBYTE)pFatStart + ulOffset);
         usPrevValue = *pusCluster;
         ulNewValue = ((ulCluster * 3) % 2)  ?
            (usPrevValue & 0xf) | (ulValue << 4) : // odd
            (usPrevValue & 0xf000) | (ulValue);    // even
         *pusCluster = ulNewValue;
         break;
         }

      case FAT_TYPE_FAT16:
         {
         ULONG   ulOffset;
         PUSHORT pusCluster;
         ulOffset = ulCluster * 2;

         if (usBlockSize)
            ulOffset %= usBlockSize;

         pusCluster = (PUSHORT)((PBYTE)pFatStart + ulOffset);
         *pusCluster = ulValue;
         break;
         }

      case FAT_TYPE_FAT32:
#ifdef EXFAT
      case FAT_TYPE_EXFAT:
#endif
         {
         ULONG   ulOffset;
         PULONG  pulCluster;
         ulOffset = ulCluster * 4;

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
ULONG FindDirCluster(PCDINFO pCD,
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

   ulCluster = pCD->BootSect.bpb.RootDirStrtClus;
   if (pStreamEntry)
      {
      // fill the Stream entry for root directory
      memset(pStreamEntry, 0, sizeof(DIRENTRY1));
#ifdef EXFAT
      if (pCD->bFatType == FAT_TYPE_EXFAT)
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
   ulCluster = FindPathCluster(pCD, ulCluster, szDir, NULL, &DirEntry, pStreamEntry, NULL);
   if (ulCluster == pCD->ulFatEof)
      return pCD->ulFatEof;
   if ( ulCluster != pCD->ulFatEof &&
#ifdef EXFAT
       ( ( (pCD->bFatType < FAT_TYPE_EXFAT)  && !(DirEntry.bAttr & FILE_DIRECTORY) ) ||
         ( (pCD->bFatType == FAT_TYPE_EXFAT) && !(((PDIRENTRY1)&DirEntry)->u.File.usFileAttr & FILE_DIRECTORY) ) ) )
#else
       !(DirEntry.bAttr & FILE_DIRECTORY) )
#endif
      return pCD->ulFatEof;

   if (*pDir)
      {
      if (usAttrWanted != RETURN_PARENT_DIR && !strpbrk(pDir, "?*"))
         {
         ulCluster2 = FindPathCluster(pCD, ulCluster, pDir, NULL, &DirEntry, pStreamEntry, NULL);
         if ( ulCluster2 != pCD->ulFatEof &&
#ifdef EXFAT
             ( (pCD->bFatType < FAT_TYPE_EXFAT)  && ((DirEntry.bAttr & usAttrWanted) == usAttrWanted) ) ||
               ( (pCD->bFatType == FAT_TYPE_EXFAT) && ((((PDIRENTRY1)&DirEntry)->u.File.usFileAttr & usAttrWanted) == usAttrWanted) ) )
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
ULONG FindPathCluster0(PCDINFO pCD, ULONG ulCluster, PSZ pszPath, PDIRENTRY pDirEntry, PSZ pszFullName)
{
BYTE szShortName[13];
PSZ  pszLongName;
PSZ  pszPart;
PSZ  p;
PDIRENTRY pDir;
PDIRENTRY pDirStart;
PDIRENTRY pDirEnd;
BOOL fFound;
USHORT usMode;
BYTE   bCheck;
ULONG  ulSector;
USHORT usSectorsRead;
ULONG  ulDirEntries = 0;
APIRET rc;

   if (ulCluster == 1)
      {
      // root directory starting sector
      ulSector = pCD->BootSect.bpb.ReservedSectors +
        pCD->BootSect.bpb.SectorsPerFat * pCD->BootSect.bpb.NumberOfFATs;
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
      if (ulCluster == pCD->BootSect.bpb.RootDirStrtClus)
         {
         pszFullName[0] = (BYTE)(pCD->szDrive[0] + 'A');
         pszFullName[1] = ':';
         pszFullName[2] = '\\';
         }
      }

   if (strlen(pszPath) >= 2)
      {
      if (pszPath[1] == ':')
         pszPath += 2;
      }

   //pDirStart = malloc(pCD->ulClusterSize);
   rc = mem_alloc((void **)&pDirStart, pCD->ulClusterSize);
   //if (!pDirStart)
   if (rc)
      return pCD->ulFatEof;
   pszLongName = malloc(FAT32MAXPATHCOMP * 2);
   if (!pszLongName)
      {
      //free(pDirStart);
      mem_free(pDirStart, pCD->ulClusterSize);
      return pCD->ulFatEof;
      }
   memset(pszLongName, 0, FAT32MAXPATHCOMP * 2);
   pszPart = pszLongName + FAT32MAXPATHCOMP;

   usMode = MODE_SCAN;
   /*
      Allow EA files to be found!
   */
   while (usMode != MODE_RETURN && ulCluster != pCD->ulFatEof)
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
         //free(pDirStart);
         mem_free(pDirStart, pCD->ulClusterSize);
         free(pszLongName);
         return pCD->ulFatEof;
         }

      memcpy(pszPart, pszPath, p - pszPath);
      pszPath = p;

      memset(pszLongName, 0, FAT32MAXPATHCOMP);

      fFound = FALSE;
      while (usMode == MODE_SCAN && ulCluster != pCD->ulFatEof)
         {
         if (ulCluster == 1)
            // reading root directory on FAT12/FAT16
            ReadSector(pCD, ulSector, pCD->SectorsPerCluster, (void *)pDirStart);
         else
            ReadCluster(pCD, ulCluster, (void *)pDirStart);
         pDir    = pDirStart;
         pDirEnd = (PDIRENTRY)((PBYTE)pDirStart + pCD->ulClusterSize);

#ifdef CALL_YIELD
         //Yield();
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
               strupr(szShortName); // !!! @todo DBCS/Unicode
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
                  ulCluster = ((ULONG)pDir->wClusterHigh * 0x10000L + pDir->wCluster) & pCD->ulFatEof;
                  if (strlen(pszPath))
                     {
                     if (pDir->bAttr & FILE_DIRECTORY)
                        {
                        if (pszFullName)
                           strcat(pszFullName, "\\");
                        usMode = MODE_START;
                        break;
                        }
                     ulCluster = pCD->ulFatEof;
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
            ulDirEntries++;
            if (ulCluster == 1 && ulDirEntries > pCD->BootSect.bpb.RootDirEntries)
               break;
            }
         if (usMode != MODE_SCAN)
            break;
         if (ulCluster == 1)
            {
            // reading the root directory in case of FAT12/FAT16
            ulSector += pCD->SectorsPerCluster;
            usSectorsRead += pCD->SectorsPerCluster;
            if (usSectorsRead * pCD->BootSect.bpb.BytesPerSector >
                pCD->BootSect.bpb.RootDirEntries * sizeof(DIRENTRY))
               // root directory ended
               ulCluster = 0;
            }
         else
            ulCluster = GetNextCluster(pCD, NULL, ulCluster, TRUE);
         if (!ulCluster)
            ulCluster = pCD->ulFatEof;
         }
      }
   //free(pDirStart);
   mem_free(pDirStart, pCD->ulClusterSize);
   free(pszLongName);
   return ulCluster;
}

#ifdef EXFAT

/******************************************************************
*
******************************************************************/
ULONG FindPathCluster1(PCDINFO pCD, ULONG ulCluster, PSZ pszPath, PSHOPENINFO pSHInfo,
                       PDIRENTRY1 pDirEntry, PDIRENTRY1 pDirEntryStream,
                       PSZ pszFullName)
{
// exFAT case
PSZ  pszLongName;
PSZ  pszPart;
PSZ  p;
DIRENTRY1  Dir;
PDIRENTRY1 pDir;
PDIRENTRY1 pDirStart;
PDIRENTRY1 pDirEnd;
BOOL fFound;
USHORT usMode;
//PROCINFO ProcInfo;
USHORT usDirEntries = 0;
USHORT usMaxDirEntries = (USHORT)(pCD->ulClusterSize / sizeof(DIRENTRY1));
ULONG  ulRet;
USHORT usNumSecondary;
USHORT usFileAttr;
APIRET rc;

   //if (f32Parms.fMessageActive & LOG_FUNCS)
   //   Message("FindPathCluster for %s, dircluster %lu", pszPath, ulCluster);

   if (pDirEntry)
      {
      memset(pDirEntry, 0, sizeof (DIRENTRY1));
      pDirEntry->u.File.usFileAttr = FILE_DIRECTORY;
      }
   if (pszFullName)
      {
      memset(pszFullName, 0, FAT32MAXPATH);
      if (ulCluster == pCD->BootSect.bpb.RootDirStrtClus)
         {
         pszFullName[0] = (BYTE)(pCD->szDrive[0] + 'A');
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

   //pDirStart = malloc((size_t)pCD->ulClusterSize);
   rc = mem_alloc((void **)&pDirStart, pCD->ulClusterSize);
   //if (!pDirStart)
   if (rc)
      {
      //Message("FAT32: Not enough memory for cluster in FindPathCluster");
      return pCD->ulFatEof;
      }
   pszLongName = malloc((size_t)FAT32MAXPATHCOMP * 2);
   if (!pszLongName)
      {
      //Message("FAT32: Not enough memory for buffers in FindPathCluster");
      mem_free(pDirStart, pCD->ulClusterSize);
      return pCD->ulFatEof;
      }
   memset(pszLongName, 0, FAT32MAXPATHCOMP * 2);
   pszPart = pszLongName + FAT32MAXPATHCOMP;

   usMode = MODE_SCAN;
   //GetProcInfo(&ProcInfo, sizeof ProcInfo);
   /*
      Allow EA files to be found!
   */
   //if (ProcInfo.usPdb && f32Parms.fEAS && IsEASFile(pszPath))
   //   ProcInfo.usPdb = 0;

   while (usMode != MODE_RETURN && ulCluster != pCD->ulFatEof)
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
         mem_free(pDirStart, pCD->ulClusterSize);
         free(pszLongName);
         return pCD->ulFatEof;
         }

      memcpy(pszPart, pszPath, p - pszPath);
      pszPath = p;

      memset(pszLongName, 0, FAT32MAXPATHCOMP);

      fFound = FALSE;
      while (usMode == MODE_SCAN && ulCluster != pCD->ulFatEof)
         {
         ReadCluster(pCD, ulCluster, pDirStart);
         pDir    = pDirStart;
         pDirEnd = (PDIRENTRY1)((PBYTE)pDirStart + pCD->ulClusterSize);

#ifdef CALL_YIELD
         //Yield();
#endif

         usDirEntries = 0;
         //while (usMode == MODE_SCAN && pDir < pDirEnd)
         while (usMode == MODE_SCAN && usDirEntries < usMaxDirEntries)
            {
            if (pDir->bEntryType == ENTRY_TYPE_EOD)
               {
               ulCluster = pCD->ulFatEof;
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
                           ulCluster = pCD->ulFatEof;
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
         ulCluster = GetNextCluster(pCD, pSHInfo, ulCluster, FALSE);
         if (!ulCluster)
            ulCluster = pCD->ulFatEof;
         }
      }
   //free(pDirStart);
   mem_free(pDirStart, pCD->ulClusterSize);
   free(pszLongName);
   //if (f32Parms.fMessageActive & LOG_FUNCS)
      //{
      //if (ulCluster != pCD->ulFatEof)
      //   Message("FindPathCluster for %s found cluster %ld", pszPath, ulCluster);
      //else
      //   Message("FindPathCluster for %s returned EOF", pszPath);
      //}
   return ulCluster;
}

#endif

/******************************************************************
*
******************************************************************/
ULONG FindPathCluster(PCDINFO pCD, ULONG ulCluster, PSZ pszPath, PSHOPENINFO pSHInfo,
                      PDIRENTRY pDirEntry, PDIRENTRY1 pDirEntryStream,
                      PSZ pszFullName)
{
ULONG rc;

#ifdef EXFAT
   if (pCD->bFatType < FAT_TYPE_EXFAT)
#endif
      rc = FindPathCluster0(pCD, ulCluster, pszPath, pDirEntry, pszFullName);
#ifdef EXFAT
   else
      rc = FindPathCluster1(pCD, ulCluster, pszPath, pSHInfo,
         (PDIRENTRY1)pDirEntry, (PDIRENTRY1)pDirEntryStream, pszFullName);
#endif

   return rc;
}
