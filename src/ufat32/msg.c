#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "fat32c.h"

int logbufsize = 0;
char *logbuf = NULL;
int  logbufpos = 0;

PSZ GetOS2Error(USHORT rc)
{
static BYTE szErrorBuf[MAX_MESSAGE] = "";
static BYTE szErrNo[12] = "";
ULONG rc2;
ULONG ulReplySize;

   memset(szErrorBuf, 0, sizeof(szErrorBuf));
   if (rc)
      {
      sprintf(szErrNo, "SYS%4.4u: ", rc);
      rc2 = DosGetMessage(NULL, 0, szErrorBuf, sizeof(szErrorBuf),
                          rc, "OSO001.MSG", &ulReplySize);
      switch (rc2)
         {
         case NO_ERROR:
            strcat(szErrorBuf, "\n");
            break;
         case ERROR_FILE_NOT_FOUND :
            sprintf(szErrorBuf, "SYS%04u (Message file not found!)", rc);
            break;
         default:
            sprintf(szErrorBuf, "SYS%04u (Error %d while retrieving message text!)", rc, rc2);
            break;
         }
      DosGetMessage(NULL,
         0,
         szErrorBuf + strlen(szErrorBuf),
         sizeof(szErrorBuf) - strlen(szErrorBuf),
         rc,
         "OSO001H.MSG",
         &ulReplySize);
      }

   if (memicmp(szErrorBuf, "SYS", 3))
      {
      memmove(szErrorBuf + strlen(szErrNo), szErrorBuf, strlen(szErrorBuf) + 1);
      memcpy(szErrorBuf, szErrNo, strlen(szErrNo));
      }
   return szErrorBuf;
}

INT cdecl iShowMessage2(PCDINFO pCD, USHORT usNr, USHORT usNumFields, va_list va)
{
static BYTE szErrNo[12] = "";
static BYTE szMessage[MAX_MESSAGE];
static BYTE szOut[MAX_MESSAGE];
static BYTE rgNum[9][50];
ULONG ulReplySize;
ULONG rc;
PSZ    rgPSZ[9];
USHORT usIndex;
PSZ    pszMess;

   memset(szMessage, 0, sizeof(szMessage));
   rc = DosGetMessage(NULL, 0, szMessage, sizeof(szMessage),
                          usNr, "OSO001.MSG", &ulReplySize);
   switch (rc)
      {
      case NO_ERROR:
         break;
      case ERROR_FILE_NOT_FOUND :
         sprintf(szMessage, "SYS%04u (Message file not found!)", usNr);
         break;
      default:
         sprintf(szMessage, "SYS%04u (Error %d while retrieving message text!)", usNr, rc);
         break;
      }
   if (rc)
      {
      printf("%s\n", szMessage);
      return strlen(szMessage);
      }

   pszMess = szMessage;

   memset(szOut, 0, sizeof(szOut));
   if (pCD && pCD->fPM)
      sprintf(szOut, "MSG_%u", usNr);

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
      if (pCD && pCD->fPM)
         {
         PSZ p;
         strcat(szOut, ", ");
         p = rgPSZ[usIndex];
         while (*p == 0x20)
            p++;
         strcat(szOut, p);
         }
      }

   if (pCD && pCD->fPM)
      {
      strcat(szOut, "\n");
/*
      printf(szOut);
      return usNumFields;
*/
      }

   rc = DosInsertMessage(rgPSZ,
      usNumFields,
      pszMess,
      strlen(pszMess),
      szOut + strlen(szOut),
      sizeof(szOut) - strlen(szOut),
      &ulReplySize);
   printf("%s", szOut);

   return strlen(szOut);
}

INT cdecl iShowMessage(PCDINFO pCD, USHORT usNr, USHORT usNumFields, ...)
{
va_list va;
int len;

   va_start(va, usNumFields);
   len = iShowMessage2(pCD, usNr, usNumFields, va);
   va_end( va );

   return len;
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
