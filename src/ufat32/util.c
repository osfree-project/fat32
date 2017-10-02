#include "fat32c.h"

void seek_to_sect( HANDLE hDevice, DWORD Sector, DWORD BytesPerSect );
void open_drive (char *path, HANDLE *hDevice);
void lock_drive(HANDLE hDevice);
void unlock_drive(HANDLE hDevice);
BOOL get_drive_params(HANDLE hDevice, struct extbpb *dp);
void set_part_type(HANDLE hDevice, struct extbpb *dp, int type);
void begin_format (HANDLE hDevice);
void remount_media (HANDLE hDevice);
void close_drive(HANDLE hDevice);
ULONG ReadSect(HANDLE hFile, LONG ulSector, USHORT nSectors, USHORT BytesPerSector, PBYTE pbSector);
ULONG WriteSect(HANDLE hf, LONG ulSector, USHORT nSectors, USHORT BytesPerSector, PBYTE pbSector);
ULONG ReadSectCD(HANDLE hFile, LONG ulSector, USHORT nSectors, USHORT BytesPerSector, PBYTE pbSector);
ULONG WriteSectCD(HANDLE hf, LONG ulSector, USHORT nSectors, USHORT BytesPerSector, PBYTE pbSector);
BYTE get_medium_type(char *pszFilename);


void SeekToSector(PCDINFO pCD, DWORD Sector, DWORD BytesPerSect)
{
   seek_to_sect( pCD->hDisk, Sector, BytesPerSect );
}


ULONG ReadSector(PCDINFO pCD, ULONG ulSector, ULONG nSectors, PBYTE pbSector)
{
   if (pCD->bMediumType == MEDIUM_TYPE_CDROM)
      return ReadSectCD(pCD->hDisk, ulSector, nSectors, pCD->BootSect.bpb.BytesPerSector, pbSector);
   else
      return ReadSect(pCD->hDisk, ulSector, nSectors, pCD->BootSect.bpb.BytesPerSector, pbSector);
}


ULONG WriteSector(PCDINFO pCD, ULONG ulSector, ULONG nSectors, PBYTE pbSector)
{
   if (pCD->bMediumType == MEDIUM_TYPE_CDROM)
      return WriteSectCD(pCD->hDisk, ulSector, nSectors, pCD->BootSect.bpb.BytesPerSector, pbSector);
   else
      return WriteSect(pCD->hDisk, ulSector, nSectors, pCD->BootSect.bpb.BytesPerSector, pbSector);
}


ULONG WriteSector2(PCDINFO pCD, DWORD Sector, DWORD BytesPerSector, void *Data, DWORD NumSects)
{
   DWORD dwWritten;
   ULONG ret;

   if (pCD->bMediumType == MEDIUM_TYPE_CDROM)
      ret = WriteSectCD( pCD->hDisk, Sector, NumSects, BytesPerSector, Data);
   else
      ret = WriteSect( pCD->hDisk, Sector, NumSects, BytesPerSector, Data);

   return ret;
}


ULONG ReadCluster(PCDINFO pCD, ULONG ulCluster, PBYTE pbCluster)
{
   ULONG ulSector;
   USHORT rc;

   if (ulCluster < 2 || ulCluster >= pCD->ulTotalClusters + 2)
      return ERROR_SECTOR_NOT_FOUND;

   ulSector = pCD->ulStartOfData +
      (ulCluster - 2) * pCD->SectorsPerCluster;

   rc = ReadSector(pCD, ulSector,
      pCD->SectorsPerCluster,
      pbCluster);

   if (rc)
      return rc;

   return 0;
}


ULONG WriteCluster(PCDINFO pCD, ULONG ulCluster, PVOID pbCluster)
{
   ULONG ulSector;
   ULONG rc;

   if (ulCluster < 2 || ulCluster >= pCD->ulTotalClusters + 2)
      return ERROR_SECTOR_NOT_FOUND;

   ulSector = pCD->ulStartOfData +
      (ulCluster - 2) * pCD->SectorsPerCluster;

   rc = WriteSector(pCD, ulSector,
      pCD->SectorsPerCluster, pbCluster);
   if (rc)
      return rc;

   return 0;
}


void OpenDrive(PCDINFO pCD, PSZ pszPath)
{
   pCD->bMediumType = get_medium_type(pszPath);
   open_drive(pszPath, &pCD->hDisk);
}


void LockDrive(PCDINFO pCD)
{
   lock_drive(pCD->hDisk);
}


void UnlockDrive(PCDINFO pCD)
{
   unlock_drive(pCD->hDisk);
}


BOOL GetDriveParams(PCDINFO pCD, struct extbpb *dp)
{
   return get_drive_params(pCD->hDisk, dp);
}


void SetPartType(PCDINFO pCD, struct extbpb *dp, int type)
{
   set_part_type(pCD->hDisk, dp, type);
}


void BeginFormat(PCDINFO pCD)
{
   begin_format(pCD->hDisk);
}


void RemountMedia(PCDINFO pCD)
{
   remount_media(pCD->hDisk);
}


void CloseDrive(PCDINFO pCD)
{
   close_drive(pCD->hDisk);
}
