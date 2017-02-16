#include <malloc.h>
#include <string.h>

#include "fat32c.h"
#include "portable.h"

int logbufsize = 0;
char *logbuf = NULL;
int  logbufpos = 0;

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
