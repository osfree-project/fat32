#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <dos.h>

#define INCL_DOS
#define INCL_DOSDEVIOCTL
#define INCL_DOSDEVICES
#define INCL_DOSERRORS

#include "os2.h"
#include "portable.h"
#include "fat32ifs.h"

#include "infoseg.h"

#define LOGBUF_SIZE 8192

#define serial_hw_port 0x3f8

#define TRACE_MAJOR 254

extern PGINFOSEG pGI;

void serout(unsigned short port, char *s);

static BYTE szLogBuf[LOGBUF_SIZE] = "";

static BOOL fData = FALSE;
static BYTE szLost[]="(information lost)\r\n";
static ULONG ulLogSem = 0UL;

APIRET trace(const char *fmt, va_list va);

void (_far _cdecl *LogPrint)(char _far *fmt,...) = 0;

// QSINIT/OS4LDR log support

// make pointer to DOSHLP code (this is constant value for all OS/2 versions)
ULONG FAR *doshlp = MAKEP (0x100,0);

void LogPrintInit(void)
{
  // check OS/4 loader signature at DOSHLP:0 seg
  if (*doshlp == 0x342F534F)
     {
     doshlp++;
     // check OS/4 loader info struct size
     if ((*doshlp & 0xFFFF) >= 0x30)
        {
        USHORT FAR *ptr = (USHORT FAR *) doshlp;
        SELECTOROF (LogPrint) = 0x100;
        OFFSETOF (LogPrint) = ptr[0x17];
        }
     }
}

#ifdef __WATCOM
_WCRTLINK int vsprintf(char * pszBuffer, const char * pszFormat, va_list va);
_WCRTLINK int sprintf(char * pszBuffer, const char * pszFormat, ...);
#else
int cdecl vsprintf(char * pszBuffer, const char * pszFormat, va_list va);
int cdecl sprintf(char * pszBuffer, const char *pszFormat, ...);
#endif

//int kprintf(const char *format, ...);

static BOOL fWriteLogging(PSZ pszMessage);

VOID message(USHORT usLevel, PSZ pszMessage, va_list va)
{
static BOOL fLogPrintInitted = FALSE;
static BOOL fTraceInitted = FALSE;
static BOOL fTraceEnabled;
static BYTE szMessage[512];
va_list va2;
PROCINFO Proc;
ULONG ulmSecs = *pGITicks;
USHORT usThreadID;

   _asm push es;

   if (! fTraceInitted)
      {
      fTraceEnabled = pGI->amecRAS[TRACE_MAJOR / 8] & (0x80 >> (TRACE_MAJOR % 8));
      }
   fTraceInitted = TRUE;

   if (fTraceEnabled)
      {
      // output to trace buffer
      va_copy(va2, va);
      trace(pszMessage, va2);
      va_end(va2);
      }

   if (!(f32Parms.fMessageActive & usLevel))
      {
      va_end(va);
      _asm pop es;
      return;
      }

   if (! fLogPrintInitted)
      {
      LogPrintInit();
      fLogPrintInitted = 1;
      }

   FSH_QSYSINFO(2, (PBYTE)&Proc, sizeof Proc);
   FSH_QSYSINFO(3, (PBYTE)&usThreadID, 2);
   memset(szMessage, 0, sizeof szMessage);
   sprintf(szMessage, "P:%X T:%X D:%X T:%u:%u ",
      Proc.usPid,
      usThreadID,
      Proc.usPdb,
      (USHORT)((ulmSecs / 1000) % 60),
      (USHORT)(ulmSecs % 1000));

   vsprintf(szMessage + strlen(szMessage), pszMessage, va);

   // output to FAT32 log buffer
   fWriteLogging(szMessage);

   if (LogPrint) // if QSINIT/OS4LDR found
      (*LogPrint)("%s\n", szMessage);
   else // else output directly to com port
      serout(serial_hw_port, szMessage);

   _asm pop es;
}

VOID cdecl _loadds Message(PSZ pszMessage, ...)
{
va_list va;

   va_start(va, pszMessage);
   message(0xffff, pszMessage, va);
   va_end(va);
}

VOID cdecl _loadds MessageL(USHORT usLevel, PSZ pszMessage, ...)
{
va_list va;

   va_start(va, pszMessage);
   message(usLevel, pszMessage, va);
   va_end(va);
}

BOOL fWriteLogging(PSZ pszMessage)
{
APIRET rc;
USHORT usNeeded;
USHORT usSize;
PSZ    p1, p2;

   usNeeded = strlen(pszMessage) + 2;
   usSize = strlen(szLogBuf);
   if (usSize + usNeeded >= LOGBUF_SIZE)
      {
      usNeeded += strlen(szLost);
      p2 = szLogBuf;
      while (usSize + usNeeded >= LOGBUF_SIZE)
         {
         p1 = strchr(p2, '\n');
         if (p1)
            {
            usSize -= ((p1 - p2) + 1);
            p2 = p1 + 1;
            }
         else
            {
            return TRUE;
            }
         }

      memcpy(szLogBuf, szLost, strlen(szLost));
      memmove(szLogBuf + strlen(szLost), p2, strlen(p2) + 1);
      }

   strcat(szLogBuf + usSize, pszMessage);
   strcat(szLogBuf + usSize, "\r\n");

   fData = TRUE;
   rc = FSH_SEMCLEAR(&ulLogSem);

   if (usSize)
      Yield();

   return TRUE;
}

USHORT GetLogBuffer(PBYTE pData, USHORT cbData, ULONG ulTimeOut)
{
APIRET rc;

   if (f32Parms.fInShutDown)
      return ERROR_ALREADY_SHUTDOWN;

   rc = MY_PROBEBUF(PB_OPWRITE, pData, cbData);
   if (rc)
      {
      Message("FAT32: Protection VIOLATION in GetLogBuffer! (SYS%d)", rc);
      return rc;
      }

   while (!fData && !f32Parms.fInShutDown)
   {
       rc = FSH_SEMSETWAIT(&ulLogSem,ulTimeOut);

       if (rc != NO_ERROR && rc != ERROR_SEM_TIMEOUT)
       {
           return rc;
       }
   }

   if (f32Parms.fInShutDown)
      return ERROR_ALREADY_SHUTDOWN;

   strncpy(pData, szLogBuf, cbData);
   if (strlen(szLogBuf) > cbData)
      {
      memmove(szLogBuf, szLogBuf + cbData, strlen(szLogBuf + cbData) + 1);
      rc = ERROR_BUFFER_OVERFLOW;
      }
   else
      {
      *szLogBuf = 0;
      fData = FALSE;
      rc = 0;
      }

   return rc;
}

/* Read a byte from a port.  */
static _inline unsigned char
inb (unsigned short port)
{
  unsigned char value;

  _asm {
    mov dx, port
    in  al, dx
    out 80h, al
    mov value, al
  }

  return value;
}

/* Write a byte to a port.  */
static _inline void
outb (unsigned short port, unsigned char value)
{
  _asm {
    mov dx, port
    mov al, value
    out dx, al
    out 80h, al
  }
}

void comwait(unsigned short port)
{
  while (!(inb(port + 5) & 0x20)) ; // wait while comport is ready
}

void comout(unsigned short port, unsigned char c)
{
  //if (!debug)
  //  return;

  comwait(port);
  outb(port, c);
}

void serout(unsigned short port, char *s)
{
  char *p = s;

  //if (!debug) return;

  while (*p)
  {
    if (*p == '\n')
    {
      comout(port, '\r');
      comout(port, '\n');
    }
    else
      comout(port, *p);
    p++;
  }
  comout(port, '\r');
  comout(port, '\n');
}

// accessing the system trace
APIRET trace(const char *fmt, va_list va)
{
static BOOL fInitted = FALSE;
static BOOL fTraceEnabled;
static char pData[512];
USHORT cbData = 0;
BOOL fLong, fLongLong;
USHORT minor = 0;
BYTE bToken;
char *p = (char *)fmt;
char *pBuf = pData;
char *pszValue;
APIRET rc;

   if (! fInitted)
      {
      // check if tracing with TRACE_MAJOR is enabled
      fTraceEnabled = pGI->amecRAS[TRACE_MAJOR / 8] & (0x80 >> (TRACE_MAJOR % 8));
      }
   fInitted = TRUE;

   if (! fTraceEnabled)
      return 0;

   while (*p)
      {
      p = strchr(p, '%');
      if (!p)
         break;
      fLong = FALSE;
      fLongLong = FALSE;
      p++;
      if (*p == 'l')
         {
         if (p[1] == 'l')
            {
            fLongLong = TRUE;
            p += 2;
            }
         else
            {
            fLong = TRUE;
            p++;
            }
         }
      bToken = *p;
      if (*p)
         p++;
      switch (bToken)
         {
         case '%':
            *pBuf = '%';
            pBuf += sizeof(char);
            cbData += sizeof(char);
            break;

         case 'c':
            *pBuf = (char)va_arg(va, USHORT);
            pBuf += sizeof(char);
            cbData += sizeof(char);
            break;

         case 'm':
            // system trace minor ID
            minor = va_arg(va, USHORT);
            break;

         case 's':
            pszValue = va_arg(va, char *);
            if (pszValue)
               {
               if (MY_PROBEBUF(PB_OPREAD, pszValue, 1))
                  pszValue = "(bad address)";
               }
            else
               pszValue = "(null)";
            strcpy(pBuf, pszValue);
            pBuf += strlen(pszValue) + 1;
            cbData += strlen(pszValue) + 1;
            break;

         case 'u':
         case 'd':
         case 'x':
         case 'X':
            if (fLongLong)
               {
               *(ULONGLONG *)pBuf = va_arg(va, ULONGLONG);
               pBuf += sizeof(ULONGLONG);
               cbData += sizeof(ULONGLONG);
               }
            else if (fLong)
               {
               *(ULONG *)pBuf = va_arg(va, ULONG);
               pBuf += sizeof(ULONG);
               cbData += sizeof(ULONG);
               }
            else
               {
               *(USHORT *)pBuf = va_arg(va, USHORT);
               pBuf += sizeof(USHORT);
               cbData += sizeof(USHORT);
               }
            break;
         }
      }

   if (minor)
      {
      rc = DevHelp_RAS(TRACE_MAJOR, minor, cbData, pData);
      }

   return rc;
}

APIRET Trace(const char *fmt, ...)
{
va_list va;
APIRET rc;

   va_start(va, fmt);
   rc = trace(fmt, va);
   va_end(va);
   return rc;
}
