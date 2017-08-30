#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#define INCL_DOS
#define INCL_DOSDEVIOCTL
#define INCL_DOSDEVICES
#define INCL_DOSERRORS

#include "os2.h"
#include "portable.h"
#include "fat32ifs.h"

VOID vCallStrategy2(PVOLINFO pVolInfo, struct pagereq far *pgreq);
ULONG PositionToOffset(PVOLINFO pVolInfo, POPENINFO pOpenInfo, LONGLONG llOffset);
int GetBlockNum(PVOLINFO pVolInfo, POPENINFO pOpenInfo, ULONG ulOffset, PULONG pulBlkNo);
void pageIOdone(struct pagereq far * prp);

#define PSIZE           4096
#define MAXPGREQ	16

// hVPB of volume with a swap file
unsigned short swap_hVPB = 0;

/* Request list to be passed to the driver's strategy2 routine.
 */
struct pagereq
{
   Req_List_Header pr_hdr;     /* 20: */
   struct _pr_list
   {
      PB_Read_Write pr_rw;     /* 52: */
      SG_Descriptor pr_sg;     /* 8: */
   } pr_list[MAXPGREQ];        /* 480: (60*8) */
};                             /* (500) */

struct pagereq pgreq;

static ULONG ulSemRWSwap = 0;


/******************************************************************
*
******************************************************************/
int far pascal _loadds FS_OPENPAGEFILE (
    unsigned long far *pFlags,      /* pointer to Flags           */
    unsigned long far *pcMaxReq,    /* max # of reqs packed in list   */
    char far *         pName,       /* name of paging file        */
    struct sffsi far * psffsi,      /* ptr to fs independent SFT      */
    struct sffsd far * psffsd,      /* ptr to fs dependent SFT        */
    unsigned short     OpenMode,    /* sharing, ...           */
    unsigned short     OpenFlag,    /* open flag for action       */
    unsigned short     Attr,        /* file attribute             */
    unsigned long      Reserved     /* reserved, must be zero         */
)
{
   int             rc;
   USHORT          dummyAction, dummyFlag;
   struct cdfsi    dummyCds;
   PVOLINFO        pVolInfo;

   _asm push es;

   MessageL(LOG_FS, "FS_OPENPAGEFILE%m  pName=%s OpenMode=%x, OpenFlag=%x Attr=%x",
            0x0002, pName, OpenMode, OpenFlag, Attr);

   pVolInfo = GetVolInfo(psffsi->sfi_hVPB); 

   if (! pVolInfo)
      {
      rc = ERROR_INVALID_DRIVE;
      goto FS_OPENPAGEFILE_EXIT;
      }

   if (pVolInfo->fFormatInProgress)
      {
      rc = ERROR_ACCESS_DENIED;
      goto FS_OPENPAGEFILE_EXIT;
      }

   /* Keep track of volume with swap-space.  We can't allow this volume
    * to be quiesced.
    */
   swap_hVPB = psffsi->sfi_hVPB;

   /* pathlookup needs the hVPB in the current directory structure
    * to figure out where to start.  conjure up a cds with just the
    * needed information.
    */
   dummyCds.cdi_hVPB = psffsi->sfi_hVPB;

   /* do a regular open or create
    */
   rc = FS_OPENCREATE(&dummyCds, NULL, pName, -1,
                      psffsi, psffsd, OpenMode, OpenFlag,
                      &dummyAction, Attr, NULL, &dummyFlag);

   if (rc == 0)
      {
      if (pVolInfo->pfnStrategy)
         {
            /*   Strat2 is supported.
             *   set return information:
             *   pageio requests require physical addresses;
             *   maximum request is 16 pages;
             */
            *pFlags |= PGIO_PADDR;
         }
      else
         {
            // no Strat2
            *pFlags |= PGIO_VADDR;
         }

      *pcMaxReq = MAXPGREQ;

      if (*pFlags & PGIO_FIRSTOPEN)
         {
         // make swap file zero-aligned
         rc = FS_NEWSIZEL (psffsi, psffsd, 0LL, 0x10);

         if (rc)
            {
            goto FS_OPENPAGEFILE_EXIT;
            }
         }
      }

FS_OPENPAGEFILE_EXIT:
   MessageL(LOG_FS, "FS_OPENPAGEFILE%m returned %u\n", 0x8002, rc);

   _asm pop es;

   return rc;
}


/******************************************************************
*
******************************************************************/
int far pascal _loadds FS_ALLOCATEPAGESPACE(
    struct sffsi far *psffsi,       /* ptr to fs independent SFT */
    struct sffsd far *psffsd,       /* ptr to fs dependent SFT   */
    unsigned long     ulSize,       /* new size          */
    unsigned long     ulWantContig   /* contiguous chunk size     */
)
{
   int rc;
   ULONGLONG ullSize;
   PVOLINFO  pVolInfo;

#ifdef INCL_LONGLONG
   ullSize = (ULONGLONG)ulSize;
#else
   AssignUL(&ullSize, ulSize);
#endif

   _asm push es;

   MessageL(LOG_FS, "FS_ALLOCATEPAGESPACE%m  size=%lu contig=%lu", 0x0003, ulSize, ulWantContig);

   pVolInfo = GetVolInfo(psffsi->sfi_hVPB); 

   if (! pVolInfo)
      {
      rc = ERROR_INVALID_DRIVE;
      goto FS_ALLOCATEPAGESPACE_EXIT;
      }

   if (pVolInfo->fFormatInProgress)
      {
      rc = ERROR_ACCESS_DENIED;
      goto FS_ALLOCATEPAGESPACE_EXIT;
      }

   if (ulWantContig > pVolInfo->ulClusterSize)
      {
      rc = ERROR_DISK_FULL;
      goto FS_ALLOCATEPAGESPACE_EXIT;
      }

   rc = FS_NEWSIZEL(psffsi, psffsd, ullSize, 0x10);

   if (rc == 0)
      {
      psffsi->sfi_size = ulSize;

      if (f32Parms.fLargeFiles)
         {
#ifdef INCL_LONGLONG
         psffsi->sfi_sizel = ullSize;
#else
         iAssign(&psffsi->sfi_sizel, *(PLONGLONG)&ullSize);
#endif
         }
      }

FS_ALLOCATEPAGESPACE_EXIT:
   MessageL(LOG_FS, "FS_ALLOCATEPAGESPACE%m returned %u\n", 0x8003, rc);

   _asm pop es;

   return rc;
}

/******************************************************************
*
******************************************************************/
int far pascal _loadds FS_DOPAGEIO(
    struct sffsi far *         psffsi,      /* ptr to fs independent SFT    */
    struct sffsd far *         psffsd,      /* ptr to fs dependent SFT      */
    struct PageCmdHeader far * pPageCmdList /* ptr to list of page commands */
)
{
   POPENINFO       pOpenInfo;
   PVOLINFO        pVolInfo;
   ULONG           fsbno;  /* starting block number to read/write */
   struct PageCmd  *pgcmd; /* pointer to current command in list */
   Req_List_Header *rlhp;  /* pointer to request list header */
   Req_Header      *rhp;   /* pointer to request header */
   PB_Read_Write   *rwp;   /* pointer to request */
   int i, j, rc = NO_ERROR;
   USHORT usSectors;

   _asm push es;

   MessageL(LOG_FS, "FS_DOPAGEIO%m\n", 0x0004);

   /* Serialize on the swap file inode.
    */
   //IWRITE_LOCK(ip);

   pVolInfo = GetVolInfo(psffsi->sfi_hVPB); 

   if (! pVolInfo)
      {
      rc = ERROR_INVALID_DRIVE;
      goto FS_DOPAGEIO_EXIT;
      }

   if (pVolInfo->fFormatInProgress)
      {
      rc = ERROR_ACCESS_DENIED;
      goto FS_DOPAGEIO_EXIT;
      }

   pOpenInfo = GetOpenInfo(psffsd);

   /* sectors per page */
   usSectors = PSIZE / pVolInfo->BootSect.bpb.BytesPerSector;

   if (pVolInfo->pfnStrategy)
      {
      // use strat2 routine
      memset(&pgreq, 0, sizeof(pgreq));

      rlhp = &pgreq.pr_hdr;
      rlhp->Count = pPageCmdList->OpCount;
      rlhp->Notify_Address = (void far *)pageIOdone;
      rlhp->Request_Control = (pPageCmdList->InFlags & PGIO_FI_ORDER)
                               ? RLH_Notify_Done | RLH_Exe_Req_Seq
                               : RLH_Notify_Done;
      rlhp->Block_Dev_Unit = pVolInfo->bUnit;

      /* Fill in a request for each command in the input list.
       */
      //assert(pPageCmdList->OpCount > 0);

      for (i = 0, pgcmd = pPageCmdList->PageCmdList;
           i < (int)pPageCmdList->OpCount;
           i++, pgcmd++)
         {
         /* Fill in request header.
          * These fields are set to zero by memset, above:
          *	rhp->Req_Control
          *	rhp->Status
          *	rwp->RW_Flags
          */
         rhp = &pgreq.pr_list[i].pr_rw.RqHdr;
         rhp->Length = sizeof(struct _pr_list);
         rhp->Old_Command = PB_REQ_LIST;
         rhp->Command_Code = pgcmd->Cmd;
         rhp->Head_Offset = (ULONG)rhp - (ULONG)rlhp;
         rhp->Priority = pgcmd->Priority;
         rhp->Hint_Pointer = -1;

         /* Fill in read/write request.
          */
         rwp = &pgreq.pr_list[i].pr_rw;
         rc = GetBlockNum(pVolInfo, pOpenInfo, pgcmd->FileOffset, &fsbno);
         if (rc)
            {
            /* request is not valid, return error */
            goto FS_DOPAGEIO_EXIT;
            }
         rwp->Start_Block = fsbno;
         rwp->Block_Count = usSectors;
         rwp->SG_Desc_Count = 1;

         /* Fill in the scatter/gather descriptor
          */
         pgreq.pr_list[i].pr_sg.BufferPtr = (void *)pgcmd->Addr;
         pgreq.pr_list[i].pr_sg.BufferSize = PSIZE;
         }

      /* Length in last request must be set to terminal value.
       */
      rhp->Length = RH_LAST_REQ;

      //IS_QUIESCE(ip->i_ipmnt->i_cachedev);	/* Block if hard quiesce */

      ulSemRWSwap = 0;

      vCallStrategy2(pVolInfo, &pgreq);

      /* Wait for the request to complete.
       */
      //PAGING_LOCK();
      //if (ulSemRWSwap)
      //   {
         rc = FSH_SEMREQUEST(&ulSemRWSwap, TO_INFINITE);
      //   PAGING_NOLOCK();
      //   }
      //else
      //   PAGING_UNLOCK();

      /* If hard quiesce is in progress, and this is the last pending I/O,
       * wake up the quiescing thread
       */
      //ipri = IOCACHE_LOCK();
      //cdp = ip->i_ipmnt->i_cachedev;
      //if ((--cdp->cd_pending_requests == 0) && (cdp->cd_flag & CD_QUIESCE))
      //    IOCACHE_WAKEUP(&cdp->cd_iowait);
      //IOCACHE_UNLOCK(ipri);

      /* Check for errors and update status info in the command list.
       * Set return value to error code from first failing command.
       */
      rc = 0;
      for (i = 0; i < (int)pPageCmdList->OpCount; i++)
         {
         pgcmd = &pPageCmdList->PageCmdList[i];
         rhp = &pgreq.pr_list[i].pr_rw.RqHdr;

         pgcmd->Status = rhp->Status;
         pgcmd->Error = rhp->Error_Code;
         if ((rc == 0) && (pgcmd->Error != 0))
            rc = pgcmd->Error;
         }
      }
   else
      {
      // use strat1 routine
      for (i = 0, pgcmd = pPageCmdList->PageCmdList;
           i < (int)pPageCmdList->OpCount;
           i++, pgcmd++)
         {
         // read/write operation
         USHORT op;

         switch (pgcmd->Cmd)
            {
            case PB_READ_X:
            case PB_PREFETCH_X:
               op = DVIO_OPREAD;
               break;
            case PB_WRITE_X:
               op = DVIO_OPWRITE;
               break;
            case PB_WRITEV_X:
               op = DVIO_OPWRITE | DVIO_OPVERIFY;
            }

         op |= DVIO_OPNCACHE;

         // get starting sector number
         rc = GetBlockNum(pVolInfo, pOpenInfo, pgcmd->FileOffset, &fsbno);
         if (rc)
            {
            /* request is not valid, return error */
            goto FS_DOPAGEIO_EXIT;
            }

         // read it
         rc = FSH_DOVOLIO( op, DVIO_ALLACK, pVolInfo->hVBP, (char far *)pgcmd->Addr, &usSectors, fsbno );
         if (rc)
            {
            //FatalMessage("FAT32: DOPAGEIO of sector %ld (%d sectors) failed, rc = %u",
            //   fsbno, usSectors, rc);
            Message("ERROR: DOPAGEIO of sector %ld (%d sectors) failed, rc = %u",
               fsbno, usSectors, rc);
            }
         }
      }

FS_DOPAGEIO_EXIT:
   MessageL(LOG_FS, "FS_DOPAGEIO%m returned %u\n", 0x8004, rc);

   //IWRITE_UNLOCK(ip);
   _asm pop  es;

   return rc;
}



/******************************************************************
*
******************************************************************/
int far pascal _loadds FS_SETSWAP(
    struct sffsi far * psffsi,      /* psffsi   */
    struct sffsd far * psffsd       /* psffsd   */
)
{
   int rc = NO_ERROR;

   _asm push es;

   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_SETSWAP\n");

   if (f32Parms.fMessageActive & LOG_FS)
      Message("FS_SETSWAP returned %u\n", rc);

   _asm pop  es;

   return rc;
}

/******************************************************************
*
******************************************************************/
int GetBlockNum(PVOLINFO pVolInfo, POPENINFO pOpenInfo, ULONG ulOffset, PULONG pulBlkNo)
{
   ULONG ulCluster;
   LONGLONG llOffset;
#ifdef INCL_LONGLONG
   llOffset = (LONGLONG)ulOffset;
#else
   iAssignUL(&llOffset, ulOffset);
#endif

   // get cluster number from file offset
   ulCluster = PositionToOffset(pVolInfo, pOpenInfo, llOffset);

   if (ulCluster == pVolInfo->ulFatEof)
       return ERROR_SECTOR_NOT_FOUND;

   *pulBlkNo = pVolInfo->BootSect.bpb.HiddenSectors + pVolInfo->ulStartOfData +
      (ulCluster - 2) * pVolInfo->SectorsPerCluster;

   return NO_ERROR;
}

/******************************************************************
*
******************************************************************/
void vCallStrategy2(PVOLINFO pVolInfo, struct pagereq far *pgreq)
{
   STRATFUNC pfnStrategy;
   USHORT usSeg;
   USHORT usOff;

   if (!pgreq)
      return;

   _asm push es;
   _asm push bx;

   if (f32Parms.fMessageActive & LOG_FS && pgreq->pr_hdr.Count)
      Message("vCallStrategy2 drive %c:, %lu sectors",
              pgreq->pr_hdr.Block_Dev_Unit + 'A',
              pgreq->pr_hdr.Count);

   if (!pgreq->pr_hdr.Count)
      {
      FSH_SEMCLEAR(&ulSemRWSwap);
      //DevHelp_ProcRun((ULONG)GetRequestList, &usCount);
      return;
      }

   usSeg = SELECTOROF(pgreq);
   usOff = OFFSETOF(pgreq) + offsetof(struct pagereq, pr_hdr);

   pfnStrategy = pVolInfo->pfnStrategy;

   _asm mov es, usSeg;
   _asm mov bx, usOff;

   (*pfnStrategy)();

   _asm pop   bx;
   _asm pop   es;
}

/*
 * This is called from on 16-bit stack from strat2
 */
void
pageIOdone(struct pagereq far * prp)
{
   FSH_SEMCLEAR(&ulSemRWSwap);
}
