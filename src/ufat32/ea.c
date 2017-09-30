#include <malloc.h>
#include <string.h>

#include "fat32c.h"

#define MODIFY_DIR_INSERT 0
#define MODIFY_DIR_DELETE 1
#define MODIFY_DIR_UPDATE 2
#define MODIFY_DIR_RENAME 3

ULONG FindPathCluster(PCDINFO pCD, ULONG ulCluster, PSZ pszPath, PSHOPENINFO pSHInfo,
                      PDIRENTRY pDirEntry, PDIRENTRY1 pDirEntryStream, PSZ pszFullName);
APIRET ModifyDirectory(PCDINFO pCD, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo,
                       USHORT usMode, PDIRENTRY pOld, PDIRENTRY pNew,
                       PDIRENTRY1 pStreamOld, PDIRENTRY1 pStreamNew, PSZ pszLongNameOld, PSZ pszLongNameNew);
ULONG ReadCluster(PCDINFO pCD, ULONG ulCluster, PVOID pbCluster);
ULONG WriteCluster(PCDINFO pCD, ULONG ulCluster, PVOID pbCluster);
void SetSHInfo1(PCDINFO pCD, PDIRENTRY1 pStreamEntry, PSHOPENINFO pSHInfo);
ULONG GetNextCluster(PCDINFO pCD, PSHOPENINFO pSHInfo, ULONG ulCluster, BOOL fAllowBad);
APIRET SetNextCluster(PCDINFO pCD, ULONG ulCluster, ULONG ulNext);
USHORT MakeDirEntry(PCDINFO pCD, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo,
                    PDIRENTRY pNew, PDIRENTRY1 pNewStream, PSZ pszName);
BOOL DeleteFatChain(PCDINFO pCD, ULONG ulCluster);
ULONG MakeFatChain(PCDINFO pCD, PSHOPENINFO pSHInfo, ULONG ulPrevCluster, ULONG ulClustersRequested, PULONG pulLast);
PFEA FindEA(PFEALIST pFeal, PSZ pszName, USHORT usMaxName);
USHORT usModifyEAS(PCDINFO pCD, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo, PSZ pszFileName, PEAOP pEAOP);
USHORT usReadEAS(PCDINFO pCD, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo, PSZ pszFileName, PFEALIST * ppFEAL, BOOL fCreate);
USHORT usDeleteEAS(PCDINFO pCD, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo, PSZ pszFileName);
USHORT usWriteEAS(PCDINFO pCD, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo, PSZ pszFileName, PFEALIST pFEAL);
USHORT MarkFileEAS(PCDINFO pCD, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo, PSZ pszFileName, BYTE fEAS);
USHORT usGetEAS(PCDINFO pCD, USHORT usLevel, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo, PSZ pszFileName, PEAOP pEAOP);
USHORT GetEASName(PCDINFO pCD, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo, PSZ pszFileName, PSZ * pszEASName);


/************************************************************************
*
************************************************************************/
USHORT usModifyEAS(PCDINFO pCD, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo, PSZ pszFileName, PEAOP pEAOP)
{
USHORT rc;
PFEALIST pTarFeal;
PFEALIST pSrcFeal;
PBYTE    pSrcMax;
PFEA pSrcFea;
PFEA pTarFea;

   //MessageL(LOG_EAS, "usModifyEAS%m for %s", 0x005a, pszFileName);

   /*
      Do not allow ea's file files with no filename (root)
   */
   if (!strlen(pszFileName))
      return 284;

   //rc = MY_PROBEBUF(PB_OPWRITE, (PBYTE)pEAOP, sizeof (EAOP));
   //if (rc)
   //   {
   //   Message("Protection violation in usModifyEAS (1) at %lX", pEAOP);
   //   return rc;
   //   }

   pSrcFeal = pEAOP->fpFEAList;
   if (pSrcFeal->cbList > MAX_EA_SIZE)
      return ERROR_EA_LIST_TOO_LONG;

   //rc = MY_PROBEBUF(PB_OPREAD, (PBYTE)pSrcFeal, (USHORT)pSrcFeal->cbList);
   //if (rc)
   //   {
   //   Message("Protection violation in usModifyEAS (2) at %lX", pSrcFeal);
   //   return rc;
   //   }

   if (pSrcFeal->cbList <= sizeof (ULONG))
      return 0;

   rc = usReadEAS(pCD, ulDirCluster, pDirSHInfo, pszFileName, &pTarFeal, TRUE);
   if (rc)
      return rc;

   //MessageL(LOG_EAS, "cbList before%m = %lu", 0x4036, pTarFeal->cbList);

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

      //rc = FSH_CHECKEANAME(0x0001, pSrcFea->cbName, pName);
      //if (rc && pSrcFea->cbValue)
      //   {
      //   pEAOP->oError = (PBYTE)pSrcFea - (PBYTE)pEAOP;
      //   goto usStoreEASExit;
      //   }

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

         //MessageL(LOG_EAS, "Inserting EA%m '%s' (%u,%u)", 0x4037, pName,
         //         pSrcFea->cbName, pSrcFea->cbValue);
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

         //MessageL(LOG_EAS, "Updating EA%m '%s' (%u,%u)", 0x4038, pName,
         //         pSrcFea->cbName, pSrcFea->cbValue);
         }

      usNewSize = sizeof (FEA) + (USHORT)pSrcFea->cbName + 1 + pSrcFea->cbValue;
      pSrcFea = (PFEA)((PBYTE)pSrcFea + usNewSize);
      }

   //MessageL(LOG_EAS, "cbList after%m = %lu", 0x4039, pTarFeal->cbList);

   if (pTarFeal->cbList > 4)
      rc = usWriteEAS(pCD, ulDirCluster, pDirSHInfo, pszFileName, pTarFeal);
   else
      rc = usDeleteEAS(pCD, ulDirCluster, pDirSHInfo, pszFileName);

usStoreEASExit:
   free(pTarFeal);

   //MessageL(LOG_EAS, "usModifyEAS%m for %s returned %d",
   //         0x805a, pszFileName, rc);

   return rc;
}

/************************************************************************
*
************************************************************************/
USHORT usGetEAS(PCDINFO pCD, USHORT usLevel, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo, PSZ pszFileName, PEAOP pEAOP)
{
USHORT rc;
PFEALIST pTarFeal;
PFEALIST pSrcFeal;
PFEA     pSrcFea;
PFEA     pTarFea;
PGEALIST pGeaList;
USHORT   usMaxSize;

   //MessageL(LOG_EAS, "usGetEAS%m for %s Level %d", 0x005c, pszFileName, usLevel);

   /*
      Checking all the arguments
   */

   //rc = MY_PROBEBUF(PB_OPWRITE, (PBYTE)pEAOP, sizeof (EAOP));
   //if (rc)
   //   {
   //   Message("Protection violation in usGetEAS (1) at %lX", pEAOP);
   //   return rc;
   //   }

   pTarFeal = pEAOP->fpFEAList;
   //rc = MY_PROBEBUF(PB_OPREAD, (PBYTE)pTarFeal, sizeof (ULONG));
   //if (rc)
   //   {
   //   Message("Protection violation in usGetEAS (2) at %lX", pTarFeal);
   //   return rc;
   //   }
   if (pTarFeal->cbList > MAX_EA_SIZE)
      usMaxSize = (USHORT)MAX_EA_SIZE;
   else
      usMaxSize = (USHORT)pTarFeal->cbList;

   if (usMaxSize < sizeof (ULONG))
      return ERROR_BUFFER_OVERFLOW;

   //rc = MY_PROBEBUF(PB_OPWRITE, (PBYTE)pTarFeal, (USHORT)usMaxSize);
   //if (rc)
   //   return rc;

   if (usLevel == FIL_QUERYEASFROMLIST || usLevel == FIL_QUERYEASFROMLISTL)
      {
      pGeaList = pEAOP->fpGEAList;
      //rc = MY_PROBEBUF(PB_OPREAD, (PBYTE)pGeaList, sizeof (ULONG));
      //if (rc)
      //   return rc;
      if (pGeaList->cbList > MAX_EA_SIZE)
         return ERROR_EA_LIST_TOO_LONG;
      //rc = MY_PROBEBUF(PB_OPREAD, (PBYTE)pGeaList, (USHORT)pGeaList->cbList);
      //if (rc)
      //   return rc;
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

   rc = usReadEAS(pCD, ulDirCluster, pDirSHInfo, pszFileName, &pSrcFeal, FALSE);
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
            //MessageL(LOG_EAS, "Found%m %s", 0x403c, pSrcFea + 1);
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

            //MessageL(LOG_EAS, "usGetEAS%m: %s not found!", 0x403d, pGea->szName);

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
         //MessageL(LOG_EAS, "Found%m %s (%u,%u)", 0x403e, pSrcFea + 1, (USHORT)pSrcFea->cbName, pSrcFea->cbValue);
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

   //MessageL(LOG_EAS, "usGetEAS%m for %s returned %d (%lu bytes in EAS)",
   //         0x805c, pszFileName, rc, pTarFeal->cbList);

   return rc;
}

/************************************************************************
*
************************************************************************/
USHORT MarkFileEAS(PCDINFO pCD, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo, PSZ pszFileName, BYTE fEAS)
{
ULONG ulCluster;
PDIRENTRY pOldEntry, pNewEntry;
PDIRENTRY1 pOldEntryStream = NULL;
USHORT rc;

   pOldEntry = (PDIRENTRY)malloc((size_t)sizeof(DIRENTRY));
   if (!pOldEntry)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto MarkFileEASExit;
      }
   pNewEntry = (PDIRENTRY)malloc((size_t)sizeof(DIRENTRY));
   if (!pNewEntry)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto MarkFileEASExit;
      }
#ifdef EXFAT
   pOldEntryStream = (PDIRENTRY1)malloc((size_t)sizeof(DIRENTRY1));
   if (!pOldEntryStream)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto MarkFileEASExit;
      }
#endif

   ulCluster = FindPathCluster(pCD, ulDirCluster, pszFileName, pDirSHInfo, pOldEntry, pOldEntryStream, NULL);
   if (ulCluster == pCD->ulFatEof)
      {
      rc = ERROR_FILE_NOT_FOUND;
      goto MarkFileEASExit;
      }
   memcpy(pNewEntry, pOldEntry, sizeof (DIRENTRY));
#ifdef EXFAT
   if (pCD->bFatType < FAT_TYPE_EXFAT)
      {
#endif
      if( HAS_OLD_EAS( pNewEntry->fEAS ))
           pNewEntry->fEAS &= FILE_NO_EAS_MASK;
      pNewEntry->fEAS = ( BYTE )(( pNewEntry->fEAS & FILE_NO_EAS_MASK ) | fEAS );
      //pNewEntry->fEAS = ( BYTE )(( pNewEntry->fEAS & FILE_HAS_WINNT_EXT ) | fEAS );
#ifdef EXFAT
      }
   else
      {
      PDIRENTRY1 pNewEntry1 = (PDIRENTRY1)pNewEntry;
      if( HAS_OLD_EAS( pNewEntry1->u.File.fEAS ) )
         {
         pNewEntry1->u.File.fEAS &= FILE_NO_EAS_MASK;
         }
      pNewEntry1->u.File.fEAS = ( BYTE )(( pNewEntry1->u.File.fEAS & FILE_NO_EAS_MASK ) | fEAS );
      //pNewEntry1->u.File.fEAS = ( BYTE )(( pNewEntry1->u.File.fEAS & FILE_HAS_WINNT_EXT ) | fEAS );
      }
#endif

   if (!memcmp(pNewEntry, pOldEntry, sizeof (DIRENTRY)))
      {
      rc = 0;
      goto MarkFileEASExit;
      }

   rc = ModifyDirectory(pCD, ulDirCluster, pDirSHInfo,
      MODIFY_DIR_UPDATE, pOldEntry, pNewEntry, pOldEntryStream, NULL, pszFileName, NULL);

MarkFileEASExit:
   if (pOldEntry)
      free(pOldEntry);
   if (pNewEntry)
      free(pNewEntry);
#ifdef EXFAT
   if (pOldEntryStream)
      free(pOldEntryStream);
#endif

   return rc;
}

/************************************************************************
*
************************************************************************/
USHORT usReadEAS(PCDINFO pCD, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo, PSZ pszFileName, PFEALIST * ppFEAL, BOOL fCreate)
{
PFEALIST pFEAL;
ULONG ulCluster;
PBYTE  pszEAName;
PBYTE pRead;
USHORT rc = 0;
PDIRENTRY1 pStreamEntry = NULL;
PSHOPENINFO pSHInfo = NULL;
USHORT usClustersUsed;
USHORT usBlocksUsed;
BOOL fFirst = TRUE;

#ifdef EXFAT
   pStreamEntry = (PDIRENTRY1)malloc((size_t)sizeof(DIRENTRY1));
   if (!pStreamEntry)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto usReadEASExit;
      }
   pSHInfo = (PSHOPENINFO)malloc((size_t)sizeof(SHOPENINFO));
   if (!pSHInfo)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto usReadEASExit;
      }
#endif

   *ppFEAL = NULL;

   rc = GetEASName(pCD, ulDirCluster, pDirSHInfo, pszFileName, &pszEAName);
   if (rc)
      goto usReadEASExit;

   ulCluster = FindPathCluster(pCD, ulDirCluster, pszEAName, pDirSHInfo, NULL, pStreamEntry, NULL);
   free(pszEAName);

   if ((ulCluster && ulCluster != pCD->ulFatEof) || fCreate)
      {
      pFEAL = malloc((size_t)MAX_EA_SIZE);
      if (!pFEAL)
         {
         rc = ERROR_NOT_ENOUGH_MEMORY;
         goto usReadEASExit;
         }
      memset(pFEAL, 0, (size_t) MAX_EA_SIZE);
      pFEAL->cbList = sizeof (ULONG);
      }
   else
      pFEAL = NULL;

   if (!ulCluster || ulCluster == pCD->ulFatEof)
      {
      *ppFEAL = pFEAL;
      rc = 0;
      goto usReadEASExit;
      }

   pRead = (PBYTE)pFEAL;
   //MessageL(LOG_EAS, "usReadEAS%m: Reading (1) cluster %lu", 0x4041, ulCluster);

   rc = ReadCluster(pCD, ulCluster, pRead);
   if (rc)
      {
      free(pFEAL);
      goto usReadEASExit;
      }
   if (pFEAL->cbList > MAX_EA_SIZE)
      {
      free(pFEAL);
      rc = ERROR_EAS_DIDNT_FIT;
      goto usReadEASExit;
      }

   usClustersUsed = (USHORT)(pFEAL->cbList / pCD->ulClusterSize);
   //if (pFEAL->cbList % pCD->ulClusterSize)
   //   usClustersUsed++;

   usBlocksUsed--;
   pRead += pCD->ulClusterSize;

#ifdef EXFAT
   if (pCD->bFatType == FAT_TYPE_EXFAT)
      {
      SetSHInfo1(pCD, pStreamEntry, pSHInfo);
      }
#endif

   while (usClustersUsed)
      {
      ulCluster = GetNextCluster(pCD, pSHInfo, ulCluster, FALSE);
      if (!ulCluster)
         ulCluster = pCD->ulFatEof;
      if (ulCluster == pCD->ulFatEof)
         {
         free(pFEAL);
         return ERROR_EA_FILE_CORRUPT;
         }
   /*
      vreemd: zonder deze Messages lijkt deze routine mis te gaan.
      Optimalisatie?
   */
      rc = ReadCluster(pCD, ulCluster, pRead);
      if (rc)
         {
         free(pFEAL);
         return rc;
         }
      usClustersUsed--;
      pRead += pCD->ulClusterSize;
      }
   *ppFEAL = pFEAL;

usReadEASExit:
#ifdef EXFAT
   if (pStreamEntry)
      free(pStreamEntry);
   if (pSHInfo)
      free(pSHInfo);
#endif
   return rc;
}

/************************************************************************
*
************************************************************************/
USHORT usDeleteEAS(PCDINFO pCD, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo, PSZ pszFileName)
{
PSZ pszEAName;
USHORT rc;
PDIRENTRY pDirEntry;
PDIRENTRY1 pStreamEntry = NULL;
ULONG    ulCluster;

   pDirEntry = (PDIRENTRY)malloc((size_t)sizeof(DIRENTRY));
   if (!pDirEntry)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto usDeleteEASExit;
      }
#ifdef EXFAT
   pStreamEntry = (PDIRENTRY1)malloc((size_t)sizeof(DIRENTRY1));
   if (!pStreamEntry)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto usDeleteEASExit;
      }
#endif

   rc = GetEASName(pCD, ulDirCluster, pDirSHInfo, pszFileName, &pszEAName);
   if (rc)
      return rc;

   ulCluster = FindPathCluster(pCD, ulDirCluster, pszEAName, pDirSHInfo, pDirEntry, pStreamEntry, NULL);
   if (ulCluster == pCD->ulFatEof)
      {
      rc = 0;
      goto usDeleteEASExit;
      }
   rc = ModifyDirectory(pCD, ulDirCluster, pDirSHInfo, MODIFY_DIR_DELETE, pDirEntry, NULL, pStreamEntry, NULL, pszEAName, NULL);
   if (rc)
      goto usDeleteEASExit;

   if (ulCluster)
      DeleteFatChain(pCD, ulCluster);

   rc = MarkFileEAS(pCD, ulDirCluster, pDirSHInfo, pszFileName, FILE_HAS_NO_EAS);

usDeleteEASExit:
   if (pszEAName)
      free(pszEAName);
   if (pDirEntry)
      free(pDirEntry);
#ifdef EXFAT
   if (pStreamEntry)
      free(pStreamEntry);
#endif

   //MessageL(LOG_EAS, "usDeleteEAS%m for %s returned %d",
   //         0x805f, pszFileName, rc);

   return rc;
}

/************************************************************************
*
************************************************************************/
USHORT usWriteEAS(PCDINFO pCD, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo, PSZ pszFileName, PFEALIST pFEAL)
{
ULONG ulCluster, ulNextCluster;
PBYTE  pszEAName;
PBYTE pWrite;
USHORT rc;
USHORT usClustersNeeded;
USHORT usBlocksNeeded;
PDIRENTRY pDirEntry;
PDIRENTRY pDirNew;
PDIRENTRY1 pDirStream = NULL;
PDIRENTRY1 pDirStreamNew = NULL;
BOOL     fCritical;
PFEA     pFea, pFeaEnd;
BOOL fFirst = TRUE;
PSHOPENINFO pSHInfo = NULL;

   if (pFEAL->cbList > MAX_EA_SIZE)
      return ERROR_EA_LIST_TOO_LONG;

   pDirEntry = (PDIRENTRY)malloc((size_t)sizeof(DIRENTRY));
   if (!pDirEntry)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto usWriteEASExit;
      }
   pDirNew = (PDIRENTRY)malloc((size_t)sizeof(DIRENTRY));
   if (!pDirNew)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto usWriteEASExit;
      }
#ifdef EXFAT
   pDirStream = (PDIRENTRY1)malloc((size_t)sizeof(DIRENTRY1));
   if (!pDirStream)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto usWriteEASExit;
      }
   pDirStreamNew = (PDIRENTRY1)malloc((size_t)sizeof(DIRENTRY1));
   if (!pDirStreamNew)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto usWriteEASExit;
      }
   pSHInfo = (PSHOPENINFO)malloc((size_t)sizeof(SHOPENINFO));
   if (!pSHInfo)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto usWriteEASExit;
      }
#endif

   rc = GetEASName(pCD, ulDirCluster, pDirSHInfo, pszFileName, &pszEAName);
   if (rc)
      goto usWriteEASExit;

   usClustersNeeded = (USHORT)((ULONG)pFEAL->cbList / pCD->ulClusterSize);
   if (pFEAL->cbList % pCD->ulClusterSize)
      usClustersNeeded++;

   ulCluster = FindPathCluster(pCD, ulDirCluster, pszEAName, pDirSHInfo, pDirEntry, pDirStream, NULL);
   if (!ulCluster || ulCluster == pCD->ulFatEof)
      {
      BOOL fNew = FALSE;

      if (ulCluster == pCD->ulFatEof)
         {
         fNew = TRUE;
         memset(pDirNew, 0, sizeof(DIRENTRY));
#ifdef EXFAT
         memset(pDirStreamNew, 0, sizeof(DIRENTRY1));
#endif
         }
      else
         {
         memcpy(pDirNew, pDirEntry, sizeof(DIRENTRY));
#ifdef EXFAT
         memcpy(pDirStreamNew, pDirStream, sizeof(DIRENTRY1));
#endif
         }

#ifdef EXFAT
      if (pCD->bFatType < FAT_TYPE_EXFAT)
         {
#endif
         pDirNew->bAttr  = FILE_HIDDEN | FILE_SYSTEM | FILE_READONLY;
         pDirNew->ulFileSize = pFEAL->cbList;
#ifdef EXFAT
         }
      else
         {
         PDIRENTRY1 pDirNew1 = (PDIRENTRY1)pDirNew;
         pDirNew1->u.File.usFileAttr  = FILE_HIDDEN | FILE_SYSTEM | FILE_READONLY;
         pDirStreamNew->u.Stream.bAllocPossible = 1;
         pDirStreamNew->u.Stream.bNoFatChain = 0;

#ifdef INCL_LONGLONG
         pDirStreamNew->u.Stream.ullValidDataLen = (ULONGLONG)pFEAL->cbList;
         pDirStreamNew->u.Stream.ullDataLen = pDirStreamNew->u.Stream.ullValidDataLen;
         //   (pFEAL->cbList / pCD->ulClusterSize) * pCD->ulClusterSize +
         //   (pFEAL->cbList % pCD->ulClusterSize ? pCD->ulClusterSize : 0);
#else
         AssignUL(pDirStreamNew->u.Stream.ullValidDataLen, pFEAL->cbList);
         AssignUL(pDirStreamNew->u.Stream.ullDataLen, pFEAL->cbList);
         //   (pFEAL->cbList / pCD->ulClusterSize) * pCD->ulClusterSize +
         //   (pFEAL->cbList % pCD->ulClusterSize ? pCD->ulClusterSize : 0));
#endif
         }
#endif

#ifdef EXFAT
      if (pCD->bFatType == FAT_TYPE_EXFAT)
         {
         SetSHInfo1(pCD, pDirStream, pSHInfo);
         }
#endif

      ulCluster = MakeFatChain(pCD, pSHInfo, pCD->ulFatEof, (ULONG)usClustersNeeded, NULL);
      if (ulCluster == pCD->ulFatEof)
         {
         free(pszEAName);
         rc = ERROR_DISK_FULL;
         goto usWriteEASExit;
         }

#ifdef EXFAT
      if (pCD->bFatType < FAT_TYPE_EXFAT)
         {
#endif
         pDirNew->wCluster = LOUSHORT(ulCluster);
         pDirNew->wClusterHigh = HIUSHORT(ulCluster);
#ifdef EXFAT
         }
      else
         {
         pDirStreamNew->u.Stream.ulFirstClus = ulCluster;
         }
#endif

      if (fNew)
         rc = MakeDirEntry(pCD, ulDirCluster, pDirSHInfo, pDirNew, pDirStreamNew, pszEAName);
      else
         rc = ModifyDirectory(pCD, ulDirCluster, pDirSHInfo,
            MODIFY_DIR_UPDATE, pDirEntry, pDirNew, pDirStream, pDirStreamNew, pszEAName, NULL);
      if (rc)
         {
         free(pszEAName);
         goto usWriteEASExit;
         }
      }
   else
      {
      memcpy(pDirNew, pDirEntry, sizeof(DIRENTRY));
#ifdef EXFAT
      memcpy(pDirStreamNew, pDirStream, sizeof(DIRENTRY1));
      if (pCD->bFatType < FAT_TYPE_EXFAT)
#endif
         pDirNew->ulFileSize = pFEAL->cbList;
#ifdef EXFAT
      else
         {
         PDIRENTRY1 pDirNew1 = (PDIRENTRY1)pDirNew;
         //pDirNew1->bEntryType = ENTRY_TYPE_FILE;
         //pDirStreamNew->bEntryType = ENTRY_TYPE_STREAM_EXT;
         pDirStreamNew->u.Stream.bAllocPossible = 1;
         pDirStreamNew->u.Stream.bNoFatChain = 0;
#ifdef INCL_LONGLONG
         pDirStreamNew->u.Stream.ullValidDataLen = (ULONGLONG)pFEAL->cbList;
         pDirStreamNew->u.Stream.ullDataLen = (ULONGLONG)pFEAL->cbList;
         //   (pFEAL->cbList / pCD->ulClusterSize) * pCD->ulClusterSize +
         //   (pFEAL->cbList % pCD->ulClusterSize ? pCD->ulClusterSize : 0);
#else
         AssignUL(pDirStreamNew->u.Stream.ullValidDataLen, pFEAL->cbList);
         AssignUL(pDirStreamNew->u.Stream.ullDataLen, pFEAL->cbList);
         //   (pFEAL->cbList / pCD->ulClusterSize) * pCD->ulClusterSize +
         //   (pFEAL->cbList % pCD->ulClusterSize ? pCD->ulClusterSize : 0));
#endif
         }
#endif
      rc = ModifyDirectory(pCD, ulDirCluster, pDirSHInfo, MODIFY_DIR_UPDATE,
           pDirEntry, pDirNew, pDirStream, pDirStreamNew, pszEAName, NULL);
      if (rc)
         {
         free(pszEAName);
         goto usWriteEASExit;
         }
      }

   free(pszEAName);

#ifdef EXFAT
   if (pCD->bFatType == FAT_TYPE_EXFAT)
      {
      SetSHInfo1(pCD, pDirStreamNew, pSHInfo);
      }
#endif

   pWrite = (PBYTE)pFEAL;
   ulNextCluster = pCD->ulFatEof;
   while (usClustersNeeded)
      {
      ulNextCluster = GetNextCluster(pCD, pSHInfo, ulCluster, FALSE);
      if (!ulNextCluster)
         ulNextCluster = pCD->ulFatEof;
      rc = WriteCluster(pCD, ulCluster, pWrite);
      if (rc)
         return rc;
      usClustersNeeded --;
      pWrite += pCD->ulClusterSize;

      if (usClustersNeeded)
         {
         if (ulNextCluster == pCD->ulFatEof)
            ulCluster = MakeFatChain(pCD, pSHInfo, ulCluster, (ULONG)usClustersNeeded, NULL);
         else
            ulCluster = ulNextCluster;
         if (ulCluster == pCD->ulFatEof)
            return ERROR_DISK_FULL;
         }
      }
   if (ulNextCluster != pCD->ulFatEof)
      {
      SetNextCluster(pCD, ulCluster, pCD->ulFatEof);
      DeleteFatChain(pCD, ulNextCluster);
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
      rc = MarkFileEAS(pCD, ulDirCluster, pDirSHInfo, pszFileName, FILE_HAS_CRITICAL_EAS);
   else
      rc = MarkFileEAS(pCD, ulDirCluster, pDirSHInfo, pszFileName, FILE_HAS_EAS);

usWriteEASExit:
   if (pDirEntry)
      free(pDirEntry);
   if (pDirNew)
      free(pDirNew);
#ifdef EXFAT
   if (pDirStream)
      free(pDirStream);
   if (pDirStreamNew)
      free(pDirStreamNew);
   if (pSHInfo)
      free(pSHInfo);
#endif

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
      MessageL(LOG_EAS, "FindEA%m: '%s'", 0x0060, pName);
#endif
      if (pFea->cbName == (BYTE)usMaxName && !memicmp(pName, pszName, usMaxName))
         return pFea;

      pFea = (PFEA)((PBYTE)pFea + sizeof (FEA) + (USHORT)pFea->cbName + 1 + pFea->cbValue);
      }
   return NULL;
}

USHORT GetEASName(PCDINFO pCD, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo, PSZ pszFileName, PSZ * pszEASName)
{
   if (strlen(pszFileName) > FAT32MAXPATH - 4)
      return ERROR_FILENAME_EXCED_RANGE;

   *pszEASName = malloc(FAT32MAXPATH);
   if (!(*pszEASName))
      return ERROR_NOT_ENOUGH_MEMORY;

   //if ( TranslateName( pCD, ulDirCluster, pDirSHInfo, pszFileName, *pszEASName, TRANSLATE_SHORT_TO_LONG ))
   //    strcpy(*pszEASName, pszFileName);
   strcpy(*pszEASName, pszFileName);

   strcat(*pszEASName, EA_EXTENTION);
   return 0;
}
