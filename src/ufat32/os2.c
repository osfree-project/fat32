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

HANDLE hDev;

char msg = FALSE;

PSZ GetOS2Error(USHORT rc);

void LogOutMessagePrintf(ULONG ulMsgNo, char *psz, ULONG ulParmNo, va_list va);

#ifdef __UNICODE__
USHORT QueryUni2NLS( USHORT usPage, USHORT usChar );
USHORT QueryNLS2Uni( USHORT usCode );

extern UCHAR rgFirstInfo[ 256 ];
#endif

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

   ulDataSize = nSectors * BytesPerSector;

   if ( (rc = DosDevIOCtl((HFILE)hFile, IOCTL_DISK, DSK_GETDEVICEPARAMS,
                          &parm2, parmlen2, &parmlen2, &bpb, datalen2, &datalen2)) )
       return rc;

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
   ULONG rc;
   ULONG parmlen4;
   char trkbuf[sizeof(TRACKLAYOUT) + sizeof(USHORT) * 2 * 255];
   PTRACKLAYOUT ptrk = (PTRACKLAYOUT)trkbuf;
   ULONG datalen4 = BUFSIZE;
   USHORT cyl, head, sec, n;
   ULONGLONG off;
   int i;

   ulDataSize = nSectors * BytesPerSector;

   if ( (rc = DosDevIOCtl((HFILE)hf, IOCTL_DISK, DSK_GETDEVICEPARAMS,
                          &parm2, parmlen2, &parmlen2, &bpb, datalen2, &datalen2)) )
       return rc;

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

ULONG write_sect ( HANDLE hDevice, DWORD Sector, DWORD BytesPerSector, void *Data, DWORD NumSects )
{
    DWORD dwWritten;
    ULONG ret;

    ret = WriteSect( hDevice, Sector, NumSects, BytesPerSector, Data);

    return ret;
}


void open_drive (char *path, HANDLE *hDevice)
{
  APIRET rc;
  ULONG  ulAction;
  char DriveDevicePath[]="Z:"; // for DosOpen
  char *p = path;

  if (path[1] == ':' && path[2] == '\0')
  {
    // drive letter (UNC)
    DriveDevicePath[0] = *path;
    p = DriveDevicePath;
  }

  rc = DosOpen( p,               // filename
	      (HFILE *)hDevice,  // handle returned
              &ulAction,         // action taken by DosOpen
              0,                 // cbFile
              0,                 // ulAttribute
              OPEN_ACTION_OPEN_IF_EXISTS, // | OPEN_ACTION_REPLACE_IF_EXISTS,
              OPEN_FLAGS_FAIL_ON_ERROR |  // OPEN_FLAGS_WRITE_THROUGH | 
              OPEN_SHARE_DENYREADWRITE |  // OPEN_FLAGS_NO_CACHE  |
              OPEN_ACCESS_READWRITE    | OPEN_FLAGS_DASD,
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

void begin_format (HANDLE hDevice)
{
  // Detach the volume from the old FSD 
  // and attach to the new one
  unsigned char  cmdinfo     = 0;
  unsigned long  parmlen     = sizeof(cmdinfo);
  unsigned char  datainfo[]  = "FAT32"; // FSD name
  unsigned long  datalen     = sizeof(datainfo); 
  APIRET rc;

  rc = DosDevIOCtl( (HFILE)hDevice, IOCTL_DISK, DSK_BEGINFORMAT, &cmdinfo, 
                    parmlen, &parmlen, &datainfo,
                    datalen, &datalen);

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

  rc = DosDevIOCtl( (HFILE)hDevice, IOCTL_DISK, DSK_REDETERMINEMEDIA,
                    &parminfo, parmlen, &parmlen, // Param packet
                    &datainfo, datalen, &datalen);

  if ( rc ) 
  {
      show_message( "WARNING: Failed to do final remount, rc = %lu!\n" , 0, 0, 1, rc );
      show_message( "WARNING: probably, you need to reboot for changes to take in effect.\n", 0, 0, 0 );
  }
}


void cleanup ( void )
{
    static int num_called = 0;

    num_called++;

    if (num_called > 1)
        return;

    if (hDev == 0)
        return;

    remount_media ( hDev );
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
                 FAT32_STARTLW, "FAT32", -1,
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
                 FAT32_STOPLW, "FAT32", -1,
                 FSCTL_FSDNAME);

   if (rc)
   {
     show_message("Error %lu doing FAT32_STOPLW.\n", 0, 0, 1, rc);
     show_message("%s\n", 0, 0, 1, get_error(rc));
     return;
   }
}

void close_drive(HANDLE hDevice)
{
    // Close Device
    DosClose ( (HFILE)hDevice );
}


int mem_alloc(void **p, ULONG cb)
{
    APIRET rc;

    if (!(rc = DosAllocMem ( (void **)p, cb, PAG_COMMIT | PAG_READ | PAG_WRITE )))
        memset(*p, 0, cb);
    else
        show_message("mem_alloc failed, rc=%lu\n", 0, 0, 1, rc);

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
    rc = DosQueryFSInfo((toupper(path[0]) - 'A' + 1), FSIL_VOLSER,
                        (PVOID)&fsiBuffer, sizeof(fsiBuffer));

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

  rc = DosSetFSInfo(diskno, FSIL_VOLSER,
                    (PVOID)&vl, sizeof(VOLUMELABEL));

  if ( rc )
    show_message ("WARNING: failed to set the volume label, rc=%lu\n", 0, 0, 1, rc);
}


void set_datetime(DIRENTRY *pDir)
{
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
             printf(",%12.0lf", arg);
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
