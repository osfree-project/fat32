#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>

#define INCL_DOS
#define INCL_DOSDEVIOCTL
#define INCL_DOSDEVICES
#define INCL_DOSERRORS

#include "os2.h"
#include "portable.h"
#include "fat32ifs.h"

typedef struct _UniPage
{
BYTE bAscii[256];
} UNIPAGE, *PUNIPAGE;

#define MAX_PAGES 0x26

static UNIPAGE rgPage[MAX_PAGES];
static USHORT  rgUnicode[256];

PRIVATE USHORT GetCurrentCodePage(VOID);

VOID Translate2Win(PSZ pszName, PUSHORT pusUni, USHORT usLen)
{
   if (!f32Parms.fTranslateNames)
      {
      while (*pszName && usLen)
         {
         *pusUni++ = (USHORT)*pszName++;
         usLen--;
         }
      return;
      }

//   GetCurrentCodePage();

   while (*pszName && usLen)
      {
      *pusUni++ = rgUnicode[*pszName++];
      usLen--;
      }
}

VOID Translate2OS2(PUSHORT pusUni, PSZ pszName, USHORT usLen)
{
USHORT usPage;
USHORT usChar;

   if (!f32Parms.fTranslateNames)
      {
      while (*pusUni && usLen)
         {
         *pszName++ = (BYTE)*pusUni++;
         usLen--;
         }

      return;
      }

//   GetCurrentCodePage();

   while (*pusUni && usLen)
      {
      usPage = ((*pusUni) >> 8) & 0x00FF;
      usChar = (*pusUni) & 0x00FF;
      if (usPage < MAX_PAGES)
         *pszName++ = rgPage[usPage].bAscii[usChar];
      else
         *pszName++ = '_';
      pusUni++;
      usLen--;
      }

}

VOID TranslateInit(BYTE rgTrans[], USHORT usSize)
{
USHORT usIndex;
USHORT usPage;
USHORT usChar;

   if (usSize != sizeof rgUnicode)
      return;

   memcpy(rgUnicode, rgTrans, sizeof rgUnicode);

   memset(rgPage, '_', sizeof rgPage);

   for (usIndex = 0; usIndex < 256; usIndex++)
      {
      usPage = ((rgUnicode[usIndex]) >> 8) & 0x00FF;
      usChar = (rgUnicode[usIndex] & 0x00FF);
      if (usPage < MAX_PAGES)
         rgPage[usPage].bAscii[usChar] = (BYTE)usIndex;
      }

   f32Parms.fTranslateNames = TRUE;
}


USHORT GetCurrentCodePage(VOID)
{
PULONG pulCP;
USHORT rc;

   rc = DevHelp_GetDOSVar(12,  0, &pulCP);
   if (rc)
      {
      if (f32Parms.fMessageActive)
         Message("GetDosVar returned %u", rc);
      return 0;
      }
   if (f32Parms.fMessageActive)
      Message("Current codepage tag at %lX = %lu", pulCP, *pulCP);

   rc = DevHelp_GetDOSVar(DHGETDOSV_DOSCODEPAGE,  0, &pulCP);
   if (rc)
      {
      if (f32Parms.fMessageActive)
         Message("GetDosVar returned %u", rc);
      return 0;
      }
   if (f32Parms.fMessageActive)
      Message("Current codepage tag at %lX = %lu", pulCP, *pulCP);
   return (USHORT)(*pulCP);
}
