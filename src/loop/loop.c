#define INCL_LONGLONG
#define INCL_DOSERRORS
#define INCL_DOSINFOSEG
#define INCL_NOBASEAPI
#define INCL_NOPMAPI
#include <os2.h>

#include <lvm_info.h>

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
  long semSerialize;
  long semRqAvail;
  long semRqDone;
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

#pragma pack()

typedef struct _DDD_Parm_List {
    USHORT cache_parm_list;      /* addr of InitCache_Parameter List  */
    USHORT disk_config_table;    /* addr of disk_configuration table  */
    USHORT init_req_vec_table;   /* addr of IRQ_Vector_Table          */
    USHORT cmd_line_args;        /* addr of Command Line Args         */
    USHORT machine_config_table; /* addr of Machine Config Info       */
} DDD_PARM_LIST, far *PDDD_PARM_LIST;

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
void _cdecl notify_hook(void);
void _cdecl put_IORB(IORBH far *);
int PreSetup(void);
int PostSetup(void);
PID queryCurrentPid(void);
LIN virtToLin(void far *p);
APIRET daemonStarted(PEXBUF pexbuf);
APIRET daemonStopped(void);
int Mount(MNTOPTS far *opts);
APIRET doio(struct unit *u, PSCATGATENTRY SGList, USHORT cSGList, ULONG sector, USHORT count, UCHAR write);
APIRET SemClear(long far *sem);
APIRET SemSet(long far *sem);
APIRET SemWait(long far *sem);

extern USHORT data_end;
extern USHORT code_end;

extern volatile ULONG pIORBHead;

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

void (_far _cdecl *LogPrint)(char _far *fmt,...) = 0;

#define log_printf(str, ...)  if (LogPrint) (*LogPrint)("loop: " str, __VA_ARGS__)

// DevHelp entry point
ULONG Device_Help = 0;
USHORT pidDaemon = 0;
CPDATA far *pCPData = NULL;
LIN lock = 0;
USHORT devhandle = 0;  // ADD handle
ULONG hook_handle = 0; // Ctx Hook handle

struct unit units[8] = {0};

int bLastUnit = -1;

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
        rc = DevHelp_ProcBlock((ULONG)sem, -1, 1);
    }

    return rc;
}

void _cdecl _loadds strategy(RPH _far *reqpkt)
{
    switch (reqpkt->Cmd)
    {
        case CMDInitBase:
            init((PRPINITIN)reqpkt);
            break;

        case CMDShutdown:
        case CMDInitComplete:
        case CMDOpen:
        case CMDClose:
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
    PSZ pCmdLine; 
    char far *p;
    int nUnits = 1;

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

    LogPrint("loop.add started\n");

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
        nUnits = 1;
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
            }

            rc = Mount((MNTOPTS far *)pParm);

            if (rc)
            {
                rc = ERROR_I24_INVALID_PARAMETER; // ???
            }
            break;

        case FUNC_DAEMON_STARTED:
#ifdef DEBUG
            log_printf("ioctl: %s\n", (char far *)"daemon_started");
#endif
           //if (pidDaemon)
           //{
           //    rc = ERROR_I24_BAD_COMMAND;
           //    //rc = ERROR_ALREADY_EXISTS;
           //    goto ioctl_exit;
           //}

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

            DevHelp_SemClear((ULONG)&pCPData->semSerialize);

            rc = SemWait(&pCPData->semRqAvail);

            if (rc)
                break;
         
            rc = SemSet(&pCPData->semRqAvail);
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

            rc = SemClear(&pCPData->semRqDone);
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

   rc = DevHelp_SemRequest((ULONG)&pCPData->semSerialize, -1);

   if (rc)
      return rc;

   rc = SemWait(&pCPData->semRqDone);

   if (rc)
      {
      DevHelp_SemClear((ULONG)&pCPData->semSerialize);
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

   rc = SemSet(&pCPData->semRqDone);

   if (rc)
      {
      DevHelp_SemClear((ULONG)&pCPData->semSerialize);
      return rc;
      }

   SemClear(&pCPData->semRqAvail);

   rc = SemWait(&pCPData->semRqDone);

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
                       virtToLin(pagelist),
                       virtToLin(&lock),
                       &pages);

   if (rc)
      return rc;

   rc = DevHelp_LinToGDTSelector(SELECTOROF(pCPData),
                                 linaddr, sizeof(CPDATA));

   if (rc)
      return rc;

   pidDaemon = queryCurrentPid();

   rc = SemSet(&pCPData->semRqAvail);

   if (rc)
      return rc;

   rc = SemClear(&pCPData->semRqDone); ////
   
   rc = DevHelp_SemClear((ULONG)&pCPData->semSerialize);

   return rc;
}

APIRET daemonStopped(void)
{
APIRET rc;

   pidDaemon = 0;

   SemClear(&pCPData->semRqDone);

   rc = DevHelp_SemRequest((ULONG)&pCPData->semSerialize, -1);

   if (rc)
      return rc;

   rc = DevHelp_SemClear((ULONG)&pCPData->semSerialize);

   DevHelp_VMUnLock(virtToLin(&lock));
   DevHelp_FreeGDTSelector(SELECTOROF(pCPData));

   if (rc)
      return rc;

   return NO_ERROR;
}

int Mount(MNTOPTS far *opts)
{
APIRET rc = 0;
ULONG hf;
struct unit _far *u;

    switch (opts->usOp)
    {
        case MOUNT_MOUNT:
            DevHelp_SemClear((ULONG)&pCPData->semSerialize);

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

            if (bLastUnit == -1)
                bLastUnit = 0;
            else
                bLastUnit++;

            if (bLastUnit > 8)
            {
                rc = 1; // @todo better error
                break;
            }

            u = &units[bLastUnit];

            strcpy(u->szName, opts->pFilename);
            u->ullOffset = opts->ullOffset;
            u->ullSize = opts->ullSize;
            u->hf = hf;
            u->cUnitNo = bLastUnit;

            u->bMounted = 1;
            break;

        case MOUNT_RELEASE:
            if (bLastUnit == -1)
            {
                rc = 1;
                break;
            }

            u = &units[bLastUnit];

            if (! opts->hf)
            {
                /* close file */
                rc = PreSetup();

                if (rc)
                    return rc;

                pCPData->Op = OP_CLOSE;
                pCPData->hf = u->hf;

                rc = PostSetup();

                if (rc)
                    return rc;
            }

            memset(u, 0, sizeof(struct unit));

            if (bLastUnit > 0)
                bLastUnit--;
            else
                bLastUnit = -1;

            u->bMounted = 0;
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
        next = rcont & IORB_CHAIN ? pIORB->pNxtIORB : 0;

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
                                        memcpy(u, cpUI->pUnitInfo, sizeof(UNITINFO));
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

                        pgeo->BytesPerSector  = 512;
                        pgeo->Reserved        = 0;

                        if (u->ullSize)
                        {
                            pgeo->TotalSectors    = u->ullSize / 512;
                            pgeo->NumHeads        = 255;
                            pgeo->SectorsPerTrack = 63;
                            pgeo->TotalCylinders  = pgeo->TotalSectors / (pgeo->NumHeads * pgeo->SectorsPerTrack);
                        }
                        else
                        {
                            // 96 MB disk geometry (as it's done in usbmsd.add)
                            pgeo->TotalSectors    = 0x30000;
                            pgeo->NumHeads        = 12;
                            pgeo->SectorsPerTrack = 32;
                            pgeo->TotalCylinders  = 512;
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
                        switch (scode)
                        {
                            case IOCM_EXECUTE_CDB:
                                {
                                    PIORB_ADAPTER_PASSTHRU cpPT = (PIORB_ADAPTER_PASSTHRU)pIORB;

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
                                error = IOERR_CMD_SGLIST_BAD;
                        }

                        rc = doio(u, cpIO->pSGList, cpIO->cSGList, cpIO->RBA,
                                  cpIO->BlockCount, scode > IOCM_READ_PREFETCH);

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
        pIORB->Status    = (error ? IORB_ERROR : 0) | IORB_DONE;

        // call to completion callback?
        if (rcont & IORB_ASYNC_POST)
        {
           /* a short story:
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
            if (ccode == IOCC_EXECUTE_IO && next)
                put_IORB(pIORB);
            else
                IORB_NotifyCall(pIORB);
        }
        // chained request?
        pIORB = next;
    } while (pIORB);

    if (pIORBHead)
    {
        DevHelp_ArmCtxHook(0, hook_handle);
    }
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
  MaxLen: 268 435 455 ���� (2 147 483 647 ���) - �����㦥���
   ��������, �������, ������� � ��� ������ �訡��

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

DLA_Table_Sector dlat;
DLA_Entry far *dlae;

APIRET doio(struct unit *u, PSCATGATENTRY SGList, USHORT cSGList, ULONG rba, USHORT count, UCHAR write)
{
    APIRET rc;
    PSCATGATENTRY sge;
    char far *p = pCPData->Buf;
    SEL sel;
    int i;

    if (! u->bMounted)
    {
        return IOERR_UNIT_NOT_READY;
    }

    rc = DevHelp_AllocGDTSelector(&sel, 1);

    if (rc)
    {
        return IOERR_CMD_ABORTED;
    }

    for (i = 0; i < cSGList; i++)
    {
        USHORT flag = 0;
        char far *pBuf;

        sge = &SGList[i];

        rc = DevHelp_PhysToGDTSelector(sge->ppXferBuf, sge->XferBufLen, sel);

        if (rc)
        {
            rc = IOERR_CMD_ABORTED;
            break;
        }

        pBuf = (char far *)MAKEP(sel, 0);

        if (rba < 63)
        {
            PTE far *pte;
            ULONG Size = u->ullSize / 512;
            USHORT Cyl, Head, Sec, Cyl0;
            ULONG CRC32;

            if (write)
            {
                // deny write to the emulated sectors
                rc = IOERR_MEDIA_WRITE_PROTECT;
                break;
            }

            memset(pBuf, 0, sge->XferBufLen);

            Cyl = (Size + 255 * 63 - 1) / (255 * 63);
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

                    pBuf[510] = 0x55;
                    pBuf[511] = 0xaa;
                    break;

                case 62:
                    // DLAT
                    dlat.DLA_Signature1 = DLA_TABLE_SIGNATURE1;
                    dlat.DLA_Signature2 = DLA_TABLE_SIGNATURE2;

                    dlat.Disk_Serial_Number = 0x4a93f05eUL;
                    dlat.Boot_Disk_Serial_Number = 0x4a93f05eUL;

                    dlat.Cylinders = Cyl0;
                    dlat.Heads_Per_Cylinder = 255;
                    dlat.Sectors_Per_Track = 63;

                    memcpy(&dlat.Disk_Name, (char far *)"Loop Device ", 12);
                    dlat.Disk_Name[12] = u->cUnitNo + '0';
                    dlat.Disk_Name[13] = '\0';

                    dlae = (DLA_Entry far *)&dlat.DLA_Array;

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

                    dlat.DLA_CRC = 0;
                    memcpy(pBuf, (char far *)&dlat, sizeof(DLA_Table_Sector));
                    CRC32 = crc32(pBuf, 0x200);
                    ((DLA_Table_Sector far *)pBuf)->DLA_CRC = CRC32;
                    break;

                default:
                    // zero sectors
                    break;
            }

            return 0;
        }
        else
        {
            rba -= 63;
        }

        if (write)
        {
            rc = PreSetup();

            if (rc)
                break;

            pCPData->Op = OP_WRITE;
            pCPData->hf = u->hf;
            pCPData->llOffset = (LONGLONG)u->ullOffset + rba * 512;
            pCPData->cbData = count * 512;
            memcpy(p, pBuf, sge->XferBufLen);
            p += sge->XferBufLen;

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
            pCPData->llOffset = (LONGLONG)u->ullOffset + rba * 512;
            pCPData->cbData = count * 512;

            rc = PostSetup();

            if (rc)
                break;

            memcpy(pBuf, p, sge->XferBufLen);
            p += sge->XferBufLen;
            rc = pCPData->rc;
        }
    }

    DevHelp_FreeGDTSelector(sel);

    return rc;
}
