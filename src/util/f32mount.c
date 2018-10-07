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
#define FUNC_MOUNTED         0x16

#define REDISCOVER_DRIVE_IOCTL 0x6A

#include "portable.h"
#include "fat32def.h"

#define  INCL_DOSFILEMGR
#define  INCL_DOSERRORS
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

struct args
{
    char szFilename[256];
    char szMntPoint[256];
    BOOL fDelete;
    BOOL fPart;
    BOOL fBlock;
    BOOL fMountAll;
    BOOL fUnmountAll;
    ULONG ulPart; 
    ULONG ulSecSize;
    ULONGLONG ullOffset;
    ULONGLONG ullSize;
    char pFmt[16];
};

#define LINE_MAX_LEN 1024

#pragma pack()

void (*pbdrv_init)(void);
BlockDriverState *(*pbdrv_new)(const char *device_name);
BlockDriver *(*pbdrv_find_format)(const char *format_name);
void (*pbdrv_delete)(BlockDriverState *bs);
int (*pbdrv_open2)(BlockDriverState *bs, const char *filename, int flags,
                   BlockDriver *drv);
int (*pbdrv_pread)(BlockDriverState *bs, int64_t offset,
                   void *buf, int count1);
int (*pbdrv_pwrite)(BlockDriverState *bs, int64_t offset,
                    void *buf, int count1);

int ParseOpt(int argc, char *argv[], struct args *args);
int ProcessFsTab(BOOL isUnmount);
int Mount(struct args *args);

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
char szName[260];
HMODULE hmod;
HFILE hf;
ULONG ulAction, cbActual;
//LONGLONG ibActual;
struct _MBR mbr;
PTE pte;
int i;

   rc = DosLoadModule(szName, sizeof(szName), "QEMUIMG", &hmod);
   
   if (rc)
      {
      return rc;
      }

   rc = DosQueryProcAddr(hmod, 0, "bdrv_init",         (PFN *)&pbdrv_init);

   if (rc)
      {
      return rc;
      }

   rc = DosQueryProcAddr(hmod, 0, "bdrv_new",          (PFN *)&pbdrv_new);

   if (rc)
      {
      return rc;
      }

   rc = DosQueryProcAddr(hmod, 0, "bdrv_find_format",  (PFN *)&pbdrv_find_format);

   if (rc)
      {
      return rc;
      }

   rc = DosQueryProcAddr(hmod, 0, "bdrv_delete",       (PFN *)&pbdrv_delete);

   if (rc)
      {
      return rc;
      }

   rc = DosQueryProcAddr(hmod, 0, "bdrv_open2",        (PFN *)&pbdrv_open2);

   if (rc)
      {
      return rc;
      }

   rc = DosQueryProcAddr(hmod, 0, "bdrv_pread",        (PFN *)&pbdrv_pread);

   if (rc)
      {
      return rc;
      }

   (*pbdrv_init)();

   bs = (*pbdrv_new)("");

   if (! bs)
      {
      return ERROR_NOT_ENOUGH_MEMORY;
      }

   if (pFmt)
      {
      drv = (*pbdrv_find_format)(pFmt);
      }

   if ((*pbdrv_open2)(bs, pszFilename, 0, drv) < 0)
      {
      (*pbdrv_delete)(bs);
      return ERROR_FILE_NOT_FOUND;
      }

   if ((*pbdrv_pread)(bs, *pullOffset, (char *)&mbr, 512) < 0)
      {
      (*pbdrv_delete)(bs);
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
         (*pbdrv_delete)(bs);
         return ERROR_FILE_NOT_FOUND;
      }

      // extended partition found
      *pullOffset += (ULONGLONG)pte.relative_sector * *pulSecSize;

      do
      {
         if ((*pbdrv_pread)(bs, *pullOffset, (char *)&mbr, 512) < 0)
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
   (*pbdrv_delete)(bs);

   return rc;
}

int GetLine(FILE *fd, char *pszLine)
{
static char prevbuf[LINE_MAX_LEN] = {0};
static int cb = 0;
char *p, *p1, *p2, *p3, *p4;

   if (! fd || ! pszLine)
      return 1;
      
   for (;;)
   {
      p1 = strstr(prevbuf, "\r\n");
      p2 = strchr(prevbuf, '\r');
      p3 = strchr(prevbuf, '\n');
      p4 = strchr(prevbuf, '\0');

      if ( cb && ( p1 || p2 || p3 || p4 ) )
      {
         char *r = NULL, *r1 = NULL, *r2 = NULL;
         int len, l = 0;

         p = prevbuf + LINE_MAX_LEN;

         if (p1 && p1 < p)
            p = p1;
         if (p2 && p2 < p)
            p = p2;
         if (p3 && p3 < p)
            p = p3;
         if (p4 && p4 < p)
            p = p4;

         // skip comments
         r = prevbuf + LINE_MAX_LEN;

         r1 = strchr(prevbuf, ';');
         r2 = strchr(prevbuf, '#');

         if (r1 && r1 < r)
            r = r1;
         if (r2 && r2 < r)
            r = r2;
            
         if (r < p)
            len = r - prevbuf;
         else
            len = p - prevbuf;

         strncpy(pszLine, prevbuf, len);
         pszLine[len] = '\0';

         if (p1)
            p += 2;
         else
            p += 1;

         if (r < p)
            cb -= p - r;
         else
            cb -= p - prevbuf;

         strncpy(prevbuf, p, cb);
         prevbuf[cb] = '\0';

         return 0;
      }
      else
      {
         if (! feof(fd))
            cb += fread(prevbuf + cb, 1, LINE_MAX_LEN - cb, fd);
         else
            // line too long or empty
            return 1;
      }
   }

   return 1;
}

int ProcessFsTab(BOOL isUnmount)
{
char szCfg[CCHMAXPATHCOMP];
char szImgName[CCHMAXPATHCOMP];
char szLine[LINE_MAX_LEN];
char szArgs[LINE_MAX_LEN];
struct args args;
char *argv[32];
int argc;
FILE *fd;
ULONG ulTemp;
int rc;
char *p, *q;
int len, pos, i;

struct
{
   UCHAR ucIsMounted;
   char szPath[CCHMAXPATHCOMP];
} Data;

ULONG cbData;

   DosQuerySysInfo(QSV_BOOT_DRIVE, QSV_BOOT_DRIVE, &ulTemp, sizeof(ulTemp));
   
   szCfg[0] = 'a' + ulTemp - 1;
   szCfg[1] = ':';
   strcpy(&szCfg[2], "\\os2\\boot\\fstab.cfg");
   
   fd = fopen(szCfg, "r");

   if (! fd)
   {
      return 1;
   }
   
   while ( ! GetLine(fd, (char *)szLine) )
   {
      // skip empty lines
      if (! *szLine)
         continue;

      // change tabs to spaces
      while ( (p = strchr(szLine, '\t')) )
      {
         *p = ' ';
      }

      // remove duplicate spaces
      while ( (p = strstr(szLine, "  ")) )
      {
         strncpy(p + 1, p + 2, LINE_MAX_LEN - (p - szLine) - 1);
         p[LINE_MAX_LEN - (p - szLine)] = '\0';
      }

      memset(szArgs, 0, sizeof(szArgs));
      memset(szImgName, 0, sizeof(szImgName));

      if (strlen(szLine) + strlen("f32mount.exe") + 5 > LINE_MAX_LEN)
      {
          printf("Command line too long!");
          return 1;
      }

      strcpy(szArgs, "f32mount.exe");

      pos = strlen(szArgs) + 1;
      argv[0] = szArgs;
      argc = 1;

      strcpy(&szArgs[pos], szLine);
      p = &szArgs[pos];

      while (p && *p)
      {
          if (*p == '"')
          {
             p++;
             q = strchr(p, '"');
             
             if (q)
                q++;
          }
          else
             q = strchr(p + 1, ' ');

          argv[argc++] = p;

          if (! q)
          {
              pos += strlen(&szArgs[pos]) + 1;
              p = NULL;
          }
          else
          {
              szArgs[pos + q - p] = '\0';
              pos += q - p + 1;
              p = &szArgs[pos];
              if (*p == ' ')
              {
                 p++;
                 pos++;
              }
          }

          if (argc > 32)
          {
              printf("Too many command line parameters!\n");
              return 1;
          }
      }
      
      if (isUnmount)
      {
          // unmount operation
          strcat(&szArgs[pos], "/d");
          len += 3;
          argv[argc] = &szArgs[pos];
          pos += strlen(&szArgs[pos]) + 1;
          argc++;
      }

      strcpy(szImgName, argv[1]);

      cbData = sizeof(Data);
      memset(&Data, 0, sizeof(Data));
      strcpy(Data.szPath, szImgName);

      // determine if this image is already mounted
      if (strstr(&szArgs[pos], "/block"))
      {
         ULONG ulAction;
         HFILE hf;

         rc = DosOpen("\\DEV\\LOOP$", &hf, &ulAction, 0, FILE_NORMAL,
                      OPEN_ACTION_FAIL_IF_NEW | OPEN_ACTION_OPEN_IF_EXISTS,
                      OPEN_ACCESS_READONLY | OPEN_SHARE_DENYNONE, NULL);

         if (rc)
             return rc;

         rc = DosDevIOCtl(hf, CAT_LOOP, FUNC_MOUNTED,
                          NULL, 0, NULL,
                          &Data, cbData, &cbData);

         DosClose(hf);
      }
      else
      {
         rc = DosFSCtl(&Data, cbData, &cbData,
                       NULL, 0, NULL,
                       FAT32_MOUNTED, FS_NAME, -1, FSCTL_FSDNAME);
      }

      if (! rc && Data.ucIsMounted && ! isUnmount)
          return rc;

      rc = ParseOpt(argc, argv, &args);

      if (rc)
          return rc;

      Mount(&args);
   }

   fclose(fd);
   return 0;
}



int ParseOpt(int argc, char *argv[], struct args *args)
{
    ULONG ulDriveNum, ulDriveMap;
    char fn[256];
    char szDir[256];
    ULONG cbDir;
    char chDisk;
    APIRET rc;
    char *arg;
    char *p, *q;
    int i;
    
    memset(args, 0, sizeof(struct args));
    args->ulSecSize = 512;

    if (argc == 1)
    {
        printf("f32mount [d:\\somedir\\somefile.img [/a | /u] [<somedir> | /d | block:] [/p:<partition no>][/o:<offset>][/f:<format>][/s:<sector size>]]\n\n"
               "   Partitions 1..4 are primary partitions. Partitions 5..255 are logical partitions, 5 being the 1st logical.\n"
               "   Offsets can be decimal, as well as hexadecimals, starting from \"0x\".\n"
               "   The following formats are supported: raw, vpc, vmdk, vdi, vvfat, parallels, bochs, cloop, dmg, qcow, qcow2\n");
        return 1;
    }

    if (argc == 2)
    {
        if (!strcmp(strlwr(argv[1]), "/a"))
            args->fMountAll = TRUE;
        else if (!strcmp(strlwr(argv[1]), "/u"))
            args->fUnmountAll = TRUE;

        return 0;
    }

    rc = DosQueryCurrentDisk (&ulDriveNum, &ulDriveMap);
    chDisk=(UCHAR)(ulDriveNum+'A'-1);

    memset(szDir, 0, sizeof(szDir));
    cbDir = 0;

    rc = DosQueryCurrentDir(0, szDir, &cbDir);
    rc = DosQueryCurrentDir(0, szDir, &cbDir);

    q = fn;

    for (p = argv[1]; *p; p++)
    {
       if (*p != '"')
          *q++ = *p;
    }
    *q++ = '\0';
    
    if (fn[1] != ':')
    {
       args->szFilename[0] = chDisk;
       args->szFilename[1] = ':';
       args->szFilename[2] = '\\';
       args->szFilename[3] = '\0';
       strcat(args->szFilename, szDir);

       if (strrchr(args->szFilename, '\\') != args->szFilename + strlen(args->szFilename) - 1)
          strcat(args->szFilename, "\\");

       strcat(args->szFilename, fn);
    }
    else
       strcpy(args->szFilename, fn);

    q = fn;

    for (p = argv[2]; *p; p++)
    {
       if (*p != '"')
          *q++ = *p;
    }
    *q++ = '\0';

    if (fn[1] != ':')
    {
       args->szMntPoint[0] = chDisk;
       args->szMntPoint[1] = ':';
       args->szMntPoint[2] = '\\';
       args->szMntPoint[3] = '\0';
       strcat(args->szMntPoint, szDir);

       if (strrchr(args->szMntPoint, '\\') != args->szMntPoint + strlen(args->szMntPoint) - 1)
          strcat(args->szMntPoint, "\\");

       strcat(args->szMntPoint, fn);
    }
    else
       strcpy(args->szMntPoint, fn);

    // parse options
    for (i = 1; i < argc; i++)
    {
       char *q;
       int base;

       arg = argv[i];

       // join args enclosed in quotes as a single arg
       if (*arg == '"')
       {
          memset(szDir, 0, sizeof(szDir));

          do
          {
             arg = argv[i];
             i++;
             strcat(szDir, arg);
          }
          while (!strstr(arg, "\""));

          arg = szDir;
       }

       if (!stricmp(arg, "/d"))
       {
          args->fDelete = TRUE;
       }
       else if (!stricmp(arg, "/block"))
       {
          args->fBlock = TRUE;
       }
       else if ((p = strstr(strlwr(arg), "/o:")))
       {
          p += 3;

          if (strstr(p, "0x"))
             base = 16;
          else
             base = 10;

          args->ullOffset = strtoll(p, &q, base);
       }
       else if ((p = strstr(strlwr(arg), "/p:")))
       {
          args->fPart = TRUE;
          p += 3;

          args->ulPart = atol(p);
       }
       else if ((p = strstr(strlwr(arg), "/f:")))
       {
          if (strlen(p + 3) <= 8)
             strcpy(args->pFmt, p + 3);
       }
       else if ((p = strstr(strlwr(arg), "/s:")))
       {
           p += 3;

           if (strstr(p, "0x"))
              base = 16;
           else
              base = 10;

           args->ulSecSize = strtol(p, &q, base);
       }
       else
       {
          if (argc == 2 || i > 2)
          {
             return ERROR_INVALID_PARAMETER;
          }
       }
    }

    return NO_ERROR;
}

int Mount(struct args *args)
{
    FILESTATUS3L info;
    MNTOPTS opts;
    ULONG cbData, cbParms = sizeof(MNTOPTS);
    HFILE hf;
    APIRET rc;

    rc = DosQueryPathInfo(args->szFilename,
                          FIL_STANDARDL,
                          &info,
                          sizeof(info));

    if (rc && rc != ERROR_ACCESS_DENIED)
       goto err;

    args->ullSize = info.cbFile;

    if (! args->fDelete && args->fPart)
    {
       rc = parsePartTable(args->szFilename,
                           args->pFmt,
                           &args->ullOffset,
                           &args->ullSize,
                           &args->ulSecSize,
                           args->ulPart);

       if (rc)
          goto err;

       if (! args->fBlock)
       {
          rc = DosQueryPathInfo(args->szMntPoint,
                                FIL_STANDARDL,
                                &info,
                                sizeof(info));
          
          if (rc)
             goto err;
       }
    }

    if (args->fBlock)
    {
       ULONG ulAction;

       rc = DosOpen("\\DEV\\LOOP$", &hf, &ulAction, 0, FILE_NORMAL,
                    OPEN_ACTION_FAIL_IF_NEW | OPEN_ACTION_OPEN_IF_EXISTS,
                    OPEN_ACCESS_READONLY | OPEN_SHARE_DENYNONE, NULL);

       if (rc)
          goto err;
    }

    if (args->fDelete)
    {
       strcpy(opts.pMntPoint, args->szFilename);
       opts.hf = 0;
       opts.usOp = MOUNT_RELEASE;

       if (! args->fBlock)
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
        strcpy(opts.pFilename, args->szFilename);
        strcpy(opts.pMntPoint, args->szMntPoint);
        opts.ullOffset = args->ullOffset;
        opts.ullSize = args->ullSize;
        opts.ulBytesPerSector = args->ulSecSize;
        opts.hf = 0;
        opts.usOp = MOUNT_MOUNT;

        if (args->pFmt)
           strcpy(opts.pFmt, args->pFmt);

        if (! args->fBlock)
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
            strcpy(opts.pFilename, args->szFilename);
            strcpy(opts.pMntPoint, args->szMntPoint);
            opts.ullOffset = args->ullOffset;
            opts.ullSize = args->ullSize;
            opts.ulBytesPerSector = args->ulSecSize;
            opts.hf = 0;
            opts.usOp = MOUNT_ACCEPT;

            if (! args->fBlock)
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

       if (args->fBlock)
       {
          HFILE hfPhys;
          DDI_Rediscover_param parm;
          DDI_Rediscover_data  data;
          ULONG cbData, cbParm, ulAction;

          if (! args->fDelete)
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

             if (rc != ERROR_BAD_COMMAND)
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
             printf ("Error: IFS=FAT32.IFS not loaded in CONFIG.SYS\n");
             return 1;

      case ERROR_INVALID_PROCID:
             printf ("Error: cachef32.exe is not running\n");
             return 1;

      case ERROR_INVALID_PATH:
             return 0;

      case ERROR_ALREADY_ASSIGNED:
             printf ("Error: This file is already in use\n");
             return 1;
 
      case NO_ERROR:
             printf ("OK\n");
             return 0;

      default:
             printf ("Error mounting image, rc=%lu\n", rc);
             return 1;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    struct args args;
    APIRET rc = NO_ERROR;
    
    rc = ParseOpt(argc, argv, &args);
    
    if (rc)
       return 1;
    
    if (args.fMountAll)
       rc = ProcessFsTab(FALSE);
    else if (args.fUnmountAll)
       rc = ProcessFsTab(TRUE);
    else
       rc = Mount(&args);

    return rc;
}
