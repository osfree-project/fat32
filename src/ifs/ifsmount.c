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

#define STDON   0x0100

int mount(unsigned short usFlag,
          PVOLINFO       pVolInfo,
          char far      *pBoot);

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
PRIVATE P_DriverCaps ReturnDriverCaps(UCHAR);
PRIVATE ULONG GetVolId(void);
IMPORT SEL cdecl SaSSel(void);

UCHAR GetFatType(PVOLINFO pVolInfo, PBOOTSECT pSect);

ULONG fat_mask = 0;
ULONG fat32_mask = 0;
#ifdef EXFAT
ULONG exfat_mask = 0;
#endif

extern PGINFOSEG pGI;

extern PCPDATA pCPData;

#pragma optimize("eglt",off)

int far pascal _loadds FS_MOUNT(unsigned short usFlag,  /* flag     */
                        struct vpfsi far * pvpfsi,      /* pvpfsi   */
                        struct vpfsd far * pvpfsd,      /* pvpfsd   */
                        unsigned short hVBP,            /* hVPB     */
                        char far *  pBoot               /* pBoot    */
)
{
PBOOTSECT pSect    = NULL;
PVOLINFO  pVolInfo = NULL;
PVOLINFO  pNext = NULL;
PVOLINFO  pPrev = NULL;
USHORT usVolCount;
USHORT hDupVBP  = 0;
USHORT rc = NO_ERROR;

   /*
    openjfs source does the same, just be on the safe side
   */
   _asm push es;
   _asm sti;

   MessageL(LOG_FS, "FS_MOUNT%m for %c (%d):, flag = %d", 0x0001,
            pvpfsi->vpi_drive + 'A',
            pvpfsi->vpi_unit,
            usFlag);

   switch (usFlag)
      {
      case MOUNT_MOUNT  :
         pSect = (PBOOTSECT)pBoot;

         if (FSH_FINDDUPHVPB(hVBP, &hDupVBP))
            hDupVBP = 0;

         if (hDupVBP)  /* remount of volume */
            {
            if ( *((ULONG *)pvpfsd->vpd_work + 1) == FAT32_VPB_MAGIC )
               {
               /* good VPB magic, valid VPB - retrieve VOLINFO from it */
               FSH_GETVOLPARM(hDupVBP,&pvpfsi,&pvpfsd);       /* Get the volume dependent/independent structure from the original volume block */
               pVolInfo = *((PVOLINFO *)(pvpfsd->vpd_work));  /* Get the pointer to the FAT32 Volume structure from the original block */
               hVBP = hDupVBP;                                /* indicate that the old duplicate will become the remaining block */
                                                              /* since the new VPB will be discarded if there already is an old one according to IFS.INF */
               }
            else
               {
               /* bad VPB magic, do an initial mount then too */
               hDupVBP = 0;
               }
            }

         if (!hDupVBP)   /* initial mounting of the volume */
            {
            pVolInfo = gdtAlloc(STORAGE_NEEDED, FALSE);
            if (!pVolInfo)
               {
               rc = ERROR_NOT_ENOUGH_MEMORY;
               goto FS_MOUNT_EXIT;
               }

            memset(pVolInfo, 0, (size_t)STORAGE_NEEDED);

            if (!ulCacheSectors)
               Message("FAT32: Warning CACHE size is zero!");
            //else
            //   InitCache(ulCacheSectors);

            pVolInfo->hVBP    = hVBP;
            pVolInfo->hDupVBP = hDupVBP;

            pVolInfo->bDrive = pvpfsi->vpi_drive;
            pVolInfo->bUnit  = pvpfsi->vpi_unit;

            rc = mount(usFlag, pVolInfo, pBoot);

            if (rc)
               goto FS_MOUNT_EXIT;

            rc = FSH_FORCENOSWAP(SELECTOROF(pVolInfo));
            if (rc)
               {
               FatalMessage("FSH_FORCENOSWAP on VOLINFO Segment failed, rc=%u", rc);
               rc = ERROR_GEN_FAILURE;
               goto FS_MOUNT_EXIT;
               }
            *((PVOLINFO *)pvpfsd->vpd_work) = pVolInfo;
            *((ULONG *)pvpfsd->vpd_work + 1) = FAT32_VPB_MAGIC;

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
            }

         break;

      case MOUNT_ACCEPT :
         if (FSH_FINDDUPHVPB(hVBP, &hDupVBP))
            hDupVBP = 0;

         //if (pvpfsi->vpi_bsize != SECTOR_SIZE)
         //   {
         //   rc = ERROR_VOLUME_NOT_MOUNTED;
         //   goto FS_MOUNT_EXIT;
         //   }
      
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

         pVolInfo->hVBP = hVBP;
         pVolInfo->hDupVBP = hDupVBP; //// ?

         pVolInfo->bDrive = pvpfsi->vpi_drive;
         pVolInfo->bUnit  = pvpfsi->vpi_unit;

         rc = mount(usFlag, pVolInfo, pBoot);

         if (rc)
            goto FS_MOUNT_EXIT;

         *((PVOLINFO *)(pvpfsd->vpd_work)) = pVolInfo;
         *((ULONG *)pvpfsd->vpd_work + 1) = FAT32_VPB_MAGIC;

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

         break;

      case MOUNT_VOL_REMOVED:
         break;

      case MOUNT_RELEASE:

         if ( *((ULONG *)pvpfsd->vpd_work + 1) != FAT32_VPB_MAGIC )
            {
            /* VPB magic is invalid, so it is not the VPB
               created by fat32.ifs */
            rc = ERROR_VOLUME_NOT_MOUNTED;
            goto FS_MOUNT_EXIT;
            }

         pVolInfo = GetVolInfo(hVBP);

         if (!pVolInfo)
            {
            if (f32Parms.fMessageActive & LOG_FS)
               Message("pVolInfo == 0\n");
            
            rc = ERROR_VOLUME_NOT_MOUNTED;
            goto FS_MOUNT_EXIT;
            }

         rc = mount(usFlag, pVolInfo, pBoot);

         if (rc)
            goto FS_MOUNT_EXIT;

         // delete pVolInfo from the list
         RemoveVolume(pVolInfo);
         *(PVOLINFO *)(pvpfsd->vpd_work) = NULL;
         *((ULONG *)pvpfsd->vpd_work + 1) = 0;
         free(pVolInfo);
         rc = NO_ERROR;
         break;
      }

FS_MOUNT_EXIT:
   MessageL(LOG_FS, "FS_MOUNT%m returned %u", 0x8001, rc);

   _asm pop es;

   return rc;
}

int PreSetup(void)
{
APIRET rc;

   rc = FSH_SEMREQUEST(&pCPData->semSerialize, -1);

   if (rc)
      return rc;

   rc = FSH_SEMWAIT(&pCPData->semRqDone, -1);

   if (rc)
      {
      FSH_SEMCLEAR(&pCPData->semSerialize);
      return rc;
      }

   return NO_ERROR;
}

int PostSetup(void)
{
APIRET rc;

   rc = FSH_SEMSET(&pCPData->semRqDone);

   if (rc)
      {
      FSH_SEMCLEAR(&pCPData->semSerialize);
      return rc;
      }

   FSH_SEMCLEAR(&pCPData->semRqAvail);

   rc = FSH_SEMWAIT(&pCPData->semRqDone, -1);

   FSH_SEMCLEAR(&pCPData->semSerialize);

   if (rc)
      return rc;

   return NO_ERROR;
}

int Mount(PMNTOPTS opts, PVOLINFO *pVolInfo)
{
PVOLINFO pNext;
APIRET rc;
UCHAR pBoot[512];
USHORT usVolCount;
ULONG hf;

   switch (opts->usOp)
      {
      case MOUNT_MOUNT:
         if (!opts->hf && GetVolInfoX(opts->pMntPoint))
            {
            return ERROR_ALREADY_EXISTS;
            }

         FSH_SEMCLEAR(&pCPData->semSerialize);

         if (opts->hf)
            {
            /* already opened */
            hf = opts->hf;
            rc = NO_ERROR;
            }
         else
            {
            /* open file */
            rc = PreSetup();

            if (rc)
               return rc;

            pCPData->Op = OP_OPEN;
            strcpy(pCPData->Buf, opts->pFilename);
            memcpy(pCPData->pFmt, opts->pFmt, 8);

            rc = PostSetup();

            if (rc)
               return rc;

            hf = pCPData->hf;
            rc = pCPData->rc;

            if (rc)
               return rc;
            }

         /* read the boot sector */
         rc = PreSetup();

         if (rc)
            return rc;

         pCPData->Op = OP_READ;
         pCPData->hf = hf;
         pCPData->llOffset = (LONGLONG)opts->ullOffset;
         pCPData->cbData = 512;

         rc = PostSetup();

         if (rc)
            return rc;

         rc = pCPData->rc;

         if (rc)
            return rc;

         memcpy(pBoot, pCPData->Buf, 512);

         /* create the VOLINFO structure */
         *pVolInfo = gdtAlloc(STORAGE_NEEDED, FALSE);
         if (!*pVolInfo)
            {
            rc = ERROR_NOT_ENOUGH_MEMORY;
            return rc;
            }

         memset(*pVolInfo, 0, (size_t)STORAGE_NEEDED);

         (*pVolInfo)->pbMntPoint = malloc(strlen(opts->pMntPoint) + 1);

         if (! (*pVolInfo)->pbMntPoint)
            return ERROR_NOT_ENOUGH_MEMORY;

         (*pVolInfo)->pbFilename = malloc(strlen(opts->pFilename) + 1);

         if (! (*pVolInfo)->pbFilename)
            return ERROR_NOT_ENOUGH_MEMORY;

         strcpy((*pVolInfo)->pbMntPoint, opts->pMntPoint);
         strcpy((*pVolInfo)->pbFilename, opts->pFilename);
         (*pVolInfo)->ullOffset = opts->ullOffset;
         (*pVolInfo)->hf = hf;

         rc = mount(opts->usOp, *pVolInfo, pBoot);

         if (rc)
            return rc;

         rc = FSH_FORCENOSWAP(SELECTOROF(*pVolInfo));
         if (rc)
            {
            FatalMessage("FSH_FORCENOSWAP on VOLINFO Segment failed, rc=%u", rc);
            rc = ERROR_GEN_FAILURE;
            return rc;
            }

         (*pVolInfo)->pNextVolInfo = NULL;

         if (!pGlobVolInfo)
            {
            pGlobVolInfo = *pVolInfo;
            }
         else
            {
            pNext = pGlobVolInfo;
            while(pNext->pNextVolInfo)
               {
               pNext = pNext->pNextVolInfo;
               }
            pNext->pNextVolInfo = *pVolInfo;
            }
         break;

      case MOUNT_ACCEPT:
         if (!opts->hf && GetVolInfoX(opts->pMntPoint))
            {
            return ERROR_ALREADY_EXISTS;
            }

         if (opts->hf)
            {
            /* already opened */
            hf = opts->hf;
            rc = NO_ERROR;
            }
         else
            {
            /* open file */
            rc = PreSetup();

            if (rc)
               return rc;

            pCPData->Op = OP_OPEN;
            strcpy(pCPData->Buf, opts->pFilename);
            memcpy(pCPData->pFmt, opts->pFmt, 8);

            rc = PostSetup();

            if (rc)
               return rc;

            hf = pCPData->hf;
            rc = pCPData->rc;

            if (rc)
               return rc;
            }

         /* read the boot sector */
         rc = PreSetup();

         if (rc)
            return rc;

         pCPData->Op = OP_READ;
         pCPData->hf = hf;
         pCPData->llOffset = (LONGLONG)opts->ullOffset;
         pCPData->cbData = 512;

         rc = PostSetup();

         if (rc)
            return rc;

         memcpy(pBoot, pCPData->Buf, 512);

         *pVolInfo = gdtAlloc(STORAGE_NEEDED, FALSE);
         if (!*pVolInfo)
            {
            rc = ERROR_NOT_ENOUGH_MEMORY;
            return rc;
            }
         rc = FSH_FORCENOSWAP(SELECTOROF(*pVolInfo));
         if (rc)
            FatalMessage("FSH_FORCENOSWAP on VOLINFO Segment failed, rc=%u", rc);

         memset(*pVolInfo, 0, (size_t)STORAGE_NEEDED);

         (*pVolInfo)->pbMntPoint = malloc(strlen(opts->pMntPoint) + 1);

         if (! (*pVolInfo)->pbMntPoint)
            return ERROR_NOT_ENOUGH_MEMORY;

         (*pVolInfo)->pbFilename = malloc(strlen(opts->pFilename) + 1);

         if (! (*pVolInfo)->pbFilename)
            return ERROR_NOT_ENOUGH_MEMORY;

         strcpy((*pVolInfo)->pbMntPoint, opts->pMntPoint);
         strcpy((*pVolInfo)->pbFilename, opts->pFilename);
         (*pVolInfo)->ullOffset = opts->ullOffset;
         (*pVolInfo)->hf = hf;

         rc = mount(opts->usOp, *pVolInfo, pBoot);

         if (rc)
            return rc;

         if (!pGlobVolInfo)
            {
            pGlobVolInfo = *pVolInfo;
            usVolCount = 1;
            }
         else
            {
            pNext = pGlobVolInfo;
            usVolCount = 1;
            //if (pNext->bDrive == pvpfsi->vpi_drive && !(*pVolInfo)->hDupVBP)
            //   (*pVolInfo)->hDupVBP = pNext->hVBP;
            while (pNext->pNextVolInfo)
               {
               pNext = (PVOLINFO)pNext->pNextVolInfo;
               //if (pNext->bDrive == pvpfsi->vpi_drive && !(*pVolInfo)->hDupVBP)
               //   (*pVolInfo)->hDupVBP = pNext->hVBP;
               usVolCount++;
               }
            pNext->pNextVolInfo = *pVolInfo;
            usVolCount++;
            }
         if (f32Parms.fMessageActive & LOG_FS)
            Message("%u Volumes mounted!", usVolCount);

         break;

      case MOUNT_VOL_REMOVED:
         break;

      case MOUNT_RELEASE:
         if (! opts->hf)
            {
            *pVolInfo = GetVolInfoX(opts->pMntPoint);
            }

         if (!*pVolInfo)
            {
            if (f32Parms.fMessageActive & LOG_FS)
               Message("pVolInfo == 0\n");
            
            rc = ERROR_VOLUME_NOT_MOUNTED;
            return rc;
            }

         rc = mount(opts->usOp, *pVolInfo, pBoot);

         if (rc)
            return rc;

         if (! opts->hf)
            {
            /* close file */
            rc = PreSetup();

            if (rc)
               return rc;

            pCPData->Op = OP_CLOSE;
            pCPData->hf = (*pVolInfo)->hf;

            rc = PostSetup();

            if (rc)
               return rc;
            }

         // delete pVolInfo from the list
         free((*pVolInfo)->pbMntPoint);
         free((*pVolInfo)->pbFilename);
         RemoveVolume(*pVolInfo);
         free(*pVolInfo);
         *pVolInfo = NULL;
         rc = NO_ERROR;
         break;
      }

   return rc;
}

int mount(unsigned short usFlag,
          PVOLINFO       pVolInfo,
          char far       *pBoot)
{
struct vpfsi far * pvpfsi;
struct vpfsd far * pvpfsd;
PBOOTSECT pSect    = NULL;
USHORT rc = NO_ERROR;

P_DriverCaps pDevCaps  = NULL;
P_VolChars   pVolChars = NULL;
BOOL fValidBoot = TRUE;
BOOL fNewBoot = TRUE;
int i;

   if (pVolInfo->hVBP)
      FSH_GETVOLPARM(pVolInfo->hVBP, &pvpfsi, &pvpfsd);

   switch (usFlag)
      {
      case MOUNT_MOUNT  :
            pSect = (PBOOTSECT)pBoot;

            pVolInfo->bFatType = GetFatType(pVolInfo, pSect);

            if (pVolInfo->bFatType == FAT_TYPE_NONE)
               {
               rc = ERROR_VOLUME_NOT_MOUNTED;
               freeseg(pVolInfo);
               goto MOUNT_EXIT;
               }

            if ( (pVolInfo->bFatType < FAT_TYPE_FAT32) &&
                 ((! f32Parms.fFat) ||
                  (pVolInfo->hVBP && ! (fat_mask & (1UL << pvpfsi->vpi_drive)))) )
               {
               rc = ERROR_VOLUME_NOT_MOUNTED;
               freeseg(pVolInfo);
               goto MOUNT_EXIT;
               }

            if ( (pVolInfo->bFatType == FAT_TYPE_FAT32) &&
                 (pVolInfo->hVBP && ! (fat32_mask & (1UL << pvpfsi->vpi_drive))) )
               {
               rc = ERROR_VOLUME_NOT_MOUNTED;
               freeseg(pVolInfo);
               goto MOUNT_EXIT;
               }

#ifdef EXFAT
            if ( (pVolInfo->bFatType == FAT_TYPE_EXFAT) &&
                 ((! f32Parms.fExFat) ||
                 (pVolInfo->hVBP && ! (exfat_mask & (1UL << pvpfsi->vpi_drive)))) )
               {
               rc = ERROR_VOLUME_NOT_MOUNTED;
               freeseg(pVolInfo);
               goto MOUNT_EXIT;
               }
#endif

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

#ifdef EXFAT
               case FAT_TYPE_EXFAT:
                  pVolInfo->ulFatEof   = EXFAT_EOF;
                  pVolInfo->ulFatEof2  = EXFAT_EOF2;
                  pVolInfo->ulFatBad   = EXFAT_BAD_CLUSTER;
                  pVolInfo->ulFatClean = EXFAT_CLEAN_SHUTDOWN;
#endif
               }

               // check for 0xf6 symbol (is the BPB erased?)
               for (i = 0; i < sizeof(BOOTSECT0); i++)
                  {
                  if (pBoot[i] != 0xf6)
                     break;
                  }

               if (i == sizeof(BOOTSECT0))
                  {
                  // erased by 0xf6
                  fValidBoot = FALSE;
                  }

               // check for 0x00 symbol (is the BPB erased?)
               for (i = 0; i < sizeof(BOOTSECT0); i++)
                  {
                  if (pBoot[i] != 0)
                     break;
                  }

               if (i == sizeof(BOOTSECT0))
                  {
                  // erased by 0x00
                  fValidBoot = FALSE;
                  }

               if (fValidBoot)
                  {
                  // not erased
                  switch (pVolInfo->bFatType)
                     {
                     case FAT_TYPE_FAT12:
                     case FAT_TYPE_FAT16:
                        if ( ((PBOOTSECT0)pSect)->bBootSig != 0x29 &&
                             ((PBOOTSECT0)pSect)->bBootSig != 0x28 )
                           fNewBoot = FALSE;
                        break;

                     case FAT_TYPE_FAT32:
                        if ( pSect->bBootSig != 0x29 &&
                             pSect->bBootSig != 0x28 )
                           fNewBoot = FALSE;
                     }
                  }

               if (! fValidBoot && pVolInfo->hVBP)
                  {
                  // construct the Serial No and Volume label
                  memcpy(pvpfsi->vpi_text, "UNREADABLE ", sizeof(pSect->VolumeLabel));
                  pvpfsi->vpi_vid = GetVolId();
                  }

               if (! fNewBoot && pVolInfo->hVBP)
                  {
                  memcpy(pvpfsi->vpi_text, "UNLABELED  ", sizeof(pSect->VolumeLabel));
                  pvpfsi->vpi_vid = GetVolId();
                  }

               if (pVolInfo->hVBP)
                  {
                  if (pVolInfo->bFatType < FAT_TYPE_FAT32)
                     {
                     PBOOTSECT0 pSect0 = (PBOOTSECT0)pSect;

                     /* Mounting pre-BPB FAT (from DOS 1.x),
                        the 1st FAT table sector is passed in pSect,
                        instead of the BPB. The 1st byte is the 
                        media type byte (this is the case with
                        mounting the VFDISK.SYS virtual floppy) */

                     // Media type byte:
                     // Byte   Capacity   Media Size and Type
                     // F0     2.88 MB    3.5-inch, 2-sided, 36-sector DS?ED
                     // F0     1.84 MB    3.5-inch  2-sided, 23-sector DS/HD
                     // F0     1.44 MB    3.5-inch, 2-sided, 18-sector DS/HD
                     // F9     1.2 MB     5.25-inch, 2-sided, 15-sector DS/HD
                     // F9     720 KB     3.5-inch, 2-sided, 9-sector DS/DHD
                     // FA     RAM disk   (not all ramdisks use this)
                     // FC     180 KB     5.25-inch, 1-sided, 9-sector SS/DD
                     // FD     360 KB     5.25-inch, 2-sided, 9-sector DS/DD
                     // FE     160 KB     5.25-inch, 1-sided, 8-sector SS/DD
                     // FF     320 KB     5.25-inch, 2-sided, 8-sector DS/DD
                     // F8     -----      Fixed disk
                     // F8     1.44 MB    3.5-inch, 2-sided, 9-sector DS/QD PS/2 diskette

                     // predefined floppy formats, no BPB
                     if (*(UCHAR *)pBoot != 0xeb && fValidBoot)
                        {
                        pSect0->bpb.BytesPerSector = 0x200;
                        pSect0->bpb.SectorsPerCluster = 1;
                        pSect0->bpb.ReservedSectors = 1;
                        pSect0->bpb.NumberOfFATs = 2;
                        pSect0->bpb.RootDirEntries = 0xe0;
                        pSect0->bpb.TotalSectors = 0;
                        pSect0->bpb.SectorsPerFat = 9;
                        pSect0->bpb.HiddenSectors = 0;
                        }

                     switch (*(UCHAR *)pBoot)
                        {
                        case 0xf0:
                           pSect0->bpb.MediaDescriptor = 0xf0;
                           if (pvpfsi->vpi_totsec == 2880)
                              {
                              // 1.44 MB, 80t/16s/2h, 3.5" // +
                              pSect0->bpb.SectorsPerFat = 9;
                              pSect0->bpb.SectorsPerCluster = 1;
                              pSect0->bpb.RootDirEntries = 0xe0;
                              pSect0->bpb.BigTotalSectors = 2880;
                              pSect0->bpb.SectorsPerTrack = 18;
                              pSect0->bpb.Heads = 2;
                              }
                           else if (pvpfsi->vpi_totsec == 3680)
                              {
                              // 1.84 MB, 80t/23s/2h, 3.5" // +
                              pSect0->bpb.SectorsPerFat = 11;
                              pSect0->bpb.SectorsPerCluster = 1;
                              pSect0->bpb.RootDirEntries = 0xe0;
                              pSect0->bpb.BigTotalSectors = 3680;
                              pSect0->bpb.SectorsPerTrack = 23;
                              pSect0->bpb.Heads = 2;
                              }
                           else
                              {
                              // 2.88 MB, 80t/36s/2h, 3.5" // +
                              pSect0->bpb.SectorsPerFat = 9;
                              pSect0->bpb.SectorsPerCluster = 2;
                              pSect0->bpb.RootDirEntries = 0xf0;
                              pSect0->bpb.BigTotalSectors = 5760;
                              pSect0->bpb.SectorsPerTrack = 36;
                              pSect0->bpb.Heads = 2;
                              }
                           break;

                        case 0xf9:
                           pSect0->bpb.MediaDescriptor = 0xf9;
                           if (pvpfsi->vpi_totsec == 1440)
                              {
                              // 720 KB, 80t/9s/2h, 3.5" // +
                              pSect0->bpb.SectorsPerFat = 3;
                              pSect0->bpb.SectorsPerCluster = 2;
                              pSect0->bpb.RootDirEntries = 0x70;
                              pSect0->bpb.BigTotalSectors = 1440;
                              pSect0->bpb.SectorsPerTrack = 9;
                              pSect0->bpb.Heads = 2;
                              }
                           else
                              {
                              // 1.2 MB, 80t/15s/2h, 5.25" // +
                              pSect0->bpb.SectorsPerFat = 7;
                              pSect0->bpb.BigTotalSectors = 2400;
                              pSect0->bpb.SectorsPerTrack = 15;
                              pSect0->bpb.Heads = 2;
                              }
                           break;

                        case 0xfc:
                           // 180 KB, 40t/9s/1h, 5.25" // -
                           pSect0->bpb.MediaDescriptor = 0xfc;
                           pSect0->bpb.SectorsPerFat = 2; // ?
                           pSect0->bpb.SectorsPerCluster = 1; // ?
                           pSect0->bpb.RootDirEntries = 0x70; // ?
                           pSect0->bpb.BigTotalSectors = 360;
                           pSect0->bpb.SectorsPerTrack = 9;
                           pSect0->bpb.Heads = 1;
                           break;

                        case 0xfd:
                           // 360 KB, 40t/9s/2h, 5.25" // +
                           pSect0->bpb.MediaDescriptor = 0xfd;
                           pSect0->bpb.SectorsPerFat = 2;
                           pSect0->bpb.SectorsPerCluster = 2;
                           pSect0->bpb.RootDirEntries = 0x70;
                           pSect0->bpb.BigTotalSectors = 720;
                           pSect0->bpb.SectorsPerTrack = 9;
                           pSect0->bpb.Heads = 2;
                           break;

                        case 0xfe:
                           // 160 KB, 40t/8s/1h, 5.25" // -
                           pSect0->bpb.MediaDescriptor = 0xfe;
                           pSect0->bpb.SectorsPerFat = 1; // ?
                           pSect0->bpb.SectorsPerCluster = 1; // ?
                           pSect0->bpb.RootDirEntries = 0x70; // ?
                           pSect0->bpb.BigTotalSectors = 320;
                           pSect0->bpb.SectorsPerTrack = 8;
                           pSect0->bpb.Heads = 1;
                           break;

                        case 0xff:
                           // 320 KB, 80t/8s/1h, 5.25" // -
                           pSect0->bpb.MediaDescriptor = 0xff;
                           pSect0->bpb.SectorsPerFat = 1; // ?
                           pSect0->bpb.SectorsPerCluster = 2; // ?
                           pSect0->bpb.RootDirEntries = 0x70; // ?
                           pSect0->bpb.BigTotalSectors = 640;
                           pSect0->bpb.SectorsPerTrack = 8;
                           pSect0->bpb.Heads = 1;
                        }

                        if (fValidBoot)
                           {
                           pvpfsi->vpi_bsize  = ((PBOOTSECT0)pSect)->bpb.BytesPerSector;
                           pvpfsi->vpi_totsec = ((PBOOTSECT0)pSect)->bpb.BigTotalSectors;
                           pvpfsi->vpi_trksec = ((PBOOTSECT0)pSect)->bpb.SectorsPerTrack;
                           pvpfsi->vpi_nhead  = ((PBOOTSECT0)pSect)->bpb.Heads;
                           }
                     }
                  else if (pVolInfo->bFatType == FAT_TYPE_FAT32)
                     {
                     if (fValidBoot)
                        {
                        pvpfsi->vpi_bsize  = pSect->bpb.BytesPerSector;
                        pvpfsi->vpi_totsec = pSect->bpb.BigTotalSectors;
                        pvpfsi->vpi_trksec = pSect->bpb.SectorsPerTrack;
                        pvpfsi->vpi_nhead  = pSect->bpb.Heads;
                        }
                     }
#ifdef EXFAT
                  else if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
                     {
                     if (fValidBoot)
                        {
                        pvpfsi->vpi_bsize  = 1 << ((PBOOTSECT1)pSect)->bBytesPerSectorShift;
#ifdef INCL_LONGLONG
                        pvpfsi->vpi_totsec = (ULONG)((PBOOTSECT1)pSect)->ullVolumeLength;
#else
                        pvpfsi->vpi_totsec = ((PBOOTSECT1)pSect)->ullVolumeLength.ulLo;
#endif
                        }
                     }
#endif
                  }
         /* continue mount in both cases:
            for a first time mount it's an initializaton
            for the n-th remount it's a reinitialization of the old VPB
         */
         memcpy(&pVolInfo->BootSect, pSect, sizeof (BOOTSECT));

#ifdef EXFAT
         if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
#endif
            pVolInfo->ulActiveFatStart = pSect->bpb.ReservedSectors;
#ifdef EXFAT
         else
            pVolInfo->ulActiveFatStart = ((PBOOTSECT1)pSect)->ulFatOffset;
#endif

#ifdef EXFAT
         if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
            {
            pVolInfo->ulStartOfData    = ((PBOOTSECT1)pSect)->ulClusterHeapOffset;
            }
         else if (pVolInfo->bFatType == FAT_TYPE_FAT32)
#else
         if (pVolInfo->bFatType == FAT_TYPE_FAT32)
#endif
            {
            pVolInfo->ulStartOfData    = pSect->bpb.ReservedSectors +
                  pSect->bpb.BigSectorsPerFat * pSect->bpb.NumberOfFATs;
            }
         else if (pVolInfo->bFatType < FAT_TYPE_FAT32)
            {
            PBOOTSECT0 pSect0 = (PBOOTSECT0)pSect;
            pVolInfo->ulStartOfData    = pSect0->bpb.ReservedSectors +
               (ULONG)pSect0->bpb.SectorsPerFat * pSect0->bpb.NumberOfFATs +
               ((ULONG)pSect0->bpb.RootDirEntries * sizeof(DIRENTRY)) / pSect0->bpb.BytesPerSector +
               ((((ULONG)pSect0->bpb.RootDirEntries * sizeof(DIRENTRY)) % pSect0->bpb.BytesPerSector) ? 1 : 0);
            }

         pVolInfo->pBootFSInfo = (PBOOTFSINFO)(pVolInfo + 1);

         pVolInfo->pbFatSector = (PBYTE)(pVolInfo->pBootFSInfo + 1);

         pVolInfo->ulCurFatSector = -1L;
         pVolInfo->pbFatBits = (PBYTE)pVolInfo->pbFatSector + SECTOR_SIZE * 8 * 3;
         
#ifdef EXFAT
         if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
            {
#endif
            pVolInfo->ulClusterSize = (ULONG)pSect->bpb.BytesPerSector;
            pVolInfo->ulClusterSize *= pSect->bpb.SectorsPerCluster;
            pVolInfo->SectorsPerCluster = pVolInfo->BootSect.bpb.SectorsPerCluster;
#ifdef EXFAT
            }
         else
            {
            // exFAT case
            pVolInfo->ulClusterSize =  (ULONG)(1 << ((PBOOTSECT1)pSect)->bSectorsPerClusterShift);
            pVolInfo->ulClusterSize *= 1 << ((PBOOTSECT1)pSect)->bBytesPerSectorShift;
            pVolInfo->BootSect.bpb.BytesPerSector = 1 << ((PBOOTSECT1)pSect)->bBytesPerSectorShift;
            pVolInfo->SectorsPerCluster = 1 << ((PBOOTSECT1)pSect)->bSectorsPerClusterShift;
            pVolInfo->BootSect.bpb.ReservedSectors = (USHORT)((PBOOTSECT1)pSect)->ulFatOffset;
            pVolInfo->BootSect.bpb.RootDirStrtClus = ((PBOOTSECT1)pSect)->RootDirStrtClus;
            pVolInfo->BootSect.bpb.BigSectorsPerFat = ((PBOOTSECT1)pSect)->ulFatLength;
            pVolInfo->BootSect.bpb.SectorsPerFat = (USHORT)((PBOOTSECT1)pSect)->ulFatLength;
            pVolInfo->BootSect.bpb.NumberOfFATs = ((PBOOTSECT1)pSect)->bNumFats;
#ifdef INCL_LONGLONG
            pVolInfo->BootSect.bpb.BigTotalSectors = (ULONG)((PBOOTSECT1)pSect)->ullVolumeLength;  ////
            pVolInfo->BootSect.bpb.HiddenSectors = (ULONG)((PBOOTSECT1)pSect)->ullPartitionOffset; ////
#else
            pVolInfo->BootSect.bpb.BigTotalSectors = ((PBOOTSECT1)pSect)->ullVolumeLength.ulLo;  ////
            pVolInfo->BootSect.bpb.HiddenSectors = ((PBOOTSECT1)pSect)->ullPartitionOffset.ulLo; ////
#endif
            }
#endif

         // size of a subcluster block
         pVolInfo->ulBlockSize = min(pVolInfo->ulClusterSize, 32768UL);

         pVolInfo->pNextVolInfo = NULL;

         pVolInfo->fFormatInProgress = FALSE;

         if (usDefaultRASectors == 0xFFFF)
            pVolInfo->usRASectors = (USHORT)(pVolInfo->ulBlockSize / pVolInfo->BootSect.bpb.BytesPerSector ) * 2;
         else
            pVolInfo->usRASectors = usDefaultRASectors;

#if 1
         if( pVolInfo->usRASectors > MAX_RASECTORS )
            pVolInfo->usRASectors = MAX_RASECTORS;
#else
         if (pVolInfo->usRASectors > (pVolInfo->ulBlockSize / pVolInfo->BootSect.bpb.BytesPerSector) * 4)
            pVolInfo->usRASectors = (pVolInfo->ulBlockSize / pVolInfo->BootSect.bpb.BytesPerSector ) * 4;
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
            if (fNewBoot && pVolInfo->hVBP)
               {
               pvpfsi->vpi_vid = ((PBOOTSECT0)pSect)->ulVolSerial;
               memcpy(pVolInfo->BootSect.FileSystem, "FAT     ", 8);
               }

            // create FAT32-type extended BPB for FAT12/FAT16
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
         if (pVolInfo->bFatType == FAT_TYPE_FAT32)
            {
            if (fNewBoot && pVolInfo->hVBP)
               {
               pvpfsi->vpi_vid = pSect->ulVolSerial;
               memcpy(pVolInfo->BootSect.FileSystem, "FAT32   ", 8);
               }
            }
#ifdef EXFAT
         else if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
            {
            // create FAT32-type extended BPB for exFAT
            if (fValidBoot && pVolInfo->hVBP)
               {
               pvpfsi->vpi_vid = ((PBOOTSECT1)pSect)->ulVolSerial;
               memcpy(pVolInfo->BootSect.FileSystem, "EXFAT   ", 8);
               }

#ifdef INCL_LONGLONG
            pVolInfo->BootSect.bpb.BigTotalSectors = (ULONG)((PBOOTSECT1)pSect)->ullVolumeLength; ////
#else
            pVolInfo->BootSect.bpb.BigTotalSectors = ((PBOOTSECT1)pSect)->ullVolumeLength.ulLo; ////
#endif
            pVolInfo->BootSect.bpb.BigSectorsPerFat = ((PBOOTSECT1)pSect)->ulFatLength;
            pVolInfo->BootSect.bpb.ExtFlags = 0;
            // force calculating the free space
            pVolInfo->BootSect.bpb.FSinfoSec = 0xFFFF;
            }
#endif

         if (pVolInfo->hVBP)
            pVolInfo->BootSect.ulVolSerial = pvpfsi->vpi_vid;

         if (! pVolInfo->BootSect.bpb.BigSectorsPerFat)
            // if partition is small
            pVolInfo->BootSect.bpb.BigSectorsPerFat = pVolInfo->BootSect.bpb.SectorsPerFat;

         pVolInfo->ulTotalClusters =
            (pVolInfo->BootSect.bpb.BigTotalSectors - pVolInfo->ulStartOfData) / pVolInfo->SectorsPerCluster;

         if (pVolInfo->bFatType == FAT_TYPE_FAT32)
            {
               {
               pVolInfo->ulActiveFatStart +=
                  pSect->bpb.BigSectorsPerFat * (pSect->bpb.ExtFlags & 0x000F);
               }
            }
#ifdef EXFAT
         else if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
            {
            // exFAT case
            ULONGLONG ullLen;
            ULONG ulChecksum;

            if (((PBOOTSECT1)pSect)->usVolumeFlags & VOL_FLAG_ACTIVEFAT)
               {
               pVolInfo->ulActiveFatStart += ((PBOOTSECT1)pSect)->ulFatLength;
               }

            fGetAllocBitmap(pVolInfo, &pVolInfo->ulAllocBmpCluster, &ullLen);
#ifdef INCL_LONGLONG
            pVolInfo->ulAllocBmpLen = (ULONG)ullLen;
#else
            pVolInfo->ulAllocBmpLen = ullLen.ulLo;
#endif
            pVolInfo->ulBmpStartSector = pVolInfo->ulStartOfData +
               (pVolInfo->ulAllocBmpCluster - 2) * pVolInfo->SectorsPerCluster;
            pVolInfo->ulCurBmpSector = 0xffffffff;
            fGetUpCaseTbl(pVolInfo, &pVolInfo->ulUpcaseTblCluster, &ullLen, &ulChecksum);
#ifdef INCL_LONGLONG
            pVolInfo->ulUpcaseTblLen = (ULONG)ullLen;
#else
            pVolInfo->ulUpcaseTblLen = ullLen.ulLo;
#endif
            }
#endif

         if (fValidBoot)
            {
            char pszVolLabel[11];
            USHORT usSize = 11;

            memset(pszVolLabel, 0, sizeof(pszVolLabel));

            rc = fGetSetVolLabel(pVolInfo, INFO_RETRIEVE, pszVolLabel, &usSize);

            // prevent writing the FSInfo sector in pVolInfo->pbFatSector buffer
            // over 0 sector of 1st FAT on unmount if pVolInfo->ulCurFatSector == 0
            // (see MarkDiskStatus)
            pVolInfo->ulCurFatSector = 0xffff;

            if (pVolInfo->hVBP)
               {
               memset(pvpfsi->vpi_text, 0, sizeof(pvpfsi->vpi_text));

               if (rc || ! pszVolLabel || ! *pszVolLabel)
                  strcpy(pvpfsi->vpi_text, "UNLABELED  ");
               else
                  strcpy(pvpfsi->vpi_text, pszVolLabel);

               memset(pVolInfo->BootSect.VolumeLabel, 0, sizeof(pVolInfo->BootSect.VolumeLabel));
               memcpy(pVolInfo->BootSect.VolumeLabel, pvpfsi->vpi_text, sizeof(pVolInfo->BootSect.VolumeLabel));
               }
            }

         rc = CheckWriteProtect(pVolInfo);

         if (rc && rc != ERROR_WRITE_PROTECT)
            {
            Message("Cannot access drive, rc = %u", rc);
            goto MOUNT_EXIT;
            }
         if (rc == ERROR_WRITE_PROTECT)
            {
            pVolInfo->fWriteProtected = TRUE;
            rc = NO_ERROR;
            }

         if (f32Parms.fCalcFree ||
            pVolInfo->pBootFSInfo->ulFreeClusters == 0xFFFFFFFF ||
          /*!pVolInfo->fDiskClean ||*/
            pVolInfo->BootSect.bpb.FSinfoSec == 0xFFFF)
            {
            GetFreeSpace(pVolInfo);
            }

         if (pVolInfo->hVBP)
            {
         //   pDevCaps  = pvpfsi->vpi_pDCS;
            pVolChars = pvpfsi->vpi_pVCS;

         //if (!pDevCaps)
         //   {
         //   Message("Strategy2 not found, searching Device Driver chain !");
            pDevCaps = ReturnDriverCaps(pvpfsi->vpi_unit);
         //   }
            }

         if ( pVolChars && (pVolChars->VolDescriptor & VC_REMOVABLE_MEDIA) )
            pVolInfo->fRemovable = TRUE;

         // consider CDRW (which have 0xffff sectors per track) to be "removables" too
         if ( ! pVolInfo->fRemovable &&
              pVolInfo->hVBP &&
              pvpfsi->vpi_trksec == 0xffff )
            pVolInfo->fRemovable = TRUE;

         if (! pVolInfo->fRemovable && pVolInfo->hVBP)
            pVolInfo->fDiskCleanOnMount = pVolInfo->fDiskClean = GetDiskStatus(pVolInfo);
         else
            // ignore disk status on floppies
            pVolInfo->fDiskCleanOnMount = pVolInfo->fDiskClean = TRUE;

         if (!pVolInfo->fDiskCleanOnMount && f32Parms.fMessageActive & LOG_FS)
            Message("DISK IS DIRTY!");

         if (pVolInfo->fWriteProtected)
            pVolInfo->fDiskCleanOnMount = TRUE;

         if (pDevCaps && f32Parms.fMessageActive & LOG_FS)
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

         if (pDevCaps && pDevCaps->Strategy2)
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
         pSect = (PBOOTSECT)pBoot;

         if (pVolInfo->hVBP)
            {
            // pSect == NULL here!
            memset(&pVolInfo->BootSect, 0, sizeof (BOOTSECT));
            pVolInfo->BootSect.bpb.BigTotalSectors = pvpfsi->vpi_totsec;
            pVolInfo->BootSect.bpb.BytesPerSector  = pvpfsi->vpi_bsize;
            }
         else
            {
            memcpy(&pVolInfo->BootSect, pSect, sizeof (BOOTSECT));
            };

         pVolInfo->fWriteProtected   = FALSE;
         pVolInfo->fDiskCleanOnMount = FALSE;

         pVolInfo->pNextVolInfo = NULL;

         // the volume is being formatted
         pVolInfo->fFormatInProgress = TRUE;

         // fake values assuming sector == cluster
         if (pVolInfo->hVBP)
            {
            pVolInfo->ulClusterSize   = pvpfsi->vpi_bsize;
            pVolInfo->ulTotalClusters = pvpfsi->vpi_totsec;
            }
         else
            {
            pVolInfo->ulClusterSize   = pSect->bpb.BytesPerSector;
            pVolInfo->ulTotalClusters = pSect->bpb.BigTotalSectors;
            }

         pVolInfo->ulBlockSize = min(pVolInfo->ulClusterSize, 32768UL);

         pVolInfo->usRASectors = usDefaultRASectors;

         // undefined
         pVolInfo->ulStartOfData = 0;

         if (pVolInfo->hVBP)
            {
         //   pDevCaps  = pvpfsi->vpi_pDCS;
            pVolChars = pvpfsi->vpi_pVCS;

         //if (!pDevCaps)
         //   {
         //   Message("Strategy2 not found, searching Device Driver chain !");
            pDevCaps = ReturnDriverCaps(pvpfsi->vpi_unit);
         //   }
            }

         if (pDevCaps && f32Parms.fMessageActive & LOG_FS)
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

         if (pDevCaps && pDevCaps->Strategy2)
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
         break;

      case MOUNT_RELEASE:
         if (! pVolInfo->fFormatInProgress &&
             ! pVolInfo->fRemovable && (pVolInfo->bFatType != FAT_TYPE_FAT12) )
            {
               usFlushVolume( pVolInfo, FLUSH_DISCARD, TRUE, PRIO_URGENT );
               UpdateFSInfo(pVolInfo);

               // ignore dirty status on floppies (and FAT12)
               MarkDiskStatus(pVolInfo, pVolInfo->fDiskCleanOnMount);
            }
         break;

      default :
         rc = ERROR_NOT_SUPPORTED;
         break;
      }

MOUNT_EXIT:
   if (rc)
      rc = ERROR_VOLUME_NOT_MOUNTED;

   return rc;
}

#pragma optimize("",on)


USHORT CheckWriteProtect(PVOLINFO pVolInfo)
{
USHORT rc;
USHORT usSectors = 1;

   MessageL(LOG_FUNCS, "CheckWriteProtect%m", 0x0100);

   if (pVolInfo->hVBP)
      {
      rc = FSH_DOVOLIO(DVIO_OPREAD, DVIO_ALLACK, pVolInfo->hVBP, pVolInfo->pbFatSector, &usSectors, 1L);
      if (!rc)
         {
         usSectors = 1;
         rc = FSH_DOVOLIO(DVIO_OPWRITE, DVIO_ALLACK, pVolInfo->hVBP, pVolInfo->pbFatSector, &usSectors, 1L);
         }
      }
   else
      {
      /* read loopback device */
      rc = PreSetup();

      if (rc)
         return rc;

      pCPData->Op = OP_READ;
      pCPData->hf = pVolInfo->hf;
      pCPData->llOffset = (LONGLONG)pVolInfo->ullOffset + pVolInfo->BootSect.bpb.BytesPerSector;
      pCPData->cbData = usSectors * pVolInfo->BootSect.bpb.BytesPerSector;

      rc = PostSetup();

      if (rc)
         return rc;

      rc = pCPData->rc;

      /* write loopback device */
      rc = PreSetup();

      if (rc)
         return rc;

      usSectors = 1;

      pCPData->Op = OP_WRITE;
      pCPData->hf = pVolInfo->hf;
      pCPData->llOffset = (LONGLONG)pVolInfo->ullOffset + pVolInfo->BootSect.bpb.BytesPerSector;
      pCPData->cbData = usSectors * pVolInfo->BootSect.bpb.BytesPerSector;

      rc = PostSetup();

      if (rc)
         return rc;

      rc = pCPData->rc;

      if (rc == ERROR_WRITE_FAULT)
         rc = ERROR_WRITE_PROTECT;
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


UCHAR GetFatType(PVOLINFO pVolInfo, PBOOTSECT pSect)
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
   ULONG TotClus;
   ULONG CountOfClusters;
   ULONG ulSector, ulNumFats;
   USHORT usSectorsPerFat;
   BYTE bLeadByte, bFatType;
   char *pFat;
   ULONG buf[2];
   int i;

   if (! pSect)
      {
      return FAT_TYPE_NONE;
      } /* endif */

   // BPB/FAT leading byte
   bLeadByte = *(PBYTE)pSect;

   // floppy formats defined with the media type byte
   // (the first byte of FAT)
   if (bLeadByte == 0xf0 ||
       bLeadByte == 0xf9 ||
       bLeadByte == 0xfc ||
       bLeadByte == 0xfd ||
       bLeadByte == 0xfe ||
       bLeadByte == 0xff)
      return FAT_TYPE_FAT12;

#ifdef EXFAT
   if (!memcmp(pSect->oemID, "EXFAT   ", 8))
      {
      return FAT_TYPE_EXFAT;
      } /* endif */
#endif

   pbpb = &pSect->bpb;

   if (!pbpb->BytesPerSector)
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

   RootDirSectors = (((ULONG)pbpb->RootDirEntries * 32UL) + (pbpb->BytesPerSector-1UL)) / pbpb->BytesPerSector;

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

   if (! pSect->bpb.BigTotalSectors && ! pSect->bpb.TotalSectors)
      return FAT_TYPE_NONE;

   if (! pSect->bpb.BytesPerSector || ! pSect->bpb.SectorsPerCluster)
      return FAT_TYPE_NONE;

   ulSector = pSect->bpb.ReservedSectors;
   ulNumFats = pSect->bpb.NumberOfFATs;
   usSectorsPerFat = pSect->bpb.SectorsPerFat;
   bLeadByte = pSect->bpb.MediaDescriptor;

   if (! ulSector || ! ulNumFats || ! bLeadByte)
      return FAT_TYPE_NONE;

   if (! usSectorsPerFat && ! pSect->bpb.BigSectorsPerFat)
      return FAT_TYPE_NONE;

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

   TotClus = (TotSec - NonDataSec) / pbpb->SectorsPerCluster;

   if (!memcmp(((PBOOTSECT0)pSect)->FileSystem, "FAT", 3))
      {
      if (TotClus < 0xff6)
         return FAT_TYPE_FAT12;

      return FAT_TYPE_FAT16;
      }

   // JMP SHORT instruction at the beginning of the boot sector
   if (pSect->bJmp[0] != 0xeb)
      return FAT_TYPE_NONE;

   if (pSect->bJmp[1] < 0x29)
      {
      // old-style pre-DOS 4.0 BPB (jump offset < 0x29, which
      // means that FS name and Serial No. are missing)
      if (TotClus < 0xff6)
         return FAT_TYPE_FAT12;

      return FAT_TYPE_FAT16;
      } /* endif */

   if (! pVolInfo)
      {
      // don't check FATs if called from autocheck routine
      // passing pVolInfo == NULL
      return FAT_TYPE_NONE;
      }

   // init some fields for ReadSector()
   if (pSect->bpb.TotalSectors)
      pVolInfo->BootSect.bpb.BigTotalSectors = pSect->bpb.TotalSectors;
   else
      pVolInfo->BootSect.bpb.BigTotalSectors = pSect->bpb.BigTotalSectors;

   pVolInfo->BootSect.bpb.BytesPerSector = pSect->bpb.BytesPerSector;

   pVolInfo->ulStartOfData = pVolInfo->BootSect.bpb.BigTotalSectors;

   if (! pVolInfo->BootSect.bpb.BigTotalSectors || ! pVolInfo->BootSect.bpb.BytesPerSector)
      return FAT_TYPE_NONE;

   if (CountOfClusters >= 65525UL)
      bFatType = FAT_TYPE_FAT32;
   else if (CountOfClusters >= 4085UL)
      bFatType = FAT_TYPE_FAT16;
   else
      bFatType = FAT_TYPE_FAT12;

   pFat = malloc(SECTOR_SIZE * 8);

   if (! pFat)
      return FAT_TYPE_NONE;

   for (i = 0; i < ulNumFats; i++)
      {
      // read each FAT copy start
      if (ReadSector(pVolInfo, ulSector, 1, (void far *)pFat, 0))
         {
         bFatType = FAT_TYPE_NONE;
         break;
         }

      switch (bFatType)
         {
         case FAT_TYPE_FAT12:
            buf[0] = 0xffff00 | bLeadByte;
            buf[1] = 0;

            if (memcmp(pFat, buf, 2))
               bFatType = FAT_TYPE_NONE;

            break;

         case FAT_TYPE_FAT16:
            buf[0] = 0xffffff00 | bLeadByte;
            buf[1] = 0;

            if (memcmp(pFat, buf, 2))
               bFatType = FAT_TYPE_NONE;

            break;

         case FAT_TYPE_FAT32:
            buf[0] = 0x0fffff00 | bLeadByte;
            buf[1] = 0x0fffffff;

            if (memcmp(pFat, buf, 4))
               bFatType = FAT_TYPE_NONE;

            break;

         default:
            bFatType = FAT_TYPE_NONE;
         }

      ulSector += usSectorsPerFat;
      }

   free(pFat);

   return bFatType;
}

/* Generate Volume Serial Number */
ULONG GetVolId(void)
{
    ULONG d;
    ULONG lo,hi,tmp;

    lo = pGI->day + ( pGI->month << 8 );
    tmp = pGI->hundredths + (pGI->seconds << 8 );
    lo += tmp;

    hi = pGI->minutes + ( pGI->hour << 8 );
    hi += pGI->year;
   
    d = lo + (hi << 16);

    return d;
}

#pragma optimize("eglt",off)

#define SAS_SEL 0x70

PRIVATE P_DriverCaps ReturnDriverCaps(UCHAR ucUnit)
{
   RP_GETDRIVERCAPS rp={0};
   PRP_GETDRIVERCAPS pRH = &rp;
   PFN pStrat = NULL;
   SEL dsSel = 0;
   SEL SAS_selector;
   struct SAS far *pSas;
   struct SAS_dd_section far *pDDSection;
   struct SysDev far *pDD;

   SAS_selector    = SAS_SEL;
   pSas            = (struct SAS far *)MAKEP(SAS_selector,0);
   pDDSection      = (struct SAS_dd_section far *)MAKEP(SAS_selector,pSas->SAS_dd_data);
   pDD             = (struct SysDev far *)MAKEP(SAS_selector,pDDSection->SAS_dd_bimodal_chain);

   while (pDD && (pDD != (struct SysDev far *)-1))
      {
      //if (
      //    (memicmp(&pDD->SDevName[1],"Disk DD",7) == 0) &&        // found OS2DASD.DMD
      //    (pDD->SDevAtt == (DEV_NON_IBM | DEVLEV_1 | DEV_30))     // found OS2DASD.DMD
      //    )
      //   {
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
            if (pRH->rph.Status == STDON) //
               {                          //
               return pRH->pDCS;          //
               }                          //
      //      }

      //      break;
            }
      pDD = (struct SysDev far *)pDD->SDevNext;
      }
   return NULL;
}

#pragma optimize("",on)
