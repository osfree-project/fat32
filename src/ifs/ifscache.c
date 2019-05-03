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

#define NOT_USED    0xFFFFFFFF
#define MAX_SECTORS 4096
#define PAGE_SIZE   4096
#define MAX_SLOTS   0x4000
#define FREE_SLOT   0xFFFF

#ifdef MAX_RASECTORS
#undef MAX_RASECTORS
#endif
#define MAX_RASECTORS   64

PRIVATE ULONG ulSemEmergencyTrigger=0UL;
PRIVATE ULONG ulSemEmergencyDone   =0UL;
PRIVATE ULONG ulDummySem           =0UL;

PRIVATE volatile USHORT  usOldestEntry = 0xFFFF;
PRIVATE volatile USHORT  usNewestEntry = 0xFFFF;
PRIVATE ULONG    ulPhysCacheAddr = 0UL;
PRIVATE SEL      rgCacheSel[(MAX_SECTORS / 128) + 1] = {0};
PRIVATE USHORT   usSelCount = 1;

PRIVATE CACHEBASE  _based(_segname("IFSCACHE1_DATA"))pCacheBase[MAX_SECTORS] = {0};
PRIVATE CACHEBASE2 _based(_segname("IFSCACHE2_DATA"))pCacheBase2[MAX_SECTORS] = {0};
PRIVATE ULONG      _based(_segname("IFSCACHE2_DATA"))ulLockSem[MAX_SECTORS] = {0};
PRIVATE USHORT     _based(_segname("IFSCACHE2_DATA"))rgSlot[MAX_SLOTS] = {0};
PRIVATE BOOL       _based(_segname("IFSCACHE3_DATA"))rgfDirty[MAX_SECTORS] = {0};
PRIVATE CACHE      _based(_segname("IFSCACHE3_DATA"))raSectors[MAX_RASECTORS] = {0};

extern PCPDATA pCPData;

PRIVATE BOOL   IsSectorInCache(PVOLINFO pVolInfo, ULONG ulSector, PBYTE bSector);
PRIVATE BOOL   fStoreSector(PVOLINFO pVolInfo, ULONG ulSector, PBYTE pbSector, BOOL fDirty);
PRIVATE PVOID  GetAddress(ULONG ulEntry);
PRIVATE BOOL   fFindSector(ULONG ulSector, BYTE bDrive, PUSHORT pusIndex);
PRIVATE USHORT WriteCacheSector(PVOLINFO pVolInfo, USHORT usCBIndex, BOOL fSetTime);
PRIVATE VOID   UpdateChain(USHORT usCBIndex);
PRIVATE VOID   vGetSectorFromCache(PVOLINFO pVolInfo, USHORT usCBIndex, PBYTE pbSector);
PRIVATE VOID   vReplaceSectorInCache(PVOLINFO pVolInfo, USHORT usCBIndex, PBYTE pbSector, BOOL fDirty);
PRIVATE VOID   LockBuffer(PCACHEBASE pBase);
PRIVATE VOID   UnlockBuffer(PCACHEBASE pBase);
PRIVATE USHORT usEmergencyFlush(VOID);
PRIVATE USHORT VerifyOn(VOID);

int PreSetup(void);
int PostSetup(void);

/******************************************************************
*
******************************************************************/
BOOL InitCache(ULONG ulSectors)
{
static BOOL fInitDone = FALSE;
APIRET      rc;
USHORT      usIndex;
ULONG       ulSize;
PVOID       p;
CACHEBASE   _based(_segname("IFSCACHE1_DATA")) *pBase;

   if (fInitDone || !ulSectors || f32Parms.usCacheSize)
      return FALSE;

   fInitDone = TRUE;

   if (ulSectors > MAX_SECTORS)
      ulSectors = MAX_SECTORS;
   Message("Allocating cache space for %ld sectors", ulSectors);

   /* Account for enough selectors */
   usSelCount = (USHORT)(ulSectors / 128 + (ulSectors % 128 ? 1: 0));

   rc = DevHelp_AllocGDTSelector(rgCacheSel, usSelCount);
   if (rc)
   {
      FatalMessage("FAT32: AllocGDTSelector failed, rc = %d", rc);
      return FALSE;
   }

   /* Allocate linear memory */
   ulSize = ulSectors * (ULONG)sizeof(CACHE);
   rc = linalloc(ulSize, f32Parms.fHighMem, f32Parms.fHighMem,&ulPhysCacheAddr);
   if (rc)
   {
      /* If tried to use high memory, try to use low memory */
      if (f32Parms.fHighMem)
      {
          f32Parms.fHighMem = FALSE;
          rc = linalloc(ulSize, f32Parms.fHighMem, f32Parms.fHighMem,&ulPhysCacheAddr);
      }
      if (rc)
          return FALSE;
   }

   /* Fill the selectors */
    for (usIndex = 0; usIndex < usSelCount; usIndex++)
    {
        ULONG ulBlockSize = 0x10000UL;
        if (ulBlockSize > ulSize)
            ulBlockSize = ulSize;

        rc = DevHelp_PhysToGDTSelector( ulPhysCacheAddr+usIndex*0x10000UL,
                                        (USHORT)ulBlockSize,
                                        rgCacheSel[usIndex]);
        if (rc)
        {
            FatalMessage("FAT32: PhysGDTSelector (%d) failed, rc = %d", usIndex, rc);
            return FALSE;
        }
        ulSize -= 0x10000UL;
    }

   /* this uses propagation to copy the contents of the
    * first array element into the remaining elements
    */
   pCacheBase[0].ulSector = NOT_USED;
   pCacheBase[0].bDrive = 0xFF;
   pCacheBase[0].usNext = FREE_SLOT;
   memcpy(&pCacheBase[1], pCacheBase, (ulSectors - 1) * sizeof(CACHEBASE));

   /* init all but the last element with the index of the next element */
   for (pBase = pCacheBase, usIndex = 1; usIndex < ulSectors; pBase++, usIndex++)
      pBase->usNext = usIndex;

   memset(pCacheBase2, 0xFF, ulSectors * sizeof(CACHEBASE2));
   memset(rgSlot, 0xFF, sizeof(rgSlot));
   memset(rgfDirty, FALSE, sizeof(rgfDirty));

   f32Parms.usCacheSize = (USHORT)ulSectors;
   f32Parms.usDirtyTreshold =
      f32Parms.usCacheSize - (f32Parms.usCacheSize / 10);
   f32Parms.usCacheUsed = 0;

   p = (PVOID)InitCache;
   rc = FSH_FORCENOSWAP(SELECTOROF(p));
   if (rc)
      FatalMessage("FAT32: FSH_FORCENOSWAP on CODE Segment failed, rc=%u", rc);

   p = (PVOID)pCacheBase;
   rc = FSH_FORCENOSWAP(SELECTOROF(p));
   if (rc)
      FatalMessage("FAT32:FSH_FORCENOSWAP on IFSCACHE1_DATA Segment failed, rc=%u", rc);

   p = (PVOID)pCacheBase2;
   rc = FSH_FORCENOSWAP(SELECTOROF(p));
   if (rc)
      FatalMessage("FAT32:FSH_FORCENOSWAP on IFSCACHE2_DATA Segment failed, rc=%u", rc);

   p = (PVOID)rgfDirty;
   rc = FSH_FORCENOSWAP(SELECTOROF(p));
   if (rc)
      FatalMessage("FAT32:FSH_FORCENOSWAP on IFSCACHE3_DATA Segment failed, rc=%u", rc);

   return TRUE;
}

/******************************************************************
*
******************************************************************/

USHORT ReadSector(PVOLINFO pVolInfo, ULONG ulSector, USHORT nSectors, PCHAR pbData, USHORT usIOMode)
{
APIRET   rc,rc2;
USHORT   usSectors;
USHORT   usIndex;
BYTE far *pbSectors;
USHORT   usCBIndex;
char far *p;
ULONG    ulLast;
ULONG    _based(_segname("IFSCACHE2_DATA"))*pLockSem = ulLockSem;
CACHE    _based(_segname("IFSCACHE3_DATA"))*praSectors = raSectors;

   if (ulSector + nSectors - 1 >= pVolInfo->BootSect.bpb.BigTotalSectors)
      return ERROR_SECTOR_NOT_FOUND;

   f32Parms.ulTotalReads += nSectors;

   /* See how many leading sectors are in the cache -
    * each one found is one less that has to be read from disk
    */
   for (ulLast = ulSector + nSectors;
        ulSector < ulLast;
        ulSector++, nSectors--, pbData += (ULONG)pVolInfo->BootSect.bpb.BytesPerSector)
   {
      if (!IsSectorInCache(pVolInfo, ulSector, pbData))
         break;
   }

   /* if all sectors were found in cache, we are done */
   if (ulSector >= ulLast)
      return 0;

   pbSectors = NULL;
   if ((ulSector >= pVolInfo->ulStartOfData) &&
       !(usIOMode & DVIO_OPNCACHE) && nSectors < pVolInfo->usRASectors)
   {
      usSectors = min(pVolInfo->usRASectors, MAX_RASECTORS);
      if (ulSector + usSectors > pVolInfo->BootSect.bpb.BigTotalSectors)
         usSectors = (USHORT)(pVolInfo->BootSect.bpb.BigTotalSectors - ulSector);
      pbSectors = praSectors->bSector;
   }

   if (!pbSectors)
   {
      pbSectors = pbData;
      usSectors = nSectors;
   }

   usIOMode &= ~DVIO_OPWRITE;
   pVolInfo->ulLastDiskTime = GetCurTime();
   if (pVolInfo->hVBP)
      rc = FSH_DOVOLIO(DVIO_OPREAD | usIOMode, DVIO_ALLACK, pVolInfo->hVBP, pbSectors, &usSectors, ulSector);
   else
      {
      /* read loopback device */
      rc = PreSetup();

      if (rc)
         {
         if (pbSectors != pbData)
            {
            free(pbSectors);
            }
         return rc;
         }

      pCPData->Op = OP_READ;
      pCPData->hf = pVolInfo->hf;
#ifdef INCL_LONGLONG
      pCPData->llOffset = (LONGLONG)pVolInfo->ullOffset + ulSector * pVolInfo->BootSect.bpb.BytesPerSector;
#else
      iAssign(&pCPData->llOffset, *(PLONGLONG)&pVolInfo->ullOffset);
      pCPData->llOffset = iAddUL(pCPData->llOffset, ulSector * pVolInfo->BootSect.bpb.BytesPerSector);
#endif
      pCPData->cbData = usSectors * pVolInfo->BootSect.bpb.BytesPerSector;

      rc = PostSetup();

      if (rc)
         {
         if (pbSectors != pbData)
            {
            free(pbSectors);
            }
         return rc;
         }

      memcpy(pbSectors, &pCPData->Buf, (USHORT)pCPData->cbData);
      rc = (USHORT)pCPData->rc;
      }

   if (rc)
   {
      Message("ERROR: ReadSector of sector %ld (%d sectors) failed, rc = %u",
              ulSector, usSectors, rc);
      return rc;
   }

    /* Store sector only in cache if we should */
   for (usIndex = 0,p=pbSectors; usIndex < usSectors; usIndex++,p+= (ULONG)pVolInfo->BootSect.bpb.BytesPerSector)
   {
      /* Was sector already in cache? */
      if (!fFindSector(ulSector + usIndex, pVolInfo->bDrive, &usCBIndex))
      {
         /* No, it wasn't. Store it if needed. (and if caching is not to be bypassed) */
         if (!(usIOMode & DVIO_OPNCACHE))
            fStoreSector(pVolInfo, ulSector + usIndex, p, FALSE);
      }
      else {
         /* Yes it was. Get it if it was dirty since it is different from version on disk. */
         if (rgfDirty[usCBIndex])
            vGetSectorFromCache(pVolInfo, usCBIndex, p);
         /* when fFindSector returns with true it has the semaphore requested */
         rc2 = FSH_SEMCLEAR(&pLockSem[usCBIndex]);
      }
   }

   if (pbSectors == praSectors->bSector)
   {
      f32Parms.ulTotalRA += usSectors > nSectors ? (usSectors - nSectors) : 0;
      memcpy(pbData, pbSectors, min(usSectors, nSectors ) * pVolInfo->BootSect.bpb.BytesPerSector);
   }

   return rc;
}

/******************************************************************
*
******************************************************************/
USHORT WriteSector(PVOLINFO pVolInfo, ULONG ulSector, USHORT nSectors, PCHAR pbData, USHORT usIOMode)
{
APIRET   rc,rc2;
USHORT 	usSectors = nSectors;
USHORT 	usIndex;
BOOL     fDirty;
USHORT   usCBIndex;
char     *p;
ULONG  	_based(_segname("IFSCACHE2_DATA"))*pLockSem = ulLockSem;

   if (pVolInfo->fWriteProtected)
      return ERROR_WRITE_PROTECT;

   if (pVolInfo->fDiskClean)
   {
      MarkDiskStatus(pVolInfo, FALSE);
      //pVolInfo->fDiskClean = FALSE; // 0.9.13 didn't change this to FALSE
   }

   if (ulSector + nSectors - 1 >= pVolInfo->BootSect.bpb.BigTotalSectors)
      return ERROR_SECTOR_NOT_FOUND;

   fDirty = TRUE;
   rc = 0;
   if (!f32Parms.fLW || (usIOMode & DVIO_OPWRTHRU) || (usIOMode & DVIO_OPNCACHE))
   {
      MessageL(LOG_CACHE, "WriteSector%m: Writing sector thru", 0x4063);
      pVolInfo->ulLastDiskTime = GetCurTime();
      if (pVolInfo->hVBP)
         rc = FSH_DOVOLIO(DVIO_OPWRITE | usIOMode | VerifyOn(), DVIO_ALLACK, pVolInfo->hVBP, pbData, &usSectors, ulSector);
         //rc = FSH_DOVOLIO(DVIO_OPWRITE | usIOMode | VerifyOn(), DVIO_ALLABORT | DVIO_ALLRETRY | DVIO_ALLFAIL, pVolInfo->hVBP, pbData, &usSectors, ulSector);
      else
         {
         /* write loopback device */
         rc = PreSetup();

         if (rc)
            return rc;

         pCPData->Op = OP_WRITE;
         pCPData->hf = pVolInfo->hf;
#ifdef INCL_LONGLONG
         pCPData->llOffset = (LONGLONG)pVolInfo->ullOffset + ulSector * pVolInfo->BootSect.bpb.BytesPerSector;
#else
         iAssign(&pCPData->llOffset, *(PLONGLONG)&pVolInfo->ullOffset);
         pCPData->llOffset = iAddUL(pCPData->llOffset, ulSector * pVolInfo->BootSect.bpb.BytesPerSector);
#endif
         pCPData->cbData = usSectors * pVolInfo->BootSect.bpb.BytesPerSector;
         memcpy(&pCPData->Buf, pbData, (USHORT)pCPData->cbData);

         rc = PostSetup();

         if (rc)
            return rc;

         rc = (USHORT)pCPData->rc;
         }

      fDirty = FALSE;

      if (rc)
      {
         if (rc != ERROR_WRITE_PROTECT )
            Message("FAT32: ERROR: WriteSector sector %ld (%d sectors) failed, rc = %u",
                    ulSector, nSectors, rc);
         return rc;
      }
   }

   for (usIndex = 0,p= pbData; usIndex < usSectors; usIndex++,p+= (ULONG)pVolInfo->BootSect.bpb.BytesPerSector)
   {
      if (!fFindSector(ulSector + usIndex, pVolInfo->bDrive, &usCBIndex))
      {
         if (!(usIOMode & DVIO_OPNCACHE))
            fStoreSector(pVolInfo, ulSector + usIndex, p, fDirty);
      }
      else
      {
          vReplaceSectorInCache(pVolInfo, usCBIndex, p, fDirty);
         /* when fFindSector returns with true it has the semaphore requested */
         rc2 = FSH_SEMCLEAR(&pLockSem[usCBIndex]);
      }
   }

   return rc;
}

/******************************************************************
*
******************************************************************/
BOOL IsSectorInCache(PVOLINFO pVolInfo, ULONG ulSector, PBYTE pbSector)
{
APIRET rc2;
USHORT usIndex;
ULONG  _based(_segname("IFSCACHE2_DATA"))*pLockSem = ulLockSem;

   if (!fFindSector(ulSector, pVolInfo->bDrive, &usIndex))
      return FALSE;
   f32Parms.ulTotalHits++;
   vGetSectorFromCache(pVolInfo, usIndex, pbSector);

   rc2 = FSH_SEMCLEAR(&pLockSem[usIndex]); /* when fFindSector returns with true it has the semaphore requested */

   return TRUE;
}

/******************************************************************
*
******************************************************************/
VOID vGetSectorFromCache(PVOLINFO pVolInfo, USHORT usCBIndex, PBYTE pbSector)
{
CACHEBASE  _based(_segname("IFSCACHE1_DATA"))*pBase;
PCACHE pCache;

   pBase = pCacheBase + usCBIndex;

   pCache = GetAddress(usCBIndex);
   memcpy(pbSector, pCache->bSector, pVolInfo->BootSect.bpb.BytesPerSector);

   pBase->ulAccessTime = GetCurTime();
   UpdateChain(usCBIndex);

   return;
}

/******************************************************************
*
******************************************************************/
VOID UpdateChain(USHORT usCBIndex)
{
CACHEBASE2 _based(_segname("IFSCACHE2_DATA"))*pBase2;

   /*
      Is entry already the newest ?
   */
   if (usNewestEntry == usCBIndex)
      return;

   pBase2 = pCacheBase2 + usCBIndex;
   /*
      Remove entry from current position in chain
   */
   if (pBase2->usOlder != 0xFFFF)
      pCacheBase2[pBase2->usOlder].usNewer = pBase2->usNewer;
   if (pBase2->usNewer != 0xFFFF)
      pCacheBase2[pBase2->usNewer].usOlder = pBase2->usOlder;

   /*
      Update the oldest if this entry was the oldest
   */

   if (usOldestEntry == 0xFFFF)
      usOldestEntry = usCBIndex;
   else if (usOldestEntry == usCBIndex)
      usOldestEntry = pBase2->usNewer;

   /*
      Update base itself
   */
   pBase2->usOlder = usNewestEntry;
   if (usNewestEntry != 0xFFFF)
      pCacheBase2[usNewestEntry].usNewer = usCBIndex;
   usNewestEntry = usCBIndex;
   pBase2->usNewer = 0xFFFF;
}

/******************************************************************
*
******************************************************************/
BOOL fStoreSector(PVOLINFO pVolInfo, ULONG ulSector, PBYTE pbSector, BOOL fDirty)
{
APIRET rc2;
USHORT usSlot;
USHORT usCBIndex,usCBIndexNew;
CACHEBASE  _based(_segname("IFSCACHE1_DATA"))*pBase;
CACHEBASE2 _based(_segname("IFSCACHE2_DATA"))*pBase2;
ULONG      _based(_segname("IFSCACHE2_DATA"))*pLockSem = ulLockSem;

   if (!f32Parms.usCacheSize)
      return FALSE;

   pBase = NULL;
   if (f32Parms.usCacheUsed < f32Parms.usCacheSize)
      {
      usCBIndex = f32Parms.usCacheUsed;

      rc2 = FSH_SEMREQUEST(&pLockSem[usCBIndex],-1L);

      f32Parms.usCacheUsed++;

      pBase = pCacheBase + usCBIndex;

      usSlot = (USHORT)(ulSector % MAX_SLOTS);
      pBase->usNext = rgSlot[usSlot];
      rgSlot[usSlot] = usCBIndex;

      pBase->ulSector = ulSector;
      pBase->bDrive = pVolInfo->bDrive;

      vReplaceSectorInCache(pVolInfo, usCBIndex, pbSector, fDirty);


      rc2 = FSH_SEMCLEAR(&pLockSem[usCBIndex]);
      }
    else
    {
      CACHEBASE  _based(_segname("IFSCACHE1_DATA"))*pWork;
      USHORT usIndex;
      USHORT leaveFlag = FALSE;

      if (usOldestEntry == 0xFFFF)
         FatalMessage("FAT32: No Oldest entry found!");

      /*
         find the oldest non dirty, not locked, not pending sector in cache
      */

        while (leaveFlag == FALSE)
        {
            // Should try various thresholds current is
            // f32Parms.usCacheSize - (f32Parms.usCacheSize / 20)
            while (f32Parms.usDirtySectors >= f32Parms.usDirtyTreshold)
            {

                MessageL(LOG_CACHE | LOG_WAIT, "waiting for dirty sectors to be less than threshold%m...", 0x4064);

                FSH_SEMSET(&ulSemEmergencyDone);
                if (FSH_SEMCLEAR(&ulSemEmergencyTrigger) == NO_ERROR)
                {
                    FSH_SEMWAIT(&ulSemEmergencyDone,-1);
                }
                else
                {
                    FSH_SEMCLEAR(&ulSemEmergencyDone);
                    usEmergencyFlush();
                    
                }
                if (f32Parms.usDirtySectors >= f32Parms.usDirtyTreshold)
                {
                    BOOL fMsg = f32Parms.fMessageActive;
                    //CritMessage("FAT32:No free sectors in cache! (run MONITOR now!)");
                    Message("ERROR: fStoreSector - No sectors available!");
                    Message("       %u sectors are dirty", f32Parms.usDirtySectors);
                    f32Parms.fMessageActive = fMsg;
                }
            }

            usCBIndex = usOldestEntry;
            while (usCBIndex != FREE_SLOT)
            {
                rc2 = FSH_SEMREQUEST(&pLockSem[usCBIndex],-1L);
                if (!rgfDirty[usCBIndex])
                {
                    /*
                        Remove entry from slot chain
                    */

                    pBase = pCacheBase + usCBIndex;
                    usSlot = (USHORT)(pBase->ulSector % MAX_SLOTS);
                    usIndex = rgSlot[usSlot];
                    if (usIndex == usCBIndex)
                        rgSlot[usSlot] = pBase->usNext;
                    else
                    {
                        pWork = NULL;
                        while (usIndex != FREE_SLOT)
                        {
                            pWork = pCacheBase + usIndex;
                            if (pWork->usNext == usCBIndex)
                                break;
                            usIndex = pWork->usNext;
                        }
                        if (usIndex == FREE_SLOT) {
                            FatalMessage("FAT32: Store: Oldest entry not found in slot chain!");
                        }
                        pWork->usNext = pBase->usNext;
                    }

                    usSlot = (USHORT)(ulSector % MAX_SLOTS);
                    pBase->usNext = rgSlot[usSlot];
                    rgSlot[usSlot] = usCBIndex;

                    pBase->ulSector = ulSector;
                    pBase->bDrive = pVolInfo->bDrive;

                    vReplaceSectorInCache(pVolInfo, usCBIndex, pbSector, fDirty);

                    leaveFlag = TRUE;
                    rc2 = FSH_SEMCLEAR(&pLockSem[usCBIndex]);
                    break;
                }

                pBase2 = pCacheBase2 + usCBIndex;
                usCBIndexNew = pBase2->usNewer;

                rc2 = FSH_SEMCLEAR(&pLockSem[usCBIndex]);

                usCBIndex = usCBIndexNew;
            }
        }
    }
    return TRUE;
}

/******************************************************************
*
******************************************************************/
VOID vReplaceSectorInCache(PVOLINFO pVolInfo, USHORT usCBIndex, PBYTE pbSector, BOOL fDirty)
{
CACHEBASE  _based(_segname("IFSCACHE1_DATA"))*pBase;
PCACHE     pCache;

      pBase = pCacheBase + usCBIndex;

      if (!rgfDirty[usCBIndex] && fDirty)
         {
         f32Parms.usDirtySectors++;
         pBase->ulCreateTime = GetCurTime();
         }
      else if (rgfDirty[usCBIndex] && !fDirty)
         {
         if (f32Parms.usDirtySectors)
            f32Parms.usDirtySectors--;
         }
      rgfDirty[usCBIndex] = fDirty;
      pCache = GetAddress(usCBIndex);
      memcpy(pCache->bSector, pbSector, pVolInfo->BootSect.bpb.BytesPerSector);
      pBase->ulAccessTime = GetCurTime();
      UpdateChain(usCBIndex);
      return;
}
/******************************************************************
*
******************************************************************/
PVOID GetAddress(ULONG ulEntry)
{
ULONG ulOffset;
USHORT usSel;
    
   ulOffset = ulEntry * sizeof (CACHE);
   usSel = (USHORT)(ulOffset / 0x10000);

   ulOffset = ulOffset % 0x10000;

   return (PVOID)MAKEP(rgCacheSel[usSel], (USHORT)ulOffset);
}

/******************************************************************
*
******************************************************************/
BOOL fFindSector(ULONG ulSector, BYTE bDrive, PUSHORT pusIndex)
{
APIRET rc2;
CACHEBASE  _based(_segname("IFSCACHE1_DATA"))*pBase;
ULONG      _based(_segname("IFSCACHE2_DATA"))*pLockSem = ulLockSem;
USHORT    usCBIndex,usCBIndexNew;

   if (!f32Parms.usCacheUsed)
      return FALSE;

   usCBIndex = rgSlot[(USHORT)(ulSector % MAX_SLOTS)];

   while (usCBIndex != FREE_SLOT)
      {
      rc2 = FSH_SEMREQUEST(&pLockSem[usCBIndex],-1L);
      pBase = pCacheBase + usCBIndex;
      if (pBase->ulSector == ulSector && pBase->bDrive == bDrive)
         {
         *pusIndex = usCBIndex;
         return TRUE;
         }
      usCBIndexNew = pBase->usNext;
      rc2 = FSH_SEMCLEAR(&pLockSem[usCBIndex]);
      usCBIndex = usCBIndexNew;
      }

   return FALSE;
}


/******************************************************************
*
******************************************************************/
USHORT WriteCacheSector(PVOLINFO pVolInfo, USHORT usCBIndex, BOOL fSetTime)
{
CACHEBASE  _based(_segname("IFSCACHE1_DATA"))*pBase;
USHORT   rc;
USHORT   usSectors;
PCACHE   pCache;

   pBase = pCacheBase + usCBIndex;
   if (!rgfDirty[usCBIndex])
   {
      if( pBase->fDiscard )
      {
         pBase->bDrive = 0xFF;
         pBase->fDiscard = OFF;
      }

      return 0;
   }

   if (!pVolInfo)
      pVolInfo = pGlobVolInfo;

   while (pVolInfo)
      {
      if (pVolInfo->bDrive == (BYTE)pBase->bDrive)
         {
         usSectors = 1;

         pCache = GetAddress(usCBIndex);
         if (pVolInfo->hVBP)
            {
            rc = FSH_DOVOLIO(DVIO_OPWRITE | VerifyOn(), DVIO_ALLACK,
                  pVolInfo->hVBP, pCache->bSector, &usSectors, pBase->ulSector);
            }
         else
            {
            /* write loopback device */
            rc = PreSetup();

            if (rc)
               return rc;

            pCPData->Op = OP_WRITE;
            pCPData->hf = pVolInfo->hf;
#ifdef INCL_LONGLONG
            pCPData->llOffset = (LONGLONG)pVolInfo->ullOffset + pBase->ulSector * pVolInfo->BootSect.bpb.BytesPerSector;
#else
            iAssign(&pCPData->llOffset, *(PLONGLONG)&pVolInfo->ullOffset);
            pCPData->llOffset = iAddUL(pCPData->llOffset, pBase->ulSector * pVolInfo->BootSect.bpb.BytesPerSector);
#endif
            pCPData->cbData = usSectors * pVolInfo->BootSect.bpb.BytesPerSector;
            memcpy(&pCPData->Buf, pCache->bSector, (USHORT)pCPData->cbData);

            rc = PostSetup();

            if (rc)
               return rc;

            rc = (USHORT)pCPData->rc;
            }

         FSH_IOBOOST();

         if (!rc || rc == ERROR_WRITE_PROTECT)
            {
            if( pBase->fDiscard )
            {
               pBase->bDrive = 0xFF;
               pBase->fDiscard = OFF;
            }

            rgfDirty[usCBIndex] = FALSE;
            if (f32Parms.usDirtySectors)
                f32Parms.usDirtySectors--;

            if (fSetTime)
               pVolInfo->ulLastDiskTime = GetCurTime();
            }
         else
            Message("FAT32: Error %u in WriteCacheSector", rc);

         return rc;
         }
      pVolInfo = (PVOLINFO)pVolInfo->pNextVolInfo;
      }

   FatalMessage("FAT32: WriteCacheSector: VOLINFO not found!");
   return 1;
}


/******************************************************************
*
******************************************************************/
VOID DoEmergencyFlush(PLWOPTS pOptions)
{
USHORT rc;

    while (!f32Parms.fInShutDown && !pOptions->fTerminate)
    {
        rc = FSH_SEMSETWAIT(&ulSemEmergencyTrigger,2000UL);
        if (rc == NO_ERROR)
        {
            usEmergencyFlush();
            rc = FSH_SEMCLEAR(&ulSemEmergencyDone);
        }
        else
        {
            rc = FSH_SEMCLEAR(&ulSemEmergencyTrigger);
        }
    }
    rc = FSH_SEMCLEAR(&ulSemEmergencyDone);
}

/******************************************************************
*
******************************************************************/
USHORT usFlushAll(VOID)
{
PVOLINFO pVolInfo = pGlobVolInfo;
USHORT rc;

   while (pVolInfo)
      {
      rc = usFlushVolume(pVolInfo, FLUSH_DISCARD, TRUE, PRIO_URGENT);
      pVolInfo = (PVOLINFO)pVolInfo->pNextVolInfo;
      }
   return 0;
}

/******************************************************************
*
******************************************************************/
VOID DoLW(PVOLINFO pVolInfo, PLWOPTS pOptions)
{
APIRET rc2;
//BYTE bPriority;
LONG lWait;

   rc2 = FSH_SEMSET(&ulDummySem);

   Message("DoLW started");

   lWait = f32Parms.ulDiskIdle;
   while (!f32Parms.fInShutDown && !pOptions->fTerminate)
      {
      rc2 = FSH_SEMWAIT(&ulDummySem,lWait); /* noone will clear the semaphore, so this is just a wait */

      if (f32Parms.fLW)
         {
         pVolInfo = pGlobVolInfo;
         while (pVolInfo)
            {
            ULONG ulTime = GetCurTime();

            if (ulTime < pVolInfo->ulLastDiskTime ||
                pVolInfo->ulLastDiskTime + f32Parms.ulDiskIdle <= ulTime)

               usFlushVolume(pVolInfo, FLUSH_RETAIN, FALSE, 0x01);
            pVolInfo = (PVOLINFO)pVolInfo->pNextVolInfo;
            }
         }
      }

   f32Parms.fLW = FALSE;
   usFlushAll();
   Message("DoLW Stopped, Lazy writing also");

   rc2 = FSH_SEMCLEAR(&ulDummySem);
}

/******************************************************************
*
******************************************************************/
USHORT usFlushVolume(PVOLINFO pVolInfo, USHORT usFlag, BOOL fFlushAll, BYTE bPriority)
{
APIRET rc2;
USHORT usCBIndex = 0,usCBIndexNew;
CACHEBASE  _based(_segname("IFSCACHE1_DATA"))*pBase;
CACHEBASE2 _based(_segname("IFSCACHE2_DATA"))*pBase2;
ULONG      _based(_segname("IFSCACHE2_DATA"))*pLockSem = ulLockSem;
USHORT usCount;
ULONG  ulCurTime = GetCurTime();

   if (!f32Parms.usCacheUsed )
      return 0;

   usCount = 0;

   if (!fFlushAll)
      {
      usCBIndex = usOldestEntry;
      while ((usCBIndex != FREE_SLOT) && (FSH_SEMREQUEST(&pLockSem[usCBIndex],0L) == NO_ERROR))
      {
         pBase2 = pCacheBase2 + usCBIndex;
         pBase  = pCacheBase + usCBIndex;

         if (pBase->bDrive == pVolInfo->bDrive &&
             (ulCurTime - pBase->ulCreateTime >= f32Parms.ulMaxAge ||
              ulCurTime - pBase->ulAccessTime >= f32Parms.ulBufferIdle))
            {
            if( rgfDirty[ usCBIndex ] )
               {
                if( usFlag == FLUSH_DISCARD )
                    pBase->fDiscard = SET;
                WriteCacheSector(pVolInfo, usCBIndex, FALSE);
                usCount++;
               }
            else if( usFlag == FLUSH_DISCARD )
               {
                  pBase->bDrive = 0xFF;
                  pBase->fDiscard = OFF;
               }
            }
         usCBIndexNew = pBase2->usNewer;
         rc2 = FSH_SEMCLEAR(&pLockSem[usCBIndex]);
         usCBIndex = usCBIndexNew;
         }
      if (usCount > 0)
         MessageL(LOG_CACHE, "%m%u sectors LAZY flushed, still %u dirty", 0x8065, usCount, f32Parms.usDirtySectors);

      return 0;
      }

   MessageL(LOG_CACHE, "usFlushVolume%m ALL", 0x4066);

   usCBIndex = 0;
   for (usCBIndex = 0; ( f32Parms.usDirtySectors || usFlag == FLUSH_DISCARD ) &&
         usCBIndex < f32Parms.usCacheUsed; usCBIndex++)
      {
      rc2 = FSH_SEMREQUEST(&pLockSem[usCBIndex],-1L);

      pBase = pCacheBase + usCBIndex;
      if (pVolInfo->bDrive == (BYTE)pBase->bDrive)
         {
         if( rgfDirty[usCBIndex] )
            {
            if( usFlag == FLUSH_DISCARD )
               pBase->fDiscard = SET;
            WriteCacheSector(pVolInfo, usCBIndex, FALSE);
            usCount++;
            }
         else if( usFlag == FLUSH_DISCARD )
            {
               pBase->bDrive = 0xFF;
               pBase->fDiscard = OFF;
            }
         }

         rc2 = FSH_SEMCLEAR(&pLockSem[usCBIndex]);
      }

   MessageL(LOG_CACHE, "%m%u sectors flushed, still %u dirty", 0x8167, usCount, f32Parms.usDirtySectors);
   return 0;
}

/******************************************************************
*
******************************************************************/
USHORT usEmergencyFlush(VOID)
{
APIRET rc2;
USHORT usCBIndex = 0,usCBIndexNew;
CACHEBASE  _based(_segname("IFSCACHE1_DATA"))*pBase;
CACHEBASE2 _based(_segname("IFSCACHE2_DATA"))*pBase2;
ULONG      _based(_segname("IFSCACHE2_DATA"))*pLockSem = ulLockSem;
USHORT usCount;
PVOLINFO pVolInfo;

   if (!f32Parms.usCacheUsed)
      return 0;

   /*
      find the volinfo for the oldest entry
   */
   pVolInfo = NULL;
   usCBIndex = usOldestEntry;
   while (usCBIndex != FREE_SLOT)
   {
      rc2 = FSH_SEMREQUEST(&pLockSem[usCBIndex],-1L);

      pBase2 = pCacheBase2 + usCBIndex;
      pBase  = pCacheBase + usCBIndex;
      if (rgfDirty[usCBIndex])
         {
         pVolInfo = pGlobVolInfo;
         while (pVolInfo)
            {
            if ((BYTE)pBase->bDrive == pVolInfo->bDrive)
               break;
            pVolInfo = (PVOLINFO)(pVolInfo->pNextVolInfo);
            }
         if (!pVolInfo)
            FatalMessage("FAT32: Drive not found in emergency flush!");
         break;
         }
      usCBIndexNew = pBase2->usNewer;
      rc2 = FSH_SEMCLEAR(&pLockSem[usCBIndex]);
      usCBIndex = usCBIndexNew;
      }

   if (usCBIndex == FREE_SLOT)
      return 0;

   if (!pVolInfo)
      return 0;

   usCount = 0;
   while (usCBIndex != FREE_SLOT)
      {
      pBase2 = pCacheBase2 + usCBIndex;
      pBase  = pCacheBase + usCBIndex;
      if (rgfDirty[usCBIndex] &&
         pVolInfo->bDrive == (BYTE)pBase->bDrive)
         {
         pBase->fDiscard = SET;
         WriteCacheSector(pVolInfo, usCBIndex, FALSE);
         usCount++;
         }
      usCBIndexNew = pBase2->usNewer;
      rc2 = FSH_SEMCLEAR(&pLockSem[usCBIndex]);
      usCBIndex = usCBIndexNew;
      if (usCBIndex != FREE_SLOT)
        rc2 = FSH_SEMREQUEST(&pLockSem[usCBIndex],-1L);
      }
   if (usCBIndex != FREE_SLOT)
   {
      rc2 = FSH_SEMCLEAR(&pLockSem[usCBIndex]);
   }

   pVolInfo->ulLastDiskTime = GetCurTime();

   MessageL(LOG_CACHE, "usEmergencyFlush%m: %u sectors flushed, still %u dirty", 0x8168, usCount, f32Parms.usDirtySectors);

   return 0;
}

USHORT VerifyOn(VOID)
{
   APIRET rc=NO_ERROR;
   UCHAR fVerifyOn = 0;
   USHORT ret = 0;

   //Message("VerifyOn Pre-Invocation");
   rc = FSH_QSYSINFO(4,&fVerifyOn,sizeof(fVerifyOn));
   if (rc == NO_ERROR)
      {
      ret = (fVerifyOn ? DVIO_OPVERIFY: 0);
      }
   //Message("VerifyOn Post-Invocation");
   return ret;
}

