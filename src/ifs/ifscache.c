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

PRIVATE ULONG ulSemEmergencyTrigger=0UL;
PRIVATE ULONG ulSemEmergencyDone   =0UL;
PRIVATE ULONG ulDummySem           =0UL;
PRIVATE ULONG ulSemRWRead          =0UL;

PRIVATE volatile USHORT  usOldestEntry = 0xFFFF;
PRIVATE volatile USHORT  usNewestEntry = 0xFFFF;
PRIVATE ULONG    ulPhysCacheAddr = 0UL;
PRIVATE SEL      rgCacheSel[(MAX_SECTORS / 128) + 1] = {0};
PRIVATE USHORT   usSelCount = 1;
PRIVATE USHORT   usPhysCount = 0;

PRIVATE CACHEBASE  _based(_segname("IFSCACHE1_DATA"))pCacheBase[MAX_SECTORS] = {0};
PRIVATE CACHEBASE2 _based(_segname("IFSCACHE2_DATA"))pCacheBase2[MAX_SECTORS] = {0};
ULONG          _based(_segname("IFSCACHE2_DATA"))ulLockSem[MAX_SECTORS] = {0};
PRIVATE USHORT _based(_segname("IFSCACHE2_DATA"))rgSlot[MAX_SLOTS] = {0};
PRIVATE BOOL   _based(_segname("IFSCACHE3_DATA"))rgfDirty[MAX_SECTORS] = {0};

PRIVATE USHORT GetReadAccess(PVOLINFO pVolInfo, PSZ pszName);
PRIVATE VOID   ReleaseReadBuf(PVOLINFO pVolInfo);
PRIVATE BOOL   IsSectorInCache(PVOLINFO pVolInfo, ULONG ulSector, PBYTE bSector);
PRIVATE BOOL   fStoreSector(PVOLINFO pVolInfo, ULONG ulSector, PBYTE pbSector, BOOL fDirty);
PRIVATE PVOID  GetAddress(ULONG ulEntry);
PRIVATE PVOID  GetPhysAddr(PRQLIST pRQ, ULONG ulEntry);
PRIVATE BOOL   fFindSector(ULONG ulSector, BYTE bDrive, PUSHORT pusIndex);
PRIVATE USHORT WriteCacheSector(PVOLINFO pVolInfo, USHORT usCBIndex, BOOL fSetTime);
PRIVATE VOID   UpdateChain(USHORT usCBIndex);
PRIVATE VOID   vGetSectorFromCache(PVOLINFO pVolInfo, USHORT usCBIndex, PBYTE pbSector);
PRIVATE VOID   vReplaceSectorInCache(PVOLINFO pVolInfo, USHORT usCBIndex, PBYTE pbSector, BOOL fDirty);
PRIVATE VOID   LockBuffer(PCACHEBASE pBase);
PRIVATE VOID   UnlockBuffer(PCACHEBASE pBase);
PRIVATE USHORT usEmergencyFlush(VOID);
PRIVATE USHORT VerifyOn(VOID);
USHORT GetFatAccess(PVOLINFO pVolInfo, PSZ pszName);
VOID   ReleaseFat(PVOLINFO pVolInfo);
USHORT GetBufAccess(PVOLINFO pVolInfo, PSZ pszName);
VOID   ReleaseBuf(PVOLINFO pVolInfo);
ULONG GetNextCluster2(PVOLINFO pVolInfo, PSHOPENINFO pSHInfo, ULONG ulCluster);

PUBLIC VOID _cdecl InitMessage(PSZ pszMessage,...);



/******************************************************************
*
******************************************************************/
BOOL InitCache(ULONG ulSectors)
{
static BOOL fInitDone = FALSE;
APIRET rc;
PCACHEBASE pBase;
PCACHEBASE2 pBase2;
USHORT usIndex;
ULONG ulSize;
PVOID p;

   if (fInitDone)
      return FALSE;
   fInitDone = TRUE;

   if (!ulSectors || f32Parms.usCacheSize)
      return FALSE;


   if (ulSectors > MAX_SECTORS)
      ulSectors = MAX_SECTORS;
   Message("Allocating cache space for %ld sectors", ulSectors);


   f32Parms.usCacheSize = 0L;

   /* Account for enough selectors */

   usSelCount = (USHORT)(ulSectors / 128 + (ulSectors % 128 ? 1: 0));

   rc = DevHelp_AllocGDTSelector(rgCacheSel, usSelCount);
   if (rc)
      {
      FatalMessage("FAT32: AllocGDTSelector failed, rc = %d", rc);
      return FALSE;
      }


   /* Allocate linear memory */

   ulSize = ulSectors * (ULONG)sizeof (CACHE);
   rc = linalloc(ulSize, f32Parms.fHighMem, f32Parms.fHighMem,&ulPhysCacheAddr);
   if (rc != NO_ERROR)
   {
      /* If tried to use high memory, try to use low memory */
      if( f32Parms.fHighMem )
      {
          f32Parms.fHighMem = FALSE;
          rc = linalloc(ulSize, f32Parms.fHighMem, f32Parms.fHighMem,&ulPhysCacheAddr);
      }

      if( rc != NO_ERROR)
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

   f32Parms.usCacheSize = (USHORT)ulSectors;
   f32Parms.usDirtyTreshold =
      f32Parms.usCacheSize - (f32Parms.usCacheSize / 20);

   pBase = pCacheBase;
   pBase2 = pCacheBase2;
   for (usIndex = 0; usIndex < f32Parms.usCacheSize; usIndex++)
      {
      pBase->ulSector = NOT_USED;
      pBase->bDrive = 0xFF;
      pBase2->usOlder = 0xFFFF;
      pBase2->usNewer = 0xFFFF;
      if (usIndex + 1 < f32Parms.usCacheSize)
         pBase->usNext = usIndex + 1;
      else
         pBase->usNext = FREE_SLOT;
      pBase++;
      pBase2++;
      }

   memset(rgSlot, 0xFF, sizeof rgSlot);
   memset(rgfDirty, FALSE, sizeof rgfDirty);

   f32Parms.usCacheUsed = 0L;

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


#define Cluster2Sector( ulCluster )     (( ULONG )( pVolInfo->ulStartOfData + \
                                         (( ULONG )( ulCluster ) - 2) * pVolInfo->SectorsPerCluster ))

#define Sector2Cluster( ulSector )      (( ULONG )((( ULONG )( ulSector ) - pVolInfo->ulStartOfData ) / \
                                         pVolInfo->SectorsPerCluster + 2 ))

/******************************************************************
*
******************************************************************/
USHORT ReadSector(PVOLINFO pVolInfo, ULONG ulSector, USHORT nSectors, PCHAR pbData, USHORT usIOMode)
{
APIRET rc,rc2;
USHORT usSectors;
USHORT usIndex;
PBYTE pbSectors;
//static BYTE pbSect[0x10000];
BOOL fFromCache;
BOOL fSectorInCache;
USHORT usCBIndex;
BOOL fRASectors = FALSE;
char far *p;

   if (ulSector + nSectors - 1 >= pVolInfo->BootSect.bpb.BigTotalSectors)
      {
      FatalMessage("FAT32: ERROR: Sector %ld does not exist on disk %c:",
         ulSector + nSectors - 1, pVolInfo->bDrive + 'A');
      return ERROR_SECTOR_NOT_FOUND;
      }

   f32Parms.ulTotalReads += nSectors;

   /*
      See if all sectors are in cache
   */
   fFromCache = TRUE;
   for (usIndex = 0,p=pbData; usIndex < nSectors; usIndex++,p += (ULONG)pVolInfo->BootSect.bpb.BytesPerSector)
      {
      if (!IsSectorInCache(pVolInfo, ulSector + usIndex, p))
         {
         fFromCache = FALSE;
         break;
         }
      }
   /*
      if all sectors were found in cache, we are done
   */
   if (fFromCache)
      return 0;

#if 0
   if (f32Parms.fMessageActive & LOG_CACHE)
      {
      if (ulSector > pVolInfo->ulStartOfData)
         Message("Cluster %lu not found in cache!",
            (ulSector - pVolInfo->ulStartOfData) / pVolInfo->SectorsPerCluster + 2);
      }
#endif
   pbSectors = NULL;
   if (( ulSector >= pVolInfo->ulStartOfData ) &&
       !(usIOMode & DVIO_OPNCACHE) && nSectors < pVolInfo->usRASectors)
      {
      usSectors = pVolInfo->usRASectors;
      if (ulSector + usSectors > pVolInfo->BootSect.bpb.BigTotalSectors)
         usSectors = (USHORT)(pVolInfo->BootSect.bpb.BigTotalSectors - ulSector);
      pbSectors = malloc(usSectors * pVolInfo->BootSect.bpb.BytesPerSector);
      fRASectors = TRUE;
      }
   //if (!pbSectors)
   if (!fRASectors)
      {
      pbSectors = pbData;
      usSectors = nSectors;
      }

   /* check bad cluster (moved to ReadBlock) */
   if( ulSector >= pVolInfo->ulStartOfData )
   {
        ULONG ulStartCluster = Sector2Cluster( ulSector );
        ULONG ulEndCluster = Sector2Cluster( ulSector + usSectors - 1 );
        ULONG ulNextCluster = 0;
        ULONG ulCluster;

        /* for( ulCluster = ulStartCluster; ulCluster <= ulEndCluster; ulCluster++ )
        {
            //ulNextCluster = GetNextCluster2( pVolInfo, NULL, ulCluster );
            ulNextCluster = GetNextCluster( pVolInfo, NULL, ulCluster );
            if( ulNextCluster == pVolInfo->ulFatBad )
                break;
        }

        if( ulNextCluster == pVolInfo->ulFatBad )
        {
            usSectors = ( ulStartCluster != ulCluster ) ?
                ( min(( USHORT )( Cluster2Sector( ulCluster ) - ulSector ), usSectors )) : 0;
        }

        if (usSectors == 0)
            // avoid reading zero sectors
            return ERROR_SECTOR_NOT_FOUND; */
   }

   usIOMode &= ~DVIO_OPWRITE;
   pVolInfo->ulLastDiskTime = GetCurTime();
   rc = FSH_DOVOLIO(DVIO_OPREAD | usIOMode, DVIO_ALLACK, pVolInfo->hVBP, pbSectors, &usSectors, ulSector);
   if (rc)
      {
      CritMessage("FAT32: ReadSector of sector %ld (%d sectors) failed, rc = %u",
         ulSector, usSectors, rc);
      Message("ERROR: ReadSector of sector %ld (%d sectors) failed, rc = %u",
         ulSector, usSectors, rc);
      }

    /*
       Store sector only in cache if we should
    */
    if (!rc)
       {
       for (usIndex = 0,p=pbSectors; usIndex < usSectors; usIndex++,p+= (ULONG)pVolInfo->BootSect.bpb.BytesPerSector)
          {
          /*
             Was sector already in cache?
          */
          fSectorInCache = fFindSector(ulSector + usIndex,
             pVolInfo->bDrive,
             &usCBIndex);

          switch (fSectorInCache)
             {
             /*
                No, it wasn't. Store it if needed. (and if caching is not to be bypassed)
             */
             case FALSE :
                if (!(usIOMode & DVIO_OPNCACHE))
                   fStoreSector(pVolInfo, ulSector + usIndex, p, FALSE);
                break;
             case TRUE  :
             /*
                Yes it was. Get it if it was dirty since then it is different
                from version on disk.
             */
                if (rgfDirty[usCBIndex])
                   {
                   vGetSectorFromCache(pVolInfo, usCBIndex, p);
                   }
                   rc2 = FSH_SEMCLEAR(&ulLockSem[usCBIndex]); /* when fFindSector returns with true it has the semaphore requested */
                break;
             }
          }
       }

    //if (!rc && pbSectors != pbData)
    if (!rc && fRASectors)
       {
       f32Parms.ulTotalRA += usSectors > nSectors ? (usSectors - nSectors) : 0;
       memcpy(pbData, pbSectors, min( usSectors, nSectors ) * pVolInfo->BootSect.bpb.BytesPerSector);
       }

   if (pbSectors != pbData)
      free(pbSectors);

   return rc;
}

/******************************************************************
*
******************************************************************/
USHORT WriteSector(PVOLINFO pVolInfo, ULONG ulSector, USHORT nSectors, PCHAR pbData, USHORT usIOMode)
{
APIRET rc,rc2;
USHORT usSectors = nSectors;
USHORT usIndex;
BOOL   fDirty;
BOOL   fSectorInCache;
USHORT usCBIndex;
char *p;

   if (pVolInfo->fWriteProtected)
      return ERROR_WRITE_PROTECT;

   if (pVolInfo->fDiskClean)
      MarkDiskStatus(pVolInfo, FALSE);

   if (ulSector + nSectors - 1 >= pVolInfo->BootSect.bpb.BigTotalSectors)
      {
      FatalMessage("FAT32: ERROR: Sector %ld does not exist on disk %c:",
         ulSector + nSectors - 1, pVolInfo->bDrive + 'A');
      return ERROR_SECTOR_NOT_FOUND;
      }

   fDirty = TRUE;
   rc = 0;
   if (!f32Parms.fLW || (usIOMode & DVIO_OPWRTHRU) || (usIOMode & DVIO_OPNCACHE))
      {
      if (f32Parms.fLW && f32Parms.fMessageActive & LOG_CACHE)
         Message("WriteSector: Writing sector thru");
      pVolInfo->ulLastDiskTime = GetCurTime();
      rc = FSH_DOVOLIO(DVIO_OPWRITE | usIOMode | VerifyOn(), DVIO_ALLACK, pVolInfo->hVBP, pbData, &usSectors, ulSector);
      if (rc && rc != ERROR_WRITE_PROTECT )
         CritMessage("FAT32: ERROR: WriteSector sector %ld (%d sectors) failed, rc = %u",
            ulSector, nSectors, rc);
      fDirty = FALSE;
      }

   if (!rc)
      {
      for (usIndex = 0,p= pbData; usIndex < usSectors; usIndex++,p+= (ULONG)pVolInfo->BootSect.bpb.BytesPerSector)
         {
         fSectorInCache =
            fFindSector(ulSector + usIndex, pVolInfo->bDrive, &usCBIndex);
         switch (fSectorInCache)
            {
            case FALSE :
               if (!(usIOMode & DVIO_OPNCACHE))
                  fStoreSector(pVolInfo, ulSector + usIndex, p, fDirty);
               break;
            case TRUE  :
               {
               BOOL fIdent = FALSE;

               if( fDirty )
                  {
                  PCACHE pCache;

                  pCache = GetAddress(usCBIndex);
                  fIdent = memcmp(p, pCache->bSector, pVolInfo->BootSect.bpb.BytesPerSector) == 0;
                  }

               if( !fIdent )
                  vReplaceSectorInCache(pVolInfo, usCBIndex, p, fDirty);

               rc2 = FSH_SEMCLEAR(&ulLockSem[usCBIndex]); /* when fFindSector returns with true it has the semaphore requested */
               break;
               }
            }
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

   if (!fFindSector(ulSector, pVolInfo->bDrive, &usIndex))
      return FALSE;
   f32Parms.ulTotalHits++;
   vGetSectorFromCache(pVolInfo, usIndex, pbSector);

   rc2 = FSH_SEMCLEAR(&ulLockSem[usIndex]); /* when fFindSector returns with true it has the semaphore requested */

   return TRUE;
}

/******************************************************************
*
******************************************************************/
VOID vGetSectorFromCache(PVOLINFO pVolInfo, USHORT usCBIndex, PBYTE pbSector)
{
PCACHEBASE pBase;
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
PCACHEBASE2 pBase2;

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

#if 1
#define WAIT_THRESHOLD
#endif
/******************************************************************
*
******************************************************************/
BOOL fStoreSector(PVOLINFO pVolInfo, ULONG ulSector, PBYTE pbSector, BOOL fDirty)
{
APIRET rc2;
USHORT usSlot;
USHORT usCBIndex,usCBIndexNew;
PCACHEBASE pBase;
PCACHEBASE2 pBase2;

   if (!f32Parms.usCacheSize)
      return FALSE;

   pBase = NULL;
   if (f32Parms.usCacheUsed < f32Parms.usCacheSize)
      {
      usCBIndex = f32Parms.usCacheUsed;

      rc2 = FSH_SEMREQUEST(&ulLockSem[usCBIndex],-1L);

      f32Parms.usCacheUsed++;

      pBase = pCacheBase + usCBIndex;

      usSlot = (USHORT)(ulSector % MAX_SLOTS);
      pBase->usNext = rgSlot[usSlot];
      rgSlot[usSlot] = usCBIndex;

      pBase->ulSector = ulSector;
      pBase->bDrive = pVolInfo->bDrive;

      vReplaceSectorInCache(pVolInfo, usCBIndex, pbSector, fDirty);

      rc2 = FSH_SEMCLEAR(&ulLockSem[usCBIndex]);
      }
    else
    {
      PCACHEBASE pWork;
      USHORT usIndex;
      USHORT leaveFlag = FALSE;

      if (usOldestEntry == 0xFFFF)
         FatalMessage("FAT32: No Oldest entry found!");

      /*
         find the oldest non dirty, not locked, not pending sector in cache
      */

        while (leaveFlag == FALSE)
        {
            while (f32Parms.usDirtySectors >= f32Parms.usDirtyTreshold)
            {

#ifdef WAIT_THRESHOLD
                if (f32Parms.fMessageActive & LOG_CACHE ||
                    f32Parms.fMessageActive & LOG_WAIT)
                    Message("waiting for dirty sectors to be less than threshold...");

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
#else
                Yield();
#endif
                if (f32Parms.usDirtySectors >= f32Parms.usDirtyTreshold)
                {
                    BOOL fMsg = f32Parms.fMessageActive;
                    CritMessage("FAT32:No free sectors in cache! (run MONITOR now!)");
                    Message("ERROR: fStoreSector - No sectors available!");
                    Message("       %u sectors are dirty", f32Parms.usDirtySectors);
                    f32Parms.fMessageActive = fMsg;
                }
            }

            usCBIndex = usOldestEntry;
            while (usCBIndex != 0xFFFF)
            {
                rc2 = FSH_SEMREQUEST(&ulLockSem[usCBIndex],-1L);

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
                        if (usIndex == FREE_SLOT)
                        FatalMessage("FAT32: Store: Oldest entry not found in slot chain!");
                        pWork->usNext = pBase->usNext;
                    }

                    usSlot = (USHORT)(ulSector % MAX_SLOTS);
                    pBase->usNext = rgSlot[usSlot];
                    rgSlot[usSlot] = usCBIndex;

                    pBase->ulSector = ulSector;
                    pBase->bDrive = pVolInfo->bDrive;

                    vReplaceSectorInCache(pVolInfo, usCBIndex, pbSector, fDirty);

                    leaveFlag = TRUE;
                    rc2 = FSH_SEMCLEAR(&ulLockSem[usCBIndex]);
                    break;
                }

                pBase2 = pCacheBase2 + usCBIndex;
                usCBIndexNew = pBase2->usNewer;

                rc2 = FSH_SEMCLEAR(&ulLockSem[usCBIndex]);

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
PCACHEBASE pBase;
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
PVOID GetPhysAddr(PRQLIST pRQ, ULONG ulEntry)
{
ULONG ulOffset;
USHORT usEntry;

   ulOffset = ulEntry * sizeof (CACHE);
   usEntry = (USHORT)(ulOffset / PAGE_SIZE);

   ulOffset = ulOffset % PAGE_SIZE;

   return (PVOID)(pRQ->rgPhys[usEntry] + ulOffset);
}

/******************************************************************
*
******************************************************************/
BOOL fFindSector(ULONG ulSector, BYTE bDrive, PUSHORT pusIndex)
{
APIRET rc2;
PCACHEBASE pBase;
USHORT    usCBIndex,usCBIndexNew;

   if (!f32Parms.usCacheUsed)
      return FALSE;

   usCBIndex = rgSlot[(USHORT)(ulSector % MAX_SLOTS)];

   while (usCBIndex != FREE_SLOT)
      {
      rc2 = FSH_SEMREQUEST(&ulLockSem[usCBIndex],-1L);
      pBase = pCacheBase + usCBIndex;
      if (pBase->ulSector == ulSector && pBase->bDrive == bDrive)
         {
         *pusIndex = usCBIndex;
         return TRUE;
         }
      usCBIndexNew = pBase->usNext;
      rc2 = FSH_SEMCLEAR(&ulLockSem[usCBIndex]);
      usCBIndex = usCBIndexNew;
      }

   return FALSE;
}


/******************************************************************
*
******************************************************************/
USHORT WriteCacheSector(PVOLINFO pVolInfo, USHORT usCBIndex, BOOL fSetTime)
{
PCACHEBASE pBase;
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
         rc = FSH_DOVOLIO(DVIO_OPWRITE | VerifyOn(), DVIO_ALLACK,
               pVolInfo->hVBP, pCache->bSector, &usSectors, pBase->ulSector);
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
            CritMessage("FAT32: Error %u in WriteCacheSector", rc);

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
BYTE bPriority;
LONG lWait;

   rc2 = FSH_SEMSET(&ulDummySem);

   Message("DoLW started");

   lWait = f32Parms.ulDiskIdle;
   while (!f32Parms.fInShutDown && !pOptions->fTerminate)
      {
      rc2 = FSH_SEMWAIT(&ulDummySem,lWait); /* noone will clear the semaphore, so this is just a wait */

      if (f32Parms.fLW)
         {
         switch (pOptions->bLWPrio)
            {
            case 1:
               bPriority = PRIO_LAZY_WRITE;
               break;

            case 2:
               bPriority = PRIO_BACKGROUND_USER;
               break;

            case 3:
               bPriority = PRIO_FOREGROUND_USER;
               break;

            default:
               bPriority = PRIO_PAGER_HIGH;
               break;
            }

         pVolInfo = pGlobVolInfo;
         while (pVolInfo)
            {
            ULONG ulTime = GetCurTime();

            if (ulTime < pVolInfo->ulLastDiskTime ||
                pVolInfo->ulLastDiskTime + f32Parms.ulDiskIdle <= ulTime)

               usFlushVolume(pVolInfo, FLUSH_RETAIN, FALSE, bPriority);
            pVolInfo = (PVOLINFO)pVolInfo->pNextVolInfo;
            }
         }
      }

   f32Parms.fLW = FALSE;
   usFlushAll();
   Message("DoLW Stopped, Lazy writing also");

   rc2 = FSH_SEMCLEAR(&ulDummySem);
}

#if 0
#define WAIT_PENDINGFLUSH
#endif

/******************************************************************
*
******************************************************************/
USHORT usFlushVolume(PVOLINFO pVolInfo, USHORT usFlag, BOOL fFlushAll, BYTE bPriority)
{
APIRET rc2;
USHORT usCBIndex = 0,usCBIndexNew;
PCACHEBASE pBase;
PCACHEBASE2 pBase2;
USHORT usCount;
ULONG  ulCurTime = GetCurTime();

   if (!f32Parms.usCacheUsed )
      return 0;

   usCount = 0;

   if (!fFlushAll)
      {
      usCBIndex = usOldestEntry;
      while ((usCBIndex != 0xFFFF) && (FSH_SEMREQUEST(&ulLockSem[usCBIndex],0L) == NO_ERROR))
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
         rc2 = FSH_SEMCLEAR(&ulLockSem[usCBIndex]);
         usCBIndex = usCBIndexNew;
         }
      if (f32Parms.fMessageActive & LOG_CACHE && usCount > 0)
         Message("%u sectors LAZY flushed, still %u dirty", usCount, f32Parms.usDirtySectors);

      return 0;
      }

   if (f32Parms.fMessageActive & LOG_CACHE)
      Message("usFlushVolume ALL");

   usCBIndex = 0;
   for (usCBIndex = 0; ( f32Parms.usDirtySectors || usFlag == FLUSH_DISCARD ) &&
         usCBIndex < f32Parms.usCacheUsed; usCBIndex++)
      {
      rc2 = FSH_SEMREQUEST(&ulLockSem[usCBIndex],-1L);

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

         rc2 = FSH_SEMCLEAR(&ulLockSem[usCBIndex]);
      }

   if (f32Parms.fMessageActive & LOG_CACHE)
      Message("%u sectors flushed, still %u dirty", usCount, f32Parms.usDirtySectors);
   return 0;
}

/******************************************************************
*
******************************************************************/
USHORT usEmergencyFlush(VOID)
{
APIRET rc2;
USHORT usCBIndex = 0,usCBIndexNew;
PCACHEBASE pBase;
PCACHEBASE2 pBase2;
USHORT usCount;
PVOLINFO pVolInfo;

   if (!f32Parms.usCacheUsed)
      return 0;

   /*
      find the volinfo for the oldest entry
   */
   pVolInfo = NULL;
   usCBIndex = usOldestEntry;
   while (usCBIndex != 0xFFFF)
      {
      rc2 = FSH_SEMREQUEST(&ulLockSem[usCBIndex],-1L);

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
      rc2 = FSH_SEMCLEAR(&ulLockSem[usCBIndex]);
      usCBIndex = usCBIndexNew;
      }

   if (usCBIndex == 0xFFFF)
      return 0;

   if (!pVolInfo)
      return 0;

   usCount = 0;
   while (usCBIndex != 0xFFFF)
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
      rc2 = FSH_SEMCLEAR(&ulLockSem[usCBIndex]);
      usCBIndex = usCBIndexNew;
      if (usCBIndex != 0xFFFF)
        rc2 = FSH_SEMREQUEST(&ulLockSem[usCBIndex],-1L);
      }
   if (usCBIndex != 0xFFFF)
   {
      rc2 = FSH_SEMCLEAR(&ulLockSem[usCBIndex]);
   }

   pVolInfo->ulLastDiskTime = GetCurTime();

   if (f32Parms.fMessageActive & LOG_CACHE)
      Message("usEmergencyFlush: %u sectors flushed, still %u dirty", usCount, f32Parms.usDirtySectors);

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

USHORT GetReadAccess(PVOLINFO pVolInfo, PSZ pszName)
{
USHORT rc;

   pVolInfo = pVolInfo;

   Message("GetReadAccess: %s", pszName);
   rc = SemRequest(&ulSemRWRead, TO_INFINITE, pszName);
   if (rc)
      {
      Message("ERROR: SemRequest GetReadAccess Failed, rc = %d!", rc);
      CritMessage("FAT32: SemRequest GetReadAccess Failed, rc = %d!", rc);
      Message("GetReadAccess Failed for %s, rc = %d", pszName, rc);
      return rc;
      }
   return 0;
}

VOID ReleaseReadBuf(PVOLINFO pVolInfo)
{
   pVolInfo = pVolInfo;
   Message("ReleaseReadBuf");
   FSH_SEMCLEAR(&ulSemRWRead);
}
