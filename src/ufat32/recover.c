#include <string.h>
#include <malloc.h>
#include <stdio.h>
#include <ctype.h>

#include "portable.h"
#include "fat32c.h"

BOOL  GetDiskStatus(PCDINFO pCD);
ULONG ReadSect(HANDLE hf, ULONG ulSector, USHORT nSectors, USHORT BytesPerSect, PBYTE pbSector);
ULONG FindPathCluster(PCDINFO pCD, ULONG ulCluster, PSZ pszPath, PDIRENTRY pDirEntry, PSZ pszFullName);
USHORT RecoverChain2(PCDINFO pCD, ULONG ulCluster, PBYTE pData, USHORT cbData);
void CodepageConvInit(BOOL fSilent);

BOOL DoRecover(PCDINFO pCD, char *pszFilename);
int recover_thread(int argc, char *argv[]);
void remount_media (HANDLE hDevice);

#define STACKSIZE 0x10000

int recover_thread(int argc, char *argv[])
{
   PCDINFO pCD;
   HANDLE  hFile;
   ULONG  ulAction;
   BYTE   bSector[512];
   char   szTarget[0x8000];
   ULONG  ulBytes;
   USHORT usBlocks;
   APIRET  rc;
   int i;

   show_message("FAT32 version %s compiled on " __DATE__ "\n", 0, 0, 1, FAT32_VERSION);

#ifdef __OS2__
   CodepageConvInit(0);
#endif

   pCD = (PCDINFO)malloc(sizeof(CDINFO));

   if (!pCD)
      return ERROR_NOT_ENOUGH_MEMORY;

   memset(pCD, 0, sizeof (CDINFO));

   if (!strlen(pCD->szDrive))
      strncpy(pCD->szDrive, argv[1], 2);

   if (!strlen(pCD->szDrive))
      {
      query_current_disk(pCD->szDrive);
      }

   pCD->szDrive[0] = toupper(pCD->szDrive[0]);

   open_drive(pCD->szDrive, &hFile);

   pCD->fCleanOnBoot = GetDiskStatus(pCD);

   if (pCD->fAutoRecover && pCD->fCleanOnBoot)
      pCD->fAutoRecover = FALSE;

   lock_drive(hFile);
   pCD->hDisk = hFile;

   rc = ReadSect(hFile, 0, 1, SECTOR_SIZE, bSector);
   if (rc)
      {
      show_message("Error: Cannot read boot sector: %s\n", 0, 0, 1, get_error(rc));
      return rc;
      }
   memcpy(&pCD->BootSect, bSector, sizeof (BOOTSECT));

   rc = ReadSect(hFile, pCD->BootSect.bpb.FSinfoSec, 1, SECTOR_SIZE, bSector);
   if (rc)
      {
      show_message("Error: Cannot read FSInfo sector\n%s\n", 0, 0, 1, get_error(rc));
      return rc;
      }

   /*
      Some preparations
   */
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

   pCD->pFatBits = calloc(usBlocks,4096);
   if (!pCD->pFatBits)
      {
      show_message("Not enough memory for FATBITS\n", 0, 0, 0);
      return ERROR_NOT_ENOUGH_MEMORY;
      }

   memset(szTarget, 0, sizeof(szTarget));

   for (i = 1; i < argc; i++)
      {
      //DosEditName(1, argv[i], "*", szTarget, sizeof(szTarget));
      //show_message("%s\n", 0, 0, 1, szTarget);
      DoRecover(pCD, argv[i]);
      }   

   remount_media(hFile);
   unlock_drive(hFile);
   close_drive(hFile);
   free(pCD);

   return 0;
}

int recover(int argc, char *argv[], char *envp[])
{
  char *stack;
  APIRET rc;

  // Here we're switching stack, because the original
  // recover.com stack is too small.

  // allocate stack
  rc = mem_alloc((void **)&stack, STACKSIZE);

  if (rc)
    return rc;

  // call recover_thread on new stack
  _asm {
    mov eax, esp
    mov edx, stack
    mov ecx, argv
    add edx, STACKSIZE - 4
    mov esp, edx
    push eax
    push ecx
    mov ecx, argc
    push ecx
    call recover_thread
    add esp, 8
    pop esp
  }

  // deallocate new stack
  mem_free(stack, STACKSIZE);

  return 0;
}

BOOL DoRecover(PCDINFO pCD, char *pszFilename)
{
   ULONG ulCluster = pCD->BootSect.bpb.RootDirStrtClus;
   BYTE  szRecovered[CCHMAXPATH];
   DIRENTRY DirEntry;
   APIRET rc;

   ulCluster = FindPathCluster(pCD, ulCluster, pszFilename, &DirEntry, NULL);

   if (ulCluster == FAT_EOF)
      return FALSE;

   memset(szRecovered, 0, sizeof(szRecovered));
   rc = RecoverChain2(pCD, ulCluster, szRecovered, sizeof(szRecovered));

   if (rc)
      {
      pCD->ulErrorCount++;
      show_message("RECOVER was unable to recover a lost chain. SYS%4.4u\n", 0, 0, 1, rc);
      return FALSE;
      }

   return TRUE;
}
