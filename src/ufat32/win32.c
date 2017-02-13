/*  Win32-specific functions
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fat32c.h"

extern HANDLE hDev;

char msg = FALSE;

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

  // Only support hard disks at the moment 
  //if ( dgDrive.BytesPerSector != 512 )
  //{
  //    die ( "This version of fat32format only supports hard disks with 512 bytes per sector.\n" );
  //}
  dp->BytesPerSect = dgDrive.BytesPerSector;
  //dp->PartitionLength = piDrive.PartitionLength.QuadPart;
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


void mem_alloc(void **p, ULONG cb)
{
    *p = (void *) VirtualAlloc ( NULL, cb, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE );
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

void show_progress (float fPercentWritten)
{
    char str[128];
    int len, i;
    
    sprintf(str, "%3.f%%", fPercentWritten);
    len = show_message( "%s percent of disk formatted...", 0, 1312, 1,
                        TYPE_STRING, str);

    for (i = 0; i < len; i++)
       printf("\b");

    fflush(stdout); 
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

    va_end( va );

    return strlen(szBuf);
}
