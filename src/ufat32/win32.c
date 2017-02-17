/*  Win32-specific functions
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fat32c.h"

HANDLE hDev;

char msg = FALSE;

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
 
    exit (dw);
}

void seek_to_sect( HANDLE hDevice, DWORD Sector, DWORD BytesPerSect )
{
    LONGLONG Offset;
    LONG HiOffset;
    
    Offset = Sector * BytesPerSect ;
    HiOffset = (LONG) (Offset>>32);
    SetFilePointer ( hDevice, (LONG) Offset , &HiOffset , FILE_BEGIN );
}


BOOL read_file ( HANDLE hDevice, BYTE *pData, DWORD ulNumBytes, DWORD *dwRead )
{
    return ReadFile ( hDevice, pData, ulNumBytes, dwRead, NULL );
}


ULONG ReadSect ( HANDLE hDevice, LONG ulSector, USHORT nSectors, USHORT BytesPerSector, PBYTE pbSector )
{
    DWORD dwRead;
    BOOL ret;

    seek_to_sect ( hDevice, ulSector, BytesPerSector );

    ret = ReadFile ( hDevice, pbSector, nSectors * BytesPerSector, &dwRead, NULL );

    if ( !ret )
        return GetLastError();

    return 0;
}


BOOL write_file ( HANDLE hDevice, BYTE *pData, DWORD ulNumBytes, DWORD *dwWritten )
{
    return WriteFile ( hDevice, pData, ulNumBytes, dwWritten, NULL );
}

ULONG WriteSect ( HANDLE hDevice, LONG ulSector, USHORT nSectors, USHORT BytesPerSector, PBYTE pbSector )
{
    DWORD dwWritten;
    BOOL ret;

    seek_to_sect ( hDevice, ulSector, BytesPerSector );
    ret = WriteFile ( hDevice, pbSector, nSectors * BytesPerSector, &dwWritten, NULL );

    if ( !ret )
        return GetLastError();

    return 0;
}

void write_sect ( HANDLE hDevice, DWORD Sector, DWORD BytesPerSector, void *Data, DWORD NumSects )
{
    DWORD rc;

    rc = WriteSect ( hDevice, Sector, NumSects, BytesPerSector, Data );

    if ( rc )
        die ( "Failed to write", rc );
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
}

void cleanup ( void )
{
    if (hDev == 0)
        return;

    remount_media ( hDev );
    unlock_drive ( hDev );
    close_drive ( hDev);
    hDev = 0;
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

ULONG query_vol_label(char *path, char *pszVolLabel, int cbVolLabel)
{
    /* file system information buffer */
    ULONG volSerNum;
    ULONG maxFileNameLen;
    ULONG fsFlags;
    char  fsName[8];
    BOOL rc;

    memset(fsName,  0, sizeof(fsName));

    // Query the filesystem info, 
    // including the current volume label
    rc = GetVolumeInformation(path, pszVolLabel, cbVolLabel,
                              &volSerNum, &maxFileNameLen, 
                              &fsFlags, fsName, sizeof(fsName));

    if ( ! rc )
        return GetLastError();

    return 0;
}

void set_vol_label (char *path, char *vol)
{
    SetVolumeLabel(path, vol);
}

void set_datetime(DIRENTRY *pDir)
{
    SYSTEMTIME datetime;

    GetSystemTime(&datetime);

    pDir->wCreateTime.hours = datetime.wHour;
    pDir->wCreateTime.minutes = datetime.wMinute;
    pDir->wCreateTime.twosecs = datetime.wSecond / 2;
    pDir->wCreateDate.day = datetime.wDay;
    pDir->wCreateDate.month = datetime.wMonth;
    pDir->wCreateDate.year = datetime.wYear - 1980;
    pDir->wAccessDate.day = datetime.wDay;
    pDir->wAccessDate.month = datetime.wMonth;
    pDir->wAccessDate.year = datetime.wYear - 1980;
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


void query_current_disk(char *pszDrive)
{
    TCHAR pBuf[MAX_PATH];

    GetCurrentDirectory(sizeof(pBuf), pBuf);

    pszDrive[0] = pBuf[0];
    pszDrive[1] = ':';
    pszDrive[2] = '\0';
}


#ifdef __UNICODE__
BOOL IsDBCSLead( UCHAR uch )
{
    return IsDBCSLeadByte(uch);
}


/* Unicode to OEM */
VOID Translate2OS2(PUSHORT pusUni, PSZ pszName, USHORT usLen)
{
    int cbSize;
    USHORT pBuf[256];

    if (! pusUni || !usLen)
       {
       *pszName = '\0';
       return;
       }

    memcpy(pBuf, pusUni, usLen);
    cbSize = WideCharToMultiByte(CP_OEMCP, 0, pBuf, usLen, pszName, 2 * usLen, NULL, NULL);

    if (! cbSize)
       {
       *pszName = '\0';
       return;
       }
}


/* OEM to Unicode */
USHORT Translate2Win(PSZ pszName, PUSHORT pusUni, USHORT usLen)
{
    int cbSize;
    USHORT pBuf[256];

    if (! pszName || !usLen)
       {
       *pusUni = '\0';
       return 0;
       }

    memcpy(pBuf, pszName, usLen);
    cbSize = MultiByteToWideChar(CP_OEMCP, 0, (char *)pBuf, usLen, pusUni, usLen);

    if (! cbSize)
       {
       *pusUni = '\0';
       return 0;
       }

    return cbSize;
}
#endif


BOOL OutputToFile(HANDLE hFile)
{
   DWORD rc = GetFileType(hFile);

   switch (rc)
      {
      case FILE_TYPE_DISK:
         return TRUE;

      case FILE_TYPE_CHAR:
      case FILE_TYPE_PIPE:
         return FALSE;
/*
      default:
         return FALSE;
*/
      }

   return FALSE;
}


size_t strnlen(const char * str, size_t n)
{
  const char *start = str;

  while (*str && n-- > 0)
    str++;

  return str - start;
}


#define ERROR_MR_MSG_TOO_LONG   316
#define ERROR_MR_INV_IVCOUNT    320
#define ERROR_MR_UN_PERFORM     321


ULONG DosInsertMessage(char **pTable, ULONG cTable,
                       char *pszMsg, ULONG cbMsg, char *pBuf,
                       ULONG cbBuf, ULONG *pcbMsg)
{
  int i;

  // Check arguments
  if (!pszMsg) return ERROR_INVALID_PARAMETER;                 // Nothing to proceed
  if (!pBuf) return ERROR_INVALID_PARAMETER;                   // No target buffer
  if ((cTable) && (!pTable)) return ERROR_INVALID_PARAMETER;   // No inserting strings array
  if (cbMsg > cbBuf) return ERROR_MR_MSG_TOO_LONG;             // Target buffer too small

  if (!cTable)
  {
    // If nothing to insert then just copy message to buffer
    strncpy(pBuf, pszMsg, cbMsg);
    *pcbMsg = strlen(pszMsg);
    return NO_ERROR;
  } else {
    // Produce output string
    PCHAR src;
    PCHAR dst;
    int   srclen;
    int   dstlen;
    int   len, rest, maxlen = 0;
    int   ivcount = 0;
    int   i;

    src    = (char *)pszMsg;
    dst    = pBuf;
    srclen = cbMsg;
    maxlen = srclen;
    dstlen = 0;

    // add params lenths (without zeroes)
    for (i = 0; i < cTable; i++)
      maxlen += strnlen(pTable[i], cbBuf) - 1;

    for (;;)
    {
      if (*src == '%')
      {
        src++;
        srclen--;

        ivcount++;

        switch (*src)
        {
          case '0': // %0
            srclen--;
            src++;
            break;
          case '1': // %1
          case '2': // %2
          case '3': // %3
          case '4': // %4
          case '5': // %5
          case '6': // %6
          case '7': // %7
          case '8': // %8
          case '9': // %9
            len = strnlen(pTable[*src - '1'], cbBuf);
            strncpy(dst, pTable[*src - '1'],  len);
            src++;
            dst    += len;
            dstlen += len;
            break;
          default:  // Can't perfom action?
            if (srclen <= 0)
              break;

            ivcount--;

            *dst++ = '%';
            dstlen++;

            *dst++ = *src++;
            srclen--;
            dstlen++;
        }
      }
      else
      {
        *dst++ = *src++;
        srclen--;
        dstlen++;
      }

     if (ivcount > 9)
       return ERROR_MR_INV_IVCOUNT;

     if (srclen <= 0)
       break;

     // if no bytes remaining for terminating zero, return an error
     if (dstlen > maxlen)
     {
       *pcbMsg = cbBuf;
       *dst++ = '\0';
       dstlen++;
       return ERROR_MR_MSG_TOO_LONG;
     }
   }

   *dst++ = '\0';
   dstlen++;
   *pcbMsg = dstlen;

   return NO_ERROR;
   }
}


INT cdecl iShowMsg2 ( PSZ pszMsg, USHORT usNumFields, va_list va )
{
static BYTE szErrNo[12] = "";
static BYTE szOut[MAX_MESSAGE];
static BYTE rgNum[9][50];
ULONG ulReplySize;
ULONG rc;
PSZ    rgPSZ[9];
USHORT usIndex;
PSZ    pszMess;
char   *p;
int    i;

   pszMess = pszMsg;

   memset(szOut, 0, sizeof(szOut));
   memset(rgPSZ, 0, sizeof(rgPSZ));
   memset(rgNum, 0, sizeof(rgNum));
   for (usIndex = 0; usIndex < usNumFields; usIndex++)
      {
      USHORT usType = va_arg(va, USHORT);
      if (usType == TYPE_PERC)
         {
         sprintf(rgNum[usIndex], "%3u", va_arg(va, USHORT));
         rgPSZ[usIndex] = rgNum[usIndex];
         }
      else if (usType == TYPE_LONG || usType == TYPE_LONG2)
         {
         if (!usIndex && usType == TYPE_LONG)
            sprintf(rgNum[usIndex], "%12lu", va_arg(va, ULONG));
         else
            sprintf(rgNum[usIndex], "%lu", va_arg(va, ULONG));
         rgPSZ[usIndex] = rgNum[usIndex];
         }
      else if (usType == TYPE_DOUBLE || usType == TYPE_DOUBLE2)
         {
         PSZ p;
         if (!usIndex && usType == TYPE_DOUBLE)
            sprintf(rgNum[usIndex], "%12.0lf", va_arg(va, double));
         else
            sprintf(rgNum[usIndex], "%lf", va_arg(va, double));
         p = strchr(rgNum[usIndex], '.');
         if (p)
            *p = 0;
         rgPSZ[usIndex] = rgNum[usIndex];
         }
      else if (usType == TYPE_STRING)
         {
         rgPSZ[usIndex] = va_arg(va, PSZ);
         }
      }

   rc = DosInsertMessage(rgPSZ,
      usNumFields,
      pszMess,
      strlen(pszMess),
      szOut + strlen(szOut),
      sizeof(szOut) - strlen(szOut),
      &ulReplySize);
   printf("%s", szOut);

   return ulReplySize;
}


int show_message (char *pszMsg, unsigned short usLogMsg, unsigned short usMsg, unsigned short usNumFields, ...)
{
    va_list va;
    UCHAR szBuf[1024];
    int i, len = 0;
    char *dummy;

    va_start(va, usNumFields);

    if (pszMsg)
       {
       if (usMsg)
          {
          // output message which is not in oso001.msg
          len = iShowMsg2 ( pszMsg, usNumFields, va );
          }
       else
          {
          vsprintf( szBuf, pszMsg, va );
          printf("%s", szBuf);
          len = strlen(szBuf);
          }
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
