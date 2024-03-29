#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <dos.h>

#define INCL_DOS
#define INCL_DOSDEVIOCTL
#define INCL_DOSDEVICES
#define INCL_DOSERRORS

#include "os2.h"
#include "portable.h"
#include "fat32ifs.h"

#define HEAP_MAGIC     0xdead
#define HEAP_SIZE      (0xFFF0)
#define MAX_SELECTORS  100
#define RESERVED_SEGMENT (PVOID)0x00000001


static BOOL   IsBlockFree(PBYTE pMCB);
static ULONG  BlockSize(PBYTE pMCB);
static VOID   SetFree(PBYTE pMCB);
static VOID   SetInUse(PBYTE pMCB);
static VOID SetMagic(PBYTE pMCB);
static ULONG GetMagic(PBYTE pMCB);
static VOID   SetBlockSize(PBYTE pMCB, ULONG ulSize);
static void * FindFreeSpace(void * pStart, size_t tSize);
static VOID GetMemAccess(VOID);
static VOID ReleaseMemAccess(VOID);

static void * rgpSegment[MAX_SELECTORS] = {0};

static BOOL fLocked = FALSE;
static ULONG ulMemSem = 0UL;

extern UCHAR fRing3;

VOID CheckHeap(VOID)
{
USHORT usSel;
PBYTE pPrev = NULL;
BYTE _huge * pHeapStart;
BYTE _huge * pHeapEnd;
BYTE _huge * pWork;
USHORT rc;

   for (usSel = 0; usSel < MAX_SELECTORS; usSel++)
      {
      pHeapStart = rgpSegment[usSel];
      if (!pHeapStart || pHeapStart == RESERVED_SEGMENT)
         continue;
      rc = MY_PROBEBUF(PB_OPREAD, (PBYTE)pHeapStart, HEAP_SIZE);
      if (rc)
         {
         //CritMessage("FAT32: Protection VIOLATION in CheckHeap (SYS%d)", rc);
         Message("FAT32: Protection VIOLATION in CheckHeap (SYS%d)", rc);
         return;
         }
      pHeapEnd = pHeapStart + HEAP_SIZE;
      pWork = pHeapStart;
      while (pWork < pHeapEnd)
      {
         if (GetMagic(pWork) != HEAP_MAGIC)
            {
            Message("pWork=%lx, size=%lx, magic=%x", pWork, BlockSize(pWork), (USHORT)GetMagic(pWork));
            Message("pPrev=%lx, size=%lx, magic=%x", pPrev, BlockSize(pPrev), (USHORT)GetMagic(pWork));
            FatalMessage("FAT32: Heap corruption found!");
            }

         pPrev = pWork;
         pWork += BlockSize(pWork) + sizeof (ULONG);
      }
      if (pWork != pHeapEnd)
         FatalMessage("FAT32: Heap corruption found!");
      }
}

void *alloc3(size_t tSize)
{
   SEL sel;
   DosAllocSeg(tSize, &sel, 0);
   return MAKEP(sel, 0);
}

void free3(void *p)
{
   DosFreeSeg(SELECTOROF(p));
}

/*********************************************************************
* malloc
*********************************************************************/
#ifdef __WATCOM
_WCRTLINK void * malloc(size_t tSize)
#else
void * cdecl malloc(size_t tSize)
#endif
{
USHORT usSel;
void * pRet;

   if (fRing3)
      {
      return alloc3(tSize);
      }

   //CheckHeap();

   GetMemAccess();

   if (tSize % 2)
      tSize++;

if( tSize > 0 )
   {
   for (usSel = 0; usSel < MAX_SELECTORS; usSel++)
      {
      if (rgpSegment[usSel] && rgpSegment[usSel] != RESERVED_SEGMENT)
         {
         pRet = FindFreeSpace(rgpSegment[usSel], tSize);
         if (pRet)
            goto malloc_exit;
         }
      }

   for (usSel = 0; usSel < MAX_SELECTORS; usSel++)
      {
      if (!rgpSegment[usSel])
         {
         rgpSegment[usSel] = RESERVED_SEGMENT;
         rgpSegment[usSel] = gdtAlloc(HEAP_SIZE, TRUE);
         if (!rgpSegment[usSel])
            {
            FatalMessage("FAT32: No gdtSelector for heap!");
            pRet = NULL;
            goto malloc_exit;
            }
         SetBlockSize(rgpSegment[usSel], HEAP_SIZE - sizeof (ULONG));
         SetMagic(rgpSegment[usSel]);
         SetFree(rgpSegment[usSel]);

         pRet = FindFreeSpace(rgpSegment[usSel], tSize);
         if (pRet)
            goto malloc_exit;
         }
      }
   }

   MessageL(LOG_MEM, "Malloc failed, calling gdtAlloc%m", 0x4069);
   pRet = gdtAlloc(tSize ? ( ULONG )tSize : 65536L, TRUE);

malloc_exit:

   MessageL(LOG_MEM, "malloc%m %lu bytes at %lX", 0x806a, tSize ? ( ULONG )tSize : 65536L, pRet);

   ReleaseMemAccess();
   return pRet;
}

/*********************************************************************
* FindFreeSpace
*********************************************************************/
void * FindFreeSpace(void * pStart, size_t tSize)
{
BYTE _huge * pHeapStart;
BYTE _huge * pHeapEnd;
BYTE _huge * pWork;
BYTE _huge * pNext;
USHORT rc;

   pHeapStart = pStart;
   pHeapEnd = pHeapStart + HEAP_SIZE;

   rc = MY_PROBEBUF(PB_OPREAD, (PBYTE)pHeapStart, HEAP_SIZE);
   if (rc)
      {
      //CritMessage("FAT32: Protection VIOLATION in FindFreeSpace (SYS%d)", rc);
      Message("FAT32: Protection VIOLATION in FindFreeSpace (SYS%d)", rc);
      return NULL;
      }

   pWork = pHeapStart;
   while (pWork < pHeapEnd)
      {
      if (BlockSize(pWork) >= tSize && IsBlockFree(pWork))
         {
         ULONG ulRemaining = BlockSize(pWork) - tSize;
         if (ulRemaining > sizeof (ULONG))
            {
            pNext = pWork + sizeof (ULONG) + tSize;
            SetBlockSize(pNext, BlockSize(pWork) - tSize - sizeof (ULONG));
            SetFree(pNext);
            SetMagic(pNext);
            SetBlockSize(pWork, tSize);
            SetMagic(pWork);
            }

         SetInUse(pWork);
         SetMagic(pWork);
         return pWork + sizeof (ULONG);
         }
      pWork += BlockSize(pWork) + sizeof (ULONG);
      }
   return NULL;
}

#ifdef __WATCOM
_WCRTLINK void free(void * pntr)
#else
void cdecl free(void * pntr)
#endif
{
USHORT usSel;
BYTE _huge * pHeapStart;
BYTE _huge * pHeapEnd;
BYTE _huge * pWork;
BYTE _huge * pToFree = pntr;
BYTE _huge * pPrev;
BYTE _huge * pNext;
USHORT rc;

   MessageL(LOG_MEM, "free%m %lX", 0x006c, pntr);

   if (fRing3)
      {
      free3(pntr);
      return;
      }

   //CheckHeap();

   if (OFFSETOF(pntr) == 0)
      {
      freeseg(pntr);
      return;
      }

   GetMemAccess();

   for (usSel = 0; usSel < MAX_SELECTORS; usSel++)
      {
      if (SELECTOROF(pntr) == SELECTOROF(rgpSegment[usSel]))
         break;
      }
   if (usSel == MAX_SELECTORS)
      {
      //CritMessage("FAT32: %lX not found in free!", pntr);
      Message("FAT32: %lX not found in free!", pntr);
      ReleaseMemAccess();
      return;
      }

   pHeapStart = rgpSegment[usSel];
   pHeapEnd = pHeapStart + HEAP_SIZE;

   rc = MY_PROBEBUF(PB_OPREAD, (PBYTE)pHeapStart, HEAP_SIZE);
   if (rc)
      {
      //CritMessage("FAT32: Protection VIOLATION in free (SYS%d)", rc);
      Message("FAT32: Protection VIOLATION in free (SYS%d)", rc);
      ReleaseMemAccess();
      return;
      }

   pWork = pHeapStart;
   pPrev = NULL;
   while (pWork < pHeapEnd)
      {
      if (pWork + sizeof (ULONG) == pToFree)
         {
         if (pPrev && IsBlockFree(pPrev))
            {
            SetBlockSize(pPrev,
               BlockSize(pPrev) + BlockSize(pWork) + sizeof (ULONG));
            pWork = pPrev;
            }

         pNext = pWork + BlockSize(pWork) + sizeof (ULONG);
         if (pNext < pHeapEnd && IsBlockFree(pNext))
            SetBlockSize(pWork, BlockSize(pWork) + BlockSize(pNext) + sizeof (ULONG));

         SetMagic(pWork);
         SetFree(pWork);
         break;
         }
      else
         pPrev = pWork;

      pWork += BlockSize(pWork) + sizeof (ULONG);
      }
   if (pWork >= pHeapEnd)
      {
      //CritMessage("FAT32: ERROR: Address not found in free");
      Message("ERROR: Address not found in free");
      ReleaseMemAccess();
      return;
      }

   /*
      free selector if no longer needed
   */
   if (usSel > 0 &&
      BlockSize(rgpSegment[usSel]) == (HEAP_SIZE - sizeof (ULONG)) &&
      IsBlockFree(rgpSegment[usSel]))
      {
      PBYTE p = rgpSegment[usSel];
      rgpSegment[usSel] = NULL;
      freeseg(p);
      }
   ReleaseMemAccess();
}

BOOL IsBlockFree(PBYTE pMCB)
{
   return (BOOL)(*((PUSHORT)pMCB + 1) & (USHORT)1);
   //return (BOOL)(*((ULONG)pMCB) & 1L);
}


ULONG BlockSize(PBYTE pMCB)
{
   return *((PUSHORT)pMCB + 1) & (USHORT)~1;
   //return *((PULONG)pMCB) & ~1L;
}

ULONG GetMagic(PBYTE pMCB)
{
   return *((PUSHORT)pMCB);
}

VOID SetMagic(PBYTE pMCB)
{
   *((PUSHORT)pMCB) = HEAP_MAGIC;
}

VOID SetFree(PBYTE pMCB)
{
   *((PUSHORT)pMCB + 1) |= (USHORT)1;
   //*((PULONG)pMCB) |= 1L;
}

VOID SetInUse(PBYTE pMCB)
{
   *((PUSHORT)pMCB + 1) &= (USHORT)~1;
   //*((PULONG)pMCB) &= ~1L;
}

VOID SetBlockSize(PBYTE pMCB, ULONG ulSize)
{
BOOL fFree = IsBlockFree(pMCB);

   *((PUSHORT)pMCB + 1) = (USHORT)ulSize;
   //*((PULONG)pMCB) = ulSize;

   if (fFree)
      SetFree(pMCB);
}

VOID GetMemAccess(VOID)
{
    APIRET rc;

    MessageL(LOG_WAIT, "Waiting for a heap access%m", 0x4070);

    rc = FSH_SEMREQUEST(&ulMemSem,-1);
    if (rc == NO_ERROR)
    {
        fLocked = TRUE;
    }
}

VOID ReleaseMemAccess(VOID)
{
    APIRET rc;

    if (fLocked)
    {
        rc = FSH_SEMCLEAR(&ulMemSem);
    }
}
