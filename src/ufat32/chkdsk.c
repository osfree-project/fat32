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
PRIVATE ULONG GetClusterCount(PCDINFO pCD, ULONG ulCluster, PSZ pszFile);
PRIVATE BOOL   MarkCluster(PCDINFO pCD, ULONG ulCluster, PSZ pszFile);
PRIVATE PSZ    MakeName(PDIRENTRY pDir, PSZ pszName, USHORT usMax);
PRIVATE ULONG fGetVolLabel(PCDINFO pCD, PSZ pszVolLabel);
PRIVATE BOOL   fGetLongName(PDIRENTRY pDir, PSZ pszName, USHORT wMax);
PRIVATE BOOL   ClusterInUse(PCDINFO pCD, ULONG ulCluster);
PRIVATE BOOL RecoverChain(PCDINFO pCD, ULONG ulCluster);
PRIVATE BOOL LostToFile(PCDINFO pCD, ULONG ulCluster, ULONG ulSize);
PRIVATE BOOL ClusterInChain(PCDINFO pCD, ULONG ulStart, ULONG ulCluster);

ULONG ReadSector(PCDINFO pCD, ULONG ulSector, USHORT nSectors, PBYTE pbSector);
ULONG ReadCluster(PCDINFO pDrive, ULONG ulCluster, PBYTE pbCluster);
ULONG WriteSector(PCDINFO pCD, ULONG ulSector, USHORT nSectors, PBYTE pbSector);
ULONG WriteCluster(PCDINFO pCD, ULONG ulCluster, PVOID pbCluster);
ULONG ReadSect(HANDLE hFile, ULONG ulSector, USHORT nSectors, USHORT BytesPerSector, PBYTE pbSector);
ULONG WriteSect(HANDLE hf, ULONG ulSector, USHORT nSectors, USHORT BytesPerSector, PBYTE pbSector);

UCHAR GetFatType(PBOOTSECT pSect);
ULONG GetFatEntrySec(PCDINFO pCD, ULONG ulCluster);
ULONG GetFatEntriesPerSec(PCDINFO pCD);
ULONG GetFatEntry(PCDINFO pCD, ULONG ulCluster);
void  SetFatEntry(PCDINFO pCD, ULONG ulCluster, ULONG ulValue);

ULONG GetFatEntryBlock(PCDINFO pCD, ULONG ulCluster, USHORT usBlockSize);
ULONG GetFatEntriesPerBlock(PCDINFO pCD, USHORT usBlockSize);
ULONG GetFatSize(PCDINFO pCD);
ULONG GetFatEntryEx(PCDINFO pCD, PBYTE pFatStart, ULONG ulCluster, USHORT usBlockSize);
void  SetFatEntryEx(PCDINFO pCD, PBYTE pFatStart, ULONG ulCluster, ULONG ulValue, USHORT usBlockSize);

ULONG SetNextCluster(PCDINFO pCD, ULONG ulCluster, ULONG ulNext);
BOOL  GetDiskStatus(PCDINFO pCD);
ULONG GetFreeSpace(PCDINFO pCD);
BOOL MarkDiskStatus(PCDINFO pCD, BOOL fClean);
USHORT GetSetFileEAS(PCDINFO pCD, USHORT usFunc, PMARKFILEEASBUF pMark);
USHORT SetFileSize(PCDINFO pCD, PFILESIZEDATA pFileSize);
USHORT RecoverChain2(PCDINFO pCD, ULONG ulCluster, PBYTE pData, USHORT cbData);
USHORT MakeDirEntry(PCDINFO pCD, ULONG ulDirCluster, PDIRENTRY pNew, PSZ pszName);
BOOL DeleteFatChain(PCDINFO pCD, ULONG ulCluster);
VOID Translate2OS2(PUSHORT pusUni, PSZ pszName, USHORT usLen);
ULONG FindDirCluster(PCDINFO pCD, PSZ pDir, USHORT usCurDirEnd, USHORT usAttrWanted, PSZ *pDirEnd);
ULONG FindPathCluster(PCDINFO pCD, ULONG ulCluster, PSZ pszPath, PDIRENTRY pDirEntry, PSZ pszFullName);
APIRET ModifyDirectory(PCDINFO pCD, ULONG ulDirCluster, USHORT usMode, PDIRENTRY pOld, PDIRENTRY pNew, PSZ pszLongName);
APIRET MakeFile(PCDINFO pCD, ULONG ulDirCluster, PSZ pszOldFile, PSZ pszFile, PBYTE pBuf, ULONG cbBuf);
ULONG  GetNextCluster(PCDINFO pCD, ULONG ulCluster, BOOL fNoFatChain, BOOL fAllowBad);
BOOL   ReadFATSector(PCDINFO pCD, ULONG ulSector);
ULONG ReadFatSector(PCDINFO pCD, ULONG ulSector);
ULONG WriteFatSector(PCDINFO pCD, ULONG ulSector);
void CodepageConvInit(BOOL fSilent);
int lastchar(const char *string);

USHORT _Far16 _Pascal _loadds INIT16(HMODULE hmod, ULONG flag);

extern int logbufsize;
extern char *logbuf;
extern int  logbufpos;

static HANDLE hDisk = 0;

F32PARMS  f32Parms = {0};
static BOOL fToFile;

extern char msg;

static void Handler(INT iSignal)
{
   show_message("\nSignal %d was received\n", 0, 0, 1, iSignal);

   // remount disk for changes to take effect
   cleanup();
   exit(1);
}

static void usage(char *s)
{
   show_message("Usage: [c:\\] %s x: [/v[:[1|2]] [/p] [/f] [/c] [/a] [/h] [/?]]\n"
                "(c) Henk Kelder & Netlabs, covered by (L)GPL\n"
                "/v         verbose, /v[:<verboseness level>]\n"
                "/p         use with PM frontend\n"
                "/f         fix errors\n"
                "/c         auto recover files only if the FS\n"
                "           is in inconsistent state\n"
                "/a         autocheck\n"
                "/h or /?   show help\n",
                0, 0, 1, s);
}

int chkdsk_thread(int iArgc, char *rgArgv[], char *rgEnv[])
{
INT iArg;
HANDLE hFile;
ULONG rc = 0;
PCDINFO pCD;
BYTE   bSector[SECTOR_SIZE * 8];
struct extbpb dp;

#ifdef __OS2__
   /* Enable hard errors     */
   DosError(1);
#endif

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
            case 'H':
            case '?':
               // show help
               usage(rgArgv[0]);
               exit(0);
               break;
            default :
               show_message( "%1 is not a valid parameter with the CHKDSK\n"
                             "command when checking a hard disk.\n", 0, 543, 1, TYPE_STRING, rgArgv[iArg] );
               exit(543);
            }
         }
      else if (!strlen(pCD->szDrive))
         strncpy(pCD->szDrive, rgArgv[iArg], 2);
      }

#ifdef __OS2__
   CodepageConvInit(pCD->fAutoCheck);
#endif

   fToFile = OutputToFile((HANDLE)fileno(stdout));

   if (!strlen(pCD->szDrive))
      {
      query_current_disk(pCD->szDrive);
      }

   open_drive(pCD->szDrive, &hFile);

   if (pCD->fFix)
      {
      lock_drive(hFile);
      }

   pCD->hDisk = hFile;
   hDisk = hFile;

   get_drive_params(hFile, &dp);
   memcpy(&pCD->BootSect.bpb, &dp, sizeof(dp));

   rc = ReadSector(pCD, 0, 1, bSector);
   if (rc)
      {
      show_message("Error: Cannot read boot sector: %s\n", 0, 0, 1, get_error(rc));
      return rc;
      }
   memcpy(&pCD->BootSect, bSector, sizeof (BOOTSECT));

   pCD->bFatType = GetFatType(&pCD->BootSect);

   switch (pCD->bFatType)
      {
      case FAT_TYPE_FAT12:
         pCD->ulFatEof   = FAT12_EOF;
         pCD->ulFatEof2  = FAT12_EOF2;
         pCD->ulFatBad   = FAT12_BAD_CLUSTER;
         pCD->ulFatClean = FAT12_CLEAN_SHUTDOWN;
         break;

      case FAT_TYPE_FAT16:
         pCD->ulFatEof   = FAT16_EOF;
         pCD->ulFatEof2  = FAT16_EOF2;
         pCD->ulFatBad   = FAT16_BAD_CLUSTER;
         pCD->ulFatClean = FAT16_CLEAN_SHUTDOWN;
         break;

      case FAT_TYPE_FAT32:
         pCD->ulFatEof   = FAT32_EOF;
         pCD->ulFatEof2  = FAT32_EOF2;
         pCD->ulFatBad   = FAT32_BAD_CLUSTER;
         pCD->ulFatClean = FAT32_CLEAN_SHUTDOWN;
         break;

#ifdef EXFAT
      case FAT_TYPE_EXFAT:
         pCD->ulFatEof   = EXFAT_EOF;
         pCD->ulFatEof2  = EXFAT_EOF2;
         pCD->ulFatBad   = EXFAT_BAD_CLUSTER;
         pCD->ulFatClean = EXFAT_CLEAN_SHUTDOWN;
#endif
      }

   if (pCD->bFatType == FAT_TYPE_FAT32)
      {
      rc = ReadSector(pCD, pCD->BootSect.bpb.FSinfoSec, 1, bSector);
      if (rc)
         return rc;
      memcpy(&pCD->FSInfo, bSector + FSINFO_OFFSET, sizeof (BOOTFSINFO));
      }

   if (pCD->bFatType < FAT_TYPE_FAT32)
      {
      // create FAT32-type extended BPB for FAT12/FAT16
      pCD->BootSect.ulVolSerial = ((PBOOTSECT0)bSector)->ulVolSerial;
      memcpy(pCD->BootSect.VolumeLabel, ((PBOOTSECT0)bSector)->VolumeLabel, 11);
      memcpy(pCD->BootSect.FileSystem, ((PBOOTSECT0)bSector)->FileSystem, 8);

      if (!((PBOOTSECT0)bSector)->bpb.TotalSectors)
         pCD->BootSect.bpb.BigTotalSectors = ((PBOOTSECT0)bSector)->bpb.BigTotalSectors;
      else
         pCD->BootSect.bpb.BigTotalSectors = ((PBOOTSECT0)bSector)->bpb.TotalSectors;

      pCD->BootSect.bpb.BigSectorsPerFat = ((PBOOTSECT0)bSector)->bpb.SectorsPerFat;
      pCD->BootSect.bpb.ExtFlags = 0;
      // special value for root dir cluster (fake one)
      pCD->BootSect.bpb.RootDirStrtClus = 1;
      }

   if (pCD->bFatType)
      {
      // try checking only FAT32/FAT16/FAT12/exFAT drives
      rc = ChkDskMain(pCD);
      }
   else
      {
      // show help
      usage(rgArgv[0]);
      }

   if (pCD->fFix)
      {
      unlock_drive(hFile);
      }

   close_drive(hFile);

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
  rc = mem_alloc((void **)&stack, STACKSIZE);

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
  mem_free(stack, STACKSIZE);

  return 0;
}


#ifndef __DLL__
int main(int argc, char *argv[])
{
  return chkdsk(argc, argv, NULL);
}
#endif


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

ULONG ReadSector(PCDINFO pCD, ULONG ulSector, USHORT nSectors, PBYTE pbSector)
{
   return ReadSect(pCD->hDisk, ulSector, nSectors, pCD->BootSect.bpb.BytesPerSector, pbSector);
}

ULONG WriteSector(PCDINFO pCD, ULONG ulSector, USHORT nSectors, PBYTE pbSector)
{
   return WriteSect(pCD->hDisk, ulSector, nSectors, pCD->BootSect.bpb.BytesPerSector, pbSector);
}

ULONG WriteCluster(PCDINFO pCD, ULONG ulCluster, PVOID pbCluster)
{
   ULONG ulSector;
   ULONG rc;

   if (ulCluster < 2 || ulCluster >= pCD->ulTotalClusters + 2)
      return ERROR_SECTOR_NOT_FOUND;

   ulSector = pCD->ulStartOfData +
      (ulCluster - 2) * pCD->BootSect.bpb.SectorsPerCluster;

   rc = WriteSector(pCD, ulSector,
      pCD->BootSect.bpb.SectorsPerCluster, pbCluster);
   if (rc)
      return rc;

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
PSZ    pszType;

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
#if defined(__OS2__) && defined(__DLL__)
         /* Install 16-bit signal handler */
         rc = INIT16(0, 0);
#endif
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

   if (pCD->bFatType == FAT_TYPE_FAT32)
      {
      pCD->ulStartOfData    = pCD->BootSect.bpb.ReservedSectors +
        pCD->BootSect.bpb.BigSectorsPerFat * pCD->BootSect.bpb.NumberOfFATs;
      }
   else if (pCD->bFatType < FAT_TYPE_FAT32)
      {
      pCD->ulStartOfData    = pCD->BootSect.bpb.ReservedSectors +
        pCD->BootSect.bpb.SectorsPerFat * pCD->BootSect.bpb.NumberOfFATs +
        (pCD->BootSect.bpb.RootDirEntries * sizeof(DIRENTRY)) / pCD->BootSect.bpb.BytesPerSector;
      }

   pCD->ulClusterSize = pCD->BootSect.bpb.BytesPerSector * pCD->BootSect.bpb.SectorsPerCluster;
   pCD->ulTotalClusters = (pCD->BootSect.bpb.BigTotalSectors - pCD->ulStartOfData) / pCD->BootSect.bpb.SectorsPerCluster;

   if (pCD->bFatType < FAT_TYPE_FAT32)
      {
      // create FSInfo, calculate free space and next free cluster
      memset(&pCD->FSInfo, 0, sizeof(BOOTFSINFO));
      GetFreeSpace(pCD);
      }

   ulBytes = pCD->ulTotalClusters / 8 +
      (pCD->ulTotalClusters % 8 ? 1:0);
   usBlocks = (USHORT)(ulBytes / 4096 +
            (ulBytes % 4096 ? 1:0));

   pCD->pFatBits = calloc(usBlocks,4096);
   if (!pCD->pFatBits)
   //if (mem_alloc((void **)&pCD->pFatBits, ulBytes))
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

   if (pCD->bFatType >= FAT_TYPE_FAT16 && pCD->BootSect.bpb.MediaDescriptor != 0xF8)
      {
      show_message("The media descriptor is incorrect\n", 2400, 0, 0);
      pCD->ulErrorCount++;
      }

   pCD->fCleanOnBoot = GetDiskStatus(pCD);

   if (! pCD->fAutoCheck || ! pCD->fCleanOnBoot)
      {
      show_message("FAT32 version %s compiled on " __DATE__ "\n", 0, 0, 1, FAT32_VERSION);

      if( p > szString )
         show_message("The volume label is %1.\n", 0, 1375, 1, TYPE_STRING, szString);

      sprintf(szString, "%4.4X-%4.4X",
         HIUSHORT(pCD->BootSect.ulVolSerial), LOUSHORT(pCD->BootSect.ulVolSerial));
      show_message("The Volume Serial Number is %1.\n", 0, 1243, 1, TYPE_STRING, szString);

      switch (pCD->bFatType)
         {
         case FAT_TYPE_FAT12:
            pszType = "FAT12";
            break;

         case FAT_TYPE_FAT16:
            pszType = "FAT16";
            break;

         case FAT_TYPE_FAT32:
            pszType = "FAT32";
            break;

#ifdef EXFAT
         case FAT_TYPE_EXFAT:
            pszType = "exFAT";
#endif
         }

      show_message("The type of file system for the disk is %1.\n", 0, 1507, 1, TYPE_STRING, pszType);
      show_message("\n", 0, 0, 0);
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
      show_message("The copies of the FATs do not match\n"
                   "Please run CHKDSK under Windows to correct this problem\n", 2401, 0, 0);
      goto ChkDskMainExit;
      }
   rc = CheckFiles(pCD);
   rc = CheckFreeSpace(pCD);
   pCD->FSInfo.ulFreeClusters = pCD->ulFreeClusters;

   if (pCD->fFix)
      {
      ULONG ulFreeBlocks;
      ulFreeBlocks = GetFreeSpace(pCD);
      show_message("The correct free space is set to %lu allocation units\n", 2404, 0, 1,
                   ulFreeBlocks);
      }

   show_message("\n%1 bytes total disk space.\n", 0, 1361, 1,
      TYPE_DOUBLE, (DOUBLE)pCD->ulTotalClusters * pCD->ulClusterSize);
   if (pCD->ulBadClusters)
      show_message("%1 bytes in bad sectors.\n", 0, 1362, 1,
         TYPE_DOUBLE, (DOUBLE)pCD->ulBadClusters * pCD->ulClusterSize);
   show_message("%1 bytes in %2 hidden files.\n", 0, 1363, 2,
      TYPE_DOUBLE, (DOUBLE)pCD->ulHiddenClusters * pCD->ulClusterSize,
      TYPE_LONG, pCD->ulHiddenFiles);
   show_message("%1 bytes in %2 directories.\n", 0, 1364, 2,
      TYPE_DOUBLE, (DOUBLE)pCD->ulDirClusters * pCD->ulClusterSize,
      TYPE_LONG, pCD->ulTotalDirs);
   show_message("%1 bytes in extended attributes.\n", 0, 1819, 1,
      TYPE_DOUBLE, (DOUBLE)pCD->ulEAClusters * pCD->ulClusterSize);
   show_message("%1 bytes in %2 user files.\n", 0, 1365, 2,
      TYPE_DOUBLE, (DOUBLE)pCD->ulUserClusters * pCD->ulClusterSize,
      TYPE_LONG, pCD->ulUserFiles);

   if (pCD->ulRecoveredClusters)
      show_message("%1 bytes in %2 user files.\n", 0, 1365, 2,
         TYPE_DOUBLE, (DOUBLE)pCD->ulRecoveredClusters * pCD->ulClusterSize,
         TYPE_LONG, pCD->ulRecoveredFiles);

   if (pCD->ulLostClusters)
      show_message("%1 bytes disk space would be freed.\n", 0, 1359, 1,
         TYPE_DOUBLE, (DOUBLE)pCD->ulLostClusters * pCD->ulClusterSize);

   show_message("%1 bytes available on disk.\n", 0, 1368, 1,
      TYPE_DOUBLE, (DOUBLE)pCD->ulFreeClusters * pCD->ulClusterSize);

   show_message("\n", 0, 0, 0);

   show_message("%1 bytes in each allocation unit.\n", 0, 1304, 1,
      TYPE_LONG, (ULONG)pCD->ulClusterSize);

   show_message("%1 total allocation units.\n", 0, 1305, 1,
      TYPE_LONG, pCD->ulTotalClusters);

   show_message("%1 available allocation units on disk.\n", 0, 1306, 1,
      TYPE_LONG, pCD->ulFreeClusters);

   if (pCD->ulTotalChains > 0)
      {
      show_message("\n%u%% of the files and directories are fragmented.\n", 0, 0, 1,
         (USHORT)((pCD->ulFragmentedChains * 100) / pCD->ulTotalChains));
      }

ChkDskMainExit:
   if (pCD->ulErrorCount)
      {
      show_message("\n", 0, 0, 0);
      if (!pCD->fFix)
         show_message("\nErrors found.  The /F parameter was not specified.  Corrections will\n"
                      "not be written to disk.\n", 0, 1339, 0);
      else
         show_message("Errors may still exist on this volume. \n"
                      "Recommended action: Run CHKDSK under Windows\n", 0, 0, 0);
      }
   else if (pCD->fFix)
      {
      // set disk clean
      MarkDiskStatus(pCD, TRUE);
      }

   if (pCD->fFix)
      {
      // write chkdsk log
      if (! pCD->fCleanOnBoot || ! pCD->fAutoCheck )
         MakeFile(pCD, pCD->BootSect.bpb.RootDirStrtClus, "chkdsk.old", "chkdsk.log", logbuf, logbufpos);

      // remount disk for changes to take effect
      if (! pCD->fAutoCheck)
         remount_media(pCD->hDisk);
      }

   //mem_free((void *)pCD->pFatBits, ulBytes);
   free((void *)pCD->pFatBits);
   return rc;
}


ULONG MarkVolume(PCDINFO pCD, BOOL fClean)
{
ULONG rc;
ULONG dummy = 0;

   rc = MarkDiskStatus(pCD, fClean);

   return rc;
}

ULONG CheckFats(PCDINFO pCD)
{
PBYTE pSector;
USHORT nFat;
ULONG ulSector = 0;
ULONG  rc;
USHORT usPerc = 0xFFFF;
BOOL   fDiff;
ULONG  ulCluster = 0;
USHORT usIndex;
USHORT fRetco;
ULONG  ulRemained;
ULONG  ulReadPortion;

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

   pSector = calloc(pCD->BootSect.bpb.NumberOfFATs, BLOCK_SIZE);
   if (!pSector)
      return ERROR_NOT_ENOUGH_MEMORY;

   ulRemained = GetFatSize(pCD);

   fDiff = FALSE;
   ulCluster = 0L;
   fRetco = 0;
   //for (ulSector = 0; ulSector < pCD->BootSect.bpb.BigSectorsPerFat; ulSector+=32)
   for (ulSector = 0; ulSector < pCD->BootSect.bpb.BigSectorsPerFat; ulSector+=24)
      {
      USHORT usNew  = (USHORT)(ulSector * 100 / pCD->BootSect.bpb.BigSectorsPerFat);
      USHORT nSectors;

      if (!pCD->fPM && !fToFile && usNew != usPerc)
         {
         printf("\b\b\b\b%3u%%", usNew);
         usPerc = usNew;
         }

      nSectors = BLOCK_SIZE / pCD->BootSect.bpb.BytesPerSector;
      if ((ULONG)nSectors > pCD->BootSect.bpb.BigSectorsPerFat - ulSector)
         nSectors = (USHORT)(pCD->BootSect.bpb.BigSectorsPerFat - ulSector);

      if (ulRemained >= BLOCK_SIZE)
         {
         ulReadPortion = BLOCK_SIZE;
         ulRemained -= BLOCK_SIZE;
         }
      else
         {
         ulReadPortion = ulRemained;
         ulRemained = 0;
         }

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
                    ulReadPortion))
            fDiff = TRUE;
         }

      if (!ulSector)
         {
         ulCluster = 2;
         usIndex = 2;
         }
      else
         usIndex = 0;
      for (; ulCluster < pCD->ulTotalClusters + 2 && usIndex < GetFatEntriesPerBlock(pCD, ulReadPortion); usIndex++)
         {
         ULONG ulNextCluster = GetFatEntryEx(pCD, pSector, ulCluster, ulReadPortion);
         if (ulNextCluster >= pCD->ulTotalClusters + 2)
            {
            ULONG ulVal = ulNextCluster;
            if (!(ulVal >= pCD->ulFatBad && ulVal <= pCD->ulFatEof))
               {
               show_message("FAT Entry for cluster %lu contains an invalid value.\n", 2406, 0, 1,
                  ulCluster);
               fRetco = 1;
               }
            }
         ulCluster++;
         }
      }

   if (!pCD->fPM && !fToFile)
      printf("\b\b\b\b");
   if (fDiff)
      {
      show_message("\n", 0, 0, 0);
      show_message("File Allocation Table (FAT) is bad on drive %1.\n", 2407, 1374, 1, TYPE_STRING, pCD->szDrive);
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

   show_message("CHKDSK is searching for lost data.\n", 0, 564, 0);

   pCD->ulFreeClusters = 0;
   for (ulCluster = 0; ulCluster < pCD->ulTotalClusters; ulCluster++)
      {
      USHORT usNew  = (USHORT)(ulCluster * 100 / pCD->ulTotalClusters);
      ULONG ulNext = GetNextCluster(pCD, ulCluster + 2, FALSE, TRUE);

      if (!pCD->fPM && !fToFile && usNew != usPerc)
         {
         show_message("CHKDSK has searched %1% of the disk.", 0, 563, 1, TYPE_PERC, usNew);
         printf("\r");

         usPerc = usNew;
         }
      /* bad cluster ? */
      if (ulNext == pCD->ulFatBad)
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
               show_message("The system detected lost data on disk %1.\n", 2442, 562, 1, TYPE_STRING, pCD->szDrive);
               show_message("CHKDSK has searched %1% of the disk.", 0, 563, 1, TYPE_PERC, usNew);
               printf("\r");

               fMsg = TRUE;
               }
            RecoverChain(pCD, ulCluster+2);
            }
         }
      }

   if (!pCD->fPM && !fToFile)
      show_message("CHKDSK has searched %1% of the disk.", 0, 563, 1, TYPE_PERC, 100);
   show_message("\n", 0, 0, 0);

   if (pCD->usLostChains)
      {
      BYTE bChar;
      USHORT usIndex;
      ULONG rc;

      if (pCD->usLostChains >= MAX_LOST_CHAINS)
         show_message("Warning!  Not enough memory is available for CHKDSK\n"
                      "to recover all lost data.\n", 2441, 548, 0);

      if (!pCD->fAutoRecover)
         {
         show_message("%1 lost clusters found in %2 chains\n"
                      "These clusters and chains will be erased unless you convert\n"
                      "them to files.  Do you want to convert them to files(Y/N)? ", 0, 1356, 2,
            TYPE_LONG2, pCD->ulLostClusters,
            TYPE_LONG2, (ULONG)pCD->usLostChains);
         show_message("%lu lost clusters found in %lu chains\n"
                      "These clusters and chains will be erased unless you convert\n"
                      "them to files.  Do you want to convert them to files(Y/N)? ", 2440, 0, 2,
            pCD->ulLostClusters,
            (ULONG)pCD->usLostChains);
         }
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
               rc = DeleteFatChain(pCD, pCD->rgulLost[usIndex]);
               if (rc == FALSE)
                  {
                  show_message("CHKDSK was unable to delete a lost chain\n", 2409, 0, 0);
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
      show_message("An invalid cluster number %8.8lX was found\n", 2410, 0, 1, ulCluster);
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
DIRENTRY * pDir;
DIRENTRY * pEnd;
DIRENTRY * pPrevDir;
PBYTE pbCluster;
PBYTE pbPath;
ULONG ulCluster;
ULONG ulClusters;
ULONG ulBytesNeeded;
BYTE * p;
BYTE bCheckSum, bCheck;
ULONG ulClustersNeeded;
ULONG ulClustersUsed;
ULONG ulEntries;
PBYTE pEA;
ULONG rc;
ULONG dummy = 0;
UCHAR fModified = FALSE;
ULONG  ulSector;
USHORT usSectorsRead;

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
      show_message("\n\nDirectory of %s (%lu clusters)\n\n", 2438, 0, 2, pszPath, ulClusters);

   ulBytesNeeded = (ULONG)pCD->BootSect.bpb.SectorsPerCluster * (ULONG)pCD->BootSect.bpb.BytesPerSector * ulClusters;
   pbCluster = calloc(ulClusters, pCD->BootSect.bpb.SectorsPerCluster * pCD->BootSect.bpb.BytesPerSector);
   if (!pbCluster)
      {
      show_message("ERROR:Directory %s is too large ! (Not enough memory!)\n", 2412, 0, 1, pszPath);
      return ERROR_NOT_ENOUGH_MEMORY;
      }

   //memset(pbCluster, 0, pCD->BootSect.bpb.SectorsPerCluster * pCD->BootSect.bpb.BytesPerSector);
   memset(pbCluster, 0, ulClusters * pCD->BootSect.bpb.SectorsPerCluster * pCD->BootSect.bpb.BytesPerSector);
   ulCluster = ulDirCluster;
   p = pbCluster;

   if (ulDirCluster == 1)
      {
      // root directory starting sector for FAT12/FAT16 case
      ulSector = pCD->BootSect.bpb.ReservedSectors +
         pCD->BootSect.bpb.SectorsPerFat * pCD->BootSect.bpb.NumberOfFATs;
      usSectorsRead = 0;
      }

   while (ulCluster != pCD->ulFatEof)
      {
      if (ulDirCluster == 1)
         {
         // reading root directory on FAT12/FAT16
         ReadSector(pCD, ulSector, pCD->BootSect.bpb.SectorsPerCluster, p);
         // reading the root directory in case of FAT12/FAT16
         ulSector += pCD->BootSect.bpb.SectorsPerCluster;
         usSectorsRead += pCD->BootSect.bpb.SectorsPerCluster;
         if (usSectorsRead * pCD->BootSect.bpb.BytesPerSector >=
            pCD->BootSect.bpb.RootDirEntries * sizeof(DIRENTRY))
            // root directory ended
            ulCluster = 0;
         }
      else
         {
         ReadCluster(pCD, ulCluster, p);
         ulCluster = GetNextCluster(pCD, ulCluster, FALSE, FALSE);
         }
      if (!ulCluster)
         ulCluster = pCD->ulFatEof;
      p += pCD->BootSect.bpb.SectorsPerCluster * pCD->BootSect.bpb.BytesPerSector;
      }

   memset(szLongName, 0, sizeof(szLongName));
   pDir = (DIRENTRY *)pbCluster;

   if (ulDirCluster == 1)
      pEnd = pDir + pCD->BootSect.bpb.RootDirEntries;
   else
      pEnd = (PDIRENTRY)p;

   pEnd--;

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
               show_message("The longname does not belong to %s.\n", 2414, 0, 1, MakeName(pDir, szShortName, sizeof(szShortName)));
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
                show_message("%s has old EA mark byte(0x%0X)\n", 2415, 0, 2, pbPath, pDir->fEAS );
                if (pCD->fFix)
                {
                     strcpy(Mark.szFileName, pbPath);
                     Mark.fEAS = ( BYTE )( pDir->fEAS == FILE_HAS_OLD_EAS ? FILE_HAS_EAS : FILE_HAS_CRITICAL_EAS );
                     rc = GetSetFileEAS(pCD, FAT32_SETEAS, (PMARKFILEEASBUF)&Mark);
                     if (!rc)
                        show_message("This has been corrected\n", 2439, 0, 0);
                     else
                        show_message("SYS%4.4u: Unable to correct problem\n", 2416, 0, 1, rc);
                }
            }

#if 1
            if( f32Parms.fEAS && pDir->fEAS && !HAS_WINNT_EXT( pDir->fEAS ) && !HAS_EAS( pDir->fEAS ))
                show_message("%s has unknown EA mark byte(0x%0X)\n", 2417, 0, 2, pbPath, pDir->fEAS );
#endif

#if 0
            if( f32Parms.fEAS && HAS_EAS( pDir->fEAS ))
                show_message("%s has EA byte(0x%0X)\n", 2418, 0, 2, pbPath, pDir->fEAS );
#endif

            if (f32Parms.fEAS && HAS_EAS( pDir->fEAS ))
               {
               ULONG ulDirCluster, ulFileCluster;
               DIRENTRY DirEntry;
               PSZ pszFile;

               strcpy(Mark.szFileName, pbPath);
               strcat(Mark.szFileName, EA_EXTENTION);
               rc = NO_ERROR;
               ulDirCluster = FindDirCluster(pCD, Mark.szFileName, -1, 0xffff, &pszFile);

               if (ulDirCluster == pCD->ulFatEof)
                  rc = ERROR_PATH_NOT_FOUND;
               else
                  {
                  ulFileCluster = FindPathCluster(pCD, ulDirCluster, pszFile, &DirEntry, NULL);

                  if (ulFileCluster == pCD->ulFatEof)
                     rc = ERROR_FILE_NOT_FOUND;
                  }
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
                     rc = GetSetFileEAS(pCD, FAT32_SETEAS, (PMARKFILEEASBUF)&Mark);
                     if (!rc)
                        show_message("This has been corrected\n", 2439, 0, 0);
                     else
                        show_message("SYS%4.4u: Unable to correct problem\n", 2416, 0, 1, rc);
                     }
                  }
               else if (!DirEntry.ulFileSize)
                  {
                  show_message("%s is marked having EAs, but the EA file (%s) is empty\n", 0, 0, 2, pbPath, Mark.szFileName);
                  show_message("File marked having EAs, but the EA file (%s) is empty\n", 2420, 0, 1, Mark.szFileName);
                  if (pCD->fFix)
                     {
                     unlink(Mark.szFileName);
                     strcpy(Mark.szFileName, pbPath);
                     Mark.fEAS = FILE_HAS_NO_EAS;
                     rc = GetSetFileEAS(pCD, FAT32_SETEAS, (PMARKFILEEASBUF)&Mark);
                     if (!rc)
                        show_message("This has been corrected\n", 2439, 0, 0);
                     else
                        show_message("SYS%4.4u: Unable to correct problem\n", 2416, 0, 1, rc);
                     }
                  }
               }

            if (!(pDir->bAttr & FILE_DIRECTORY))
               {
               ulClustersNeeded = pDir->ulFileSize / pCD->ulClusterSize +
                  (pDir->ulFileSize % pCD->ulClusterSize ? 1:0);
               ulClustersUsed = GetClusterCount(pCD,((ULONG)pDir->wClusterHigh * 0x10000 + pDir->wCluster) & pCD->ulFatEof, pbPath);
               pEA = strstr(pbPath, EA_EXTENTION);
               if (f32Parms.fEAS && pEA && pDir->ulFileSize)
                  {
                  ULONG rc;

                  pCD->ulEAClusters += ulClustersUsed;

                  memset(&Mark, 0, sizeof(Mark));
                  memcpy(Mark.szFileName, pbPath, pEA - pbPath);

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
                           ulDirCluster = FindDirCluster(pCD, pbPath, -1, 0xffff, &pszFile);

                           rc = NO_ERROR;
                           if (ulDirCluster == pCD->ulFatEof)
                              rc = ERROR_PATH_NOT_FOUND;
                           else
                              {
                              ulFileCluster = FindPathCluster(pCD, ulDirCluster, pszFile, &DirEntry, NULL);

                              if (ulFileCluster == pCD->ulFatEof)
                                 rc = ERROR_FILE_NOT_FOUND;
                              }
                           
                           memcpy(&DirNew, &DirEntry, sizeof(DIRENTRY));
                           DirNew.bAttr = FILE_NORMAL;
                           rc = ModifyDirectory(pCD, ulDirCluster, MODIFY_DIR_UPDATE,
                                                &DirEntry, &DirNew, NULL);
                           }
                        if (!rc)
                           {
                           ulDstFileCluster = FindPathCluster(pCD, ulDirCluster, Mark.szFileName, &DstDirEntry, NULL);

                           rc = NO_ERROR;
                           if (ulDstFileCluster == pCD->ulFatEof)
                              rc = ERROR_FILE_NOT_FOUND;

                           if (ulDstFileCluster == ulFileCluster)
                              rc = ERROR_ACCESS_DENIED;

                           rc = ModifyDirectory(pCD, ulDirCluster, MODIFY_DIR_RENAME,
                                                &DirEntry, &DirNew, Mark.szFileName);
                           }
                        if (!rc)
                           show_message("This attribute has been converted to a file \n(%s)\n", 2422, 0, 1, Mark.szFileName);
                        else
                           {
                           show_message("Cannot convert %s. SYS%4.4u\n", 2423, 0, 2,
                              pbPath, rc);
                           pCD->ulErrorCount++;
                           }
                        MarkVolume(pCD, pCD->fCleanOnBoot);
                        }
                     }
                  else if (rc)
                     show_message("Retrieving EA flag for %s. SYS%4.4u occured\n", 2424, 0, 2, Mark.szFileName, rc);
                  else
                     {
                     if ( !HAS_EAS( Mark.fEAS ))
                        {
                        show_message("EAs detected for %s, but it is not marked having EAs\n", 2425, 0, 1, Mark.szFileName);
                        if (pCD->fFix)
                           {
                           Mark.fEAS = FILE_HAS_EAS;
                           rc = GetSetFileEAS(pCD, FAT32_SETEAS, (PMARKFILEEASBUF)&Mark);
                           if (!rc)
                              show_message("This has been corrected\n", 2439, 0, 0);
                           else
                              show_message("SYS%4.4u: Unable to correct problem\n", 2416, 0, 1, rc);
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
                     show_message("File allocation error detected for %s\n", 2426, 0, 1,
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
                     rc = SetFileSize(pCD, (PFILESIZEDATA)&fs);
                     strcpy( strrchr( fs.szFileName, '\\' ) + 1, szLongName );
                     if (rc)
                        {
                        show_message("File allocation error detected for %s\n", 2426, 0, 1,
                           fs.szFileName);
                        show_message("CHKDSK was unable to correct the filesize. SYS%4.4u\n", 2428, 0, 1, rc);
                        pCD->ulErrorCount++;
                        }
                     else
                        show_message("CHKDSK corrected an allocation error for the file %1.\n", 2444, 560, 1,
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
               ulCluster = ((ULONG)pDir->wClusterHigh * 0x10000 + pDir->wCluster) & pCD->ulFatEof;
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
                  show_message("Non matching longname for %s\n", 2429, 0, 1,
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
                     ((ULONG)pDir->wClusterHigh * 0x10000 + pDir->wCluster) & pCD->ulFatEof,
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
      if (ulCluster == 1)
         {
         // root directory starting sector for FAT12/FAT16 case
         ulSector = pCD->BootSect.bpb.ReservedSectors +
            pCD->BootSect.bpb.SectorsPerFat * pCD->BootSect.bpb.NumberOfFATs;
         usSectorsRead = 0;
         }
      p = pbCluster;
      while (ulCluster != pCD->ulFatEof)
         {
         if (ulCluster == 1)
            {
            // reading root directory on FAT12/FAT16
            WriteSector(pCD, ulSector, pCD->BootSect.bpb.SectorsPerCluster, p);
            // reading the root directory in case of FAT12/FAT16
            ulSector += pCD->BootSect.bpb.SectorsPerCluster;
            usSectorsRead += pCD->BootSect.bpb.SectorsPerCluster;
            if (usSectorsRead * pCD->BootSect.bpb.BytesPerSector >=
               pCD->BootSect.bpb.RootDirEntries * sizeof(DIRENTRY))
               // root directory ended
               ulCluster = 0;
            }
         else
            {
            WriteCluster(pCD, ulCluster, p);
            ulCluster = GetNextCluster(pCD, ulCluster, FALSE, FALSE);
            }
         if (!ulCluster)
            ulCluster = pCD->ulFatEof;
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

   if (ulCluster == 1)
      {
      // special case: root directory in FAT12/FAT16 case
      ulCount = pCD->BootSect.bpb.RootDirEntries * sizeof(DIRENTRY) /
         (pCD->BootSect.bpb.SectorsPerCluster * pCD->BootSect.bpb.BytesPerSector);
      return (pCD->BootSect.bpb.RootDirEntries * sizeof(DIRENTRY) %
         (pCD->BootSect.bpb.SectorsPerCluster * pCD->BootSect.bpb.BytesPerSector)) ?
         ulCount + 1 :
         ulCount;
      }

   if (ulCluster  > pCD->ulTotalClusters + 2)
      {
      show_message("%s: Invalid start of cluster chain %lX found\n", 2432, 0, 2,
         pszFile, ulCluster);
      return 0;
      }

   while (ulCluster != pCD->ulFatEof)
      {
      ulNextCluster = GetNextCluster(pCD, ulCluster, FALSE, FALSE);
      if (!MarkCluster(pCD, ulCluster, pszFile))
         return ulCount;
      ulCount++;

      if (!ulNextCluster)
         {
         if (pCD->fFix)
            {
            show_message("CHKDSK found an improperly terminated cluster chain for %s ", 2433, 0, 1, pszFile);
            if (SetNextCluster(pCD, ulCluster, pCD->ulFatEof) != pCD->ulFatEof)
               {
               show_message(", but was unable to fix it\n", 0, 0, 0);
               pCD->ulErrorCount++;
               }
            else
               show_message(" and corrected the problem\n", 2434, 0, 0);
            }
         else
            {
            show_message("A bad terminated cluster chain was found for %s\n", 2435, 0, 1, pszFile);
            pCD->ulErrorCount++;
            }
         ulNextCluster = pCD->ulFatEof;
         }

      if (ulNextCluster != pCD->ulFatEof && ulNextCluster != ulCluster + 1)
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
      show_message("%1 is cross-linked on cluster %2\n", 0, 1343, 2,
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
ULONG  ulSector;
USHORT usSectorsRead;
ULONG  ulDirEntries = 0;

   pDir = NULL;

   pDirStart = malloc(pCD->ulClusterSize);
   if (!pDirStart)
      return ERROR_NOT_ENOUGH_MEMORY;

   fFound = FALSE;
   ulCluster = pCD->BootSect.bpb.RootDirStrtClus;
   if (ulCluster == 1)
      {
      // root directory starting sector
      ulSector = pCD->BootSect.bpb.ReservedSectors +
        pCD->BootSect.bpb.SectorsPerFat * pCD->BootSect.bpb.NumberOfFATs;
      usSectorsRead = 0;
      }
   while (!fFound && ulCluster != pCD->ulFatEof)
      {
      if (ulCluster == 1)
         // reading root directory on FAT12/FAT16
         ReadSector(pCD, ulSector, pCD->BootSect.bpb.SectorsPerCluster, (void *)pDirStart);
      else
         ReadCluster(pCD, ulCluster, (void *)pDirStart);
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
         ulDirEntries++;
         if (ulCluster == 1 && ulDirEntries > pCD->BootSect.bpb.RootDirEntries)
            break;
         }
      if (!fFound)
         {
         if (ulCluster == 1)
            {
            // reading the root directory in case of FAT12/FAT16
            ulSector += pCD->BootSect.bpb.SectorsPerCluster;
            usSectorsRead += pCD->BootSect.bpb.SectorsPerCluster;
            if (usSectorsRead * pCD->BootSect.bpb.BytesPerSector >
                pCD->BootSect.bpb.RootDirEntries * sizeof(DIRENTRY))
               // root directory ended
               ulCluster = 0;
            }
         else
            ulCluster = GetNextCluster(pCD, ulCluster, FALSE, FALSE);
         if (!ulCluster)
            ulCluster = pCD->ulFatEof;
         }
      if (ulCluster == 1 && ulDirEntries > pCD->BootSect.bpb.RootDirEntries)
         break;
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
   Translate2OS2((PUSHORT)uniName, szLongName, ulDataSize);

   wNameSize = strlen( szLongName );

   if (strlen(pszName) + wNameSize > wMax)
      return FALSE;

   memmove(pszName + wNameSize, pszName, strlen(pszName) + 1);
   memcpy(pszName, szLongName, wNameSize);
   return TRUE;
}



ULONG GetNextCluster(PCDINFO pCD, ULONG ulCluster, BOOL fNoFatChain, BOOL fAllowBad)
{
ULONG  ulSector = 0;
ULONG  ulRet = 0;

   ulSector = GetFatEntrySec(pCD, ulCluster);
   if (!ReadFATSector(pCD, ulSector))
      return pCD->ulFatEof;

   ulRet = GetFatEntry(pCD, ulCluster);

   if (ulRet >= pCD->ulFatEof2 && ulRet <= pCD->ulFatEof)
      return pCD->ulFatEof;

   if (ulRet == pCD->ulFatBad && fAllowBad)
      return ulRet;

   if (ulRet >= pCD->ulTotalClusters  + 2)
      {
      show_message("Error: Next cluster for %lu = %8.8lX\n", 0, 0, 2,
         ulCluster, ulRet);
      return pCD->ulFatEof;
      }

   return ulRet;
}

BOOL ReadFATSector(PCDINFO pCD, ULONG ulSector)
{
ULONG  ulSec = ulSector * 3;
ULONG rc;

   // read multiples of three sectors,
   // to fit a whole number of FAT12 entries
   // (ulSector is indeed a number of 3*512
   // bytes blocks, so, it is needed to multiply by 3)

   if (pCD->ulCurFATSector == ulSector)
      return TRUE;

   rc = ReadSector(pCD, pCD->ulActiveFatStart + ulSec, 3,
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
            ulNext = GetNextCluster(pCD, ulNext, FALSE, FALSE);
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
   while (ulStart && ulStart != pCD->ulFatEof)
      {
      if (ulStart == ulCluster)
         return TRUE;
      ulStart = GetNextCluster(pCD, ulStart, FALSE, FALSE);
      }
   return FALSE;
}

BOOL LostToFile(PCDINFO pCD, ULONG ulCluster, ULONG ulSize)
{
BYTE   szRecovered[CCHMAXPATH];
ULONG  rc, dummy1 = 0, dummy2 = 0;

   memset(szRecovered, 0, sizeof(szRecovered));
   rc = RecoverChain2(pCD, ulCluster, szRecovered, sizeof(szRecovered));
   if (rc)
      {
      pCD->ulErrorCount++;
      show_message("CHKDSK was unable to recover a lost chain. SYS%4.4u\n", 2437, 0, 1, rc);
      return FALSE;
      }
   pCD->ulRecoveredClusters += ulSize;
   pCD->ulRecoveredFiles++;

   show_message("CHKDSK placed recovered data in file %1.\n", 2443, 574, 1, TYPE_STRING, szRecovered);
   return TRUE;
}

UCHAR GetFatType(PBOOTSECT pSect)
{
   /*
    *  check for FAT32 according to the Microsoft FAT32 specification
    */
   PBPB  pbpb;
   ULONG FATSz;
   ULONG TotSec;
   ULONG RootDirSectors;
   ULONG NonDataSec;
   ULONG DataSec;
   ULONG CountOfClusters;

#ifdef EXFAT
   //if (!memcmp(pSect->oemID, "EXFAT   ", 8))
   //   {
   //   return FAT_TYPE_EXFAT;
   //   } /* endif */
#endif

   if (!pSect)
      {
      return FAT_TYPE_NONE;
      } /* endif */

   pbpb = &pSect->bpb;

   if (!pbpb->BytesPerSector)
      {
      return FAT_TYPE_NONE;
      }

   //if (pbpb->BytesPerSector != SECTOR_SIZE)
   //   {
   //   return FAT_TYPE_NONE;
   //   }

   if (! pbpb->SectorsPerCluster)
      {
      // this could be the case with a JFS partition, for example
      return FAT_TYPE_NONE;
      }

   if(( ULONG )pbpb->BytesPerSector * pbpb->SectorsPerCluster > MAX_CLUSTER_SIZE )
      {
      return FAT_TYPE_NONE;
      }

   RootDirSectors = ((pbpb->RootDirEntries * 32UL) + (pbpb->BytesPerSector-1UL)) / pbpb->BytesPerSector;

   if (pbpb->SectorsPerFat)
      {
      FATSz = pbpb->SectorsPerFat;
      }
   else
      {
      FATSz = pbpb->BigSectorsPerFat;
      } /* endif */

   if (pbpb->TotalSectors)
      {
      TotSec = pbpb->TotalSectors;
      }
   else
      {
      TotSec = pbpb->BigTotalSectors;
      } /* endif */

   NonDataSec = pbpb->ReservedSectors
                   +  (pbpb->NumberOfFATs * FATSz)
                   +  RootDirSectors;

   if (TotSec < NonDataSec)
      {
      return FAT_TYPE_NONE;
      } /* endif */

   DataSec = TotSec - NonDataSec;
   CountOfClusters = DataSec / pbpb->SectorsPerCluster;

   if ((CountOfClusters >= 65525UL) && !memcmp(pSect->FileSystem, "FAT32   ", 8))
      {
      return FAT_TYPE_FAT32;
      } /* endif */

   if (!memcmp(((PBOOTSECT0)pSect)->FileSystem, "FAT12   ", 8))
      {
      return FAT_TYPE_FAT12;
      } /* endif */

   if (!memcmp(((PBOOTSECT0)pSect)->FileSystem, "FAT16   ", 8))
      {
      return FAT_TYPE_FAT16;
      } /* endif */

   if (!memcmp(((PBOOTSECT0)pSect)->FileSystem, "FAT", 3))
      {
      ULONG TotClus = (TotSec - NonDataSec) / pbpb->SectorsPerCluster;

      if (TotClus < 0xff6)
         return FAT_TYPE_FAT12;

      return FAT_TYPE_FAT16;
      }

   return FAT_TYPE_NONE;
}

/******************************************************************
*
******************************************************************/
ULONG GetFatEntrySec(PCDINFO pCD, ULONG ulCluster)
{
   return GetFatEntryBlock(pCD, ulCluster, pCD->BootSect.bpb.BytesPerSector * 3); // in three sector blocks
}

/******************************************************************
*
******************************************************************/
ULONG GetFatEntryBlock(PCDINFO pCD, ULONG ulCluster, USHORT usBlockSize)
{
ULONG  ulSector;

   ulCluster &= pCD->ulFatEof;

   switch (pCD->bFatType)
      {
      case FAT_TYPE_FAT12:
         ulSector = ((ulCluster * 3) / 2) / usBlockSize;
         break;

      case FAT_TYPE_FAT16:
         ulSector = (ulCluster * 2) / usBlockSize;
         break;

      case FAT_TYPE_FAT32:
#ifdef EXFAT
      case FAT_TYPE_EXFAT:
#endif
         ulSector = (ulCluster * 4) / usBlockSize;
      }

   return ulSector;
}

/******************************************************************
*
******************************************************************/
ULONG GetFatEntriesPerSec(PCDINFO pCD)
{
   return GetFatEntriesPerBlock(pCD, pCD->BootSect.bpb.BytesPerSector * 3);
}

/******************************************************************
*
******************************************************************/
ULONG GetFatEntriesPerBlock(PCDINFO pCD, USHORT usBlockSize)
{
   switch (pCD->bFatType)
      {
      case FAT_TYPE_FAT12:
         return (ULONG)usBlockSize * 2 / 3;

      case FAT_TYPE_FAT16:
         return (ULONG)usBlockSize / 2;

      case FAT_TYPE_FAT32:
#ifdef EXFAT
      case FAT_TYPE_EXFAT:
#endif
         return (ULONG)usBlockSize / 4;
      }

   return 0;
}

/******************************************************************
*
******************************************************************/
ULONG GetFatSize(PCDINFO pCD)
{
ULONG ulFatSize = pCD->ulTotalClusters;

   switch (pCD->bFatType)
      {
      case FAT_TYPE_FAT12:
         ulFatSize *= 3;
         ulFatSize /= 2;
         break;

      case FAT_TYPE_FAT16:
         ulFatSize *= 2;
         break;

      case FAT_TYPE_FAT32:
#ifdef EXFAT
      case FAT_TYPE_EXFAT:
#endif
         ulFatSize *= 4;
      }

   return ulFatSize;
}

/******************************************************************
*
******************************************************************/
ULONG GetFatEntry(PCDINFO pCD, ULONG ulCluster)
{
   return GetFatEntryEx(pCD, pCD->pbFATSector, ulCluster, pCD->BootSect.bpb.BytesPerSector * 3);
}

/******************************************************************
*
******************************************************************/
ULONG GetFatEntryEx(PCDINFO pCD, PBYTE pFatStart, ULONG ulCluster, USHORT usBlockSize)
{
   ulCluster &= pCD->ulFatEof;

   switch (pCD->bFatType)
      {
      case FAT_TYPE_FAT12:
         {
         ULONG   ulOffset;
         PUSHORT pusCluster;
         ulOffset = (ulCluster * 3) / 2;

         if (usBlockSize)
            ulOffset %= usBlockSize;

         pusCluster = (PUSHORT)((PBYTE)pFatStart + ulOffset);
         ulCluster = ( ((ulCluster * 3) % 2) ?
            *pusCluster >> 4 : // odd
            *pusCluster )      // even
            & pCD->ulFatEof;
         break;
         }

      case FAT_TYPE_FAT16:
         {
         ULONG   ulOffset;
         PUSHORT pusCluster;
         ulOffset = ulCluster * 2;

         if (usBlockSize)
            ulOffset %= usBlockSize;

         pusCluster = (PUSHORT)((PBYTE)pFatStart + ulOffset);
         ulCluster = *pusCluster & pCD->ulFatEof;
         break;
         }

      case FAT_TYPE_FAT32:
#ifdef EXFAT
      case FAT_TYPE_EXFAT:
#endif
         {
         ULONG   ulOffset;
         PULONG pulCluster;
         ulOffset = ulCluster * 4;

         if (usBlockSize)
            ulOffset %= usBlockSize;

         pulCluster = (PULONG)((PBYTE)pFatStart + ulOffset);
         ulCluster = *pulCluster & pCD->ulFatEof;
         }
      }

   return ulCluster;
}

/******************************************************************
*
******************************************************************/
void SetFatEntry(PCDINFO pCD, ULONG ulCluster, ULONG ulValue)
{
   SetFatEntryEx(pCD, pCD->pbFATSector, ulCluster, ulValue, pCD->BootSect.bpb.BytesPerSector * 3);
}

/******************************************************************
*
******************************************************************/
void SetFatEntryEx(PCDINFO pCD, PBYTE pFatStart, ULONG ulCluster, ULONG ulValue, USHORT usBlockSize)
{
   ulCluster &= pCD->ulFatEof;
   ulValue   &= pCD->ulFatEof;

   switch (pCD->bFatType)
      {
      case FAT_TYPE_FAT12:
         {
         ULONG   ulOffset;
         PUSHORT pusCluster;
         USHORT  usPrevValue;
         ULONG   ulNewValue;
         ulOffset = (ulCluster * 3) / 2;

         if (usBlockSize)
            ulOffset %= usBlockSize;

         pusCluster = (PUSHORT)((PBYTE)pFatStart + ulOffset);
         usPrevValue = *pusCluster;
         ulNewValue = ((ulCluster * 3) % 2)  ?
            (usPrevValue & 0xf) | (ulValue << 4) : // odd
            (usPrevValue & 0xf000) | (ulValue);    // even
         *pusCluster = ulNewValue;
         break;
         }

      case FAT_TYPE_FAT16:
         {
         ULONG   ulOffset;
         PUSHORT pusCluster;
         ulOffset = ulCluster * 2;

         if (usBlockSize)
            ulOffset %= usBlockSize;

         pusCluster = (PUSHORT)((PBYTE)pFatStart + ulOffset);
         *pusCluster = ulValue;
         break;
         }

      case FAT_TYPE_FAT32:
#ifdef EXFAT
      case FAT_TYPE_EXFAT:
#endif
         {
         ULONG   ulOffset;
         PULONG  pulCluster;
         ulOffset = ulCluster * 4;

         if (usBlockSize)
            ulOffset %= usBlockSize;

         pulCluster = (PULONG)((PBYTE)pFatStart + ulOffset);
         *pulCluster = ulValue;
         }
      }
}
