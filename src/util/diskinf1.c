#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INCL_DOSDEVIOCTL
#define INCL_DOSDEVICES
#define INCL_DOS


#include <os2.h>
#include "portable.h"

#include "fat32.h"


BYTE szText[]="\
³ ³³³ ³\n\
³ ³³³ À Partition type (the number the specify after /P to get this\n\
³ ³³³                   partition type handled by PARTFILT)\n\
³ ³³ÀÄÄ H = Hidden partition\n\
³ ³ÀÄÄÄ A = Active / B = Bootable via bootmanager\n\
³ ÀÄÄÄÄ P = Primary / L = Logical (extended)\n\
ÀÄÄÄÄÄÄ Seq # to be used in the OPTIONAL /M argument for PARTFILT.\n\n";



static APIRET ReadSectors(HFILE hDisk, USHORT usHead, USHORT usCylinder, USHORT usSector, USHORT nSectors, PVOID pvData);
static APIRET ScanDisks(WORD wDiskNum);
static BOOL   GetDPMB(HFILE hDisk, PDEVICEPARAMETERBLOCK pDPMB);
static BOOL   GetMBR(HFILE hDisk, PMBR pMBR, USHORT usHead, USHORT usCylinder, USHORT usSector);
static VOID   vListMBR(HFILE hDisk, PMBR pMBR,  PDEVICEPARAMETERBLOCK pdpmb, ULONG ulFirstExtOffset, ULONG ulStartOffset, WORD wDiskNum);
static APIRET PrepareDrive(PDRIVEINFO pDrive);
VOID vDumpSector(PBYTE pbSector);

UCHAR GetFatType(PBOOTSECT pSect);
ULONG GetFatEntrySec(PDRIVEINFO pDrive, ULONG ulCluster);
ULONG GetFatEntryBlock(PDRIVEINFO pDrive, ULONG ulCluster, USHORT usBlockSize);
ULONG GetFatEntryEx(PDRIVEINFO pDrive, PBYTE pFatStart, ULONG ulCluster, USHORT usBlockSize);
ULONG GetFatEntry(PDRIVEINFO pDrive, ULONG ulCluster);

/****************************************************************
*
****************************************************************/
APIRET InitProg(void)
{
APIRET rc;
WORD   wDiskCount;
WORD   wDisk;
USHORT usDrive;

   rc = DosPhysicalDisk(INFO_COUNT_PARTITIONABLE_DISKS,
      &wDiskCount,
      sizeof wDiskCount,
      NULL,
      0);

   if (rc)
      {
      printf("DosPhysicalDisk returned %ld\n", rc);
      return rc;
      }
   printf("There are %d disks\n", wDiskCount);

   for (wDisk = 1; wDisk <= wDiskCount; wDisk++)
      ScanDisks(wDisk);

   printf(szText);

   printf("%d FAT32 partitions found!\n", usDriveCount);
   if (!memcmp(rgfFakePart, rgfFakeComp, 256))
      {
      printf("WARNING: /P xx not specified.\n");
      printf("         Only 'normal' partitions are assigned a partition sequence number!\n");
      }

   if (usDriveCount > 0 && fDetailed > 1)
      {
      for (usDrive = 0; usDrive < usDriveCount; usDrive++)
         {
         printf("\n");
         if (PrepareDrive(&rgDrives[usDrive]))
            return rc;
         }
      }

   return 0;
}



/****************************************************************
*
****************************************************************/
APIRET ScanDisks(WORD wDiskNum)
{
DEVICEPARAMETERBLOCK dpmb;
APIRET rc;
HFILE  hDisk;
MBR    mbr;

   printf("=== Scanning physical disk %d.===\n", wDiskNum);

   rc  = OpenDisk(wDiskNum, &hDisk);
   if (rc)
      return rc;

   if (GetDPMB(hDisk, &dpmb))
      {
      if (fDetailed > 1)
         printf("There are %d cylinders, %d heads and %d sectors/track\n",
            dpmb.cCylinders,
            dpmb.cHeads,
            dpmb.cSectorsPerTrack);
      }

   if (GetMBR(hDisk, &mbr, 0, 0, 1))
      vListMBR(hDisk, &mbr, &dpmb, 0L, 0L, wDiskNum);

   CloseDisk(hDisk);

   return 0;
}

APIRET OpenDisk(WORD wDiskNum, PHFILE phFile)
{
APIRET rc;
BYTE   szDisk[5];
WORD   wHandle;

   sprintf(szDisk, "%d:", wDiskNum);
   rc = DosPhysicalDisk(INFO_GETIOCTLHANDLE,
      &wHandle,
      sizeof wHandle,
      szDisk,
      strlen(szDisk)+1);
   if (rc)
      {
      printf("Unable to obtain an IOCTL handle, SYS%4.4u!\n", rc);
      return rc;
      }
   *phFile = wHandle;
   return 0;
}

APIRET CloseDisk(HFILE hFile)
{
APIRET rc;
WORD   wHandle = hFile;

   rc = DosPhysicalDisk(INFO_FREEIOCTLHANDLE,
      NULL,
      0,
      &wHandle,
      sizeof wHandle);
   if (rc)
      {
      printf("Unable to close an IOCTL handle, rc = %d!\n", rc);
      return rc;
      }
   return 0;
}
/****************************************************************
*
****************************************************************/
VOID vListMBR(HFILE hDisk, PMBR pMBR,  PDEVICEPARAMETERBLOCK pdpmb, ULONG ulFirstExtOffset, ULONG ulStartOffset, WORD wDiskNum)
{
int  iIndex;
PSZ  pszPartType;
BYTE szUnknown[50];
PPARTITIONENTRY pPart;
static iCount = 0;
static iLevel = 0;
BOOL fKnown;
BOOL fExtended;
BYTE bActive;
BYTE bType;
ULONG ulFirstSector, ulLastSector, ulSector;
USHORT usSector, usHead, usCylinder;

      iLevel++;
      for (iIndex = 0 ; iIndex < 4 ; iIndex++)
         {
         pPart = &pMBR->partition[iIndex];
         fKnown = FALSE;
         fExtended = FALSE;
         if (rgfFakePart[pPart->cSystem])
            fKnown = TRUE;

         if (pPart->cSystem == PARTITION_EXTENDED ||
            pPart->cSystem == PARTITION_XINT13_EXTENDED)
            {
            ulFirstSector = ulFirstExtOffset + pPart->ulRelSectorNr;
            ulLastSector  = ulFirstExtOffset + pPart->ulRelSectorNr + pPart->ulTotalSectors - 1;
            }
         else
            {
            ulFirstSector = ulStartOffset + pPart->ulRelSectorNr;
            ulLastSector  = ulStartOffset + pPart->ulRelSectorNr + pPart->ulTotalSectors - 1;
            }

         switch (pPart->cSystem & ~PARTITION_HIDDEN)
            {
            case PARTITION_ENTRY_UNUSED   :
               pszPartType = NULL;
               break;
            case PARTITION_FAT_12         :
               fKnown = TRUE;
               pszPartType = "FAT12";
               break;
            case PARTITION_XENIX_1        :
               pszPartType = "XENIX_1";
               break;
            case PARTITION_XENIX_2        :
               pszPartType = "XENIX_2";
               break;
            case PARTITION_FAT_16         :
               fKnown = TRUE;
               pszPartType = "FAT16";
               break;
            case PARTITION_EXTENDED       :
               fExtended = TRUE;
               if (fDetailed)
                  pszPartType = "EXTENDED (05)";
               else
                  pszPartType = NULL;
               break;
            case PARTITION_HUGE           :
               fKnown = TRUE;
               pszPartType = "HUGE";
               break;
            case PARTITION_IFS            :
               fKnown = TRUE;
               pszPartType = "IFS";
               break;
            case PARTITION_BOOTMANAGER    :
               pszPartType = "BOOTMANAGER";
               break;
            case PARTITION_FAT32          :
            case PARTITION_FAT32_XINT13   :
               if ((pPart->cSystem & ~PARTITION_HIDDEN) == PARTITION_FAT32)
                  pszPartType = "FAT32";
               else
                  pszPartType = "FAT32 (int13)";

               rgDrives[usDriveCount].bPartType = pPart->cSystem;
               rgDrives[usDriveCount].DiskNum = wDiskNum;
               rgDrives[usDriveCount].nHeads = pdpmb->cHeads;
               rgDrives[usDriveCount].nCylinders = pdpmb->cCylinders;
               rgDrives[usDriveCount].nSectorsPerTrack = pdpmb->cSectorsPerTrack;

               rgDrives[usDriveCount].ulRelStartSector = ulFirstSector;
               rgDrives[usDriveCount].ulRelEndSector   = ulLastSector;

               ulSector = ulFirstSector;
               usSector = ulSector % pdpmb->cSectorsPerTrack;
               ulSector = (ulSector - usSector) / pdpmb->cSectorsPerTrack;
               usHead = ulSector % pdpmb->cHeads;
               usCylinder = (ulSector - usHead) / pdpmb->cHeads;

               rgDrives[usDriveCount].StartHead = usHead;
               rgDrives[usDriveCount].StartCylinder = usCylinder;
               rgDrives[usDriveCount].StartSector = usSector + 1;

               ulSector = ulLastSector;
               usSector = ulSector % pdpmb->cSectorsPerTrack;
               ulSector = (ulSector - usSector) / pdpmb->cSectorsPerTrack;
               usHead = ulSector % pdpmb->cHeads;
               usCylinder = (ulSector - usHead) / pdpmb->cHeads;

               rgDrives[usDriveCount].EndHead = usHead;
               rgDrives[usDriveCount].EndCylinder = usCylinder;
               rgDrives[usDriveCount].EndSector = usSector + 1;

               usDriveCount++;
               break;

            case PARTITION_XINT13         :
               pszPartType = "XINT13";
               break;
            case PARTITION_XINT13_EXTENDED:
               fExtended = TRUE;
               if (fDetailed)
                  pszPartType = "EXTENDED (0F)";
               else
                  pszPartType = NULL;
               break;
            case PARTITION_PREP           :
               pszPartType = "PREP";
               break;
            case PARTITION_UNIX           :
               pszPartType = "UNIX";
               break;
            case VALID_NTFT               :
               pszPartType = "NTFT";
               break;
            case PARTITION_LINUX:
               pszPartType = "LINUX";
               break;
            default:
               sprintf(szUnknown, "UNKNOWN %X", pPart->cSystem);
               pszPartType = szUnknown;
               break;
            }

         if (pszPartType)
            {
//            if (fDetailed)
//               printf("%*.*s", iLevel-1, iLevel-1, "               ");
            if (!fExtended)
               {
               if (fKnown)
                  {
                  printf("%d:", iCount);
                  iCount++;
                  }
               else
                  printf("%c:", '-');
               }
            else
               printf(" :");

            bActive = ' ';
            if (iLevel == 1 && (pPart->iBoot & 0x80))
               bActive = 'A';
            else if (pPart->iBoot & 0x80)
               bActive = 'B';
            if (!fExtended)
               {
               if (iLevel == 1)
                  bType = 'P';
               else
                  bType = 'L';
               }
            else
               bType = 'E';

            printf("%c%c%c %2.2X %-15.15s",
               bType,
               bActive,
               (pPart->cSystem & PARTITION_HIDDEN ? 'H' : ' '),
               pPart->cSystem,
               pszPartType);

            // calculate first Head, Cylinder and sector

            ulSector = ulFirstSector;
            usSector = ulSector % pdpmb->cSectorsPerTrack;
            ulSector = (ulSector - usSector) / pdpmb->cSectorsPerTrack;
            usHead = ulSector % pdpmb->cHeads;
            usCylinder = (ulSector - usHead) / pdpmb->cHeads;

            printf("Strt:H:%6d C:%4d S:%4d",
               usHead, usCylinder, usSector + 1);

            // calculate last Head, Cylinder and sector

            ulSector = ulLastSector;
            usSector = ulSector % pdpmb->cSectorsPerTrack;
            ulSector = (ulSector - usSector) / pdpmb->cSectorsPerTrack;
            usHead = ulSector % pdpmb->cHeads;
            usCylinder = (ulSector - usHead) / pdpmb->cHeads;

            printf(" End:H:%6d C:%4d S:%4d\n",
               usHead, usCylinder, usSector + 1);

#if 0
            if (pPart->cSystem == PARTITION_EXTENDED ||
               pPart->cSystem == PARTITION_XINT13_EXTENDED)
               printf("(%8lu - %8lu)\n",
                  ulFirstExtOffset + pPart->ulRelSectorNr, ulFirstExtOffset + pPart->ulRelSectorNr + pPart->ulTotalSectors - 1);
             else
               printf("(%8lu - %8lu)\n",
                  ulStartOffset + pPart->ulRelSectorNr, ulStartOffset + pPart->ulRelSectorNr + pPart->ulTotalSectors - 1);
#endif
            }

         }

      for (iIndex = 0 ; iIndex < 4 ; iIndex++)
         {
         pPart = &pMBR->partition[iIndex];
         if (pPart->cSystem == PARTITION_EXTENDED ||
             pPart->cSystem == PARTITION_XINT13_EXTENDED)
            {
            MBR mbr2;


            if (fDetailed)
               printf(" (extended partition)\n");

            ulFirstSector = ulFirstExtOffset + pPart->ulRelSectorNr;

            usSector = ulFirstSector % pdpmb->cSectorsPerTrack;
            ulFirstSector = (ulFirstSector - usSector) / pdpmb->cSectorsPerTrack;
            usHead = ulFirstSector % pdpmb->cHeads;
            usCylinder = (ulFirstSector - usHead) / pdpmb->cHeads;

//            printf("H:%u C:%u S:%u\n", usHead, usCylinder, usSector);

            if (GetMBR(hDisk, &mbr2,
               usHead, usCylinder, usSector + 1))
//               pPart->Start.Head,
//               pPart->Start.Cylinders256 * 256 + pPart->Start.Cylinders,
//               pPart->Start.Sectors))
               vListMBR(hDisk, &mbr2, pdpmb,
                  (iLevel == 1 ? pPart->ulRelSectorNr : ulFirstExtOffset),
                  ulFirstExtOffset + pPart->ulRelSectorNr,
                  wDiskNum);
            }
         }

      iLevel--;
}

/****************************************************************
*
****************************************************************/
BOOL GetDPMB(HFILE hDisk, PDEVICEPARAMETERBLOCK pDPMB)
{
BYTE   bParam;
ULONG  ulParmSize;
ULONG  ulDataSize;
APIRET rc;

   bParam = 0;
   ulParmSize = sizeof bParam;
   ulDataSize  = sizeof (DEVICEPARAMETERBLOCK);

   rc = DosDevIOCtl(hDisk,
      IOCTL_PHYSICALDISK,
      PDSK_GETPHYSDEVICEPARAMS,
      (PVOID)&bParam, ulParmSize, &ulParmSize,
      (PVOID)pDPMB, ulDataSize, &ulDataSize);

   if (rc)
      {
      printf("GetDPMB: DosDevIOCtl failed, SYS%4.4u\n", rc);
      exit(1);
      }
   return TRUE;
}



/**************************************************************
*
**************************************************************/
BOOL GetMBR(HFILE hDisk, PMBR pMBR, USHORT usHead, USHORT usCylinder, USHORT usSector)
{
static BYTE   bSector[SECTOR_SIZE];
APIRET        rc;

   rc = ReadSectors(hDisk, usHead, usCylinder, usSector, 1, bSector);
   if (rc)
      return FALSE;

   memcpy(pMBR, bSector + MBRTABLEOFFSET, sizeof (MBR));

   return TRUE;
}

/**************************************************************
*
**************************************************************/
APIRET ReadSectors(HFILE hDisk, USHORT usHead, USHORT usCylinder, USHORT usSector, USHORT nSectors, PVOID pvData)
{
DEVICEPARAMETERBLOCK dpmb;
ULONG         ulParmSize;
ULONG         ulDataSize;
PTRACKLAYOUT  pTrackLayout;
USHORT        usIndex;
APIRET        rc;

   GetDPMB(hDisk, &dpmb);

//   printf("Reading H:%d C:%d S:%d (%d sectors)\n",
//      usHead, usCylinder, usSector, nSectors);

   ulParmSize = sizeof (TRACKLAYOUT) + (dpmb.cSectorsPerTrack - 1) * 4;
   pTrackLayout = malloc(ulParmSize);
   if (!pTrackLayout)
      {
      printf("Not enough memory for Tracklayout!\n");
      return FALSE;
      }

   memset(pTrackLayout, 0, ulParmSize);

   pTrackLayout->bCommand      = (usSector == 1 ? 0x01 : 0x00);
   pTrackLayout->usHead        = usHead;
   pTrackLayout->usCylinder    = usCylinder;
   pTrackLayout->usFirstSector = usSector - 1;
   pTrackLayout->cSectors      = nSectors;

   for (usIndex = 0; usIndex < dpmb.cSectorsPerTrack; usIndex ++)
      {
      pTrackLayout->TrackTable[usIndex].usSectorNumber = usIndex + 1;
      pTrackLayout->TrackTable[usIndex].usSectorSize = SECTOR_SIZE;
      }

   ulDataSize = SECTOR_SIZE * nSectors;

   rc = DosDevIOCtl(hDisk, IOCTL_PHYSICALDISK, PDSK_READPHYSTRACK,
      (PVOID)pTrackLayout, ulParmSize, &ulParmSize,
      pvData, ulDataSize, &ulDataSize);

   if (rc)
      printf("ReadSectors: DosDevIOCtl failed, SYS%4.4u\n", rc);

   free(pTrackLayout);

   return rc;

}

APIRET PrepareDrive(PDRIVEINFO pDrive)
{
APIRET rc;
BYTE bSector[SECTOR_SIZE];
BOOTSECT  bSect;
USHORT    usIndex;
ULONG     ulBytes;
USHORT    usBlocks;


   pDrive->cTrackSize = sizeof (TRACKLAYOUT) +
      (pDrive->nSectorsPerTrack - 1) * 4;

   pDrive->pTrack = malloc(pDrive->cTrackSize);
   if (!pDrive->pTrack)
      return 8;

   memset(pDrive->pTrack, 0, pDrive->cTrackSize);
   for (usIndex = 0; usIndex < pDrive->nSectorsPerTrack; usIndex ++)
      {
      pDrive->pTrack->TrackTable[usIndex].usSectorNumber = usIndex + 1;
      pDrive->pTrack->TrackTable[usIndex].usSectorSize = SECTOR_SIZE;
      }

   printf("Reading BOOT Sector\n");

   rc = OpenDisk(pDrive->DiskNum, &pDrive->hDisk);
   if (rc)
      return rc;

   rc = ReadSectors(pDrive->hDisk,
      pDrive->StartHead,
      pDrive->StartCylinder,
      pDrive->StartSector,
      1, bSector);

   if (rc)
      {
      CloseDisk(pDrive->hDisk);
      pDrive->hDisk = 0;
      return rc;
      }

   memcpy(&bSect, bSector, sizeof bSect);


   memcpy(&pDrive->bpb, &bSect.bpb, sizeof pDrive->bpb);

   pDrive->bFatType = GetFatType(&bSect);

   switch (pDrive->bFatType)
      {
      case FAT_TYPE_FAT12:
         pDrive->ulFatEof  = FAT12_EOF;
         pDrive->ulFatEof2 = FAT12_EOF2;
         pDrive->ulFatBad  = FAT12_BAD_CLUSTER;
         break;

      case FAT_TYPE_FAT16:
         pDrive->ulFatEof  = FAT16_EOF;
         pDrive->ulFatEof2 = FAT16_EOF2;
         pDrive->ulFatBad  = FAT16_BAD_CLUSTER;
         break;

      case FAT_TYPE_FAT32:
         pDrive->ulFatEof  = FAT32_EOF;
         pDrive->ulFatEof2 = FAT32_EOF2;
         pDrive->ulFatBad  = FAT32_BAD_CLUSTER;
         break;

#ifdef EXFAT
      case FAT_TYPE_EXFAT:
         pDrive->ulFatEof  = EXFAT_EOF;
         pDrive->ulFatEof2 = EXFAT_EOF2;
         pDrive->ulFatBad  = EXFAT_BAD_CLUSTER;
#endif
      }

   if (pDrive->bFatType < FAT_TYPE_FAT32)
      {
      // create FAT32-type extended BPB for FAT12/FAT16
      if (!((PBOOTSECT0)&bSect)->bpb.TotalSectors)
         pDrive->bpb.BigTotalSectors = ((PBOOTSECT0)&bSect)->bpb.BigTotalSectors;
      else
         pDrive->bpb.BigTotalSectors = ((PBOOTSECT0)&bSect)->bpb.TotalSectors;

      pDrive->bpb.BigSectorsPerFat = ((PBOOTSECT0)&bSect)->bpb.SectorsPerFat;
      pDrive->bpb.ExtFlags |= 0x0080;
      pDrive->bpb.RootDirStrtClus = 2;
      }

   pDrive->ulStartOfFAT = pDrive->bpb.ReservedSectors;

   pDrive->ulCurFATSector = -1L;
   pDrive->ulCurCluster = pDrive->ulFatEof;
   pDrive->ulClusterSize = pDrive->bpb.SectorsPerCluster * pDrive->bpb.BytesPerSector;
   if (pDrive->bFatType == FAT_TYPE_FAT32)
      {
      pDrive->ulStartOfData = pDrive->bpb.ReservedSectors +
         pDrive->bpb.BigSectorsPerFat * pDrive->bpb.NumberOfFATs;
      }
   else if (pDrive->bFatType < FAT_TYPE_FAT32)
      {
      pDrive->ulStartOfData = pDrive->bpb.ReservedSectors +
         pDrive->bpb.SectorsPerFat * pDrive->bpb.NumberOfFATs +
         (pDrive->bpb.RootDirEntries * sizeof(DIRENTRY)) / pDrive->bpb.BytesPerSector;
      }
   pDrive->ulTotalClusters = (pDrive->bpb.BigTotalSectors - pDrive->ulStartOfData) / pDrive->bpb.SectorsPerCluster;

   pDrive->pbCluster = malloc(pDrive->bpb.SectorsPerCluster * pDrive->bpb.BytesPerSector);
   if (!pDrive->pbCluster)
      return FALSE;

   ulBytes = pDrive->ulTotalClusters / 8 +
      (pDrive->ulTotalClusters % 8 ? 1:0);
   usBlocks = (USHORT)(ulBytes / 4096 +
             (ulBytes % 4096 ? 1:0));

   pDrive->pFatBits = calloc(usBlocks,4096);
   if (!pDrive->pFatBits)
      printf("Not enough memory for FATBITS\n");

   if (fDetailed > 1)
      {
      printf("BytesPerSector    : %u\n", bSect.bpb.BytesPerSector);
      printf("SectorsPerCluster : %u\n", bSect.bpb.SectorsPerCluster);
      printf("Reserved Sectors  : %u\n", bSect.bpb.ReservedSectors);
      printf("NumberOfFATs;     : %u\n", bSect.bpb.NumberOfFATs);
      printf("RootDirEntries;   : %u\n", bSect.bpb.RootDirEntries);
      printf("TotalSectors;     : %u\n", bSect.bpb.TotalSectors);
      printf("MediaDescriptor;  : %u\n", bSect.bpb.MediaDescriptor);
      printf("SectorsPerFat;    : %u\n", bSect.bpb.SectorsPerFat);
      printf("SectorsPerTrack;  : %u\n", bSect.bpb.SectorsPerTrack);
      printf("Heads;            : %u\n", bSect.bpb.Heads);
      printf("HiddenSectors;    : %lu\n",bSect.bpb.HiddenSectors);
      printf("BigTotalSectors;  : %lu\n",bSect.bpb.BigTotalSectors);
      printf("BigSectorsPerFat; : %lu\n",bSect.bpb.BigSectorsPerFat);
      printf("ExtFlags;         : %u\n", bSect.bpb.ExtFlags);
      printf("FS_Version;       : %u\n", bSect.bpb.FS_Version);
      printf("RootDirStrtClus;  : %lu\n",bSect.bpb.RootDirStrtClus);
      printf("FSinfoSec;        : %u\n", bSect.bpb.FSinfoSec);
      printf("BkUpBootSec;      : %u\n", bSect.bpb.BkUpBootSec);
      printf("bReserved[6];     : %-6.6s\n", bSect.bpb.bReserved);

      printf("Volume label      : '%-11.11s'\n", bSect.VolumeLabel);
      printf("FileSystem        : '%-8.8s'\n", bSect.FileSystem);
      printf("Total clusters    : %lu\n", pDrive->ulTotalClusters);

      if (fShowBootSector)
         vDumpSector((PBYTE)&bSect);
      }

   CloseDisk(pDrive->hDisk);
   pDrive->hDisk = 0;

   return 0;
}


VOID vDumpSector(PBYTE pbSector)
{
BYTE szText[17];
PSZ  p;
USHORT usIndex;

   memset(szText, 0, sizeof szText);

   for (usIndex = 0; usIndex < 512; usIndex++)
      {
      if (!(usIndex % 16))
         {
         printf("  %s\n", szText);
         memset(szText, 0, sizeof szText);
         p = szText;
         }
      printf("%2.2X ", pbSector[usIndex]);
      if (pbSector[usIndex] >= ' ')
         *p++ = pbSector[usIndex];
      else
         *p++ = '.';
      }
   printf("  %s\n", szText);

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

   if (!pSect)
      {
      return FAT_TYPE_NONE;
      } /* endif */

   pbpb = &pSect->bpb;

   if (!pbpb->BytesPerSector)
      {
      return FAT_TYPE_NONE;
      }

   if (pbpb->BytesPerSector != SECTOR_SIZE)
      {
      return FAT_TYPE_NONE;
      }

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
ULONG GetFatEntrySec(PDRIVEINFO pDrive, ULONG ulCluster)
{
   // in three sector blocks
   return GetFatEntryBlock(pDrive, ulCluster, 512 * 3);
}

/******************************************************************
*
******************************************************************/
ULONG GetFatEntryBlock(PDRIVEINFO pDrive, ULONG ulCluster, USHORT usBlockSize)
{
ULONG  ulSector;

   ulCluster &= pDrive->ulFatEof;

   switch (pDrive->bFatType)
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
ULONG GetFatEntry(PDRIVEINFO pDrive, ULONG ulCluster)
{
   // in three sector blocks
   return GetFatEntryEx(pDrive, pDrive->pbFATSector[0], ulCluster, 512 * 3);
}

/******************************************************************
*
******************************************************************/
ULONG GetFatEntryEx(PDRIVEINFO pDrive, PBYTE pFatStart, ULONG ulCluster, USHORT usBlockSize)
{
   ulCluster &= pDrive->ulFatEof;

   switch (pDrive->bFatType)
      {
      case FAT_TYPE_FAT12:
         {
         PUSHORT pusCluster;
         ULONG   ulOffset = (ulCluster * 3) / 2;

         if (usBlockSize)
            ulOffset %= usBlockSize;

         pusCluster = (PUSHORT)((PBYTE)pFatStart + ulOffset);
         ulCluster = ( ((ulCluster * 3) % 2) ?
            *pusCluster >> 4 : // odd
            *pusCluster )      // even
            & pDrive->ulFatEof;
         break;
         }

      case FAT_TYPE_FAT16:
         {
         PUSHORT pusCluster;
         ULONG   ulOffset = ulCluster * 2;

         if (usBlockSize)
            ulOffset %= usBlockSize;

         pusCluster = (PUSHORT)((PBYTE)pFatStart + ulOffset);
         ulCluster = *pusCluster & pDrive->ulFatEof;
         break;
         }

      case FAT_TYPE_FAT32:
#ifdef EXFAT
      case FAT_TYPE_EXFAT:
#endif
         {
         PULONG pulCluster;
         ULONG  ulOffset = ulCluster * 4;

         if (usBlockSize)
            ulOffset %= usBlockSize;

         pulCluster = (PULONG)((PBYTE)pFatStart + ulOffset);
         ulCluster = *pulCluster & pDrive->ulFatEof;
         }
      }

   return ulCluster;
}
