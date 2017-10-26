#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fat32c.h"

extern char msg;

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
void low_level_fmt_floppy(HANDLE hDevice, format_params *params);
void low_level_fmt_cd(HANDLE hDevice, format_params *params);

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

void LowLevelFmtFloppy(PCDINFO pCD, format_params *params)
{
   low_level_fmt_floppy(pCD->hDisk, params);
}

void LowLevelFmtCD(PCDINFO pCD, format_params *params)
{
   low_level_fmt_cd(pCD->hDisk, params);
}

void LowLevelFmt(PCDINFO pCD, format_params *params)
{
   switch (pCD->bMediumType)
   {
   case MEDIUM_TYPE_CDROM:
      LowLevelFmtCD(pCD, params);
      break;

   case MEDIUM_TYPE_FLOPPY:
      LowLevelFmtFloppy(pCD, params);
      break;

   case MEDIUM_TYPE_OPTICAL:
      // none for now
      break;

   case MEDIUM_TYPE_DASD:
      // do nothing for hard disks
      break;
   }
}

void check_vol_label(char *path, char **vol_label)
{
    /* Current volume label  */
    char cur_vol[12];
    char testvol[12];
    /* file system information buffer */
    char   c;
    ULONG  rc;

    show_message("FAT32 version %s compiled on " __DATE__ "\n", 0, 0, 1, FAT32_VERSION);

    if (! msg)
    {
        memset(cur_vol, 0, sizeof(cur_vol));
        memset(testvol, 0, sizeof(testvol));

        rc = query_vol_label(path, cur_vol, sizeof(cur_vol));

        show_message( "The new type of file system is %1.\n", 0, 1293, 1, TYPE_STRING, "FAT32" );

        if (!cur_vol || !*cur_vol)
            show_message( "The disk has no volume label.\n", 0, 125, 0 );
        else
        {
            if (!vol_label || !*vol_label || !**vol_label)
            {
                show_message( "Enter the current volume label for drive %1 ", 0, 1318, 1, TYPE_STRING, path );

                // Read the volume label
                gets(testvol);
            }
        }

        // if the entered volume label is empty, or doesn't
        // the same as a current volume label, write an error
        if ( stricmp(*vol_label, cur_vol) && stricmp(testvol, cur_vol) && (!**vol_label || !*testvol))
        {
            show_message( "An incorrect volume label was entered for this drive.\n", 0, 636, 0 );
            exit (1);
        }
    }

    show_message( "Warning!  All data on hard disk %1 will be lost!\n"
                  "Proceed with FORMAT (Y/N)? ", 0, 1271, 1, TYPE_STRING, path );

    c = getchar();

    if ( c != '1' && toupper(c) != 'Y' )
        exit (1);
 
    fflush(stdout);
}

char *get_vol_label(char *path, char *vol)
{
    static char default_vol[12] = "NO NAME    ";
    static char v[12] = "           ";
    char *label = vol;

    if (!vol || !*vol)
    {
        fflush(stdin);
        show_message( "Enter up to 11 characters for the volume label\n"
                      "or press Enter for no volume label. ", 0, 1288, 0 );

        label = v;
    }

    memset(label, 0, 12);
    gets(label);

    if (!*label)
        label = default_vol;

    if (strlen(label) > 11)
    {
       show_message( "The volume label you entered exceeds the 11-character limit.\n"
                     "The first 11 characters were written to disk.  Any characters that\n"
                     "exceeded the 11-character limit were automatically deleted.\n", 0, 154, 0 );
       // truncate it
       label[11] = '\0';
    }

    return label;
}

void show_progress (float fPercentWritten)
{
    char str[128];
    
    sprintf(str, "%3.f", fPercentWritten);
    show_message( "%1% of the disk has been formatted.", 0, 538, 1,
                  TYPE_STRING, str );

    // restore cursor position
    printf("\r");

    fflush(stdout); 
}

void quit (int rc)
{
    cleanup ();
    exit (rc);
}
