#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INCL_DOS
#define INCL_DOSDEVIOCTL
#define INCL_DOSDEVICES
#define INCL_DOSERRORS

#include "os2.h"
#include "portable.h"
#include "fat32ifs.h"

ULONG ulIdxStartCluster = 0; // 'ea idx. sf'  start cluster
ULONG ulEadStartCluster = 0; // 'ea data. sf' start cluster

PUSHORT pBaseTable; // base table

typedef unsigned char STR14[14];
typedef STR14   *PSTR14;

USHORT usCopyEAS2(PVOLINFO pVolInfo, ULONG ulSrcDirCluster, PSZ pszSrcFile, PSHOPENINFO pDirSrcSHInfo,
                  ULONG ulTarDirCluster, PSZ pszTarFile, PSHOPENINFO pDirTarSHInfo);
USHORT usIdxGetRecord(PVOLINFO pVolInfo, ULONG ulFileCluster, PUSHORT pusEAHandle);
USHORT usIdxAddRecord(PVOLINFO pVolInfo, ULONG ulFileCluster, USHORT usEAHandle);
USHORT usIdxDelRecord(PVOLINFO pVolInfo, ULONG ulFileCluster);
USHORT usGetEAKeys(PVOLINFO pVolInfo, ULONG ulDirCluster, PSZ pszFile,
                    PSHOPENINFO pDirSHInfo, ULONG *pulFirstClus, USHORT *pusEAHandle, PSTR14 chShortName);
USHORT usSetEAKeys(PVOLINFO pVolInfo, ULONG ulDirCluster, PSZ pszFile,
                   PSHOPENINFO pDirSHInfo, ULONG *pulFirstClus, USHORT *pusEAHandle);

USHORT usInitEAS2(PVOLINFO pVolInfo)
{
PDIRENTRY  pDirEntry;
PDIRENTRY1 pStreamEntry = NULL;
char *pBuf;
ULONG ulCluster;
APIRET rc;

   pBuf = (char *)malloc((size_t)pVolInfo->ulBlockSize);
   if (!pBuf)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto usInitEAS2Exit;
      }
   pDirEntry = (PDIRENTRY)malloc((size_t)sizeof(DIRENTRY));
   if (!pDirEntry)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto usInitEAS2Exit;
      }

   ulCluster = FindPathCluster(pVolInfo, pVolInfo->BootSect.bpb.RootDirStrtClus, EA_DATA_FILE,
                               NULL, pDirEntry, pStreamEntry, NULL);

   if (ulCluster == pVolInfo->ulFatEof || !ulCluster)
      {
      rc = ERROR_FILE_NOT_FOUND;
      goto usInitEAS2Exit;
      }

   ulEadStartCluster = ulCluster;

   ulCluster = FindPathCluster(pVolInfo, pVolInfo->BootSect.bpb.RootDirStrtClus, EA_IDX_FILE,
                               NULL, pDirEntry, pStreamEntry, NULL);

   if (ulCluster == pVolInfo->ulFatEof || !ulCluster)
      {
      rc = ERROR_FILE_NOT_FOUND;
      goto usInitEAS2Exit;
      }

   ulIdxStartCluster = ulCluster;

   pBaseTable = (PUSHORT)malloc(BASE_TBL_ENTRIES * sizeof(USHORT));
   if (pBaseTable)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto usInitEAS2Exit;
      }

   rc = ReadBlock(pVolInfo, ulEadStartCluster, 0, pBuf, 0);
   if (rc)
      rc = ERROR_NOT_ENOUGH_MEMORY;

   memcpy(pBaseTable, pBuf + 32, BASE_TBL_ENTRIES * sizeof(USHORT));

usInitEAS2Exit:
   if (pBuf)
      free(pBuf);
   if (pDirEntry)
      free(pDirEntry);

   return rc;
}

USHORT usMoveEAS2(PVOLINFO pVolInfo, ULONG ulSrcDirCluster, PSZ pszSrcFile, PSHOPENINFO pDirSrcSHInfo,
                  ULONG ulTarDirCluster, PSZ pszTarFile, PSHOPENINFO pDirTarSHInfo)
{
ULONG ulSrcCluster, ulTarCluster;
USHORT usSrcEAHandle, usTarEAHandle;
STR14 chSrcShortName, chTarShortName;
APIRET rc;

   rc = usGetEAKeys(pVolInfo, ulSrcDirCluster, pszSrcFile, pDirSrcSHInfo, &ulSrcCluster, &usSrcEAHandle, &chSrcShortName);
   if (rc)
      return rc;
   rc = usGetEAKeys(pVolInfo, ulTarDirCluster, pszTarFile, pDirTarSHInfo, &ulTarCluster, &usTarEAHandle, &chTarShortName);
   if (rc)
      return rc;

   rc = usSetEAKeys(pVolInfo, ulTarDirCluster, pszTarFile, pDirTarSHInfo, &ulTarCluster, &usSrcEAHandle);
   if (rc)
      return rc;

   usTarEAHandle = 0xffff;
   rc = usSetEAKeys(pVolInfo, ulSrcDirCluster, pszSrcFile, pDirSrcSHInfo, &ulSrcCluster, &usTarEAHandle);
   return rc;
}

USHORT usCopyEAS2(PVOLINFO pVolInfo, ULONG ulSrcDirCluster, PSZ pszSrcFile, PSHOPENINFO pDirSrcSHInfo,
                  ULONG ulTarDirCluster, PSZ pszTarFile, PSHOPENINFO pDirTarSHInfo)
{
ULONG ulSrcCluster, ulTarCluster;
USHORT usSrcEAHandle, usTarEAHandle;
STR14 chSrcShortName, chTarShortName;
APIRET rc;

   rc = usGetEAKeys(pVolInfo, ulSrcDirCluster, pszSrcFile, pDirSrcSHInfo, &ulSrcCluster, &usSrcEAHandle, &chSrcShortName);
   if (rc)
      return rc;

   rc = usIdxAddRecord(pVolInfo, ulTarCluster, usTarEAHandle);;
   if (rc)
      return rc;

   // copy EAS

   rc = usGetEAKeys(pVolInfo, ulTarDirCluster, pszTarFile, pDirTarSHInfo, &ulTarCluster, &usTarEAHandle, &chTarShortName);
   return rc;
}

// Get FirstCluster/EAHandle/short name from the dir entry
USHORT usGetEAKeys(PVOLINFO pVolInfo, ULONG ulDirCluster, PSZ pszFile,
                    PSHOPENINFO pDirSHInfo, ULONG *pulFirstClus, USHORT *pusEAHandle, PSTR14 chShortName)
{
ULONG ulSrcCluster;
PDIRENTRY  pDirEntry;
PDIRENTRY1 pStreamEntry = NULL;
APIRET rc;
char *p = *chShortName;
int i;

   pDirEntry = (PDIRENTRY)malloc((size_t)sizeof(DIRENTRY));
   if (!pDirEntry)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto usGetEAKeysExit;
      }
#ifdef EXFAT
   pStreamEntry = (PDIRENTRY1)malloc((size_t)sizeof(DIRENTRY1));
   if (!pStreamEntry)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto usGetEAKeysExit;
      }
#endif

   ulSrcCluster = FindPathCluster(pVolInfo, ulDirCluster, pszFile, pDirSHInfo, pDirEntry, pStreamEntry, NULL);
   if (ulSrcCluster == pVolInfo->ulFatEof)
      {
      rc = ERROR_FILE_NOT_FOUND;
      goto usGetEAKeysExit;
      }

#ifdef EXFAT
   if (pVolInfo->bFatType < FAT_TYPE_FAT32)
      {
#endif
      *pusEAHandle = pDirEntry->wClusterHigh;
#ifdef EXFAT
      }
   else
      {
      *pusEAHandle = EA_HANDLE_UNUSED;
      }
#endif

#ifdef EXFAT
   if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
      {
#endif
      // copy short name
      i = 0;
      while (*(PBYTE)(pDirEntry + i) != ' ')
         {
         *p++ = *(PBYTE)(pDirEntry + i++);
         if (i >= 8)
            break;
         }
      *p++ = '.';
      i = 8;
      while (*(PBYTE)(pDirEntry + i) != ' ')
         {
         *p++ = *(PBYTE)(pDirEntry + i++);
         if (i >= 3)
            break;
         }
      *p++ = '\0';

      *pulFirstClus = (ULONG)pDirEntry->wClusterHigh * 0x10000 + pDirEntry->wCluster;
#ifdef EXFAT
      }
   else
      {
      **chShortName = '\0';
      *pulFirstClus = pStreamEntry->u.Stream.ulFirstClus;
      }
#endif

usGetEAKeysExit:
   if (pDirEntry)
      free(pDirEntry);
#ifdef EXFAT
   if (pStreamEntry)
      free(pStreamEntry);
#endif

   return rc;
}

// Set EAHandle/First cluster in the direntry
USHORT usSetEAKeys(PVOLINFO pVolInfo, ULONG ulDirCluster, PSZ pszFile,
                    PSHOPENINFO pDirSHInfo, ULONG *pulFirstClus, USHORT *pusEAHandle)
{
ULONG ulSrcCluster;
PDIRENTRY  pDirEntry;
PDIRENTRY1 pStreamEntry = NULL;
APIRET rc;

   pDirEntry = (PDIRENTRY)malloc((size_t)sizeof(DIRENTRY));
   if (!pDirEntry)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto usSetEAKeysExit;
      }
#ifdef EXFAT
   pStreamEntry = (PDIRENTRY1)malloc((size_t)sizeof(DIRENTRY1));
   if (!pStreamEntry)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto usSetEAKeysExit;
      }
#endif

   ulSrcCluster = FindPathCluster(pVolInfo, ulDirCluster, pszFile, pDirSHInfo, pDirEntry, pStreamEntry, NULL);
   if (ulSrcCluster == pVolInfo->ulFatEof)
      {
      rc = ERROR_FILE_NOT_FOUND;
      goto usSetEAKeysExit;
      }

   if (pVolInfo->bFatType < FAT_TYPE_FAT32)
      pDirEntry->wClusterHigh = *pusEAHandle;

#ifdef EXFAT
   if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
      {
#endif
      pDirEntry->wCluster = *pulFirstClus & 0xffff;
      pDirEntry->wClusterHigh = *pulFirstClus >> 16;
#ifdef EXFAT
      }
   else
      {
      pStreamEntry->u.Stream.ulFirstClus = *pulFirstClus;
      }
#endif

   rc = ModifyDirectory(pVolInfo, ulDirCluster, pDirSHInfo, MODIFY_DIR_UPDATE, pDirEntry, NULL, pStreamEntry, NULL, pszFile, NULL, 0);
   if (rc)
      goto usSetEAKeysExit;

usSetEAKeysExit:
   if (pDirEntry)
      free(pDirEntry);
#ifdef EXFAT
   if (pStreamEntry)
      free(pStreamEntry);
#endif

   return rc;
}

USHORT usGetEAS2(PVOLINFO pVolInfo, ULONG ulFirstClus, USHORT usEAHandle, PFEALIST pFeal)
{
   return 0;
}

USHORT usSetEAS2(PVOLINFO pVolInfo, ULONG ulFirstClus, USHORT usEAHandle, PSTR14 pszShortName, PFEALIST pFeal)
{
   return 0;
}

USHORT usDelEAS2(PVOLINFO pVolInfo, ULONG ulFirstClus, USHORT usEAHandle)
{
   return 0;
}

// Get a record from 'ea idx. sf'
USHORT usIdxGetRecord(PVOLINFO pVolInfo, ULONG ulFileCluster, PUSHORT pusEAHandle)
{
PEA_INDEX_REC pIndRec, pIndMax;
ULONG ulCluster = ulIdxStartCluster;
ULONG ulBlock;
char *pBuf;
APIRET rc = 0;

   pBuf = (char *)malloc((size_t)pVolInfo->ulBlockSize);
   if (!pBuf)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto usIdxGetRecordExit;
      }

   while (ulCluster != pVolInfo->ulFatEof)
      {
      for (ulBlock = 0; ulBlock < pVolInfo->ulClusterSize / pVolInfo->ulBlockSize; ulBlock++)
         {
         rc = ReadBlock(pVolInfo, ulCluster, ulBlock, pBuf, 0);
         if (rc)
            goto usIdxGetRecordExit;
         pIndRec = (PEA_INDEX_REC)pBuf;
         pIndMax = (PEA_INDEX_REC)((PBYTE)pBuf + pVolInfo->ulBlockSize);
         while (pIndRec < pIndMax)
            {
            if (pIndRec->ulCluster == pVolInfo->ulFatEof) // skip deleted entries
               continue;
            if (!pIndRec->ulCluster) // the EOF reached
               {
               rc = ERROR_FILE_NOT_FOUND;
               goto usIdxGetRecordExit;
               }
            if (pIndRec->ulCluster == ulFileCluster)
               {
               *pusEAHandle = pIndRec->usEAHandle;
               rc = 0;
               goto usIdxGetRecordExit;
               }
            pIndRec++;
            }
         }
      ulCluster = GetNextCluster(pVolInfo, NULL, ulCluster);
      if (!ulCluster)
         ulCluster = pVolInfo->ulFatEof;
      }

usIdxGetRecordExit:
   if (pBuf)
      free(pBuf);

   return rc;
}

// Set a record in 'ea idx. sf'
USHORT usIdxAddRecord(PVOLINFO pVolInfo, ULONG ulFileCluster, USHORT usEAHandle)
{
PEA_INDEX_REC pIndRec, pIndMax;
ULONG ulCluster = ulIdxStartCluster;
ULONG ulBlock;
char *pBuf;
APIRET rc = 0;

   pBuf = (char *)malloc((size_t)pVolInfo->ulBlockSize);
   if (!pBuf)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto usIdxSetRecordExit;
      }

   while (ulCluster != pVolInfo->ulFatEof)
      {
      for (ulBlock = 0; ulBlock < pVolInfo->ulClusterSize / pVolInfo->ulBlockSize; ulBlock++)
         {
         rc = ReadBlock(pVolInfo, ulCluster, ulBlock, pBuf, 0);
         if (rc)
            goto usIdxSetRecordExit;
         pIndRec = (PEA_INDEX_REC)pBuf;
         pIndMax = (PEA_INDEX_REC)((PBYTE)pBuf + pVolInfo->ulBlockSize);
         while (pIndRec < pIndMax)
            {
            if (pIndRec->ulCluster == pVolInfo->ulFatEof || // deleted entry found
                !pIndRec->ulCluster) // or, the EOF reached
               {
               rc = 0;
               pIndRec->ulCluster = ulFileCluster;
               pIndRec->usEAHandle = usEAHandle;
               pIndRec++;
               rc = WriteBlock(pVolInfo, ulCluster, ulBlock, pBuf, 0);
               goto usIdxSetRecordExit;
               }
            pIndRec++;
            }
         }
      ulCluster = GetNextCluster(pVolInfo, NULL, ulCluster);
      if (!ulCluster)
         ulCluster = pVolInfo->ulFatEof;
      if (ulCluster == pVolInfo->ulFatEof)
         {
         // try grow the Idx file to 1 cluster
         if (MakeFatChain(pVolInfo, NULL, pVolInfo->ulFatEof, 1UL, NULL) == pVolInfo->ulFatEof)
            {
            rc = ERROR_DISK_FULL;
            goto usIdxSetRecordExit;
            }
         else
            {
            ulCluster = GetNextCluster(pVolInfo, NULL, ulCluster);
            }
         }
      }

usIdxSetRecordExit:
   if (pBuf)
      free(pBuf);

   return rc;
}

// Delete a record in 'ea idx. sf'
USHORT usIdxDelRecord(PVOLINFO pVolInfo, ULONG ulFileCluster)
{
PEA_INDEX_REC pIndRec, pIndMax;
ULONG ulCluster = ulIdxStartCluster;
ULONG ulBlock;
char *pBuf;
APIRET rc = 0;

   pBuf = (char *)malloc((size_t)pVolInfo->ulBlockSize);
   if (!pBuf)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto usIdxDelRecordExit;
      }

   while (ulCluster != pVolInfo->ulFatEof)
      {
      for (ulBlock = 0; ulBlock < pVolInfo->ulClusterSize / pVolInfo->ulBlockSize; ulBlock++)
         {
         rc = ReadBlock(pVolInfo, ulCluster, ulBlock, pBuf, 0);
         if (rc)
            goto usIdxDelRecordExit;
         pIndRec = (PEA_INDEX_REC)pBuf;
         pIndMax = (PEA_INDEX_REC)((PBYTE)pBuf + pVolInfo->ulBlockSize);
         while (pIndRec < pIndMax)
            {
            if (pIndRec->ulCluster == ulFileCluster)
               {
               rc = 0;
               pIndRec->ulCluster = pVolInfo->ulFatEof;
               pIndRec->usEAHandle = 0xffff;
               rc = WriteBlock(pVolInfo, ulCluster, ulBlock, pBuf, 0);
               goto usIdxDelRecordExit;
               }
            pIndRec++;
            }
         }
      ulCluster = GetNextCluster(pVolInfo, NULL, ulCluster);
      if (!ulCluster)
         ulCluster = pVolInfo->ulFatEof;
      if (ulCluster == pVolInfo->ulFatEof)
         {
         // try grow the Idx file to 1 cluster
         if (MakeFatChain(pVolInfo, NULL, pVolInfo->ulFatEof, 1UL, NULL) == pVolInfo->ulFatEof)
            {
            rc = ERROR_DISK_FULL;
            goto usIdxDelRecordExit;
            }
         else
            {
            ulCluster = GetNextCluster(pVolInfo, NULL, ulCluster);
            }
         }
      }

usIdxDelRecordExit:
   if (pBuf)
      free(pBuf);

   return rc;
}
