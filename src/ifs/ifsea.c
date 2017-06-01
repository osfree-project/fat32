#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stddef.h>
#include <time.h>
#include <dos.h>

#define INCL_DOS
#define INCL_DOSDEVIOCTL
#define INCL_DOSDEVICES
#define INCL_DOSERRORS

#include "os2.h"
#include "portable.h"
#include "fat32ifs.h"


PRIVATE PFEA   FindEA(PFEALIST pFeal, PSZ pszName, USHORT usMaxName);
PRIVATE USHORT usReadEAS(PVOLINFO pVolInfo, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo, PSZ pszFileName, PFEALIST * ppFEAL, BOOL fCreate);
PRIVATE USHORT usWriteEAS(PVOLINFO pVolInfo, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo, PSZ pszFileName, PFEALIST pFEAL);
PRIVATE USHORT GetEASName(PVOLINFO pVolInfo, ULONG ulDirCluster, PSZ pszFileName, PSZ * pszEASName);


/************************************************************************
*
************************************************************************/
USHORT usModifyEAS(PVOLINFO pVolInfo, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo, PSZ pszFileName, PEAOP pEAOP)
{
USHORT rc;
PFEALIST pTarFeal;
PFEALIST pSrcFeal;
PBYTE    pSrcMax;
PFEA pSrcFea;
PFEA pTarFea;

   if (f32Parms.fMessageActive & LOG_EAS)
      Message("usModifyEAS for %s", pszFileName);

   /*
      Do not allow ea's file files with no filename (root)
   */
   if (!strlen(pszFileName))
      return 284;

   rc = MY_PROBEBUF(PB_OPWRITE, (PBYTE)pEAOP, sizeof (EAOP));
   if (rc)
      {
      Message("Protection violation in usModifyEAS (1) at %lX", pEAOP);
      return rc;
      }

   pSrcFeal = pEAOP->fpFEAList;
   if (pSrcFeal->cbList > MAX_EA_SIZE)
      return ERROR_EA_LIST_TOO_LONG;

   rc = MY_PROBEBUF(PB_OPREAD, (PBYTE)pSrcFeal, (USHORT)pSrcFeal->cbList);
   if (rc)
      {
      Message("Protection violation in usModifyEAS (2) at %lX", pSrcFeal);
      return rc;
      }

   if (pSrcFeal->cbList <= sizeof (ULONG))
      return 0;

   rc = usReadEAS(pVolInfo, ulDirCluster, pDirSHInfo, pszFileName, &pTarFeal, TRUE);
   if (rc)
      return rc;

   if (f32Parms.fMessageActive & LOG_EAS)
      Message("cbList before = %lu", pTarFeal->cbList);

   pSrcMax = (PBYTE)pSrcFeal + pSrcFeal->cbList;
   pSrcFea = pSrcFeal->list;
   while ((PBYTE)pSrcFea + sizeof (FEA) < pSrcMax)
      {
      PBYTE pName;
      USHORT usNewSize = sizeof (FEA) + (USHORT)pSrcFea->cbName + 1 + pSrcFea->cbValue;

      if ((PBYTE)pSrcFea + usNewSize > pSrcMax)
         {
         pEAOP->oError = (PBYTE)pSrcFea - (PBYTE)pEAOP;
         rc = ERROR_EA_LIST_INCONSISTENT;
         goto usStoreEASExit;
         }
      pName  = (PBYTE)(pSrcFea + 1);

      rc = FSH_CHECKEANAME(0x0001, pSrcFea->cbName, pName);
      if (rc && pSrcFea->cbValue)
         {
         pEAOP->oError = (PBYTE)pSrcFea - (PBYTE)pEAOP;
         goto usStoreEASExit;
         }

      if (!pSrcFea->cbValue || !pSrcFea->cbName)
         usNewSize = 0;
      else
         usNewSize = sizeof (FEA) + (USHORT)pSrcFea->cbName + 1 + pSrcFea->cbValue;

      pTarFea = FindEA(pTarFeal, pName, pSrcFea->cbName);
      if (!pTarFea)
         {
         pTarFea = (PFEA)((PBYTE)pTarFeal + pTarFeal->cbList);
         if (MAX_EA_SIZE - pTarFeal->cbList < (ULONG)usNewSize)
            {
            rc = ERROR_EAS_DIDNT_FIT;
            goto usStoreEASExit;
            }
         memcpy(pTarFea, pSrcFea, usNewSize);
         pTarFeal->cbList += usNewSize;

         if (f32Parms.fMessageActive & LOG_EAS)
            Message("Inserting EA '%s' (%u,%u)", pName,
               pSrcFea->cbName, pSrcFea->cbValue);
         }
      else
         {
         USHORT usOldSize  = sizeof (FEA) + (USHORT)pTarFea->cbName + 1 + pTarFea->cbValue;
         USHORT usMoveSize = (USHORT)pTarFeal->cbList -
            ((PBYTE)pTarFea - (PBYTE)pTarFeal);
         usMoveSize -= usOldSize;

         if (usOldSize < usNewSize)
            {
            if (MAX_EA_SIZE - pTarFeal->cbList < (ULONG)(usNewSize - usOldSize))
               {
               rc = ERROR_EAS_DIDNT_FIT;
               goto usStoreEASExit;
               }
            }
         memmove((PBYTE)pTarFea + usNewSize,
                 (PBYTE)pTarFea + usOldSize, usMoveSize);
         memcpy(pTarFea, pSrcFea, usNewSize);
         pTarFeal->cbList -= usOldSize;
         pTarFeal->cbList += usNewSize;

         if (f32Parms.fMessageActive & LOG_EAS)
            Message("Updating EA '%s' (%u,%u)", pName,
               pSrcFea->cbName, pSrcFea->cbValue);
         }

      usNewSize = sizeof (FEA) + (USHORT)pSrcFea->cbName + 1 + pSrcFea->cbValue;
      pSrcFea = (PFEA)((PBYTE)pSrcFea + usNewSize);
      }

   if (f32Parms.fMessageActive & LOG_EAS)
      Message("cbList after = %lu", pTarFeal->cbList);

   if (pTarFeal->cbList > 4)
      rc = usWriteEAS(pVolInfo, ulDirCluster, pDirSHInfo, pszFileName, pTarFeal);
   else
      rc = usDeleteEAS(pVolInfo, ulDirCluster, pDirSHInfo, pszFileName);

usStoreEASExit:
   free(pTarFeal);

   if (f32Parms.fMessageActive & LOG_EAS)
      Message("usModifyEAS for %s returned %d",
         pszFileName, rc);

   return rc;
}

/************************************************************************
*
************************************************************************/
USHORT usGetEASize(PVOLINFO pVolInfo, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo, PSZ pszFileName, PULONG pulSize)
{
PSZ pszEAName;
USHORT rc;
DIRENTRY DirEntry;
DIRENTRY1 DirEntryStream;
ULONG    ulCluster;

   if (f32Parms.fMessageActive & LOG_EAS)
      Message("usGetEASSize for %s", pszFileName);

   *pulSize = sizeof (ULONG);

   rc = GetEASName(pVolInfo, ulDirCluster, pszFileName, &pszEAName);
   if (rc)
      return rc;

   ulCluster = FindPathCluster(pVolInfo, ulDirCluster, pszEAName,
      pDirSHInfo, &DirEntry, &DirEntryStream, NULL);
   if (ulCluster == pVolInfo->ulFatEof || !ulCluster)
      {
      rc = 0;
      goto usGetEASizeExit;
      }
#ifdef EXFAT
   if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
#endif
      *pulSize = DirEntry.ulFileSize;
#ifdef EXFAT
   else
#ifdef INCL_LONGLONG
      *pulSize = DirEntryStream.u.Stream.ullValidDataLen;
#else
      *pulSize = DirEntryStream.u.Stream.ullValidDataLen.ulLo;
#endif
#endif

   rc = 0;

usGetEASizeExit:
   free(pszEAName);

   if (f32Parms.fMessageActive & LOG_EAS)
      Message("usGetEASize for %s returned %d (%u bytes large)",
         pszFileName, rc, *pulSize);

   return rc;
}

/************************************************************************
*
************************************************************************/
USHORT usGetEAS(PVOLINFO pVolInfo, USHORT usLevel, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo, PSZ pszFileName, PEAOP pEAOP)
{
USHORT rc;
PFEALIST pTarFeal;
PFEALIST pSrcFeal;
PFEA     pSrcFea;
PFEA     pTarFea;
PGEALIST pGeaList;
USHORT   usMaxSize;

   if (f32Parms.fMessageActive & LOG_EAS)
      Message("usGetEAS for %s Level %d", pszFileName, usLevel);

   /*
      Checking all the arguments
   */

   rc = MY_PROBEBUF(PB_OPWRITE, (PBYTE)pEAOP, sizeof (EAOP));
   if (rc)
      {
      Message("Protection violation in usGetEAS (1) at %lX", pEAOP);
      return rc;
      }

   pTarFeal = pEAOP->fpFEAList;
   rc = MY_PROBEBUF(PB_OPREAD, (PBYTE)pTarFeal, sizeof (ULONG));
   if (rc)
      {
      Message("Protection violation in usGetEAS (2) at %lX", pTarFeal);
      return rc;
      }
   if (pTarFeal->cbList > MAX_EA_SIZE)
      usMaxSize = (USHORT)MAX_EA_SIZE;
   else
      usMaxSize = (USHORT)pTarFeal->cbList;

   if (usMaxSize < sizeof (ULONG))
      return ERROR_BUFFER_OVERFLOW;

   rc = MY_PROBEBUF(PB_OPWRITE, (PBYTE)pTarFeal, (USHORT)usMaxSize);
   if (rc)
      return rc;

   if (usLevel == FIL_QUERYEASFROMLIST || usLevel == FIL_QUERYEASFROMLISTL)
      {
      pGeaList = pEAOP->fpGEAList;
      rc = MY_PROBEBUF(PB_OPREAD, (PBYTE)pGeaList, sizeof (ULONG));
      if (rc)
         return rc;
      if (pGeaList->cbList > MAX_EA_SIZE)
         return ERROR_EA_LIST_TOO_LONG;
      rc = MY_PROBEBUF(PB_OPREAD, (PBYTE)pGeaList, (USHORT)pGeaList->cbList);
      if (rc)
         return rc;
      }
   else
      pGeaList = NULL;

   /*
      Initialize the FEALIST
   */
   memset(pTarFeal, 0, usMaxSize);
   pTarFeal->cbList = sizeof (ULONG);
   usMaxSize -= sizeof (ULONG);
   pTarFea = pTarFeal->list;

   /*
      Does the EA Exist?
   */

   rc = usReadEAS(pVolInfo, ulDirCluster, pDirSHInfo, pszFileName, &pSrcFeal, FALSE);
   if (rc)
      goto usGetEASExit;

   /*
      If not, return
   */
   if (usLevel == FIL_QUERYEASFROMLIST || usLevel == FIL_QUERYEASFROMLISTL)
      {
      PBYTE    pGeaMax;
      PGEA     pGea;

      pGeaMax = (PBYTE)pGeaList + pGeaList->cbList;
      pGea    = pGeaList->list;
      while ((PBYTE)pGea + sizeof (GEA) < pGeaMax)
         {
         USHORT usGeaSize = sizeof (GEA) + (USHORT)pGea->cbName;
         USHORT usFeaSize;

         if (pGea->szName + (USHORT)pGea->cbName > pGeaMax)
            {
            rc = ERROR_EA_LIST_INCONSISTENT;
            goto usGetEASExit;
            }

         pSrcFea = FindEA(pSrcFeal, pGea->szName, pGea->cbName);
         if (pSrcFea)
            {
            usFeaSize = sizeof (FEA) + (USHORT)pSrcFea->cbName + 1 + pSrcFea->cbValue;
            if (usFeaSize > usMaxSize)
               {
               rc = ERROR_BUFFER_OVERFLOW;
               pTarFeal->cbList = pSrcFeal->cbList;
               goto usGetEASExit;
               }
            if (f32Parms.fMessageActive & LOG_EAS)
               Message("Found %s", pSrcFea + 1);
            memcpy(pTarFea, pSrcFea, usFeaSize);
            }
         else
            {
            usFeaSize = sizeof (FEA) + (USHORT)pGea->cbName + 1;
            if (usFeaSize > usMaxSize)
               {
               rc = ERROR_BUFFER_OVERFLOW;
               if (pSrcFeal)
                  pTarFeal->cbList = pSrcFeal->cbList;
               else
                  pTarFeal->cbList = 4;
               goto usGetEASExit;
               }

            if (f32Parms.fMessageActive & LOG_EAS)
               Message("usGetEAS: %s not found!", pGea->szName);

            pTarFea->fEA = 0x00;
            pTarFea->cbName = pGea->cbName;
            pTarFea->cbValue = 0;
            strcpy((PBYTE)(pTarFea + 1), pGea->szName);
            }

         pTarFea = (PFEA)((PBYTE)pTarFea + usFeaSize);
         pTarFeal->cbList += usFeaSize;
         usMaxSize -= usFeaSize;

         pGea = (PGEA)((PBYTE)pGea + usGeaSize);
         }
      }
   else if (pSrcFeal)
      {
      PBYTE     pSrcMax = (PBYTE)pSrcFeal + pSrcFeal->cbList;
      pSrcFea = pSrcFeal->list;

      while ((PBYTE)pSrcFea + sizeof (FEA) < pSrcMax)
         {
         USHORT usFeaSize = sizeof (FEA) + (USHORT)pSrcFea->cbName + 1 + pSrcFea->cbValue;
         if (usFeaSize > usMaxSize)
            {
            rc = ERROR_BUFFER_OVERFLOW;
            pTarFeal->cbList = pSrcFeal->cbList;
            goto usGetEASExit;
            }
         if (f32Parms.fMessageActive & LOG_EAS)
            Message("Found %s (%u,%u)", pSrcFea + 1, (USHORT)pSrcFea->cbName, pSrcFea->cbValue);
         memcpy(pTarFea, pSrcFea, usFeaSize);
         pTarFea = (PFEA)((PBYTE)pTarFea + usFeaSize);
         pTarFeal->cbList += usFeaSize;
         pSrcFea = (PFEA)((PBYTE)pSrcFea + usFeaSize);
         usMaxSize -= usFeaSize;
         }
      }

   rc = 0;

usGetEASExit:

   if (pSrcFeal)
      free(pSrcFeal);

   if (f32Parms.fMessageActive & LOG_EAS)
      Message("usGetEAS for %s returned %d (%lu bytes in EAS)",
         pszFileName, rc, pTarFeal->cbList);

   return rc;
}

/************************************************************************
*
************************************************************************/
USHORT usCopyEAS(PVOLINFO pVolInfo, ULONG ulSrcDirCluster, PSZ pszSrcFile, PSHOPENINFO pDirSrcSHInfo,
                 ULONG ulTarDirCluster, PSZ pszTarFile, PSHOPENINFO pDirTarSHInfo)
{
USHORT rc;
ULONG ulSrcCluster, ulTarCluster;
PSZ   pszSrcEAName = NULL,
      pszTarEAName = NULL;
DIRENTRY SrcEntry, TarEntry;
DIRENTRY1 TarStreamEntry;
DIRENTRY1 SrcStreamEntry;
#ifdef EXFAT
SHOPENINFO SrcSHInfo;
#endif
PSHOPENINFO pSrcSHInfo = NULL;

   rc = GetEASName(pVolInfo, ulSrcDirCluster, pszSrcFile, &pszSrcEAName);
   if (rc)
      goto usCopyEASExit;
   rc = GetEASName(pVolInfo, ulTarDirCluster, pszTarFile, &pszTarEAName);
   if (rc)
      goto usCopyEASExit;

   ulSrcCluster = FindPathCluster(pVolInfo, ulSrcDirCluster, pszSrcEAName, pDirSrcSHInfo, &SrcEntry, &SrcStreamEntry, NULL);
   ulTarCluster = FindPathCluster(pVolInfo, ulTarDirCluster, pszTarEAName, pDirTarSHInfo, &TarEntry, &TarStreamEntry, NULL);
   if (ulTarCluster != pVolInfo->ulFatEof)
      {
      rc = ModifyDirectory(pVolInfo, ulTarDirCluster, pDirTarSHInfo, MODIFY_DIR_DELETE, &TarEntry, NULL, &TarStreamEntry, NULL, NULL, 0);
      if (rc)
         goto usCopyEASExit;
      DeleteFatChain(pVolInfo, ulTarCluster);
      }

   if (ulSrcCluster == pVolInfo->ulFatEof)
      goto usCopyEASExit;

#ifdef EXFAT
   if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
      {
      pSrcSHInfo = &SrcSHInfo;
      SetSHInfo1(pVolInfo, &SrcStreamEntry, pSrcSHInfo);
      }
#endif

   rc = CopyChain(pVolInfo, pSrcSHInfo, ulSrcCluster, &ulTarCluster);
   if (rc)
      goto usCopyEASExit;

#ifdef EXFAT
   if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
      {
#endif
      SrcEntry.wCluster = LOUSHORT(ulTarCluster);
      SrcEntry.wClusterHigh = HIUSHORT(ulTarCluster);
#ifdef EXFAT
      }
   else
      {
      SrcStreamEntry.u.Stream.ulFirstClus = ulTarCluster;
      }
#endif

   /*
      Make new direntry
   */
   rc = ModifyDirectory(pVolInfo, ulTarDirCluster, pDirTarSHInfo, MODIFY_DIR_INSERT, NULL, &SrcEntry, NULL, &SrcStreamEntry, pszTarEAName, 0);


usCopyEASExit:
   if (pszSrcEAName)
      free(pszSrcEAName);
   if (pszTarEAName)
      free(pszTarEAName);

   if (f32Parms.fMessageActive & LOG_EAS)
      Message("usCopyEAS for returned %d", rc);

   return rc;
}

/************************************************************************
*
************************************************************************/
USHORT usMoveEAS(PVOLINFO pVolInfo, ULONG ulSrcDirCluster, PSZ pszSrcFile, PSHOPENINFO pDirSrcSHInfo,
                 ULONG ulTarDirCluster, PSZ pszTarFile, PSHOPENINFO pDirTarSHInfo)
{
USHORT rc;
ULONG ulSrcCluster, ulTarCluster;
PSZ   pszSrcEAName = NULL,
      pszTarEAName = NULL;
DIRENTRY SrcEntry, TarEntry;
DIRENTRY1 SrcStreamEntry, TarStreamEntry;

   rc = GetEASName(pVolInfo, ulSrcDirCluster, pszSrcFile, &pszSrcEAName);
   if (rc)
      goto usMoveEASExit;
   rc = GetEASName(pVolInfo, ulTarDirCluster, pszTarFile, &pszTarEAName);
   if (rc)
      goto usMoveEASExit;


   ulSrcCluster = FindPathCluster(pVolInfo, ulSrcDirCluster, pszSrcEAName, pDirSrcSHInfo, &SrcEntry, &SrcStreamEntry, NULL);
   ulTarCluster = FindPathCluster(pVolInfo, ulTarDirCluster, pszTarEAName, pDirTarSHInfo, &TarEntry, &TarStreamEntry, NULL);
   if (ulTarCluster != pVolInfo->ulFatEof && ulTarCluster != ulSrcCluster)
      {
      rc = ModifyDirectory(pVolInfo, ulTarDirCluster, pDirTarSHInfo, MODIFY_DIR_DELETE, &TarEntry, NULL, &TarStreamEntry, NULL, NULL, 0);
      if (rc)
         goto usMoveEASExit;
      DeleteFatChain(pVolInfo, ulTarCluster);
      }

   if (ulSrcCluster == pVolInfo->ulFatEof)
      goto usMoveEASExit;

   if (ulSrcDirCluster == ulTarDirCluster)
      {
      memmove(&TarEntry, &SrcEntry, sizeof TarEntry);
      memmove(&TarStreamEntry, &SrcStreamEntry, sizeof TarEntry);
      rc = ModifyDirectory(pVolInfo, ulSrcDirCluster, pDirSrcSHInfo,
         MODIFY_DIR_RENAME, &SrcEntry, &TarEntry, &SrcStreamEntry, &TarStreamEntry, pszTarEAName, 0);
      goto usMoveEASExit;
      }

   rc = ModifyDirectory(pVolInfo, ulSrcDirCluster, pDirSrcSHInfo, MODIFY_DIR_DELETE, &SrcEntry, NULL, &SrcStreamEntry, NULL, NULL, 0);
   if (rc)
      goto usMoveEASExit;

   rc = ModifyDirectory(pVolInfo, ulTarDirCluster, pDirTarSHInfo, MODIFY_DIR_INSERT, NULL, &SrcEntry, NULL, &SrcStreamEntry, pszTarEAName, 0);

usMoveEASExit:
   if (pszSrcEAName)
      free(pszSrcEAName);
   if (pszTarEAName)
      free(pszTarEAName);

   if (f32Parms.fMessageActive & LOG_EAS)
      Message("usMoveEAS for returned %d", rc);

   return rc;
}

/************************************************************************
*
************************************************************************/
USHORT MarkFileEAS(PVOLINFO pVolInfo, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo, PSZ pszFileName, BYTE fEAS)
{
ULONG ulCluster;
DIRENTRY OldEntry, NewEntry;
DIRENTRY1 OldEntryStream;
PDIRENTRY1 pNewEntry = (PDIRENTRY1)&NewEntry;
USHORT rc;


   Message("ulDirCluster=%lx, pszFileName=%s, pDirSHInfo=%lx", ulDirCluster, pszFileName, pDirSHInfo);
   ulCluster = FindPathCluster(pVolInfo, ulDirCluster, pszFileName, pDirSHInfo, &OldEntry, &OldEntryStream, NULL);
   if (ulCluster == pVolInfo->ulFatEof)
      {
      CritMessage("FAT32: MarkfileEAS : %s not found!", pszFileName);
      return ERROR_FILE_NOT_FOUND;
      }
   memcpy(&NewEntry, &OldEntry, sizeof (DIRENTRY));
#ifdef EXFAT
   if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
      {
#endif
      if( HAS_OLD_EAS( NewEntry.fEAS ))
           NewEntry.fEAS = FILE_HAS_NO_EAS;
      NewEntry.fEAS = ( BYTE )(( NewEntry.fEAS & FILE_HAS_WINNT_EXT ) | fEAS );
#ifdef EXFAT
      }
   else
      {
      if( HAS_OLD_EAS( pNewEntry->u.File.fEAS ))
           pNewEntry->u.File.fEAS = FILE_HAS_NO_EAS;
      pNewEntry->u.File.fEAS = ( BYTE )(( pNewEntry->u.File.fEAS & FILE_HAS_WINNT_EXT ) | fEAS );
      }
#endif

   if (!memcmp(&NewEntry, &OldEntry, sizeof (DIRENTRY)))
      return 0;

   rc = ModifyDirectory(pVolInfo, ulDirCluster, pDirSHInfo,
      MODIFY_DIR_UPDATE, &OldEntry, &NewEntry, &OldEntryStream, NULL, NULL, 0);

   return rc;
}

/************************************************************************
*
************************************************************************/
USHORT usReadEAS(PVOLINFO pVolInfo, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo, PSZ pszFileName, PFEALIST * ppFEAL, BOOL fCreate)
{
PFEALIST pFEAL;
ULONG ulCluster;
PBYTE  pszEAName;
PBYTE pRead;
USHORT rc;
DIRENTRY1 StreamEntry;
#ifdef EXFAT
SHOPENINFO SHInfo;
#endif
PSHOPENINFO pSHInfo = NULL;
USHORT usClustersUsed;
USHORT usBlocksUsed;
BOOL fFirst = TRUE;

   *ppFEAL = NULL;

   rc = GetEASName(pVolInfo, ulDirCluster, pszFileName, &pszEAName);
   if (rc)
      return rc;

   ulCluster = FindPathCluster(pVolInfo, ulDirCluster, pszEAName, pDirSHInfo, NULL, &StreamEntry, NULL);
   free(pszEAName);

   if ((ulCluster && ulCluster != pVolInfo->ulFatEof) || fCreate)
      {
      pFEAL = malloc((size_t)MAX_EA_SIZE);
      if (!pFEAL)
         return ERROR_NOT_ENOUGH_MEMORY;
      memset(pFEAL, 0, (size_t) MAX_EA_SIZE);
      pFEAL->cbList = sizeof (ULONG);
      }
   else
      pFEAL = NULL;

   if (!ulCluster || ulCluster == pVolInfo->ulFatEof)
      {
      *ppFEAL = pFEAL;
      return 0;
      }

   pRead = (PBYTE)pFEAL;
   if (f32Parms.fMessageActive & LOG_EAS)
      Message("usReadEAS: Reading (1) cluster %lu", ulCluster);

   rc = ReadBlock(pVolInfo, ulCluster, 0, pRead, 0);
   if (rc)
      {
      free(pFEAL);
      return rc;
      }
   if (pFEAL->cbList > MAX_EA_SIZE)
      {
      free(pFEAL);
      return ERROR_EAS_DIDNT_FIT;
      }

   usClustersUsed = (USHORT)(pFEAL->cbList / pVolInfo->ulClusterSize);
   if (pFEAL->cbList % pVolInfo->ulClusterSize)
      usClustersUsed++;

   usBlocksUsed = (USHORT)(pFEAL->cbList / pVolInfo->ulBlockSize);
   if (pFEAL->cbList % pVolInfo->ulBlockSize)
      usBlocksUsed++;

   /*
      vreemd: zonder deze Messages lijkt deze routine mis te gaan.
      Optimalisatie?
   */
   if (f32Parms.fMessageActive & LOG_EAS)
      Message("usReadEAS: %u clusters used", usClustersUsed);

   usBlocksUsed--;
   pRead += pVolInfo->ulBlockSize;

#ifdef EXFAT
   if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
      {
      pSHInfo = &SHInfo;
      SetSHInfo1(pVolInfo, &StreamEntry, pSHInfo);
      }
#endif

   while (usClustersUsed)
      {
      ULONG ulBlock;
      if (!fFirst)
         {
         ulCluster = GetNextCluster(pVolInfo, pSHInfo, ulCluster);
         if (!ulCluster)
            ulCluster = pVolInfo->ulFatEof;
         if (ulCluster == pVolInfo->ulFatEof)
            {
            free(pFEAL);
            return ERROR_EA_FILE_CORRUPT;
            }
         }
      for (ulBlock = 0; ulBlock < pVolInfo->ulClusterSize / pVolInfo->ulBlockSize; ulBlock++)
         {
         if (fFirst)
            {
            // skip the 1st block we've already read
            fFirst = FALSE;
            continue;
            }
      /*
         vreemd: zonder deze Messages lijkt deze routine mis te gaan.
         Optimalisatie?
      */
         if (f32Parms.fMessageActive & LOG_EAS)
            Message("usReadEAS: Reading (2) cluster %lu, block %lu", ulCluster, ulBlock);

         rc = ReadBlock(pVolInfo, ulCluster, ulBlock, pRead, 0);
         if (rc)
            {
            free(pFEAL);
            return rc;
            }
         pRead += pVolInfo->ulBlockSize;
         usBlocksUsed--;
         if (!usBlocksUsed)
            break;
         }
      if (!usBlocksUsed)
         break;
      usClustersUsed--;
      }
   *ppFEAL = pFEAL;

   return 0;
}

/************************************************************************
*
************************************************************************/
USHORT usDeleteEAS(PVOLINFO pVolInfo, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo, PSZ pszFileName)
{
PSZ pszEAName;
USHORT rc;
DIRENTRY DirEntry;
DIRENTRY1 StreamEntry;
ULONG    ulCluster;

   rc = GetEASName(pVolInfo, ulDirCluster, pszFileName, &pszEAName);
   if (rc)
      return rc;

   ulCluster = FindPathCluster(pVolInfo, ulDirCluster, pszEAName, pDirSHInfo, &DirEntry, &StreamEntry, NULL);
   if (ulCluster == pVolInfo->ulFatEof)
      {
      rc = 0;
      goto usDeleteEASExit;
      }
   rc = ModifyDirectory(pVolInfo, ulDirCluster, pDirSHInfo, MODIFY_DIR_DELETE, &DirEntry, NULL, &StreamEntry, NULL, NULL, 0);
   if (rc)
      goto usDeleteEASExit;

   if (ulCluster)
      DeleteFatChain(pVolInfo, ulCluster);

   rc = MarkFileEAS(pVolInfo, ulDirCluster, pDirSHInfo, pszFileName, FILE_HAS_NO_EAS);

usDeleteEASExit:
   free(pszEAName);

   if (f32Parms.fMessageActive & LOG_EAS)
      Message("usDeleteEAS for %s returned %d",
         pszFileName, rc);

   return rc;
}

/************************************************************************
*
************************************************************************/
USHORT usWriteEAS(PVOLINFO pVolInfo, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo, PSZ pszFileName, PFEALIST pFEAL)
{
ULONG ulCluster, ulNextCluster;
PBYTE  pszEAName;
PBYTE pWrite;
USHORT rc;
USHORT usClustersNeeded;
USHORT usBlocksNeeded;
DIRENTRY DirEntry;
DIRENTRY DirNew;
PDIRENTRY1 pDirNew = (PDIRENTRY1)&DirNew;
DIRENTRY1 DirStream;
DIRENTRY1 DirStreamNew;
BOOL     fCritical;
PFEA     pFea, pFeaEnd;
BOOL fFirst = TRUE;
#ifdef EXFAT
SHOPENINFO SHInfo;
#endif
PSHOPENINFO pSHInfo = NULL;

   if (pFEAL->cbList > MAX_EA_SIZE)
      return ERROR_EA_LIST_TOO_LONG;

   rc = GetEASName(pVolInfo, ulDirCluster, pszFileName, &pszEAName);
   if (rc)
      return rc;

   usClustersNeeded = (USHORT)((ULONG)pFEAL->cbList / pVolInfo->ulClusterSize);
   if (pFEAL->cbList % pVolInfo->ulClusterSize)
      usClustersNeeded++;

   usBlocksNeeded = (USHORT)((ULONG)pFEAL->cbList / pVolInfo->ulBlockSize);
   if (pFEAL->cbList % pVolInfo->ulBlockSize)
      usBlocksNeeded++;

   ulCluster = FindPathCluster(pVolInfo, ulDirCluster, pszEAName, pDirSHInfo, &DirEntry, &DirStream, NULL);
   if (!ulCluster || ulCluster == pVolInfo->ulFatEof)
      {
      BOOL fNew = FALSE;

      if (ulCluster == pVolInfo->ulFatEof)
         {
         fNew = TRUE;
         memset(&DirNew, 0, sizeof DirNew);
         memset(&DirStreamNew, 0, sizeof DirStreamNew);
         }
      else
         {
         memcpy(&DirNew, &DirEntry, sizeof DirEntry);
         memcpy(&DirStreamNew, &DirStream, sizeof DirStream);
         }

#ifdef EXFAT
      if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
         {
#endif
         DirNew.bAttr  = FILE_HIDDEN | FILE_SYSTEM | FILE_READONLY;
         DirNew.ulFileSize = pFEAL->cbList;
#ifdef EXFAT
         }
      else
         {
         pDirNew->u.File.usFileAttr  = FILE_HIDDEN | FILE_SYSTEM | FILE_READONLY;
         DirStreamNew.u.Stream.bAllocPossible = 1;
         DirStreamNew.u.Stream.bNoFatChain = 0;

#ifdef INCL_LONGLONG
         DirStreamNew.u.Stream.ullValidDataLen = (ULONGLONG)pFEAL->cbList;
         DirStreamNew.u.Stream.ullDataLen =
            (pFEAL->cbList / pVolInfo->ulClusterSize) * pVolInfo->ulClusterSize +
            (pFEAL->cbList % pVolInfo->ulClusterSize ? pVolInfo->ulClusterSize : 0);
#else
         AssignUL(&DirStreamNew.u.Stream.ullValidDataLen, pFEAL->cbList);
         AssignUL(&DirStreamNew.u.Stream.ullDataLen, 
            (pFEAL->cbList / pVolInfo->ulClusterSize) * pVolInfo->ulClusterSize +
            (pFEAL->cbList % pVolInfo->ulClusterSize ? pVolInfo->ulClusterSize : 0));
#endif
         }
#endif

#ifdef EXFAT
      if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
         {
         pSHInfo = &SHInfo;
         SetSHInfo1(pVolInfo, &DirStream, pSHInfo);
         }
#endif

      ulCluster = MakeFatChain(pVolInfo, pSHInfo, pVolInfo->ulFatEof, (ULONG)usClustersNeeded, NULL);
      if (ulCluster == pVolInfo->ulFatEof)
         {
         free(pszEAName);
         return ERROR_DISK_FULL;
         }

#ifdef EXFAT
      if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
         {
#endif
         DirNew.wCluster = LOUSHORT(ulCluster);
         DirNew.wClusterHigh = HIUSHORT(ulCluster);
#ifdef EXFAT
         }
      else
         {
         DirStreamNew.u.Stream.ulFirstClus = ulCluster;
         }
#endif

      if (fNew)
         rc = MakeDirEntry(pVolInfo, ulDirCluster, pDirSHInfo, &DirNew, &DirStreamNew, pszEAName);
      else
         rc = ModifyDirectory(pVolInfo, ulDirCluster, pDirSHInfo,
            MODIFY_DIR_UPDATE, &DirEntry, &DirNew, &DirStream, &DirStreamNew, NULL, 0);
      if (rc)
         {
         free(pszEAName);
         return rc;
         }
      }
   else
      {
      memcpy(&DirNew, &DirEntry, sizeof (DIRENTRY));
      memcpy(&DirStreamNew, &DirStream, sizeof (DIRENTRY1));
#ifdef EXFAT
      if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
#endif
         DirNew.ulFileSize = pFEAL->cbList;
#ifdef EXFAT
      else
         {
         pDirNew->bEntryType = ENTRY_TYPE_FILE;
         DirStreamNew.bEntryType = ENTRY_TYPE_STREAM_EXT;
         DirStreamNew.u.Stream.bAllocPossible = 1;
         DirStreamNew.u.Stream.bNoFatChain = 0;
#ifdef INCL_LONGLONG
         DirStreamNew.u.Stream.ullValidDataLen = (ULONGLONG)pFEAL->cbList;
         DirStreamNew.u.Stream.ullDataLen =
            (pFEAL->cbList / pVolInfo->ulClusterSize) * pVolInfo->ulClusterSize +
            (pFEAL->cbList % pVolInfo->ulClusterSize ? pVolInfo->ulClusterSize : 0);
#else
         AssignUL(&DirStreamNew.u.Stream.ullValidDataLen, pFEAL->cbList);
         AssignUL(&DirStreamNew.u.Stream.ullDataLen, 
            (pFEAL->cbList / pVolInfo->ulClusterSize) * pVolInfo->ulClusterSize +
            (pFEAL->cbList % pVolInfo->ulClusterSize ? pVolInfo->ulClusterSize : 0));
#endif
         }
#endif
      rc = ModifyDirectory(pVolInfo, ulDirCluster, pDirSHInfo, MODIFY_DIR_UPDATE,
           &DirEntry, &DirNew, &DirStream, &DirStreamNew, NULL, 0);
      if (rc)
         {
         free(pszEAName);
         return rc;
         }
      }

   free(pszEAName);

#ifdef EXFAT
   if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
      {
      pSHInfo = &SHInfo;
      SetSHInfo1(pVolInfo, (PDIRENTRY1)&DirStreamNew, pSHInfo);
      }
#endif

   pWrite = (PBYTE)pFEAL;
   ulNextCluster = pVolInfo->ulFatEof;
   while (usClustersNeeded)
      {
      ULONG ulBlock;
      ulNextCluster = GetNextCluster(pVolInfo, pSHInfo, ulCluster);
      if (!ulNextCluster)
         ulNextCluster = pVolInfo->ulFatEof;
      for (ulBlock = 0; ulBlock < pVolInfo->ulClusterSize / pVolInfo->ulBlockSize; ulBlock++)
         {
         rc = WriteBlock(pVolInfo, ulCluster, ulBlock, pWrite, 0);
         if (rc)
            return rc;
         pWrite += pVolInfo->ulBlockSize;
         }
      usClustersNeeded --;

      if (usClustersNeeded)
         {
         if (ulNextCluster == pVolInfo->ulFatEof)
            ulCluster = MakeFatChain(pVolInfo, pSHInfo, ulCluster, (ULONG)usClustersNeeded, NULL);
         else
            ulCluster = ulNextCluster;
         if (ulCluster == pVolInfo->ulFatEof)
            return ERROR_DISK_FULL;
         }
      }
   if (ulNextCluster != pVolInfo->ulFatEof)
      {
      SetNextCluster(pVolInfo, ulCluster, pVolInfo->ulFatEof);
      DeleteFatChain(pVolInfo, ulNextCluster);
      }

   pFea = pFEAL->list;
   pFeaEnd = (PFEA)((PBYTE)pFEAL + pFEAL->cbList);
   fCritical = FALSE;
   while (pFea < pFeaEnd)
      {
      if (pFea->fEA & FEA_NEEDEA)
         fCritical = TRUE;
      pFea = (PFEA)((PBYTE)pFea + sizeof (FEA) + (USHORT)pFea->cbName + 1 + pFea->cbValue);
      }


   if (fCritical)
      rc = MarkFileEAS(pVolInfo, ulDirCluster, pDirSHInfo, pszFileName, FILE_HAS_CRITICAL_EAS);
   else
      rc = MarkFileEAS(pVolInfo, ulDirCluster, pDirSHInfo, pszFileName, FILE_HAS_EAS);

   return rc;
}


/************************************************************************
*
************************************************************************/
PFEA FindEA(PFEALIST pFeal, PSZ pszName, USHORT usMaxName)
{
PFEA pFea;
PBYTE pMax;

   if (!pFeal)
      return NULL;

   pFea = pFeal->list;
   pMax = (PBYTE)pFeal + pFeal->cbList;

   while ((PBYTE)pFea + sizeof (FEA) < pMax)
      {
      PBYTE pName, pValue;

      pName  = (PBYTE)(pFea + 1);
      if (pName >= pMax)
         return NULL;
      pValue = pName + (USHORT)pFea->cbName + 1;
      if (pValue + pFea->cbValue > pMax)
         return NULL;
#if 0
      if (f32Parms.fMessageActive & LOG_EAS)
         Message("FindEA: '%s'", pName);
#endif
      if (pFea->cbName == (BYTE)usMaxName && !memicmp(pName, pszName, usMaxName))
         return pFea;

      pFea = (PFEA)((PBYTE)pFea + sizeof (FEA) + (USHORT)pFea->cbName + 1 + pFea->cbValue);
      }
   return NULL;
}

USHORT GetEASName(PVOLINFO pVolInfo, ULONG ulDirCluster, PSZ pszFileName, PSZ * pszEASName)
{
   if (strlen(pszFileName) > FAT32MAXPATH - 4)
      return ERROR_FILENAME_EXCED_RANGE;

   *pszEASName = malloc(FAT32MAXPATH);
   if (!(*pszEASName))
      return ERROR_NOT_ENOUGH_MEMORY;

   if( TranslateName( pVolInfo, ulDirCluster, pszFileName, *pszEASName, TRANSLATE_SHORT_TO_LONG ))
       strcpy(*pszEASName, pszFileName);

   strcat(*pszEASName, EA_EXTENTION);
   return 0;
}

BOOL IsEASFile(PSZ pszFileName)
{
USHORT usExtLen = strlen(EA_EXTENTION);

   if (strlen(pszFileName) > usExtLen)
      {
      if (!stricmp(pszFileName + (strlen(pszFileName) - usExtLen), EA_EXTENTION))
         return TRUE;
      }
   return FALSE;
}

USHORT usGetEmptyEAS(PSZ pszFileName, PEAOP pEAOP)
{
   USHORT rc;

   PFEALIST pTarFeal;
   USHORT usMaxSize;
   PFEA     pCurrFea;

   PGEALIST pGeaList;
   PGEA     pCurrGea;

   ULONG    ulGeaSize;
   ULONG    ulFeaSize;
   ULONG    ulCurrFeaLen;
   ULONG    ulCurrGeaLen;

   if (f32Parms.fMessageActive & LOG_EAS)
      Message("usGetEmptyEAS for %s with pEAOP %lX", pszFileName,pEAOP);

   /*
      Checking all the arguments
   */

   rc = MY_PROBEBUF(PB_OPWRITE, (PBYTE)pEAOP, sizeof (EAOP));
   if (rc)
      {
      Message("Protection violation in usGetEmptyEAS (1) at %lX", pEAOP);
      return rc;
      }

   pTarFeal = pEAOP->fpFEAList;
   rc = MY_PROBEBUF(PB_OPREAD, (PBYTE)pTarFeal, sizeof (ULONG));
   if (rc)
      {
      Message("Protection violation in usGetEmptyEAS (2) at %lX", pTarFeal);
      return rc;
      }
   if (pTarFeal->cbList > MAX_EA_SIZE)
      usMaxSize = (USHORT)MAX_EA_SIZE;
   else
      usMaxSize = (USHORT)pTarFeal->cbList;

   if (usMaxSize < sizeof (ULONG))
      return ERROR_BUFFER_OVERFLOW;

   rc = MY_PROBEBUF(PB_OPWRITE, (PBYTE)pTarFeal, (USHORT)usMaxSize);
   if (rc)
      return rc;

   pGeaList = pEAOP->fpGEAList;
   rc = MY_PROBEBUF(PB_OPREAD, (PBYTE)pGeaList, sizeof (ULONG));
   if (rc)
      return rc;
   if (pGeaList->cbList > MAX_EA_SIZE)
      return ERROR_EA_LIST_TOO_LONG;
   rc = MY_PROBEBUF(PB_OPREAD, (PBYTE)pGeaList, (USHORT)pGeaList->cbList);
   if (rc)
      return rc;

   ulFeaSize = sizeof(pTarFeal->cbList);
   ulGeaSize = sizeof(pGeaList->cbList);

   pCurrGea = pGeaList->list;
   pCurrFea = pTarFeal->list;
   while(ulGeaSize < pGeaList->cbList)
      {
      ulFeaSize += sizeof(FEA) + pCurrGea->cbName + 1;
      ulCurrGeaLen = sizeof(GEA) + pCurrGea->cbName;
      pCurrGea = (PGEA)((PBYTE)pCurrGea + ulCurrGeaLen);
      ulGeaSize += ulCurrGeaLen;
      }

   if (ulFeaSize > usMaxSize)
      {
      /* this is what HPFS.IFS returns */
      /* when a file does not have any EAs */
      pTarFeal->cbList = 0xEF;
      rc = ERROR_EAS_DIDNT_FIT;
      }
   else
      {
       /* since we DO copy something to */
       /* FEALIST, we have to set the complete */
       /* size of the resulting FEALIST in the */
       /* length field */
       pTarFeal->cbList = ulFeaSize;
       ulGeaSize = sizeof(pGeaList->cbList);
       pCurrGea = pGeaList->list;
       pCurrFea = pTarFeal->list;
       /* copy the EA names requested to the FEA area */
       /* even if any values cannot be returned       */
       while (ulGeaSize < pGeaList->cbList)
          {
          pCurrFea->fEA     = 0;
          strcpy((PBYTE)(pCurrFea+1),pCurrGea->szName);
          pCurrFea->cbName  = (BYTE)strlen(pCurrGea->szName);
          pCurrFea->cbValue = 0;

          ulCurrFeaLen = sizeof(FEA) + pCurrFea->cbName + 1;
          pCurrFea = (PFEA)((PBYTE)pCurrFea + ulCurrFeaLen);

          ulCurrGeaLen = sizeof(GEA) + pCurrGea->cbName;
          pCurrGea = (PGEA)((PBYTE)pCurrGea + ulCurrGeaLen);
          ulGeaSize += ulCurrGeaLen;
          }
       rc = 0;
       }
   return rc;
}
