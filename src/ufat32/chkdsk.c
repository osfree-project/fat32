/*****************************************
MSG_552, Errors
MSG_1375,VOLUMELABEL
MSG_1243,VOLSERIAL
MSG_1339 (Errors found)
MSG_1314 (Lost extended attributes)
MSG_1359,LostClusterSpace
MSG_1361,TotalDiskSpace
MSG_1363, HiddenSpace, hiddenFileCount
MSG_1364, DirSpace, DirCount
MSG_1365, UserSpace, UserFileCount
MSG_1819, ExtendedAttributes, 0
MSG_1368, FreeSpace, 0
MSG_1304, ClusterSize
MSG_1305, TotalClusters
MSG_1306, AvailableClusters
******************************************/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <signal.h>
#include <io.h>
#include <fcntl.h>
#include <stdarg.h>
#include <conio.h>

#define INCL_DOS
#define INCL_DOSERRORS
#define INCL_DOSDEVICES
#define INCL_DOSDEVIOCTL
#define INCL_LONGLONG
#include <os2.h>
#include "portable.h"
#include "fat32c.h"

#define STACKSIZE 0x20000

#define MODIFY_DIR_INSERT 0
#define MODIFY_DIR_DELETE 1
#define MODIFY_DIR_UPDATE 2
#define MODIFY_DIR_RENAME 3

PRIVATE ULONG ChkDskMain(PCDINFO pCD);
PRIVATE ULONG MarkVolume(PCDINFO pCD, BOOL fClean);
PRIVATE ULONG CheckFats(PCDINFO pCD);
PRIVATE ULONG CheckFiles(PCDINFO pCD);
PRIVATE ULONG CheckFreeSpace(PCDINFO pCD);
PRIVATE ULONG CheckDir(PCDINFO pCD, ULONG ulDirCluster, PSZ pszPath, ULONG ulParentDirCluster);
PRIVATE BOOL   ReadFATSector(PCDINFO pCD, ULONG ulSector);
PRIVATE ULONG  GetNextCluster(PCDINFO pCD, ULONG ulCluster, BOOL fAllowBad);
PRIVATE ULONG GetClusterCount(PCDINFO pCD, ULONG ulCluster, PSZ pszFile);
PRIVATE BOOL   MarkCluster(PCDINFO pCD, ULONG ulCluster, PSZ pszFile);
PRIVATE PSZ    MakeName(PDIRENTRY pDir, PSZ pszName, USHORT usMax);
PRIVATE ULONG fGetVolLabel(PCDINFO pCD, PSZ pszVolLabel);
PRIVATE BOOL   fGetLongName(PDIRENTRY pDir, PSZ pszName, USHORT wMax);
PRIVATE BOOL   ClusterInUse(PCDINFO pCD, ULONG ulCluster);
PRIVATE BOOL RecoverChain(PCDINFO pCD, ULONG ulCluster);
PRIVATE BOOL LostToFile(PCDINFO pCD, ULONG ulCluster, ULONG ulSize);
PRIVATE BOOL ClusterInChain(PCDINFO pCD, ULONG ulStart, ULONG ulCluster);
PRIVATE BOOL OutputToFile(VOID);

ULONG ReadSector(PCDINFO pCD, ULONG ulSector, USHORT nSectors, PBYTE pbSector);
ULONG ReadCluster(PCDINFO pDrive, ULONG ulCluster, PBYTE pbCluster);
ULONG WriteSector(PCDINFO pCD, ULONG ulSector, USHORT nSectors, PBYTE pbSector);
ULONG WriteCluster(PCDINFO pCD, ULONG ulCluster, PVOID pbCluster);
ULONG ReadSect(HFILE hFile, ULONG ulSector, USHORT nSectors, PBYTE pbSector);
ULONG WriteSect(HFILE hf, ULONG ulSector, USHORT nSectors, PBYTE pbSector);

//ULONG ReadFatSector(PCDINFO pCD, ULONG ulSector);
ULONG SetNextCluster(PCDINFO pCD, ULONG ulCluster, ULONG ulNext);
BOOL  GetDiskStatus(PCDINFO pCD);
ULONG GetFreeSpace(PCDINFO pVolInfo);
BOOL MarkDiskStatus(PCDINFO pCD, BOOL fClean);
USHORT GetSetFileEAS(PCDINFO pCD, USHORT usFunc, PMARKFILEEASBUF pMark);
USHORT SetFileSize(PCDINFO pCD, PFILESIZEDATA pFileSize);
USHORT RecoverChain2(PCDINFO pCD, ULONG ulCluster, PBYTE pData, USHORT cbData);
USHORT MakeDirEntry(PCDINFO pCD, ULONG ulDirCluster, PDIRENTRY pNew, PSZ pszName);
BOOL DeleteFatChain(PCDINFO pCD, ULONG ulCluster);
BOOL LoadTranslateTable(BOOL fSilent);
VOID GetFirstInfo( PBOOL pFirstInfo );
VOID GetCaseConversion( PUCHAR pCase );
VOID Translate2OS2(PUSHORT pusUni, PSZ pszName, USHORT usLen);
ULONG FindDirCluster(PCDINFO pCD, PSZ pDir, USHORT usCurDirEnd, USHORT usAttrWanted, PSZ *pDirEnd);
ULONG FindPathCluster(PCDINFO pCD, ULONG ulCluster, PSZ pszPath, PDIRENTRY pDirEntry, PSZ pszFullName);
APIRET ModifyDirectory(PCDINFO pCD, ULONG ulDirCluster, USHORT usMode, PDIRENTRY pOld, PDIRENTRY pNew, PSZ pszLongName);
APIRET MakeFile(PCDINFO pCD, ULONG ulDirCluster, PSZ pszFile, PBYTE pBuf, ULONG cbBuf);

PSZ       GetOS2Error(USHORT rc);

extern int logbufsize;
extern char *logbuf;
extern int  logbufpos;

static HFILE hDisk = NULLHANDLE;

F32PARMS  f32Parms = {0};
static BOOL fToFile;

extern char msg;

#if 1  /* by OAX */
static UCHAR rgFirstInfo[ 256 ] = { 0, };
static UCHAR rgLCase[ 256 ] = { 0, };

VOID TranslateInitDBCSEnv( VOID )
{
   ULONG ulDataSize;
   ULONG ulParamSize;

   //DosFSCtl( rgFirstInfo, sizeof( rgFirstInfo ), &ulDataSize,
   //   NULL, 0, &ulParamSize,
   //   FAT32_GETFIRSTINFO, "FAT32", -1, FSCTL_FSDNAME );
   GetFirstInfo(( PBOOL )rgFirstInfo );
}

BOOL IsDBCSLead( UCHAR uch)
{
    return ( rgFirstInfo[ uch ] == 2 );
}

VOID CaseConversionInit( VOID )
{
   ULONG ulDataSize;
   ULONG ulParamSize;

   //DosFSCtl( rgLCase, sizeof( rgLCase ), &ulDataSize,
   //   NULL, 0, &ulParamSize,
   //   FAT32_GETCASECONVERSION, "FAT32", -1, FSCTL_FSDNAME );
   GetCaseConversion(rgLCase);
}

/* Get the last-character. (sbcs/dbcs) */
int lastchar(const char *string)
{
    UCHAR *s;
    unsigned int c = 0;
    int i, len = strlen(string);
    s = (UCHAR *)string;
    for(i = 0; i < len; i++)
    {
        c = *(s + i);
        if(IsDBCSLead(( UCHAR )c))
        {
            c = (c << 8) + ( unsigned int )*(s + i + 1);
            i++;
        }
    }
    return c;
}

#endif /* by OAX */

VOID Handler(INT iSignal)
{
   show_message("Signal %d was received\n", 0, 0, 1, iSignal);

   // remount disk for changes to take effect
   if (hDisk)
      remount_media(hDisk);
}

int chkdsk_thread(int iArgc, char *rgArgv[], char *rgEnv[])
{
INT iArg;
HFILE hFile;
ULONG rc = 0;
PCDINFO pCD;
ULONG  ulAction;
BYTE   bSector[512];
ULONG  ulDeadFace = 0xDEADFACE;
ULONG  ulParmSize;
ULONG  ulDataSize;
ULONG  cbDataLen;

   /* Enable hard errors     */
   DosError(1);

   f32Parms.fEAS = TRUE;

   pCD = (PCDINFO)malloc(sizeof(CDINFO));
   if (!pCD)
      return ERROR_NOT_ENOUGH_MEMORY;
   memset(pCD, 0, sizeof (CDINFO));

   for (iArg = 1; iArg < iArgc; iArg++)
      {
      strupr(rgArgv[iArg]);
      if (rgArgv[iArg][0] == '/' || rgArgv[iArg][0] == '-')
         {
         switch (rgArgv[iArg][1])
            {
            case 'V':
               pCD->fDetailed = 2;
               if (rgArgv[iArg][2] == ':' && rgArgv[iArg][3] == '1')
                  pCD->fDetailed = 1;
               if (rgArgv[iArg][2] == ':' && rgArgv[iArg][3] == '2')
                  pCD->fDetailed = 2;
               break;
            case 'P':
               pCD->fPM = TRUE;
               msg = TRUE;
               break;
            case 'F':
               pCD->fFix = TRUE;
               break;
            case 'C':
               pCD->fAutoRecover = TRUE;
               break;
            case 'A':
               pCD->fAutoCheck = TRUE;
               break;
            default :
               show_message( "%s is not a valid parameter with the CHKDSK"
                             "command when checking a hard disk.", 0, 543, 1, TYPE_STRING, rgArgv[iArg] );
               exit(543);
            }
         }
      else if (!strlen(pCD->szDrive))
         strncpy(pCD->szDrive, rgArgv[iArg], 2);
      }

   LoadTranslateTable(pCD->fAutoCheck);

   TranslateInitDBCSEnv();

   CaseConversionInit();

   fToFile = OutputToFile();

   ulDataSize = sizeof(f32Parms);
   //rc = DosFSCtl(
   //   (PVOID)&f32Parms, ulDataSize, &ulDataSize,
   //   NULL, 0, &ulParmSize,
   //   FAT32_GETPARMS, "FAT32", -1, FSCTL_FSDNAME);
   //if (rc)
   //   {
   //   printf("DosFSCtl, FAT32_GETPARMS failed, rc = %d\n", rc);
   //   DosExit(EXIT_PROCESS, 1);
   //   }
   //if (strcmp(f32Parms.szVersion, FAT32_VERSION))
   //   {
   //   printf("ERROR: FAT32 version (%s) differs from UFAT32.DLL version (%s)\n", f32Parms.szVersion, FAT32_VERSION);
   //   DosExit(EXIT_PROCESS, 1);
   //   }

   if (!strlen(pCD->szDrive))
      {
      ULONG ulDisk;
      ULONG  ulDrives;
      rc = DosQueryCurrentDisk(&ulDisk, &ulDrives);
      pCD->szDrive[0] = (BYTE)(ulDisk + '@');
      pCD->szDrive[1] = ':';
      }
   rc = DosOpenL(pCD->szDrive,
      &hFile,
      &ulAction,                         /* action taken */
      0LL,                               /* new size     */
      0,                                 /* attributes   */
      OPEN_ACTION_OPEN_IF_EXISTS,        /* open flags   */
      OPEN_ACCESS_READONLY | OPEN_SHARE_DENYNONE | OPEN_FLAGS_DASD |
      OPEN_FLAGS_WRITE_THROUGH,         /* OPEN_FLAGS_NO_CACHE , */
      0L);
   if (rc)
      {
      if (rc == ERROR_DRIVE_LOCKED)
         show_message(NULL, 0, rc, 0);
      else
         show_message("%s\n", 0, 0, 1, GetOS2Error(rc));
      return 1;
      }
   ulParmSize = sizeof(ulDeadFace);
   //rc = DosFSCtl(NULL, 0, 0,
   //              (PBYTE)&ulDeadFace, ulParmSize, &ulParmSize,
   //              FAT32_SECTORIO,
   //              NULL,
   //              hFile,
   //              FSCTL_HANDLE);
   //if (rc)
   //   {
   //   printf("DosFSCtl (SectorIO) failed:\n%s\n", GetOS2Error(rc));
   //   exit(1);
   //   }
   //rc = DosDevIOCtl(hFile, IOCTL_FAT32, FAT32_GETVOLCLEAN,
   //                 NULL, 0, NULL,
   //                 (PVOID)&pCD->fCleanOnBoot, sizeof(pCD->fCleanOnBoot), &cbDataLen);
   //rc = DosDevIOCtl2((PVOID)&pCD->fCleanOnBoot, sizeof(pCD->fCleanOnBoot),
   //   NULL, 0,
   //   FAT32_GETVOLCLEAN, IOCTL_FAT32, hFile);

   if (pCD->fFix)
      {
      rc = DosDevIOCtl(hFile, IOCTL_DISK, DSK_LOCKDRIVE,
                       NULL, 0, NULL,
                       NULL, 0, NULL);
      //rc = DosDevIOCtl(NULL, NULL, DSK_LOCKDRIVE, IOCTL_DISK, hFile);
      if (rc)
         {
         if (rc == ERROR_DRIVE_LOCKED)
            show_message(NULL, 0, rc, 0);
         else
            show_message("%s\n", 0, 0, 1, GetOS2Error(rc));
         return 1;
         }
      }

   //rc = DosQueryFSInfo(pCD->szDrive[0] - '@', FSIL_ALLOC,
   //   (PBYTE)&pCD->DiskInfo, sizeof (DISKINFO));
   //if (rc)
   //   {
   //   fprintf(stderr, "DosQueryFSInfo failed, %s\n", GetOS2Error(rc));
   //   DosExit(EXIT_PROCESS, 1);
   //   }
   pCD->hDisk = hFile;
   hDisk = hFile;

   rc = ReadSector(pCD, 0, 1, bSector);
   if (rc)
      {
      show_message("Error: Cannot read boot sector: %s\n", 0, 0, 1, GetOS2Error(rc));
      return rc;
      }
   memcpy(&pCD->BootSect, bSector, sizeof (BOOTSECT));

   rc = ReadSector(pCD, pCD->BootSect.bpb.FSinfoSec, 1, bSector);
   if (rc)
      {
      show_message("Error: Cannot read FSInfo sector\n%s\n", 0, 0, 1, GetOS2Error(rc));
      return rc;
      }

   rc = ChkDskMain(pCD);

   if (pCD->fFix)
      {
      rc = DosDevIOCtl(hFile, IOCTL_DISK, DSK_UNLOCKDRIVE,
                       NULL, 0, NULL,
                       NULL, 0, NULL);
      //rc = DosDevIOCtl(NULL, NULL, DSK_UNLOCKDRIVE, IOCTL_DISK, hFile);
      if (rc)
         {
         show_message("The drive cannot be unlocked. SYS%4.4u\n", 0, 0, 1, rc);
         return 1;
         }
      }
   DosClose(hFile);
   free(pCD);

   return rc;

   rgEnv = rgEnv;
}

int chkdsk(int argc, char *argv[], char *envp[])
{
  char *stack;
  APIRET rc;

  // Here we're switching stack, because the original
  // chkdsk.com stack is too small.

  // allocate stack
  rc = DosAllocMem((void **)&stack, 
                   STACKSIZE, 
                   PAG_READ | PAG_WRITE | 
                   PAG_COMMIT | OBJ_TILE);

  if (rc)
    return rc;

  // call chkdsk_thread on new stack
  _asm {
    mov eax, esp
    mov edx, stack
    mov ecx, envp
    add edx, STACKSIZE - 4
    mov esp, edx
    push eax
    push ecx
    mov ecx, argv
    push ecx
    mov ecx, argc
    push ecx
    call chkdsk_thread
    add esp, 12
    pop esp
  }

  // deallocate new stack
  DosFreeMem(stack);

  return 0;
}


BOOL OutputToFile(VOID)
{
ULONG rc;
ULONG ulType;
ULONG ulAttr;

   rc = DosQueryHType(fileno(stdout), &ulType, &ulAttr);
   switch (ulType & 0x000F)
      {
      case 0:
         return TRUE;
      case 1:
      case 2:
         return FALSE;
/*
      default:
         return FALSE;
*/
      }
   return FALSE;
}

#if 1
ULONG ReadCluster(PCDINFO pCD, ULONG ulCluster, PBYTE pbCluster)
{
   ULONG ulSector;
   USHORT rc;

   if (ulCluster < 2 || ulCluster >= pCD->ulTotalClusters + 2)
      return ERROR_SECTOR_NOT_FOUND;

   ulSector = pCD->ulStartOfData +
      (ulCluster - 2) * pCD->BootSect.bpb.SectorsPerCluster;

   rc = ReadSector(pCD, ulSector,
      pCD->BootSect.bpb.SectorsPerCluster,
      pbCluster);

   if (rc)
      return rc;

   return 0;
}
#else
ULONG ReadCluster(PCDINFO pCD, ULONG ulCluster, PBYTE pbCluster)
{
   ULONG dummy1 = 0, dummy2 = 0;
   ULONG ulDataSize = pCD->ulClusterSize;
   ULONG rc;

//   return DosDevIOCtl2(
//     (PVOID)pbCluster, pCD->ulClusterSize,
//      (PVOID)&ulCluster, sizeof(ulCluster),
//      FAT32_READCLUSTER, IOCTL_FAT32, pCD->hDisk);
   rc = DosDevIOCtl(pCD->hDisk, IOCTL_FAT32, FAT32_READCLUSTER,
                      (PVOID)&ulCluster, sizeof(ulCluster), &dummy1,
                      (PVOID)pbCluster, ulDataSize, &dummy2);
   return rc;
}
#endif

#if 1
ULONG ReadSector(PCDINFO pCD, ULONG ulSector, USHORT nSectors, PBYTE pbSector)
{
   return ReadSect(pCD->hDisk, ulSector, nSectors, pbSector);
}

#else
ULONG ReadSector(PCDINFO pCD, ULONG ulSector, USHORT nSectors, PBYTE pbSector)
{
   READSECTORDATA rsd;
   ULONG ulDataSize;
   ULONG dummy1 = 0, dummy2 = 0;

   rsd.ulSector = ulSector;
   rsd.nSectors = nSectors;

   ulDataSize = nSectors * SECTOR_SIZE;

//   return DosDevIOCtl2(
//      (PVOID)pbSector, usDataSize,
//      (PVOID)&rsd, sizeof(rsd),
//      FAT32_READSECTOR, IOCTL_FAT32, pCD->hDisk);
   rc = DosDevIOCtl(pCD->hDisk, IOCTL_FAT32, FAT32_READSECTOR,
                    (PVOID)&rsd, sizeof(rsd), &dummy1,
                    (PVOID)pbSector, ulDataSize, &dummy2);
}
#endif

ULONG WriteSector(PCDINFO pCD, ULONG ulSector, USHORT nSectors, PBYTE pbSector)
{
   return WriteSect(pCD->hDisk, ulSector, nSectors, pbSector);
}

ULONG WriteCluster(PCDINFO pCD, ULONG ulCluster, PVOID pbCluster)
{
   ULONG ulSector;
   ULONG rc;

   if (ulCluster < 2 || ulCluster >= pCD->ulTotalClusters + 2)
      //{
      //CritMessage("ERROR: Cluster %ld does not exist on disk %c:",
      //   ulCluster, pVolInfo->bDrive + 'A');
      //Message("ERROR: Cluster %ld does not exist on disk %c:",
      //   ulCluster, pVolInfo->bDrive + 'A');
      return ERROR_SECTOR_NOT_FOUND;
      //}

   ulSector = pCD->ulStartOfData +
      (ulCluster - 2) * pCD->BootSect.bpb.SectorsPerCluster;

   rc = WriteSector(pCD, ulSector,
      pCD->BootSect.bpb.SectorsPerCluster, pbCluster);
   if (rc)
      //{
      //Message("WriteCluster: Cluster %lu failed!", ulCluster);
      return rc;
      //}

   return 0;
}

ULONG ChkDskMain(PCDINFO pCD)
{
ULONG  rc;
ULONG  ulBytes;
USHORT usBlocks;
BYTE   szString[12];
PSZ    p;
ULONG  dummy = 0;
HFILE  hf;
ULONG  cbActual, ulAction;

   if (pCD->fFix)
      {
      if (! pCD->fAutoCheck)
         {
         /* Install signal handler */
         signal(SIGABRT, Handler);
         signal(SIGBREAK, Handler);
         signal(SIGTERM, Handler);
         signal(SIGINT, Handler);
         signal(SIGFPE, Handler);
         signal(SIGSEGV, Handler);
         signal(SIGILL, Handler);
         // issue BEGINFORMAT ioctl to prepare disk for checking
         begin_format(pCD->hDisk);
         }
      }
   /*
      Some preparations
   */
   memset(logbuf, 0, logbufsize);
   logbufpos = 0;

   pCD->ulCurFATSector = 0xFFFFFFFF;
   pCD->ulActiveFatStart =  pCD->BootSect.bpb.ReservedSectors;
   if (pCD->BootSect.bpb.ExtFlags & 0x0080)
      pCD->ulActiveFatStart +=
         pCD->BootSect.bpb.BigSectorsPerFat * (pCD->BootSect.bpb.ExtFlags & 0x000F);

   pCD->ulStartOfData    = pCD->BootSect.bpb.ReservedSectors +
     pCD->BootSect.bpb.BigSectorsPerFat * pCD->BootSect.bpb.NumberOfFATs;

   pCD->ulClusterSize = pCD->BootSect.bpb.BytesPerSector * pCD->BootSect.bpb.SectorsPerCluster;
   pCD->ulTotalClusters = (pCD->BootSect.bpb.BigTotalSectors - pCD->ulStartOfData) / pCD->BootSect.bpb.SectorsPerCluster;

   ulBytes = pCD->ulTotalClusters / 8 +
      (pCD->ulTotalClusters % 8 ? 1:0);
   usBlocks = (USHORT)(ulBytes / 4096 +
            (ulBytes % 4096 ? 1:0));

   //pCD->pFatBits = halloc(usBlocks,4096);
   pCD->pFatBits = calloc(usBlocks,4096);
   if (!pCD->pFatBits)
      {
      show_message("Not enough memory for FATBITS\n", 0, 0, 0);
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto ChkDskMainExit;
      }

   memset(szString, 0, sizeof(szString));
#if 0
   strncpy(szString, pCD->BootSect.VolumeLabel, 11);
#else
   fGetVolLabel( pCD, szString );
#endif
   p = szString + strlen(szString);
   while (p > szString && *(p-1) == ' ')
      p--;
   *p = 0;

   if (pCD->BootSect.bpb.MediaDescriptor != 0xF8)
      {
      show_message("The media descriptor is incorrect\n", 0, 2400, 0, 0);
      pCD->ulErrorCount++;
      }

   pCD->fCleanOnBoot = GetDiskStatus(pCD);

   if (! pCD->fAutoCheck || ! pCD->fCleanOnBoot)
      {
      show_message("(UFAT32.DLL version %s compiled on " __DATE__ ")\n", 0, 0, 1, FAT32_VERSION);

      if( p > szString )
         show_message("The volume label is %s.", 0, 1375, 1, TYPE_STRING, szString);

      sprintf(szString, "%4.4X-%4.4X",
         HIUSHORT(pCD->BootSect.ulVolSerial), LOUSHORT(pCD->BootSect.ulVolSerial));
      show_message("The Volume Serial Number is %s.", 0, 1243, 1, TYPE_STRING, szString);
      }

   if (pCD->fAutoRecover && pCD->fCleanOnBoot)
      pCD->fAutoRecover = FALSE;

   if (pCD->fAutoCheck && pCD->fCleanOnBoot)
      {
      // cancel autocheck if disk is clean
      rc = 0;
      goto ChkDskMainExit;
      }

   rc = CheckFats(pCD);
   if (rc)
      {
      show_message("The copies of the FATs do not match.\n"
                   "Please run CHKDSK under Windows to correct this problem.\n", 2401, 0, 0);
      goto ChkDskMainExit;
      }
   rc = CheckFiles(pCD);
   rc = CheckFreeSpace(pCD);
   pCD->FSInfo.ulFreeClusters = pCD->ulFreeClusters;

   //if (! pCD->fAutoCheck)
   //if (pCD->DiskInfo.total_clusters != pCD->ulTotalClusters)
   //   {
   //   show_message("Total clusters mismatch!\n", 2402, 0, 0);
   //   }

   //if (pCD->DiskInfo.avail_clusters != pCD->ulFreeClusters)
   //   {
   //   show_message("The stored free disk space is incorrect.\n", 0, 0, 0);
   //   show_message("(%lu free allocation units are reported,\nwhile %lu free units are detected.)\n",
   //      2403, 0, 2, pCD->DiskInfo.avail_clusters, pCD->ulFreeClusters);
      if (pCD->fFix)
         {
         ULONG ulFreeBlocks;
         //rc = DosDevIOCtl(pCD->hDisk, IOCTL_FAT32, FAT32_GETFREESPACE,
         //                 NULL, 0, NULL,
         //                 &ulFreeBlocks, sizeof(ulFreeBlocks), &dummy);
         //rc = DosDevIOCtl2(&ulFreeBlocks, sizeof(ulFreeBlocks),
         //   NULL, 0,
         //   FAT32_GETFREESPACE, IOCTL_FAT32, pCD->hDisk);
         ulFreeBlocks = GetFreeSpace(pCD);
         //if (!rc)
            show_message("The correct free space is set to %lu allocation units.\n", 2404, 0, 1,
               ulFreeBlocks);
         //else
            //{
            //printf("Setting correct free space failed, rc = %u.\n", rc);
            //pCD->ulErrorCount++;
            //}
         }
      //else
      //   pCD->ulErrorCount++;
      //}

   show_message("\n%lf bytes total disk space.", 0, 1361, 1,
      TYPE_DOUBLE, (DOUBLE)pCD->ulTotalClusters * pCD->ulClusterSize);
   if (pCD->ulBadClusters)
      show_message("%lf bytes in bad sectors.", 0, 1362, 1,
         TYPE_DOUBLE, (DOUBLE)pCD->ulBadClusters * pCD->ulClusterSize);
   show_message("%lf bytes in %lu hidden files.", 0, 1363, 2,
      TYPE_DOUBLE, (DOUBLE)pCD->ulHiddenClusters * pCD->ulClusterSize,
      TYPE_LONG, pCD->ulHiddenFiles);
   show_message("%lf bytes in %lu directories.", 0, 1364, 2,
      TYPE_DOUBLE, (DOUBLE)pCD->ulDirClusters * pCD->ulClusterSize,
      TYPE_LONG, pCD->ulTotalDirs);
   show_message("%lf bytes in extended attributes.", 0, 1819, 1,
      TYPE_DOUBLE, (DOUBLE)pCD->ulEAClusters * pCD->ulClusterSize);
   show_message("%lf bytes in %lu user files.", 0, 1365, 2,
      TYPE_DOUBLE, (DOUBLE)pCD->ulUserClusters * pCD->ulClusterSize,
      TYPE_LONG, pCD->ulUserFiles);

   if (pCD->ulRecoveredClusters)
      show_message("%lf bytes in %lu user files.", 0, 1365, 2,
         TYPE_DOUBLE, (DOUBLE)pCD->ulRecoveredClusters * pCD->ulClusterSize,
         TYPE_LONG, pCD->ulRecoveredFiles);

   if (pCD->ulLostClusters)
      show_message("%lf bytes disk space would be freed.", 0, 1359, 1,
         TYPE_DOUBLE, (DOUBLE)pCD->ulLostClusters * pCD->ulClusterSize);

   show_message("%lf bytes available on disk.", 0, 1368, 2,
      TYPE_DOUBLE, (DOUBLE)pCD->ulFreeClusters * pCD->ulClusterSize,
      TYPE_LONG, 0L);

   show_message("\n", 0, 0, 0);

   show_message("%lu bytes in each allocation unit.", 0, 1304, 1,
      TYPE_LONG, (ULONG)pCD->ulClusterSize);

   show_message("%lu total allocation units.", 0, 1305, 1,
      TYPE_LONG, pCD->ulTotalClusters);

   show_message("%lu available allocation units on disk.", 0, 1306, 1,
      TYPE_LONG, pCD->ulFreeClusters);

   if (pCD->ulTotalChains > 0)
      show_message("\n%u%% of the files and directories are fragmented.\n", 0, 0, 1,
         (USHORT)(pCD->ulFragmentedChains * 100 / pCD->ulTotalChains));

ChkDskMainExit:
   if (pCD->ulErrorCount)
      {
      show_message("\n", 0, 0, 0);
      if (!pCD->fFix)
         show_message(NULL, 0, 1339, 0);
      else
         show_message("Errors may still exist on this volume. \nRecommended action: Run CHKDSK under Windows.\n", 0, 0, 0);
      }
   else if (pCD->fFix)
      {
      //DosDevIOCtl(NULL, NULL, FAT32_MARKVOLCLEAN, IOCTL_FAT32, pCD->hDisk);
      //DosDevIOCtl(pCD->hDisk, IOCTL_FAT32, FAT32_MARKVOLCLEAN,
      //            NULL, 0, NULL, NULL, 0, NULL);
      // set disk clean
      MarkDiskStatus(pCD, TRUE);
      }

   if (pCD->fFix)
      {
      // write chkdsk log
      if (! pCD->fCleanOnBoot || ! pCD->fAutoCheck )
         MakeFile(pCD, pCD->BootSect.bpb.RootDirStrtClus, "chkdsk.log", logbuf, logbufpos);

      // remount disk for changes to take effect
      if (! pCD->fAutoCheck)
         remount_media(pCD->hDisk);
      }

   return rc;
}


ULONG MarkVolume(PCDINFO pCD, BOOL fClean)
{
ULONG rc;
ULONG dummy = 0;

   //rc = DosDevIOCtl(pCD->hDisk,
   //       IOCTL_FAT32,
   //       FAT32_FORCEVOLCLEAN,
   //       (PVOID)&fClean, sizeof(fClean), &dummy,
   //       NULL, 0, NULL);
//   rc = DosDevIOCtl2( NULL, 0,
//      (PVOID)&fClean, sizeof(fClean),
//      FAT32_FORCEVOLCLEAN,
//      IOCTL_FAT32,
//      pCD->hDisk);
   rc = MarkDiskStatus(pCD, fClean);

   return rc;
}

ULONG CheckFats(PCDINFO pCD)
{
PBYTE pSector;
USHORT nFat;
ULONG ulSector;
ULONG  rc;
USHORT usPerc = 0xFFFF;
BOOL   fDiff;
ULONG  ulCluster;
USHORT usIndex;
PULONG pulCluster;
USHORT fRetco;

/*
//   printf("Each fat contains %lu sectors\n",
//      pCD->BootSect.bpb.BigSectorsPerFat);
*/
   show_message("CHKDSK is checking fats :    ", 0, 0, 0);

   if (pCD->BootSect.bpb.ExtFlags & 0x0080)
      {
      show_message("There is only one active FAT.\n", 2405, 0, 0);
      return 0;
      }

   //pSector = halloc(pCD->BootSect.bpb.NumberOfFATs, BLOCK_SIZE);
   pSector = calloc(pCD->BootSect.bpb.NumberOfFATs, BLOCK_SIZE);
   if (!pSector)
      return ERROR_NOT_ENOUGH_MEMORY;

   fDiff = FALSE;
   ulCluster = 0L;
   fRetco = 0;
   for (ulSector = 0; ulSector < pCD->BootSect.bpb.BigSectorsPerFat; ulSector+=32)
      {
      USHORT usNew  = (USHORT)(ulSector * 100 / pCD->BootSect.bpb.BigSectorsPerFat);
      USHORT nSectors;

      if (!pCD->fPM && !fToFile && usNew != usPerc)
         {
         printf("\b\b\b\b%3u%%", usNew);
         usPerc = usNew;
         }

      nSectors = BLOCK_SIZE / 512;
      if ((ULONG)nSectors > pCD->BootSect.bpb.BigSectorsPerFat - ulSector)
         nSectors = (USHORT)(pCD->BootSect.bpb.BigSectorsPerFat - ulSector);


      for (nFat = 0; nFat < pCD->BootSect.bpb.NumberOfFATs; nFat++)
         {
         rc = ReadSector(pCD,
            pCD->ulActiveFatStart + (nFat * pCD->BootSect.bpb.BigSectorsPerFat) + ulSector,
               nSectors, pSector + nFat * BLOCK_SIZE);
         }
      for (nFat = 0; nFat < (USHORT)(pCD->BootSect.bpb.NumberOfFATs - 1); nFat++)
         {
         if (memcmp(pSector + nFat * BLOCK_SIZE,
                    pSector + nFat * BLOCK_SIZE + BLOCK_SIZE,
                    nSectors * 512))
            fDiff = TRUE;
         }

      pulCluster = (PULONG)pSector;
      if (!ulSector)
         {
         pulCluster += 2;
         ulCluster = 2;
         usIndex = 2;
         }
      else
         usIndex = 0;
      for (; ulCluster < pCD->ulTotalClusters + 2 && usIndex < nSectors * 128; usIndex++)
         {
         if ((*pulCluster & FAT_EOF) >= pCD->ulTotalClusters + 2)
            {
            ULONG ulVal = *pulCluster & FAT_EOF;
            if (!(ulVal >= FAT_BAD_CLUSTER && ulVal <= FAT_EOF))
               {
               show_message("FAT Entry for cluster %lu contains an invalid value.\n", 2406, 0, 1,
                  ulCluster);
               fRetco = 1;
               }
            }
         pulCluster++;
         ulCluster++;
         }
      }

   if (!pCD->fPM && !fToFile)
      printf("\b\b\b\b");
   if (fDiff)
      {
      show_message("\n", 0, 0, 0);
      show_message("File Allocation Table (FAT) is bad on drive %s.", 2407, 1374, 1, TYPE_STRING, pCD->szDrive);
      pCD->ulErrorCount++;
      pCD->fFatOk = FALSE;
      fRetco = 1;
      }
   else
      {
      show_message("Ok.   \n", 2408, 0, 0);
      pCD->fFatOk = TRUE;
      }

   free(pSector);
   return fRetco;
}


ULONG CheckFiles(PCDINFO pCD)
{
   show_message("CHKDSK is checking files and directories...\n", 0, 0, 0);
   return CheckDir(pCD, pCD->BootSect.bpb.RootDirStrtClus, pCD->szDrive, 0L);
}

ULONG CheckFreeSpace(PCDINFO pCD)
{
ULONG ulCluster;
USHORT usPerc = 100;
BOOL fMsg = FALSE;
ULONG dummy = 0;

   show_message("CHKDSK is searching for lost data.", 0, 564, 0);

   pCD->ulFreeClusters = 0;
   for (ulCluster = 0; ulCluster < pCD->ulTotalClusters; ulCluster++)
      {
      USHORT usNew  = (USHORT)(ulCluster * 100 / pCD->ulTotalClusters);
      ULONG ulNext = GetNextCluster(pCD, ulCluster + 2, TRUE);

      if (!pCD->fPM && !fToFile && usNew != usPerc)
         {
         show_message("CHKDSK has searched %u%% of the disk.", 0, 563, 1, TYPE_PERC, usNew);
         show_message("\r", 0, 0, 0);
         usPerc = usNew;
         }
      /* bad cluster ? */
      if (ulNext == FAT_BAD_CLUSTER)
         {
         pCD->ulBadClusters++;
         MarkCluster(pCD, ulCluster+2, "Bad sectors");
         }
      else if (!ulNext)
         {
         MarkCluster(pCD, ulCluster+2, "Free space");
         pCD->ulFreeClusters++;
         }
      else
         {
         if (!ClusterInUse(pCD, ulCluster+2))
            {
            if (!fMsg)
               {
               show_message("\n", 0, 0, 0);
               show_message("The system detected lost data on disk %s.", 0, 562, 1, TYPE_STRING, pCD->szDrive);
               show_message("CHKDSK has searched %s%% of the disk.", 0, 563, 1, TYPE_STRING, pCD->szDrive);
               show_message("\r", 0, 0, 0);
               fMsg = TRUE;
               }
            RecoverChain(pCD, ulCluster+2);
            }
         }
      }

   if (!pCD->fPM && !fToFile)
      show_message("CHKDSK has searched %s%% of the disk.", 0, 563, 1, TYPE_PERC, 100);
   show_message("\n", 0, 0, 0);

   if (pCD->usLostChains)
      {
      BYTE bChar;
      USHORT usIndex;
      ULONG rc;

      if (pCD->usLostChains >= MAX_LOST_CHAINS)
         show_message("Warning!  Not enough memory is available for CHKDSK"
                      "to recover all lost data.", 0, 548, 0);

      if (!pCD->fAutoRecover)
         show_message("%lu lost clusters found in %lu chains."
                      "These clusters and chains will be erased unless you convert"
                      "them to files.  Do you want to convert them to files(Y/N)? ", 0, 1356, 2,
            TYPE_LONG2, pCD->ulLostClusters,
            TYPE_LONG2, (ULONG)pCD->usLostChains);
      fflush(stdout);

      for (;;)
         {
         if (!pCD->fAutoRecover)
            bChar = (BYTE)getch();
         else
            bChar = 'Y';
         if (bChar == 'Y' || bChar == 'y')
            {
            bChar = 'Y';
            break;
            }
         if (bChar == 'N' || bChar == 'n')
            {
            bChar = 'N';
            break;
            }
         }
      show_message("\n", 0, 0, 0);


      if (pCD->fFix)
         {
         for (usIndex = 0; usIndex < MAX_LOST_CHAINS &&
                           usIndex < pCD->usLostChains; usIndex++)
            {
            pCD->ulLostClusters -= pCD->rgulSize[usIndex];
            if (bChar == 'Y')
               LostToFile(pCD, pCD->rgulLost[usIndex], pCD->rgulSize[usIndex]);
            else
               {
               //rc = DosDevIOCtl(pCD->hDisk, IOCTL_FAT32, FAT32_DELETECHAIN, 
               //             (PVOID)&pCD->rgulLost[usIndex], 4, &dummy,
               //             NULL, 0, NULL);
               //rc = DosDevIOCtl2(NULL, 0,
               //   (PVOID)&pCD->rgulLost[usIndex], 4,
               //   FAT32_DELETECHAIN, IOCTL_FAT32, pCD->hDisk);
               rc = DeleteFatChain(pCD, pCD->rgulLost[usIndex]);
               if (rc == FALSE)
                  {
                  show_message("CHKDSK was unable to delete a lost chain.\n", 2409, 0, 0);
                  pCD->ulErrorCount++;
                  }

               else
                  pCD->ulFreeClusters += pCD->rgulSize[usIndex];
               }
            }
         }
      }
   return 0;
}

BOOL ClusterInUse(PCDINFO pCD, ULONG ulCluster)
{
ULONG ulOffset;
USHORT usShift;
BYTE bMask;

   if (ulCluster >= pCD->ulTotalClusters + 2)
      {
      show_message("An invalid cluster number %8.8lX was found.\n", 2410, 0, 1, ulCluster);
      return TRUE;
      }

   ulCluster -= 2;
   ulOffset = ulCluster / 8;
   usShift = (USHORT)(ulCluster % 8);
   bMask = (BYTE)(0x80 >> usShift);
   if (pCD->pFatBits[ulOffset] & bMask)
      return TRUE;
   else
      return FALSE;
}


ULONG CheckDir(PCDINFO pCD, ULONG ulDirCluster, PSZ pszPath, ULONG ulParentDirCluster)
{
static BYTE szLongName[512] = "";
static BYTE szShortName[13] = "";
static MARKFILEEASBUF Mark;

int iIndex;
//DIRENTRY _huge * pDir;
//DIRENTRY _huge * pEnd;
DIRENTRY * pDir;
DIRENTRY * pEnd;
DIRENTRY * pPrevDir;
PBYTE pbCluster;
PBYTE pbPath;
ULONG ulCluster;
ULONG ulClusters;
ULONG ulBytesNeeded;
//BYTE _huge * p;
BYTE * p;
BYTE bCheckSum, bCheck;
ULONG ulClustersNeeded;
ULONG ulClustersUsed;
ULONG ulEntries;
PBYTE pEA;
ULONG rc;
ULONG dummy = 0;
UCHAR fModified = FALSE;

   if (!ulDirCluster)
      {
      show_message("ERROR: Cluster for %s is 0!\n", 2411, 0, 1, pszPath);
      return TRUE;
      }

   pCD->ulTotalDirs++;

   pbPath = malloc(512);
   strcpy(pbPath, "Directory ");
   strcat(pbPath, pszPath);
   ulClusters = GetClusterCount(pCD, ulDirCluster, pbPath);

   pCD->ulDirClusters += ulClusters;

   if (pCD->fDetailed == 2)
      show_message("\n\nDirectory of %s (%lu clusters)\n\n", 0, 0, 0, pszPath, ulClusters);

   ulBytesNeeded = (ULONG)pCD->BootSect.bpb.SectorsPerCluster * (ULONG)pCD->BootSect.bpb.BytesPerSector * ulClusters;
   //pbCluster = halloc(ulClusters, pCD->BootSect.bpb.SectorsPerCluster * pCD->BootSect.bpb.BytesPerSector);
   pbCluster = calloc(ulClusters, pCD->BootSect.bpb.SectorsPerCluster * pCD->BootSect.bpb.BytesPerSector);
   if (!pbCluster)
      {
      show_message("ERROR:Directory %s is too large ! (Not enough memory!)\n", 2412, 0, 1, pszPath);
      return ERROR_NOT_ENOUGH_MEMORY;
      }

   ulCluster = ulDirCluster;
   p = pbCluster;
   while (ulCluster != FAT_EOF)
      {
      ReadCluster(pCD, ulCluster, p);
      ulCluster = GetNextCluster(pCD, ulCluster, FALSE);
      if (!ulCluster)
         ulCluster = FAT_EOF;
      p += pCD->BootSect.bpb.SectorsPerCluster * pCD->BootSect.bpb.BytesPerSector;
      }

   memset(szLongName, 0, sizeof(szLongName));
   //pDir = (DIRENTRY _huge *)pbCluster;
   //pEnd = (DIRENTRY _huge *)(p - sizeof (DIRENTRY));
   pDir = (DIRENTRY *)pbCluster;
   pEnd = (DIRENTRY *)(p - sizeof (DIRENTRY));
   ulEntries = 0;
   bCheck = 0;
   while (pDir <= pEnd)
      {
      if (pDir->bFileName[0] && pDir->bFileName[0] != 0xE5)
         {
         if (pDir->bAttr == FILE_LONGNAME)
            {
            if (strlen(szLongName) && bCheck != pDir->bReserved)
               {
               show_message("A lost long filename was found: %s\n", 2413, 0, 1,
                  szLongName);
               if (pCD->fFix)
                  {
                  // mark it as deleted
                  pPrevDir = pDir - 1;
                  while (pPrevDir->bAttr == FILE_LONGNAME &&
                         pPrevDir->bFileName[0] &&
                         pPrevDir->bFileName[0] != 0xE5)
                     {
                     pPrevDir->bFileName[0] = 0xE5;
                     pPrevDir--;
                     }
                  fModified = TRUE;
                  }
               else
                  pCD->ulErrorCount++;
               memset(szLongName, 0, sizeof(szLongName));
               }
            bCheck = pDir->bReserved;
            fGetLongName(pDir, szLongName, sizeof(szLongName));
            }
         else
            {
            bCheckSum = 0;
            for (iIndex = 0; iIndex < 11; iIndex++)
               {
               if (bCheckSum & 0x01)
                  {
                  bCheckSum >>=1;
                  bCheckSum |= 0x80;
                  }
               else
                  bCheckSum >>=1;
               bCheckSum += pDir->bFileName[iIndex];
               }
            if (strlen(szLongName) && bCheck != bCheckSum)
               {
               show_message("The longname %s does not belong to %s\\%s\n", 0, 0, 3,
                  szLongName, pszPath, MakeName(pDir, szShortName, sizeof(szShortName)));
               show_message("The longname does not belong to %s.\n", 2414, 0, 1,
                  MakeName(pDir, szShortName, sizeof(szShortName)));
               if (pCD->fFix)
                  {
                  // mark it as deleted
                  pPrevDir = pDir - 1;
                  while (pPrevDir->bAttr == FILE_LONGNAME &&
                         pPrevDir->bFileName[0] &&
                         pPrevDir->bFileName[0] != 0xE5)
                     {
                     pPrevDir->bFileName[0] = 0xE5;
                     pPrevDir--;
                     }
                  fModified = TRUE;
                  }
               else
                  pCD->ulErrorCount++;
               memset(szLongName, 0, sizeof(szLongName));
               }

            /* support for the FAT32 variation of WinNT family */
            if( !*szLongName && HAS_WINNT_EXT( pDir->fEAS ))
            {
                PBYTE pDot;

                MakeName( pDir, szLongName, sizeof( szLongName ));
                pDot = strchr( szLongName, '.' );

                if( HAS_WINNT_EXT_NAME( pDir->fEAS )) /* name part is lower case */
                {
                    if( pDot )
                        *pDot = 0;

                        strlwr( szLongName );

                        if( pDot )
                        *pDot = '.';
                }

                if( pDot && HAS_WINNT_EXT_EXT( pDir->fEAS )) /* ext part is lower case */
                    strlwr( pDot + 1 );
            }

            if (pCD->fDetailed == 2)
               {
               show_message("%-13.13s", 0, 0, 1, MakeName(pDir, szShortName, sizeof(szShortName)));
               if (pDir->bAttr & FILE_DIRECTORY)
                  show_message("<DIR>      ", 0, 0, 0);
               else
                  show_message("%10lu ", 0, 0, 1, pDir->ulFileSize);

/*               printf("%8.8lX ", MAKEP(pDir->wClusterHigh, pDir->wCluster));*/

               show_message("%s ", 0, 0, 1, szLongName);
               }

            /*
               Construct full path
            */
            strcpy(pbPath, pszPath);
            if (lastchar(pbPath) != '\\')
               strcat(pbPath, "\\");
            if (strlen(szLongName))
               strcat(pbPath, szLongName);
            else
               {
               MakeName(pDir, szLongName, sizeof(szLongName));
               strcat(pbPath, szLongName);
               }

            if( f32Parms.fEAS && HAS_OLD_EAS( pDir->fEAS ))
            {
                show_message("%s has old EA mark byte(0x%0X).\n", 2415, 0, 2, pbPath, pDir->fEAS );
                if (pCD->fFix)
                {
                     strcpy(Mark.szFileName, pbPath);
                     Mark.fEAS = ( BYTE )( pDir->fEAS == FILE_HAS_OLD_EAS ? FILE_HAS_EAS : FILE_HAS_CRITICAL_EAS );
                     //rc = DosDevIOCtl(pCD->hDisk, IOCTL_FAT32, FAT32_SETEAS,
                     //                (PVOID)&Mark, sizeof(Mark), &dummy,
                     //                NULL, 0, NULL);
                     //rc = DosDevIOCtl2(NULL, 0,
                     //                  (PVOID)&Mark, sizeof(Mark),
                     //                  FAT32_SETEAS, IOCTL_FAT32, pCD->hDisk);
                     rc = GetSetFileEAS(pCD, FAT32_SETEAS, (PMARKFILEEASBUF)&Mark);
                     if (!rc)
                        show_message("This has been corrected.\n", 0, 0, 0);
                     else
                        show_message("SYS%4.4u: Unable to correct problem.\n", 2416, 0, 1, rc);
                }
            }

#if 1
            if( f32Parms.fEAS && pDir->fEAS && !HAS_WINNT_EXT( pDir->fEAS ) && !HAS_EAS( pDir->fEAS ))
                show_message("%s has unknown EA mark byte(0x%0X).\n", 2417, 0, 2, pbPath, pDir->fEAS );
#endif

#if 0
            if( f32Parms.fEAS && HAS_EAS( pDir->fEAS ))
                show_message("%s has EA byte(0x%0X).\n", 2418, 0, 2, pbPath, pDir->fEAS );
#endif

            if (f32Parms.fEAS && HAS_EAS( pDir->fEAS ))
               {
               FILESTATUS fStat;
               ULONG ulDirCluster, ulFileCluster;
               DIRENTRY DirEntry;
               PSZ pszFile;

               strcpy(Mark.szFileName, pbPath);
               strcat(Mark.szFileName, EA_EXTENTION);
               //rc = DosQueryPathInfo(Mark.szFileName, FIL_STANDARD, (PBYTE)&fStat, sizeof(fStat));
               rc = NO_ERROR;
               ulDirCluster = FindDirCluster(pCD, Mark.szFileName, -1, 0xffff, &pszFile);

               if (ulDirCluster == FAT_EOF)
                  rc = ERROR_PATH_NOT_FOUND;
               else
                  {
                  ulFileCluster = FindPathCluster(pCD, ulDirCluster, pszFile, &DirEntry, NULL);

                  if (ulFileCluster == FAT_EOF)
                     rc = ERROR_FILE_NOT_FOUND;
                  }
               //rc = DosQPathInfo(Mark.szFileName, FIL_STANDARD, (PBYTE)&fStat, sizeof(fStat), 0L);
               if (rc)
                  {
                  show_message("%s is marked having EAs, but the EA file (%s) is not found. (SYS%4.4u)\n",
                     0, 0, 3, pbPath, Mark.szFileName, rc);
                  show_message("File marked having EAs, but the EA file (%s) is not found. (SYS%4.4u)\n",
                     2419, 0, 2, Mark.szFileName, rc);
                  if (pCD->fFix)
                     {
                     strcpy(Mark.szFileName, pbPath);
                     Mark.fEAS = FILE_HAS_NO_EAS;
                     //rc = DosDevIOCtl(pCD->hDisk, IOCTL_FAT32, FAT32_SETEAS, 
                     //              (PVOID)&Mark, sizeof(Mark), &dummy,
                     //              NULL, 0, NULL);
                     //rc = DosDevIOCtl2(NULL, 0,
                     //   (PVOID)&Mark, sizeof(Mark),
                     //   FAT32_SETEAS, IOCTL_FAT32, pCD->hDisk);
                     rc = GetSetFileEAS(pCD, FAT32_SETEAS, (PMARKFILEEASBUF)&Mark);
                     if (!rc)
                        show_message("This has been corrected.\n", 0, 0, 0);
                     else
                        show_message("SYS%4.4u: Unable to correct problem.\n", 2416, 0, 1, rc);
                     }
                  }
               else if (!DirEntry.ulFileSize)
               //else if (!fStat.cbFile)
                  {
                  show_message("%s is marked having EAs, but the EA file (%s) is empty.\n", 0, 0, 2, pbPath, Mark.szFileName);
                  show_message("File marked having EAs, but the EA file (%s) is empty.\n", 2420, 0, 1, Mark.szFileName);
                  if (pCD->fFix)
                     {
                     unlink(Mark.szFileName);
                     strcpy(Mark.szFileName, pbPath);
                     Mark.fEAS = FILE_HAS_NO_EAS;
                     //rc = DosDevIOCtl(pCD->hDisk, IOCTL_FAT32, FAT32_SETEAS, 
                     //       (PVOID)&Mark, sizeof(Mark), &dummy,
                     //       NULL, 0, NULL);
                     //rc = DosDevIOCtl2(NULL, 0,
                     //   (PVOID)&Mark, sizeof(Mark),
                     //   FAT32_SETEAS, IOCTL_FAT32, pCD->hDisk);
                     rc = GetSetFileEAS(pCD, FAT32_SETEAS, (PMARKFILEEASBUF)&Mark);
                     if (!rc)
                        show_message("This has been corrected.\n", 0, 0, 0);
                     else
                        show_message("SYS%4.4u: Unable to correct problem.\n", 2416, 0, 1, rc);
                     }
                  }
               }

            if (!(pDir->bAttr & FILE_DIRECTORY))
               {
               ulClustersNeeded = pDir->ulFileSize / pCD->ulClusterSize +
                  (pDir->ulFileSize % pCD->ulClusterSize ? 1:0);
               ulClustersUsed = GetClusterCount(pCD,(ULONG)pDir->wClusterHigh * 0x10000 + pDir->wCluster, pbPath);
               pEA = strstr(pbPath, EA_EXTENTION);
               if (f32Parms.fEAS && pEA && pDir->ulFileSize)
                  {
                  ULONG rc;
                  FILESTATUS3 fs;

                  pCD->ulEAClusters += ulClustersUsed;

                  memset(&Mark, 0, sizeof(Mark));
                  memcpy(Mark.szFileName, pbPath, pEA - pbPath);

                  //rc = DosDevIOCtl(pCD->hDisk, IOCTL_FAT32, FAT32_QUERYEAS, 
                  //                 (PVOID)&Mark, sizeof(Mark), &dummy,
                  //                 NULL, 0, NULL);
                  //rc = DosDevIOCtl2(NULL, 0,
                  //   (PVOID)&Mark, sizeof(Mark),
                  //   FAT32_QUERYEAS, IOCTL_FAT32, pCD->hDisk);
                  rc = GetSetFileEAS(pCD, FAT32_QUERYEAS, (PMARKFILEEASBUF)&Mark);
                  if (rc == 2 || rc == 3)
                     {
                     show_message("A lost Extended attribute was found (for %s)\n", 2421, 0, 1,
                        Mark.szFileName);
                     if (pCD->fFix)
                        {
                        DIRENTRY DirEntry, DirNew, DstDirEntry;
                        ULONG ulDirCluster, ulFileCluster;
                        ULONG ulDstFileCluster;
                        PSZ pszFile;

                        strcat(Mark.szFileName, ".EA");
                        rc = MarkVolume(pCD, TRUE);
                        if (!rc)
                           {
                           //DosQueryPathInfo(pbPath, FIL_STANDARD, &fs, sizeof(fs));
                           ulDirCluster = FindDirCluster(pCD, pbPath, -1, 0xffff, &pszFile);

                           rc = NO_ERROR;
                           if (ulDirCluster == FAT_EOF)
                              rc = ERROR_PATH_NOT_FOUND;
                           else
                              {
                              ulFileCluster = FindPathCluster(pCD, ulDirCluster, pszFile, &DirEntry, NULL);

                              if (ulFileCluster == FAT_EOF)
                                 rc = ERROR_FILE_NOT_FOUND;
                              }
                           
			   //fs.attrFile = FILE_NORMAL;
                           memcpy(&DirNew, &DirEntry, sizeof(DIRENTRY));
                           DirNew.bAttr = FILE_NORMAL;
                           //rc = DosSetPathInfo(pbPath, FIL_STANDARD, &fs, sizeof(fs), 0);
                           //rc = DosSetFileMode(pbPath, FILE_NORMAL, 0L);
                           rc = ModifyDirectory(pCD, ulDirCluster, MODIFY_DIR_UPDATE,
                                                &DirEntry, &DirNew, NULL);
                           }
                        if (!rc)
                           {
                           //rc = DosMove(pbPath, Mark.szFileName, 0L);
                           //rc = DosMove(pbPath, Mark.szFileName);
                           ulDstFileCluster = FindPathCluster(pCD, ulDirCluster, Mark.szFileName, &DstDirEntry, NULL);

                           rc = NO_ERROR;
                           if (ulDstFileCluster == FAT_EOF)
                              rc = ERROR_FILE_NOT_FOUND;

                           if (ulDstFileCluster == ulFileCluster)
                              rc = ERROR_ACCESS_DENIED;

                           rc = ModifyDirectory(pCD, ulDirCluster, MODIFY_DIR_RENAME,
                                                &DirEntry, &DirNew, Mark.szFileName);
                           }
                        if (!rc)
                           show_message("This attribute has been converted to a file \n(%s).\n", 2422, 0, 1, Mark.szFileName);
                        else
                           {
                           show_message("Cannot convert %s. SYS%4.4u.\n", 2423, 0, 2,
                              pbPath, rc);
                           pCD->ulErrorCount++;
                           }
                        MarkVolume(pCD, pCD->fCleanOnBoot);
                        }
                     }
                  else if (rc)
                     show_message("Retrieving EA flag for %s. SYS%4.4u occured.\n", 2424, 0, 2, Mark.szFileName, rc);
                  else
                     {
                     if ( !HAS_EAS( Mark.fEAS ))
                        {
                        show_message("EAs detected for %s, but it is not marked having EAs.\n", 2425, 0, 1, Mark.szFileName);
                        if (pCD->fFix)
                           {
                           Mark.fEAS = FILE_HAS_EAS;
                           //rc = DosDevIOCtl(pCD->hDisk, IOCTL_FAT32, FAT32_SETEAS, 
                           //             (PVOID)&Mark, sizeof(Mark), &dummy,
                           //             NULL, 0, NULL);
                           //rc = DosDevIOCtl2(NULL, 0,
                           //   (PVOID)&Mark, sizeof(Mark),
                           //   FAT32_SETEAS, IOCTL_FAT32, pCD->hDisk);
                           rc = GetSetFileEAS(pCD, FAT32_SETEAS, (PMARKFILEEASBUF)&Mark);
                           if (!rc)
                              show_message("This has been corrected.\n", 0, 0, 0);
                           else
                              show_message("SYS%4.4u: Unable to correct problem.\n", 2416, 0, 1, rc);
                           }
                        }
                     }
                  }
               else if (pDir->bAttr & FILE_HIDDEN)
                  {
                  pCD->ulHiddenClusters += ulClustersUsed;
                  pCD->ulHiddenFiles++;
                  }
               else
                  {
                  pCD->ulUserClusters += ulClustersUsed;
                  pCD->ulUserFiles++;
                  }
               if (ulClustersNeeded != ulClustersUsed)
                  {
                  if (!pCD->fFix)
                     {
                     show_message("File allocation error detected for %s\\%s\n", 0, 0, 2,
#if 0
                        pszPath, MakeName(pDir, szShortName, sizeof(szShortName)));
#else
                        pszPath, szLongName);
#endif
                     show_message("File allocation error detected for %s.\n", 2426, 0, 1,
                        szLongName);
                     pCD->ulErrorCount++;
                     }
                  else
                     {
                     FILESIZEDATA fs;
                     ULONG rc;

                     memset(&fs, 0, sizeof(fs));
                     strcpy(fs.szFileName, pszPath);
                     if (lastchar(fs.szFileName) != '\\')
                        strcat(fs.szFileName, "\\");
                     strcat(fs.szFileName, MakeName(pDir, szShortName, sizeof(szShortName)));
                     fs.ulFileSize = ulClustersUsed * pCD->ulClusterSize;
                     //rc = DosDevIOCtl(pCD->hDisk, IOCTL_FAT32, FAT32_SETFILESIZE, 
                     //          (PVOID)&fs, sizeof(fs), &dummy,
                     //          NULL, 0, NULL);
                     //rc = DosDevIOCtl2(NULL, 0, 
                     //   (PVOID)&fs, sizeof(fs),
                     //   FAT32_SETFILESIZE, IOCTL_FAT32, pCD->hDisk);
                     rc = SetFileSize(pCD, (PFILESIZEDATA)&fs);
                     strcpy( strrchr( fs.szFileName, '\\' ) + 1, szLongName );
                     if (rc)
                        {
                        show_message("File allocation error detected for %s.\n", 2426, 0, 1,
                           fs.szFileName);
                        show_message("CHKDSK was unable to correct the filesize. SYS%4.4u.\n", 2428, 0, 1, rc);
                        pCD->ulErrorCount++;
                        }
                     else
                        show_message("CHKDSK corrected an allocation error for the file %s.", 0, 560, 1,
                           TYPE_STRING, fs.szFileName);
                     }

                  }
               }
            if (pCD->fDetailed == 2)
               show_message("\n", 0, 0, 0);

           memset(szLongName, 0, sizeof(szLongName));
            }
         ulEntries++;
         }
      pDir++;

      }
   if (pCD->fDetailed == 2)
      show_message("%ld files\n", 0, 0, 1, ulEntries);

   bCheck = 0;
   pDir = (PDIRENTRY)pbCluster;
   memset(szLongName, 0, sizeof(szLongName));
   while (pDir <= pEnd)
      {
      if (pDir->bFileName[0] && pDir->bFileName[0] != 0xE5)
         {
         if (pDir->bAttr == FILE_LONGNAME)
            {
            if (strlen(szLongName) && bCheck != pDir->bReserved)
               memset(szLongName, 0, sizeof(szLongName));
            bCheck = pDir->bReserved;
            fGetLongName(pDir, szLongName, sizeof(szLongName));
            }
         else
            {
            if (pDir->bAttr & FILE_DIRECTORY &&
                  !(pDir->bAttr & FILE_VOLID))
               {
               ulCluster = (ULONG)pDir->wClusterHigh * 0x10000 + pDir->wCluster;
               bCheckSum = 0;
               for (iIndex = 0; iIndex < 11; iIndex++)
                  {
                  if (bCheckSum & 0x01)
                     {
                     bCheckSum >>=1;
                     bCheckSum |= 0x80;
                     }
                  else
                     bCheckSum >>=1;
                  bCheckSum += pDir->bFileName[iIndex];
                  }
               if (strlen(szLongName) && bCheck != bCheckSum)
                  {
                  show_message("Non matching longname %s for %-11.11s\n", 0, 0, 2,
                     szLongName, pDir->bFileName);
                  show_message("Non matching longname for %s.\n", 2429, 0, 1,
                     pDir->bFileName);
                  memset(szLongName, 0, sizeof(szLongName));
                  }

               /* support for the FAT32 variation of WinNT family */
               if( !*szLongName && HAS_WINNT_EXT( pDir->fEAS ))
               {
                    PBYTE pDot;

                    MakeName( pDir, szLongName, sizeof( szLongName ));
                    pDot = strchr( szLongName, '.' );

                    if( HAS_WINNT_EXT_NAME( pDir->fEAS )) /* name part is lower case */
                    {
                        if( pDot )
                            *pDot = 0;

                        strlwr( szLongName );

                        if( pDot )
                            *pDot = '.';
                    }

                    if( pDot && HAS_WINNT_EXT_EXT( pDir->fEAS )) /* ext part is lower case */
                        strlwr( pDot + 1 );
               }

               if (!memicmp(pDir->bFileName,      ".          ", 11))
                  {
                  if (ulCluster != ulDirCluster)
                     show_message(". entry in %s is incorrect!\n", 2430, 0, 1, pszPath);
                  }
               else if (!memicmp(pDir->bFileName, "..         ", 11))
                  {
                  if (ulCluster != ulParentDirCluster)
                     show_message(".. entry in %s is incorrect! (%lX %lX)\n", 2431, 0, 3,
                        pszPath, ulCluster, ulParentDirCluster);
                  }
               else
                  {
                  /*
                     Construct full path
                  */
                  strcpy(pbPath, pszPath);
                  if (lastchar(pbPath) != '\\')
                     strcat(pbPath, "\\");
                  if (strlen(szLongName))
                     strcat(pbPath, szLongName);
                  else
                     {
                     MakeName(pDir, szLongName, sizeof(szLongName));
                     strcat(pbPath, szLongName);
                     }

                  memset(szLongName, 0, sizeof(szLongName));
                  CheckDir(pCD,
                     (ULONG)pDir->wClusterHigh * 0x10000 + pDir->wCluster,
                     pbPath, (ulDirCluster == pCD->BootSect.bpb.RootDirStrtClus ? 0L : ulDirCluster));
                  }
               }
            memset(szLongName, 0, sizeof(szLongName));
            }
         }
      pDir++;
      }

   if (pCD->fFix && fModified)
      {
      // write directory back
      ulCluster = ulDirCluster;
      p = pbCluster;
      while (ulCluster != FAT_EOF)
         {
         WriteCluster(pCD, ulCluster, p);
         ulCluster = GetNextCluster(pCD, ulCluster, FALSE);
         if (!ulCluster)
            ulCluster = FAT_EOF;
         p += pCD->BootSect.bpb.SectorsPerCluster * pCD->BootSect.bpb.BytesPerSector;
         }
      }

   free(pbCluster);
   free(pbPath);

   return 0;
}


ULONG GetClusterCount(PCDINFO pCD, ULONG ulCluster, PSZ pszFile)
{
ULONG ulCount;
ULONG ulNextCluster;
BOOL  fCont = TRUE;
BOOL  fShown = FALSE;

   ulCount = 0;
   if (!ulCluster)
      return ulCount;

   if (ulCluster  > pCD->ulTotalClusters + 2)
      {
      show_message("%s: Invalid start of cluster chain %lX found\n", 2432, 0, 2,
         pszFile, ulCluster);
      return 0;
      }

   while (ulCluster != FAT_EOF)
      {
      ulNextCluster = GetNextCluster(pCD, ulCluster, FALSE);
      if (!MarkCluster(pCD, ulCluster, pszFile))
         return ulCount;
      ulCount++;

      if (!ulNextCluster)
         {
         if (pCD->fFix)
            {
            show_message("CHKDSK found an improperly terminated cluster chain for %s ", 2433, 0, 1, pszFile);
            if (SetNextCluster(pCD, ulCluster, FAT_EOF) != FAT_EOF)
               {
               show_message(", but was unable to fix it.\n", 0, 0, 0);
               pCD->ulErrorCount++;
               }
            else
               show_message(" and corrected the problem.\n", 2434, 0, 0);
            }
         else
            {
            show_message("A bad terminated cluster chain was found for %s\n", 2435, 0, 1, pszFile);
            pCD->ulErrorCount++;
            }
         ulNextCluster = FAT_EOF;
         }

      if (ulNextCluster != FAT_EOF && ulNextCluster != ulCluster + 1)
         {
         if (pCD->fDetailed)
            {
            if (!fShown)
               {
               show_message("%s is fragmented\n", 2436, 0, 1, pszFile);
               fShown = TRUE;
               }
            }
         fCont = FALSE;
         }
      ulCluster = ulNextCluster;
      }
   pCD->ulTotalChains++;
   if (!fCont)
      pCD->ulFragmentedChains++;
   return ulCount;
}

BOOL MarkCluster(PCDINFO pCD, ULONG ulCluster, PSZ pszFile)
{
ULONG ulOffset;
USHORT usShift;
BYTE bMask;

   if (ClusterInUse(pCD, ulCluster))
      {
      show_message("%s is cross-linked on cluster %lu", 0, 1343, 2,
         TYPE_STRING, pszFile,
         TYPE_LONG, ulCluster);
      pCD->ulErrorCount++;
      return FALSE;
      }

   ulCluster -= 2;
   ulOffset = ulCluster / 8;
   usShift = (USHORT)(ulCluster % 8);
   bMask = (BYTE)(0x80 >> usShift);
   pCD->pFatBits[ulOffset] |= bMask;
   return TRUE;
}

PSZ MakeName(PDIRENTRY pDir, PSZ pszName, USHORT usMax)
{
PSZ p;
BYTE szExtention[4];

   memset(pszName, 0, usMax);
   strncpy(pszName, pDir->bFileName, 8);
   p = pszName + strlen(pszName);
   while (p > pszName && *(p-1) == 0x20)
      p--;
   *p = 0;

   memset(szExtention, 0, sizeof(szExtention));
   strncpy(szExtention, pDir->bExtention, 3);
   p = szExtention + strlen(szExtention);
   while (p > szExtention && *(p-1) == 0x20)
      p--;
   *p = 0;
   if (strlen(szExtention))
      {
      strcat(pszName, ".");
      strcat(pszName, szExtention);
      }
   return pszName;
}

ULONG fGetVolLabel( PCDINFO pCD, PSZ pszVolLabel )
{
PDIRENTRY pDirStart, pDir, pDirEnd;
ULONG ulCluster;
DIRENTRY DirEntry;
BOOL     fFound;

   pDir = NULL;

   pDirStart = malloc(pCD->ulClusterSize);
   if (!pDirStart)
      return ERROR_NOT_ENOUGH_MEMORY;

   fFound = FALSE;
   ulCluster = pCD->BootSect.bpb.RootDirStrtClus;
   while (!fFound && ulCluster != FAT_EOF)
      {
      ReadCluster(pCD, ulCluster, (PBYTE)pDirStart);
      pDir = pDirStart;
      pDirEnd = (PDIRENTRY)((PBYTE)pDirStart + pCD->ulClusterSize);
      while (pDir < pDirEnd)
         {
         if ((pDir->bAttr & 0x0F) == FILE_VOLID && pDir->bFileName[0] != DELETED_ENTRY)
            {
            fFound = TRUE;
            memcpy(&DirEntry, pDir, sizeof (DIRENTRY));
            break;
            }
         pDir++;
         }
      if (!fFound)
         {
         ulCluster = GetNextCluster(pCD, ulCluster, FALSE);
         if (!ulCluster)
            ulCluster = FAT_EOF;
         }
      }
   free(pDirStart);
   if (!fFound)
      memset(pszVolLabel, 0, 11);
   else
      memcpy(pszVolLabel, DirEntry.bFileName, 11);

   return 0;
}

BOOL fGetLongName(PDIRENTRY pDir, PSZ pszName, USHORT wMax)
{
static BYTE szLongName[30] = "";
static USHORT uniName[15] = {0};
USHORT wNameSize;
USHORT usIndex;
ULONG ulDataSize;
ULONG ulParmSize;
PLNENTRY pLN = (PLNENTRY)pDir;

   memset(szLongName, 0, sizeof(szLongName));
   memset(uniName, 0, sizeof(uniName));

   wNameSize = 0;
   for (usIndex = 0; usIndex < 5; usIndex ++)
      {
      if (pLN->usChar1[usIndex] != 0xFFFF)
         uniName[wNameSize++] = pLN->usChar1[usIndex];
      }
   for (usIndex = 0; usIndex < 6; usIndex ++)
      {
      if (pLN->usChar2[usIndex] != 0xFFFF)
         uniName[wNameSize++] = pLN->usChar2[usIndex];
      }
   for (usIndex = 0; usIndex < 2; usIndex ++)
      {
      if (pLN->usChar3[usIndex] != 0xFFFF)
         uniName[wNameSize++] = pLN->usChar3[usIndex];
      }

   ulDataSize = sizeof(szLongName);
   ulParmSize = sizeof(uniName);
   //DosFSCtl(szLongName, usDataSize, &usDataSize,
   //      (PVOID)uniName, usParmSize, &usParmSize,
   //      FAT32_WIN2OS,
   //      "FAT32",
   //      -1,
   //      FSCTL_FSDNAME,
   //      0);
   //DosFSCtl(szLongName,  ulDataSize, &ulDataSize,
   //      (PVOID)uniName, ulParmSize, &ulParmSize,
   //      FAT32_WIN2OS,
   //      "FAT32",
   //      -1,
   //      FSCTL_FSDNAME);
   Translate2OS2((PUSHORT)uniName, szLongName, ulDataSize);

   wNameSize = strlen( szLongName );

   if (strlen(pszName) + wNameSize > wMax)
      return FALSE;

   memmove(pszName + wNameSize, pszName, strlen(pszName) + 1);
   memcpy(pszName, szLongName, wNameSize);
   return TRUE;
}



ULONG GetNextCluster(PCDINFO pCD, ULONG ulCluster, BOOL fAllowBad)
{
PULONG pulCluster;
ULONG  ulSector;
ULONG  ulRet;

   ulSector = ulCluster / 128;
   if (!ReadFATSector(pCD, ulSector))
      return FAT_EOF;

   pulCluster = (PULONG)pCD->pbFATSector + ulCluster % 128;

   ulRet = *pulCluster & FAT_EOF;
   if (ulRet >= FAT_EOF2 && ulRet <= FAT_EOF)
      return FAT_EOF;

   if (ulRet == FAT_BAD_CLUSTER && fAllowBad)
      return ulRet;

   if (ulRet >= pCD->ulTotalClusters  + 2)
      {
      show_message("Error: Next cluster for %lu = %8.8lX\n", 0, 0, 2,
         ulCluster, *pulCluster);
      return FAT_EOF;
      }

   return ulRet;
}

BOOL ReadFATSector(PCDINFO pCD, ULONG ulSector)
{
ULONG rc;

   if (pCD->ulCurFATSector == ulSector)
      return TRUE;

   rc = ReadSector(pCD, pCD->ulActiveFatStart + ulSector, 1,
      (PBYTE)pCD->pbFATSector);
   if (rc)
      return FALSE;

   pCD->ulCurFATSector = ulSector;

   return TRUE;
}

BOOL RecoverChain(PCDINFO pCD, ULONG ulCluster)
{
ULONG  ulSize;
USHORT usIndex;


   if (!pCD->fFix)
      {
      pCD->ulErrorCount++;
      pCD->ulLostClusters ++;
      return TRUE;
      }

   for (usIndex = 0; usIndex < pCD->usLostChains &&
                     usIndex < MAX_LOST_CHAINS; usIndex++)
      {
      if (ClusterInChain(pCD, ulCluster, pCD->rgulLost[usIndex]))
         {
         ULONG ulNext = ulCluster;
         while (ulNext != pCD->rgulLost[usIndex])
            {
            MarkCluster(pCD, ulNext, "Lost cluster");
            pCD->rgulSize[usIndex]++;
            pCD->ulLostClusters++;
            ulNext = GetNextCluster(pCD, ulNext, FALSE);
            }
         pCD->rgulLost[usIndex] = ulCluster;
         return TRUE;
         }
      }

   ulSize = GetClusterCount(pCD, ulCluster, "Lost data");
   if (pCD->usLostChains < MAX_LOST_CHAINS)
      {
      pCD->rgulLost[pCD->usLostChains] = ulCluster;
      pCD->rgulSize[pCD->usLostChains] = ulSize;
      }
   pCD->ulLostClusters += ulSize;
   pCD->usLostChains++;
   return TRUE;
}

BOOL ClusterInChain(PCDINFO pCD, ULONG ulStart, ULONG ulCluster)
{
   while (ulStart && ulStart != FAT_EOF)
      {
      if (ulStart == ulCluster)
         return TRUE;
      ulStart = GetNextCluster(pCD, ulStart, FALSE);
      }
   return FALSE;
}

BOOL LostToFile(PCDINFO pCD, ULONG ulCluster, ULONG ulSize)
{
BYTE   szRecovered[CCHMAXPATH];
ULONG  rc, dummy1 = 0, dummy2 = 0;

   memset(szRecovered, 0, sizeof(szRecovered));
   //rc = DosDevIOCtl(pCD->hDisk, IOCTL_FAT32, FAT32_RECOVERCHAIN, 
   //           (PVOID)&ulCluster, 4, &dummy1,
   //           szRecovered, sizeof(szRecovered), &dummy2);
   //rc = DosDevIOCtl2(szRecovered, sizeof(szRecovered),
   //   (PVOID)&ulCluster, 4,
   //   FAT32_RECOVERCHAIN, IOCTL_FAT32, pCD->hDisk);
   rc = RecoverChain2(pCD, ulCluster, szRecovered, sizeof(szRecovered));
   if (rc)
      {
      pCD->ulErrorCount++;
      show_message("CHKDSK was unable to recover a lost chain. SYS%4.4u\n", 2437, 0, 1, rc);
      return FALSE;
      }
   pCD->ulRecoveredClusters += ulSize;
   pCD->ulRecoveredFiles++;

   show_message("CHKDSK placed recovered data in file %s.", 0, 574, 1, TYPE_STRING, szRecovered);
   return TRUE;
}
