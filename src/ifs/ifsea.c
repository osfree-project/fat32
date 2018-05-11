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


USHORT usCopyEAS2(PVOLINFO pVolInfo, ULONG ulSrcDirCluster, PSZ pszSrcFile, PSHOPENINFO pDirSrcSHInfo,
                  ULONG ulTarDirCluster, PSZ pszTarFile, PSHOPENINFO pDirTarSHInfo);

PRIVATE PFEA   FindEA(PFEALIST pFeal, PSZ pszName, USHORT usMaxName);
PRIVATE USHORT usReadEAS(PVOLINFO pVolInfo, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo, PSZ pszFileName, PFEALIST * ppFEAL, BOOL fCreate);
PRIVATE USHORT usWriteEAS(PVOLINFO pVolInfo, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo, PSZ pszFileName, PFEALIST pFEAL);
PRIVATE USHORT GetEASName(PVOLINFO pVolInfo, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo, PSZ pszFileName, PSZ * pszEASName);

extern F32PARMS f32Parms;


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

   MessageL(LOG_EAS, "usModifyEAS%m for %s", 0x005a, pszFileName);

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

   MessageL(LOG_EAS, "cbList before%m = %lu", 0x4036, pTarFeal->cbList);

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

         MessageL(LOG_EAS, "Inserting EA%m '%s' (%u,%u)", 0x4037, pName,
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

         MessageL(LOG_EAS, "Updating EA%m '%s' (%u,%u)", 0x4038, pName,
                  pSrcFea->cbName, pSrcFea->cbValue);
         }

      usNewSize = sizeof (FEA) + (USHORT)pSrcFea->cbName + 1 + pSrcFea->cbValue;
      pSrcFea = (PFEA)((PBYTE)pSrcFea + usNewSize);
      }

   MessageL(LOG_EAS, "cbList after%m = %lu", 0x4039, pTarFeal->cbList);

   if (pTarFeal->cbList > 4)
      rc = usWriteEAS(pVolInfo, ulDirCluster, pDirSHInfo, pszFileName, pTarFeal);
   else
      rc = usDeleteEAS(pVolInfo, ulDirCluster, pDirSHInfo, pszFileName);

usStoreEASExit:
   free(pTarFeal);

   MessageL(LOG_EAS, "usModifyEAS%m for %s returned %d",
            0x805a, pszFileName, rc);

   return rc;
}

/************************************************************************
*
************************************************************************/
USHORT usGetEASize(PVOLINFO pVolInfo, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo, PSZ pszFileName, PULONG pulSize)
{
PSZ pszEAName = NULL;
USHORT rc;
PDIRENTRY pDirEntry = NULL;
PDIRENTRY1 pDirEntryStream = NULL;
ULONG    ulCluster;

   MessageL(LOG_EAS, "usGetEASSize%m for %s", 0x005b, pszFileName);

   pDirEntry = (PDIRENTRY)malloc((size_t)sizeof(DIRENTRY));
   if (!pDirEntry)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto usGetEASizeExit;
      }
#ifdef EXFAT
   pDirEntryStream = (PDIRENTRY1)malloc((size_t)sizeof(DIRENTRY1));
   if (!pDirEntryStream)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto usGetEASizeExit;
      }
#endif

   *pulSize = sizeof (ULONG);

   rc = GetEASName(pVolInfo, ulDirCluster, pDirSHInfo, pszFileName, &pszEAName);
   if (rc)
      goto usGetEASizeExit;

   ulCluster = FindPathCluster(pVolInfo, ulDirCluster, pszEAName,
      pDirSHInfo, pDirEntry, pDirEntryStream, NULL);
   if (ulCluster == pVolInfo->ulFatEof || !ulCluster)
      {
      rc = 0;
      goto usGetEASizeExit;
      }
#ifdef EXFAT
   if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
#endif
      *pulSize = pDirEntry->ulFileSize;
#ifdef EXFAT
   else
#ifdef INCL_LONGLONG
      *pulSize = pDirEntryStream->u.Stream.ullValidDataLen;
#else
      *pulSize = pDirEntryStream->u.Stream.ullValidDataLen.ulLo;
#endif
#endif

   rc = 0;

usGetEASizeExit:
   if (pszEAName)
      free(pszEAName);
   if( pDirEntry)
      free(pDirEntry);
#ifdef EXFAT
   if (pDirEntryStream)
      free(pDirEntryStream);
#endif

   MessageL(LOG_EAS, "usGetEASize%m for %s returned %d (%u bytes large)",
            0x805b, pszFileName, rc, *pulSize);

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

   MessageL(LOG_EAS, "usGetEAS%m for %s Level %d", 0x005c, pszFileName, usLevel);

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
            MessageL(LOG_EAS, "Found%m %s", 0x403c, pSrcFea + 1);
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

            MessageL(LOG_EAS, "usGetEAS%m: %s not found!", 0x403d, pGea->szName);

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
         MessageL(LOG_EAS, "Found%m %s (%u,%u)", 0x403e, pSrcFea + 1, (USHORT)pSrcFea->cbName, pSrcFea->cbValue);
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

   MessageL(LOG_EAS, "usGetEAS%m for %s returned %d (%lu bytes in EAS)",
            0x805c, pszFileName, rc, pTarFeal->cbList);

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
PDIRENTRY pSrcEntry = NULL, pTarEntry = NULL;
PDIRENTRY1 pTarStreamEntry = NULL, pSrcStreamEntry = NULL;
PSHOPENINFO pSrcSHInfo = NULL;

   pSrcEntry = (PDIRENTRY)malloc((size_t)sizeof(DIRENTRY));
   if (!pSrcEntry)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto usCopyEASExit;
      }
   pTarEntry = (PDIRENTRY)malloc((size_t)sizeof(DIRENTRY));
   if (!pTarEntry)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto usCopyEASExit;
      }
#ifdef EXFAT
   pSrcStreamEntry = (PDIRENTRY1)malloc((size_t)sizeof(DIRENTRY1));
   if (!pSrcStreamEntry)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto usCopyEASExit;
      }
   pTarStreamEntry = (PDIRENTRY1)malloc((size_t)sizeof(DIRENTRY1));
   if (!pTarStreamEntry)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto usCopyEASExit;
      }
   pSrcSHInfo = (PSHOPENINFO)malloc((size_t)sizeof(SHOPENINFO));
   if (!pSrcSHInfo)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto usCopyEASExit;
      }
#endif

   if (f32Parms.fEAS2)
      {
      rc = usCopyEAS2(pVolInfo, ulSrcDirCluster, pszSrcFile, pDirSrcSHInfo,
                      ulTarDirCluster, pszTarFile, pDirTarSHInfo);
      goto usCopyEASExit;
      }

   rc = GetEASName(pVolInfo, ulSrcDirCluster, pDirSrcSHInfo, pszSrcFile, &pszSrcEAName);
   if (rc)
      goto usCopyEASExit;
   rc = GetEASName(pVolInfo, ulTarDirCluster, pDirTarSHInfo, pszTarFile, &pszTarEAName);
   if (rc)
      goto usCopyEASExit;

   ulSrcCluster = FindPathCluster(pVolInfo, ulSrcDirCluster, pszSrcEAName, pDirSrcSHInfo, pSrcEntry, pSrcStreamEntry, NULL);
   ulTarCluster = FindPathCluster(pVolInfo, ulTarDirCluster, pszTarEAName, pDirTarSHInfo, pTarEntry, pTarStreamEntry, NULL);
   if (ulTarCluster != pVolInfo->ulFatEof)
      {
      rc = ModifyDirectory(pVolInfo, ulTarDirCluster, pDirTarSHInfo, MODIFY_DIR_DELETE, pTarEntry, NULL, pTarStreamEntry, NULL, pszTarEAName, pszTarEAName, 0);
      //rc = ModifyDirectory(pVolInfo, ulTarDirCluster, pDirTarSHInfo, MODIFY_DIR_DELETE, pTarEntry, NULL, pTarStreamEntry, NULL, NULL, 0);
      if (rc)
         goto usCopyEASExit;
      DeleteFatChain(pVolInfo, ulTarCluster);
      }

   if (ulSrcCluster == pVolInfo->ulFatEof)
      goto usCopyEASExit;

#ifdef EXFAT
   if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
      {
      SetSHInfo1(pVolInfo, pSrcStreamEntry, pSrcSHInfo);
      }
#endif

   rc = CopyChain(pVolInfo, pSrcSHInfo, ulSrcCluster, &ulTarCluster);
   if (rc)
      goto usCopyEASExit;

#ifdef EXFAT
   if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
      {
#endif
      pSrcEntry->wCluster = LOUSHORT(ulTarCluster);
      pSrcEntry->wClusterHigh = HIUSHORT(ulTarCluster);
#ifdef EXFAT
      }
   else
      {
      pSrcStreamEntry->u.Stream.ulFirstClus = ulTarCluster;
      }
#endif

   /*
      Make new direntry
   */
   rc = ModifyDirectory(pVolInfo, ulTarDirCluster, pDirTarSHInfo, MODIFY_DIR_INSERT, NULL, pSrcEntry, NULL, pSrcStreamEntry, pszTarEAName, pszTarEAName, 0);


usCopyEASExit:
   if (pszSrcEAName)
      free(pszSrcEAName);
   if (pszTarEAName)
      free(pszTarEAName);
   if (pSrcEntry)
      free(pSrcEntry);
   if (pTarEntry)
      free(pTarEntry);
#ifdef EXFAT
   if (pSrcStreamEntry)
      free(pSrcStreamEntry);
   if (pTarStreamEntry)
      free(pTarStreamEntry);
   if (pSrcSHInfo)
      free(pSrcSHInfo);
#endif

   MessageL(LOG_EAS, "usCopyEAS%m for returned %d", 0x805d, rc);

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
PDIRENTRY pSrcEntry = NULL, pTarEntry = NULL;
PDIRENTRY1 pSrcStreamEntry = NULL, pTarStreamEntry = NULL;

   pSrcEntry = (PDIRENTRY)malloc((size_t)sizeof(DIRENTRY));
   if (!pSrcEntry)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto usMoveEASExit;
      }
   pTarEntry = (PDIRENTRY)malloc((size_t)sizeof(DIRENTRY));
   if (!pTarEntry)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto usMoveEASExit;
      }
#ifdef EXFAT
   pSrcStreamEntry = (PDIRENTRY1)malloc((size_t)sizeof(DIRENTRY1));
   if (!pSrcStreamEntry)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto usMoveEASExit;
      }
   pTarStreamEntry = (PDIRENTRY1)malloc((size_t)sizeof(DIRENTRY1));
   if (!pTarStreamEntry)
      {
      rc = ERROR_NOT_ENOUGH_MEMORY;
      goto usMoveEASExit;
      }
#endif

   rc = GetEASName(pVolInfo, ulSrcDirCluster, pDirSrcSHInfo, pszSrcFile, &pszSrcEAName);
   if (rc)
      goto usMoveEASExit;
   rc = GetEASName(pVolInfo, ulTarDirCluster, pDirTarSHInfo, pszTarFile, &pszTarEAName);
   if (rc)
      goto usMoveEASExit;


   ulSrcCluster = FindPathCluster(pVolInfo, ulSrcDirCluster, pszSrcEAName, pDirSrcSHInfo, pSrcEntry, pSrcStreamEntry, NULL);
   ulTarCluster = FindPathCluster(pVolInfo, ulTarDirCluster, pszTarEAName, pDirTarSHInfo, pTarEntry, pTarStreamEntry, NULL);
   if (ulTarCluster != pVolInfo->ulFatEof && ulTarCluster != ulSrcCluster)
      {
      rc = ModifyDirectory(pVolInfo, ulTarDirCluster, pDirTarSHInfo, MODIFY_DIR_DELETE, pTarEntry, NULL, pTarStreamEntry, NULL, pszTarEAName, pszTarEAName, 0);
      //rc = ModifyDirectory(pVolInfo, ulTarDirCluster, pDirTarSHInfo, MODIFY_DIR_DELETE, pTarEntry, NULL, pTarStreamEntry, NULL, NULL, 0);
      if (rc)
         goto usMoveEASExit;
      DeleteFatChain(pVolInfo, ulTarCluster);
      }

   if (ulSrcCluster == pVolInfo->ulFatEof)
      goto usMoveEASExit;

   if (ulSrcDirCluster == ulTarDirCluster)
      {
      memmove(pTarEntry, pSrcEntry, sizeof(DIRENTRY1));
#ifdef EXFAT
      memmove(pTarStreamEntry, pSrcStreamEntry, sizeof(DIRENTRY1));
#endif
      rc = ModifyDirectory(pVolInfo, ulSrcDirCluster, pDirSrcSHInfo,
         MODIFY_DIR_RENAME, pSrcEntry, pTarEntry, pSrcStreamEntry, pTarStreamEntry, pszSrcEAName, pszTarEAName, 0);
      goto usMoveEASExit;
      }

   rc = ModifyDirectory(pVolInfo, ulSrcDirCluster, pDirSrcSHInfo, MODIFY_DIR_DELETE, pSrcEntry, NULL, pSrcStreamEntry, NULL, pszSrcEAName, pszSrcEAName, 0);
   //rc = ModifyDirectory(pVolInfo, ulSrcDirCluster, pDirSrcSHInfo, MODIFY_DIR_DELETE, pSrcEntry, NULL, pSrcStreamEntry, NULL, NULL, 0);
   if (rc)
      goto usMoveEASExit;

   rc = ModifyDirectory(pVolInfo, ulTarDirCluster, pDirTarSHInfo, MODIFY_DIR_INSERT, NULL, pSrcEntry, NULL, pSrcStreamEntry, pszTarEAName, pszTarEAName, 0);

usMoveEASExit:
   if (pszSrcEAName)
      free(pszSrcEAName);
   if (pszTarEAName)
      free(pszTarEAName);
   if (pSrcEntry)
      free(pSrcEntry);
   if (pTarEntry)
      free(pTarEntry);
#ifdef EXFAT
   if (pSrcStreamEntry)
      free(pSrcStreamEntry);
   if (pTarStreamEntry)
      free(pTarStreamEntry);
#endif

   MessageL(LOG_EAS, "usMoveEAS%m for returned %d", 0x805e, rc);

   return rc;
}

/************************************************************************
*
************************************************************************/
USHORT MarkFileEAS(PVOLINFO pVolInfo, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo, PSZ pszFileName, BYTE fEAS)
{
ULONG ulCluster;
PDIRENTRY pOldEntry = NULL, pNewEntry = NULL;
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

   ulCluster = FindPathCluster(pVolInfo, ulDirCluster, pszFileName, pDirSHInfo, pOldEntry, pOldEntryStream, NULL);
   if (ulCluster == pVolInfo->ulFatEof)
      {
      //CritMessage("FAT32: MarkfileEAS : %s not found!", pszFileName);
      rc = ERROR_FILE_NOT_FOUND;
      goto MarkFileEASExit;
      }
   memcpy(pNewEntry, pOldEntry, sizeof (DIRENTRY));
#ifdef EXFAT
   if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
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

   rc = ModifyDirectory(pVolInfo, ulDirCluster, pDirSHInfo,
      MODIFY_DIR_UPDATE, pOldEntry, pNewEntry, pOldEntryStream, NULL, pszFileName, pszFileName, 0);
   //rc = ModifyDirectory(pVolInfo, ulDirCluster, pDirSHInfo,
   //   MODIFY_DIR_UPDATE, pOldEntry, pNewEntry, pOldEntryStream, NULL, NULL, 0);

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
USHORT usReadEAS(PVOLINFO pVolInfo, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo, PSZ pszFileName, PFEALIST * ppFEAL, BOOL fCreate)
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

   rc = GetEASName(pVolInfo, ulDirCluster, pDirSHInfo, pszFileName, &pszEAName);
   if (rc)
      goto usReadEASExit;

   ulCluster = FindPathCluster(pVolInfo, ulDirCluster, pszEAName, pDirSHInfo, NULL, pStreamEntry, NULL);
   free(pszEAName);

   if ((ulCluster && ulCluster != pVolInfo->ulFatEof) || fCreate)
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

   if (!ulCluster || ulCluster == pVolInfo->ulFatEof)
      {
      *ppFEAL = pFEAL;
      rc = 0;
      goto usReadEASExit;
      }

   pRead = (PBYTE)pFEAL;
   MessageL(LOG_EAS, "usReadEAS%m: Reading (1) cluster %lu", 0x4041, ulCluster);

   rc = ReadBlock(pVolInfo, ulCluster, 0, pRead, 0);
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
   MessageL(LOG_EAS, "usReadEAS%m: %u clusters used", 0x4042, usClustersUsed);

   usBlocksUsed--;
   pRead += pVolInfo->ulBlockSize;

#ifdef EXFAT
   if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
      {
      SetSHInfo1(pVolInfo, pStreamEntry, pSHInfo);
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
            rc = ERROR_EA_FILE_CORRUPT;
            goto usReadEASExit;
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
         MessageL(LOG_EAS, "usReadEAS%m: Reading (2) cluster %lu, block %lu", 0x4043, ulCluster, ulBlock);

         rc = ReadBlock(pVolInfo, ulCluster, ulBlock, pRead, 0);
         if (rc)
            {
            free(pFEAL);
            goto usReadEASExit;
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
USHORT usDeleteEAS(PVOLINFO pVolInfo, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo, PSZ pszFileName)
{
PSZ pszEAName;
USHORT rc;
PDIRENTRY pDirEntry = NULL;
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

   rc = GetEASName(pVolInfo, ulDirCluster, pDirSHInfo, pszFileName, &pszEAName);
   if (rc)
      return rc;

   ulCluster = FindPathCluster(pVolInfo, ulDirCluster, pszEAName, pDirSHInfo, pDirEntry, pStreamEntry, NULL);
   if (ulCluster == pVolInfo->ulFatEof)
      {
      rc = 0;
      goto usDeleteEASExit;
      }
   rc = ModifyDirectory(pVolInfo, ulDirCluster, pDirSHInfo, MODIFY_DIR_DELETE, pDirEntry, NULL, pStreamEntry, NULL, pszEAName, pszEAName, 0);
   //rc = ModifyDirectory(pVolInfo, ulDirCluster, pDirSHInfo, MODIFY_DIR_DELETE, pDirEntry, NULL, pStreamEntry, NULL, NULL, 0);
   if (rc)
      goto usDeleteEASExit;

   if (ulCluster)
      DeleteFatChain(pVolInfo, ulCluster);

   rc = MarkFileEAS(pVolInfo, ulDirCluster, pDirSHInfo, pszFileName, FILE_HAS_NO_EAS);

usDeleteEASExit:
   if (pszEAName)
      free(pszEAName);
   if (pDirEntry)
      free(pDirEntry);
#ifdef EXFAT
   if (pStreamEntry)
      free(pStreamEntry);
#endif

   MessageL(LOG_EAS, "usDeleteEAS%m for %s returned %d",
            0x805f, pszFileName, rc);

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
PDIRENTRY pDirEntry = NULL;
PDIRENTRY pDirNew = NULL;
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

   rc = GetEASName(pVolInfo, ulDirCluster, pDirSHInfo, pszFileName, &pszEAName);
   if (rc)
      goto usWriteEASExit;

   usClustersNeeded = (USHORT)((ULONG)pFEAL->cbList / pVolInfo->ulClusterSize);
   if (pFEAL->cbList % pVolInfo->ulClusterSize)
      usClustersNeeded++;

   usBlocksNeeded = (USHORT)((ULONG)pFEAL->cbList / pVolInfo->ulBlockSize);
   if (pFEAL->cbList % pVolInfo->ulBlockSize)
      usBlocksNeeded++;

   ulCluster = FindPathCluster(pVolInfo, ulDirCluster, pszEAName, pDirSHInfo, pDirEntry, pDirStream, NULL);
   if (!ulCluster || ulCluster == pVolInfo->ulFatEof)
      {
      BOOL fNew = FALSE;

      if (ulCluster == pVolInfo->ulFatEof)
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
      if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
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
         //   (pFEAL->cbList / pVolInfo->ulClusterSize) * pVolInfo->ulClusterSize +
         //   (pFEAL->cbList % pVolInfo->ulClusterSize ? pVolInfo->ulClusterSize : 0);
#else
         AssignUL(pDirStreamNew->u.Stream.ullValidDataLen, pFEAL->cbList);
         AssignUL(pDirStreamNew->u.Stream.ullDataLen, pDirStreamNew->u.Stream.ullValidDataLen);
         //   (pFEAL->cbList / pVolInfo->ulClusterSize) * pVolInfo->ulClusterSize +
         //   (pFEAL->cbList % pVolInfo->ulClusterSize ? pVolInfo->ulClusterSize : 0));
#endif
         }
#endif

#ifdef EXFAT
      if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
         {
         SetSHInfo1(pVolInfo, pDirStream, pSHInfo);
         }
#endif

      ulCluster = MakeFatChain(pVolInfo, pSHInfo, pVolInfo->ulFatEof, (ULONG)usClustersNeeded, NULL);
      if (ulCluster == pVolInfo->ulFatEof)
         {
         free(pszEAName);
         rc = ERROR_DISK_FULL;
         goto usWriteEASExit;
         }

#ifdef EXFAT
      if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
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
         rc = MakeDirEntry(pVolInfo, ulDirCluster, pDirSHInfo, pDirNew, pDirStreamNew, pszEAName);
      else
         rc = ModifyDirectory(pVolInfo, ulDirCluster, pDirSHInfo,
            MODIFY_DIR_UPDATE, pDirEntry, pDirNew, pDirStream, pDirStreamNew, pszEAName, pszEAName, 0);
         //rc = ModifyDirectory(pVolInfo, ulDirCluster, pDirSHInfo,
         //   MODIFY_DIR_UPDATE, pDirEntry, pDirNew, pDirStream, pDirStreamNew, NULL, 0);
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
      if (pVolInfo->bFatType < FAT_TYPE_EXFAT)
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
         pDirStreamNew->u.Stream.ullDataLen = pDirStreamNew->u.Stream.ullValidDataLen;
         //   (pFEAL->cbList / pVolInfo->ulClusterSize) * pVolInfo->ulClusterSize +
         //   (pFEAL->cbList % pVolInfo->ulClusterSize ? pVolInfo->ulClusterSize : 0);
#else
         AssignUL(pDirStreamNew->u.Stream.ullValidDataLen, pFEAL->cbList);
         AssignUL(pDirStreamNew->u.Stream.ullDataLen, pDirStreamNew->u.Stream.ullValidDataLen);
         //   (pFEAL->cbList / pVolInfo->ulClusterSize) * pVolInfo->ulClusterSize +
         //   (pFEAL->cbList % pVolInfo->ulClusterSize ? pVolInfo->ulClusterSize : 0));
#endif
         }
#endif
      rc = ModifyDirectory(pVolInfo, ulDirCluster, pDirSHInfo, MODIFY_DIR_UPDATE,
           pDirEntry, pDirNew, pDirStream, pDirStreamNew, pszEAName, pszEAName, 0);
      //rc = ModifyDirectory(pVolInfo, ulDirCluster, pDirSHInfo, MODIFY_DIR_UPDATE,
      //     pDirEntry, pDirNew, pDirStream, pDirStreamNew, NULL, 0);
      if (rc)
         {
         free(pszEAName);
         goto usWriteEASExit;
         }
      }

   free(pszEAName);

#ifdef EXFAT
   if (pVolInfo->bFatType == FAT_TYPE_EXFAT)
      {
      SetSHInfo1(pVolInfo, pDirStreamNew, pSHInfo);
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
            goto usWriteEASExit;
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
            {
            rc = ERROR_DISK_FULL;
            goto usWriteEASExit;
            }
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

USHORT GetEASName(PVOLINFO pVolInfo, ULONG ulDirCluster, PSHOPENINFO pDirSHInfo, PSZ pszFileName, PSZ * pszEASName)
{
   if (strlen(pszFileName) > FAT32MAXPATH - 4)
      return ERROR_FILENAME_EXCED_RANGE;

   *pszEASName = malloc(FAT32MAXPATH);
   if (!(*pszEASName))
      return ERROR_NOT_ENOUGH_MEMORY;

   if( TranslateName( pVolInfo, ulDirCluster, pDirSHInfo, pszFileName, *pszEASName, TRANSLATE_SHORT_TO_LONG ))
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

   MessageL(LOG_EAS, "usGetEmptyEAS%m for %s with pEAOP %lX", 0x0061, pszFileName,pEAOP);

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
