/*
 * $Header$
 */

#ifndef _OS2HEAD_H_
#define _OS2HEAD_H_

#define INCL_NOBASEAPI
#define INCL_NOPMAPI
#include <os2.h>

#define INCL_INITRP_ONLY
#include <reqpkt.h>
#include <iorb.h>

#ifndef FAR
#define FAR _far
#endif

#define _enable()	_asm { sti }
#define _disable()	_asm { cli }

#define ERROR_I24_QUIET_INIT_FAIL       21	/* from bseerr.h */

#define IOERR_NO_SUCH_UNIT		IOERR_UNIT_NOT_ALLOCATED

#define CMDInit              0                 /* init                            */
#define CMDMedChk            1                 /* media check                     */
#define CMDBldBPB            2                 /* build BPB                       */
#define CMDIOCTLR            3                 /* reserved                        */
#define CMDINPUT             4                 /* read                            */
#define CMDNDR               5                 /* non-destructive read            */
#define CMDInputS            6                 /* input status                    */
#define CMDInputF            7                 /* flush input                     */
#define CMDOUTPUT            8                 /* write                           */
#define CMDOUTPUTV           9                 /* write and verify                */
#define CMDOutputS           10                /* output status                   */
#define CMDOutputF           11                /* flush output                    */
#define CMDIOCTLW            12                /* reserved                        */
#define CMDOpen              13                /* open                            */
#define CMDClose             14                /* close                           */
#define CMDRemMed            15                /* is media removable?             */
#define CMDGenIOCTL          16                /* gen. ioctl                      */
#define CMDResMed            17                /* reset media                     */
#define CMDGetLogMap         18                /* get logical map                 */
#define CMDSetLogMap         19                /* set logfical map                */
#define CMDDeInstall         20                /* deinstall driver                */
#define CMDPartfixeddisks    22                /* partitionable fixed disks       */
#define CMDGetfd_logunitsmap 23                /* get fixed disk logical unit map */
#define CMDInputBypass       24                /* input bypass                    */
#define CMDOutputBypass      25                /* output bypass                   */
#define CMDOutputBypassV     26                /* output bypass / verify          */
#define CMDInitBase          27                /* init basedev                    */
#define CMDShutdown          28                /* shutdown                        */
#define CMDGetDevSupport     29                /* query extended capabilities     */
#define CMDInitComplete      31                /* init complete                   */
#define CMDSaveRestore       32                /* save / restore                  */
#define CMDAddOnPrep         97                /* prepare for add-on              */
#define CMDStar              98                /* start console output            */
#define CMDStop              99                /* stop console output             */

#define MAXDEVCLASSTABLES    2
#define MAXMOUSEDCENTRIES    3
#define MAXDEVCLASSNAMELEN   16
#define MAXDISKDCENTRIES     32

struct DevClassTableEntry
{
   USHORT   DCOffset;
   USHORT   DCSelector;
   USHORT   DCFlags;
   UCHAR    DCName[MAXDEVCLASSNAMELEN];
};

struct DevClassTableStruc
{
   USHORT                     DCCount;
   USHORT                     DCMaxCount;
   struct DevClassTableEntry  DCTableEntries[1];
};

typedef struct DevClassTableEntry FAR *PDevClassTableEntry;
typedef struct DevClassTableStruc FAR *PDevClassTableStruc;

typedef VOID (FAR* PADDEntryPoint)(PIORB pSomeIORB);
typedef PIORB (FAR* PNotifyAddr)(PIORB pSomeIORB);

#define	MAKE_SEC_CYL(SEC,CYL)		(((SEC)&0x3F)|(((CYL)&0x300)>>2)|\
					 (((CYL)&0xFF)<<8))

/* Modified: original file was wrong. */
typedef struct _PARTITIONENTRY
{
   UCHAR        BootIndicator;
   UCHAR        BegHead;
   USHORT       BegSecCyl;
   UCHAR        SysIndicator;
   UCHAR        EndHead;
   USHORT       EndSecCyl;
   ULONG        RelativeSectors;
   ULONG        NumSectors;
} PARTITIONENTRY;

typedef struct _MBR
{
   UCHAR          Pad[0x1BE];
   PARTITIONENTRY PartitionTable[4];
   USHORT         Signature;
} MBR;

typedef struct _DDD_Parm_List
{
  USHORT cache_parm_list;
  USHORT disk_config_table;
  USHORT init_req_vec_table;
  USHORT cmd_line_args;
  USHORT machine_config_table;
} DDD_PARM_LIST, far *PDDD_PARM_LIST;

/* 
 * sti and cli are a builtin function in MS Visual C++  (M.Willm, 1995-11-14)
 */
#ifndef __MSC__
#define ENABLE	_asm { sti }
#define DISABLE	_asm { cli }
#else
#define ENABLE  _enable();
#define DISABLE _disable();
#endif

#endif
