#define INCL_DOSMODULEMGR
#define INCL_DOSPROCESS
#define INCL_DOSMISC
#include <os2.h>

#include "extboot.h"
#include "portable.h"
#include "fat32def.h"

/* globals */
ULONG autocheck_mask = 0;              /* global to be set by parse args for */
                                       /* drives to be autochecked           */
ULONG force_mask = 0;                  /* global to be set by parse args for */
                                       /* drives to be autochecked regardless*/
                                       /* of dirty status                    */

extern ULONG fat_mask;
extern ULONG fat32_mask;
#ifdef EXFAT
extern ULONG exfat_mask;
#endif

extern F32PARMS f32Parms;

UCHAR fRing3 = FALSE;

VOID _cdecl InitMessage(PSZ pszMessage, ...);
UCHAR GetFatType(PBOOTSECT pBoot);
BOOL TranslateInit(BYTE rgTrans[], USHORT usSize);

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

/*
 * NAME:        autocheck()
 *
 * FUNCTION:    invoke Autocheck for FAT32 drives
 *
 *
 * PARAMETERS:  cachef32.exe parameters
 *
 * RETURN:      void
 */

void _cdecl autocheck(char *args)
{
  PSZ        pszSharedMem    = "\\SHAREMEM\\FAT32\\TRANSTBL";
  PSZ        ProgramName     = "F32CHK.EXE";           /* special exe        */
  UCHAR      LoadError[64];                            /* for dosloadmodule  */
  UCHAR Arg_Buf[256];                       /* argument buffer               */
  USHORT env,cmd;
  RESULTCODES Result;
  USHORT len = 0;                           /* temp var for filling in buffer*/
  ULONG i;                                  /* loop count                    */
  char far * temp;

  char     drive[4];                        /* drive name ie C:              */
  HFILE    drive_handle;                    /* file handle of drive          */
  USHORT   action;                          /* return alue from dosopen      */
  char     boot_sector[512];                /* data area for boot sector     */
  ULONG    rc =0;
  USHORT   bytes_read;                      /* bytes read by dosread         */
  UCHAR    type;

  fRing3 = TRUE;

  /* get the current environment to be passed in on execpgm */
  DosGetEnv(&env, &cmd);

  for (i = 2; i <= 25; i++)  /* loop for max drives allowed */
  {
    if ( ! (autocheck_mask & (1UL << i)) )
    {
      // mask not on for this drive
      continue;  /* go to next drive */
    }

#ifdef EXFAT
    if ( ! (fat_mask   & (1UL << i)) &&
         ! (fat32_mask & (1UL << i)) &&
         ! (exfat_mask & (1UL << i)) )
#else
    if ( ! (fat_mask   & (1UL << i)) &&
         ! (fat32_mask & (1UL << i)) )
#endif
    {
      // skip disabled drive letters
      continue;  /* go to next drive */
    }

    /* set up the drive name to be used in DosOpen */
    drive[0] = (char)('a' + i);
    drive[1] = ':';
    drive[2] = 0;

    /* open the drive */
    rc = DosOpen(drive, &drive_handle, &action, 0L, 0, 1, 0x8020, 0L);

    /* if the open fails then just skip this drive */
    if (rc)
    {
      continue;
    }

    /* read sector 0 the boot sector of this drive */
    rc = DosRead(drive_handle, boot_sector, 512, &bytes_read);

    /* close the drive */
    DosClose(drive_handle);

    /* if the read fails then just skip this drive */
    if (rc)
    {
      continue;
    }

    /* check the system_id in the boot sector to be sure it is FAT */
    /* if it is not then we will just skip this drive              */
    /* due to link problems we can't use memcmp for this           */

    // get FAT type
    type = GetFatType((PBOOTSECT)boot_sector);

#ifdef EXFAT
    if ( (type == FAT_TYPE_NONE) ||
         (! f32Parms.fFat   && (type <  FAT_TYPE_FAT32)) ||
         (! f32Parms.fExFat && (type == FAT_TYPE_EXFAT)) )
#else
    if ( (type == FAT_TYPE_NONE) ||
         (! f32Parms.fFat   && (type <  FAT_TYPE_FAT32)) )
#endif
    {
      // skip unsupported (or disabled) FS'es
      continue;
    }

    /* write something to the screen.  This will prevent bvhvga.dll from */
    /* unloading when chkdsk exits. */
    DosPutMessage(0, 1, " ");

    len = 0;
    temp = ProgramName;

    while (*temp != 0)       /* copy prog name as first arg */
    {
      Arg_Buf[len] = *temp;
      len++;
      temp++;
    }

    Arg_Buf[len] = '\0'; /* special null after first arg, all others use blank */
    len++;

    Arg_Buf[len] = (char)('a' + i); /* parm 2 is the drive letter to be checked */
    len++;
    Arg_Buf[len] = ':';
    len++;
    Arg_Buf[len] = ' ';
    len++;

    Arg_Buf[len] = '/';     /* parm 3 is /F for fix it                  */
    len++;
    Arg_Buf[len] = 'F';
    len++;
    Arg_Buf[len] = ' ';
    len++;

    Arg_Buf[len] = '/';     /* parm 4 is /A for autocheck               */
    len++;
    Arg_Buf[len] = 'A';
    len++;
    Arg_Buf[len] = ' ';
    len++;

    /* if the force mask is set for this drive ommit the /C, otherwise */
    /* add it as parm 5 */
    if (! (force_mask & (1UL << i)) )
    {
      Arg_Buf[len] = '/';
      len++;
      Arg_Buf[len] = 'C';
      len++;
    }

    /* terminate the arg buf with a double null */
    Arg_Buf[len]='\0';
    len++;
    Arg_Buf[len]='\0';

    /* call exec PGM to execute chkdsk on this drive */
    rc = DosExecPgm(LoadError, sizeof(LoadError), 0, /* EXEC_SYNC */
                    Arg_Buf, MAKEP(env,0), &Result, ProgramName);

  } /* end for */

#if 0
  //if (! args)
  //  return;

  rc = DosAllocShrSeg(sizeof(PVOID) * (MAX_TRANS_TABLE + EXTRA_ELEMENT), pszSharedMem, &sel);

  if (rc)
     return;

  temp = ProgramName;
  len  = 0;

  while (*temp)        /* copy prog name as first arg */
  {
    Arg_Buf[len] = *temp;
    len++;
    temp++;
  }

  Arg_Buf[len] = '\0'; /* special null after first arg, all others use blank */
  len++;

  /* terminate the arg buf with a double null */
  Arg_Buf[len]='\0';
  len++;
  Arg_Buf[len]='\0';

  /* call exec PGM to execute chkdsk on this drive */
  rc = DosExecPgm(LoadError, sizeof(LoadError), 0, /* EXEC_SYNC */
                  Arg_Buf, MAKEP(env,0), &Result, ProgramName);

  /* Now we'll call f32chk.exe without parameters,
     in order to load the unicode translate table */
  TranslateInit(MAKEP(sel, 0), sizeof(PVOID) * (MAX_TRANS_TABLE + EXTRA_ELEMENT) );
  DosFreeSeg(sel);
#endif

  fRing3 = FALSE;
  return;
}
