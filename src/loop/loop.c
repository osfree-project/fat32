#define INCL_LONGLONG
#define INCL_DOSERRORS
#define INCL_DOSINFOSEG
#define INCL_NOBASEAPI
#define INCL_NOPMAPI
#include <os2.h>

#include "lvm_info.h"

typedef USHORT APIRET;
typedef long long LONGLONG, *PLONGLONG;
typedef unsigned long long ULONGLONG, *PULONGLONG;

#include <devhelp.h>
#include <strat2.h>
#include <devcmd.h>
#include <devhdr.h>
#include <reqpkt.h>
#include <iorb.h>
#include <fsd.h>

#define ERROR_I24_BAD_COMMAND           3
#define ERROR_I24_CHAR_CALL_INTERRUPTED 17
#define ERROR_I24_INVALID_PARAMETER     19

#define CAT_LOOP 0x85

#define FUNC_MOUNT           0x10
#define FUNC_DAEMON_STARTED  0x11
#define FUNC_DAEMON_STOPPED  0x12
#define FUNC_DAEMON_DETACH   0x13
#define FUNC_GET_REQ         0x14
#define FUNC_DONE_REQ        0x15

#define FUNC_FIRST FUNC_MOUNT
#define FUNC_LAST  FUNC_DONE_REQ

#define OP_OPEN  0UL
#define OP_CLOSE 1UL
#define OP_READ  2UL
#define OP_WRITE 3UL

#pragma pack(1)

typedef struct _exbuf
{
    ULONG buf;
} EXBUF, far *PEXBUF;

typedef struct _cpdata
{
    ULONG rc;
    ULONG hf;
    ULONG Op;
    ULONG cbData;
    LONGLONG llOffset;
    UCHAR pFmt[8];
    UCHAR Buf[32768];
} CPDATA, far *PCPDATA;

typedef struct _Mountoptions
{
    UCHAR pFilename[CCHMAXPATHCOMP];
    UCHAR pMntPoint[CCHMAXPATHCOMP];
    UCHAR pFmt[8];
    ULONGLONG ullOffset;
    ULONGLONG ullSize;
    ULONG ulBytesPerSector;
    ULONG hf;
    USHORT usOp;
} MNTOPTS, *PMNTOPTS;

// unit data
struct unit
{
    UCHAR szName[260];
    BYTE bAllocated;
    BYTE bMounted;
    BYTE lock_state;
    ULONGLONG ullOffset;
    ULONGLONG ullSize;
    ULONG hf;
    ULONG ulBytesPerSector;
    UCHAR cUnitNo;
};

typedef struct _PTE
{
    char boot_ind;
    char starting_head;
    unsigned short starting_sector:6;
    unsigned short starting_cyl:10;
    char system_id;
    char ending_head;
    unsigned short ending_sector:6;
    unsigned short ending_cyl:10;
    unsigned long relative_sector;
    unsigned long total_sectors;
} PTE;

typedef struct
{
    struct unit far *u;
    PIORB_EXECUTEIO cpIO;
    USHORT cmd;
    APIRET rc;
} HookData;

#pragma pack()

typedef struct _DDD_Parm_List {
    USHORT cache_parm_list;      /* addr of InitCache_Parameter List  */
    USHORT disk_config_table;    /* addr of disk_configuration table  */
    USHORT init_req_vec_table;   /* addr of IRQ_Vector_Table          */
    USHORT cmd_line_args;        /* addr of Command Line Args         */
    USHORT machine_config_table; /* addr of Machine Config Info       */
} DDD_PARM_LIST, far *PDDD_PARM_LIST;

int strcmp(const char _far *s, const char _far *t);
char _far *strstr(const char _far *s1, const char _far *s2);
void _far *memcpy(void _far *in_dst, void _far *in_src, int len);
char _far *strcpy(char _far *s, const char _far *t );
void _far *memset(void _far *dst, char c, int len);
char _far *strlwr(char _far *s);
int atoi(const char _far *p);
void _far strat(void);
void _far _cdecl _loadds iohandler(PIORB pIORB);
void _cdecl _loadds strategy(RPH _far *reqpkt);
void init(PRPINITIN reqpkt);
void ioctl(RP_GENIOCTL far *reqpkt);
void _far _cdecl notify_hook(void);
void _far _cdecl io_hook(void);
void _stdcall put_IORB(IORBH far *);
int PreSetup(void);
int PostSetup(void);
PID queryCurrentPid(void);
LIN virtToLin(void far *p);
APIRET daemonStarted(PEXBUF pexbuf);
APIRET daemonStopped(void);
int Mount(MNTOPTS far *opts);
APIRET _cdecl IoHook(HookData far *data);
APIRET _cdecl IoFunc(struct unit far *u, PIORB_EXECUTEIO cpIO, ULONG rba, ULONG len,
                  LIN cpdata_linaddr, LIN sg_linaddr, USHORT cmd);
APIRET doio(struct unit *u, PIORB_EXECUTEIO cpIO, USHORT cmd);
APIRET dorw(struct unit far *u, PIORB_EXECUTEIO cpIO, ULONG len, USHORT cmd, ULONG rba,
            LIN sg_linaddr, LIN buf_linaddr, LIN cpdata_linaddr);
void _cdecl memlcpy(LIN lDst, LIN lSrc, ULONG numBytes);
void far *malloc(ULONG size);
void free(void far *p);
APIRET SemClear(long far *sem);
APIRET SemSet(long far *sem);
APIRET SemWait(long far *sem);

extern USHORT data_end;
extern USHORT code_end;
extern USHORT FlatDS;
extern USHORT DS;
extern volatile ULONG pIORBHead;

// !!! should be the 1st variable in the data segment !!!
struct SysDev3 DevHeader = {
    {
        -1,
        DEV_CHAR_DEV | DEVLEV_3 | DEV_30,
        (USHORT)strat,                     // strategy entry point
        0,                                 // IDC entry point
        "LOOP$   ",                        // device name
        0,                                 // prot CS
        0,                                 // prot DS
        0,
        0
    },
    DEV_ADAPTER_DD | DEV_16MB | DEV_INITCOMPLETE | DEV_IOCTL2
};
// !!!

void (_far _cdecl *LogPrint)(char _far *fmt,...) = 0;

#define log_printf(str, ...)  if (LogPrint) (*LogPrint)("loop: " str, __VA_ARGS__)

long semIoSerialize = 0;
long semSerialize = 0;
long semRqAvail = 0;
long semRqDone = 0;
ULONG Device_Help = 0; // DevHelp entry point
PID pidDaemon = 0;
CPDATA far *pCPData = NULL;
BYTE lock[12] = {0};
USHORT devhandle = 0;  // ADD handle
ULONG open_refcnt = 0; // open reference count
USHORT driver_handle = 0;
UCHAR init_complete = 0;
ULONG io_hook_handle = 0; // Ctx Hook handle
ULONG hook_handle = 0; // Ctx Hook handle
LIN ppTransferBuf;

struct unit units[8] = {0};

// !!! these two structures must be consecutive !!!
ADAPTERINFO ainfo = {
   "loop device", 0, 8, AI_DEVBUS_OTHER | AI_DEVBUS_32BIT,
   AI_IOACCESS_MEMORY_MAP, AI_HOSTBUS_OTHER | AI_BUSWIDTH_64BIT, 0, 0, AF_16M,
   16,
   0,
   // unit info
   { 0, 0, UF_NOSCSI_SUPT | UF_REMOVABLE, 0, (USHORT)&units[0], 0, UIB_TYPE_DISK, 16, 0, 0 }
};

UNITINFO u2[7] = {
   { 0, 0, UF_NOSCSI_SUPT | UF_REMOVABLE, 0, (USHORT)&units[1], 0, UIB_TYPE_DISK, 16, 0, 0 },
   { 0, 0, UF_NOSCSI_SUPT | UF_REMOVABLE, 0, (USHORT)&units[2], 0, UIB_TYPE_DISK, 16, 0, 0 },
   { 0, 0, UF_NOSCSI_SUPT | UF_REMOVABLE, 0, (USHORT)&units[3], 0, UIB_TYPE_DISK, 16, 0, 0 },
   { 0, 0, UF_NOSCSI_SUPT | UF_REMOVABLE, 0, (USHORT)&units[4], 0, UIB_TYPE_DISK, 16, 0, 0 },
   { 0, 0, UF_NOSCSI_SUPT | UF_REMOVABLE, 0, (USHORT)&units[5], 0, UIB_TYPE_DISK, 16, 0, 0 },
   { 0, 0, UF_NOSCSI_SUPT | UF_REMOVABLE, 0, (USHORT)&units[6], 0, UIB_TYPE_DISK, 16, 0, 0 },
   { 0, 0, UF_NOSCSI_SUPT | UF_REMOVABLE, 0, (USHORT)&units[7], 0, UIB_TYPE_DISK, 16, 0, 0 }
};
// !!!

void IORB_NotifyCall(IORBH far *pIORB);
#pragma aux IORB_NotifyCall = \
   "push    es"                  \
   "push    di"                  \
   "call    far ptr es:[di+1Ch]" \
   "add     sp,4"                \
   parm [es di] \
   modify exact [ax bx cx dx];

APIRET SemClear(long far *sem)
{
    USHORT usValue;
    APIRET rc = 0;

    (*sem)--;

#ifdef DEBUG
    log_printf("%lx\n", *sem);
#endif

    rc = DevHelp_ProcRun((ULONG)sem, &usValue);

    return rc;
}

APIRET SemSet(long far *sem)
{
    USHORT usValue;
    APIRET rc = 0;

    (*sem)++;

#ifdef DEBUG
    log_printf("%lx\n", *sem);
#endif

    return rc;
}

APIRET SemWait(long far *sem)
{
    APIRET rc = 0;

#ifdef DEBUG
    log_printf("%lx\n", *sem);
#endif

    if (*sem >= 0)
    {
        rc = DevHelp_ProcBlock((ULONG)sem, -1, 0);
    }

    return rc;
}

void far *malloc(ULONG size)
{
    PVOID sysresvd;
    APIRET rc;
    LIN lin;
    SEL sel;

    rc = DevHelp_VMAlloc(VMDHA_FIXED,
                         size,
                         -1,
                         &lin,
                         &sysresvd);

    if (rc)
        return NULL;

    rc = DevHelp_AllocGDTSelector(&sel, 1);

    if (rc)
    {
        DevHelp_VMFree(lin);
        return NULL;
    }

    rc = DevHelp_LinToGDTSelector(sel,
                                  lin,
                                  size);

    if (rc)
    {
        DevHelp_FreeGDTSelector(sel);
        DevHelp_VMFree(lin);
    }

    return MAKEP(sel, 0);
}

void free(void far *p)
{
    APIRET rc;
    LIN lin;

    rc = DevHelp_VirtToLin(SELECTOROF(p),
                           OFFSETOF(p),
                           &lin);

    if (rc)
    {
        return;
    }

    rc = DevHelp_FreeGDTSelector(SELECTOROF(p));

    if (rc)
    {
        return;
    }

    DevHelp_VMFree(lin);
}

void _cdecl _loadds strategy(RPH _far *reqpkt)
{
    switch (reqpkt->Cmd)
    {
        case CMDInitBase:
            init((PRPINITIN)reqpkt);
            SemClear(&semRqAvail);
            SemClear(&semRqDone);
            break;

        case CMDOpen:
            if (! open_refcnt && ! driver_handle)
            {
                SemSet(&semRqAvail);
                SemSet(&semRqDone);
                driver_handle = ((PRP_OPENCLOSE)reqpkt)->sfn;
            }
            open_refcnt++;
            reqpkt->Status = STATUS_DONE;
            break;

        case CMDClose:
            open_refcnt--;
            if (driver_handle && driver_handle == ((PRP_OPENCLOSE)reqpkt)->sfn)
            {
                SemClear(&semRqAvail);
                SemClear(&semRqDone);
                if (pidDaemon)
                {
                    daemonStopped();
                }
                driver_handle = 0;
            }
            reqpkt->Status = STATUS_DONE;
            break;

        case CMDShutdown:
            reqpkt->Status = STATUS_DONE;
            break;

        case CMDInitComplete:
            init_complete = 1;
            reqpkt->Status = STATUS_DONE;
            break;

        case CMDGenIOCTL:
            ioctl((RP_GENIOCTL far *)reqpkt);
            break;

        default:
            reqpkt->Status = STATUS_DONE | STATUS_ERR_UNKCMD;
    }
}

void init(PRPINITIN reqpkt)
{
    ULONG far *doshlp = MAKEP (0x100, 0);
    PDDD_PARM_LIST pADDParms;
    PAGELIST pagelist[17];
    ULONG count;
    PVOID sysresvd;
    LIN lin;
    USHORT err = 0;
    PSZ pCmdLine; 
    char far *p;
    int nUnits = 3;

    // check for OS/4 loader signature at DOSHLP:0 seg
    if (*doshlp == 0x342F534F)
    {
        doshlp++;

        // check for OS/4 loader info struct size
        if ((*doshlp & 0xFFFF) >= 0x30)
        {
            USHORT far *ptr = (USHORT FAR *) doshlp;
            SELECTOROF (LogPrint) = 0x100;
            OFFSETOF (LogPrint) = ptr[0x17];
        }
    }

    log_printf("%s\n", (char far *)"loop.add started");

    pADDParms = (PDDD_PARM_LIST)reqpkt->InitArgs;
    pCmdLine = MAKEP(SELECTOROF(pADDParms), (USHORT)pADDParms->cmd_line_args);
    strlwr(pCmdLine);

    if ( p = strstr(pCmdLine, (char far *)"/units:") )
    {
        p += 7;
        nUnits = atoi(p);
    }

    if (! nUnits)
    {
        nUnits = 3;
    }

    if (nUnits > 8)
    {
        nUnits = 8;
    }

    ainfo.AdapterUnits = nUnits;

    // DevHelp entry point
    Device_Help = (ULONG)reqpkt->DevHlpEP;

    log_printf("pDevHelp = %x:%x\n", SELECTOROF(Device_Help), OFFSETOF(Device_Help));

    // register as an ADD
    DevHelp_RegisterDeviceClass(ainfo.AdapterName, (PFN)iohandler, 0, 1, &devhandle);

    DevHelp_AllocateCtxHook((NPFN)notify_hook, &hook_handle);
    DevHelp_AllocateCtxHook((NPFN)io_hook, &io_hook_handle);

    ((PRPINITOUT)reqpkt)->rph.Status = STATUS_DONE;
    ((PRPINITOUT)reqpkt)->CodeEnd = code_end;
    ((PRPINITOUT)reqpkt)->DataEnd = data_end;
}

void ioctl(RP_GENIOCTL far *reqpkt)
{
    APIRET rc;
    USHORT usCount;

    void far *pParm = reqpkt->ParmPacket;
    void far *pData = reqpkt->DataPacket;

    USHORT cbParm = reqpkt->ParmLen;
    USHORT cbData = reqpkt->DataLen;

#ifdef DEBUG
    log_printf("ioctl: cat=%x, func=%x\n", reqpkt->Category, reqpkt->Function);
#endif

    if (reqpkt->Category != CAT_LOOP  ||
        reqpkt->Function < FUNC_FIRST ||
        reqpkt->Function > FUNC_LAST)
    {
        goto ioctl_exit;
    }

    switch (reqpkt->Function)
    {
        case FUNC_MOUNT:
#ifdef DEBUG
            log_printf("ioctl: %s\n", (char far *)"mount");
#endif
            if (! pParm)
            {
                rc = ERROR_I24_INVALID_PARAMETER;
                break;
            }

            rc = Mount((MNTOPTS far *)pParm);

            if (rc)
            {
                rc = ERROR_I24_INVALID_PARAMETER; // ???
                break;
            }
            break;

        case FUNC_DAEMON_STARTED:
#ifdef DEBUG
           log_printf("ioctl: %s\n", (char far *)"daemon_started");
#endif
           if (pidDaemon)
           {
               rc = ERROR_I24_BAD_COMMAND;
               //rc = ERROR_ALREADY_EXISTS;
               goto ioctl_exit;
           }

           if (cbParm < sizeof(EXBUF))
           {
               rc = ERROR_I24_INVALID_PARAMETER;
               goto ioctl_exit;
           }

           rc = daemonStarted((PEXBUF)pParm);
           break;

        case FUNC_DAEMON_STOPPED:
#ifdef DEBUG
            log_printf("ioctl: %s\n", (char far *)"daemon_stopped");
#endif
            if (! pidDaemon)
            {
                rc = NO_ERROR;
                goto ioctl_exit;
            }

            rc = daemonStopped();
            break;

        case FUNC_DAEMON_DETACH:
#ifdef DEBUG
            log_printf("ioctl: %s\n", (char far *)"daemon_detach");
#endif
            rc = daemonStopped();
            break;

        case FUNC_DONE_REQ:
#ifdef DEBUG
            log_printf("ioctl: %s\n", (char far *)"done_req");
#endif
            if (! pidDaemon)
            {
                rc = ERROR_I24_INVALID_PARAMETER;
                //return ERROR_INVALID_PROCID;
                goto ioctl_exit;
            }

            if (queryCurrentPid() != pidDaemon)
            {
                rc = ERROR_I24_INVALID_PARAMETER;
                //rc = ERROR_ALREADY_ASSIGNED;
                goto ioctl_exit;
            }

            rc = SemClear(&semRqDone);
            // fall through
            //break;

        case FUNC_GET_REQ:
#ifdef DEBUG
            log_printf("ioctl: %s\n", (char far *)"get_req");
#endif
            if (! pidDaemon)
            {
                rc = ERROR_I24_INVALID_PARAMETER;
                //return ERROR_INVALID_PROCID;
                goto ioctl_exit;
            }

            if (queryCurrentPid() != pidDaemon)
            {
                rc = ERROR_I24_INVALID_PARAMETER;
                //rc = ERROR_ALREADY_ASSIGNED;
                goto ioctl_exit;
            }

            DevHelp_SemClear((ULONG)&semSerialize);

            rc = SemWait(&semRqAvail);

            if (rc == WAIT_INTERRUPTED)
            {
                daemonStopped();
                rc = ERROR_I24_CHAR_CALL_INTERRUPTED;
                break;
            }

            if (rc)
                break;
         
            rc = SemSet(&semRqAvail);
            break;

        default:
            rc = ERROR_I24_INVALID_PARAMETER;
    }

ioctl_exit:
    if (rc)
        reqpkt->rph.Status = STDON | STERR | rc;
    else
        reqpkt->rph.Status = STDON;
}

int PreSetup(void)
{
USHORT rc;

   if (! pidDaemon)
      return ERROR_INVALID_PROCID;

   rc = DevHelp_SemRequest((ULONG)&semSerialize, -1);

   if (rc)
      return rc;

   rc = SemWait(&semRqDone);

   if (rc)
      {
      DevHelp_SemClear((ULONG)&semSerialize);
      return rc;
      }

   return NO_ERROR;
}

int PostSetup(void)
{
USHORT rc;
USHORT usCount;

   if (! pidDaemon)
      return ERROR_INVALID_PROCID;

   rc = SemSet(&semRqDone);

   if (rc)
      {
      DevHelp_SemClear((ULONG)&semSerialize);
      return rc;
      }

   SemClear(&semRqAvail);

   rc = SemWait(&semRqDone);

   if (rc)
      return rc;

   return NO_ERROR;
}

PID queryCurrentPid(void)
{
   APIRET rc;
   PID    pid;
   PLINFOSEG _far *ppLIS;

   rc = DevHelp_GetDOSVar(DHGETDOSV_LOCINFOSEG, 0, (void _far * _far *)&ppLIS);

   if (rc)
      if (LogPrint) LogPrint("Cannot query PID!\n");

   pid = (*ppLIS)->pidCurrent;

   return pid;
}


LIN virtToLin(void far *p)
{
   APIRET rc;
   LIN lin;

   rc = DevHelp_VirtToLin(
      SELECTOROF(p),
      OFFSETOF(p),
      &lin);

   if (rc)
      if (LogPrint) LogPrint("virtToLin failed!\n");

   return lin;
}

APIRET daemonStarted(PEXBUF pexbuf)
{
APIRET rc;
PAGELIST pagelist[17];
ULONG pages;
LIN linaddr;
PAGELIST far *pPagelist = (PAGELIST far *)&pagelist;
BYTE far *pLock = (BYTE far *)&lock;

   rc = DevHelp_AllocGDTSelector((PSEL)&SELECTOROF(pCPData), 1);

   if (rc)
      return rc;

   rc = DevHelp_VMProcessToGlobal(VMDHPG_WRITE,
                                  pexbuf->buf, sizeof(CPDATA),
                                  &linaddr);

   if (rc)
      return rc;

   rc = DevHelp_VMLock(VMDHL_WRITE | VMDHL_LONG,
                       linaddr, sizeof(CPDATA),
                       virtToLin(pPagelist),
                       virtToLin(pLock),
                       &pages);

   if (rc)
      return rc;

   rc = DevHelp_LinToGDTSelector(SELECTOROF(pCPData),
                                 linaddr, sizeof(CPDATA));

   if (rc)
      return rc;

   pidDaemon = queryCurrentPid();

   rc = SemClear(&semRqDone); ////

   if (rc)
      return rc;
   
   rc = DevHelp_SemClear((ULONG)&semSerialize);

   return rc;
}

APIRET daemonStopped(void)
{
APIRET rc;
BYTE far *pLock = (BYTE far *)&lock;

   pidDaemon = 0;

   SemClear(&semRqDone);

   SemSet(&semRqDone); ////
   SemSet(&semRqDone); ////

   rc = DevHelp_SemRequest((ULONG)&semSerialize, -1);

   if (rc)
      return rc;

   rc = DevHelp_SemClear((ULONG)&semSerialize);

   DevHelp_VMUnLock(virtToLin(pLock));
   DevHelp_FreeGDTSelector(SELECTOROF(pCPData));

   if (rc)
      return rc;

   return NO_ERROR;
}

int Mount(MNTOPTS far *opts)
{
APIRET rc = 0;
ULONG hf;
int iUnitNo;
struct unit *u;

    if (! pidDaemon)
    {
        return ERROR_INVALID_PROCID;
    }

    switch (opts->usOp)
    {
        case MOUNT_MOUNT:
            DevHelp_SemClear((ULONG)&semSerialize);

            if (opts->hf)
            {
                /* already opened */
                hf = opts->hf;
                rc = NO_ERROR;
            }
            else
            {
                /* open file */
                rc = PreSetup();

                if (rc)
                    return rc;

                pCPData->Op = OP_OPEN;
                strcpy(pCPData->Buf, opts->pFilename);
                memcpy(pCPData->pFmt, opts->pFmt, 8);

                rc = PostSetup();

                if (rc)
                    return rc;

                hf = pCPData->hf;
                rc = pCPData->rc;

                if (rc)
                    return rc;
            }

            // find first free slot
            for (iUnitNo = 0; iUnitNo < 8; iUnitNo++)
            {
                u = &units[iUnitNo];

                if (! u->bMounted)
                    break;
            }

            if (iUnitNo == 8)
            {
                rc = 1; // @todo better error
                break;
            }

            strcpy(u->szName, opts->pFilename);
            u->ullOffset = opts->ullOffset;
            u->ullSize = opts->ullSize;
            u->hf = hf;
            u->ulBytesPerSector = opts->ulBytesPerSector;
            u->cUnitNo = iUnitNo;
            u->bAllocated = 0;
            u->lock_state = 0;
            
            u->bMounted = 1;
            break;

        case MOUNT_RELEASE:
            for (iUnitNo = 0; iUnitNo < 8; iUnitNo++)
            {
                u = &units[iUnitNo];

                strlwr(u->szName);
                strlwr(opts->pMntPoint);

                if (! strcmp(u->szName, opts->pMntPoint) && u->hf)
                    break;
            }

            if ((iUnitNo == 8) || (iUnitNo == 0 && ! u->bMounted))
            {
                rc = 1; // @todo better error
                break;
            }

            if (! opts->hf)
            {
                /* close file */
                rc = PreSetup();

                if (rc)
                    return rc;

                pCPData->Op = OP_CLOSE;
                pCPData->hf = u->hf;

                rc = PostSetup();
            }

            memset(u, 0, sizeof(struct unit));
            break;

        default:
            break;
    }

    return rc;
}

void _far _cdecl _loadds iohandler(PIORB pIORB)
{
    struct unit *u = (struct unit *)pIORB->UnitHandle;
    PIORB   next;
    USHORT  ccode = pIORB->CommandCode;
    USHORT  scode = pIORB->CommandModifier;
    USHORT  rcont = pIORB->RequestControl;
    USHORT  error = 0;
    APIRET  rc;

#ifdef DEBUG
    log_printf("iorb: cc=%d, cm=%d\n", pIORB->CommandCode, pIORB->CommandModifier);
#endif

    do
    {
        if (ccode != IOCC_CONFIGURATION    &&
            (void *)pIORB->UnitHandle != units     &&
            (void *)pIORB->UnitHandle != units + 1 &&
            (void *)pIORB->UnitHandle != units + 2 &&
            (void *)pIORB->UnitHandle != units + 3 &&
            (void *)pIORB->UnitHandle != units + 4 &&
            (void *)pIORB->UnitHandle != units + 5 &&
            (void *)pIORB->UnitHandle != units + 6 &&
            (void *)pIORB->UnitHandle != units + 7)
        {
            error = IOERR_CMD_SYNTAX;
        }
        else
        {
            switch (ccode)
            {
                case IOCC_CONFIGURATION:
                    {
                        PIORB_CONFIGURATION cpIO = (PIORB_CONFIGURATION)pIORB;
                        PDEVICETABLE          dt = cpIO->pDeviceTable;
                        PADAPTERINFO        aptr = (PADAPTERINFO)(dt+1);

                        if (scode != IOCM_COMPLETE_INIT && scode != IOCM_GET_DEVICE_TABLE)
                        {
                            error = IOERR_CMD_NOT_SUPPORTED;
                            break;
                        }

                        if (scode == IOCM_GET_DEVICE_TABLE)
                        {
                            dt->ADDLevelMajor = ADD_LEVEL_MAJOR;
                            dt->ADDLevelMinor = ADD_LEVEL_MINOR;
                            dt->ADDHandle     = devhandle;
                            dt->TotalAdapters = 1;
                            dt->pAdapter[0]   = (ADAPTERINFO near *)aptr;
                            // copy adapter info
                            memcpy(aptr, &ainfo, sizeof(ADAPTERINFO) + 7 * sizeof(UNITINFO));
                        }
                    }
                    break;

                case IOCC_UNIT_CONTROL:
                    {
                        switch (scode)
                        {
                            case IOCM_ALLOCATE_UNIT:
                                if (! u->bAllocated)
                                    u->bAllocated = 1;
                                else
                                    error = IOERR_UNIT_ALLOCATED;
                                break;

                            case IOCM_DEALLOCATE_UNIT:
                                if (u->bAllocated)
                                    u->bAllocated = 0;
                                else
                                    error = IOERR_UNIT_NOT_ALLOCATED;
                                break;

                            case IOCM_CHANGE_UNITINFO:
                                {
                                    PIORB_UNIT_CONTROL cpUI = (PIORB_UNIT_CONTROL)pIORB;

                                    if (cpUI->UnitInfoLen == sizeof(UNITINFO))
                                        memcpy(&ainfo.UnitInfo[u->cUnitNo], cpUI->pUnitInfo, sizeof(UNITINFO));
                                    else
                                        error = IOERR_CMD_SYNTAX;
                                }
                                break;

                            default:
                                error = IOERR_CMD_NOT_SUPPORTED;
                        }
                    }
                    break;

                case IOCC_GEOMETRY:
                    {
                        PIORB_GEOMETRY cpDG = (PIORB_GEOMETRY)pIORB;
                        PGEOMETRY      pgeo = cpDG->pGeometry;

                        if (scode != IOCM_GET_MEDIA_GEOMETRY && scode != IOCM_GET_DEVICE_GEOMETRY)
                        {
                            error = IOERR_CMD_NOT_SUPPORTED;
                            break;
                        }

                        pgeo->Reserved        = 0;

                        if (u->bMounted)
                        {
                            pgeo->NumHeads        = 255;
                            pgeo->SectorsPerTrack = 63;
                            pgeo->BytesPerSector  = u->ulBytesPerSector;
                            pgeo->TotalSectors    = u->ullSize / u->ulBytesPerSector + 63;
                            pgeo->TotalCylinders  = (pgeo->TotalSectors + pgeo->NumHeads * pgeo->SectorsPerTrack - 1) 
                                / (pgeo->NumHeads * pgeo->SectorsPerTrack);
                        }
                        else
                        {
                            pgeo->BytesPerSector = 512;
                            error = IOERR_UNIT_NOT_READY;
                        }
                    }
                    break;

                case IOCC_UNIT_STATUS:
                    {
                        PIORB_UNIT_STATUS cpUS = (PIORB_UNIT_STATUS)pIORB;

                        switch (scode)
                        {
                            case IOCM_GET_UNIT_STATUS:
                                if (u->bMounted)
                                    cpUS->UnitStatus = US_READY;
                                else
                                    cpUS->UnitStatus = US_POWER;
                                break;

                            case IOCM_GET_LOCK_STATUS:
                                cpUS->UnitStatus = u->lock_state;
                                break;

                            default:
                                error = IOERR_CMD_NOT_SUPPORTED;
                        }
                    }
                    break;

                case IOCC_DEVICE_CONTROL  :
                    switch (scode)
                    {
                        case IOCM_LOCK_MEDIA:
                            u->lock_state |= US_LOCKED;
                            break;

                        case IOCM_UNLOCK_MEDIA:
                            u->lock_state &= ~US_LOCKED;
                            break;

                        default:
                            error = (scode != IOCM_EJECT_MEDIA) ? 
                                IOERR_CMD_NOT_SUPPORTED :
                                IOERR_DEVICE_REQ_NOT_SUPPORTED;
                    }
                    break;

                case IOCC_ADAPTER_PASSTHRU:
                    {
                        if (! u->bMounted || ! pidDaemon)
                        {
                            rc = IOERR_UNIT_NOT_READY;
                            break;
                        }

                        switch (scode)
                        {
                            case IOCM_EXECUTE_CDB:
                                {
                                    PIORB_ADAPTER_PASSTHRU cpPT = (PIORB_ADAPTER_PASSTHRU)pIORB;

                                    // handle 'eject' command
                                    if (cpPT->pControllerCmd[0] == 0x1b)
                                    {
                                        // SCSI Start stop unit command
                                        MNTOPTS opts;
                                        APIRET rc;

                                        if (u->bMounted)
                                        {
                                            strcpy(opts.pMntPoint, u->szName);
                                            opts.hf = 0;
                                            opts.usOp = MOUNT_RELEASE;

                                            rc = Mount(&opts);

                                            if (rc)
                                            {
                                                error = IOERR_CMD_ABORTED;
                                            }
                                        }
                                        error = IOERR_MEDIA_CHANGED;
                                        break;
                                    }
                                }
                                error = IOERR_CMD_NOT_SUPPORTED;
                                break;

                            default:
                                error = IOERR_CMD_NOT_SUPPORTED;
                        }
                    }
                    break;

                case IOCC_EXECUTE_IO:
                    {
                        PIORB_EXECUTEIO cpIO = (PIORB_EXECUTEIO)pIORB;

                        if (scode && scode != IOCM_READ_PREFETCH &&
                            (scode < IOCM_READ || scode > IOCM_WRITE_VERIFY))
                        {
                            error = IOERR_CMD_NOT_SUPPORTED;
                            break;
                        }

                        if (rcont & IORB_CHS_ADDRESSING)
                        {
                            error = IOERR_ADAPTER_REQ_NOT_SUPPORTED;
                            break;
                        }

                        if (cpIO->BlockSize != 512)
                        {
                            error = IOERR_ADAPTER_REQ_NOT_SUPPORTED;
                            break;
                        }

                        cpIO->BlocksXferred = 0;

                        if (!cpIO->cSGList || !cpIO->ppSGList || !cpIO->BlockCount)
                        {
                            if (cpIO->cSGList)
                            {
                                error = IOERR_CMD_SGLIST_BAD;
                            }
                        }

                        rc = doio(u, cpIO, scode);

                        if (rc)
                            error = rc;
                        else
                            cpIO->BlocksXferred = cpIO->BlockCount;

                        if (error)
                        {
                            PSCATGATENTRY pt = cpIO->pSGList;
                            int i;

                            log_printf("iohandler %lx %d -> rc %d\n", cpIO->RBA, cpIO->BlockCount, error);

                            for (i = 0; i < cpIO->cSGList; i++)
                            {
                                log_printf("sge: %d. %lx %lx\n", i, pt[i].ppXferBuf, pt[i].XferBufLen);
                            }
                        }
                    }
                    break;

                default:
                    error  = IOERR_CMD_NOT_SUPPORTED;
            }
        }
        
        pIORB->ErrorCode = error;
        pIORB->Status    = ((error != IOERR_MEDIA_CHANGED ? error : 0) ? IORB_ERROR : 0) | IORB_DONE;

        next = rcont & IORB_CHAIN ? pIORB->pNxtIORB : 0;
        
        // call to completion callback?
        if (rcont & IORB_ASYNC_POST)
        {
           /* a short story (from _dixie_, copied from hd4disk.add):
            * if we set MaxHWSGList in adapter info to >=16 - IORB_NotifyCall
              became recursive and reach stack limit!
              It designed to be async-called from interrupt, but we call it
              on caller stack. DASD just FROM IORB_NotifyCall call us again
              and again - until out of stack.

              UNITINFO.QueuingCount = 16 (max value) - can help on FAT only,
              HPFS reboots on any activity.

            * if we set MaxHWSGList to 1 - driver forced to use Strat1 calls.
              This is FINE and FAST and STABLE, but this stupid FAT32.IFS does
              not check for zero in pvpfsi->vpi_pDCS in FS_MOUNT - and goes to
              trap D. And JFS unable to operate with disk too.

            * if we Arm context hook on every call - we became blocked on
              init (DASD calls), because hook called in task-time only.

            * if we allow at least 2-3 recursive calls here - JFS traps system
              on huge load.

              So, Call it directly until the end of basedev loading, then use
              Arm hook. This is the only method, which makes happy all: FAT,
              FAT32, HPFS, HPFS386 and JFS.

              BUT - if any basedev will try to call us directly from own
              IOCM_COMPLETE_INIT - we will stop system here, I think ;)
            */
            if (ccode != IOCC_EXECUTE_IO)
            {
                IORB_NotifyCall(pIORB);
            }
        }
        // chained request?
        pIORB = next;
    } while (pIORB);
}

ULONG crc_table[256];

/*
  Name  : CRC-32
  Poly  : 0x04C11DB7    x^32 + x^26 + x^23 + x^22 + x^16 + x^12 + x^11
                       + x^10 + x^8 + x^7 + x^5 + x^4 + x^2 + x + 1
  Init  : 0xFFFFFFFF
  Revert: true
  XorOut: 0xFFFFFFFF
  Check : 0xCBF43926 ("123456789")
  MaxLen: 268 435 455 байт (2 147 483 647 бит) - обнаружение
   одинарных, двойных, пакетных и всех нечетных ошибок

  (Got from russian wikipedia, http://ru.wikipedia.org/wiki/CRC32)
  and corrected ;)
*/
ULONG crc32(UCHAR far *buf, ULONG len)
{
    ULONG crc;
    int i, j;

    for (i = 0; i < 256; i++)
    {
        crc = i;
        for (j = 0; j < 8; j++)
            crc = crc & 1 ? (crc >> 1) ^ 0xEDB88320UL : crc >> 1;

        crc_table[i] = crc;
    };

    crc = 0xFFFFFFFFUL;

    while (len--)
      crc = crc_table[(crc ^ *buf++) & 0xff] ^ ((crc >> 8) & 0x00ffffffL);

    return crc;
}

char buf[4096];

DLA_Table_Sector far *dlat;
DLA_Entry far *dlae;

APIRET _cdecl IoFunc(struct unit far *u, PIORB_EXECUTEIO cpIO, 
                     ULONG rba, ULONG len,
                     LIN lCp, LIN lSg, USHORT cmd)
{
    APIRET rc;
    USHORT rcont;
    PIORBH next;

    rc = DevHelp_SemRequest((ULONG)&semIoSerialize, -1);

    do
    {
        if (cmd >= IOCM_WRITE)
        {
            rc = PreSetup();

            if (rc)
                break;

            pCPData->Op = OP_WRITE;
            pCPData->hf = u->hf;
            pCPData->llOffset = (LONGLONG)u->ullOffset + rba * u->ulBytesPerSector;
            pCPData->cbData = len;
            memlcpy(lCp, lSg, len);

            rc = PostSetup();

            if (rc)
                break;

            rc = pCPData->rc;
        }
        else
        {
            rc = PreSetup();

            if (rc)
                break;

            pCPData->Op = OP_READ;
            pCPData->hf = u->hf;
            pCPData->llOffset = (LONGLONG)u->ullOffset + rba * u->ulBytesPerSector;
            pCPData->cbData = len;

            rc = PostSetup();

            if (rc)
                break;

            if (cmd == IOCM_READ)
            {
                memlcpy(lSg, lCp, len);
            }
            rc = pCPData->rc;
        }
    } while (0);

    rcont = cpIO->iorbh.RequestControl;
    next = rcont & IORB_CHAIN ? cpIO->iorbh.pNxtIORB : 0;

    //cpIO->iorbh.ErrorCode = rc ? IOERR_CMD_ABORTED : 0;
    //cpIO->iorbh.Status    = (rc ? IORB_ERROR : 0) | IORB_DONE;

    if (rcont & IORB_ASYNC_POST)
    {
        if (init_complete)
        {
            put_IORB((PIORBH)cpIO);
        }
        else
        {
            IORB_NotifyCall((PIORBH)cpIO);
        }
    }

    rc = DevHelp_SemClear((ULONG)&semIoSerialize);
    
    return rc;
}


APIRET dorw(struct unit far *u, PIORB_EXECUTEIO cpIO, ULONG len, USHORT cmd, ULONG rba,
            LIN sg_linaddr, LIN buf_linaddr, LIN cpdata_linaddr)
{
    APIRET rc = 0;
    USHORT rcont;
    //ULONG  rba = cpIO->RBA;
    char far *pBuf = (char far *)&buf;

    if (rba < 63)
    {
        PTE far *pte;
        ULONG Size = u->ullSize / u->ulBytesPerSector + 63;
        USHORT Cyl, Head, Sec, Cyl0;
        ULONG CRC32;

        if (cmd >= IOCM_WRITE)
        {
            // deny write to the emulated sectors
            //rc = IOERR_MEDIA_WRITE_PROTECT;
            rc = 0;
            goto rw_end;
        }

        memset(buf, 0, sizeof(buf));

        Cyl = (Size + 255 * 63 - 1) / (255 * 63);
        //Cyl = Size / (255 * 63);
        Head = (Size % (255 * 63)) / 63;
        Sec = Size % 63 + 1;

        Cyl0 = Cyl;

        if (Cyl > 1023)
        {
            Cyl = 1023;
            Head = 254;
            Sec = 63;
        }

        switch (rba)
        {
            case 0:
                // partition table
                pte = (PTE far *)(pBuf + 0x1be);

                pte->boot_ind = 0x0;
                pte->starting_cyl = 0;
                pte->starting_head = 1;
                pte->starting_sector = 1;
                pte->system_id = 0x7;
                pte->ending_cyl = Cyl;
                pte->ending_head = Head;
                pte->ending_sector = Sec;
                pte->relative_sector = 63;
                pte->total_sectors = Size;

                pBuf[u->ulBytesPerSector - 2] = 0x55;
                pBuf[u->ulBytesPerSector - 1] = 0xaa;
                break;

            case 62:
                // DLAT
                dlat = (DLA_Table_Sector far *)pBuf;

                dlat->DLA_Signature1 = DLA_TABLE_SIGNATURE1;
                dlat->DLA_Signature2 = DLA_TABLE_SIGNATURE2;

                dlat->Disk_Serial_Number = 0x4a93f05eUL;
                dlat->Boot_Disk_Serial_Number = 0x4a93f05eUL;

                dlat->Cylinders = Cyl0;
                dlat->Heads_Per_Cylinder = 255;
                dlat->Sectors_Per_Track = 63;

                memcpy(&dlat->Disk_Name, (char far *)"Loop Device ", 12);
                dlat->Disk_Name[12] = u->cUnitNo + '0';
                dlat->Disk_Name[13] = '\0';

                dlae = (DLA_Entry far *)&dlat->DLA_Array;

                dlae->Volume_Serial_Number = 0xfb8ed8f6UL;
                dlae->Partition_Serial_Number = 0x824be8efUL;

                dlae->Partition_Size = Size;
                dlae->Partition_Start = 63;

                memcpy(&dlae->Partition_Name, (char far *)"Loop Partition ", 15);
                dlae->Partition_Name[15] = u->cUnitNo + '0';
                dlae->Partition_Name[16] = '\0';

                memcpy(&dlae->Volume_Name, (char far *)"Loop Volume ", 12);
                dlae->Volume_Name[12] = u->cUnitNo + '0';
                dlae->Volume_Name[13] = '\0';

                dlae->Drive_Letter = '*';

                dlat->DLA_CRC = 0;
                CRC32 = crc32(pBuf, 0x200);
                dlat->DLA_CRC = CRC32;
                break;

            default:
                // zero sectors
                break;
        }

        if (cmd == IOCM_READ)
        {
            memlcpy(sg_linaddr, buf_linaddr, u->ulBytesPerSector);
        }

        rcont = cpIO->iorbh.RequestControl;

        cpIO->iorbh.ErrorCode = rc;
        cpIO->iorbh.Status    = (rc ? IORB_ERROR : 0) | IORB_DONE;

        if (rcont & IORB_ASYNC_POST)
        {
            IORB_NotifyCall((PIORBH)cpIO);
        }

        return rc;
    }
    else
    {
        rba -= 63;

        cpIO->iorbh.ErrorCode = rc;
        cpIO->iorbh.Status    = (rc ? IORB_ERROR : 0) | IORB_DONE;

        rc = IoFunc(u, cpIO, rba, len, cpdata_linaddr, sg_linaddr, cmd);

        return rc;
    }

rw_end:
    cpIO->iorbh.ErrorCode = rc;
    cpIO->iorbh.Status    = (rc ? IORB_ERROR : 0) | IORB_DONE;

    rcont = cpIO->iorbh.RequestControl;

    if (rcont & IORB_ASYNC_POST)
    {
        IORB_NotifyCall((PIORBH)cpIO);
    }

    return rc;
}

APIRET _cdecl IoHook(HookData far *data)
{
    PSCATGATENTRY SGList = data->cpIO->pSGList;
    USHORT cSGList = data->cpIO->cSGList;
    ULONG rba = data->cpIO->RBA;
    ULONG read = 0;
    USHORT count = data->cpIO->BlockCount;
    char far *p = pCPData->Buf;
    PSCATGATENTRY sge;
    USHORT rcont = data->cpIO->iorbh.RequestControl;
    APIRET rc;
    int i;
    ULONG crc;

    struct
    {
        ULONG PhysAddr;
        ULONG Size;
    } pagelist;

    void far *pl = &pagelist;
    char far *pBuf = (char far *)&buf;

    LIN linaddr, cpdata_linaddr, sg_linaddr, buf_linaddr;

    if (! data->u->bMounted || ! pidDaemon)
    {
        rc = IOERR_UNIT_NOT_READY;
        goto io_end;
    }

    rc = DevHelp_VirtToLin(SELECTOROF(pl),
                           OFFSETOF(pl),
                           &linaddr);

    if (rc)
    {
        rc = IOERR_CMD_ABORTED;
        goto io_end;
    }

    rc = DevHelp_VirtToLin(SELECTOROF(p),
                           OFFSETOF(p),
                           &cpdata_linaddr);

    if (rc)
    {
        rc = IOERR_CMD_ABORTED;
        goto io_end;
    }

    rc = DevHelp_VirtToLin(SELECTOROF(pBuf),
                           OFFSETOF(pBuf),
                           &buf_linaddr);

    if (rc)
    {
        rc = IOERR_CMD_ABORTED;
        goto io_end;
    }

    switch (data->cmd)
    {
        case IOCM_READ:
        case IOCM_WRITE:
            read = 0;
            for (i = 0; i < cSGList; i++)
            {
                sge = &SGList[i];

                pagelist.PhysAddr = sge->ppXferBuf;
                pagelist.Size = sge->XferBufLen;

                rc = DevHelp_PageListToLin(sge->XferBufLen,
                                           linaddr,
                                           &sg_linaddr);

                if (rc)
                {
                    rc = IOERR_CMD_ABORTED;
                    break;
                }

                rc = dorw(data->u, data->cpIO, sge->XferBufLen, data->cmd, rba + read,
                          sg_linaddr, buf_linaddr, cpdata_linaddr);
                read += sge->XferBufLen / data->cpIO->BlockSize;
            }
            break;

        case IOCM_READ_VERIFY:
            rc = dorw(data->u, data->cpIO, data->cpIO->BlockCount * data->cpIO->BlockSize, data->cmd, rba,
                      sg_linaddr, buf_linaddr, cpdata_linaddr);
            break;

        case IOCM_WRITE_VERIFY:
            crc = crc32(pCPData->Buf, sizeof(pCPData->Buf));
            rc = dorw(data->u, data->cpIO, data->cpIO->BlockCount * data->cpIO->BlockSize, IOCM_WRITE, rba,
                      sg_linaddr, buf_linaddr, cpdata_linaddr);

            if (rc)
                break;

            rc = dorw(data->u, data->cpIO, data->cpIO->BlockCount * data->cpIO->BlockSize, IOCM_READ_VERIFY, rba,
                      sg_linaddr, buf_linaddr, cpdata_linaddr);

            if (rc)
                break;

            if (crc32(pCPData->Buf, sizeof(pCPData->Buf)) != crc)
            {
                rc = IOERR_RBA_CRC_ERROR;
                break;
            }
            break;

        default:
            rc = IOERR_CMD_NOT_SUPPORTED;
            goto io_end;

    }

    if (pIORBHead)
    {
        notify_hook();
    }
    
    free(data);
    return rc;    

io_end:
    data->cpIO->iorbh.ErrorCode = rc;
    data->cpIO->iorbh.Status    = (rc ? IORB_ERROR : 0) | IORB_DONE;

    if (rcont & IORB_ASYNC_POST)
    {
        IORB_NotifyCall((PIORBH)data->cpIO);
    }
    
    free(data);
    return rc;    
}

APIRET doio(struct unit *u, PIORB_EXECUTEIO cpIO, USHORT cmd)
{
    HookData far *hookdata;
    APIRET rc = 0;

    hookdata = (HookData far *)malloc(sizeof(HookData));

    if (! hookdata)
    {
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    hookdata->u = MAKEP(DS, u);
    hookdata->cpIO = cpIO;
    hookdata->cmd = cmd;

    if (init_complete)
        DevHelp_ArmCtxHook((ULONG)hookdata, io_hook_handle);
    else
        IoHook(hookdata);

    return rc;
}
