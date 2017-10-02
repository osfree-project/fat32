#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "fat32c.h"

#define RETURN_PARENT_DIR 0xFFFF

#define MODIFY_DIR_INSERT 0
#define MODIFY_DIR_DELETE 1
#define MODIFY_DIR_UPDATE 2
#define MODIFY_DIR_RENAME 3

#define LONGNAME_OFF         0
#define LONGNAME_OK          1
#define LONGNAME_MAKE_UNIQUE 2
#define LONGNAME_ERROR       3

typedef struct _EAOP EAOP;
typedef struct _EAOP *PEAOP;
typedef struct _FEALIST FEALIST;
typedef struct _FEALIST *PFEALIST;

BYTE rgValidChars[]="01234567890 ABCDEFGHIJKLMNOPQRSTUVWXYZ!#$%&'()-_@^`{}~";

ULONG ReadSector(PCDINFO pCD, ULONG ulSector, ULONG nSectors, PBYTE pbSector);
ULONG ReadCluster(PCDINFO pCD, ULONG ulCluster, PVOID pbCluster);
ULONG WriteSector(PCDINFO pCD, ULONG ulSector, ULONG nSectors, PBYTE pbSector);
ULONG WriteCluster(PCDINFO pCD, ULONG ulCluster, PVOID pbCluster);
ULONG ReadFatSector(PCDINFO pCD, ULONG ulSector);
ULONG WriteFatSector(PCDINFO pCD, ULONG ulSector);
ULONG GetNextCluster(PCDINFO pCD, PSHOPENINFO pSHInfo, ULONG ulCluster, BOOL fAllowBad);
BOOL  GetDiskStatus(PCDINFO pCD);
ULONG GetFreeSpace(PCDINFO pCD);
BOOL  MarkDiskStatus(PCDINFO pCD, BOOL fClean);
ULONG FindDirCluster(PCDINFO pCD, PSZ pDir, USHORT usCurDirEnd, USHORT usAttrWanted, PSZ *pDirEnd, PDIRENTRY1 pStreamEntry);
ULONG FindPathCluster(PCDINFO pCD, ULONG ulCluster, PSZ pszPath, PSHOPENINFO pSHInfo,
                      PDIRENTRY pDirEntry, PDIRENTRY1 pDirEntryStream, PSZ pszFullName);
APIRET ModifyDirectory(PCDINFO pCD, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo,
                       USHORT usMode, PDIRENTRY pOld, PDIRENTRY pNew,
                       PDIRENTRY1 pStreamOld, PDIRENTRY1 pStreamNew, PSZ pszLongNameOld, PSZ pszLongNameNew);
APIRET MarkFileEAS(PCDINFO pCD, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo, PSZ pszFileName, BYTE fEAS);
USHORT GetSetFileEAS(PCDINFO pCD, USHORT usFunc, PMARKFILEEASBUF pMark);
BOOL fGetLongName(PDIRENTRY pDir, PSZ pszName, USHORT wMax, PBYTE pbCheck);
USHORT QueryUni2NLS( USHORT usPage, USHORT usChar );
BYTE GetVFATCheckSum(PDIRENTRY pDir);
APIRET MakeShortName(PCDINFO pCD, ULONG ulDirCluster, PSZ pszLongName, PSZ pszShortName);
PDIRENTRY fSetLongName(PDIRENTRY pDir, PSZ pszLongName, BYTE bCheck);
USHORT DBCSStrlen( const PSZ pszStr );
BOOL IsDBCSLead( UCHAR uch );
VOID Translate2OS2(PUSHORT pusUni, PSZ pszName, USHORT usLen);
USHORT Translate2Win(PSZ pszName, PUSHORT pusUni, USHORT usLen);
USHORT QueryNLS2Uni( USHORT usCode );
APIRET SetNextCluster(PCDINFO pCD, ULONG ulCluster, ULONG ulNext);
VOID MakeName(PDIRENTRY pDir, PSZ pszName, USHORT usMax);
USHORT GetFreeEntries(PDIRENTRY pDirBlock, ULONG ulSize);
PDIRENTRY CompactDir(PDIRENTRY pStart, ULONG ulSize, USHORT usEntriesNeeded);
VOID MarkFreeEntries(PDIRENTRY pDirBlock, ULONG ulSize);
ULONG GetFreeCluster(PCDINFO pCD);
APIRET SetFileSize(PCDINFO pCD, PFILESIZEDATA pFileSize);
USHORT RecoverChain2(PCDINFO pCD, ULONG ulCluster, PBYTE pData, USHORT cbData);
USHORT MakeDirEntry(PCDINFO pCD, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo,
                    PDIRENTRY pNew, PDIRENTRY1 pNewStream, PSZ pszName);
BOOL DeleteFatChain(PCDINFO pCD, ULONG ulCluster);
BOOL UpdateFSInfo(PCDINFO pCD);
ULONG MakeFatChain(PCDINFO pCD, PSHOPENINFO pSHInfo, ULONG ulPrevCluster, ULONG ulClustersRequested, PULONG pulLast);
USHORT MakeChain(PCDINFO pCD, PSHOPENINFO pSHInfo, ULONG ulFirstCluster, ULONG ulSize);
APIRET MakeFile(PCDINFO pCD, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo, PSZ pszOldFile, PSZ pszFile, PBYTE pBuf, ULONG cbBuf);
APIRET MakeDir(PCDINFO pCD, ULONG ulDirCluster, PDIRENTRY pDir, PSZ pszFile);
BOOL IsCharValid(char ch);

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

USHORT ReadBmpSector(PCDINFO pCD, ULONG ulSector);
USHORT WriteBmpSector(PCDINFO pCD, ULONG ulSector);
ULONG GetAllocBitmapSec(PCDINFO pCD, ULONG ulCluster);
BOOL GetBmpEntry(PCDINFO pCD, ULONG ulCluster);
VOID SetBmpEntry(PCDINFO pCD, ULONG ulCluster, BOOL fState);
BOOL ClusterInUse2(PCDINFO pCD, ULONG ulCluster);
BOOL MarkCluster2(PCDINFO pCD, ULONG ulCluster, BOOL fState);
BOOL fGetLongName1(PDIRENTRY1 pDir, PSZ pszName, USHORT wMax);
PDIRENTRY1 CompactDir1(PDIRENTRY1 pStart, ULONG ulSize, USHORT usEntriesNeeded);
PDIRENTRY1 fSetLongName1(PDIRENTRY1 pDir, PSZ pszLongName, PUSHORT pusNameHash);
VOID MarkFreeEntries1(PDIRENTRY1 pDirBlock, ULONG ulSize);
ULONG GetLastCluster(PCDINFO pCD, ULONG ulCluster, PDIRENTRY1 pDirEntryStream);
USHORT fGetAllocBitmap(PCDINFO pCD, PULONG pulFirstCluster, PULONGLONG pullLen);
void SetSHInfo1(PCDINFO pCD, PDIRENTRY1 pStreamEntry, PSHOPENINFO pSHInfo);
APIRET DelFile(PCDINFO pCD, PSZ pszFilename);
void FileGetSize(PCDINFO pCD, PDIRENTRY pDirEntry, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo, PSZ pszFile, PULONGLONG pullSize);
void FileSetSize(PCDINFO pCD, PDIRENTRY pDirEntry, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo, PSZ pszFile, ULONGLONG ullSize);
USHORT usModifyEAS(PCDINFO pCD, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo, PSZ pszFileName, PEAOP pEAOP);
USHORT usGetEAS(PCDINFO pCD, USHORT usLevel, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo, PSZ pszFileName, PEAOP pEAOP);

void set_datetime(DIRENTRY *pDir);
void set_datetime1(DIRENTRY1 *pDir);

/******************************************************************
*
******************************************************************/
BOOL IsCharValid(char ch)
{
    return (BOOL)strchr(rgValidChars, ch);
}

/******************************************************************
*
******************************************************************/
/* Get the last-character. (sbcs/dbcs) */
int lastchar(const char *string)
{
    UCHAR *s;
    unsigned int c = 0;
    int i, len = strlen(string);
    s = (UCHAR *)string;
    for(i = 0; i < len; i++)
    {
        c = *(s + i);
        if(IsDBCSLead(( UCHAR )c))
        {
            c = (c << 8) + ( unsigned int )*(s + i + 1);
            i++;
        }
    }
    return c;
}

/******************************************************************
*
******************************************************************/
BOOL GetDiskStatus(PCDINFO pCD)
{
   ULONG ulStatus;
   BOOL  fStatus;

   if (ReadFatSector(pCD, 0L))
      return FALSE;

   ulStatus = GetFatEntry(pCD, 1);

   if (ulStatus & pCD->ulFatClean)
      fStatus = TRUE;
   else
      fStatus = FALSE;

   return fStatus;
}

/******************************************************************
*
******************************************************************/
ULONG ReadFatSector(PCDINFO pCD, ULONG ulSector)
{
   ULONG  ulSec = ulSector * 3;
   USHORT usNumSec = 3;
   APIRET rc;

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
      return 0;

   if (ulSec >= pCD->BootSect.bpb.BigSectorsPerFat)
      return ERROR_SECTOR_NOT_FOUND;

   rc = ReadSector(pCD, pCD->ulActiveFatStart + ulSec, usNumSec,
      pCD->pbFATSector);
   if (rc)
      return rc;

   pCD->ulCurFATSector = ulSector;

   return 0;
}

/******************************************************************
*
******************************************************************/
ULONG WriteFatSector(PCDINFO pCD, ULONG ulSector)
{
   USHORT usFat;
   ULONG  ulSec = ulSector * 3;
   USHORT usNumSec = 3;
   APIRET rc;

   if (pCD->ulCurFATSector != ulSector)
      return ERROR_SECTOR_NOT_FOUND;

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

   if (ulSec >= pCD->BootSect.bpb.BigSectorsPerFat)
      return ERROR_SECTOR_NOT_FOUND;

   for (usFat = 0; usFat < pCD->BootSect.bpb.NumberOfFATs; usFat++)
      {
      rc = WriteSector(pCD, pCD->ulActiveFatStart + ulSec, usNumSec,
         pCD->pbFATSector);

      if (rc)
         return rc;

      if (pCD->BootSect.bpb.ExtFlags & 0x0080)
         break;

      ulSec += pCD->BootSect.bpb.BigSectorsPerFat;
      }

   return 0;
}

#ifdef EXFAT

/******************************************************************
*
******************************************************************/
USHORT ReadBmpSector(PCDINFO pCD, ULONG ulSector)
{
// Read exFAT Allocation bitmap sector
USHORT rc;

   if (pCD->ulCurBmpSector == ulSector)
      return 0;

   if (ulSector >= (pCD->ulAllocBmpLen + pCD->BootSect.bpb.BytesPerSector - 1)
         / pCD->BootSect.bpb.BytesPerSector)
      {
      return ERROR_SECTOR_NOT_FOUND;
      }

   rc = ReadSector(pCD, pCD->ulBmpStartSector + ulSector, 1,
      pCD->pbFatBits);
   if (rc)
      return rc;

   pCD->ulCurBmpSector = ulSector;

   return 0;
}

/******************************************************************
*
******************************************************************/
USHORT WriteBmpSector(PCDINFO pCD, ULONG ulSector)
{
// Write exFAT Allocation bitmap sector
//USHORT usBmp;
USHORT rc;

   if (pCD->ulCurBmpSector != ulSector)
      {
      return ERROR_SECTOR_NOT_FOUND;
      }

   if (ulSector >= (pCD->ulAllocBmpLen + pCD->BootSect.bpb.BytesPerSector - 1)
         / pCD->BootSect.bpb.BytesPerSector)
      {
      return ERROR_SECTOR_NOT_FOUND;
      }

   //for (usFat = 0; usFat < pCD->BootSect.bpb.NumberOfFATs; usFat++)
   //   {
      rc = WriteSector(pCD, pCD->ulBmpStartSector + ulSector, 1,
         pCD->pbFatBits);
      if (rc)
         return rc;

      //if (pCD->BootSect.bpb.ExtFlags & 0x0080)
      //   break;
      //ulSec += pCD->BootSect.bpb.BigSectorsPerFat; // !!!
      // @todo what if pCD->ulActiveFatStart does not point to 1st FAT?
   //   }

   return 0;
}

USHORT fGetAllocBitmap(PCDINFO pCD, PULONG pulFirstCluster, PULONGLONG pullLen)
{
// This is relevant for exFAT only
PDIRENTRY1 pDirStart, pDir, pDirEnd;
DIRENTRY1 DirEntry;
ULONG ulCluster;
BOOL fFound;
APIRET rc;

   pDir = NULL;

   pDirStart = (PDIRENTRY1)malloc((size_t)pCD->ulClusterSize);
   if (!pDirStart)
      {
      return ERROR_NOT_ENOUGH_MEMORY;
      }

   fFound = FALSE;
   ulCluster = pCD->BootSect.bpb.RootDirStrtClus;
   while (!fFound && ulCluster != pCD->ulFatEof)
      {
      rc = ReadCluster(pCD, ulCluster, pDirStart);
      pDir    = pDirStart;
      pDirEnd = (PDIRENTRY1)((PBYTE)pDirStart + pCD->ulClusterSize);
      while (pDir < pDirEnd)
         {
         if (pDir->bEntryType == ENTRY_TYPE_ALLOC_BITMAP)
            {
            fFound = TRUE;
            memcpy(&DirEntry, pDir, sizeof (DIRENTRY1));
            break;
            }
         pDir++;
         }
      if (fFound)
         break;
      else
         {
         ulCluster = GetNextCluster(pCD, NULL, ulCluster, FALSE);
         if (!ulCluster)
            {
            ulCluster = pCD->ulFatEof;
            }
         }
      pDir++;
      if (ulCluster == pCD->ulFatEof)
         {
         break;
         }
      }

   *pulFirstCluster = DirEntry.u.AllocBmp.ulFirstCluster;
   *pullLen = DirEntry.u.AllocBmp.ullDataLength;
   free(pDirStart);
   return 0;
}

USHORT fGetUpCaseTbl(PCDINFO pCD, PULONG pulFirstCluster, PULONGLONG pullLen, PULONG pulChecksum)
{
// This is relevant for exFAT only
PDIRENTRY1 pDirStart, pDir, pDirEnd;
DIRENTRY1 DirEntry;
ULONG ulCluster;
ULONG ulBlock;
BOOL fFound;

   pDir = NULL;

   pDirStart = (PDIRENTRY1)malloc((size_t)pCD->ulClusterSize);
   if (!pDirStart)
      return ERROR_NOT_ENOUGH_MEMORY;

   fFound = FALSE;
   ulCluster = pCD->BootSect.bpb.RootDirStrtClus;
   while (!fFound && ulCluster != pCD->ulFatEof)
      {
      ReadCluster(pCD, ulCluster, pDirStart);
      pDir    = pDirStart;
      pDirEnd = (PDIRENTRY1)((PBYTE)pDirStart + pCD->ulClusterSize);
      while (pDir < pDirEnd)
         {
         if (pDir->bEntryType == ENTRY_TYPE_UPCASE_TABLE)
            {
            fFound = TRUE;
            memcpy(&DirEntry, pDir, sizeof (DIRENTRY1));
            break;
            }
         pDir++;
         }
      if (fFound)
         break;
      else
         {
         ulCluster = GetNextCluster(pCD, NULL, ulCluster, FALSE);
         if (!ulCluster)
            ulCluster = pCD->ulFatEof;
         }
      pDir++;
      if (ulCluster == pCD->ulFatEof)
         break;
      }

   *pulFirstCluster = DirEntry.u.UpCaseTbl.ulFirstCluster;
   *pullLen = DirEntry.u.UpCaseTbl.ullDataLength;
   *pulChecksum = DirEntry.u.UpCaseTbl.ulTblCheckSum;
   return 0;
}

void SetSHInfo1(PCDINFO pCD, PDIRENTRY1 pStreamEntry, PSHOPENINFO pSHInfo)
{
   memset(pSHInfo, 0, sizeof(PSHOPENINFO));
   pSHInfo->fNoFatChain = pStreamEntry->u.Stream.bNoFatChain & 1;
   pSHInfo->ulStartCluster = pStreamEntry->u.Stream.ulFirstClus;
   pSHInfo->ulLastCluster = GetLastCluster(pCD, pStreamEntry->u.Stream.ulFirstClus, pStreamEntry);
}

#endif

ULONG GetLastCluster(PCDINFO pCD, ULONG ulCluster, PDIRENTRY1 pDirEntryStream)
{
ULONG  ulReturn = 0;

   //if (f32Parms.fMessageActive & LOG_FUNCS)
   //   Message("GetLastCluster");

   if (!ulCluster)
      return pCD->ulFatEof;

#ifdef EXFAT
   if (pCD->bFatType == FAT_TYPE_EXFAT)
      {
      if (pDirEntryStream->u.Stream.bNoFatChain & 1)
         {
         return ulCluster + (pDirEntryStream->u.Stream.ullValidDataLen / pCD->ulClusterSize) +
             ((pDirEntryStream->u.Stream.ullValidDataLen % pCD->ulClusterSize) ? 1 : 0) - 1;
         }
      }
#endif

   //if (GetFatAccess(pCD, "GetLastCluster"))
   //   return pCD->ulFatEof;

   ulReturn = ulCluster;
   while (ulCluster != pCD->ulFatEof)
      {
      ulReturn = ulCluster;
      ulCluster = GetNextCluster(pCD, NULL, ulCluster, FALSE);
      //ulSector = GetFatEntrySec(pCD, ulCluster);

      //if (ulSector != pCD->ulCurFatSector)
      //   ReadFatSector(pCD, ulSector);

      //ulCluster = GetFatEntry(pCD, ulCluster);

      if (ulCluster >= pCD->ulFatEof2 && ulCluster <= pCD->ulFatEof)
         break;
      if (! ulCluster)
         {
         //Message("The file ends with NULL FAT entry!");
         break;
         }
      }
   //ReleaseFat(pCD);
   return ulReturn;
}

/******************************************************************
*
******************************************************************/
ULONG GetFreeSpace(PCDINFO pCD)
{
   ULONG ulSector = 0;
   ULONG ulCluster = 0;
   ULONG ulTotalFree;
   ULONG ulNextFree = 0;
   ULONG ulNextCluster = 0;

   ulTotalFree = 0;
   for (ulCluster = 2; ulCluster < pCD->ulTotalClusters + 2; ulCluster++)
      {
      //ulSector = GetFatEntrySec(pCD, ulCluster);
      //if (ulSector != pCD->ulCurFATSector)
      //   ReadFatSector(pCD, ulSector);
      //ulNextCluster = GetFatEntry(pCD, ulCluster);
      //if (ulNextCluster == 0)
      if (!ClusterInUse2(pCD, ulCluster))
         {
         ulTotalFree++;
         if (!ulNextFree)
            ulNextFree = ulCluster;
         }
      }

   pCD->FSInfo.ulFreeClusters = ulTotalFree;
   pCD->FSInfo.ulNextFreeCluster = ulNextFree;
   UpdateFSInfo(pCD);

   return ulTotalFree;
}

/******************************************************************
*
******************************************************************/
BOOL MarkDiskStatus(PCDINFO pCD, BOOL fClean)
{
   static BYTE bSector[SECTOR_SIZE * 8] = {0};
   USHORT usFat;
   ULONG ulNextCluster = 0;
   PBYTE pbSector;

#ifdef EXFAT
   if (pCD->bFatType == FAT_TYPE_EXFAT)
      {
      // set dirty flag in volume flags of boot sector
      USHORT usVolFlags;
      PBOOTSECT1 pbBoot = (PBOOTSECT1)bSector;

      if (ReadSector(pCD, 0, 1, (PBYTE)pbBoot))
         return FALSE;

      usVolFlags = pbBoot->usVolumeFlags;

      if (fClean)
         usVolFlags &= ~VOL_FLAG_VOLDIRTY;
      else
         usVolFlags |= VOL_FLAG_VOLDIRTY;

      pbBoot->usVolumeFlags = usVolFlags;

      if (WriteSector(pCD, 0, 1, (PBYTE)pbBoot))
         return FALSE;

      return TRUE;
      }
#endif

   if (pCD->ulCurFATSector != 0)
      {
      if (ReadFatSector(pCD, 0))
         return FALSE;
      }

   ulNextCluster = GetFatEntry(pCD, 1);

   if (fClean)
      {
      ulNextCluster |= pCD->ulFatClean;
      }
   else
      {
      ulNextCluster  &= ~pCD->ulFatClean;
      }

   SetFatEntry(pCD, 1, ulNextCluster);

   if (WriteFatSector(pCD, 0))
      return FALSE;

   return TRUE;
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

/******************************************************************
*
******************************************************************/
USHORT GetSetFileEAS(PCDINFO pCD, USHORT usFunc, PMARKFILEEASBUF pMark)
{
   ULONG ulDirCluster;
   DIRENTRY1 DirStream;
#ifdef EXFAT
   SHOPENINFO DirSHInfo;
#endif
   PSHOPENINFO pDirSHInfo = NULL;
   PSZ   pszFile;

   ulDirCluster = FindDirCluster(pCD,
      pMark->szFileName,
      0xFFFF,
      RETURN_PARENT_DIR,
      &pszFile,
      &DirStream);

   if (ulDirCluster == pCD->ulFatEof)
      return ERROR_PATH_NOT_FOUND;

#ifdef EXFAT
   if (pCD->bFatType == FAT_TYPE_EXFAT)
      {
      pDirSHInfo = &DirSHInfo;
      SetSHInfo1(pCD, &DirStream, pDirSHInfo);
      }
#endif

   if (usFunc == FAT32_QUERYEAS)
      {
      ULONG ulCluster;
      DIRENTRY DirEntry;
      PDIRENTRY1 pDirEntry = (PDIRENTRY1)&DirEntry;

      ulCluster = FindPathCluster(pCD, ulDirCluster, pszFile, pDirSHInfo, &DirEntry, NULL, NULL);
      if (ulCluster == pCD->ulFatEof)
         return ERROR_FILE_NOT_FOUND;
#ifdef EXFAT
      if (pCD->bFatType < FAT_TYPE_EXFAT)
#endif
         pMark->fEAS = DirEntry.fEAS;
#ifdef EXFAT
      else
         pMark->fEAS = pDirEntry->u.File.fEAS;
#endif
      return 0;
      }

   return MarkFileEAS(pCD, ulDirCluster, pDirSHInfo, pszFile, pMark->fEAS);
}


/************************************************************************
*
************************************************************************/
APIRET ModifyDirectory0(PCDINFO pCD, ULONG ulDirCluster, USHORT usMode, PDIRENTRY pOld, PDIRENTRY pNew, PSZ pszLongName)
{
   PDIRENTRY pDirectory;
   PDIRENTRY pDir2;
   PDIRENTRY pWork, pWork2;
   PDIRENTRY pMax;
   USHORT    usEntriesNeeded;
   USHORT    usFreeEntries;
   DIRENTRY  DirNew;
   ULONG     ulCluster;
   ULONG     ulPrevCluster;
   ULONG     ulNextCluster;
   PDIRENTRY pLNStart;
   APIRET    rc;
   USHORT    usClusterCount;
   BOOL      fNewCluster;
   ULONG     ulSector;
   ULONG     ulPrevSector;
   USHORT    usSectorsRead;
   ULONG     ulBytesToRead;
   ULONG     ulPrevBytesToRead = 0;
   ULONG     ulBytesRemained;

   if (usMode == MODIFY_DIR_RENAME ||
       usMode == MODIFY_DIR_INSERT)
      {
      if (!pNew || !pszLongName)
         return ERROR_INVALID_PARAMETER;

      memcpy(&DirNew, pNew, sizeof (DIRENTRY));
      if ((pNew->bAttr & 0x0F) != FILE_VOLID)
         {
         rc = MakeShortName(pCD, ulDirCluster, pszLongName, DirNew.bFileName);
         if (rc == LONGNAME_ERROR)
            return ERROR_FILE_EXISTS;
         set_datetime(&DirNew);
         memcpy(pNew, &DirNew, sizeof (DIRENTRY));

         if (rc == LONGNAME_OFF)
            pszLongName = NULL;
         }
      else
         pszLongName = NULL;

      usEntriesNeeded = 1;
      if (pszLongName)
         {
#if 0
         usEntriesNeeded += strlen(pszLongName) / 13 +
            (strlen(pszLongName) % 13 ? 1 : 0);
#else
         usEntriesNeeded += ( DBCSStrlen( pszLongName ) + 12 ) / 13;
#endif
         }
      }

   if (usMode == MODIFY_DIR_RENAME ||
       usMode == MODIFY_DIR_DELETE ||
       usMode == MODIFY_DIR_UPDATE)
      {
      if (!pOld)
         return ERROR_INVALID_PARAMETER;
      }

   pDirectory = (PDIRENTRY)malloc(2 * pCD->ulClusterSize);
   if (!pDirectory)
      return ERROR_NOT_ENOUGH_MEMORY;

   memset(pDirectory, 0, pCD->ulClusterSize);
   pDir2 =(PDIRENTRY)((PBYTE)pDirectory + pCD->ulClusterSize);
   memset(pDir2, 0, pCD->ulClusterSize);

   ulCluster = ulDirCluster;
   pLNStart = NULL;
   ulPrevCluster = pCD->ulFatEof;
   usClusterCount = 0;
   fNewCluster = FALSE;

   if (ulCluster == 1)
      {
      // root directory starting sector
      ulSector = pCD->BootSect.bpb.ReservedSectors +
         pCD->BootSect.bpb.SectorsPerFat * pCD->BootSect.bpb.NumberOfFATs;
      usSectorsRead = 0;
      ulBytesRemained = pCD->BootSect.bpb.RootDirEntries * sizeof(DIRENTRY);
      }

   while (ulCluster != pCD->ulFatEof)
      {
#ifdef CALL_YIELD
      //Yield();
#endif

      usClusterCount++;
      if (!fNewCluster)
         {
         if (ulCluster == 1)
            {
            // reading root directory on FAT12/FAT16
            rc = ReadSector(pCD, ulSector, pCD->SectorsPerCluster, (void *)pDir2);
            if (ulBytesRemained >= pCD->ulClusterSize)
               {
               ulBytesToRead = pCD->ulClusterSize;
               ulBytesRemained -= pCD->ulClusterSize;
               }
            else
               {
               ulBytesToRead = ulBytesRemained;
               ulBytesRemained = 0;
               }
            }
         else
            {
            rc = ReadCluster(pCD, ulCluster, pDir2);
            ulBytesToRead = pCD->ulClusterSize;
            }
         if (rc)
            {
            free(pDirectory);
            return rc;
            }
         }
      else
         {
         memset(pDir2, 0, pCD->ulClusterSize);
         fNewCluster = FALSE;
         }

      pMax = (PDIRENTRY)((PBYTE)pDirectory + pCD->ulClusterSize + ulBytesToRead);

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
                     memcpy(pWork, pNew, sizeof (DIRENTRY));
                     set_datetime(pWork);
                     if (ulCluster == 1)
                        // reading root directory on FAT12/FAT16
                        rc = WriteSector(pCD, ulSector, pCD->SectorsPerCluster, (void *)pDir2);
                     else
                        rc = WriteCluster(pCD, ulCluster, (void *)pDir2);
                     if (rc)
                        {
                        free(pDirectory);
                        return rc;
                        }
                     ulCluster = pCD->ulFatEof;
                     break;

                  case MODIFY_DIR_DELETE:
                  case MODIFY_DIR_RENAME:
                     pWork2 = pLNStart;
                     while (pWork2 < pWork)
                        {
                        pWork2->bFileName[0] = DELETED_ENTRY;
                        pWork2++;
                        }
                     pWork->bFileName[0] = DELETED_ENTRY;

                     /*
                        Write previous cluster if LN start lies there
                     */
                     if (ulPrevCluster != pCD->ulFatEof &&
                        pLNStart < pDir2)
                        {
                        if (ulPrevCluster == 1)
                           // reading root directory on FAT12/FAT16
                           rc = WriteSector(pCD, ulPrevSector, pCD->SectorsPerCluster, (void *)pDirectory);
                        else
                           rc = WriteCluster(pCD, ulPrevCluster, (void *)pDirectory);
                        if (rc)
                           {
                           free(pDirectory);
                           return rc;
                           }
                        }

                     /*
                        Write current cluster
                     */
                     if (ulCluster == 1)
                        // reading root directory on FAT12/FAT16
                        rc = WriteSector(pCD, ulSector, pCD->SectorsPerCluster, (void *)pDir2);
                     else
                        rc = WriteCluster(pCD, ulCluster, (void *)pDir2);
                     if (rc)
                        {
                        free(pDirectory);
                        return rc;
                        }

                     if (usMode == MODIFY_DIR_DELETE)
                        ulCluster = pCD->ulFatEof;
                     else
                        {
                        usMode = MODIFY_DIR_INSERT;
                        ulCluster = ulDirCluster;
                        ulBytesRemained = pCD->BootSect.bpb.RootDirEntries * sizeof(DIRENTRY);
                        ulPrevCluster = pCD->ulFatEof;
                        ulPrevSector = 0;
                        usClusterCount = 0;
                        pLNStart = NULL;
                        continue;
                        }
                     break;
                  }
               }

            break;

         case MODIFY_DIR_INSERT:
            if (ulPrevCluster != pCD->ulFatEof && GetFreeEntries(pDirectory, ulPrevBytesToRead + ulBytesToRead) >= usEntriesNeeded)
               {
               BYTE bCheck = GetVFATCheckSum(&DirNew);

               pWork = (PDIRENTRY)CompactDir(pDirectory, ulPrevBytesToRead + ulBytesToRead, usEntriesNeeded);
               pWork = (PDIRENTRY)fSetLongName(pWork, pszLongName, bCheck);
               memcpy(pWork, &DirNew, sizeof (DIRENTRY));
               if (ulPrevCluster == 1)
                  // reading root directory on FAT12/FAT16
                  rc = WriteSector(pCD, ulPrevSector, pCD->SectorsPerCluster, (void *)pDirectory);
               else
                  rc = WriteCluster(pCD, ulPrevCluster, (void *)pDirectory);
               if (rc)
                  {
                  free(pDirectory);
                  return rc;
                  }

               if (ulCluster == 1)
                  // reading root directory on FAT12/FAT16
                  rc = WriteSector(pCD, ulSector, pCD->SectorsPerCluster, (void *)pDir2);
               else
                  rc = WriteCluster(pCD, ulCluster, (void *)pDir2);
               if (rc)
                  {
                  free(pDirectory);
                  return rc;
                  }
               ulCluster = pCD->ulFatEof;
               break;
               }

            usFreeEntries = GetFreeEntries(pDir2, ulBytesToRead);
            if (usFreeEntries >= usEntriesNeeded)
               {
               BYTE bCheck = GetVFATCheckSum(&DirNew);

               pWork = (PDIRENTRY)CompactDir(pDir2, ulBytesToRead, usEntriesNeeded);
               pWork = (PDIRENTRY)fSetLongName(pWork, pszLongName, bCheck);
               memcpy(pWork, &DirNew, sizeof (DIRENTRY));
               if (ulCluster == 1)
                  // reading root directory on FAT12/FAT16
                  rc = WriteSector(pCD, ulSector, pCD->SectorsPerCluster, (void *)pDir2);
               else
                  rc = WriteCluster(pCD, ulCluster, (void *)pDir2);
               if (rc)
                  {
                  free(pDirectory);
                  return rc;
                  }
               ulCluster = pCD->ulFatEof;
               break;
               }
            else if (usFreeEntries > 0)
               {
               MarkFreeEntries(pDir2, ulBytesToRead);
               if (ulCluster == 1)
                  // reading root directory on FAT12/FAT16
                  rc = WriteSector(pCD, ulSector, pCD->SectorsPerCluster, (void *)pDir2);
               else
                  rc = WriteCluster(pCD, ulCluster, (void *)pDir2);
               if (rc)
                  {
                  free(pDirectory);
                  return rc;
                  }
               }

            break;
         }

      if (ulCluster != pCD->ulFatEof)
         {
         ulPrevBytesToRead = ulBytesToRead;
         ulPrevCluster = ulCluster;
         ulPrevSector = ulSector;
         memset(pDirectory, 0, pCD->ulClusterSize);
         memmove(pDirectory, pDir2, ulBytesToRead);
         if (pLNStart)
            pLNStart = (PDIRENTRY)((PBYTE)pLNStart - pCD->ulClusterSize);

         if (ulCluster == 1)
            {
            // reading the root directory in case of FAT12/FAT16
            ulSector += pCD->SectorsPerCluster;
            usSectorsRead += pCD->SectorsPerCluster;
            if (usSectorsRead * pCD->BootSect.bpb.BytesPerSector >=
                pCD->BootSect.bpb.RootDirEntries * sizeof(DIRENTRY))
               // root directory ended
               ulNextCluster = 0;
            else
               ulNextCluster = 1;
            }
         else
            ulNextCluster = GetNextCluster(pCD, NULL, ulCluster, TRUE);
         if (!ulNextCluster)
            ulNextCluster = pCD->ulFatEof;
         if (ulNextCluster == pCD->ulFatEof)
            {
            if (usMode == MODIFY_DIR_UPDATE ||
                usMode == MODIFY_DIR_DELETE ||
                usMode == MODIFY_DIR_RENAME)
               {
               free(pDirectory);
               return ERROR_FILE_NOT_FOUND;
               }

            if (ulCluster == 1)
               // no expanding for root directory in case of FAT12/FAT16
               ulNextCluster = pCD->ulFatEof;
            else
               ulNextCluster = SetNextCluster(pCD, ulCluster, FAT_ASSIGN_NEW);
            if (ulNextCluster == pCD->ulFatEof)
               {
               free(pDirectory);
               return ERROR_DISK_FULL;
               }
            fNewCluster = TRUE;
            }
         ulCluster = ulNextCluster;
         }
      }

   free(pDirectory);
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


USHORT ModifyDirectory1(PCDINFO pCD, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo,
                       USHORT usMode, PDIRENTRY1 pOld, PDIRENTRY1 pNew,
                       PDIRENTRY1 pStreamOld, PDIRENTRY1 pStreamNew, PSZ pszLongNameOld, PSZ pszLongNameNew)
{
PDIRENTRY1 pDirectory;
PDIRENTRY1 pDir2, pDir1;
PDIRENTRY1 pWork, pWork2, pWorkStream, pWorkFile;
PDIRENTRY1 pMax;
USHORT    usEntriesNeeded;
USHORT    usFreeEntries;
DIRENTRY1  DirNew;
ULONG     ulCluster;
ULONG     ulPrevCluster;
//ULONG     ulPrevBlock;
ULONG     ulNextCluster = pCD->ulFatEof;
ULONG     ulCluster2;
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

   //if (f32Parms.fMessageActive & LOG_FUNCS)
   //   Message("ModifyDirectory DirCluster %ld, Mode = %d",
   //   ulDirCluster, usMode);

   if (usMode == MODIFY_DIR_RENAME ||
       usMode == MODIFY_DIR_INSERT)
      {
      if (!pNew || !pszLongNameOld)
         {
         return ERROR_INVALID_PARAMETER;
         }

      memcpy(&DirNew, pNew, sizeof (DIRENTRY1));
      /* if ((pNew->bAttr & 0x0F) != FILE_VOLID)
         {
         rc = MakeShortName(pCD, ulDirCluster, pszLongNameOld, DirNew.bFileName);
         if (rc == LONGNAME_ERROR)
            {
            Message("Modify directory: Longname error");
            return ERROR_FILE_EXISTS;
            }
         set_datetime1(&DirNew);
         memcpy(pNew, &DirNew, sizeof (DIRENTRY));

         if (rc == LONGNAME_OFF)
            pszLongNameOld = NULL;
         }
      else
         pszLongNameOld = NULL; */

      //usEntriesNeeded = 1;
      usEntriesNeeded = 2;
      if (pszLongNameOld)
#if 0
         usEntriesNeeded += strlen(pszLongNameOld) / 13 +
            (strlen(pszLongNameOld) % 13 ? 1 : 0);
#else
         //usEntriesNeeded += ( DBCSStrlen( pszLongNameOld ) + 12 ) / 13;
         usEntriesNeeded += ( DBCSStrlen( pszLongNameOld ) + 14 ) / 15;
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

   //pDirectory = (PDIRENTRY1)malloc(2 * (size_t)pCD->ulClusterSize);
   rc = mem_alloc((void **)&pDirectory, 2 * (size_t)pCD->ulClusterSize);
   //if (!pDirectory)
   if (rc)
      {
      return ERROR_NOT_ENOUGH_MEMORY;
      }
   memset(pDirectory, 0, (size_t)pCD->ulClusterSize);
   pDir2 = (PDIRENTRY1)((PBYTE)pDirectory + pCD->ulClusterSize);
   memset(pDir2, 0, (size_t)pCD->ulClusterSize);

   ulCluster = ulDirCluster;
   pLNStart = NULL;
   ulPrevCluster = pCD->ulFatEof;
   //ulPrevBlock = 0;
   usClusterCount = 0;
   fNewCluster = FALSE;

   //if (ulCluster == 1)
   //   {
   //   // root directory starting sector
   //   ulSector = pCD->BootSect.bpb.ReservedSectors +
   //      pCD->BootSect.bpb.SectorsPerFat * pCD->BootSect.bpb.NumberOfFATs;
   //   usSectorsPerBlock = pCD->SectorsPerCluster /
   //      (pCD->ulClusterSize / pCD->ulBlockSize);
   //   usSectorsRead = 0;
   //   ulBytesRemained = pCD->BootSect.bpb.RootDirEntries * sizeof(DIRENTRY);
   //   }

   while (ulCluster != pCD->ulFatEof)
      {
#ifdef CALL_YIELD
      //Yield();
#endif
      usClusterCount++;
      if (!fNewCluster)
         {
         //if (ulCluster == 1)
         //   {
         //   // reading root directory on FAT12/FAT16
         //   rc = ReadSector(pCD, ulSector + ulCluster * usSectorsPerCluster, usSectorsPerCluster, (void *)pDir2);
         //   if (ulBytesRemained >= pCD->ulClusterSize)
         //      {
         //      ulBytesToRead = pCD->ulClusterSize;
         //      ulBytesRemained -= pCD->ulClusterSize;
         //      }
         //   else
         //      {
         //      ulBytesToRead = ulBytesRemained;
         //      ulBytesRemained = 0;
         //      }
         //   }
         //else
         //   {
            rc = ReadCluster(pCD, ulCluster, pDir2);
            ulBytesToRead = pCD->ulClusterSize;
         //   }
         if (rc)
            {
            //free(pDirectory);
            mem_free(pDirectory, 2 * (size_t)pCD->ulClusterSize);
            return rc;
            }
         }
      else
         {
         memset(pDir2, 0, (size_t)pCD->ulClusterSize);
         fNewCluster = FALSE;
         }

      pMax = (PDIRENTRY1)((PBYTE)pDirectory + pCD->ulClusterSize + ulBytesToRead);

      switch (usMode)
         {
         case MODIFY_DIR_RENAME :
         case MODIFY_DIR_UPDATE :
         case MODIFY_DIR_DELETE :
            memcpy(pDirectory, pDir2, pCD->ulClusterSize);
            ulCluster2 = GetNextCluster(pCD, pDirSHInfo, ulCluster, FALSE);
            if (ulCluster2 != pCD->ulFatEof)
               {
               rc = ReadCluster(pCD, ulCluster2, pDir2);
               if (rc)
                  {
                  //free(pDirectory);
                  mem_free(pDirectory, 2 * (size_t)pCD->ulClusterSize);
                  return rc;
                  }
               }
            else
               memset(pDir2, 0, pCD->ulClusterSize);
 
           /*
               Find old entry
            */
 
            fFound = FALSE;
            pWorkFile = NULL;
            pWork = pDirectory;
            //pWork = pDir2;
            pDir1 = NULL;
            pMax = (PDIRENTRY1)((PBYTE)pDirectory + ulBytesToRead);
            while (pWorkFile < pMax && !pLNStart)
            //while (pWork != pMax)
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

            if (fFound)
            //if (pWork != pMax)
               {
               switch (usMode)
                  {
                  case MODIFY_DIR_UPDATE:
                     //if (f32Parms.fMessageActive & LOG_FUNCS)
                     //   Message(" Updating cluster");
                     memcpy(pWorkFile, pNew, sizeof (DIRENTRY1));
                     set_datetime1(pWorkFile);
                     if (pStreamNew)
                        {
                        memcpy(pWorkStream, pStreamNew, sizeof (DIRENTRY1));
                        }
                     pWorkFile->u.File.usSetCheckSum = GetChkSum16((char *)pWorkFile,
                        sizeof(DIRENTRY1) * (pWorkFile->u.File.bSecondaryCount + 1));
                     rc = WriteCluster(pCD, ulCluster, pDirectory);
                     if (rc)
                        {
                        //free(pDirectory);
                        mem_free(pDirectory, 2 * (size_t)pCD->ulClusterSize);
                        return rc;
                        }
                     if (ulCluster2 != pCD->ulFatEof)
                        {
                        //if (ulCluster == 1)
                        //   // reading root directory on FAT12/FAT16
                        //   rc = WriteSector(pCD, ulSector + ulCluster * usSectorsPerCluster, usSectorsPerCluster, (void *)pDir2);
                        //else
                           rc = WriteCluster(pCD, ulCluster2, pDir2);
                        if (rc)
                           {
                           //free(pDirectory);
                           mem_free(pDirectory, 2 * (size_t)pCD->ulClusterSize);
                           return rc;
                           }
                        }
                     ulCluster = pCD->ulFatEof;
                     break;

                  case MODIFY_DIR_DELETE:
                  case MODIFY_DIR_RENAME:
                     //if (f32Parms.fMessageActive & LOG_FUNCS)
                     //   Message(" Removing entry from cluster");
                     pWork2 = pLNStart;
                     //while (pWork2 < pWork)
                     for (usIndex = 0; usIndex < usNumSecondary - 1; usIndex++)
                        {
                        //if (f32Parms.fMessageActive & LOG_FUNCS)
                        //   Message("Deleting Longname entry.");
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
                     //if (ulPrevCluster != pCD->ulFatEof &&
                     //   pLNStart < pDir2)
                     //   {
                     //   if (ulPrevCluster == 1)
                     //      // reading root directory on FAT12/FAT16
                     //      rc = WriteSector(pCD, ulPrevSector + ulPrevCluster * usSectorsPerCluster, usSectorsPerCluster, (void *)pDirectory);
                     //   else
                     //      rc = WriteCluster(pCD, ulPrevCluster, pDirectory);
                     //   if (rc)
                     //      {
                     //      //free(pDirectory);
                     //      mem_free(pDirectory, 2 * (size_t)pCD->ulClusterSize);
                     //      return rc;
                     //      }
                     //   }

                     /*
                     Write current cluster
                     */
                     rc = WriteCluster(pCD, ulCluster, pDirectory);
                     if (rc)
                        {
                        //free(pDirectory);
                        mem_free(pDirectory, 2 * (size_t)pCD->ulClusterSize);
                        return rc;
                        }
                     if (ulCluster2 != pCD->ulFatEof)
                        {
                        //if (ulCluster == 1)
                        //   // reading root directory on FAT12/FAT16
                        //   rc = WriteSector(pCD, ulSector + ulCluster * usSectorsPerCluster, usSectorsPerCluster, (void *)pDir2);
                        //else
                           rc = WriteCluster(pCD, ulCluster2, pDir2);
                        if (rc)
                           {
                           //free(pDirectory);
                           mem_free(pDirectory, 2 * (size_t)pCD->ulClusterSize);
                           return rc;
                           }
                        }

                     if (usMode == MODIFY_DIR_DELETE)
                        ulCluster = pCD->ulFatEof;
                     else
                        {
                        usMode = MODIFY_DIR_INSERT;
                        pszLongNameOld = pszLongNameNew;
                        ulCluster = ulDirCluster;
                        ulBytesRemained = pCD->BootSect.bpb.RootDirEntries * sizeof(DIRENTRY);
                        ulPrevCluster = pCD->ulFatEof;
                        ulPrevSector = 0;
                        //ulPrevBlock = 0;
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

            if (ulPrevCluster != pCD->ulFatEof && GetFreeEntries((PDIRENTRY)pDirectory, ulPrevBytesToRead + ulBytesToRead) >= usEntriesNeeded)
               {
               PDIRENTRY1 pWork3;
               //BYTE bCheck = GetVFATCheckSum(&DirNew);

               //if (f32Parms.fMessageActive & LOG_FUNCS)
               //   Message(" Inserting entry into 2 clusters");

               pWork = CompactDir1(pDirectory, ulPrevBytesToRead + ulBytesToRead, usEntriesNeeded);
               //pWork = fSetLongName(pWork, pszLongNameOld, bCheck);
               //memcpy(pWork, &DirNew, sizeof (DIRENTRY));
               pWork3 = fSetLongName1(pWork+2, pszLongNameOld, &usNameHash);

               pNew->u.File.bSecondaryCount = (BYTE)(pWork3 - pWork - 1);
               memcpy(pWork++, pNew, sizeof (DIRENTRY1));
               pStreamNew->u.Stream.usNameHash = usNameHash;
               pStreamNew->u.Stream.bNameLen = (BYTE)strlen(pszLongNameOld);
               memcpy(pWork++, pStreamNew, sizeof (DIRENTRY1));
               (pWork-2)->u.File.usSetCheckSum = GetChkSum16((char *)(pWork-2),
                  sizeof(DIRENTRY1) * ((pWork-2)->u.File.bSecondaryCount + 1));

               //if (ulPrevCluster == 1)
               //   // reading root directory on FAT12/FAT16
               //   rc = WriteSector(pCD, ulPrevSector + ulPrevCluster * usSectorsPerCluster, usSectorsPerCluster, (void *)pDirectory);
               //else
                  rc = WriteCluster(pCD, ulPrevCluster, pDirectory);
               if (rc)
                  {
                  //free(pDirectory);
                  mem_free(pDirectory, 2 * (size_t)pCD->ulClusterSize);
                  return rc;
                  }
               //if (ulCluster == 1)
               //   // reading root directory on FAT12/FAT16
               //   rc = WriteSector(pCD, ulSector + ulCluster * usSectorsPerCluster, usSectorsPerCluster, (void *)pDir2);
               //else
                  rc = WriteCluster(pCD, ulCluster, pDir2);
               if (rc)
                  {
                  //free(pDirectory);
                  mem_free(pDirectory, 2 * (size_t)pCD->ulClusterSize);
                  return rc;
                  }
               ulCluster = pCD->ulFatEof;
               break;
               }

            usFreeEntries = GetFreeEntries((PDIRENTRY)pDir2, ulBytesToRead);
            if (usFreeEntries >= usEntriesNeeded)
               {
               PDIRENTRY1 pWork3;
               //BYTE bCheck = GetVFATCheckSum(&DirNew);

               //if (f32Parms.fMessageActive & LOG_FUNCS)
               //   Message(" Inserting entry into 1 cluster");

               pWork = CompactDir1(pDir2, ulBytesToRead, usEntriesNeeded);
               pWork3 = fSetLongName1(pWork+2, pszLongNameOld, &usNameHash);

               pNew->u.File.bSecondaryCount = (BYTE)(pWork3 - pWork - 1);
               memcpy(pWork++, pNew, sizeof (DIRENTRY1));
               pStreamNew->u.Stream.usNameHash = usNameHash;
               pStreamNew->u.Stream.bNameLen = (BYTE)strlen(pszLongNameOld);
               memcpy(pWork++, pStreamNew, sizeof (DIRENTRY1));
               (pWork-2)->u.File.usSetCheckSum = GetChkSum16((char *)(pWork-2),
                  sizeof(DIRENTRY1) * ((pWork-2)->u.File.bSecondaryCount + 1));
               //if (ulCluster == 1)
               //   // reading root directory on FAT12/FAT16
               //   rc = WriteSector(pCD, ulSector + ulCluster * usSectorsPerCluster, usSectorsPerCluster, (void *)pDir2);
               //else
                  rc = WriteCluster(pCD, ulCluster, pDir2);
               if (rc)
                  {
                  //free(pDirectory);
                  mem_free(pDirectory, 2 * (size_t)pCD->ulClusterSize);
                  return rc;
                  }
               ulCluster = pCD->ulFatEof;
               break;
               }
            else if (usFreeEntries > 0)
               {
               MarkFreeEntries1(pDir2, ulBytesToRead);
               //if (ulCluster == 1)
               //   // reading root directory on FAT12/FAT16
               //   rc = WriteSector(pCD, ulSector + ulCluster * usSectorsPerCluster, usSectorsPerCluster, (void *)pDir2);
               //else
                  rc = WriteCluster(pCD, ulCluster, pDir2);
               if (rc)
                  {
                  //free(pDirectory);
                  mem_free(pDirectory, 2 * (size_t)pCD->ulClusterSize);
                  return rc;
                  }
               }

            break;
         }

      if (ulCluster != pCD->ulFatEof)
         {
         ulPrevBytesToRead = ulBytesToRead;
         ulPrevCluster = ulCluster;
         ulPrevSector = ulSector;
         //ulPrevBlock = ulBlock;
         memset(pDirectory, 0, (size_t)pCD->ulClusterSize);
         memmove(pDirectory, pDir2, (size_t)ulBytesToRead);
         if (pLNStart)
            pLNStart = (PDIRENTRY1)((PBYTE)pLNStart - pCD->ulClusterSize);
 
         //if (ulCluster == 1)
         //   {
         //   // reading the root directory in case of FAT12/FAT16
         //   ulSector += pCD->SectorsPerCluster;
         //   usSectorsRead += pCD->SectorsPerCluster;
         //   if (usSectorsRead * pCD->BootSect.bpb.BytesPerSector >=
         //       pCD->BootSect.bpb.RootDirEntries * sizeof(DIRENTRY))
         //      // root directory ended
         //      ulNextCluster = 0;
         //   else
         //      ulNextCluster = 1;
         //   }
         //else
            ulNextCluster = GetNextCluster(pCD, pDirSHInfo, ulCluster, FALSE);
         if (!ulNextCluster)
            ulNextCluster = pCD->ulFatEof;
         if (ulNextCluster == pCD->ulFatEof)
            {
            if (usMode == MODIFY_DIR_UPDATE ||
                usMode == MODIFY_DIR_DELETE ||
                usMode == MODIFY_DIR_RENAME)
               {
               //free(pDirectory);
               mem_free(pDirectory, 2 * (size_t)pCD->ulClusterSize);
               return ERROR_FILE_NOT_FOUND;
               }
            else
               {
               //if (ulCluster == 1)
               //   {
               //   // no expanding for root directory in case of FAT12/FAT16
               //   ulNextCluster = pCD->ulFatEof;
               //   }
               //else
                  ulNextCluster = SetNextCluster(pCD, ulCluster, FAT_ASSIGN_NEW);
               if (ulNextCluster == pCD->ulFatEof)
                  {
                  //free(pDirectory);
                  mem_free(pDirectory, 2 * (size_t)pCD->ulClusterSize);
                  return ERROR_DISK_FULL;
                  }
               fNewCluster = TRUE;
               }
            }
         ulCluster = ulNextCluster;
         }
      }

   //free(pDirectory);
   mem_free(pDirectory, 2 * (size_t)pCD->ulClusterSize);
   return 0;
}

#endif

APIRET ModifyDirectory(PCDINFO pCD, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo,
                       USHORT usMode, PDIRENTRY pOld, PDIRENTRY pNew,
                       PDIRENTRY1 pStreamOld, PDIRENTRY1 pStreamNew, PSZ pszLongNameOld, PSZ pszLongNameNew)
{
APIRET rc;

#ifdef EXFAT
      if (pCD->bFatType < FAT_TYPE_EXFAT)
#endif
         rc = ModifyDirectory0(pCD, ulDirCluster, usMode, pOld, pNew,
                               pszLongNameOld);
#ifdef EXFAT
      else
         rc = ModifyDirectory1(pCD, ulDirCluster, pDirSHInfo, usMode,
                               (PDIRENTRY1)pOld, (PDIRENTRY1)pNew,
                               pStreamOld, pStreamNew, pszLongNameOld, pszLongNameNew);
#endif

   return rc;
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

/******************************************************************
*
******************************************************************/
APIRET MakeShortName(PCDINFO pCD, ULONG ulDirCluster, PSZ pszLongName, PSZ pszShortName)
{
   USHORT usLongName;
   PSZ pLastDot;
   PSZ pFirstDot;
   PSZ p;
   USHORT usIndex;
   BYTE szShortName[12];
   PSZ  pszUpper;
   APIRET rc;

   usLongName = LONGNAME_OFF;
   memset(szShortName, 0x20, 11);
   szShortName[11] = 0;

   /*
      Uppercase the longname
   */
   usIndex = strlen(pszLongName) + 5;
   pszUpper = malloc(usIndex);
   if (!pszUpper)
      return LONGNAME_ERROR;

   strupr(pszLongName); // !!! @todo DBCS/Unicode

   /* Skip all leading dots */
   p = pszUpper;
   while (*p == '.')
      p++;

   pLastDot  = strrchr(p, '.');
   pFirstDot = strchr(p, '.');

   if (!pLastDot)
      pLastDot = pszUpper + strlen(pszUpper);
   if (!pFirstDot)
      pFirstDot = pLastDot;

   /*
      Is the name a valid 8.3 name ?
   */
   if ((!strcmp(pszLongName, pszUpper)) &&
      pFirstDot == pLastDot &&
      pLastDot - pszUpper <= 8 &&
      strlen(pLastDot) <= 4)
      {
      PSZ p = pszUpper;

      if (*p != '.')
         {
         while (*p)
            {
            if (*p < 128 && ! IsCharValid(*p) && *p != '.')
               break;
            p++;
            }
         }

      if (!(*p))
         {
         memset(szShortName, 0x20, sizeof szShortName);
         szShortName[11] = 0;
         memcpy(szShortName, pszUpper, pLastDot - pszUpper);
         if (*pLastDot)
            memcpy(szShortName + 8, pLastDot + 1, strlen(pLastDot + 1));

         memcpy(pszShortName, szShortName, 11);
         free(pszUpper);
         return usLongName;
         }
      }

   usLongName = LONGNAME_OK;

   if (pLastDot - pszUpper > 8)
      usLongName = LONGNAME_MAKE_UNIQUE;

   szShortName[11] = 0;


   usIndex = 0;
   p = pszUpper;
   while (usIndex < 11)
      {
      if (!(*p))
         break;

      if (usIndex == 8 && p <= pLastDot)
         {
         if (p < pLastDot)
            usLongName = LONGNAME_MAKE_UNIQUE;
         if (*pLastDot)
            p = pLastDot + 1;
         else
            break;
         }

      while (*p == 0x20)
         {
         usLongName = LONGNAME_MAKE_UNIQUE;
         p++;
         }
      if (!(*p))
         break;

      if (*p >= 128)
         {
         szShortName[usIndex++] = *p;
         }
      else if (*p == '.')
         {
         /*
            Skip all dots, if multiple dots are in the
            name create an unique name
         */
         if (p == pLastDot)
            usIndex = 8;
         else
            usLongName = LONGNAME_MAKE_UNIQUE;
         }
      else if (IsCharValid(*p))
         szShortName[usIndex++] = *p;
      else
         {
         szShortName[usIndex++] = '_';
         }
      p++;
      }
   if (strlen(p))
      usLongName = LONGNAME_MAKE_UNIQUE;

   free(pszUpper);

   p = szShortName;
   for( usIndex = 0; usIndex < 8; usIndex++ )
      if( IsDBCSLead( p[ usIndex ]))
         usIndex++;

   if( usIndex > 8 )
      p[ 7 ] = 0x20;

   p = szShortName + 8;
   for( usIndex = 0; usIndex < 3; usIndex++ )
      if( IsDBCSLead( p[ usIndex ]))
         usIndex++;

   if( usIndex > 3 )
      p[ 2 ] = 0x20;

   if (usLongName == LONGNAME_MAKE_UNIQUE)
      {
      USHORT usNum;
      BYTE   szFileName[25];
      BYTE   szNumber[18];
      ULONG ulCluster;
      PSZ p;
      USHORT usPos1, usPos2;

      for (usPos1 = 8; usPos1 > 0; usPos1--)
         if (szShortName[usPos1 - 1] != 0x20)
            break;

      for (usNum = 1; usNum < 32000; usNum++)
         {
         memset(szFileName, 0, sizeof szFileName);
         memcpy(szFileName, szShortName, 8);

         /*
            Find last blank in filename before dot.
         */

         memset(szNumber, 0, sizeof szNumber);
         itoa(usNum, szNumber, 10);

         usPos2 = 7 - (strlen(szNumber));
         if (usPos1 && usPos1 < usPos2)
            usPos2 = usPos1;

         for( usIndex = 0; usIndex < usPos2; usIndex++ )
            if( IsDBCSLead( szShortName[ usIndex ]))
               usIndex++;

         if( usIndex > usPos2 )
            usPos2--;

         strcpy(szFileName + usPos2, "~");
         strcat(szFileName, szNumber);

         if (memcmp(szShortName + 8, "   ", 3))
            {
            strcat(szFileName, ".");
            memcpy(szFileName + strlen(szFileName), szShortName + 8, 3);
            p = szFileName + strlen(szFileName);
            while (p > szFileName && *(p-1) == 0x20)
               p--;
            *p = 0;
            }
         ulCluster = FindPathCluster(pCD, ulDirCluster, szFileName, NULL, NULL, NULL, NULL);
         if (ulCluster == pCD->ulFatEof)
            break;
         }
      if (usNum < 32000)
         {
         p = strchr(szFileName, '.');
#if 0
         if (p && p - szFileName < 8)
            memcpy(szShortName, szFileName, p - szFileName);
         else
            memccpy(szShortName, szFileName, 0, 8 );
         }
#else
         if( !p )
            p = szFileName + strlen( szFileName );

         memcpy(szShortName, szFileName, p - szFileName);
         memset( szShortName + ( p - szFileName ), 0x20, 8 - ( p - szFileName ));
#endif
         }
      else
         return LONGNAME_ERROR;
      }
   memcpy(pszShortName, szShortName, 11);
   return usLongName;
}

/******************************************************************
*
******************************************************************/
PDIRENTRY fSetLongName(PDIRENTRY pDir, PSZ pszLongName, BYTE bCheck)
{
   USHORT usNeededEntries;
   PDIRENTRY pRet;
   BYTE bCurEntry;
   PLNENTRY pLN;
   USHORT usIndex;
   USHORT uniName[13];
   USHORT uniEnd[13];
   PUSHORT p;

   if (!pszLongName || !strlen(pszLongName))
      return pDir;

#if 0
   usNeededEntries = strlen(pszLongName) / 13 +
      (strlen(pszLongName) % 13 ? 1 : 0);
#else
   usNeededEntries = ( DBCSStrlen( pszLongName ) + 12 ) / 13;
#endif

   if (!usNeededEntries)
      return pDir;

   pDir += (usNeededEntries - 1);
   pRet = pDir + 1;
   pLN = (PLNENTRY)pDir;

   bCurEntry = 1;
   while (*pszLongName)
      {
#if 0
      USHORT usLen;
#endif
      pLN->bNumber = bCurEntry;
      if (DBCSStrlen(pszLongName) <= 13)
         pLN->bNumber += 0x40;
      pLN->wCluster = 0L;
      pLN->bAttr = FILE_LONGNAME;
      pLN->bReserved = 0;
      pLN->bVFATCheckSum = bCheck;

#if 0
      usLen = strlen(pszLongName);
      if (usLen > 13)
         usLen = 13;
#endif

      memset(uniEnd, 0xFF, sizeof uniEnd);
      memset(uniName, 0, sizeof uniName);

      pszLongName += Translate2Win(pszLongName, uniName, 13);

      p = uniName;
      for (usIndex = 0; usIndex < 5; usIndex ++)
         {
         pLN->usChar1[usIndex] = *p;
         if (*p == 0)
            p = uniEnd;
         p++;
         }

      for (usIndex = 0; usIndex < 6; usIndex ++)
         {
         pLN->usChar2[usIndex] = *p;
         if (*p == 0)
            p = uniEnd;
         p++;
         }

      for (usIndex = 0; usIndex < 2; usIndex ++)
         {
         pLN->usChar3[usIndex] = *p;
         if (*p == 0)
            p = uniEnd;
         p++;
         }

      pLN--;
      bCurEntry++;
      }

   return pRet;
}

#ifdef EXFAT

/******************************************************************
*
******************************************************************/
PDIRENTRY1 fSetLongName1(PDIRENTRY1 pDir, PSZ pszLongName, PUSHORT pusNameHash)
{
// exFAT case
USHORT usNeededEntries;
BYTE bCurEntry;
PDIRENTRY1 pLN;
USHORT usIndex;
UCHAR  szLongName1[FAT32MAXPATH];
USHORT pusUniName[256];
USHORT uniName[15];
USHORT uniName1[15];
PSZ pszLongName1 = szLongName1;
PUSHORT p, q, p1;
PSZ     r;

   if (!pszLongName || !strlen(pszLongName))
      return pDir;

   // @todo Use upcase table
   strcpy(pszLongName1, pszLongName);
   ////FSH_UPPERCASE(pszLongName1, FAT32MAXPATH, pszLongName1); ////

   //pusUniName = malloc(256 * sizeof(USHORT));

   usNeededEntries = ( DBCSStrlen( pszLongName ) + 14 ) / 15;

   if (!usNeededEntries)
      return pDir;

   pLN = (PDIRENTRY1)pDir;
   q = pusUniName;
   r = pszLongName;

   bCurEntry = 1;
   while (*pszLongName)
      {
      pLN->bEntryType = ENTRY_TYPE_FILE_NAME;
      pLN->u.FileName.bAllocPossible = 0;
      pLN->u.FileName.bNoFatChain = 0;
      pLN->u.FileName.bCustomDefined1 = 0;

      memset(uniName, 0, sizeof uniName);
      memset(uniName1, 0, sizeof uniName1);

      pszLongName += Translate2Win(pszLongName, uniName, 15);
      pszLongName1 += Translate2Win(pszLongName1, uniName1, 15);

      p = uniName;
      p1 = uniName1;

      for (usIndex = 0; usIndex < 15; usIndex ++)
         {
         pLN->u.FileName.bFileName[usIndex] = *p++;
         *q++ = *p1++;
         }

      pLN++;
      bCurEntry++;
      }

   if (pusNameHash)
      *pusNameHash = NameHash(pusUniName, DBCSStrlen(r));

   //free(pusUniName);

   return pLN;
}

#endif

/******************************************************************
*
******************************************************************/
USHORT DBCSStrlen( const PSZ pszStr )
{
   USHORT usLen;
   USHORT usIndex;
   USHORT usRet;

   usLen = strlen( pszStr );
   usRet = 0;
   for( usIndex = 0; usIndex < usLen; usIndex++ )
      {
         if( IsDBCSLead( pszStr[ usIndex ]))
            usIndex++;

         usRet++;
      }

   return usRet;
}


/******************************************************************
*
******************************************************************/
APIRET SetNextCluster(PCDINFO pCD, ULONG ulCluster, ULONG ulNext)
{
   ULONG ulNextCluster = 0;
   BOOL fUpdateFSInfo;
   ULONG ulReturn;
   APIRET rc;

   ulReturn = ulNext;
   if (ulCluster == FAT_ASSIGN_NEW)
      {
      /*
         A new seperate CHAIN is started.
      */
      ulCluster = GetFreeCluster(pCD);
      if (ulCluster == pCD->ulFatEof)
         return pCD->ulFatEof;
      ulReturn = ulCluster;
      ulNext = pCD->ulFatEof;
      }

   else if (ulNext == FAT_ASSIGN_NEW)
      {
      /*
         An existing chain is extended
      */
      ulNext = SetNextCluster(pCD, FAT_ASSIGN_NEW, pCD->ulFatEof);
      if (ulNext == pCD->ulFatEof)
         return pCD->ulFatEof;
      ulReturn = ulNext;
      }

#ifdef EXFAT
   if (pCD->bFatType == FAT_TYPE_EXFAT)
      {
      // mark cluster in exFAT allocation bitmap
      MarkCluster2(pCD, ulCluster, (BOOL)ulNext);
      }
#endif

   if (ReadFatSector(pCD, GetFatEntrySec(pCD, ulCluster)))
      {
#ifdef EXFAT
      if (pCD->bFatType == FAT_TYPE_EXFAT)
         {
         // mark cluster in exFAT allocation bitmap
         MarkCluster2(pCD, ulCluster, FALSE);
         }
#endif
      return pCD->ulFatEof;
      }

   fUpdateFSInfo = FALSE;
   ulNextCluster = GetFatEntry(pCD, ulCluster);
   if (ulNextCluster && !ulNext)
      {
      fUpdateFSInfo = TRUE;
      pCD->FSInfo.ulFreeClusters++;
      }
   if (ulNextCluster == 0 && ulNext)
      {
      fUpdateFSInfo = TRUE;
      pCD->FSInfo.ulNextFreeCluster = ulCluster;
      pCD->FSInfo.ulFreeClusters--;
      }

   SetFatEntry(pCD, ulCluster, ulNext);

   rc = WriteFatSector(pCD, GetFatEntrySec(pCD, ulCluster));
   if (rc)
      {
#ifdef EXFAT
      if (pCD->bFatType == FAT_TYPE_EXFAT)
         {
         // mark cluster in exFAT allocation bitmap
         MarkCluster2(pCD, ulCluster, FALSE);
         }
#endif
      return pCD->ulFatEof;
      }

   if (fUpdateFSInfo)
      UpdateFSInfo(pCD);

   return ulReturn;
}

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

/******************************************************************
*
******************************************************************/
USHORT GetFreeEntries(PDIRENTRY pDirBlock, ULONG ulSize)
{
   USHORT usCount;
   PDIRENTRY pMax;
   BOOL bLoop;

   pMax = (PDIRENTRY)((PBYTE)pDirBlock + ulSize);
   usCount = 0;
   bLoop = pMax == pDirBlock;
   while (( pDirBlock != pMax ) || bLoop )
      {
      if (!pDirBlock->bFileName[0] || pDirBlock->bFileName[0] == DELETED_ENTRY)
         usCount++;
      bLoop = FALSE;
      pDirBlock++;
      }

   return usCount;
}

#ifdef EXFAT

USHORT GetFreeEntries1(PDIRENTRY1 pDirBlock, ULONG ulSize)
{
// GetFreeEntries version for exFAT
USHORT usCount;
PDIRENTRY1 pMax, pDirBlock2 = pDirBlock;
BOOL bLoop;

   pMax = (PDIRENTRY1)((PBYTE)pDirBlock + ulSize);
   usCount = 0;
   bLoop = pMax == pDirBlock;
   while (( pDirBlock != pMax ) || bLoop )
      {
      if ( pDirBlock->bEntryType == ENTRY_TYPE_EOD ||
           !(pDirBlock->bEntryType & ENTRY_TYPE_IN_USE_STATUS) )
         usCount++;
      bLoop = FALSE;
      pDirBlock++;
      }

   return usCount;
}

#endif

/******************************************************************
*
******************************************************************/
PDIRENTRY CompactDir(PDIRENTRY pStart, ULONG ulSize, USHORT usEntriesNeeded)
{
   PDIRENTRY pTar, pMax, pFirstFree;
   USHORT usFreeEntries;
   BOOL bLoop;

   pMax = (PDIRENTRY)((PBYTE)pStart + ulSize);
   bLoop = pMax == pStart;
   pFirstFree = pMax;
   usFreeEntries = 0;
   while (( pFirstFree != pStart ) || bLoop )
      {
      if (!(pFirstFree-1)->bFileName[0])
         usFreeEntries++;
      else
         break;
      bLoop = FALSE;
      pFirstFree--;
      }

   if ((( pFirstFree == pStart ) && !bLoop ) || (pFirstFree - 1)->bAttr != FILE_LONGNAME)
      if (usFreeEntries >= usEntriesNeeded)
         return pFirstFree;

   /*
      Leaving longname entries at the end
   */
   while ((( pFirstFree != pStart ) || bLoop ) && (pFirstFree - 1)->bAttr == FILE_LONGNAME)
   {
      bLoop = FALSE;
      pFirstFree--;
   }

   usFreeEntries = 0;
   pTar = pStart;
   while ((( pStart != pFirstFree ) || bLoop ) && usFreeEntries < usEntriesNeeded)
      {
      if (pStart->bFileName[0] && pStart->bFileName[0] != DELETED_ENTRY)
         {
         if (pTar != pStart)
            *pTar = *pStart;
         pTar++;
         }
      else
         usFreeEntries++;

      bLoop = FALSE;
      pStart++;
      }
   if (pTar != pStart)
      {
#if 1
      USHORT usEntries = 0;
      PDIRENTRY p;

      for( p = pStart; ( p != pFirstFree ) /*|| bLoop */; p++ )
        {
            /*bLoop = FALSE;*/
            usEntries++;
        }
      memmove(pTar, pStart, usEntries * sizeof (DIRENTRY));
#else
      memmove(pTar, pStart, (pFirstFree - pStart) * sizeof (DIRENTRY));
#endif
      pFirstFree -= usFreeEntries;
      memset(pFirstFree, DELETED_ENTRY, usFreeEntries * sizeof (DIRENTRY));
      }

   return pFirstFree;
}

#ifdef EXFAT

PDIRENTRY1 CompactDir1(PDIRENTRY1 pStart, ULONG ulSize, USHORT usEntriesNeeded)
{
// CompactDir version for exFAT
PDIRENTRY1 pTar, pMax, pFirstFree;
USHORT usFreeEntries;
BOOL bLoop;

   pMax = (PDIRENTRY1)((PBYTE)pStart + ulSize);
   bLoop = pMax == pStart;
   pFirstFree = pMax;
   usFreeEntries = 0;
   while (( pFirstFree != pStart ) || bLoop )
      {
      BYTE bSecondaries;
      //if (!(pFirstFree - 1)->bFileName[0])
      if ( (pFirstFree - 1)->bEntryType == ENTRY_TYPE_EOD )
         {
         usFreeEntries++;
         bSecondaries = 0;
         }
      else if ( (pFirstFree - 1)->bEntryType == ENTRY_TYPE_STREAM_EXT )
         {
         usFreeEntries++;
         bSecondaries++;
         }
      else if ( (pFirstFree - 1)->bEntryType == ENTRY_TYPE_FILE_NAME )
         {
         usFreeEntries++;
         bSecondaries++;
         }
      else if ( (pFirstFree - 1)->bEntryType == ENTRY_TYPE_FILE )
         {
         if ((pFirstFree - 1)->u.File.bSecondaryCount == bSecondaries)
            {
            pFirstFree += bSecondaries;
            usFreeEntries -= bSecondaries;
            bSecondaries = 0;
            break;
            }
         else
            {
            usFreeEntries++;
            bSecondaries = 0;
            }
         }
      else
         break;
      bLoop = FALSE;
      pFirstFree--;
      }

   //if ((( pFirstFree == pStart ) && !bLoop ) || (pFirstFree - 1)->bAttr != FILE_LONGNAME)
   if ( (( pFirstFree == pStart ) && !bLoop ) || (pFirstFree - 1)->bEntryType == ENTRY_TYPE_FILE_NAME)
      {
      if (usFreeEntries >= usEntriesNeeded)
         return pFirstFree;
      }

   /*
      Leaving longname entries at the end
   */
   //while ((( pFirstFree != pStart ) || bLoop ) && (pFirstFree - 1)->bAttr == FILE_LONGNAME)
   //{
   //   bLoop = FALSE;
   //   pFirstFree--;
   //}

   usFreeEntries = 0;
   pTar = pStart;
   while ((( pStart != pFirstFree ) || bLoop ) && usFreeEntries < usEntriesNeeded)
      {
      //if (pStart->bFileName[0] && pStart->bFileName[0] != DELETED_ENTRY)
      if (pStart->bEntryType & ENTRY_TYPE_IN_USE_STATUS)
         {
         if (pTar != pStart)
            *pTar = *pStart;
         pTar++;
         }
      else
         usFreeEntries++;

      bLoop = FALSE;
      pStart++;
      }
   if (pTar != pStart)
      {
#if 1
      USHORT usEntries = 0;
      PDIRENTRY1 p;

      for( p = pStart; ( p != pFirstFree ) /*|| bLoop */; p++ )
        {
            /*bLoop = FALSE;*/
            usEntries++;
        }
      memmove(pTar, pStart, usEntries * sizeof (DIRENTRY));
#else
      memmove(pTar, pStart, (pFirstFree - pStart) * sizeof (DIRENTRY));
#endif
      pFirstFree -= usFreeEntries;
      //memset(pFirstFree, DELETED_ENTRY, usFreeEntries * sizeof (DIRENTRY));
      memset(pFirstFree, ENTRY_TYPE_EOD, usFreeEntries * sizeof (DIRENTRY));
      }

   return pFirstFree;
}

#endif

/******************************************************************
*
******************************************************************/
VOID MarkFreeEntries(PDIRENTRY pDirBlock, ULONG ulSize)
{
   PDIRENTRY pMax;

   pMax = (PDIRENTRY)((PBYTE)pDirBlock + ulSize);
   while (pDirBlock != pMax)
      {
      if (!pDirBlock->bFileName[0])
         pDirBlock->bFileName[0] = DELETED_ENTRY;
      pDirBlock++;
      }
}

#ifdef EXFAT

/******************************************************************
*
******************************************************************/
VOID MarkFreeEntries1(PDIRENTRY1 pDirBlock, ULONG ulSize)
{
PDIRENTRY1 pMax, pDirBlock2 = pDirBlock;

   pMax = (PDIRENTRY1)((PBYTE)pDirBlock + ulSize);
   while (pDirBlock != pMax)
      {
      if (pDirBlock->bEntryType == ENTRY_TYPE_EOD)
         pDirBlock->bEntryType &= ~ENTRY_TYPE_IN_USE_STATUS;
      pDirBlock++;
      }
}

#endif

/******************************************************************
*
******************************************************************/
ULONG GetFreeCluster(PCDINFO pCD)
{
   ULONG ulStartCluster;
   ULONG ulCluster;
   BOOL fStartAt2;

   if (pCD->FSInfo.ulFreeClusters == 0L)
      return pCD->ulFatEof;

   fStartAt2 = FALSE;
   //ulCluster = pCD->FSInfo.ulNextFreeCluster + 1;
   ulCluster = pCD->FSInfo.ulNextFreeCluster;
   if (!ulCluster || ulCluster >= pCD->ulTotalClusters + 2)
      {
      fStartAt2 = TRUE;
      ulCluster = 2;
      ulStartCluster = pCD->ulTotalClusters + 2;
      }
   else
      {
      ulStartCluster = ulCluster;
      }

   //while (GetNextCluster(pCD, NULL, ulCluster, TRUE))
   while (ClusterInUse2(pCD, ulCluster))
      {
      ulCluster++;
      if (fStartAt2 && ulCluster >= ulStartCluster)
         return pCD->ulFatEof;

      if (ulCluster >= pCD->ulTotalClusters + 2)
         {
         if (!fStartAt2)
            {
            ulCluster = 2;
            fStartAt2 = TRUE;
            }
         else
            {
            return pCD->ulFatEof;
            }
         }
      }
   return ulCluster;
}

/******************************************************************
*
******************************************************************/
APIRET SetFileSize(PCDINFO pCD, PFILESIZEDATA pFileSize)
{
   ULONG ulDirCluster;
   PSZ   pszFile;
   ULONG ulCluster;
   DIRENTRY DirEntry;
   DIRENTRY DirNew;
   DIRENTRY1 DirStream, DirStreamNew, DirStreamEntry;
   ULONG ulClustersNeeded;
   ULONG ulClustersUsed;
#ifdef EXFAT
   SHOPENINFO SHInfo;
#endif
   PSHOPENINFO pSHInfo = NULL;
   SHOPENINFO DirSHInfo;
   PSHOPENINFO pDirSHInfo = NULL;
   APIRET rc;

   ulDirCluster = FindDirCluster(pCD,
      pFileSize->szFileName,
      0xFFFF,
      RETURN_PARENT_DIR,
      &pszFile,
      &DirStreamEntry);

   if (ulDirCluster == pCD->ulFatEof)
      return ERROR_PATH_NOT_FOUND;

#ifdef EXFAT
   if (pCD->bFatType == FAT_TYPE_EXFAT)
      {
      pDirSHInfo = &DirSHInfo;
      SetSHInfo1(pCD, (PDIRENTRY1)&DirStreamEntry, pDirSHInfo);
      }
#endif

   ulCluster = FindPathCluster(pCD, ulDirCluster, pszFile,
      pDirSHInfo, &DirEntry, &DirStream, NULL);
   if (ulCluster == pCD->ulFatEof)
      return ERROR_FILE_NOT_FOUND;
   if (!ulCluster)
      pFileSize->ullFileSize = 0L;

   ulClustersNeeded = pFileSize->ullFileSize / pCD->ulClusterSize;
   if (pFileSize->ullFileSize % pCD->ulClusterSize)
      ulClustersNeeded++;

#ifdef EXFAT
   if (pCD->bFatType == FAT_TYPE_EXFAT)
      {
      pSHInfo = &SHInfo;
      SetSHInfo1(pCD, (PDIRENTRY1)&DirStream, pSHInfo);
      }
#endif

   if (pFileSize->ullFileSize > 0 )
      {
      ulClustersUsed = 1;
      while (ulClustersUsed < ulClustersNeeded)
         {
         ULONG ulNextCluster = GetNextCluster(pCD, pSHInfo, ulCluster, TRUE);
         if (!ulNextCluster)
            break;
         ulCluster = ulNextCluster;
         if (ulCluster == pCD->ulFatEof)
            break;
         ulClustersUsed++;
         }
      if (ulCluster == pCD->ulFatEof)
         pFileSize->ullFileSize = ulClustersUsed * pCD->ulClusterSize;
      else
         SetNextCluster(pCD, ulCluster, pCD->ulFatEof);
      }

   memcpy(&DirNew, &DirEntry, sizeof (DIRENTRY));
#ifdef EXFAT
   if (pCD->bFatType < FAT_TYPE_EXFAT)
      {
#endif
      ULONGLONG ullSize;
      FileSetSize(pCD, &DirNew, ulDirCluster, pDirSHInfo, pszFile, pFileSize->ullFileSize);
#ifdef EXFAT
      }
   else
      {
      DirStreamNew.u.Stream.ullValidDataLen = pFileSize->ullFileSize;
      DirStreamNew.u.Stream.ullDataLen = pFileSize->ullFileSize;
      //   (pFileSize->ullFileSize / pCD->ulClusterSize) * pCD->ulClusterSize +
      //   ((pFileSize->ullFileSize % pCD->ulClusterSize) ? pCD->ulClusterSize : 0);
      }
#endif

   if (!pFileSize->ullFileSize)
      {
#ifdef EXFAT
      if (pCD->bFatType < FAT_TYPE_EXFAT)
         {
#endif
         DirNew.wCluster = 0;
         DirNew.wClusterHigh = 0;
#ifdef EXFAT
         }
      else
         {
         DirStreamNew.u.Stream.ulFirstClus = 0;
         }
#endif
      }


   rc = ModifyDirectory(pCD, ulDirCluster, pDirSHInfo, MODIFY_DIR_UPDATE,
      &DirEntry, &DirNew, &DirStream, &DirStreamNew, pszFile, NULL);

   return rc;
}


/******************************************************************
*
******************************************************************/
ULONG MakeDir(PCDINFO pCD, ULONG ulDirCluster, PDIRENTRY pDir, PSZ pszFile)
{
ULONG ulCluster;
PVOID pbCluster;
#ifdef EXFAT
PDIRENTRY1 pDir1;
SHOPENINFO DirSHInfo;
#endif
PDIRENTRY1 pDirStream;
PSHOPENINFO pDirSHInfo = NULL;
APIRET rc;

   ulCluster = SetNextCluster(pCD, FAT_ASSIGN_NEW, pCD->ulFatEof);
   if (ulCluster == pCD->ulFatEof)
      {
      ulCluster = pCD->ulFatEof;
      goto MKDIREXIT;
      }

   pbCluster = malloc(pCD->ulClusterSize);
   if (!pbCluster)
      {
      SetNextCluster( pCD, ulCluster, 0L);
      ulCluster = pCD->ulFatEof;
      goto MKDIREXIT;
      }

   memset(pbCluster, 0, pCD->ulClusterSize);

#ifdef EXFAT
   if (pCD->bFatType < FAT_TYPE_EXFAT)   
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
      (pDir1+1)->u.Stream.ullValidDataLen = pCD->ulClusterSize;
      (pDir1+1)->u.Stream.ullDataLen = pCD->ulClusterSize;
      (pDir1+1)->u.Stream.ulFirstClus = ulCluster;
      pDirStream = pDir1+1;
      }

   if (pCD->bFatType == FAT_TYPE_EXFAT)
      {
      pDirSHInfo = &DirSHInfo;
      SetSHInfo1(pCD, pDirStream, pDirSHInfo);
      }
#endif

   rc = MakeDirEntry(pCD, ulDirCluster, pDirSHInfo, (PDIRENTRY)pbCluster,
                     (PDIRENTRY1)((PBYTE)pbCluster+sizeof(DIRENTRY1)), pszFile);
   if (rc)
      {
      free(pbCluster);
      ulCluster = pCD->ulFatEof;
      goto MKDIREXIT;
      }

#ifdef EXFAT
   if (pCD->bFatType < FAT_TYPE_EXFAT)   
      {
#endif
      memset(pDir->bFileName, 0x20, 11);
      memcpy(pDir->bFileName, ".", 1);

      memcpy(pDir + 1, pDir, sizeof (DIRENTRY));
      pDir++;

      memcpy(pDir->bFileName, "..", 2);
      if (ulDirCluster == pCD->BootSect.bpb.RootDirStrtClus)
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
      memset(pbCluster, 0, (size_t)pCD->ulClusterSize);
#endif

   rc = WriteCluster(pCD, ulCluster, pbCluster);

   if (rc)
      {
      ulCluster = pCD->ulFatEof;
      goto MKDIREXIT;
      }

MKDIREXIT:
   return ulCluster;
}

/******************************************************************
*
******************************************************************/
USHORT RecoverChain2(PCDINFO pCD, ULONG ulCluster, PBYTE pData, USHORT cbData)
{
   static ULONG ulDirCluster = 0;
   DIRENTRY DirEntry;
   PDIRENTRY1 pDirEntry = (PDIRENTRY1)&DirEntry;
   DIRENTRY1 DirStream;
   BYTE     szFileName[14];
   USHORT   usNr;
#ifdef EXFAT
   SHOPENINFO DirSHInfo;
#endif
   PSHOPENINFO pDirSHInfo = NULL;

   memset(&DirEntry, 0, sizeof (DIRENTRY));
   memset(&DirStream, 0, sizeof (DIRENTRY1));

#ifdef EXFAT
   if (pCD->bFatType < FAT_TYPE_EXFAT)
#endif
      {
      memcpy(DirEntry.bFileName, "FOUND   000", 11);
      }
   strcpy(szFileName, "FOUND.000");
   for (usNr = 0; usNr <= 999; usNr++)
      {
      USHORT iPos = 8;
      USHORT usNum = usNr;

         while (usNum)
            {
            szFileName[iPos] = (BYTE)((usNum % 10) + '0');
            usNum /= 10;
            iPos--;
            }
         if (FindPathCluster(pCD, pCD->BootSect.bpb.RootDirStrtClus,
            szFileName, NULL, NULL, NULL, NULL) == pCD->ulFatEof)
            break;
      }
   if (usNr > 999)
      return ERROR_FILE_EXISTS;
#ifdef EXFAT
   if (pCD->bFatType < FAT_TYPE_EXFAT)
#endif
      memcpy(DirEntry.bExtention, szFileName + 6, 3);


#ifdef EXFAT
   if (pCD->bFatType < FAT_TYPE_EXFAT)
#endif
      set_datetime(&DirEntry);
#ifdef EXFAT
   else
      set_datetime1((PDIRENTRY1)&DirEntry);
#endif

   if (!ulDirCluster)
      ulDirCluster = MakeDir(pCD, pCD->BootSect.bpb.RootDirStrtClus, &DirEntry, szFileName);
   if (ulDirCluster == pCD->ulFatEof)
      return ERROR_DISK_FULL;

   memset(&DirEntry, 0, sizeof (DIRENTRY));

#ifdef EXFAT
   if (pCD->bFatType < FAT_TYPE_EXFAT)
#endif
      {
      memcpy(DirEntry.bFileName, "FILE0000CHK", 11);
      }
   strcpy(szFileName, "FILE0000.CHK");
   for (usNr = 0; usNr < 9999; usNr++)
      {
      USHORT iPos = 7;
      USHORT usNum = usNr;

         while (usNum)
            {
            szFileName[iPos] = (BYTE)((usNum % 10) + '0');
            usNum /= 10;
            iPos--;
            }
         if (FindPathCluster(pCD, ulDirCluster,
            szFileName, NULL, NULL, NULL, NULL) == pCD->ulFatEof)
            break;
      }
   if (usNr == 9999)
      return ERROR_FILE_EXISTS;
#ifdef EXFAT
   if (pCD->bFatType < FAT_TYPE_EXFAT)
#endif
      memcpy(DirEntry.bFileName, szFileName, 8);
   if (pData)
      strncpy(pData, szFileName, cbData);

#ifdef EXFAT
   if (pCD->bFatType < FAT_TYPE_EXFAT)
#endif
      set_datetime(&DirEntry);
#ifdef EXFAT
   else
      set_datetime1((PDIRENTRY1)&DirEntry);
#endif

#ifdef EXFAT
   if (pCD->bFatType < FAT_TYPE_EXFAT)
      {
#endif
      DirEntry.wCluster = LOUSHORT(ulCluster);
      DirEntry.wClusterHigh = HIUSHORT(ulCluster);
#ifdef EXFAT
      }
   else
      {
      DirStream.u.Stream.ulFirstClus = ulCluster;
      }
#endif
   while (ulCluster != pCD->ulFatEof)
      {
      ULONG ulNextCluster;
#ifdef EXFAT
      if (pCD->bFatType < FAT_TYPE_EXFAT)
#endif
         {
         ULONGLONG ullSize;
         FileGetSize(pCD, &DirEntry, ulDirCluster, NULL, szFileName, &ullSize);
         ullSize += pCD->ulClusterSize;
         FileSetSize(pCD, &DirEntry, ulDirCluster, NULL, szFileName, ullSize);
         }
#ifdef EXFAT
      else
         {
         DirStream.u.Stream.ullValidDataLen += pCD->ulClusterSize;
         DirStream.u.Stream.ullDataLen += pCD->ulClusterSize;

         pDirSHInfo = &DirSHInfo;
         SetSHInfo1(pCD, &DirStream, pDirSHInfo);
         }
#endif
      ulNextCluster = GetNextCluster(pCD, pDirSHInfo, ulCluster, TRUE);
      if (!ulNextCluster)
         {
         SetNextCluster(pCD, ulCluster, pCD->ulFatEof);
         ulCluster = pCD->ulFatEof;
         }
      else
         ulCluster = ulNextCluster;
      }

   return MakeDirEntry(pCD,
      ulDirCluster, NULL,
      &DirEntry, &DirStream, szFileName);
}

/******************************************************************
*
******************************************************************/
USHORT MakeDirEntry(PCDINFO pCD, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo,
                    PDIRENTRY pNew, PDIRENTRY1 pNewStream, PSZ pszName)
{
#ifdef EXFAT
   if (pCD->bFatType < FAT_TYPE_EXFAT)
      {
#endif
      set_datetime(pNew);
#ifdef EXFAT
      }
   else
      {
      set_datetime1((PDIRENTRY1)pNew);
      }
#endif

   return ModifyDirectory(pCD, ulDirCluster, pDirSHInfo, MODIFY_DIR_INSERT, 
      NULL, pNew, NULL, pNewStream, pszName, NULL);
}

/******************************************************************
*
******************************************************************/
BOOL DeleteFatChain(PCDINFO pCD, ULONG ulCluster)
{
   ULONG ulNextCluster;
   ULONG ulSector;
   ULONG ulBmpSector = 0;
   ULONG ulClustersFreed;
   APIRET rc;

   if (!ulCluster)
      return TRUE;

   ulSector = GetFatEntrySec(pCD, ulCluster);
   ReadFatSector(pCD, ulSector);

#ifdef EXFAT
   if (pCD->bFatType == FAT_TYPE_EXFAT)
      {
      ulBmpSector = GetAllocBitmapSec(pCD, ulCluster);
      ReadBmpSector(pCD, ulSector);
      }
#endif

   ulClustersFreed = 0;
   while (!(ulCluster >= pCD->ulFatEof2 && ulCluster <= pCD->ulFatEof))
      {
#ifdef CALL_YIELD
      //Yield();
#endif

      if (!ulCluster || ulCluster == pCD->ulFatBad)
         {
         break;
         }
      ulSector = GetFatEntrySec(pCD, ulCluster);
      if (ulSector != pCD->ulCurFATSector)
         {
         rc = WriteFatSector(pCD, pCD->ulCurFATSector);
         if (rc)
            return FALSE;
         ReadFatSector(pCD, ulSector);
         }
      ulNextCluster = GetFatEntry(pCD, ulCluster);

      SetFatEntry(pCD, ulCluster, 0L);

#ifdef EXFAT
      if (pCD->bFatType == FAT_TYPE_EXFAT)
         {
         // modify exFAT allocation bitmap as well
         ulBmpSector = GetAllocBitmapSec(pCD, ulCluster);
         if (ulBmpSector != pCD->ulCurBmpSector)
            {
            rc = WriteBmpSector(pCD, pCD->ulCurBmpSector);
            if (rc)
               {
               return FALSE;
               }
            ReadBmpSector(pCD, ulBmpSector);
            }
         SetBmpEntry(pCD, ulCluster, 0);
         }
#endif

      ulClustersFreed++;
      ulCluster = ulNextCluster;
      }
   rc = WriteFatSector(pCD, pCD->ulCurFATSector);
   if (rc)
      return FALSE;

#ifdef EXFAT
   if (pCD->bFatType == FAT_TYPE_EXFAT)
      {
      rc = WriteBmpSector(pCD, pCD->ulCurBmpSector);

      if (rc)
         {
         return FALSE;
         }
      }
#endif

   pCD->FSInfo.ulFreeClusters += ulClustersFreed;
   UpdateFSInfo(pCD);

   return TRUE;
}


/******************************************************************
*
******************************************************************/
BOOL UpdateFSInfo(PCDINFO pCD)
{
   PBYTE bSector = malloc(pCD->BootSect.bpb.BytesPerSector);

   if (pCD->BootSect.bpb.FSinfoSec == 0xFFFF)
      {
      free(bSector);
      return TRUE;
      }

   // no FSInfo sector on FAT12/FAT16
   if (pCD->bFatType != FAT_TYPE_FAT32)
      {
      free(bSector);
      return TRUE;
      }

   if (!ReadSector(pCD, pCD->BootSect.bpb.FSinfoSec, 1, bSector))
      {
      memcpy(bSector + FSINFO_OFFSET, (void *)&pCD->FSInfo, sizeof (BOOTFSINFO));
      if (!WriteSector(pCD, pCD->BootSect.bpb.FSinfoSec, 1, bSector))
         {
         free(bSector);
         return TRUE;
         }
      }

   free(bSector);
   return FALSE;
}

/******************************************************************
*
******************************************************************/
ULONG MakeFatChain(PCDINFO pCD, PSHOPENINFO pSHInfo, ULONG ulPrevCluster, ULONG ulClustersRequested, PULONG pulLast)
{
ULONG  ulCluster = 0;
ULONG  ulFirstCluster = 0;
ULONG  ulStartCluster = 0;
ULONG  ulLargestChain;
ULONG  ulLargestSize;
ULONG  ulReturn;
ULONG  ulSector = 0;
ULONG  ulNextCluster = 0;
ULONG  ulBmpSector = 0;
BOOL   fStartAt2;
BOOL   fContiguous;
#ifdef EXFAT
BOOL   fStatus;
#endif

   if (!ulClustersRequested)
      return pCD->ulFatEof;

   if (pCD->FSInfo.ulFreeClusters < ulClustersRequested)
      return pCD->ulFatEof;

   ulReturn = pCD->ulFatEof;
   fContiguous = TRUE;
   for (;;)
      {
      ulLargestChain = pCD->ulFatEof;
      ulLargestSize = 0;

      //ulFirstCluster = pCD->FSInfo.ulNextFreeCluster + 1;
      ulFirstCluster = pCD->FSInfo.ulNextFreeCluster;
      if (ulFirstCluster < 2 || ulFirstCluster >= pCD->ulTotalClusters + 2)
         {
         fStartAt2 = TRUE;
         ulFirstCluster = 2;
         ulStartCluster = pCD->ulTotalClusters + 3;
         }
      else
         {
         ulStartCluster = ulFirstCluster;
         fStartAt2 = FALSE;
         }

      for (;;)
         {
#ifdef CALL_YIELD
         //Yield();
#endif
         /*
            Find first free cluster
         */
#ifdef EXFAT
         if (pCD->bFatType < FAT_TYPE_EXFAT)
            {
#endif
            while (ulFirstCluster < pCD->ulTotalClusters + 2)
               {
               ulSector = GetFatEntrySec(pCD, ulFirstCluster);
               ulNextCluster = GetFatEntry(pCD, ulFirstCluster);
               if (ulSector != pCD->ulCurFATSector)
                  ReadFatSector(pCD, ulSector);
               if (!ulNextCluster)
                  break;
               ulFirstCluster++;
               }
#ifdef EXFAT
            }
         else
            {
            while (ulFirstCluster < pCD->ulTotalClusters + 2)
               {
               ulBmpSector = GetAllocBitmapSec(pCD, ulFirstCluster);
               fStatus = GetBmpEntry(pCD, ulFirstCluster);

               if (ulBmpSector != pCD->ulCurBmpSector)
                  ReadBmpSector(pCD, ulBmpSector);

               if (!fStatus)
                  break;
               ulFirstCluster++;
               }
            }
#endif

         if (fStartAt2 && ulFirstCluster >= ulStartCluster)
            break;

         if (ulFirstCluster >= pCD->ulTotalClusters + 2)
            {
            if (fStartAt2)
               break;
            ulFirstCluster = 2;
            fStartAt2 = TRUE;
            continue;
            }


         /*
            Check if chain is long enough
         */

#ifdef EXFAT
         if (pCD->bFatType < FAT_TYPE_EXFAT)
            {
#endif
            for (ulCluster = ulFirstCluster ;
                     ulCluster < ulFirstCluster + ulClustersRequested &&
                     ulCluster < pCD->ulTotalClusters + 2;
                           ulCluster++)
               {
               ulSector = GetFatEntrySec(pCD, ulCluster);
               ulNextCluster = GetFatEntry(pCD, ulCluster);
               if (ulSector != pCD->ulCurFATSector)
                  ReadFatSector(pCD, ulSector);
               if (ulNextCluster)
                  break;
               }
#ifdef EXFAT
            }
         else
            {
            for (ulCluster = ulFirstCluster ;
                     ulCluster < ulFirstCluster + ulClustersRequested &&
                     ulCluster < pCD->ulTotalClusters + 2;
                           ulCluster++)
               {
               BOOL fStatus1;
               ulBmpSector = GetAllocBitmapSec(pCD, ulCluster);
               fStatus1 = GetBmpEntry(pCD, ulCluster);
               if (ulBmpSector != pCD->ulCurBmpSector)
                  ReadBmpSector(pCD, ulBmpSector);
               if (fStatus1)
                  break;
               }
            }
#endif

         if (ulCluster != ulFirstCluster + ulClustersRequested)
            {
            /*
               Keep the largests chain found
            */
            if (ulCluster - ulFirstCluster > ulLargestSize)
               {
               ulLargestChain = ulFirstCluster;
               ulLargestSize  = ulCluster - ulFirstCluster;
               }
            ulFirstCluster = ulCluster;
            continue;
            }

         /*
            Chain found long enough
         */
         if (ulReturn == pCD->ulFatEof)
            ulReturn = ulFirstCluster;

         if (MakeChain(pCD, pSHInfo, ulFirstCluster, ulClustersRequested)) //// here
            goto MakeFatChain_Error;

         if (ulPrevCluster != pCD->ulFatEof)
            {
            if (SetNextCluster(pCD, ulPrevCluster, ulFirstCluster) == pCD->ulFatEof)
               goto MakeFatChain_Error;
            }

         if (pulLast)
            *pulLast = ulFirstCluster + ulClustersRequested - 1;
         return ulReturn;
         }

      /*
         We get here only if no free chain long enough was found!
      */
      fContiguous = FALSE;

      if (ulLargestChain != pCD->ulFatEof)
         {
         ulFirstCluster = ulLargestChain;
         if (ulReturn == pCD->ulFatEof)
            ulReturn = ulFirstCluster;

         if (MakeChain(pCD, pSHInfo, ulFirstCluster, ulLargestSize))
            goto MakeFatChain_Error;

         if (ulPrevCluster != pCD->ulFatEof)
            {
            if (SetNextCluster(pCD, ulPrevCluster, ulFirstCluster) == pCD->ulFatEof)
               goto MakeFatChain_Error;
            }

         ulPrevCluster        = ulFirstCluster + ulLargestSize - 1;
         ulClustersRequested -= ulLargestSize;
         }
      else
         break;
      }

MakeFatChain_Error:

   if (ulReturn != pCD->ulFatEof)
      DeleteFatChain(pCD, ulReturn);

   return pCD->ulFatEof;
}

/******************************************************************
*
******************************************************************/
USHORT MakeChain(PCDINFO pCD, PSHOPENINFO pSHInfo, ULONG ulFirstCluster, ULONG ulSize)
{
ULONG ulSector = 0;
ULONG ulBmpSector = 0;
ULONG ulLastCluster = 0;
ULONG ulNextCluster = 0;
ULONG  ulCluster = 0;
#ifdef EXFAT
BOOL fStatus;
#endif
USHORT rc;

   ulLastCluster = ulFirstCluster + ulSize - 1;

   ulSector = GetFatEntrySec(pCD, ulFirstCluster);
   if (ulSector != pCD->ulCurFATSector)
      ReadFatSector(pCD, ulSector);

#ifdef EXFAT
   if (pCD->bFatType == FAT_TYPE_EXFAT)
      {
      ulBmpSector = GetAllocBitmapSec(pCD, ulFirstCluster);
      if (ulBmpSector != pCD->ulCurBmpSector)
         ReadBmpSector(pCD, ulBmpSector);
      }
#endif

   for (ulCluster = ulFirstCluster; ulCluster < ulLastCluster; ulCluster++)
      {
      if (! pSHInfo || ! pSHInfo->fNoFatChain)
         {
         ulSector = GetFatEntrySec(pCD, ulCluster);
         if (ulSector != pCD->ulCurFATSector)
            {
            rc = WriteFatSector(pCD, pCD->ulCurFATSector);
            if (rc)
               return rc;
            ReadFatSector(pCD, ulSector);
            }
         ulNextCluster = GetFatEntry(pCD, ulCluster);
#ifdef EXFAT
         if (ulNextCluster && pCD->bFatType < FAT_TYPE_EXFAT)
#else
         if (ulNextCluster)
#endif
            return ERROR_SECTOR_NOT_FOUND;
         SetFatEntry(pCD, ulCluster, ulCluster + 1);
         }
#ifdef EXFAT
      if (pCD->bFatType == FAT_TYPE_EXFAT)
         {
         ulBmpSector = GetAllocBitmapSec(pCD, ulCluster);
         if (ulBmpSector != pCD->ulCurBmpSector)
            {
            rc = WriteBmpSector(pCD, pCD->ulCurBmpSector);
            if (rc)
               return rc;
            ReadBmpSector(pCD, ulBmpSector);
            }
         fStatus = GetBmpEntry(pCD, ulCluster);
         if (fStatus)
            return ERROR_SECTOR_NOT_FOUND;
         SetBmpEntry(pCD, ulCluster, 1);
         }
#endif
      }

#ifdef EXFAT
   if (! pSHInfo || ! pSHInfo->fNoFatChain)
#endif
      {
      ulSector = GetFatEntrySec(pCD, ulCluster);
      if (ulSector != pCD->ulCurFATSector)
         {
         rc = WriteFatSector(pCD, pCD->ulCurFATSector);
         if (rc)
            return rc;
         ReadFatSector(pCD, ulSector);
         }
      ulNextCluster = GetFatEntry(pCD, ulCluster);
      if (ulNextCluster && pCD->bFatType < FAT_TYPE_EXFAT)
         return ERROR_SECTOR_NOT_FOUND;

      SetFatEntry(pCD, ulCluster, pCD->ulFatEof);
      rc = WriteFatSector(pCD, pCD->ulCurFATSector);
      if (rc)
         return rc;
      }

#ifdef EXFAT
   if (pCD->bFatType == FAT_TYPE_EXFAT)
      {
      ulBmpSector = GetAllocBitmapSec(pCD, ulCluster);
      if (ulBmpSector != pCD->ulCurBmpSector)
         {
         rc = WriteBmpSector(pCD, pCD->ulCurBmpSector);
         if (rc)
            return rc;
         ReadBmpSector(pCD, ulBmpSector);
         }
      fStatus = GetBmpEntry(pCD, ulCluster);
      if (fStatus)
         return ERROR_SECTOR_NOT_FOUND;

      SetBmpEntry(pCD, ulCluster, 1);
      rc = WriteBmpSector(pCD, pCD->ulCurBmpSector);
      if (rc)
         return rc;
      }
#endif

   //pCD->FSInfo.ulNextFreeCluster = ulCluster;
   pCD->FSInfo.ulNextFreeCluster = ulCluster + 1;
   pCD->FSInfo.ulFreeClusters   -= ulSize;

   return 0;
}

/******************************************************************
*
******************************************************************/
APIRET MakeFile(PCDINFO pCD, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo, PSZ pszOldFile, PSZ pszFile, PBYTE pBuf, ULONG cbBuf)
{
   ULONG ulClustersNeeded;
   ULONG ulCluster, ulOldCluster;
   DIRENTRY OldOldEntry, OldNewEntry, OldEntry, NewEntry;
   DIRENTRY1 OldOldEntryStream, OldNewEntryStream, OldEntryStream, NewEntryStream;
   char pszOldFileName[256] = {0};
   char pszFileName[256] = {0};
   APIRET rc;
   char file_exists = 0;
   int i;

   if (cbBuf)
      {
      ulClustersNeeded = cbBuf / pCD->ulClusterSize +
         (cbBuf % pCD->ulClusterSize ? 1 : 0);

      if (pszOldFile)
         {
         strcpy(pszOldFileName, pszOldFile);

         ulOldCluster = FindPathCluster(pCD, ulDirCluster, pszOldFileName, pDirSHInfo, &OldOldEntry, &OldOldEntryStream, NULL);

         if (ulOldCluster != pCD->ulFatEof)
            {
            DeleteFatChain(pCD, ulOldCluster);
            ModifyDirectory(pCD, ulDirCluster, pDirSHInfo, MODIFY_DIR_DELETE, &OldOldEntry, NULL, &OldOldEntryStream, NULL, pszOldFile, NULL);
            }
         }

      if (pszFile)
         strcpy(pszFileName, pszFile);

      ulCluster = FindPathCluster(pCD, ulDirCluster, pszFileName, pDirSHInfo, &OldEntry, &OldEntryStream, NULL);

      if (ulCluster != pCD->ulFatEof)
         {
         file_exists = 1;
         memcpy(&NewEntry, &OldEntry, sizeof(DIRENTRY));
         if (!pszOldFile)
            DeleteFatChain(pCD, ulCluster);
         else
            {
            memcpy(&OldNewEntry, &OldEntry, sizeof(DIRENTRY));
            memcpy(&OldNewEntryStream, &OldEntryStream, sizeof(DIRENTRY));
            // rename chkdsk.log to chkdsk.old
            rc = ModifyDirectory(pCD, ulDirCluster, pDirSHInfo, MODIFY_DIR_RENAME,
               &OldEntry, &OldNewEntry, &OldEntryStream, &OldNewEntryStream, pszFile, pszOldFileName);
            if (rc)
               goto MakeFileEnd;
            }
         }
      else
         {
         memset(&NewEntry, 0, sizeof(DIRENTRY));
         memset(&NewEntryStream, 0, sizeof(DIRENTRY1));
         }

      ulCluster = MakeFatChain(pCD, NULL, pCD->ulFatEof, ulClustersNeeded, NULL);

      if (ulCluster != pCD->ulFatEof)
         {
#ifdef EXFAT
            if (pCD->bFatType < FAT_TYPE_EXFAT)
               {
#endif
               NewEntry.wCluster = LOUSHORT(ulCluster);
               NewEntry.wClusterHigh = HIUSHORT(ulCluster);
               FileSetSize(pCD, &NewEntry, ulDirCluster, pDirSHInfo, pszFile, cbBuf);
#ifdef EXFAT
               }
            else
               {
               PDIRENTRY1 pNewEntryStream = (PDIRENTRY1)&NewEntryStream;
               pNewEntryStream->u.Stream.ulFirstClus = ulCluster;
               pNewEntryStream->u.Stream.ullValidDataLen = cbBuf;
               pNewEntryStream->u.Stream.ullDataLen = cbBuf;
               //   (cbBuf / pCD->ulClusterSize) * pCD->ulClusterSize +
               //   ((cbBuf % pCD->ulClusterSize) ? pCD->ulClusterSize : 0);
               }
#endif
         }
      else
         {
            rc = ERROR_DISK_FULL;
            goto MakeFileEnd;
         }
      }

   if (! file_exists || pszOldFile)
      rc = MakeDirEntry(pCD, ulDirCluster, pDirSHInfo, &NewEntry, &NewEntryStream, pszFileName);
   else
      rc = ModifyDirectory(pCD, ulDirCluster, NULL, MODIFY_DIR_UPDATE,
              &OldEntry, &NewEntry, &OldEntryStream, &NewEntryStream, pszFileName, NULL);

   if (rc)
      goto MakeFileEnd;

   for (i = 0; i < ulClustersNeeded; i++)
      {
      rc = WriteCluster(pCD, ulCluster, pBuf);

      if (rc)
         goto MakeFileEnd;

      pBuf += pCD->ulClusterSize;
      ulCluster = GetNextCluster(pCD, NULL, ulCluster, TRUE);
      }

   UpdateFSInfo(pCD);

MakeFileEnd:
   return rc;
}

APIRET DelFile(PCDINFO pCD, PSZ pszFilename)
{
PSZ pszFile;
DIRENTRY DirEntry;
PDIRENTRY1 pDirEntry1;
DIRENTRY1 DirEntryStream;
DIRENTRY1 DirStream;
SHOPENINFO DirSHInfo;
PSHOPENINFO pDirSHInfo = NULL;
ULONG ulDirCluster;
ULONG ulCluster;
APIRET rc;

   ulDirCluster = FindDirCluster(pCD,
      pszFilename,
      0xffff,
      RETURN_PARENT_DIR,
      &pszFile,
      &DirStream);

   if (ulDirCluster == pCD->ulFatEof)
      {
      rc = ERROR_PATH_NOT_FOUND;
      goto DeleteFileExit;
      }

#ifdef EXFAT
   if (pCD->bFatType == FAT_TYPE_EXFAT)
      {
      pDirSHInfo = &DirSHInfo;
      SetSHInfo1(pCD, &DirStream, pDirSHInfo);
      }
#endif

   ulCluster = FindPathCluster(pCD, ulDirCluster, pszFile, pDirSHInfo,
                               &DirEntry, &DirEntryStream, NULL);
   if (ulCluster == pCD->ulFatEof)
      {
      rc = ERROR_FILE_NOT_FOUND;
      goto DeleteFileExit;
      }

   pDirEntry1 = (PDIRENTRY1)&DirEntry;

#ifdef EXFAT
   if ( ((pCD->bFatType <  FAT_TYPE_EXFAT) && (DirEntry.bAttr & FILE_DIRECTORY)) ||
        ((pCD->bFatType == FAT_TYPE_EXFAT) && (pDirEntry1->u.File.usFileAttr & FILE_DIRECTORY)) )
#else
   if ( DirEntry.bAttr & FILE_DIRECTORY )
#endif
      {
      rc = ERROR_ACCESS_DENIED;
      goto DeleteFileExit;
      }

   rc = ModifyDirectory(pCD, ulDirCluster, pDirSHInfo, MODIFY_DIR_DELETE,
                        &DirEntry, NULL, &DirEntryStream, NULL, pszFilename, NULL);
   if (rc)
      goto DeleteFileExit;

   if (ulCluster)
      DeleteFatChain(pCD, ulCluster);
   rc = 0;

DeleteFileExit:
   return rc;
}

/************************************************************************
*
************************************************************************/
void FileSetSize(PCDINFO pCD, PDIRENTRY pDirEntry, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo, PSZ pszFile, ULONGLONG ullSize)
{
   pDirEntry->ulFileSize = ullSize & 0xffffffff;

   //if (f32Parms.fFatPlus)
      {
      ULONGLONG ullTmp;
      ullTmp = (ULONGLONG)(pDirEntry->fEAS);
      ullSize >>= 32;
      ullTmp |= (ullSize & FILE_SIZE_MASK);
      pDirEntry->fEAS = (BYTE)ullTmp;
      }

   if ( (ullSize >> 35) )
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
      *(PUSHORT)((PBYTE)((PFEA)pfealist->list + 1) + 13) = EAT_BINARY;  // EA type
      *(PUSHORT)((PBYTE)((PFEA)pfealist->list + 1) + 15) = 8;          // length
      *(PULONGLONG)((PBYTE)((PFEA)pfealist->list + 1) + 17) = ullSize; // value

      rc = usModifyEAS(pCD, ulDirCluster, pDirSHInfo, pszFile, &eaop);
      }
}


/************************************************************************
*
************************************************************************/
void FileGetSize(PCDINFO pCD, PDIRENTRY pDirEntry, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo, PSZ pszFile, PULONGLONG pullSize)
{
   *pullSize = pDirEntry->ulFileSize;

   //if (f32Parms.fFatPlus)
      {
      *pullSize |= (((ULONGLONG)(pDirEntry->fEAS) & FILE_SIZE_MASK) << 32);
      }

   if ( (pDirEntry->fEAS & FILE_SIZE_EA) )
      {
      // read EAs
      EAOP eaop;
      BYTE pBuf1[sizeof(GEALIST) + 12];
      PGEALIST pgealist = (PGEALIST)pBuf1;
      BYTE pBuf2[sizeof(FEALIST) + 13 + 4 + sizeof(ULONGLONG)];
      PFEALIST pfealist = (PFEALIST)pBuf2;
      APIRET rc;
      int i;

      memset(&eaop, 0, sizeof(eaop));
      memset(pgealist, 0, sizeof(pBuf1));
      memset(pfealist, 0, sizeof(pBuf2));
      eaop.fpGEAList = pgealist;
      eaop.fpFEAList = pfealist;
      pgealist->cbList = sizeof(GEALIST) + 12;
      pgealist->list[0].cbName = 12;
      strcpy(pgealist->list[0].szName, "FAT_PLUS_FSZ");
      pfealist->cbList = sizeof(pBuf2);

      rc = usGetEAS(pCD, FIL_QUERYEASFROMLISTL, ulDirCluster, pDirSHInfo, pszFile, &eaop);

      if (rc)
         return;

      *pullSize = *(PULONGLONG)((PBYTE)((PFEA)pfealist->list + 1) + 17);
      }
}
