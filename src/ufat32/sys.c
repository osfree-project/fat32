#include <stdio.h>
#include <string.h>
#include <direct.h>
#include <process.h>
#include <malloc.h>

#include "portable.h"
#include "fat32c.h"
#include "sys.h"

void open_drive (char *path, HANDLE *hDevice);
void close_drive(HANDLE hDevice);
void lock_drive(HANDLE hDevice);
void unlock_drive(HANDLE hDevice);
void begin_format (HANDLE hDevice);
void remount_media (HANDLE hDevice);

ULONG ReadSect(HANDLE hf, ULONG ulSector, USHORT nSectors, USHORT BytesPerSector, PBYTE pbSector);
ULONG WriteSect(HANDLE hf, ULONG ulSector, USHORT nSectors, USHORT BytesPerSector, PBYTE pbSector);


#pragma pack(1)

struct _fat32buf
{
    // Bootsector (sector 0 = 512 bytes)
    CHAR jmp1[3];
    CHAR Oem_Id[8];
    CHAR Bpb[79];
    CHAR Boot_Code[411];
    USHORT FSD_LoadSeg;
    USHORT FSD_Entry;
    UCHAR FSD_Len;
    ULONG FSD_Addr;
    USHORT Boot_End;
    CHAR Sector1[512];
    // ldr starts from sector 2
    USHORT jmp2;
    USHORT FS_Len;
    USHORT Preldr_Len;
    UCHAR Force_Lba;
    UCHAR Bundle;
    CHAR data2[4];
    CHAR data3[30];
    UCHAR PartitionNr;
    UCHAR zero1;
    CHAR FS[16];
    CHAR Data4[8192-(60+1024)];
} fat32buf;

struct _fatbuf
{
    // Bootsector (sector 0 = 512 bytes)
    CHAR jmp1[3];
    CHAR Oem_Id[8];
    CHAR Bpb[51];
    CHAR Boot_Code[448];
    USHORT Boot_End;
} fatbuf;

// preldr0 header
struct _ldr0hdr
{
    USHORT jmp1;
    USHORT FSD_size;
    USHORT LDR_size;
    UCHAR  force_lba;
    UCHAR  bundle;
    ULONG  head2;
    CHAR   head3[30];
    UCHAR  PartNr;    // not used anymore
    UCHAR  zero1;
    CHAR   FS[16];
} ldr0hdr;

CHAR fs_type = 0;

#pragma pack()

#define STACKSIZE 0x10000

static void usage(char *s)
{
   show_message("Usage: [c:\\] %s x:\n"
                "(c) Netlabs, 2016, covered by (L)GPL\n"
                "Make a disk system.\n\n", 0, 0, 1, s);
}

void _System sysinstx_thread(int iArgc, char *rgArgv[], char *rgEnv[])
{
  ULONG cbSize, ulAction, cbActual, cbOffActual;
  struct extbpb dp;
  char   file[20];
  char   *drive = rgArgv[1];
  FILE   *fd;
  HANDLE  hf;
  APIRET rc;

  show_message("FAT32 version %s compiled on " __DATE__ "\n", 0, 0, 1, FAT32_VERSION);

  if (!rgArgv[1] ||
      (rgArgv[1] && (strstr(strlwr(rgArgv[1]), "/h") || strstr(strlwr(rgArgv[1]), "/?"))) ||
      (rgArgv[2] && (strstr(strlwr(rgArgv[2]), "/h") || strstr(strlwr(rgArgv[2]), "/?"))))
     {
     // show help
     usage(rgArgv[0]);
     exit(0);
     }

  open_drive(drive, &hf);

  lock_drive(hf);

  get_drive_params(hf, &dp);

  rc = ReadSect(hf, 0, sizeof(fat32buf) / dp.BytesPerSect, dp.BytesPerSect, (char *)&fat32buf);

  if (rc)
  {
    show_message("Cannot read %s disk, rc=%lu.\n", 0, 0, 2, drive, rc);
    show_message("%s\n", 0, 0, 1, get_error(rc));
    return;
  }

  if (!strncmp(strupr(((PBOOTSECT)(&fat32buf))->FileSystem), "FAT32   ", 8))
    fs_type = FAT_TYPE_FAT32;

  if (fs_type == FAT_TYPE_FAT32)
  {
    /* FAT32 */
    // copy bootsector to buffer (skipping JMP, OEM ID and BPB)
    memcpy(&fat32buf.Boot_Code, (char *)bootsec + 11 + 79, sizeof(bootsec) - 11 - 79);
    // copy OEM ID
    strncpy(&fat32buf.Oem_Id, "[osFree]", 8);
    // FSD load segment
    fat32buf.FSD_LoadSeg = 0x0800;
    // FSD entry point
    fat32buf.FSD_Entry = 0;
    // FSD length in sectors
    fat32buf.FSD_Len = (8192 - 1024) / 512;
    // FSD offset in sectors
    fat32buf.FSD_Addr = 2; 
    // copy the mini pre-loader
    memcpy((char *)&fat32buf.jmp2, preldr_mini, sizeof(preldr_mini));
    // copy FSD
    memcpy((char *)&fat32buf.jmp2 + sizeof(preldr_mini), fat_mdl, sizeof(fat_mdl));
    // FSD length
    fat32buf.FS_Len = sizeof(fat_mdl);
    // pre-loader length
    fat32buf.Preldr_Len = sizeof(preldr_mini);
    // FSD and pre-loaded are bundled
    fat32buf.Bundle = 0x80;
    // partition number (not used ATM)
    fat32buf.PartitionNr = 0;
    // FS name
    strncpy(&fat32buf.FS, "fat", 3);
    fat32buf.FS[3] = 0;

    rc = WriteSect(hf, 0, sizeof(fat32buf) / dp.BytesPerSect, dp.BytesPerSect, (char *)&fat32buf);

    if (rc)
    {
      show_message("Cannot write to %s disk, rc=%lu.\n", 0, 0, 2, drive, rc);
      show_message("%s\n", 0, 0, 1, get_error(rc));
      return;
    }
  }
  else
  {
    /* FAT12/FAT16 */
    // copy bootsector to fatbuf
    memcpy(&fatbuf, &fat32buf, sizeof(fatbuf));
    // copy bootsector to buffer (skipping JMP, OEM ID and BPB)
    memcpy(&fatbuf.Boot_Code, (char *)bootsec16 + 11 + 51, sizeof(bootsec16) - 11 - 51);
    // copy OEM ID
    strncpy(&fatbuf.Oem_Id, "[osFree]", 8);

    rc = WriteSect(hf, 0, sizeof(fatbuf) / dp.BytesPerSect, dp.BytesPerSect, (char *)&fatbuf);

    if (rc)
    {
      show_message("Cannot write to %s disk, rc=%lu.\n", 0, 0, 2, drive, rc);
      show_message("%s\n", 0, 0, 1, get_error(rc));
      return;
    }
  }

  //sectorio(hf);
  //stoplw(hf);

  unlock_drive(hf);
  close_drive(hf);

  // create subdirs
  memset(file, 0, sizeof(file));
  file[0] = drive[0];
  strcat(file, ":\\boot");
  mkdir(file);
  strcat(file, "\\loader");
  mkdir(file);
  strcat(file, "\\fsd");
  mkdir(file);

  memset(file, 0, sizeof(file));
  file[0] = drive[0];
  strcat(file, ":\\boot\\loader\\preldr0.mdl");

  fd = fopen(file, "wb+");

  if (! fd)
  {
    show_message("Cannot create %s file\n", 0, 0, 1, file);
    return;
  }

  cbActual = fwrite(preldr0_mdl, sizeof(preldr0_mdl), 1, fd);

  if (! cbActual)
  {
    show_message("Cannot write to %s file.\n", 0, 0, 1, file);
    return;
  }

  fclose(fd);

  memset(file, 0, sizeof(file));
  file[0] = drive[0];
  strcat(file, ":\\boot\\loader\\preldr0.mdl");

  fd = fopen(file, "rb+");

  if (! fd)
  {
    show_message("Cannot open %s file\n", 0, 0, 1, file);
    return;
  }

  fseek(fd, 0, SEEK_SET);

  cbActual = fread(&ldr0hdr, sizeof(ldr0hdr), 1, fd);

  if (! cbActual)
  {
    show_message("Cannot read from %s file.\n", 0, 0, 1, file);
    return;
  }

  ldr0hdr.FSD_size = 0;
  ldr0hdr.LDR_size = sizeof(preldr0_mdl);
  strcpy(&ldr0hdr.FS, "fat");

  fseek(fd, 0, SEEK_SET);

  cbActual = fwrite(&ldr0hdr, sizeof(ldr0hdr), 1, fd);

  if (! cbActual)
  {
    show_message("Cannot write to %s file.\n", 0, 0, 1, file);
    return;
  }

  fclose(fd);

  strncpy(file + 22, ".rel", 4);

  fd = fopen(file, "wb");

  if (! fd)
  {
    show_message("Cannot create %s file.\n", 0, 0, 1, file);
    return;
  }


  cbActual = fwrite(preldr0_rel, sizeof(preldr0_rel), 1, fd);

  if (! cbActual)
  {
    show_message("Cannot write to %s file.\n", 0, 0, 1, file);
    return;
  }

  fclose(fd);

  memset(file, 0, sizeof(file));
  file[0] = drive[0];
  strcat(file, ":\\boot\\loader\\fsd\\fat.mdl");

  fd = fopen(file, "wb");

  if (! fd)
  {
    show_message("Cannot create %s file, rc=%lu.\n", 0, 0, 1, file);
    return;
  }

  
  cbActual = fwrite(fat_mdl, sizeof(fat_mdl), 1, fd);

  if (! cbActual)
  {
    show_message("Cannot write to %s file.\n", 0, 0, 1, file);
    return;
  }

  fclose(fd);

  strncpy(file + 22, ".rel", 4);

  
  fd = fopen(file, "wb");

  if (! fd)
  {
    show_message("Cannot create %s file.\n", 0, 0, 1, file);
    return;
  }

  
  cbActual = fwrite(fat_rel, sizeof(fat_rel), 1, fd);

  if (! cbActual)
  {
    show_message("Cannot write to %s file.\n", 0, 0, 1, file);
    return;
  }

  //startlw(hf);

  fclose(fd);

  memset(file, 0, sizeof(file));
  file[0] = drive[0];
  strcat(file, ":\\os2boot");

  fd = fopen(file, "wb");

  if (! fd)
  {
    show_message("Cannot create %s file, rc=%lu.\n", 0, 0, 1, file);
    return;
  }
  
  cbActual = fwrite(os2boot, sizeof(os2boot), 1, fd);

  if (! cbActual)
  {
    show_message("Cannot write to %s file.\n", 0, 0, 1, file);
    return;
  }

  fclose(fd);

  if (fs_type < FAT_TYPE_FAT32)
  {
    /* copy boot sector config file */
    memset(file, 0, sizeof(file));
    file[0] = drive[0];
    strcat(file, ":\\boot\\bootsec.cfg");

    fd = fopen(file, "wb");

    if (! fd)
    {
      show_message("Cannot create %s file, rc=%lu.\n", 0, 0, 1, file);
      return;
    }
  
    cbActual = fwrite(bootsec16cfg, sizeof(bootsec16cfg), 1, fd);

    if (! cbActual)
    {
      show_message("Cannot write to %s file.\n", 0, 0, 1, file);
      return;
    }

    fclose(fd);
  }

  // The system files have been transferred.
  show_message("The system files have been transferred.\n", 0, 1272, 0);

  return;
}

int sys(int argc, char *argv[], char *envp[])
{
  char *stack;
  APIRET rc;

  // Here we're switching stack, because the original
  // sysinstx.com stack is too small.

  // allocate stack
  rc = mem_alloc((void **)&stack, STACKSIZE);

  if (rc)
    return rc;

  // call sysinstx_thread on new stack
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
    call sysinstx_thread
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
  return sys(argc, argv, NULL);
}
#endif
