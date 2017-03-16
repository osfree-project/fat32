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

#include "devhdr.h"
#include "devcmd.h"
#include "sas.h"

/*
**  Request Packet Header
*/

typedef struct _RPH  RPH;
typedef struct _RPH  FAR *PRPH;
typedef struct _RPH  *NPRPH;

typedef struct _RPH  {                  /* RPH */

  UCHAR         Len;
  UCHAR         Unit;
  UCHAR         Cmd;
  USHORT        Status;
  UCHAR         Flags;
  UCHAR         Reserved_1[3];
  PRPH          Link;
} RPH;

/*
**  Get Driver Capabilities  0x1D
*/

typedef struct _RP_GETDRIVERCAPS  {     /* RPDC */

  RPH           rph;
  UCHAR         Reserved[3];
  P_DriverCaps  pDCS;
  P_VolChars    pVCS;

} RP_GETDRIVERCAPS, FAR *PRP_GETDRIVERCAPS;

PRIVATE BOOL RemoveVolume(PVOLINFO pVolInfo);
PRIVATE USHORT CheckWriteProtect(PVOLINFO);
PRIVATE UCHAR GetFatType(PBOOTSECT pBoot);
PRIVATE P_DriverCaps ReturnDriverCaps(UCHAR);
IMPORT SEL cdecl SaSSel(void);

#pragma optimize("eglt",off)

int far pascal _loadds FS_MOUNT(unsigned short usFlag,      /* flag     */
                        struct vpfsi far * pvpfsi,      /* pvpfsi   */
                        struct vpfsd far * pvpfsd,      /* pvpfsd   */
                        unsigned short hVBP,        /* hVPB     */
                        char far *  pBoot       /* pBoot    */)
{
PBOOTSECT pSect    = NULL;
PVOLINFO  pVolInfo = NULL;
PVOLINFO  pNext = NULL;
PVOLINFO  pPrev = NULL;
USHORT hDupVBP  = 0;
USHORT rc = NO_ERROR;
USHORT usVolCount;

P_DriverCaps pDevCaps;
P_VolChars   pVolChars;

   /*
    openjfs source does the same, just be on the safe side
   */
   _asm push es;
   _asm sti;

   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_MOUNT for %c (%d):, flag = %d",
         pvpfsi->vpi_drive + 'A',
         pvpfsi->vpi_unit,
         usFlag);

   switch (usFlag)
      {
      case MOUNT_MOUNT  :
         pSect = (PBOOTSECT)pBoot;

         if (FSH_FINDDUPHVPB(hVBP, &hDupVBP))
            hDupVBP = 0;

         if (!hDupVBP)   /* initial mounting of the volume */
            {
            pVolInfo = gdtAlloc(STORAGE_NEEDED, FALSE);
            if (!pVolInfo)
               {
               rc = ERROR_NOT_ENOUGH_MEMORY;
               goto FS_MOUNT_EXIT;
               }

            memset(pVolInfo, 0, (size_t)STORAGE_NEEDED);

            pVolInfo->bFatType = GetFatType(pSect);

            if (pVolInfo->bFatType == FAT_TYPE_NONE)
               {
               rc = ERROR_VOLUME_NOT_MOUNTED;
               freeseg(pVolInfo);
               goto FS_MOUNT_EXIT;
               }

            if (!f32Parms.fFat && (pVolInfo->bFatType < FAT_TYPE_FAT32))
               {
               rc = ERROR_VOLUME_NOT_MOUNTED;
               freeseg(pVolInfo);
               goto FS_MOUNT_EXIT;
               }

            if (!f32Parms.fExFat && (pVolInfo->bFatType == FAT_TYPE_EXFAT))
               {
               rc = ERROR_VOLUME_NOT_MOUNTED;
               freeseg(pVolInfo);
               goto FS_MOUNT_EXIT;
               }

            switch (pVolInfo->bFatType)
               {
               case FAT_TYPE_FAT12:
                  pVolInfo->ulFatEof   = FAT12_EOF;
                  pVolInfo->ulFatEof2  = FAT12_EOF2;
                  pVolInfo->ulFatBad   = FAT12_BAD_CLUSTER;
                  pVolInfo->ulFatClean = FAT12_CLEAN_SHUTDOWN;
                  break;

               case FAT_TYPE_FAT16:
                  pVolInfo->ulFatEof   = FAT16_EOF;
                  pVolInfo->ulFatEof2  = FAT16_EOF2;
                  pVolInfo->ulFatBad   = FAT16_BAD_CLUSTER;
                  pVolInfo->ulFatClean = FAT16_CLEAN_SHUTDOWN;
                  break;

               case FAT_TYPE_FAT32:
                  pVolInfo->ulFatEof   = FAT32_EOF;
                  pVolInfo->ulFatEof2  = FAT32_EOF2;
                  pVolInfo->ulFatBad   = FAT32_BAD_CLUSTER;
                  pVolInfo->ulFatClean = FAT32_CLEAN_SHUTDOWN;
                  break;

               case FAT_TYPE_EXFAT:
                  pVolInfo->ulFatEof   = EXFAT_EOF;
                  pVolInfo->ulFatEof2  = EXFAT_EOF2;
                  pVolInfo->ulFatBad   = EXFAT_BAD_CLUSTER;
                  pVolInfo->ulFatClean = EXFAT_CLEAN_SHUTDOWN;
               }

            rc = FSH_FORCENOSWAP(SELECTOROF(pVolInfo));
            if (rc)
               {
               FatalMessage("FSH_FORCENOSWAP on VOLINFO Segment failed, rc=%u", rc);
               rc = ERROR_GEN_FAILURE;
               goto FS_MOUNT_EXIT;
               }
            *((PVOLINFO *)(pvpfsd->vpd_work)) = pVolInfo;

            pVolInfo->pNextVolInfo = NULL;

            if (!pGlobVolInfo)
               {
               pGlobVolInfo = pVolInfo;
               }
            else
               {
               pNext = pGlobVolInfo;
               while(pNext->pNextVolInfo)
                  {
                  pNext = pNext->pNextVolInfo;
                  }
               pNext->pNextVolInfo = pVolInfo;
               }
               pvpfsi->vpi_vid    = pSect->ulVolSerial;
               pvpfsi->vpi_bsize  = pSect->bpb.BytesPerSector;
               pvpfsi->vpi_totsec = pSect->bpb.BigTotalSectors;
               pvpfsi->vpi_trksec = pSect->bpb.SectorsPerTrack;
               pvpfsi->vpi_nhead  = pSect->bpb.Heads;
               memset(pvpfsi->vpi_text, 0, sizeof pvpfsi->vpi_text);
               memcpy(pvpfsi->vpi_text, pSect->VolumeLabel, sizeof pSect->VolumeLabel);
            }
         else  /* remount of volume */
            {
            FSH_GETVOLPARM(hDupVBP,&pvpfsi,&pvpfsd);       /* Get the volume dependent/independent structure from the original volume block */
            pVolInfo = *((PVOLINFO *)(pvpfsd->vpd_work));  /* Get the pointer to the FAT32 Volume structure from the original block */
            hVBP = hDupVBP;                                /* indicate that the old duplicate will become the remaining block */
                                                           /* since the new VPB will be discarded if there already is an old one according to IFS.INF */
            }

         /* continue mount in both cases:
            for a first time mount it's an initializaton
            for the n-th remount it's a reinitialization of the old VPB
         */
         memcpy(&pVolInfo->BootSect, pSect, sizeof (BOOTSECT));

         pVolInfo->ulActiveFatStart = pSect->bpb.ReservedSectors;
         if (pVolInfo->bFatType == FAT_TYPE_FAT32)
            {
            pVolInfo->ulStartOfData    = pSect->bpb.ReservedSectors +
                  pSect->bpb.BigSectorsPerFat * pSect->bpb.NumberOfFATs;
            }
         else if (pVolInfo->bFatType < FAT_TYPE_FAT32)
            {
            pVolInfo->ulStartOfData    = pSect->bpb.ReservedSectors +
               pSect->bpb.SectorsPerFat * pSect->bpb.NumberOfFATs +
               (pSect->bpb.RootDirEntries * sizeof(DIRENTRY)) / pSect->bpb.BytesPerSector;
            }

         pVolInfo->pBootFSInfo = (PBOOTFSINFO)(pVolInfo + 1);
         pVolInfo->pbFatSector = (PBYTE)(pVolInfo->pBootFSInfo + 1);
         pVolInfo->ulCurFatSector = -1L;
         pVolInfo->ulClusterSize = (ULONG)pSect->bpb.BytesPerSector;
         pVolInfo->ulClusterSize *= pSect->bpb.SectorsPerCluster;
         pVolInfo->ulBlockSize = min(pVolInfo->ulClusterSize, 32768UL);

         pVolInfo->hVBP    = hVBP;
         pVolInfo->hDupVBP = hDupVBP;

         pVolInfo->bDrive = pvpfsi->vpi_drive;
         pVolInfo->bUnit  = pvpfsi->vpi_unit;
         pVolInfo->pNextVolInfo = NULL;

         pVolInfo->fFormatInProgress = FALSE;

         if (usDefaultRASectors == 0xFFFF)
            pVolInfo->usRASectors = (pVolInfo->ulBlockSize / SECTOR_SIZE ) * 2;
         else
            pVolInfo->usRASectors = usDefaultRASectors;

#if 1
         if( pVolInfo->usRASectors > MAX_RASECTORS )
            pVolInfo->usRASectors = MAX_RASECTORS;
#else
         if (pVolInfo->usRASectors > (pVolInfo->ulBlockSize / SECTOR_SIZE) * 4)
            pVolInfo->usRASectors = (pVolInfo->ulBlockSize / SECTOR_SIZE ) * 4;
#endif

         if (pVolInfo->bFatType == FAT_TYPE_FAT32 && pSect->bpb.FSinfoSec != 0xFFFF)
            {
            ReadSector(pVolInfo, pSect->bpb.FSinfoSec, 1, pVolInfo->pbFatSector, DVIO_OPNCACHE);
            memcpy(pVolInfo->pBootFSInfo, pVolInfo->pbFatSector + FSINFO_OFFSET, sizeof (BOOTFSINFO));
            }
         else
            memset(pVolInfo->pBootFSInfo, 0, sizeof (BOOTFSINFO));

         if (pVolInfo->bFatType < FAT_TYPE_FAT32)
            {
            // create FAT32-type extended BPB for FAT12/FAT16
            pVolInfo->BootSect.ulVolSerial = ((PBOOTSECT0)pSect)->ulVolSerial;
            memcpy(pVolInfo->BootSect.VolumeLabel, ((PBOOTSECT0)pSect)->VolumeLabel, 11);
            memcpy(pVolInfo->BootSect.FileSystem, ((PBOOTSECT0)pSect)->FileSystem, 8);

            if (!((PBOOTSECT0)pSect)->bpb.TotalSectors)
               pVolInfo->BootSect.bpb.BigTotalSectors = ((PBOOTSECT0)pSect)->bpb.BigTotalSectors;
            else
               pVolInfo->BootSect.bpb.BigTotalSectors = ((PBOOTSECT0)pSect)->bpb.TotalSectors;

            pVolInfo->BootSect.bpb.BigSectorsPerFat = ((PBOOTSECT0)pSect)->bpb.SectorsPerFat;
            pVolInfo->BootSect.bpb.ExtFlags = 0;
            // special value for root dir cluster (fake one)
            pVolInfo->BootSect.bpb.RootDirStrtClus = 1;
            // force calculating the free space
            pVolInfo->BootSect.bpb.FSinfoSec = 0xFFFF;
            }

         pVolInfo->ulTotalClusters =
            (pVolInfo->BootSect.bpb.BigTotalSectors - pVolInfo->ulStartOfData) / pSect->bpb.SectorsPerCluster;

         if (pVolInfo->bFatType == FAT_TYPE_FAT32)
            {
            if (pSect->bpb.ExtFlags & 0x0080)
               {
               pVolInfo->ulActiveFatStart +=
                  pSect->bpb.BigSectorsPerFat * (pSect->bpb.ExtFlags & 0x000F);
               }
            }

         rc = CheckWriteProtect(pVolInfo);
         if (rc && rc != ERROR_WRITE_PROTECT)
            {
            Message("Cannot access drive, rc = %u", rc);
            goto FS_MOUNT_EXIT;
            }
         if (rc == ERROR_WRITE_PROTECT)
            pVolInfo->fWriteProtected = TRUE;

         pVolInfo->fDiskCleanOnMount = pVolInfo->fDiskClean = GetDiskStatus(pVolInfo);
         if (!pVolInfo->fDiskCleanOnMount && f32Parms.fMessageActive & LOG_FS)
            Message("DISK IS DIRTY!");
         if (pVolInfo->fWriteProtected)
            pVolInfo->fDiskCleanOnMount = TRUE;

         if (f32Parms.fCalcFree ||
            pVolInfo->pBootFSInfo->ulFreeClusters == 0xFFFFFFFF ||
          /*!pVolInfo->fDiskClean ||*/
            pVolInfo->BootSect.bpb.FSinfoSec == 0xFFFF)
         GetFreeSpace(pVolInfo);

         pDevCaps  = pvpfsi->vpi_pDCS;
         pVolChars = pvpfsi->vpi_pVCS;

         if (!pDevCaps)
            {
            Message("Strategy2 not found, searching Device Driver chain !");
            pDevCaps = ReturnDriverCaps(pvpfsi->vpi_unit);
            }

         if (f32Parms.fMessageActive & LOG_FS)
            {
            if (pDevCaps->Capabilities & GDC_DD_Read2)
               Message("Read2 supported");
            if (pDevCaps->Capabilities & GDC_DD_DMA_Word)
               Message("DMA on word alligned buffers supported");
            if (pDevCaps->Capabilities & GDC_DD_DMA_Byte)
               Message("DMA on byte alligned buffers supported");
            if (pDevCaps->Capabilities & GDC_DD_Mirror)
               Message("Disk Mirroring supported");
            if (pDevCaps->Capabilities & GDC_DD_Duplex)
               Message("Disk Duplexing supported");
            if (pDevCaps->Capabilities & GDC_DD_No_Block)
               Message("Strategy2 does not block");
            if (pDevCaps->Capabilities & GDC_DD_16M)
               Message(">16M supported");
            }

         if (pDevCaps->Strategy2)
            {
            if (f32Parms.fMessageActive & LOG_FS)
               {
               Message("Strategy2   address at %lX", pDevCaps->Strategy2);
               Message("ChgPriority address at %lX", pDevCaps->ChgPriority);
               }

            pVolInfo->pfnStrategy = (STRATFUNC)pDevCaps->Strategy2;
            pVolInfo->pfnPriority = (STRATFUNC)pDevCaps->ChgPriority;
            }

         rc = 0;
         break;

      case MOUNT_ACCEPT :
         if (FSH_FINDDUPHVPB(hVBP, &hDupVBP))
            hDupVBP = 0;

         if (pvpfsi->vpi_bsize != SECTOR_SIZE)
            {
            rc = ERROR_VOLUME_NOT_MOUNTED;
            goto FS_MOUNT_EXIT;
            }
      
         pVolInfo = gdtAlloc(STORAGE_NEEDED, FALSE);
         if (!pVolInfo)
            {
            rc = ERROR_NOT_ENOUGH_MEMORY;
            goto FS_MOUNT_EXIT;
            }
         rc = FSH_FORCENOSWAP(SELECTOROF(pVolInfo));
         if (rc)
            FatalMessage("FSH_FORCENOSWAP on VOLINFO Segment failed, rc=%u", rc);

         memset(pVolInfo, 0, (size_t)STORAGE_NEEDED);

         memset(&pVolInfo->BootSect, 0, sizeof (BOOTSECT));
         pVolInfo->BootSect.bpb.BigTotalSectors = pvpfsi->vpi_totsec;
         pVolInfo->BootSect.bpb.BytesPerSector  = pvpfsi->vpi_bsize;
         pVolInfo->fWriteProtected   = FALSE;
         pVolInfo->fDiskCleanOnMount = FALSE;

         pVolInfo->hVBP = hVBP;
         pVolInfo->hDupVBP = hDupVBP; //// ?
         pVolInfo->bDrive = pvpfsi->vpi_drive;
         pVolInfo->bUnit  = pvpfsi->vpi_unit;
         pVolInfo->pNextVolInfo = NULL;

         // the volume is being formatted
         pVolInfo->fFormatInProgress = TRUE;

         // fake values assuming sector == cluster
         pVolInfo->ulClusterSize   = pvpfsi->vpi_bsize;
         pVolInfo->ulBlockSize = min(pVolInfo->ulClusterSize, 32768UL);
         pVolInfo->ulTotalClusters = pvpfsi->vpi_totsec;

         pVolInfo->usRASectors = usDefaultRASectors;

         // undefined
         pVolInfo->ulStartOfData = 0;

         *((PVOLINFO *)(pvpfsd->vpd_work)) = pVolInfo;

         if (!pGlobVolInfo)
            {
            pGlobVolInfo = pVolInfo;
            usVolCount = 1;
            }
         else
            {
            pNext = pGlobVolInfo;
            usVolCount = 1;
            if (pNext->bDrive == pvpfsi->vpi_drive && !pVolInfo->hDupVBP)
               pVolInfo->hDupVBP = pNext->hVBP;
            while (pNext->pNextVolInfo)
               {
               pNext = (PVOLINFO)pNext->pNextVolInfo;
               if (pNext->bDrive == pvpfsi->vpi_drive && !pVolInfo->hDupVBP)
                  pVolInfo->hDupVBP = pNext->hVBP;
               usVolCount++;
               }
            pNext->pNextVolInfo = pVolInfo;
            usVolCount++;
            }
         if (f32Parms.fMessageActive & LOG_FS)
            Message("%u Volumes mounted!", usVolCount);

         pDevCaps  = pvpfsi->vpi_pDCS;
         pVolChars = pvpfsi->vpi_pVCS;

         if (!pDevCaps)
            {
            Message("Strategy2 not found, searching Device Driver chain !");
            pDevCaps = ReturnDriverCaps(pvpfsi->vpi_unit);
            }

         if (f32Parms.fMessageActive & LOG_FS)
            {
            if (pDevCaps->Capabilities & GDC_DD_Read2)
               Message("Read2 supported");
            if (pDevCaps->Capabilities & GDC_DD_DMA_Word)
               Message("DMA on word alligned buffers supported");
            if (pDevCaps->Capabilities & GDC_DD_DMA_Byte)
               Message("DMA on byte alligned buffers supported");
            if (pDevCaps->Capabilities & GDC_DD_Mirror)
               Message("Disk Mirroring supported");
            if (pDevCaps->Capabilities & GDC_DD_Duplex)
               Message("Disk Duplexing supported");
            if (pDevCaps->Capabilities & GDC_DD_No_Block)
               Message("Strategy2 does not block");
            if (pDevCaps->Capabilities & GDC_DD_16M)
               Message(">16M supported");
            }

         if (pDevCaps->Strategy2)
            {
            if (f32Parms.fMessageActive & LOG_FS)
               {
               Message("Strategy2   address at %lX", pDevCaps->Strategy2);
               Message("ChgPriority address at %lX", pDevCaps->ChgPriority);
               }

            pVolInfo->pfnStrategy = (STRATFUNC)pDevCaps->Strategy2;
            pVolInfo->pfnPriority = (STRATFUNC)pDevCaps->ChgPriority;
            }

         rc = 0;
         break;

      case MOUNT_VOL_REMOVED:
      case MOUNT_RELEASE:
         pVolInfo = GetVolInfo(hVBP);

         if (!pVolInfo)
            {
            if (f32Parms.fMessageActive & LOG_FS)
               Message("pVolInfo == 0\n");
            
            rc = ERROR_VOLUME_NOT_MOUNTED;
            goto FS_MOUNT_EXIT;
            }

         usFlushVolume( pVolInfo, FLUSH_DISCARD, TRUE, PRIO_URGENT );
         UpdateFSInfo(pVolInfo);
         MarkDiskStatus(pVolInfo, TRUE);

         // delete pVolInfo from the list
         RemoveVolume(pVolInfo);
         *(PVOLINFO *)(pvpfsd->vpd_work) = NULL;
         free(pVolInfo);
         rc = NO_ERROR;
         break;

      default :
         rc = ERROR_NOT_SUPPORTED;
         break;
      }

FS_MOUNT_EXIT:
   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_MOUNT returned %u\n", rc);

   _asm pop es;

   return rc;
}

#pragma optimize("",on)


USHORT CheckWriteProtect(PVOLINFO pVolInfo)
{
USHORT rc;
USHORT usSectors = 1;

   if (f32Parms.fMessageActive & LOG_FUNCS)
      Message("CheckWriteProtect");

   rc = FSH_DOVOLIO(DVIO_OPREAD, DVIO_ALLACK, pVolInfo->hVBP, pVolInfo->pbFatSector, &usSectors, 1L);
   if (!rc)
      {
      usSectors = 1;
      rc = FSH_DOVOLIO(DVIO_OPWRITE, DVIO_ALLACK, pVolInfo->hVBP, pVolInfo->pbFatSector, &usSectors, 1L);
      }

   return rc;
}

BOOL RemoveVolume(PVOLINFO pVolInfo)
{
PVOLINFO pNext;
USHORT rc;

   if (pGlobVolInfo == pVolInfo)
      {
      pGlobVolInfo = pVolInfo->pNextVolInfo;
      return TRUE;
      }

   pNext = (PVOLINFO)pGlobVolInfo;
   while (pNext)
      {
      rc = MY_PROBEBUF(PB_OPWRITE, (PBYTE)pNext, sizeof (VOLINFO));
      if (rc)
         {
         FatalMessage("FAT32: Protection VIOLATION in RemoveVolume!\n");
         return FALSE;
         }

      if (pNext->pNextVolInfo == pVolInfo)
         {
         pNext->pNextVolInfo = pVolInfo->pNextVolInfo;
         return TRUE;
         }
      pNext = (PVOLINFO)pNext->pNextVolInfo;
      }
   return FALSE;
}


static UCHAR GetFatType(PBOOTSECT pSect)
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

   if (!memcmp(pSect->FileSystem, "FAT12   ", 8))
      {
      return FAT_TYPE_FAT12;
      } /* endif */

   if (!memcmp(pSect->FileSystem, "FAT16   ", 8))
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


#pragma optimize("eglt",off)

P_DriverCaps ReturnDriverCaps(UCHAR ucUnit)
{
   RP_GETDRIVERCAPS rp={0};
   PRP_GETDRIVERCAPS pRH = &rp;
   PFN pStrat = NULL;
   SEL dsSel = 0;
   SEL SAS_selector;
   struct SAS far *pSas;
   struct SAS_dd_section far *pDDSection;
   struct SysDev far *pDD;

   SAS_selector    = SaSSel();
   pSas            = (struct SAS far *)MAKEP(SAS_selector,0);
   pDDSection      = (struct SAS_dd_section far *)MAKEP(SAS_selector,pSas->SAS_dd_data);
   pDD             = (struct SysDev far *)MAKEP(SAS_selector,pDDSection->SAS_dd_bimodal_chain);

   while (pDD && (pDD != (struct SysDev far *)-1))
      {
      if (
          (memicmp(&pDD->SDevName[1],"Disk DD",7) == 0) &&        // found OS2DASD.DMD
          (pDD->SDevAtt == (DEV_NON_IBM | DEVLEV_1 | DEV_30))     // found OS2DASD.DMD
          )
         {
         pStrat = (PFN)MAKEP(pDD->SDevProtCS,pDD->SDevStrat);
         dsSel   = pDD->SDevProtDS;

         if (pStrat)
            {

            pRH->rph.Len    = sizeof(rp);
            pRH->rph.Unit   = ucUnit;
            pRH->rph.Cmd    = CMDGetDevSupport;

            _asm
               {
               push ds
               push es
               push bx
               mov ax,dsSel
               mov ds,ax
               mov es,word ptr pRH+2
               mov bx,word ptr pRH
               call dword ptr pStrat
               pop bx
               pop es
               pop ds
               }
            return pRH->pDCS;
            }

            break;
         }
      pDD = (struct SysDev far *)pDD->SDevNext;
      }
   return NULL;
}

#pragma optimize("",on)
