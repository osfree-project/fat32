#define MOUNT_MOUNT           0x00
#define MOUNT_VOL_REMOVED     0x01
#define MOUNT_RELEASE         0x02
#define MOUNT_ACCEPT          0x03

#include "portable.h"
#include "fat32def.h"

#define  INCL_DOSFILEMGR
#define  INCL_DOSERRORS
#define  INCL_LONGLONG
#include <os2.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "block.h"
//#include "vl.h"

char FS_NAME[8] = "FAT32";

#pragma pack(1)

typedef struct _PTE
{
    char boot_ind;
    char starting_head;
    unsigned short starting_sector:6;
    unsigned short starting_cyl:10;
    char system_id;
    char ending_head;
    unsigned short ending_sector:6;
    unsigned short ending_cyl:10;
    unsigned long relative_sector;
    unsigned long total_sectors;
} PTE;

struct _MBR
{
    char pad[0x1be];
    PTE  pte[4];
    unsigned short boot_ind; /* 0x55aa */
};

#pragma pack()

APIRET parsePartTable(char *pszFilename,
                      char *pFmt,
                      PULONGLONG pullOffset,
                      PULONGLONG pullSize,
                      PULONG pulSecSize,
                      ULONG ulPart)
{
BlockDriverState *bs;
BlockDriver *drv = NULL;
APIRET rc = NO_ERROR;
HFILE hf;
ULONG ulAction, cbActual;
LONGLONG ibActual;
struct _MBR mbr;
PTE pte;
int i;

   bdrv_init();

   bs = bdrv_new("");

   if (! bs)
      {
      return ERROR_NOT_ENOUGH_MEMORY;
      }

   if (pFmt)
      {
      drv = bdrv_find_format(pFmt);
      }

   if (bdrv_open2(bs, pszFilename, 0, drv) < 0)
      {
      bdrv_delete(bs);
      return ERROR_FILE_NOT_FOUND;
      }

   if (bdrv_pread(bs, *pullOffset, (char *)&mbr, 512) < 0)
      {
      bdrv_delete(bs);
      return ERROR_READ_FAULT;
      }

   if (ulPart < 5)
   {
      ulPart--;
      pte = mbr.pte[ulPart];

      *pullOffset += (ULONGLONG)pte.relative_sector * *pulSecSize;
   }
   else
   {
      ulPart -= 4;

      for (i = 0; i < 4; i++)
      {
         pte = mbr.pte[i];

         if (pte.system_id == 0x5 || pte.system_id == 0xf)
            break; // Extended partition
      }

      if (i == 4)
      {
         printf("Extended partition does not exist!\n");
         bdrv_delete(bs);
         return ERROR_FILE_NOT_FOUND;
      }

      // extended partition found
      *pullOffset += (ULONGLONG)pte.relative_sector * *pulSecSize;

      do
      {
         if (bdrv_pread(bs, *pullOffset, (char *)&mbr, 512) < 0)
            {
            rc = ERROR_READ_FAULT;
            break;
            }

         pte = mbr.pte[0];

         if (pte.system_id == 0x5 || pte.system_id == 0xf)
            pte = mbr.pte[1];

         *pullOffset += (ULONGLONG)pte.relative_sector * *pulSecSize;
         ulPart--;
      }
      while (ulPart);
   }

   *pullSize = (ULONGLONG)pte.total_sectors * *pulSecSize;
   bdrv_delete(bs);

   return rc;
}

int main(int argc, char *argv[])
{
    FILESTATUS3L info;
    MNTOPTS opts;
    ULONG cbData, cbParms = sizeof(MNTOPTS);
    BOOL fDelete = FALSE;
    BOOL fPart = FALSE;
    char *p;
    ULONG ulPart = 0, ulSecSize = 512;
    ULONGLONG ullOffset = 0, ullSize = 0;
    char szFilename[256];
    char szMntPoint[256];
    ULONG cbDir;
    ULONG ulDriveNum, ulDriveMap;
    char szDir[256];
    char chDisk;
    char *arg;
    char *pFmt = NULL;
    APIRET rc;
    int i;

    if (argc == 1)
    {
        printf("f32mount [d:\\somedir\\somefile.img [<somedir> | /d] [/p:<partition no>][/o:<offset>][/f:<format>]]\n\n"
               "   Partitions 1..4 are primary partitions. Partitions 5..255 are logical partitions, 5 being the 1st logical.\n"
               "   Offsets can be decimal, as well as hexadecimals, starting from \"0x\".\n"
               "   The following formats are supported: raw, vpc, vmdk, vvfat, parallels, bochs, cloop, dmg, qcow\n");
        return 0;
    }

    rc = DosQueryCurrentDisk (&ulDriveNum, &ulDriveMap);
    chDisk=(UCHAR)(ulDriveNum+'A'-1);

    rc = DosQueryCurrentDir(0, szDir, &cbDir);
    rc = DosQueryCurrentDir(0, szDir, &cbDir);

    if (argv[1][1] != ':')
    {
       szFilename[0] = chDisk;
       szFilename[1] = ':';
       szFilename[2] = '\\';
       szFilename[3] = '\0';
       strcat(szFilename, szDir);

       if (strrchr(szFilename, '\\') != szFilename + strlen(szFilename) - 1)
          strcat(szFilename, "\\");

       strcat(szFilename, argv[1]);
    }
    else
       strcpy(szFilename, argv[1]);

    if (argv[2][1] != ':')
    {
       szMntPoint[0] = chDisk;
       szMntPoint[1] = ':';
       szMntPoint[2] = '\\';
       szMntPoint[3] = '\0';
       strcat(szMntPoint, szDir);

       if (strrchr(szMntPoint, '\\') != szMntPoint + strlen(szMntPoint) - 1)
          strcat(szMntPoint, "\\");

       strcat(szMntPoint, argv[2]);
    }
    else
       strcpy(szMntPoint, argv[2]);

    // parse options
    for (i = 1; i < argc; i++)
    {
       char *q;
       int base;

       arg = argv[i];

       // join args enclosed in quotes as a single arg
       if (strstr(arg, "\""))
       {
          memset(szDir, 0, sizeof(szDir));
          i++;

          do
          {
             arg = argv[i];
             strcat(szDir, arg);
             i++;
          }
          while (!strstr(arg, "\""));

          strcat(szDir, arg);

          arg = szDir;
       }

       if (!stricmp(arg, "/d"))
       {
          fDelete = TRUE;
       }
       else if ((p = strstr(strlwr(arg), "/o:")))
       {
          p += 3;

          if (strstr(p, "0x"))
             base = 16;
          else
             base = 10;

          ullOffset = strtoll(p, &q, base);
       }
       else if ((p = strstr(strlwr(arg), "/p:")))
       {
          fPart = TRUE;
          p += 3;

          ulPart = atol(p);
       }
       else if ((p = strstr(strlwr(arg), "/f:")))
       {
          if (strlen(p + 3) <= 8)
             pFmt = p + 3;
       }
       else
       {
          if (argc == 2 || i > 2)
          {
             rc = ERROR_INVALID_PARAMETER;
             goto err;
          }
          else if (! fDelete && i == 2)
          {
             rc = DosQueryPathInfo(szMntPoint,
                                   FIL_STANDARDL,
                                   &info,
                                   sizeof(info));

             if (rc && rc != ERROR_ACCESS_DENIED)
                goto err;
          }
       }
    }

    rc = DosQueryPathInfo(szFilename,
                          FIL_STANDARDL,
                          &info,
                          sizeof(info));

    if (rc && rc != ERROR_ACCESS_DENIED)
       goto err;

    ullSize = info.cbFile;

    if (! fDelete && fPart)
    {
       rc = parsePartTable(szFilename,
                           pFmt,
                           &ullOffset,
                           &ullSize,
                           &ulSecSize,
                           ulPart);

       if (rc)
          goto err;

       rc = DosQueryPathInfo(szMntPoint,
                             FIL_STANDARDL,
                             &info,
                             sizeof(info));

       if (rc)
          goto err;
    }

    if (fDelete)
    {
        strcpy(opts.pMntPoint, szFilename);
        opts.hf = 0;
        opts.usOp = MOUNT_RELEASE;

        rc = DosFSCtl(NULL, 0, NULL,
                      &opts, cbParms, &cbParms,
                      FAT32_MOUNT, FS_NAME, -1, FSCTL_FSDNAME);
    }
    else
    {
        strcpy(opts.pFilename, szFilename);
        strcpy(opts.pMntPoint, szMntPoint);
        opts.ullOffset = ullOffset;
        opts.ullSize = ullSize;
        opts.ulBytesPerSector = ulSecSize;
        opts.hf = 0;
        opts.usOp = MOUNT_MOUNT;

        if (pFmt)
           strcpy(opts.pFmt, pFmt);

        rc = DosFSCtl(NULL, 0, NULL,
                      &opts, cbParms, &cbParms,
                      FAT32_MOUNT, FS_NAME, -1, FSCTL_FSDNAME);

        if (rc == ERROR_VOLUME_NOT_MOUNTED)
        {
            strcpy(opts.pFilename, szFilename);
            strcpy(opts.pMntPoint, szMntPoint);
            opts.ullOffset = ullOffset;
            opts.ullSize = ullSize;
            opts.ulBytesPerSector = ulSecSize;
            opts.hf = 0;
            opts.usOp = MOUNT_ACCEPT;

            rc = DosFSCtl(NULL, 0, &cbData,
                          &opts, cbParms, &cbParms,
                          FAT32_MOUNT, FS_NAME, -1, FSCTL_FSDNAME);
        }
    }

err:
    switch (rc)
    {
      case ERROR_INVALID_FSD_NAME:
             printf ("Error: IFS=FAT32.IFS not loaded in CONFIG.SYS");
             return 1;

      case ERROR_INVALID_PROCID:
             printf ("Error: cachef32.exe is not running");
             return 1;

      case ERROR_INVALID_PATH:
             printf ("Error: Invalid path");
             return 1;

      case ERROR_ALREADY_ASSIGNED:
             printf ("Error: This file is already in use");
             return 1;
 
      case NO_ERROR:
             printf ("OK");
             return 0;

      default:
             printf ("Error mounting image, rc=%lu", rc);
             return 1;
    }

    return 0;
}
