#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>

#define INCL_DOS
#define INCL_DOSDEVIOCTL
#define INCL_DOSDEVICES
#define INCL_DOSERRORS
#define INCL_DOSNLS

#include "os2.h"
#include "portable.h"
#include "fat32ifs.h"

typedef struct _UniPage
{
USHORT usCode[256];
} UNIPAGE, *PUNIPAGE;

#define ARRAY_COUNT_PAGE    4
#define MAX_ARRAY_PAGE      ( 0x100 / ARRAY_COUNT_PAGE )

#define ARRAY_COUNT_UNICODE 256
#define MAX_ARRAY_UNICODE   (( USHORT )( 0x10000L / ARRAY_COUNT_UNICODE ))

static PUNIPAGE rgPage[ ARRAY_COUNT_PAGE ] = { NULL, };
static PUSHORT  rgUnicode[ ARRAY_COUNT_UNICODE ] = { NULL, };

static UCHAR rgDBCSLead[ 12 ] = { 0, };

PRIVATE USHORT QueryUni2NLS( USHORT usPage, USHORT usChar );
PRIVATE VOID   SetUni2NLS( USHORT usPage, USHORT usChar, USHORT usCode );
PRIVATE USHORT QueryNLS2Uni( USHORT usCode );
PRIVATE USHORT GetCurrentCodePage(VOID);

VOID TranslateInitDBCSEnv( VOID )
{
#if 1
   memset( rgDBCSLead, 0, sizeof( rgDBCSLead ));

   switch( f32Parms.ulCurCP )
      {
      case 934L :
      case 944L :
         rgDBCSLead[ 0 ] = 0x81; rgDBCSLead[ 1 ] = 0xBF;
         break;

      case 936L :
      case 938L :
      case 946L :
      case 948L :
         rgDBCSLead[ 0 ] = 0x81; rgDBCSLead[ 1 ] = 0xFC;
         break;

      case 932L :
      case 942L :
      case 943L :
         rgDBCSLead[ 0 ] = 0x81; rgDBCSLead[ 1 ] = 0x9F;
         rgDBCSLead[ 2 ] = 0xE0; rgDBCSLead[ 3 ] = 0xFC;
         break;

      case 949L :
         rgDBCSLead[ 0 ] = 0x8F; rgDBCSLead[ 1 ] = 0xFE;
         break;

      case 950L :
         rgDBCSLead[ 0 ] = 0x81; rgDBCSLead[ 1 ] = 0xFE;
         break;

      case 1207L :
         rgDBCSLead[ 0 ] = 0x80; rgDBCSLead[ 1 ] = 0xFF;
         break;

      case 1381L :
         rgDBCSLead[ 0 ] = 0x8C; rgDBCSLead[ 1 ] = 0xFE;
         break;

      case 1386L :
         rgDBCSLead[ 0 ] = 0x81; rgDBCSLead[ 1 ] = 0xFE;
         break;
      }
#else      /* below code causes TRAP D on Warp 4 for Korean and WSeB for US */
   COUNTRYCODE cc;

   cc.country = 0;
   cc.codepage = ( USHORT )f32Parms.ulCurCP;
   DosGetDBCSEv( sizeof( rgDBCSLead ), &cc, rgDBCSLead );
#endif
}

BOOL IsDBCSLead( USHORT usChar )
{
   USHORT usIndex;

   for( usIndex = 0; ( usIndex < sizeof(rgDBCSLead)) &&
                     ( rgDBCSLead[ usIndex ] != 0 || rgDBCSLead[ usIndex + 1 ] != 0 ); usIndex += 2  )
      {
         if( usChar >= rgDBCSLead[ usIndex ] &&
             usChar <= rgDBCSLead[ usIndex + 1 ] )
            return TRUE;
      }

   return FALSE;
}

#if 1  /* by OAX */
/* Get the last-character. (sbcs/dbcs) */
int lastchar(const char *string)
{
    UCHAR *s;
    USHORT c = 0;
    int i, len = strlen(string);
    s = (UCHAR *)string;
    for(i = 0; i < len; i++)
    {
        c = *(s + i);
        if(IsDBCSLead(c))
        {
            c = (c << 8) + *(s + i + 1);
            i++;
        }
    }
    return c;
}

/* byte step DBCS type strchr() (but. different wstrchr()) */
char _FAR_ * _FAR_ _cdecl strchr(const char _FAR_ *string, int c)
{
    UCHAR *s;
    int  i, len = strlen(string);
    unsigned int ch;
    s = (UCHAR *)string;
    for(i = 0; i < len; i++)
    {
        ch = *(s + i);
        if(IsDBCSLead(ch))
            ch = (ch << 8) + *(s + i + 1);
        if(( USHORT )c == ch)
            return (s + i);
        if(ch & 0xFF00)
            i++;
    }
    return NULL;
}
/* byte step DBCS type strrchr() (but. different wstrrchr()) */
char _FAR_ * _FAR_ _cdecl strrchr(const char _FAR_ *string, int c)
{
    char *s, *lastpos;
    s = (char *)string;
    lastpos = strchr(s, c);
    if(!lastpos)
        return NULL;
    for(;;)
    {
        s = lastpos + 1;
        s = strchr(s, c);
        if(!s)
            break;
        lastpos = s;
    }
    return lastpos;
}
#endif /* by OAX */

VOID TranslateAllocBuffer( VOID )
{
   INT iIndex;

   for( iIndex = 0; iIndex < ARRAY_COUNT_PAGE; iIndex++ )
      rgPage[ iIndex ] = malloc( sizeof( UNIPAGE ) * MAX_ARRAY_PAGE );

   for( iIndex = 0; iIndex < ARRAY_COUNT_UNICODE; iIndex++ )
      rgUnicode[ iIndex ] = malloc( sizeof( USHORT ) * MAX_ARRAY_UNICODE );
}

VOID TranslateFreeBuffer( VOID )
{
   INT iIndex;

   for( iIndex = 0; iIndex < ARRAY_COUNT_PAGE; iIndex++ )
      free( rgPage[ iIndex ]);

   for( iIndex = 0; iIndex < ARRAY_COUNT_UNICODE; iIndex++ )
      free( rgUnicode[ iIndex ]);
}

USHORT Translate2Win(PSZ pszName, PUSHORT pusUni, USHORT usLen)
{
USHORT usCode;
USHORT usProcessedLen;

   usProcessedLen = 0;

   if (!f32Parms.fTranslateNames)
      {
      while (*pszName && usLen)
         {
         *pusUni++ = (USHORT)*pszName++;
         usLen--;
         usProcessedLen++;
         }
      return usProcessedLen;
      }

/*
   GetCurrentCodePage();
*/

   while (*pszName && usLen)
      {
      usCode = *pszName++;
      if( IsDBCSLead( usCode ))
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

VOID Translate2OS2(PUSHORT pusUni, PSZ pszName, USHORT usLen)
{
USHORT usPage;
USHORT usChar;
USHORT usCode;

   if (!f32Parms.fTranslateNames)
      {
      while (*pusUni && usLen)
         {
         *pszName++ = (BYTE)*pusUni++;
         usLen--;
         }

      return;
      }

/*
   GetCurrentCodePage();
*/

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

VOID TranslateInit(BYTE rgTrans[], USHORT usSize)
{
ULONG  ulCode;
USHORT usPage;
USHORT usChar;
INT    iIndex;

PVOID *prgTrans = ( PVOID * )rgTrans;

   if( rgPage[ 0 ] == NULL )
      TranslateAllocBuffer();

   if (usSize != sizeof( PVOID ) * ARRAY_COUNT_UNICODE )
      return;

   for( iIndex = 0; iIndex < ARRAY_COUNT_UNICODE; iIndex++ )
      memcpy( rgUnicode[ iIndex ], prgTrans[ iIndex ], sizeof( USHORT ) * MAX_ARRAY_UNICODE );

   for( iIndex = 0; iIndex < ARRAY_COUNT_PAGE; iIndex++ )
      memset( rgPage[ iIndex ], '_', sizeof( UNIPAGE ) * MAX_ARRAY_PAGE );

   for (ulCode = 0; ulCode < 0x10000; ulCode++)
      {
      usPage = (QueryNLS2Uni(( USHORT )ulCode ) >> 8) & 0x00FF;
      usChar = QueryNLS2Uni(( USHORT )ulCode ) & 0x00FF;

      SetUni2NLS( usPage, usChar, ( USHORT )ulCode );
      }

   f32Parms.fTranslateNames = TRUE;
}

USHORT QueryUni2NLS( USHORT usPage, USHORT usChar )
{
    return rgPage[ usPage / MAX_ARRAY_PAGE ][ usPage % MAX_ARRAY_PAGE ].usCode[ usChar ];
}

VOID SetUni2NLS( USHORT usPage, USHORT usChar, USHORT usCode )
{
    rgPage[ usPage / MAX_ARRAY_PAGE ][ usPage % MAX_ARRAY_PAGE ].usCode[ usChar ] = usCode;
}

USHORT QueryNLS2Uni( USHORT usCode )
{
    return rgUnicode[ usCode / MAX_ARRAY_UNICODE ][ usCode % MAX_ARRAY_UNICODE ];
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


