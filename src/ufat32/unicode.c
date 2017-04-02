#include <malloc.h>
#include <string.h>
#include <stdio.h>

#include "fat32c.h"

#include <uconv.h>

#define MAX_TRANS_TABLE     0x100
#define ARRAY_TRANS_TABLE   ( 0x10000 / MAX_TRANS_TABLE )

#define INDEX_OF_START      MAX_TRANS_TABLE
#define INDEX_OF_FIRSTINFO  MAX_TRANS_TABLE
#define INDEX_OF_LCASECONV  ( MAX_TRANS_TABLE + 1 )
#define INDEX_OF_END        INDEX_OF_LCASECONV
#define EXTRA_ELEMENT       ( INDEX_OF_END - INDEX_OF_START + 1 )

#define ARRAY_COUNT_PAGE    4
#define MAX_ARRAY_PAGE      ( 0x100 / ARRAY_COUNT_PAGE )

#define ARRAY_COUNT_UNICODE 256
#define MAX_ARRAY_UNICODE   (( USHORT )( 0x10000L / ARRAY_COUNT_UNICODE ))

VOID SetUni2NLS( USHORT usPage, USHORT usChar, USHORT usCode );
VOID TranslateAllocBuffer( VOID );

extern F32PARMS f32Parms;

USHORT _Far16 _Pascal _loadds GETSHRSEG(PVOID16 *p);

int (* CALLCONV pUniCreateUconvObject)(UniChar * code_set, UconvObject * uobj);
int (* CALLCONV pUniUconvToUcs)(
             UconvObject uobj,         /* I  - Uconv object handle         */
             void    * * inbuf,        /* IO - Input buffer                */
             size_t    * inbytes,      /* IO - Input buffer size (bytes)   */
             UniChar * * outbuf,       /* IO - Output buffer size          */
             size_t    * outchars,     /* IO - Output size (chars)         */
             size_t    * subst  );     /* IO - Substitution count          */
int (* CALLCONV pUniUconvFromUcs)(
             UconvObject uobj,
             UniChar * * inbuf,
             size_t    * inchars,
             void    * * outbuf,
             size_t    * outbytes,
             size_t    * subst  );
int (* CALLCONV pUniMapCpToUcsCp)( ULONG ulCp, UniChar *ucsCp, size_t n );
UniChar (* CALLCONV pUniTolower )( UniChar uin );
int (* CALLCONV pUniQueryUconvObject )
    (UconvObject uobj, uconv_attribute_t *attr, size_t size, char first[256], char other[256], udcrange_t udcrange[32]);

static ULONG     ulNewCP = 0;

typedef struct _UniPage
{
USHORT usCode[256];
} UNIPAGE, *PUNIPAGE;

static PUSHORT  rgUnicode[ ARRAY_COUNT_UNICODE ] = { NULL, };

UCHAR rgFirstInfo[ 256 ] = { 0 , };
static PUNIPAGE rgPage[ ARRAY_COUNT_PAGE ] = { NULL, };

static UCHAR rgLCase[ 256 ] =
{   0,
    1,   2,   3,   4,   5,   6,   7,   8,   9,  10,
   11,  12,  13,  14,  15,  16,  17,  18,  19,  20,
   21,  22,  23,  24,  25,  26,  27,  28,  29,  30,
   31,  32,  33,  34,  35,  36,  37,  38,  39,  40,
   41,  42,  43,  44,  45,  46,  47,  48,  49,  50,
   51,  52,  53,  54,  55,  56,  57,  58,  59,  60,
   61,  62,  63,  64,
  'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
  'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
   91,  92,  93,  94,  95,  96,  97,  98,  99, 100,
  101, 102, 103, 104, 105, 106, 107, 108, 109, 110,
  111, 112, 113, 114, 115, 116, 117, 118, 119, 120,
  121, 122, 123, 124, 125, 126, 127, 128, 129, 130,
  131, 132, 133, 134, 135, 136, 137, 138, 139, 140,
  141, 142, 143, 144, 145, 146, 147, 148, 149, 150,
  151, 152, 153, 154, 155, 156, 157, 158, 159, 160,
  161, 162, 163, 164, 165, 166, 167, 168, 169, 170,
  171, 172, 173, 174, 175, 176, 177, 178, 179, 180,
  181, 182, 183, 184, 185, 186, 187, 188, 189, 190,
  191, 192, 193, 194, 195, 196, 197, 198, 199, 200,
  201, 202, 203, 204, 205, 206, 207, 208, 209, 210,
  211, 212, 213, 214, 215, 216, 217, 218, 219, 220,
  221, 222, 223, 224, 225, 226, 227, 228, 229, 230,
  231, 232, 233, 234, 235, 236, 237, 238, 239, 240,
  241, 242, 243, 244, 245, 246, 247, 248, 249, 250,
  251, 252, 253, 254, 255
};

HMODULE hModConv = 0;
HMODULE hModUni  = 0;

/************************************************************************
*
************************************************************************/
USHORT QueryUni2NLS( USHORT usPage, USHORT usChar )
{
    return rgPage[ usPage / MAX_ARRAY_PAGE ][ usPage % MAX_ARRAY_PAGE ].usCode[ usChar ];
}

/******************************************************************
*
******************************************************************/
USHORT QueryNLS2Uni( USHORT usCode )
{
    return rgUnicode[ usCode / MAX_ARRAY_UNICODE ][ usCode % MAX_ARRAY_UNICODE ];
}

/******************************************************************
*
******************************************************************/
VOID GetFirstInfo( PBOOL pFirstInfo )
{
    memcpy( pFirstInfo, rgFirstInfo, sizeof( rgFirstInfo ));
}

/******************************************************************
*
******************************************************************/
BOOL TranslateInit(PVOID16 rgTrans[], USHORT usSize)
{
   ULONG  ulCode;
   USHORT usPage;
   USHORT usChar;
   INT    iIndex;

   PVOID16 *prgTrans = ( PVOID16 * )rgTrans;

   if( rgPage[ 0 ] == NULL )
      TranslateAllocBuffer();

   if (usSize != sizeof( PVOID ) * ( ARRAY_COUNT_UNICODE + 2 ) )
      return FALSE;

   for( iIndex = 0; iIndex < ARRAY_COUNT_UNICODE; iIndex++ )
      memcpy( rgUnicode[ iIndex ], (void *)(void _Far16 *)prgTrans[ iIndex ], sizeof( USHORT ) * MAX_ARRAY_UNICODE );

   for( iIndex = 0; iIndex < MAX_ARRAY_UNICODE; iIndex++ )
      rgFirstInfo[ iIndex ] = ( UCHAR )((( PUSHORT )( prgTrans[ ARRAY_COUNT_UNICODE ] ))[ iIndex ]);

   for( iIndex = 0; iIndex < MAX_ARRAY_UNICODE; iIndex++ )
      rgLCase[ iIndex ] = ( UCHAR )((( PUSHORT )( prgTrans[ ARRAY_COUNT_UNICODE + 1 ] ))[ iIndex ]);

   for( iIndex = 0; iIndex < ARRAY_COUNT_PAGE; iIndex++ )
      memset( rgPage[ iIndex ], '_', sizeof( UNIPAGE ) * MAX_ARRAY_PAGE );

   for (ulCode = 0; ulCode < 0x10000; ulCode++)
      {
      usPage = (QueryNLS2Uni(( USHORT )ulCode ) >> 8) & 0x00FF;
      usChar = QueryNLS2Uni(( USHORT )ulCode ) & 0x00FF;

      SetUni2NLS( usPage, usChar, ( USHORT )ulCode );
      }

   //f32Parms.fTranslateNames = TRUE;

   return TRUE;
}

/******************************************************************
*
******************************************************************/
VOID TranslateAllocBuffer( VOID )
{
   INT iIndex;

   for( iIndex = 0; iIndex < ARRAY_COUNT_PAGE; iIndex++ )
      rgPage[ iIndex ] = malloc( sizeof( UNIPAGE ) * MAX_ARRAY_PAGE );

   for( iIndex = 0; iIndex < ARRAY_COUNT_UNICODE; iIndex++ )
      rgUnicode[ iIndex ] = malloc( sizeof( USHORT ) * MAX_ARRAY_UNICODE );
}

/******************************************************************
*
******************************************************************/
BOOL LoadTranslateTable(BOOL fSilent, UCHAR ucSource)
{
    APIRET rc;
    ULONG ulParmSize;
    BYTE   rgData[ 256 ];
    // Extra space for DBCS lead info and case conversion
    UniChar *rgTranslate[ MAX_TRANS_TABLE + EXTRA_ELEMENT ] = { NULL, };
    PBYTE  pChar;
    UniChar *pUni;
    UconvObject  uconv_object = NULL;
    INT iIndex;
    size_t bytes_left;
    size_t uni_chars_left;
    size_t num_subs;
    ULONG rgCP[3];
    ULONG cbCP;
    // Extra space for DBCS lead info and case conversion
    PVOID16 rgTransTable[ MAX_TRANS_TABLE + EXTRA_ELEMENT ] = { NULL, };
    char rgFirst[ 256 ];
    USHORT first, second;
    USHORT usCode;
    UniChar ucsCp[ 12 ];
    UniChar rgUniBuffer[ ARRAY_TRANS_TABLE ];

   rc = DosLoadModule(rgData, sizeof rgData, "UCONV.DLL", &hModConv);
   if (rc)
      {
      printf("FAT32: No NLS support found (%s does not load).\n", rgData);
      printf("FAT32: No UNICODE translate table loaded!\n");
      rc = TRUE;
      goto free_exit;
      }
   rc = DosQueryProcAddr(hModConv, 0L,
      "UniCreateUconvObject", (PFN *)&pUniCreateUconvObject);
   if (rc)
      {
      printf("FAT32: ERROR: Could not find address of UniCreateUconvObject.\n");
      rc = FALSE;
      goto free_exit;
      }
   rc = DosQueryProcAddr(hModConv, 0L,
      "UniUconvToUcs", (PFN *)&pUniUconvToUcs);
   if (rc)
      {
      printf("FAT32: ERROR: Could not find address of UniUconvToUcs.\n");
      rc = FALSE;
      goto free_exit;
      }

   rc = DosQueryProcAddr(hModConv, 0L,
      "UniUconvFromUcs", (PFN *)&pUniUconvFromUcs);
   if (rc)
      {
      printf("FAT32: ERROR: Could not find address of UniUconvFromUcs.\n");
      rc = FALSE;
      goto free_exit;
      }

   rc = DosQueryProcAddr(hModConv, 0L,
      "UniMapCpToUcsCp", (PFN *)&pUniMapCpToUcsCp);
   if (rc)
      {
      printf("FAT32: ERROR: Could not find address of UniMapCpToUcsCp.\n");
      rc = FALSE;
      goto free_exit;
      }

   rc = DosQueryProcAddr(hModConv, 0L,
      "UniQueryUconvObject", (PFN *)&pUniQueryUconvObject);
   if (rc)
      {
      printf("FAT32: ERROR: Could not find address of UniQueryUconvObject.\n");
      rc = FALSE;
      goto free_exit;
      }

   rc = DosLoadModule(rgData, sizeof rgData, "LIBUNI.DLL", &hModUni);
   if (rc)
      {
      printf("FAT32: No NLS support found (%s does not load).\n", rgData);
      printf("FAT32: No UNICODE translate table loaded!\n");
      rc = TRUE;
      goto free_exit;
      }

   rc = DosQueryProcAddr(hModUni, 0L,
      "UniTolower", (PFN *)&pUniTolower);
   if (rc)
      {
      printf("FAT32: ERROR: Could not find address of UniTolower.\n");
      rc = FALSE;
      goto free_exit;
      }

   if( ulNewCP )
        rgCP[ 0 ] = ulNewCP;
   else
        DosQueryCp(sizeof rgCP, rgCP, &cbCP);

   if (f32Parms.ulCurCP == rgCP[0])
   {
      rc = FALSE;
      goto free_exit;
   }

#if 0
   if (f32Parms.ulCurCP && !fSayYes)
      {
      BYTE chChar;
      printf("Loaded unicode translate table is for CP %lu\n", f32Parms.ulCurCP);
      printf("Current CP is %lu\n", rgCP[0]);
      printf("Would you like to reload the translate table for this CP [Y/N]? ");
      fflush(stdout);

      for (;;)
         {
         chChar = getch();
         switch (chChar)
            {
            case 'y':
            case 'Y':
               chChar = 'Y';
               break;
            case 'n':
            case 'N':
               chChar = 'N';
               break;
            default :
               DosBeep(660, 10);
               continue;
            }
         printf("%c\n", chChar);
         break;
         }
      if (chChar == 'N')
         {
         rc = FALSE;
         goto free_exit;
         }
      }
#endif

   rc = pUniMapCpToUcsCp( rgCP[ 0 ], ucsCp, sizeof( ucsCp ) / sizeof( UniChar ));
   if( rc != ULS_SUCCESS )
   {
        printf("FAT32: ERROR: UniMapCpToUcsCp error: return code = %u\n", rc );
        rc = FALSE;
        goto free_exit;
   }

   rc = pUniCreateUconvObject( ucsCp, &uconv_object);
   if (rc != ULS_SUCCESS)
      {
      printf("FAT32: ERROR: UniCreateUconvObject error: return code = %u\n", rc);
      rc = FALSE;
      goto free_exit;
      }

   rc = pUniQueryUconvObject( uconv_object, NULL, 0, rgFirst, NULL, NULL );
   if (rc != ULS_SUCCESS)
      {
      printf("FAT32: ERROR: UniQueryUConvObject error: return code = %u\n", rc);
      rc = FALSE;
      goto free_exit;
      }

   // Allocation for conversion, DBCS lead info and case conversion
   for( iIndex = 0; iIndex <= INDEX_OF_END ; iIndex ++ )
   {
        rgTransTable[ iIndex ] = rgTranslate[ iIndex ] = malloc( sizeof(USHORT ) * ARRAY_TRANS_TABLE );
        memset( rgTranslate[ iIndex ], 0, sizeof( USHORT ) * ARRAY_TRANS_TABLE );
   }

   // Initialize SBCS only for conversion and set DBCS lead info
   for( iIndex = 0; iIndex < ARRAY_TRANS_TABLE; iIndex++ )
   {
        rgData[ iIndex ] = ( rgFirst[ iIndex ] == 1 ) ? iIndex : 0;
        rgTranslate[ INDEX_OF_FIRSTINFO ][ iIndex ] = rgFirst[ iIndex ];
   }

   pChar = rgData;
   bytes_left = sizeof rgData;
   pUni = ( PVOID )rgTranslate[ 0 ];
   uni_chars_left = ARRAY_TRANS_TABLE;

   rc = pUniUconvToUcs(uconv_object,
      (PVOID *)&pChar,
      &bytes_left,
      &pUni,
      &uni_chars_left,
      &num_subs);

   if (rc != ULS_SUCCESS)
      {
      printf("FAT32: ERROR: UniUconvToUcs failed, rc = %u\n", rc);
      rc = FALSE;
      goto free_exit;
      }

   // Translate upper case to lower case
   for( iIndex = 0; iIndex < ARRAY_TRANS_TABLE; iIndex++ )
        rgUniBuffer[ iIndex ] = pUniTolower( rgTranslate[ 0 ][ iIndex ] );

   // Convert lower case in Unicode to codepage code
   pUni = ( PVOID )rgUniBuffer;
   uni_chars_left = ARRAY_TRANS_TABLE;
   pChar  = rgData;
   bytes_left = sizeof rgData;

   rc = pUniUconvFromUcs( uconv_object,
        &pUni,
        &uni_chars_left,
        ( PVOID * )&pChar,
        &bytes_left,
        &num_subs );

   if (rc != ULS_SUCCESS)
      {
      printf("FAT32: ERROR: UniUconvFromUcs failed, rc = %u\n", rc);
      rc = FALSE;
      goto free_exit;
      }

   // Store codepage code to transtable
   for( iIndex = 0; iIndex < ARRAY_TRANS_TABLE; iIndex++ )
        rgTranslate[ INDEX_OF_LCASECONV ][ iIndex ] = rgData[ iIndex ] ? rgData[ iIndex ] : iIndex;

   // Translate DBCS code to unicode
   for( first = 0; first < ARRAY_TRANS_TABLE; first++ )
   {
        if( rgFirst[ first ] == 2 )
        {
            for( second = 0; second < 0x100; second++ )
            {
                  usCode = first | (( second << 8 ) & 0xFF00 );

                  pChar  = ( PVOID )&usCode;
                  bytes_left = sizeof usCode;
                  pUni = ( PVOID )&rgTranslate[ second ][ first ];
                  uni_chars_left = 1;

                  rc = pUniUconvToUcs(uconv_object,
                     (PVOID *)&pChar,
                     &bytes_left,
                     &pUni,
                     &uni_chars_left,
                     &num_subs);

                  if (rc != ULS_SUCCESS)
                  {
                     printf("FAT32: ERROR: UniUconvToUcs failed, rc = %u\n", rc);
                     rc = FALSE;
                     goto free_exit;
                  }
            }
        }
   }

   switch (ucSource)
   {
   case 0:
      {
         // called from CHKDSK or IFS init routine
         ulParmSize = sizeof rgTransTable;
         if( !TranslateInit((PVOID)rgTransTable, ulParmSize) )
            rc = ERROR_INVALID_PARAMETER;
         else
            rc = 0;
      }
      break;

   case 1:
      {
         // called from cachef32.exe
         ulParmSize = sizeof rgTransTable;
         rc = DosFSCtl(NULL, 0, NULL,
                     ( PVOID )rgTransTable, ulParmSize, &ulParmSize,
                     FAT32_SETTRANSTABLE, "FAT32", -1, FSCTL_FSDNAME);
      }
      break;
#ifdef __DLL__
   case 2:
      {
         // called from IFS init
         PVOID16 p;
         ulParmSize = sizeof rgTransTable;
         if( !GETSHRSEG(&p) )
            rc = ERROR_INVALID_PARAMETER;
         else
         {
            rc = 0;
            memcpy((PVOID)p, (PVOID)rgTransTable, ulParmSize);
         }
      }
#endif
   }
   
   if (rc)
      {
      printf("FAT32: ERROR: Unable to set translate table for current Codepage.\n");
      rc = FALSE;
      goto free_exit;
      }

   f32Parms.ulCurCP = rgCP[0];
   if( ! fSilent )
       printf("FAT32: Unicode translate table for CP %lu loaded.\n", rgCP[0]);
   rc = TRUE;
free_exit:

   for( iIndex = 0; iIndex <= INDEX_OF_END; iIndex++ )
      if( rgTranslate[ iIndex ])
        free( rgTranslate[ iIndex ]);

   if( hModConv )
        DosFreeModule( hModConv);

   if( hModUni )
        DosFreeModule( hModUni );

   return rc;
}

/******************************************************************
*
******************************************************************/
VOID SetUni2NLS( USHORT usPage, USHORT usChar, USHORT usCode )
{
    rgPage[ usPage / MAX_ARRAY_PAGE ][ usPage % MAX_ARRAY_PAGE ].usCode[ usChar ] = usCode;
}

/******************************************************************
*
******************************************************************/
VOID GetCaseConversion( PUCHAR pCase )
{
    memcpy( pCase, rgLCase, sizeof( rgLCase ));
}

VOID TranslateInitDBCSEnv( VOID )
{
   GetFirstInfo(( PBOOL )rgFirstInfo );
}

VOID CaseConversionInit( VOID )
{
   GetCaseConversion(rgLCase);
}

void CodepageConvInit(BOOL fSilent)
{
   LoadTranslateTable(fSilent, FALSE);
   TranslateInitDBCSEnv();
   CaseConversionInit();
}
