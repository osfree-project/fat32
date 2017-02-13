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

typedef HFILE HANDLE;

extern HANDLE hDev;

char msg = FALSE;

void LogOutMessagePrintf(ULONG ulMsgNo, char *psz, ULONG ulParmNo, va_list va);
PSZ       GetOS2Error(USHORT rc);

ULONG ReadSect(HFILE hFile, LONG ulSector, USHORT nSectors, PBYTE pbSector);
ULONG WriteSect(HFILE hf, LONG ulSector, USHORT nSectors, PBYTE pbSector);

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
    show_message("%s\n", 0, 0, 1, GetOS2Error(rc));

    if ( rc )
        show_message("Error code: %lu\n", 0, 0, 1, rc);

    quit ( rc );
}

void seek_to_sect( HANDLE hDevice, DWORD Sector, DWORD BytesPerSect )
{
    LONGLONG llOffset, llActual;
    APIRET rc;
    
    llOffset = Sector * BytesPerSect;
    rc = DosSetFilePtrL( hDevice, (LONGLONG)llOffset, FILE_BEGIN, &llActual );
    //printf("seek_to_sect: hDevice=%lx, Sector=%lu, BytesPerSect=%lu\n", hDevice, Sector, BytesPerSect);
    //printf("rc=%lu\n", rc);

    if ( rc )
        die("Seek error", rc);
}

ULONG ReadSect(HFILE hFile, LONG ulSector, USHORT nSectors, PBYTE pbSector)
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

   ulDataSize = nSectors * SECTOR_SIZE;

   if ( (rc = DosDevIOCtl(hFile, IOCTL_DISK, DSK_GETDEVICEPARAMS,
                          &parm2, parmlen2, &parmlen2, &bpb, datalen2, &datalen2)) )
       return rc;

   memset((char *)trkbuf, 0, sizeof(trkbuf));
   off =  ulSector + bpb.cHiddenSectors;
   off *= SECTOR_SIZE;

   for (i = 0; i < bpb.usSectorsPerTrack; i++)
   {
       ptrk->TrackTable[i].usSectorNumber = i + 1;
       ptrk->TrackTable[i].usSectorSize = SECTOR_SIZE;
   }

   do
   {
       cbRead32 = (ulDataSize > BUFSIZE) ? BUFSIZE : (ULONG)ulDataSize;
       parmlen4 = sizeof(TRACKLAYOUT) + sizeof(USHORT) * 2 * (bpb.usSectorsPerTrack - 1);
       datalen4 = BUFSIZE;

       cyl = off / (SECTOR_SIZE * bpb.cHeads * bpb.usSectorsPerTrack);
       head = (off / SECTOR_SIZE - bpb.cHeads * bpb.usSectorsPerTrack * cyl) / bpb.usSectorsPerTrack;
       sec = off / SECTOR_SIZE - bpb.cHeads * bpb.usSectorsPerTrack * cyl - head * bpb.usSectorsPerTrack;

       if (sec + cbRead32 / SECTOR_SIZE > bpb.usSectorsPerTrack)
           cbRead32 = (bpb.usSectorsPerTrack - sec) * SECTOR_SIZE;

       n = cbRead32 / SECTOR_SIZE;

       //printf("cyl=%u, head=%u, sec=%u, n=%u, ", cyl, head, sec, n);

       ptrk->bCommand = 1;
       ptrk->usHead = head;
       ptrk->usCylinder = cyl;
       ptrk->usFirstSector = sec;
       ptrk->cSectors = n;

       rc = DosDevIOCtl(hFile, IOCTL_DISK, DSK_READTRACK,
                        trkbuf, parmlen4, &parmlen4, buf, datalen4, &datalen4);

       //printf("rc=%lu\n", rc);

       if (rc)
           break;

       memcpy(pbSector, buf, min(datalen4, ulDataSize));

       off           += cbRead32;
       ulDataSize    -= cbRead32;
       pbSector      = (char *)pbSector + cbRead32;

    } while ((ulDataSize > 0) && ! rc);

    return rc;
}

ULONG WriteSect(HFILE hf, LONG ulSector, USHORT nSectors, PBYTE pbSector)
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
   ULONG rc;
   ULONG parmlen4;
   char trkbuf[sizeof(TRACKLAYOUT) + sizeof(USHORT) * 2 * 255];
   PTRACKLAYOUT ptrk = (PTRACKLAYOUT)trkbuf;
   ULONG datalen4 = BUFSIZE;
   USHORT cyl, head, sec, n;
   ULONGLONG off;
   int i;

   ulDataSize = nSectors * SECTOR_SIZE;

   //if (pVolInfo->fWriteProtected)
   //   return ERROR_WRITE_PROTECT;

   //if (pVolInfo->fDiskClean)
   //   MarkDiskStatus(pVolInfo, FALSE);

   if ( (rc = DosDevIOCtl(hf, IOCTL_DISK, DSK_GETDEVICEPARAMS,
                          &parm2, parmlen2, &parmlen2, &bpb, datalen2, &datalen2)) )
       return rc;

   memset((char *)trkbuf, 0, sizeof(trkbuf));
   off =  ulSector + bpb.cHiddenSectors;
   off *= SECTOR_SIZE;

   for (i = 0; i < bpb.usSectorsPerTrack; i++)
   {
       ptrk->TrackTable[i].usSectorNumber = i + 1;
       ptrk->TrackTable[i].usSectorSize = SECTOR_SIZE;
   }

   do
   {
       cbWrite32 = (ulDataSize > BUFSIZE) ? BUFSIZE : (ULONG)ulDataSize;
       parmlen4 = sizeof(TRACKLAYOUT) + sizeof(USHORT) * 2 * (bpb.usSectorsPerTrack - 1);
       datalen4 = BUFSIZE;

       cyl = off / (SECTOR_SIZE * bpb.cHeads * bpb.usSectorsPerTrack);
       head = (off / SECTOR_SIZE - bpb.cHeads * bpb.usSectorsPerTrack * cyl) / bpb.usSectorsPerTrack;
       sec = off / SECTOR_SIZE - bpb.cHeads * bpb.usSectorsPerTrack * cyl - head * bpb.usSectorsPerTrack;

       if (sec + cbWrite32 / SECTOR_SIZE > bpb.usSectorsPerTrack)
           cbWrite32 = (bpb.usSectorsPerTrack - sec) * SECTOR_SIZE;

       n = cbWrite32 / SECTOR_SIZE;

       //printf("cyl=%u, head=%u, sec=%u, n=%u, ", cyl, head, sec, n);

       ptrk->bCommand = 1;
       ptrk->usHead = head;
       ptrk->usCylinder = cyl;
       ptrk->usFirstSector = sec;
       ptrk->cSectors = n;

       memcpy(buf, pbSector, min(datalen4, ulDataSize));

       rc = DosDevIOCtl(hf, IOCTL_DISK, DSK_WRITETRACK,
                        trkbuf, parmlen4, &parmlen4, buf, datalen4, &datalen4);

       //printf("rc=%lu\n", rc);

       if (rc)
           break;

       off           += cbWrite32;
       ulDataSize    -= cbWrite32;
       pbSector      = (char *)pbSector + cbWrite32;

    } while ((ulDataSize > 0) && ! rc);

    return rc;
}

void write_sect ( HANDLE hDevice, DWORD Sector, DWORD BytesPerSector, void *Data, DWORD NumSects )
{
    DWORD dwWritten;
    ULONG ret;

    //seek_to_sect ( hDevice, Sector, BytesPerSector );
    //ret = DosWrite ( hDevice, Data, NumSects * BytesPerSector, (PULONG)&dwWritten );
    ret = WriteSect( hDevice, Sector, NumSects, Data);

    //printf("write_sect: ret = %lu\n", ret);

    if ( ret )
        die ( "Failed to write", ret );
}

BOOL write_file ( HANDLE hDevice, BYTE *pData, DWORD ulNumBytes, DWORD *dwWritten )
{
    BOOL  ret = TRUE;
    ULONG rc = 0;

    assert(! (ulNumBytes % 512));

    if ( rc = DosWrite ( hDevice, pData, (ULONG)ulNumBytes >> 9, (PULONG)dwWritten ) )
        ret = FALSE;

    if (ret)
        *dwWritten <<= 9;

    //printf("write_file: ulNumBytes=%lu, rc=%lu\n", ulNumBytes, rc);

    //printf("write_file: hDevice=%lu, pData=0x%lx, ulNumBytes=%lu, dwWritten=0x%lx\n", 
    //        hDevice, pData, ulNumBytes, dwWritten);
    //printf("write_file: rc = %lu, *dwWritten=%lu\n", rc, *dwWritten); // 87 == ERROR_INVALID_PARAMETER

    return ret;
}

void open_drive (char *path, HANDLE *hDevice)
{
  APIRET rc;
  ULONG  ulAction;
  char DriveDevicePath[]="Z:"; // for DosOpenL
  char *p = path;

  if (path[1] == ':' && path[2] == '\0')
  {
    // drive letter (UNC)
    DriveDevicePath[0] = *path;
    p = DriveDevicePath;
  }

  rc = DosOpenL( p,              // filename
	      hDevice,           // handle returned
              &ulAction,         // action taken by DosOpenL
              0,                 // cbFile
              0,                 // ulAttribute
              OPEN_ACTION_OPEN_IF_EXISTS, // | OPEN_ACTION_REPLACE_IF_EXISTS,
              OPEN_FLAGS_FAIL_ON_ERROR |  // OPEN_FLAGS_WRITE_THROUGH | 
              OPEN_SHARE_DENYREADWRITE |  // OPEN_FLAGS_NO_CACHE  |
              OPEN_ACCESS_READWRITE    | OPEN_FLAGS_DASD,
              NULL);                 // peaop2

     //      OPEN_ACTION_OPEN_IF_EXISTS, OPEN_FLAGS_DASD |
     //      OPEN_FLAGS_FAIL_ON_ERROR | OPEN_SHARE_DENYREADWRITE |

     //!     OPEN_ACTION_FAIL_IF_NEW | OPEN_ACTION_OPEN_IF_EXISTS,
     //!     OPEN_FLAGS_DASD | OPEN_FLAGS_NO_CACHE | OPEN_ACCESS_READONLY | OPEN_SHARE_DENYREADWRITE,

     //      OPEN_SHARE_DENYNONE | OPEN_ACCESS_READWRITE |
     //      OPEN_FLAGS_NO_CACHE  | OPEN_FLAGS_WRITE_THROUGH, // | OPEN_FLAGS_DASD,

     // OPEN_ACTION_OPEN_IF_EXISTS,         /* open flags   */
     // OPEN_ACCESS_READONLY | OPEN_SHARE_DENYNONE | OPEN_FLAGS_DASD |
     // OPEN_FLAGS_WRITE_THROUGH, // | OPEN_FLAGS_NO_CACHE,

  //printf("open_drive: DosOpenL(%s, ...) returned %lu, hDevice=%lu\n", path, rc, *hDevice);
  //printf("ulAction=%lx\n", ulAction);

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

  rc = DosDevIOCtl( hDevice, IOCTL_DISK, DSK_LOCKDRIVE, &parminfo,
                    parmlen, &parmlen, &datainfo, // Param packet
                    datalen, &datalen);

  //printf("lock_drive: func=DSK_LOCKDRIVE returned rc=%lu\n", rc);

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

  rc = DosDevIOCtl( hDevice, IOCTL_DISK, DSK_UNLOCKDRIVE, &parminfo,
                    parmlen, &parmlen, &datainfo, // Param packet
                    datalen, &datalen);

  //printf("unlock_drive: func=DSK_UNLOCKDRIVE returned rc=%lu\n", rc);

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

void get_drive_params(HANDLE hDevice, struct extbpb *dp)
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

  rc = DosDevIOCtl( hDevice, IOCTL_DISK, DSK_GETDEVICEPARAMS,
                      &p, parmlen, &parmlen,
                      &d, datalen, &datalen);

  //printf("get_drive_params: func=DSK_GETDEVICEPARAMS returned rc=%lu\n", rc);

  if ( rc )
      die( "Failed to get device geometry", rc );

  dp->BytesPerSect = d.usBytesPerSector;
  dp->SectorsPerTrack = d.usSectorsPerTrack;
  dp->HiddenSectors = d.cHiddenSectors;
  dp->TracksPerCylinder = d.cHeads;

  //
  //printf("cylinders=%u\n",   d.cCylinders);
  //printf("device type=%u\n", d.bDeviceType);
  //printf("device attr=%u\n", d.fsDeviceAttr);

  if ((d.cSectors == 0) && (d.cLargeSectors))
      dp->TotalSectors = d.cLargeSectors;
  else if ((d.cSectors) && (d.cLargeSectors == 0))
      dp->TotalSectors = d.cSectors;
  //
  //printf("TotalSectors=%lu, BytesPerSect=%u\n", 
  //       dp->TotalSectors, dp->BytesPerSect);
  //
  //printf("SectorsPerTrack=%u\n", d.usSectorsPerTrack);
  //printf("HiddenSectors=%lu\n", d.cHiddenSectors);
  //printf("TracksPerCylinder=%u\n", d.cHeads);
  //printf("TotalSectors=%lu\n", dp->TotalSectors);
  //
}

void set_part_type(HANDLE hDevice, struct extbpb *dp, int type)
{
  APIRET rc;
  int i;

  rc = ReadSect(hDevice, -dp->HiddenSectors, 1, (char *)&mbr);

  if (rc)
      die("Error reading MBR\n", rc);

  for (i = 0; i < 4; i++)
  {
    if (mbr.pte[i].relative_sector == dp->HiddenSectors)
    {
      // set type to FAT32
      mbr.pte[i].system_id = type;
      rc = WriteSect(hDevice, -dp->HiddenSectors, 1, (char *)&mbr);

      if (rc)
          die("Error writing MBR\n", rc);

      break;
    }
  }
}

void begin_format (HANDLE hDevice)
{
  // Detach the volume from the old FSD 
  // and attach to the new one
  unsigned char  cmdinfo     = 0;
  unsigned long  parmlen     = sizeof(cmdinfo);
  unsigned char  datainfo[]  = "FAT32"; // FSD name
  unsigned long  datalen     = sizeof(datainfo); 
  APIRET rc;

  rc = DosDevIOCtl( hDevice, IOCTL_DISK, DSK_BEGINFORMAT, &cmdinfo, 
                    parmlen, &parmlen, &datainfo,
                    datalen, &datalen);

  //printf("begin_format: func=DSK_BEGINFORMAT returned rc=%lu\n", rc);

  if ( rc )
      die( "Failed to begin format device", rc );
}

void remount_media (HANDLE hDevice)
{
  // Redetermine media
  unsigned char parminfo  = 0;
  unsigned char datainfo  = 0;
  unsigned long parmlen   = 1;
  unsigned long datalen   = 1;
  APIRET rc;

  rc = DosDevIOCtl( hDevice, IOCTL_DISK, DSK_REDETERMINEMEDIA,
                    &parminfo, parmlen, &parmlen, // Param packet
                    &datainfo, datalen, &datalen);

  //printf("remount_media: func=DSK_REDETERMINEMEDIA returned rc=%lu\n", rc);

  if ( rc ) 
  {
      show_message( "WARNING: Failed to do final remount, rc = %lu!\n" , 0, 0, 1, rc );
      show_message( "WARNING: probably, you need to reboot for changes to take in effect.\n", 0, 0, 0 );
  }

  //if ( rc )
  //    die( "Failed to do final remount!", rc );
}

void sectorio(HANDLE hDevice)
{
  ULONG ulDeadFace = 0xdeadface;
  ULONG ulParmSize = sizeof(ulDeadFace);
  APIRET rc;

  rc = DosFSCtl(NULL, 0, 0,
                (PBYTE)&ulDeadFace, ulParmSize, &ulParmSize,
                FAT32_SECTORIO,
                NULL,
                hDevice,
                FSCTL_HANDLE);

  if (rc)
  {
    show_message("Error %lu doing FAT32_SECTORIO.\n", 0, 0, 1, rc);
    show_message("%s\n", 0, 0, 1, GetOS2Error(rc));
    return;
  }
}

void startlw(HANDLE hDevice)
{
   ULONG ulDataSize = 0;
   ULONG ulParmSize = 0;
   APIRET rc;

   rc = DosFSCtl(NULL, 0, &ulDataSize,
                 NULL, 0, &ulParmSize,
                 FAT32_STARTLW, "FAT32", -1,
                 FSCTL_FSDNAME);

   if (rc)
   {
     show_message("Error %lu doing FAT32_STARTLW.\n", 0, 0, 1, rc);
     show_message("%s\n", 0, 0, 1, GetOS2Error(rc));
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
                 FAT32_STOPLW, "FAT32", -1,
                 FSCTL_FSDNAME);

   if (rc)
   {
     show_message("Error %lu doing FAT32_STOPLW.\n", 0, 0, 1, rc);
     show_message("%s\n", 0, 0, 1, GetOS2Error(rc));
     return;
   }
}

APIRET read_drive(HANDLE hDevice, char *pBuf, ULONG *cbSize)
{
    // Read Device
    return DosRead ( hDevice, pBuf, *cbSize, cbSize );
}

APIRET write_drive(HANDLE hDevice, LONGLONG off, char *pBuf, ULONG *cbSize)
{
    // Seek
    APIRET rc = DosSetFilePtrL(hDevice, off, FILE_BEGIN, &off);

    if ( rc )
        die ( "Seek error!", rc );

    // Write Device
    return DosWrite ( hDevice, pBuf, *cbSize, cbSize );
}

void close_drive(HANDLE hDevice)
{
    // Close Device
    DosClose ( hDevice );
}

void mem_alloc(void **p, ULONG cb)
{
    APIRET rc;

    if (!(rc = DosAllocMem ( (void **)p, cb, PAG_COMMIT | PAG_READ | PAG_WRITE )))
    {
            memset(*p, 0, cb);
            //printf("mem_alloc: rc=%lu\n", rc);
    }
    else
            show_message("mem_alloc failed, rc=%lu\n", 0, 0, 1, rc);
}

void mem_free(void *p, ULONG cb)
{
    if (p)
       DosFreeMem(p);
}

void query_freq(ULONGLONG *freq)
{
    DosTmrQueryFreq( (ULONG *)freq );
}

void query_time(ULONGLONG *time)
{
    DosTmrQueryTime( (QWORD *)time );
}

void check_vol_label(char *path, char **vol_label)
{
    /* Current volume label  */
    char cur_vol[12];
    char testvol[12];
    /* file system information buffer */
    FSINFO fsiBuffer;
    char   c;
    ULONG  rc;

    if (! msg)
    {
        memset(cur_vol, 0, sizeof(cur_vol));
        memset(testvol, 0, sizeof(testvol));

        // Query the filesystem info, 
        // including the current volume label
        rc = DosQueryFSInfo((toupper(path[0]) - 'A' + 1), FSIL_VOLSER,
                            (PVOID)&fsiBuffer, sizeof(fsiBuffer));

        if (rc == NO_ERROR)
            // Current disk volume label
            strcpy(cur_vol, fsiBuffer.vol.szVolLabel);

        show_message( "The new type of file system is %1.", 0, 1293, 1, TYPE_STRING, "FAT32" );

        if (!cur_vol || !*cur_vol)
            show_message( "The disk has no volume label\n", 0, 125, 0 );
        else
        {
            if (!vol_label || !*vol_label || !**vol_label)
            {
                show_message( "Enter the current volume label for drive %s\n", 0, 1318, 1, TYPE_STRING, path );

                // Read the volume label
                gets(testvol);
            }
        }

        // if the entered volume label is empty, or doesn't
        // the same as a current volume label, write an error
        if ( stricmp(*vol_label, cur_vol) && stricmp(testvol, cur_vol) && (!**vol_label || !*testvol))
        {
            show_message( "An incorrect volume label was entered for this drive.\n", 0, 636, 0 );
            quit (1);
        }
    }

    show_message( "Warning! All data on hard disk %s will be lost!\n"
                  "Proceed with FORMAT (Y/N)?\n", 0, 1271, 1, TYPE_STRING, path );

    c = getchar();

    if ( c != '1' && toupper(c) != 'Y' )
        quit (1);
 
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
                      "or press Enter for no volume label.\n", 0, 1288, 0 );

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

  rc = DosSetFSInfo(diskno, FSIL_VOLSER,
                    (PVOID)&vl, sizeof(VOLUMELABEL));

  if ( rc )
    show_message ("WARNING: failed to set the volume label, rc=%lu\n", 0, 0, 1, rc);
}


void show_progress (float fPercentWritten)
{
  char str[128];
  int len, i;

  // construct message
  sprintf(str, "%3.f%%", fPercentWritten);
  // 538  %1%% of the disk has been formatted. ??? or 1312 ?
  len = show_message( "%s percent of disk formatted %s\n", 0, 1312, 2,
                      TYPE_STRING, str, 
                      TYPE_STRING, "..." );

  // restore cursor position
  for (i = 0; i < len; i++)
     printf("\b");

  fflush(stdout); 
}


int show_message (char *pszMsg, unsigned short usLogMsg, unsigned short usMsg, unsigned short usNumFields, ...)
{
    va_list va;
    UCHAR szBuf[1024];
    int len, i;
    char *dummy;

    va_start(va, usNumFields);

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
       ULONG arg = va_arg(va, ULONG);
       ULONG ulParmNo = usNumFields;

       if (strstr(pszMsg, "%s"))
          {
          if (usMsg && arg == TYPE_STRING)
             pszString = va_arg(va, PSZ);
          else
             pszString = (PSZ)arg;
          }

       if (pszString)
          ulParmNo--;

       LogOutMessagePrintf(usLogMsg, pszString, ulParmNo, va);
       }

    va_end( va );

    // output additional messages for /P switch specified
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
             printf(",%lf", arg);
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
