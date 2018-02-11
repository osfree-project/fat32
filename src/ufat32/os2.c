/*   OS/2 specific functions
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <conio.h>
#include <assert.h>
#include <string.h>

#ifdef __WATCOMC__
#include <ctype.h>
#endif

#include "fat32c.h"
#include "fat32def.h"
#include "portable.h"

#define  IOCTL_CDROMDISK2         0x82
#define  CDROMDISK_READDATA       0x76
#define  CDROMDISK_WRITEDATA      0x56
#define  CDROMDISK2_FEATURES      0x63
#define  CDROMDISK2_DRIVELETTERS  0x60

HANDLE hDev;

char msg = FALSE;

char FS_NAME[8] = "FAT32";

PSZ GetOS2Error(USHORT rc);

void LogOutMessagePrintf(ULONG ulMsgNo, char *psz, ULONG ulParmNo, va_list va);
void show_progress (float fPercentWritten);

#ifdef __UNICODE__
USHORT QueryUni2NLS( USHORT usPage, USHORT usChar );
USHORT QueryNLS2Uni( USHORT usCode );

extern UCHAR rgFirstInfo[ 256 ];
#endif

BOOL fDiskImage = FALSE;

#pragma pack(1)

typedef struct _PTE
{
    char boot_ind;
    char starting_head;
    unsigned short starting_sector:6;
    unsigned short starting_cyl:10;
    char system_id;
    char ending_head;
    unsigned short ending_sector:6;
    unsigned short ending_cyl:10;
    unsigned long relative_sector;
    unsigned long total_sectors;
} PTE;

struct _mbr
{
    char pad[0x1be];
    PTE  pte[4];
    unsigned short boot_ind; /* 0x55aa */
} mbr;

#pragma pack()

DWORD get_vol_id (void)
{
    DATETIME s;
    DWORD d;
    WORD lo,hi,tmp;

    DosGetDateTime( &s );

    lo = s.day + ( s.month << 8 );
    tmp = s.hundredths + (s.seconds << 8 );
    lo += tmp;

    hi = s.minutes + ( s.hours << 8 );
    hi += s.year;
   
    d = lo + (hi << 16);
    return(d);
}

void die ( char * error, DWORD rc )
{
    char pszMsg[MAX_MESSAGE];
    APIRET ret;
   
    // Format failed
    show_message("ERROR: %s\n", 0, 0, 1, error);
    show_message("The specified disk did not finish formatting.\n", 0, 528, 0);
    show_message("%s\n", 0, 0, 1, get_error(rc));

    if ( rc )
        show_message("Error code: %lu\n", 0, 0, 1, rc);

    cleanup();
    exit ( rc );
}

void seek_to_sect( HANDLE hDevice, DWORD Sector, DWORD BytesPerSect )
{
    LONGLONG llOffset, llActual;
    APIRET rc;
    
    llOffset = Sector * BytesPerSect;
    rc = DosSetFilePtrL( (HFILE)hDevice, (LONGLONG)llOffset, FILE_BEGIN, &llActual );

    if ( rc )
        show_message("The drive cannot find the sector (area) requested.\n", 0, 27, 0);
}

ULONG ReadSect(HANDLE hFile, LONG ulSector, USHORT nSectors, USHORT BytesPerSector, PBYTE pbSector)
{
   ULONG ulDataSize;
   BIOSPARAMETERBLOCK bpb;

   #pragma pack(1)

   /* parameter packet */
   struct {
       unsigned char command;
       unsigned char drive;
   } parm2;

   #pragma pack()

   #define BUFSIZE 0x4000

   char buf[BUFSIZE];

   ULONG parmlen2 = sizeof(parm2);
   ULONG datalen2 = sizeof(bpb);
   ULONG cbRead32;
   ULONG rc;
   ULONG parmlen4;
   char trkbuf[sizeof(TRACKLAYOUT) + sizeof(USHORT) * 2 * 255];
   PTRACKLAYOUT ptrk = (PTRACKLAYOUT)trkbuf;
   ULONG datalen4 = BUFSIZE;
   USHORT cyl, head, sec, n;
   ULONGLONG off;
   int i;

   parm2.command = 1;
   parm2.drive = 0;

   ulDataSize = nSectors * BytesPerSector;

   if ( (rc = DosDevIOCtl((HFILE)hFile, IOCTL_DISK, DSK_GETDEVICEPARAMS,
                          &parm2, parmlen2, &parmlen2, &bpb, datalen2, &datalen2)) )
   {
       parm2.command = 0;

       if ( (rc = DosDevIOCtl((HFILE)hFile, IOCTL_DISK, DSK_GETDEVICEPARAMS,
                          &parm2, parmlen2, &parmlen2, &bpb, datalen2, &datalen2)) )
           return rc;
   }

   if (!bpb.cHeads || !bpb.usSectorsPerTrack)
       return ERROR_SECTOR_NOT_FOUND;

   memset((char *)trkbuf, 0, sizeof(trkbuf));
   off =  ulSector + bpb.cHiddenSectors;
   off *= BytesPerSector;

   for (i = 0; i < bpb.usSectorsPerTrack; i++)
   {
       ptrk->TrackTable[i].usSectorNumber = i + 1;
       ptrk->TrackTable[i].usSectorSize = BytesPerSector;
   }

   do
   {
       cbRead32 = (ulDataSize > BUFSIZE) ? BUFSIZE : (ULONG)ulDataSize;
       parmlen4 = sizeof(TRACKLAYOUT) + sizeof(USHORT) * 2 * (bpb.usSectorsPerTrack - 1);
       datalen4 = BUFSIZE;

       cyl = off / (BytesPerSector * bpb.cHeads * bpb.usSectorsPerTrack);
       head = (off / BytesPerSector - bpb.cHeads * bpb.usSectorsPerTrack * cyl) / bpb.usSectorsPerTrack;
       sec = off / BytesPerSector - bpb.cHeads * bpb.usSectorsPerTrack * cyl - head * bpb.usSectorsPerTrack;

       if (sec + cbRead32 / BytesPerSector > bpb.usSectorsPerTrack)
           cbRead32 = (bpb.usSectorsPerTrack - sec) * BytesPerSector;

       n = cbRead32 / BytesPerSector;

       ptrk->bCommand = 1;
       ptrk->usHead = head;
       ptrk->usCylinder = cyl;
       ptrk->usFirstSector = sec;
       ptrk->cSectors = n;

       rc = DosDevIOCtl((HFILE)hFile, IOCTL_DISK, DSK_READTRACK,
                        trkbuf, parmlen4, &parmlen4, buf, datalen4, &datalen4);

       if (rc)
           break;

       memcpy(pbSector, buf, min(datalen4, ulDataSize));

       off           += cbRead32;
       ulDataSize    -= cbRead32;
       pbSector      = (char *)pbSector + cbRead32;

    } while ((ulDataSize > 0) && ! rc);

    return rc;
}

ULONG ReadSectCD(HANDLE hFile, LONG ulSector, USHORT nSectors, USHORT BytesPerSector, PBYTE pbSector)
{
   LONG lDataSize;
   BIOSPARAMETERBLOCK bpb;

   #pragma pack(1)

   // parameter packet
   struct ReadData_param {
      ULONG       ID_code;                // 'CD01'
      UCHAR       address_mode;           // Addressing format of start_sector:
                                          //  00 - Logical Block format
                                          //  01 - Minutes/Seconds/Frame format
      USHORT      transfer_count;         // Numbers of sectors to read.
                                          //  Must  be non zero
      ULONG       start_sector;           // Starting sector number of the read operation
      UCHAR       reserved;               // Reserved. Must be 0
      UCHAR       interleave_size;        // Not used. Must be 0
      UCHAR       interleave_skip_factor; // Not used. Must be 0
   } parm;
   ULONG parmlen = sizeof(parm);

   #define SECTORSIZE      2048

   struct ReadData_data {
      UCHAR        data_area[SECTORSIZE];
   } data;

   ULONG datalen = sizeof(data);
   ULONG cbRead32;

   /* parameter packet */
   struct PARM {
      unsigned char command;
      unsigned char drive;
   } parm2;

   #pragma pack()

   ULONGLONG off;
   ULONG parmlen2 = sizeof(parm2);
   ULONG datalen2 = sizeof(bpb);
   APIRET rc = 0;

   parm2.command = 1;
   parm2.drive = 0;

   if ( (rc = DosDevIOCtl((HFILE)hFile, IOCTL_DISK, DSK_GETDEVICEPARAMS,
                          &parm2, parmlen2, &parmlen2, &bpb, datalen2, &datalen2)) )
   {
       parm2.command = 0;

       if ( (rc = DosDevIOCtl((HFILE)hFile, IOCTL_DISK, DSK_GETDEVICEPARAMS,
                          &parm2, parmlen2, &parmlen2, &bpb, datalen2, &datalen2)) )
           return rc;
   }

   lDataSize = nSectors * BytesPerSector;
   cbRead32 = (lDataSize > BytesPerSector) ? BytesPerSector : (ULONG)lDataSize;

   off =  ulSector + bpb.cHiddenSectors;
   off *= BytesPerSector;

   memset(&parm, 0, sizeof(parm));
   memset(&data, 0, sizeof(data));

   parm.ID_code = ('C') | ('D' << 8) | ('0' << 16) | ('1' << 24);
   parm.address_mode = 0; // lba

   do
   {
       parm.transfer_count = 1;
       parm.start_sector = (ULONG)(off / BytesPerSector);

       rc = DosDevIOCtl((HFILE)hFile, IOCTL_CDROMDISK, CDROMDISK_READDATA,
                        &parm, parmlen, &parmlen, &data, datalen, &datalen);

       if (!rc)
           memcpy((char *)pbSector, &data, cbRead32);
       else
           break;

       off          += cbRead32;
       lDataSize    -= cbRead32;
       pbSector      = (PBYTE)pbSector + cbRead32;

   } while ((lDataSize > 0) && !rc);

   return rc;
}

ULONG WriteSect(HANDLE hf, LONG ulSector, USHORT nSectors, USHORT BytesPerSector, PBYTE pbSector)
{
   ULONG ulDataSize;
   BIOSPARAMETERBLOCK bpb;

   #pragma pack(1)

   /* parameter packet */
   struct {
       unsigned char command;
       unsigned char drive;
   } parm2;

   #pragma pack()

   #define BUFSIZE 0x4000

   char buf[BUFSIZE];

   ULONG parmlen2 = sizeof(parm2);
   ULONG datalen2 = sizeof(bpb);
   ULONG cbWrite32;
   ULONG rc = 0;
   ULONG parmlen4;
   char trkbuf[sizeof(TRACKLAYOUT) + sizeof(USHORT) * 2 * 255];
   PTRACKLAYOUT ptrk = (PTRACKLAYOUT)trkbuf;
   ULONG datalen4 = BUFSIZE;
   USHORT cyl, head, sec, n;
   ULONGLONG off;
   int i;

   parm2.command = 1;
   parm2.drive = 0;

   ulDataSize = nSectors * BytesPerSector;

   if ( (rc = DosDevIOCtl((HFILE)hf, IOCTL_DISK, DSK_GETDEVICEPARAMS,
                          &parm2, parmlen2, &parmlen2, &bpb, datalen2, &datalen2)) )
   {
       parm2.command = 0;

       if ( (rc = DosDevIOCtl((HFILE)hf, IOCTL_DISK, DSK_GETDEVICEPARAMS,
                          &parm2, parmlen2, &parmlen2, &bpb, datalen2, &datalen2)) )
           {
           return rc;
           }
   }

   if (!bpb.cHeads || !bpb.usSectorsPerTrack)
       return ERROR_SECTOR_NOT_FOUND;

   memset((char *)trkbuf, 0, sizeof(trkbuf));
   off =  ulSector + bpb.cHiddenSectors;
   off *= BytesPerSector;

   for (i = 0; i < bpb.usSectorsPerTrack; i++)
   {
       ptrk->TrackTable[i].usSectorNumber = i + 1;
       ptrk->TrackTable[i].usSectorSize = BytesPerSector;
   }

   do
   {
       cbWrite32 = (ulDataSize > BUFSIZE) ? BUFSIZE : (ULONG)ulDataSize;
       parmlen4 = sizeof(TRACKLAYOUT) + sizeof(USHORT) * 2 * (bpb.usSectorsPerTrack - 1);
       datalen4 = BUFSIZE;

       cyl = off / (BytesPerSector * bpb.cHeads * bpb.usSectorsPerTrack);
       head = (off / BytesPerSector - bpb.cHeads * bpb.usSectorsPerTrack * cyl) / bpb.usSectorsPerTrack;
       sec = off / BytesPerSector - bpb.cHeads * bpb.usSectorsPerTrack * cyl - head * bpb.usSectorsPerTrack;

       if (sec + cbWrite32 / BytesPerSector > bpb.usSectorsPerTrack)
           cbWrite32 = (bpb.usSectorsPerTrack - sec) * BytesPerSector;

       n = cbWrite32 / BytesPerSector;

       ptrk->bCommand = 1;
       ptrk->usHead = head;
       ptrk->usCylinder = cyl;
       ptrk->usFirstSector = sec;
       ptrk->cSectors = n;

       memcpy(buf, pbSector, min(datalen4, ulDataSize));

       rc = DosDevIOCtl((HFILE)hf, IOCTL_DISK, DSK_WRITETRACK,
                        trkbuf, parmlen4, &parmlen4, buf, datalen4, &datalen4);

       if (rc)
           break;

       off           += cbWrite32;
       ulDataSize    -= cbWrite32;
       pbSector      = (char *)pbSector + cbWrite32;

    } while ((ulDataSize > 0) && ! rc);

    return rc;
}

ULONG WriteSectCD(HANDLE hFile, LONG ulSector, USHORT nSectors, USHORT BytesPerSector, PBYTE pbSector)
{
   LONG lDataSize;
   BIOSPARAMETERBLOCK bpb;

   #pragma pack(1)

   // parameter packet
   struct WriteData_param {
      ULONG       ID_code;                // 'CD01'
      UCHAR       address_mode;           // Addressing format of start_sector:
                                          //  00 - Logical Block format
                                          //  01 - Minutes/Seconds/Frame format
      USHORT      transfer_count;         // Numbers of sectors to read.
                                          //  Must  be non zero
      ULONG       start_sector;           // Starting sector number of the read operation
      UCHAR       reserved;               // Reserved. Must be 0
      UCHAR       interleave_size;        // Not used. Must be 0
      UCHAR       interleave_skip_factor; // Not used. Must be 0
   } parm;
   ULONG parmlen = sizeof(parm);

   #define SECTORSIZE      2048

   struct WriteData_data {
      UCHAR        data_area[SECTORSIZE];
   } data;

   ULONG datalen = sizeof(data);
   ULONG cbWrite32;

   /* parameter packet */
   struct PARM {
      unsigned char command;
      unsigned char drive;
   } parm2;

   #pragma pack()

   ULONGLONG off;
   ULONG parmlen2 = sizeof(parm2);
   ULONG datalen2 = sizeof(bpb);
   APIRET rc = 0;

   parm2.command = 1;
   parm2.drive = 0;

   if ( (rc = DosDevIOCtl((HFILE)hFile, IOCTL_DISK, DSK_GETDEVICEPARAMS,
                          &parm2, parmlen2, &parmlen2, &bpb, datalen2, &datalen2)) )
   {
       parm2.command = 0;

       if ( (rc = DosDevIOCtl((HFILE)hFile, IOCTL_DISK, DSK_GETDEVICEPARAMS,
                          &parm2, parmlen2, &parmlen2, &bpb, datalen2, &datalen2)) )
           return rc;
   }

   lDataSize = nSectors * BytesPerSector;

   cbWrite32 = (lDataSize > BytesPerSector) ? BytesPerSector : (ULONG)lDataSize;

   off =  ulSector + bpb.cHiddenSectors;
   off *= BytesPerSector;

   memset(&parm, 0, sizeof(parm));
   memset(&data, 0, sizeof(data));

   parm.ID_code = ('C') | ('D' << 8) | ('0' << 16) | ('1' << 24);
   parm.address_mode = 0; // lba

   do
   {
       parm.transfer_count = 1;
       parm.start_sector = (ULONG)(off / BytesPerSector);

       memcpy(&data, (char *)pbSector, cbWrite32);

       rc = DosDevIOCtl((HFILE)hFile, IOCTL_CDROMDISK, CDROMDISK_WRITEDATA,
                        &parm, parmlen, &parmlen, &data, datalen, &datalen);

       if (rc)
           break;

       off           += cbWrite32;
       lDataSize    -= cbWrite32;
       pbSector      = (PBYTE)pbSector + cbWrite32;

   } while ((lDataSize > 0) && !rc);

   return rc;
}

// check if the driveletter is cdrom one
BYTE get_medium_type(char *pszFilename)
{
    // detect CD
    typedef struct _CDROMDeviceMap
    {
        USHORT usDriveCount;
        USHORT usFirstLetter;
    } CDROMDeviceMap;

    HFILE hf;
    ULONG ulAction = 0;
    CDROMDeviceMap CDMap;
    ULONG ulParamSize = sizeof(ulAction);
    ULONG ulDataSize = sizeof(CDROMDeviceMap);
    char driveName[3] = { '?', ':', '\0' };
    int drv;

    if (DosOpen("\\DEV\\CD-ROM2$", &hf, &ulAction, 0, FILE_NORMAL,
                OPEN_ACTION_FAIL_IF_NEW | OPEN_ACTION_OPEN_IF_EXISTS,
                OPEN_ACCESS_READONLY | OPEN_SHARE_DENYNONE, NULL))
        return MEDIUM_TYPE_DASD;

    DosDevIOCtl(hf, IOCTL_CDROMDISK2, CDROMDISK2_DRIVELETTERS,
                NULL, 0, &ulParamSize,
                (PVOID)&CDMap, sizeof(CDROMDeviceMap), &ulDataSize);
    DosClose(hf);

    for (drv = CDMap.usFirstLetter; drv < CDMap.usFirstLetter + CDMap.usDriveCount; drv++)
    {
        driveName[0] = 'A' + drv;
        if (!stricmp(driveName, pszFilename))
            return MEDIUM_TYPE_CDROM;
    }

    // detect floppy
    {
        /* parameter packet */
        #pragma pack(1)
        struct {
            unsigned char command;
            unsigned char drive;
        } parm;
        #pragma pack()
        /* data packet      */
        BIOSPARAMETERBLOCK bpb;
        ULONG num = 0, map = 0;
        char driveName[3] = { '?', ':', '\0' };
        APIRET rc;

        DosQueryCurrentDisk(&num, &map);

        for (drv = 0; drv < 'z' - 'a' + 1; drv++)
        {
            char szDevName[4] = "A:";
            char fsqbuf2[sizeof(FSQBUFFER2) + 3 * CCHMAXPATH];
            PFSQBUFFER2 pfsqbuf2 = (PFSQBUFFER2)fsqbuf2;
            ULONG cbData;
            ULONG parmlen = sizeof(parm);
            ULONG datalen = sizeof(bpb);

            driveName[0] = 'A' + drv;

            // skip if the drive is not in map
            if ((map & (drv + 1)) == 0)
                continue;

            parm.command = 0;
            parm.drive = drv;

            // skip if we fail to get an BPB
            if ( (rc = DosDevIOCtl(-1, IOCTL_DISK, DSK_GETDEVICEPARAMS,
                              &parm, parmlen, &parmlen, &bpb, datalen, &datalen)) )
                continue;

            switch (bpb.bDeviceType)
            {
                case 5: // Fixed disk
                case 6: // Tape drive
                    continue;

                case 8: // R/W optical disk
                    if (!stricmp(szDevName, pszFilename))
                        return MEDIUM_TYPE_FLOPPY;

                case 7: // Other (includes 3.5" diskette drive)
                    szDevName[0] += drv;
                    cbData = sizeof(fsqbuf2);
                    // disable error popups
                    DosError(FERR_DISABLEEXCEPTION | FERR_DISABLEHARDERR);
                    if (! DosQueryFSAttach(szDevName, 0L, FSAIL_QUERYNAME, (PFSQBUFFER2)&fsqbuf2, &cbData) )
                    {
                        if (pfsqbuf2->iType == FSAT_REMOTEDRV)
                            continue; // skip remote drives
                        else if (pfsqbuf2->iType == FSAT_LOCALDRV)
                        {
                            PSZ pszFSName = (PSZ)pfsqbuf2->szName + pfsqbuf2->cbName + 1;
                            if (!strstr(pszFSName, "FAT") || bpb.usBytesPerSector != 512) // optical
                                continue; // skip non-fat drives
                        }
                    }
                    // enable error popups
                    DosError(FERR_ENABLEEXCEPTION | FERR_ENABLEHARDERR);
                    break;
                    
                default:
                    break;
            }

            if (!stricmp(szDevName, pszFilename))
                return MEDIUM_TYPE_FLOPPY;

        }
    }

    // DASD
    return MEDIUM_TYPE_DASD;
}

void low_level_fmt_floppy(HANDLE hDevice, format_params *params)
{
  #pragma pack(1)

  struct FmtVerify_param {
   UCHAR        CmdInfo;
   USHORT       Head;
   USHORT       Cylinder;
   USHORT       TracksNo;
   USHORT       SectorsNo;
   struct _tuple
   {
     BYTE  c;
     BYTE  h;
     BYTE  s;
     BYTE  n;
   }        FormatTrackTable[1];
  };

  char *parm;
  struct FmtVerify_param *parm2;

  struct FmtVerify_data {
   UCHAR        StrtSector;
  } data;

  ULONG parmlen;
  ULONG datalen;

  APIRET rc;
  int i, j, k;

  struct {
   UCHAR command;
   UCHAR drive;
  } parm1;

  BIOSPARAMETERBLOCK bpb;

  #pragma pack()

  parmlen = sizeof(parm1);
  datalen = sizeof(bpb);
  parm1.command = 0;
  parm1.drive   = 0;

  if ( rc = DosDevIOCtl((HFILE)hDevice, IOCTL_DISK, DSK_GETDEVICEPARAMS,
                        &parm1, parmlen, &parmlen, &bpb, datalen, &datalen) )
     {
     printf("GetDeviceParams rc=%u\n", rc);
     return;
     }

  parm1.command = 2;
  bpb.cCylinders = params->tracks;
  bpb.usSectorsPerTrack = params->sectors_per_track;
  bpb.cHeads = 2;

  if ( rc = DosDevIOCtl((HFILE)hDevice, IOCTL_DISK, DSK_SETDEVICEPARAMS,
                        &parm1, parmlen, &parmlen, &bpb, datalen, &datalen) )
     {
     printf("SetDeviceParams rc=%u\n", rc);
     return;
     }

  parmlen = sizeof(struct FmtVerify_param) + 4 * (params->sectors_per_track - 1);
  datalen = sizeof(struct FmtVerify_data);

  parm  = (char *)malloc(parmlen);
  parm2 = (struct FmtVerify_param *)parm;
  parm2->CmdInfo = 1;

  data.StrtSector = 0;

  for (j = 0; j < 2; j++)
  {
    parm2->Head = j;

    for (i = 0; i < params->tracks; i++)
    {
      parm2->Cylinder = i;
      parm2->TracksNo = 0;
      parm2->SectorsNo = params->sectors_per_track;

      for (k = 0; k < params->sectors_per_track; k++)
      {
        parm2->FormatTrackTable[k].c = i;
        parm2->FormatTrackTable[k].h = j;
        parm2->FormatTrackTable[k].s = k + 1;
        parm2->FormatTrackTable[k].n = 2;
      }

      rc = DosDevIOCtl((HFILE)hDevice, IOCTL_DISK, DSK_FORMATVERIFY,
                       parm, parmlen, &parmlen,
                       &data, datalen, &datalen);
      if (rc)
      {
        printf("FormatVerify rc=%u\n", rc);
      }

      DosSleep(1000);
      if (! rc)
         show_progress( (float)((j * params->tracks + (i + 1)) * 100 / (params->tracks * 2)) );
    }
  }

  free(parm);
  DosSleep(4000);
}

void low_level_fmt_cd(HANDLE hDevice, format_params *params)
{
  struct FmtVerify_param {
   UCHAR        Command;  // Bit 7: 0 - start/cancel formatting, 1 - get format status
                          // Bit 0: 0 - start formatting, 1 - cancel formatting
                          // Bit 1: 1 - Mount Rainier
                          // Bit 2: 1 - wait for background formatting complete
                          // Bit 3: 1 - resume background formatting
  } parm;

  struct FmtVerify_data {
   UCHAR        Status;   // Percent of formatted volume, if supported such feature
                          // 0, if not supported
  } data;

  ULONG parmlen = sizeof(struct FmtVerify_param);
  ULONG datalen = sizeof(struct FmtVerify_data);
  APIRET rc;

  parm.Command = 0;
  data.Status  = 0;

  rc = DosDevIOCtl((HFILE)hDevice, IOCTL_DISK, DSK_FORMATVERIFY,
                   &parm, parmlen, &parmlen,
                   &data, datalen, &datalen);

  if (rc)
     return;

  parm.Command = (1 << 7);

  do
  {
     rc = DosDevIOCtl((HFILE)hDevice, IOCTL_DISK, DSK_FORMATVERIFY,
                   &parm, parmlen, &parmlen,
                   &data, datalen, &datalen);

     DosSleep(1000);
     if (! rc)
        show_progress ((float)data.Status);
  } while (data.Status < 100 && ! rc);

  DosSleep(7000);
}

void open_drive (char *path, HANDLE *hDevice)
{
  APIRET rc;
  ULONG  ulAction;
  char DriveDevicePath[]="Z:"; // for DosOpen
  char *p = path;
  ULONG ulFlags = 0;

  if (path[1] == ':' && path[2] == '\0')
  {
    // drive letter (UNC)
    DriveDevicePath[0] = *path;
    p = DriveDevicePath;
    ulFlags = OPEN_FLAGS_DASD;
  }

  rc = DosOpen( p,               // filename
	      (HFILE *)hDevice,  // handle returned
              &ulAction,         // action taken by DosOpen
              0,                 // cbFile
              0,                 // ulAttribute
              OPEN_ACTION_OPEN_IF_EXISTS, // | OPEN_ACTION_REPLACE_IF_EXISTS,
              OPEN_FLAGS_FAIL_ON_ERROR |  // OPEN_FLAGS_WRITE_THROUGH | 
              OPEN_SHARE_DENYREADWRITE |  // OPEN_FLAGS_NO_CACHE  |
              OPEN_ACCESS_READWRITE    | ulFlags,
              NULL);                 // peaop2

  if ( rc != 0 || hDevice == 0 )
      die( "Failed to open device - close any files before formatting,\n"
           "and make sure you have Admin rights when using fat32format\n"
           "Are you SURE you're formatting the RIGHT DRIVE!!!", rc );

  hDev = *hDevice;
}

void lock_drive(HANDLE hDevice)
{
  unsigned char  parminfo  = 0;
  unsigned char  datainfo  = 0;
  unsigned long  parmlen   = 1;
  unsigned long  datalen   = 1;
  APIRET rc;

  rc = DosDevIOCtl( (HFILE)hDevice, IOCTL_DISK, DSK_LOCKDRIVE, &parminfo,
                    parmlen, &parmlen, &datainfo, // Param packet
                    datalen, &datalen);

  if ( rc )
      die( "Failed to lock device" , rc);

}

void unlock_drive(HANDLE hDevice)
{
  unsigned char  parminfo = 0;
  unsigned char  datainfo = 0;
  unsigned long  parmlen  = 1;
  unsigned long  datalen  = 1;
  APIRET rc;

  rc = DosDevIOCtl( (HFILE)hDevice, IOCTL_DISK, DSK_UNLOCKDRIVE, &parminfo,
                    parmlen, &parmlen, &datainfo, // Param packet
                    datalen, &datalen);

  if ( rc ) 
  {
      show_message( "WARNING: Failed to unlock device, rc = %lu!\n" , 0, 0, 1, rc );
      show_message( "WARNING: probably, you need to reboot for changes to take in effect.\n", 0, 0, 0);
  }
}

/* parameter packet */
#pragma pack(1)
struct PARM {
    unsigned char command;
    unsigned char drive;
};
#pragma pack()

BOOL get_drive_params(HANDLE hDevice, struct extbpb *dp)
{
  APIRET rc;
  struct PARM        p;
  BIOSPARAMETERBLOCK d; /* DDK/Watcom OS/2 headers */
  ULONG  parmlen;
  ULONG  datalen;

  memset(&d, 0, sizeof(d));
  parmlen = sizeof(p);
  datalen = sizeof(d);
  p.command = 1;
  p.drive   = 0;

  rc = DosDevIOCtl( (HFILE)hDevice, IOCTL_DISK, DSK_GETDEVICEPARAMS,
                      &p, parmlen, &parmlen,
                      &d, datalen, &datalen);

  if (rc)
     {
     p.command = 0; // try recommended BPB for the 2nd time
     rc = DosDevIOCtl( (HFILE)hDevice, IOCTL_DISK, DSK_GETDEVICEPARAMS,
                       &p, parmlen, &parmlen,
                       &d, datalen, &datalen);
     }

  if ( rc )
      die( "Failed to get device geometry", rc );

  dp->BytesPerSect = d.usBytesPerSector;
  dp->SectorsPerTrack = d.usSectorsPerTrack;
  dp->HiddenSectors = d.cHiddenSectors;
  dp->TracksPerCylinder = d.cHeads;

  if ((d.cSectors == 0) && (d.cLargeSectors))
      dp->TotalSectors = d.cLargeSectors;
  else if ((d.cSectors) && (d.cLargeSectors == 0))
      dp->TotalSectors = d.cSectors;

  // no GPT support yet on OS/2
  return FALSE;
}

void set_part_type(HANDLE hDevice, struct extbpb *dp, int type)
{
  APIRET rc;
  int i;

  if (! dp->HiddenSectors)
    return;

  rc = ReadSect(hDevice, -dp->HiddenSectors, 1, dp->BytesPerSect, (char *)&mbr);

  if (rc)
      die("Error reading MBR\n", rc);

  for (i = 0; i < 4; i++)
  {
    if (mbr.pte[i].relative_sector == dp->HiddenSectors)
    {
      // set type to FAT32
      mbr.pte[i].system_id = type;
      rc = WriteSect(hDevice, -dp->HiddenSectors, 1, dp->BytesPerSect, (char *)&mbr);

      if (rc)
          die("Error writing MBR\n", rc);

      break;
    }
  }
}

void begin_format (HANDLE hDevice, BOOL fImage)
{
  // Detach the volume from the old FSD 
  // and attach to the new one
  unsigned char  cmdinfo     = 0;
  unsigned long  parmlen     = sizeof(cmdinfo);
  unsigned char  *datainfo   = FS_NAME; // FSD name
  unsigned long  datalen     = sizeof(datainfo); 
  unsigned char  rgFirstInfo[256];
  unsigned long  ulParmSize, ulDataSize;
  APIRET rc;

   rc = DosFSCtl( rgFirstInfo, sizeof( rgFirstInfo ), &ulDataSize,
                  NULL, 0, &ulParmSize,
                  FAT32_GETFIRSTINFO, "UNIFAT", -1, FSCTL_FSDNAME );

   if (rc != ERROR_INVALID_FSD_NAME)
      {
      // use "UNIFAT" instead of "FAT32" as FS name
      strcpy(FS_NAME, "UNIFAT");
      }

  if (fImage)
     {
     rc = DosDevIOCtl( (HFILE)hDevice, IOCTL_FAT32, FAT32_BEGINFORMAT, &cmdinfo, 
                       parmlen, &parmlen, datainfo,
                       datalen, &datalen);
     }
  else
     {
     rc = DosDevIOCtl( (HFILE)hDevice, IOCTL_DISK, DSK_BEGINFORMAT, &cmdinfo, 
                       parmlen, &parmlen, datainfo,
                       datalen, &datalen);
     }

  if ( rc )
      die( "Failed to begin format device", rc );
}

void remount_media (HANDLE hDevice, BOOL fImage)
{
  // Redetermine media
  unsigned char parminfo  = 0;
  unsigned char datainfo  = 0;
  unsigned long parmlen   = 1;
  unsigned long datalen   = 1;
  APIRET rc;

  if (fImage)
     {
     rc = DosDevIOCtl( (HFILE)hDevice, IOCTL_FAT32, FAT32_REDETERMINEMEDIA,
                       &parminfo, parmlen, &parmlen, // Param packet
                       &datainfo, datalen, &datalen);
     }
  else
     {
     rc = DosDevIOCtl( (HFILE)hDevice, IOCTL_DISK, DSK_REDETERMINEMEDIA,
                       &parminfo, parmlen, &parmlen, // Param packet
                       &datainfo, datalen, &datalen);
     }

  if ( rc ) 
  {
      show_message( "WARNING: Failed to do final remount, rc = %lu!\n" , 0, 0, 1, rc );
      show_message( "WARNING: probably, you need to reboot for changes to take in effect.\n", 0, 0, 0 );
  }
}

void close_drive(HANDLE hDevice)
{
    // Close Device
    DosClose ( (HFILE)hDevice );
}

void cleanup ( void )
{
    static int num_called = 0;

    num_called++;

    if (num_called > 1)
        return;

    if (hDev == 0)
        return;

    remount_media ( hDev, fDiskImage );
    unlock_drive ( hDev );
    close_drive ( hDev);
    hDev = 0;
}


void startlw(HANDLE hDevice)
{
   ULONG ulDataSize = 0;
   ULONG ulParmSize = 0;
   APIRET rc;

   rc = DosFSCtl(NULL, 0, &ulDataSize,
                 NULL, 0, &ulParmSize,
                 FAT32_STARTLW, FS_NAME, -1,
                 FSCTL_FSDNAME);

   if (rc)
   {
     show_message("Error %lu doing FAT32_STARTLW.\n", 0, 0, 1, rc);
     show_message("%s\n", 0, 0, 1, get_error(rc));
     return;
   }
}

void stoplw(HANDLE hDevice)
{
   ULONG ulDataSize = 0;
   ULONG ulParmSize = 0;
   APIRET rc;

   rc = DosFSCtl(NULL, 0, &ulDataSize,
                 NULL, 0, &ulParmSize,
                 FAT32_STOPLW, FS_NAME, -1,
                 FSCTL_FSDNAME);

   if (rc)
   {
     show_message("Error %lu doing FAT32_STOPLW.\n", 0, 0, 1, rc);
     show_message("%s\n", 0, 0, 1, get_error(rc));
     return;
   }
}

int mem_alloc(void **p, ULONG cb)
{
    APIRET rc;

    if (!(rc = DosAllocMem ( (void **)p, cb, PAG_COMMIT | PAG_READ | PAG_WRITE)))
        memset(*p, 0, cb);
    else
        show_message("mem_alloc failed, rc=%lu\n", 0, 0, 1, rc);

    return rc;
}

int mem_alloc2(void **p, ULONG cb)
{
    APIRET rc;
    ULONG  ulParm = cb;
    ULONG  cbParm = sizeof(ulParm);
    struct
    {
        LONG  lRc;
        PVOID pMem;
        ULONG ulLen;
    } Data;
    ULONG cbData = sizeof(Data);
    HFILE hf;
    ULONG ulAction;

    printf("cbParm=%lu, cbData=%lu\n", cbParm, cbData);

    rc = DosOpen("\\DEV\\CHKDSK$",
                 &hf,
                 &ulAction,
                 0,
                 0,
                 OPEN_ACTION_OPEN_IF_EXISTS,
                 OPEN_FLAGS_FAIL_ON_ERROR | OPEN_SHARE_DENYREADWRITE |
                 OPEN_ACCESS_READWRITE,
                 NULL);

    printf("DosOpen rc=%lu\n", rc);
    if (rc)
        return rc;

    memset(&Data, 0, sizeof(Data));
    Data.lRc  = -1;

    rc = DosDevIOCtl(hf, 0x80, 0x28,
                     &ulParm, cbParm, &cbParm,
                     &Data, cbData, &cbData);

    DosClose(hf);
    *p = Data.pMem;
    printf("lRc=%ld\n", Data.lRc);
    printf("pMem=%lu\n", Data.pMem);
    printf("ulLen=%lu\n", Data.ulLen);
    printf("DosDevIOCtl rc=%lu\n", rc);

    return rc;
}

void mem_free(void *p, ULONG cb)
{
    if (p)
       DosFreeMem(p);
}

void query_freq(ULONGLONG *freq)
{
    *freq = 0;
    DosTmrQueryFreq( (ULONG *)freq );
}

void query_time(ULONGLONG *time)
{
    DosTmrQueryTime( (QWORD *)time );
}

ULONG query_vol_label(char *path, char *pszVolLabel, int cbVolLabel)
{
    /* file system information buffer */
    FSINFO fsiBuffer;
    APIRET rc;

    // Query the filesystem info, 
    // including the current volume label
    if (fDiskImage)
       {
       ULONG ulDataSize = sizeof(fsiBuffer);
       ULONG ulParmSize = strlen(path) + 1;

       rc = DosFSCtl( (PVOID)&fsiBuffer, ulDataSize, &ulDataSize,
                      path, ulParmSize, &ulParmSize,
                      FAT32_GETVOLLABEL, "FAT32", -1, FSCTL_FSDNAME );
       }
    else
       {
       rc = DosQueryFSInfo((toupper(path[0]) - 'A' + 1), FSIL_VOLSER,
                           (PVOID)&fsiBuffer, sizeof(fsiBuffer));
       }

    if (rc)
        return rc;

    if (cbVolLabel < fsiBuffer.vol.cch)
        return ERROR_BUFFER_OVERFLOW;

    // Current disk volume label
    strncpy(pszVolLabel, fsiBuffer.vol.szVolLabel, fsiBuffer.vol.cch);
    return 0;
}

void set_vol_label (char *path, char *vol)
{
  VOLUMELABEL vl = {0};
  APIRET rc;
  int diskno;
  int l;

  if (!vol || !*vol)
    return;

  l = strlen(vol);

  if (!path || !*path)
    return;

  diskno = toupper(*path) - 'A' + 1;

  memset(&vl, 0, sizeof(VOLUMELABEL));
  strcpy(vl.szVolLabel, vol);
  vl.cch = strlen(vl.szVolLabel);

    if (fDiskImage)
       {
       ULONG ulDataSize = sizeof(vl);
       ULONG ulParmSize = strlen(path) + 1;

       rc = DosFSCtl( (PVOID)&vl, ulDataSize, &ulDataSize,
                      path, ulParmSize, &ulParmSize,
                      FAT32_SETVOLLABEL, "FAT32", -1, FSCTL_FSDNAME );
       }
    else
       {
       rc = DosSetFSInfo(diskno, FSIL_VOLSER,
                         (PVOID)&vl, sizeof(VOLUMELABEL));
       }

  if ( rc )
    show_message ("WARNING: failed to set the volume label, rc=%lu\n", 0, 0, 1, rc);
}


void set_datetime(DIRENTRY *pDir)
{
   // FAT12/FAT16/FAT32
   DATETIME datetime;

   DosGetDateTime(&datetime);

   pDir->wLastWriteDate.year = datetime.year - 1980;
   pDir->wLastWriteDate.month = datetime.month;
   pDir->wLastWriteDate.day = datetime.day;
   pDir->wLastWriteTime.hours = datetime.hours;
   pDir->wLastWriteTime.minutes = datetime.minutes;
   pDir->wLastWriteTime.twosecs = datetime.seconds / 2;

   pDir->wCreateDate = pDir->wLastWriteDate;
   pDir->wCreateTime = pDir->wLastWriteTime;
   pDir->wAccessDate = pDir->wLastWriteDate;
}

void set_datetime1(DIRENTRY1 *pDir)
{
   // exFAT
   DATETIME datetime;

   DosGetDateTime(&datetime);

   pDir->u.File.ulLastModifiedTimestp.year = datetime.year - 1980;
   pDir->u.File.ulLastModifiedTimestp.month = datetime.month;
   pDir->u.File.ulLastModifiedTimestp.day = datetime.day;
   pDir->u.File.ulLastModifiedTimestp.hour = datetime.hours;
   pDir->u.File.ulLastModifiedTimestp.minutes = datetime.minutes;
   pDir->u.File.ulLastModifiedTimestp.twosecs = datetime.seconds / 2;

   pDir->u.File.ulCreateTimestp = pDir->u.File.ulLastModifiedTimestp;
   pDir->u.File.ulLastAccessedTimestp = pDir->u.File.ulLastModifiedTimestp;
}


char *get_error(USHORT rc)
{
   return GetOS2Error(rc);
}


void query_current_disk(char *pszDrive)
{
   ULONG  ulDisk;
   ULONG  ulDrives;
   APIRET rc;

   rc = DosQueryCurrentDisk(&ulDisk, &ulDrives);

   pszDrive[0] = (BYTE)(ulDisk + '@');
   pszDrive[1] = ':';
   pszDrive[2] = '\0';
}


#ifdef __UNICODE__
BOOL IsDBCSLead( UCHAR uch)
{
    return ( rgFirstInfo[ uch ] == 2 );
}


VOID Translate2OS2(PUSHORT pusUni, PSZ pszName, USHORT usLen)
{
   USHORT usPage;
   USHORT usChar;
   USHORT usCode;

   while (*pusUni && usLen)
      {
      usPage = ((*pusUni) >> 8) & 0x00FF;
      usChar = (*pusUni) & 0x00FF;

      usCode = QueryUni2NLS( usPage, usChar );
      *pszName++ = ( BYTE )( usCode & 0x00FF );
      if( usCode & 0xFF00 )
         {
         *pszName++ = ( BYTE )(( usCode >> 8 ) & 0x00FF );
         usLen--;
         }

      pusUni++;
      usLen--;
      }
}


USHORT Translate2Win(PSZ pszName, PUSHORT pusUni, USHORT usLen)
{
   USHORT usCode;
   USHORT usProcessedLen;

   usProcessedLen = 0;

   while (*pszName && usLen)
      {
      usCode = *pszName++;
      if( IsDBCSLead(( UCHAR )usCode ))
         {
         usCode |= (( USHORT )*pszName++ << 8 ) & 0xFF00;
         usProcessedLen++;
         }

      *pusUni++ = QueryNLS2Uni( usCode );
      usLen--;
      usProcessedLen++;
      }

   return usProcessedLen;
}
#endif


BOOL OutputToFile(HANDLE hFile)
{
ULONG rc;
ULONG ulType;
ULONG ulAttr;

   rc = DosQueryHType((HFILE)hFile, &ulType, &ulAttr);

   switch (ulType & 0x000F)
      {
      case 0:
         return TRUE;

      case 1:
      case 2:
         return FALSE;
/*
      default:
         return FALSE;
*/
      }

   return FALSE;
}


int show_message (char *pszMsg, unsigned short usLogMsg, unsigned short usMsg, unsigned short usNumFields, ...)
{
    va_list va, va2;
    UCHAR szBuf[1024];
    int len, i;
    char *dummy;

    va_start(va, usNumFields);
    va_copy(va2, va);

    if (usMsg)
       {
       // output message from oso001.msg to screen
       len = iShowMessage2(NULL, usMsg, usNumFields, va);
       }
    else if (pszMsg)
       {
       // output message which is not in oso001.msg
       vsprintf ( szBuf, pszMsg, va );
       printf ( "%s", szBuf );
       len = strlen(szBuf);
       }

    // output message to CHKDSK log
    if (usLogMsg)
       {
       PSZ pszString = NULL;
       ULONG ulParmNo = usNumFields;

       if (pszMsg && strstr(pszMsg, "%1"))
          {
          ULONG arg = va_arg(va2, ULONG);
          if (usMsg && arg == TYPE_STRING)
             pszString = va_arg(va2, PSZ);
          }
       else if (pszMsg && strstr(pszMsg, "%s"))
          pszString = (PSZ)va_arg(va2, PSZ);

       if (pszString)
          ulParmNo--;

       LogOutMessagePrintf(usLogMsg, pszString, ulParmNo, va2);
       }

    va_end(va2);
    va_end(va);

    // output additional message for /P switch specified
    if (usMsg && msg)
       {
       va_start(va, usNumFields);
       printf("\nMSG_%u", usMsg);

       for (i = 0; i < usNumFields; i++)
          {
          USHORT usType = va_arg(va, USHORT);
          if (usType == TYPE_PERC)
             {
             USHORT arg = va_arg(va, USHORT);
             printf(",%u", arg);
             }
          else if (usType == TYPE_LONG || usType == TYPE_LONG2)
             {
             ULONG arg = va_arg(va, ULONG);
             printf(",%lu", arg);
             }
          else if (usType == TYPE_DOUBLE || usType == TYPE_DOUBLE2)
             {
             double arg = va_arg(va, double);
             printf(",%.0lf", arg);
             }
          else if (usType == TYPE_STRING)
             {
             PSZ arg = va_arg(va, PSZ);
             printf(",%s", arg);
             }
          }

       printf("\n");
       va_end( va );
       }

    return len;
}
