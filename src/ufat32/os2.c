/*   OS/2 specific functions
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <conio.h>
#include <string.h>

#ifdef __WATCOMC__
#include <ctype.h>
#endif

#include "fat32c.h"
#include "fat32def.h"
#include "portable.h"

typedef HFILE HANDLE;

extern HANDLE hDev;

INT cdecl iShowMessage(PCDINFO pCD, USHORT usNr, USHORT usNumFields, ...);
PSZ       GetOS2Error(USHORT rc);

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
    printf("ERROR: %s\n", error);
    iShowMessage(NULL, 528, 0);
    printf("%s\n", GetOS2Error(rc));

    if ( rc )
        printf("Error code: %lu\n", rc);

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
}

void write_sect ( HANDLE hDevice, DWORD Sector, DWORD BytesPerSector, void *Data, DWORD NumSects )
{
    DWORD dwWritten;
    ULONG ret;

    seek_to_sect ( hDevice, Sector, BytesPerSector );
    ret = DosWrite ( hDevice, Data, NumSects * BytesPerSector, (PULONG)&dwWritten );

    //printf("write_sect: ret = %lu\n", ret);

    if ( ret )
        die ( "Failed to write", ret );
}

BOOL write_file ( HANDLE hDevice, BYTE *pData, DWORD ulNumBytes, DWORD *dwWritten )
{
    BOOL  ret = TRUE;
    ULONG rc = 0;

    if ( rc = DosWrite ( hDevice, pData, (ULONG)ulNumBytes, (PULONG)dwWritten ) )
        ret = FALSE;

    //printf("write_file: hDevice=%lu, pData=0x%lx, ulNumBytes=%lu, dwWritten=0x%lx\n", 
    //        hDevice, pData, ulNumBytes, dwWritten);
    //printf("write_file: rc = %lu, *dwWritten=%lu\n", rc, *dwWritten); // 87 == ERROR_INVALID_PARAMETER

    return ret;
}

void open_drive (char *path, HANDLE *hDevice)
{
  APIRET rc;
  ULONG  ulAction;
  char DriveDevicePath[]="\\\\.\\Z:"; // for DosOpenL
  char *p = path;

  if (path[1] == ':' && path[2] == '\0')
  {
    // drive letter (UNC)
    DriveDevicePath[4] = *path;
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
              OPEN_ACCESS_READWRITE, // | OPEN_FLAGS_DASD,
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
      die( "Failed to open device - close any files before formatting,"
           "and make sure you have Admin rights when using fat32format"
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
      printf( "WARNING: Failed to unlock device, rc = %lu!\n" , rc );
      printf( "WARNING: probably, you need to reboot for changes to take in effect.\n" );
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

void set_part_type(UCHAR Dev, HANDLE hDevice, struct extbpb *dp)
{
  //if ( rc && dp->HiddenSectors )
  //    die( "Failed to set parition info", rc );

  //rc = DosDevIOCtl( hDevice, IOCTL_DISK, DSK_SETDEVICEPARAMS, 
  //                  &p, parmio, &parmio,
  //                 &d, dataio, &dataio);
  //if ( rc )
  //      {
        // This happens because the drive is a Super Floppy
        // i.e. with no partition table. Disk.sys creates a PARTITION_INFORMATION
        // record spanning the whole disk and then fails requests to set the 
        // partition info since it's not actually stored on disk. 
	// So only complain if there really is a partition table to set      


        //if ( d.bpb.cHiddenSectors  )
	//		die( "Failed to set parition info", -6 );
        //}    
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
      printf( "WARNING: Failed to do final remount, rc = %lu!\n" , rc );
      printf( "WARNING: probably, you need to reboot for changes to take in effect.\n" );
  }

  //if ( rc )
  //    die( "Failed to do final remount!", rc );
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
    //{
            memset(*p, 0, cb);
            //printf("mem_alloc: rc=%lu\n", rc);
    //}
    //else
    //        printf("mem_alloc failed, rc=%lu\n", rc);
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

    memset(cur_vol, 0, sizeof(cur_vol));
    memset(testvol, 0, sizeof(testvol));

    // Query the filesystem info, 
    // including the current volume label
    rc = DosQueryFSInfo((toupper(path[0]) - 'A' + 1), FSIL_VOLSER,
                       (PVOID)&fsiBuffer, sizeof(fsiBuffer));
        
    if (rc == NO_ERROR)
        // Current disk volume label
        strcpy(cur_vol, fsiBuffer.vol.szVolLabel);

    // The current file system type is FAT32
    iShowMessage(NULL, 1293, 1, TYPE_STRING, "FAT32");

    if (!cur_vol || !*cur_vol)
        // The disk has no a volume label
        iShowMessage(NULL, 125, 0);
    else
    {
        if (!vol_label || !*vol_label || !**vol_label)
        {
            // Enter the current volume label
            iShowMessage(NULL, 1318, 1, TYPE_STRING, path);

            // Read the volume label
            gets(testvol);
        }
    }

    if (*testvol && *cur_vol && stricmp(testvol, cur_vol))
    {
        // Incorrect volume  label for
        // disk %c is entered!
        iShowMessage(NULL, 636, 0);
        quit (1);
    }

    // Warning! All data on the specified hard disk
    // will be destroyed! Proceed with format (Yes(1)/No(0))?
    iShowMessage(NULL, 1271, 1, TYPE_STRING, path);

    c = getchar();

    if ( c != '1' && toupper(c) != 'Y' )
        quit (1);
 
    fflush(stdout);
}

char *get_vol_label(char *path, char *vol)
{
    static char default_vol[12] = "NO NAME    ";
    static char v[12];
    char *label = vol;

    if (!vol || !*vol)
    {
        fflush(stdin);
        // Enter up to 11 characters for the volume label
        iShowMessage(NULL, 1288, 0);

        label = v;
    }

    memset(label, 0, 12);
    gets(label);

    if (!*label)
        label = default_vol;

    if (strlen(label) > 11)
    {
       // volume label entered exceeds 11 chars
       iShowMessage(NULL, 154, 0);
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
    printf ("WARNING: failed to set the volume label, rc=%lu\n", rc);
}


void show_progress (float fPercentWritten)
{
  char str[128];
  static int pos = 0;
  //USHORT row, col;
  //char   chr = ' ';

  if (! pos)
  {
    pos = 1;
    // save cursor position
    printf("[s");
  }

  //VioGetCurPos(&row, &col, 0);
  //VioWrtNChar(&chr, 8, row, col, 0);
  //VioWrtCharStr(str, strlen(str), row, col, 0);

  // construct message
  sprintf(str, "%.2f%%", fPercentWritten);
  iShowMessage(NULL, 1312, 2, TYPE_STRING, str, 
                              TYPE_STRING, "...");
  //DosSleep(5);
  // restore cursor position
  printf("[u");
  fflush(stdout); 
}

void show_message (char *pszMsg, unsigned short usMsg, unsigned short usNumFields, ...)
{
    va_list va;
    UCHAR szBuf[1024];

    va_start(va, usNumFields);
    vsprintf ( szBuf, pszMsg, va );
    va_end( va );

    //iShowMessage(NULL, usMsg, usNumFields, va);
    puts (szBuf);
}
