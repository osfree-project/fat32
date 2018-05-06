#define MOUNT_MOUNT           0x00
#define MOUNT_VOL_REMOVED     0x01
#define MOUNT_RELEASE         0x02
#define MOUNT_ACCEPT          0x03

#define CAT_LOOP 0x85

#define FUNC_MOUNT           0x10
#define FUNC_DAEMON_STARTED  0x11
#define FUNC_DAEMON_STOPPED  0x12
#define FUNC_DAEMON_DETACH   0x13
#define FUNC_GET_REQ         0x14
#define FUNC_DONE_REQ        0x15

#define REDISCOVER_DRIVE_IOCTL 0x6A

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

typedef struct _DDI_Rediscover_param
{
    BYTE DDI_TotalDrives;
    BYTE DDI_aDriveNums[1];
} DDI_Rediscover_param, *PDDI_Rediscover_param;

typedef struct _DDI_ExtendRecord
{
  BYTE  DDI_Volume_UnitID;
  ULONG DDI_Volume_SerialNumber;
} DDI_ExtendRecord;

typedef struct _DDI_Rediscover_data
{
  BYTE NewIFSMUnits;
  BYTE DDI_TotalExtends;
  DDI_ExtendRecord DDI_aExtendRecords[1];
} DDI_Rediscover_data, *PDDI_Rediscover_data;

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
    BOOL fBlock = FALSE;
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
    HFILE hf;
    APIRET rc;
    int i;

    if (argc == 1)
    {
        printf("f32mount [d:\\somedir\\somefile.img [<somedir> | /d | block:] [/p:<partition no>][/o:<offset>][/f:<format>]]\n\n"
               "   Partitions 1..4 are primary partitions. Partitions 5..255 are logical partitions, 5 being the 1st logical.\n"
               "   Offsets can be decimal, as well as hexadecimals, starting from \"0x\".\n"
               "   The following formats are supported: raw, vpc, vmdk, vdi, vvfat, parallels, bochs, cloop, dmg, qcow, qcow2\n");
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
       else if (!stricmp(arg, "/block"))
       {
          fBlock = TRUE;
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
       else if ((p = strstr(strlwr(arg), "/s:")))
       {
           p += 3;

           if (strstr(p, "0x"))
              base = 16;
           else
              base = 10;

           ulSecSize = strtol(p, &q, base);
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
             if (! fBlock)
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

       if (! fBlock)
       {
          rc = DosQueryPathInfo(szMntPoint,
                                FIL_STANDARDL,
                                &info,
                                sizeof(info));

          if (rc)
             goto err;
       }
    }

    if (fBlock)
    {
       ULONG ulAction;

       rc = DosOpen("\\DEV\\LOOP$", &hf, &ulAction, 0, FILE_NORMAL,
                    OPEN_ACTION_FAIL_IF_NEW | OPEN_ACTION_OPEN_IF_EXISTS,
                    OPEN_ACCESS_READONLY | OPEN_SHARE_DENYNONE, NULL);

       if (rc)
          goto err;
    }

    if (fDelete)
    {
       strcpy(opts.pMntPoint, szFilename);
       opts.hf = 0;
       opts.usOp = MOUNT_RELEASE;

       if (! fBlock)
       {
          // mounting to a FAT subdir
          rc = DosFSCtl(NULL, 0, NULL,
                        &opts, cbParms, &cbParms,
                        FAT32_MOUNT, FS_NAME, -1, FSCTL_FSDNAME);
       }
       else
       {
          // mounting to a drive letter
          rc = DosDevIOCtl(hf, CAT_LOOP, FUNC_MOUNT,
                           &opts, cbParms, &cbParms,
                           NULL, 0, NULL);
       }
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

        if (! fBlock)
        {
           // mounting to a FAT subdir
           rc = DosFSCtl(NULL, 0, NULL,
                         &opts, cbParms, &cbParms,
                         FAT32_MOUNT, FS_NAME, -1, FSCTL_FSDNAME);
        }
        else
        {
           // mounting to a drive letter
           rc = DosDevIOCtl(hf, CAT_LOOP, FUNC_MOUNT,
                            &opts, cbParms, &cbParms,
                            NULL, 0, NULL);
        }

        if (rc == ERROR_VOLUME_NOT_MOUNTED)
        {
            strcpy(opts.pFilename, szFilename);
            strcpy(opts.pMntPoint, szMntPoint);
            opts.ullOffset = ullOffset;
            opts.ullSize = ullSize;
            opts.ulBytesPerSector = ulSecSize;
            opts.hf = 0;
            opts.usOp = MOUNT_ACCEPT;

            if (! fBlock)
            {
               // mounting to a FAT subdir
               rc = DosFSCtl(NULL, 0, &cbData,
                             &opts, cbParms, &cbParms,
                             FAT32_MOUNT, FS_NAME, -1, FSCTL_FSDNAME);
            }
            else
            {
               // mounting to a FAT subdir
               rc = DosDevIOCtl(hf, CAT_LOOP, FUNC_MOUNT,
                               &opts, cbParms, &cbParms,
                               NULL, 0, NULL);
            }
        }


       if (fBlock)
       {
          HFILE hfPhys;
          DDI_Rediscover_param parm;
          DDI_Rediscover_data  data;
          ULONG cbData, cbParm, ulAction;

          if (! fDelete)
          {
             // rediscover PRM
             USHORT cbDrives = 0;
             int i;

             rc = DosPhysicalDisk(INFO_COUNT_PARTITIONABLE_DISKS,
                                  &cbDrives,
                                  sizeof(cbDrives),
                                  NULL,
                                  0);

             if (rc)
                goto err;

             // get handle of first available disk
             for (i = 0; i < cbDrives; i++)
             {
                 char drv[3];

                 drv[0] = i + '0';
                 drv[1] = ':';
                 drv[2] = '\0';

                 rc = DosPhysicalDisk(INFO_GETIOCTLHANDLE,
                                      (PHFILE)&hfPhys,
                                      2L,
                                      drv,
                                      sizeof(drv));

                 if (! rc)
                     break;
             }

             if (rc)
                goto err;

             parm.DDI_TotalDrives = 0; // do a PRM rediscover
             parm.DDI_aDriveNums[0] = 1;
             data.DDI_TotalExtends = 0;
             data.NewIFSMUnits = 0;

             cbData = (data.DDI_TotalExtends * sizeof(DDI_ExtendRecord)) + 
                 sizeof(DDI_Rediscover_data) - sizeof(DDI_ExtendRecord);

             cbParm = (parm.DDI_TotalDrives * sizeof(UCHAR)) + sizeof(DDI_Rediscover_param) - sizeof(BYTE);

             rc = DosDevIOCtl(hfPhys, IOCTL_PHYSICALDISK, REDISCOVER_DRIVE_IOCTL,
                              &parm, cbParm, &cbParm,
                              &data, cbData, &cbData);

             if (rc)
                goto err;

             rc = DosPhysicalDisk(INFO_FREEIOCTLHANDLE,
                                  NULL,
                                  0,
                                  (PHFILE)&hfPhys,
                                  2L);
          }

          DosClose(hf);
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
             return 0;

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
