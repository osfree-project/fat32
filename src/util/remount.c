#include "fat32def.h"
#include "portable.h"

#include <stdio.h>
#include <string.h>

void lock_drive(HFILE hf);
void unlock_drive(HFILE hf);
APIRET remount_media (HFILE hf);
UCHAR GetFatType(PBOOTSECT pSect);

/* Remount all FAT disks */
int remount_all(void)
{
  char drive[3];
  char boot_sector[SECTOR_SIZE];
  HFILE drive_handle;
  ULONG action;
  ULONG bytes_read;
  char type;
  APIRET rc;
  int i;

  for (i = 2; i <= 25; i++)  /* loop for max drives allowed */
  {
    /* set up the drive name to be used in DosOpen */
    drive[0] = (char)('a' + i);
    drive[1] = ':';
    drive[2] = 0;

    /* open the drive */
    rc = DosOpen(drive,
                 &drive_handle,
                 &action,
                 0L,
                 0,
                 1,
                 0x8020,
                 0L);

    /* if the open fails then just skip this drive */
    if (rc)
    {
      continue;
    }

    /* read sector 0 the boot sector of this drive */
    rc = DosRead(drive_handle, boot_sector, SECTOR_SIZE, &bytes_read);

    /* if the read fails then just skip this drive */
    if (rc)
    {
      DosClose (drive_handle);
      continue;
    }

    /* check the system_id in the boot sector to be sure it is FAT */
    /* if it is not then we will just skip this drive              */
    /* due to link problems we can't use memcmp for this           */

    // get FAT type
    type = GetFatType((PBOOTSECT)boot_sector);

    if (type == FAT_TYPE_NONE)
    {
      DosClose (drive_handle);
      continue;
    }

    if (type >= FAT_TYPE_FAT32)
    {
      DosClose (drive_handle);
      continue;
    }

    lock_drive(drive_handle);
    remount_media(drive_handle);
    unlock_drive(drive_handle);
    DosClose(drive_handle);
  }

  return 0;
}

void lock_drive(HFILE hf)
{
  unsigned char  parminfo  = 0;
  unsigned char  datainfo  = 0;
  unsigned long  parmlen   = 1;
  unsigned long  datalen   = 1;
  APIRET rc;

  rc = DosDevIOCtl( hf, IOCTL_DISK, DSK_LOCKDRIVE, &parminfo,
                    parmlen, &parmlen, &datainfo, // Param packet
                    datalen, &datalen);
}

void unlock_drive(HFILE hf)
{
  unsigned char  parminfo = 0;
  unsigned char  datainfo = 0;
  unsigned long  parmlen  = 1;
  unsigned long  datalen  = 1;
  APIRET rc;

  rc = DosDevIOCtl( hf, IOCTL_DISK, DSK_UNLOCKDRIVE, &parminfo,
                    parmlen, &parmlen, &datainfo, // Param packet
                    datalen, &datalen);
}

APIRET remount_media (HFILE hf)
{
  // Redetermine media
  unsigned char parminfo  = 0;
  unsigned char datainfo  = 0;
  unsigned long parmlen   = 1;
  unsigned long datalen   = 1;
  APIRET rc;

  rc = DosDevIOCtl( hf, IOCTL_DISK, DSK_REDETERMINEMEDIA,
                    &parminfo, parmlen, &parmlen, // Param packet
                    &datainfo, datalen, &datalen);

  return rc;
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

   if (!memcmp(((PBOOTSECT0)pSect)->FileSystem, "FAT", 3))
      {
      ULONG TotClus = (TotSec - NonDataSec) / pbpb->SectorsPerCluster;

      if (TotClus < 0xff6)
         return FAT_TYPE_FAT12;

      return FAT_TYPE_FAT16;
      }

   return FAT_TYPE_NONE;
}
