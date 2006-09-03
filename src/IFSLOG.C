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

#define LOGBUF_SIZE 8192

static BYTE szLogBuf[LOGBUF_SIZE] = "";

static BYTE szLost[]="(information lost)\r\n";
static ULONG ulLogSem = 0UL;

static BOOL fWriteLogging(PSZ pszMessage);

VOID cdecl Message(PSZ pszMessage, ...)
{
static BYTE szMessage[512];
va_list va;
PROCINFO Proc;
ULONG ulmSecs = *pGITicks;
USHORT usThreadID;

   if (!f32Parms.fMessageActive)
      return;

   va_start(va, pszMessage);

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

   va_end( va );

   fWriteLogging(szMessage);
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

   if (!f32Parms.fInShutDown)
   {
       rc = FSH_SEMSETWAIT(&ulLogSem,ulTimeOut);
       if (rc != NO_ERROR)
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
      rc = 0;
      }

   return rc;
}


