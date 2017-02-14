/*  Win32-specific functions
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fat32c.h"

extern HANDLE hDev;

char msg = FALSE;

int logbufsize = 0;
char *logbuf = NULL;
int  logbufpos = 0;

void LogOutMessagePrintf(ULONG ulMsgNo, char *psz, ULONG ulParmNo, va_list va);

DWORD get_vol_id (void)
{
    SYSTEMTIME s;
    DWORD d;
    WORD lo,hi,tmp;

    GetLocalTime( &s );

    lo = s.wDay + ( s.wMonth << 8 );
    tmp = (s.wMilliseconds/10) + (s.wSecond << 8 );
    lo += tmp;

    hi = s.wMinute + ( s.wHour << 8 );
    hi += s.wYear;
   
    d = lo + (hi << 16);
    return(d);
}

void die ( char * error, DWORD rc )
{
    // Retrieve the system error message for the last-error code

    LPVOID lpMsgBuf;
    DWORD dw = GetLastError(); 

	if ( dw )
		{
		FormatMessage(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | 
			FORMAT_MESSAGE_FROM_SYSTEM |
			FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,
			dw,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPTSTR) &lpMsgBuf,
			0, NULL );

		// Display the error message and exit the process

		fprintf ( stderr, "%s\nGetLastError()=%d: %s\n", error, dw, lpMsgBuf );	
		}
	else
		{
		fprintf ( stderr, "%s\n", error );	
		}

    LocalFree(lpMsgBuf);
 
    quit (dw);
}

void seek_to_sect( HANDLE hDevice, DWORD Sector, DWORD BytesPerSect )
{
    LONGLONG Offset;
    LONG HiOffset;
    
    Offset = Sector * BytesPerSect ;
    HiOffset = (LONG) (Offset>>32);
    SetFilePointer ( hDevice, (LONG) Offset , &HiOffset , FILE_BEGIN );
}

void read_sect ( HANDLE hDevice, DWORD Sector, DWORD BytesPerSector, void *Data, DWORD NumSects )
{
    DWORD dwRead;
    BOOL ret;

    seek_to_sect ( hDevice, Sector, BytesPerSector );
    ret = ReadFile ( hDevice, Data, NumSects*BytesPerSector, &dwRead, NULL );

    if ( !ret )
        die ( "Failed to read", ret );
}

BOOL read_file ( HANDLE hDevice, BYTE *pData, DWORD ulNumBytes, DWORD *dwRead )
{
    return ReadFile ( hDevice, pData, ulNumBytes, dwRead, NULL );
}

ULONG ReadSect ( HANDLE hDevice, LONG ulSector, USHORT nSectors, PBYTE pbSector )
{
    read_sect ( hDevice, ulSector, SECTOR_SIZE, pbSector, nSectors );
    return 0;
}

void write_sect ( HANDLE hDevice, DWORD Sector, DWORD BytesPerSector, void *Data, DWORD NumSects )
{
    DWORD dwWritten;
    BOOL ret;

    seek_to_sect ( hDevice, Sector, BytesPerSector );
    ret=WriteFile ( hDevice, Data, NumSects*BytesPerSector, &dwWritten, NULL );

    if ( !ret )
        die ( "Failed to write", ret );
}

BOOL write_file ( HANDLE hDevice, BYTE *pData, DWORD ulNumBytes, DWORD *dwWritten )
{
    return WriteFile ( hDevice, pData, ulNumBytes, dwWritten, NULL );
}

ULONG WriteSect ( HANDLE hDevice, LONG ulSector, USHORT nSectors, PBYTE pbSector )
{
    write_sect ( hDevice, ulSector, SECTOR_SIZE, pbSector, nSectors );
    return 0;
}

void open_drive (char *path , HANDLE *hDevice)
{
  char  DriveDevicePath[]="\\\\.\\Z:"; // for CreateFile
  char  *p = path;
  ULONG cbRet;
  BOOL  bRet;

  if (strlen(path) == 2 && path[1] == ':')
  {
    // drive letter (UNC)
    DriveDevicePath[4] = *path;
    p = DriveDevicePath;
  }

  // open the drive
  *hDevice = CreateFile (
      p,  
      GENERIC_READ | GENERIC_WRITE,
      0,
      NULL, 
      OPEN_EXISTING, 
      FILE_FLAG_NO_BUFFERING,
      NULL);
  if ( *hDevice ==  INVALID_HANDLE_VALUE )
      die( "Failed to open device - close any files before formatting,\n"
           "and make sure you have Admin rights when using fat32format\n"
            "Are you SURE you're formatting the RIGHT DRIVE!!!\n", -1 );

  hDev = *hDevice;

  bRet = DeviceIoControl(
	  (HANDLE) hDevice,              // handle to device
	  FSCTL_ALLOW_EXTENDED_DASD_IO,  // dwIoControlCode
	  NULL,                          // lpInBuffer
	  0,                             // nInBufferSize
	  NULL,                          // lpOutBuffer
	  0,                             // nOutBufferSize
	  &cbRet,        	         // number of bytes returned
	  NULL                           // OVERLAPPED structure
	);

  if ( !bRet )
      show_message ( "Failed to allow extended DASD on device...\n", 0, 0, 0 );
  else
      show_message ( "FSCTL_ALLOW_EXTENDED_DASD_IO OK\n", 0, 0, 0 ); 
}

void lock_drive(HANDLE hDevice)
{
  BOOL bRet;
  DWORD cbRet;

  // lock it
  bRet = DeviceIoControl( hDevice, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &cbRet, NULL );

  if ( !bRet )
      die( "Failed to lock device", bRet );
}

void unlock_drive(HANDLE hDevice)
{
  BOOL  bRet;
  DWORD cbRet;

  bRet = DeviceIoControl( hDevice, FSCTL_UNLOCK_VOLUME, NULL, 0, NULL, 0, &cbRet, NULL );

  if ( !bRet )
      die( "Failed to unlock device", -8 );
}

void get_drive_params(HANDLE hDevice, struct extbpb *dp)
{
  BOOL  bRet;
  DWORD cbRet;
  DISK_GEOMETRY          dgDrive;
  PARTITION_INFORMATION  piDrive;

  PARTITION_INFORMATION_EX xpiDrive;
  BOOL bGPTMode = FALSE;
  SET_PARTITION_INFORMATION spiDrive;

  // work out drive params
  bRet = DeviceIoControl ( hDevice, IOCTL_DISK_GET_DRIVE_GEOMETRY,
                           NULL, 0, &dgDrive, sizeof(dgDrive),
                           &cbRet, NULL);

  if ( !bRet )
      die( "Failed to get device geometry", -10 );

  bRet = DeviceIoControl ( hDevice, 
        IOCTL_DISK_GET_PARTITION_INFO,
        NULL, 0, &piDrive, sizeof(piDrive),
        &cbRet, NULL);

  if ( !bRet )
  {
      //die( "Failed to get parition info", -10 );

      show_message ( "IOCTL_DISK_GET_PARTITION_INFO failed, \n"
               "trying IOCTL_DISK_GET_PARTITION_INFO_EX\n", 0, 0, 0 );

      bRet = DeviceIoControl ( hDevice, 
			IOCTL_DISK_GET_PARTITION_INFO_EX,
			NULL, 0, &xpiDrive, sizeof(xpiDrive),
			&cbRet, NULL);
			
      if (!bRet)
          die( "Failed to get partition info (both regular and _ex)", -11 );

      memset ( &piDrive, 0, sizeof(piDrive) );
      piDrive.StartingOffset.QuadPart = xpiDrive.StartingOffset.QuadPart;
      piDrive.PartitionLength.QuadPart = xpiDrive.PartitionLength.QuadPart;
      piDrive.HiddenSectors = (DWORD) (xpiDrive.StartingOffset.QuadPart / dgDrive.BytesPerSector);
		
      bGPTMode = ( xpiDrive.PartitionStyle == PARTITION_STYLE_MBR ) ? 0 : 1;
      show_message ( "IOCTL_DISK_GET_PARTITION_INFO_EX ok, GPTMode=%d\n", 0, 0, 1, bGPTMode );
  }

  dp->BytesPerSect = dgDrive.BytesPerSector;
  dp->TotalSectors = piDrive.PartitionLength.QuadPart / dp->BytesPerSect;
  dp->SectorsPerTrack = dgDrive.SectorsPerTrack;
  dp->HiddenSectors =  piDrive.HiddenSectors;
  dp->TracksPerCylinder = dgDrive.TracksPerCylinder;
}

void set_part_type(HANDLE hDevice, struct extbpb *dp, int type)
{
  BOOL  bRet;
  DWORD cbRet;
  SET_PARTITION_INFORMATION spiDrive;

  spiDrive.PartitionType = type; // FAT32 LBA. 
  bRet = DeviceIoControl ( hDevice, 
      IOCTL_DISK_SET_PARTITION_INFO,
      &spiDrive, sizeof(spiDrive),
      NULL, 0, 
      &cbRet, NULL);

  if ( !bRet )
      {
      // This happens because the drive is a Super Floppy
      // i.e. with no partition table. Disk.sys creates a PARTITION_INFORMATION
      // record spanning the whole disk and then fails requests to set the 
      // partition info since it's not actually stored on disk. 
      // So only complain if there really is a partition table to set      
      if ( dp->HiddenSectors  )
	  die( "Failed to set parition info", -6 );
      }    
}

void begin_format (HANDLE hDevice)
{
    // none
}

void remount_media (HANDLE hDevice)
{
  BOOL bRet;
  DWORD cbRet;

  bRet = DeviceIoControl( hDevice, FSCTL_DISMOUNT_VOLUME, NULL, 0, NULL, 0, &cbRet, NULL );

  if ( !bRet )
      die( "Failed to dismount device", -7 );

  //unlock_drive( hDevice );
}

void sectorio(HANDLE hDevice)
{
    // none
}

void startlw(HANDLE hDevice)
{
    // none
}

void stoplw(HANDLE hDevice)
{
    // none
}

void close_drive(HANDLE hDevice)
{
    // Close Device
    CloseHandle( hDevice );
}


int mem_alloc(void **p, ULONG cb)
{
    *p = (void *) VirtualAlloc ( NULL, cb, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE );
    if (*p)
       return 0;

    return 1;
}

void mem_free(void *p, ULONG cb)
{
    if (p)
        VirtualFree(p, cb, 0);
}

void query_freq(ULONGLONG *freq)
{
    LARGE_INTEGER *frequency = (LARGE_INTEGER *)freq;
    QueryPerformanceFrequency( frequency );
}

void query_time(ULONGLONG *time)
{
    LARGE_INTEGER *t = (LARGE_INTEGER *)time;
    QueryPerformanceCounter( t );
}

void check_vol_label(char *path, char **vol_label)
{
    /* Current volume label  */
    char  cur_vol[12];
    char  testvol[12];
    ULONG volSerNum;
    ULONG maxFileNameLen;
    ULONG fsFlags;
    char  fsName[8];
    char  c;

    memset(cur_vol, 0, sizeof(cur_vol));
    memset(testvol, 0, sizeof(testvol));
    memset(fsName,  0, sizeof(fsName));

    // Query the filesystem info, 
    // including the current volume label
    GetVolumeInformation(path, cur_vol, sizeof(cur_vol),
           &volSerNum, &maxFileNameLen, 
           &fsFlags, fsName, sizeof(fsName));

    show_message("The current file system type is %s.\n", 0, 0, 1, fsName);

    show_message( "The specified disk did not finish formatting.\n", 0, 528, 0 );

    if (!cur_vol || !*cur_vol)
        show_message( "The disk has no volume label\n", 0, 125, 0 );
    else
    {
        if (!vol_label || !*vol_label || !**vol_label)
        {
            show_message( "Enter the current volume label for drive %s ", 0, 1318, 1, TYPE_STRING, path );

            // Read the volume label
            gets(testvol);
        }
    }

    if (*testvol && *cur_vol && stricmp(testvol, cur_vol))
    {
        show_message( "An incorrect volume label was entered for this drive.\n", 0, 636, 0 );
        quit (1);
    }

    show_message( "Warning! All data on hard disk %s will be lost!\n"
                  "Proceed with FORMAT (Y/N)? ", 0, 1271, 1, TYPE_STRING, path );

    c = getchar();

    if ( c != '1' && toupper(c) != 'Y' )
        quit (1);
 
    fflush(stdout);
}

char *get_vol_label(char *path, char *vol)
{
    return vol;
}

void set_vol_label (char *path, char *vol)
{

}

void set_datetime(DIRENTRY *pDir)
{

}


char *get_error(USHORT rc)
{
    //Get the error message, if any.
    DWORD errID = GetLastError();
    char *msgBuf = NULL;
    static char szBuf[256];
    size_t size;

    //No error message has been recorded
    if(errID == 0)
        return "";

    size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                NULL, errID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&msgBuf, 0, NULL);

    strncpy(szBuf, msgBuf, sizeof(szBuf));

    //Free the buffer.
    LocalFree(msgBuf);

    return szBuf;
}


void show_progress (float fPercentWritten)
{
    char str[128];
    int len, i;
    
    sprintf(str, "%3.f", fPercentWritten);
    len = show_message( "%s of the disk has been formatted\n", 0, 538, 1,
                        TYPE_STRING, str );

    for (i = 0; i < len; i++)
       printf("\b");

    fflush(stdout); 
}


void LogOutMessagePrintf(ULONG ulMsgNo, char *psz, ULONG ulParmNo, va_list va)
{
#pragma pack (2)
   struct
   {
      USHORT usRecordSize;
      USHORT usMsgNo;
      USHORT ulParmNo;
      USHORT cbStrLen;
   } header;
#pragma pack()
   ULONG ulParm;
   int i, len;

   header.usRecordSize = sizeof(header) + ulParmNo * sizeof(ULONG);
   header.cbStrLen = 0;

   if (psz)
      {
      len = strlen(psz) + 1;
      header.cbStrLen = len;
      header.usRecordSize += len;
      }

   header.usMsgNo = (USHORT)ulMsgNo;
   header.ulParmNo = ulParmNo;

   if (logbufpos + header.usRecordSize > logbufsize)
      {
      if (! logbufsize)
         logbufsize = 0x10000;
      if (! logbuf)
         logbuf = malloc(logbufsize);
      else
         {
         logbufsize += 0x10000;
         logbuf = realloc(logbuf, logbufsize);
         }
      if (! logbuf)
         {
         show_message("realloc/malloc failed!\n", 0, 0, 0);
         return;
         }
      }

   memcpy(&logbuf[logbufpos], &header, sizeof(header));
   logbufpos += sizeof(header);

   if (psz)
      {
      memcpy(&logbuf[logbufpos], psz, len);
      logbufpos += len;
      }

   for (i = 0; i < ulParmNo; i++)
      {
      ulParm = va_arg(va, ULONG);
      memcpy(&logbuf[logbufpos], &ulParm, sizeof(ULONG));
      logbufpos += sizeof(ULONG);
      }
}

void LogOutMessage(ULONG ulMsgNo, char *psz, ULONG ulParmNo, ...)
{
   va_list va;

   va_start(va, ulParmNo);
   LogOutMessagePrintf(ulMsgNo, psz, ulParmNo, va);
   va_end(va);
}


int show_message (char *pszMsg, unsigned short usLogMsg, unsigned short usMsg, unsigned short usNumFields, ...)
{
    va_list va;
    UCHAR szBuf[1024];
    int i;
    char *dummy;

    va_start(va, usNumFields);

    if (pszMsg)
       {
       ULONG arg;

       if (strstr(pszMsg, "%s") && usMsg)
          // TYPE_STRING
          arg = va_arg(va, ULONG);

       // output message which is not in oso001.msg
       vsprintf ( szBuf, pszMsg, va );
       printf ( "%s", szBuf );
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

    return strlen(szBuf);
}
