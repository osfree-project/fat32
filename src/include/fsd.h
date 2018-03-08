 #ifndef __FSD_H_
 #define __FSD_H_

 #define FSA_REMOTE         0x00000001	/* remote FSD                   */
 #define FSA_UNC	    0x00000002	/* FSD supports UNC pathnames   */
 #define FSA_LOCK           0x00000004	/* FSD needs lock notifications */
 #define FSA_LVL7           0x00000008	/* FSD supports level 7         */
 #define FSA_PSVR           0x00000010  /* FSD supports remote named pipes */
 #define FSA_LARGEFILE      0x00000020  /* FSD supports files > 2 GB    */

 #define CDDWORKAREASIZE    8
 #define SFDWORKAREASIZE    30
 #define VPDWORKAREASIZE    36

 #define VPBTEXTLEN         12

 struct	vpfsi
 {
    unsigned long     vpi_vid;                      /* volume serial number */
    unsigned long     vpi_hDEV; 		    /* device driver handle */
    unsigned short    vpi_bsize;		    /* sector size          */
    unsigned long     vpi_totsec;		    /* total sectors        */
    unsigned short    vpi_trksec;		    /* sectors per track    */
    unsigned short    vpi_nhead;		    /* heads                */
    char	      vpi_text[VPBTEXTLEN];         /* volume label         */
    void far *	      vpi_pDCS;		            /* device capabilities structure    */
    void far *	      vpi_pVCS;		            /* volume characteristics structure */
    unsigned char     vpi_drive;		    /* drive letter         */
    unsigned char     vpi_unit;		            /* unit number          */
 };

 struct	vpfsd
 {
    char    vpd_work[VPDWORKAREASIZE];              /* work area            */
 };

 #define CDI_ISVALID      0x80	                    /* structure is valid   */
 #define CDI_ISROOT       0x40		            /* it is a root dir     */
 #define CDI_ISCURRENT	  0x20

 struct cdfsi
 {
    unsigned short    cdi_hVPB;		            /* VPB handle           */
    unsigned short    cdi_end;		            /* curdir end           */
    char	      cdi_flags;                    /* flags                */
    char	      cdi_curdir[CCHMAXPATH];       /* current directory    */
 };

 struct	cdfsd
 {
    char cdd_work[CDDWORKAREASIZE];	            /* work area            */
 };

 // sfi_tstamps
 #define ST_SCREAT      1                           /* stamp ctime          */
 #define ST_PCREAT      2                           /* propagate ctime      */
 #define ST_SWRITE      4                           /* stamp mtime          */
 #define ST_PWRITE      8                           /* propagate mtime      */
 #define ST_SREAD      16                           /* stamp atime          */
 #define ST_PREAD      32                           /* propagate atime      */

 // sfi_type
 #define STYPE_FILE	0	                    /* file                 */
 #define STYPE_DEVICE	1	                    /* device               */
 #define STYPE_NMPIPE	2                           /* named pipe           */
 #define STYPE_FCB	4                           /* fcb                  */

 struct	sffsi
 {
    unsigned long   sfi_mode;                       /* access and sharing mode  */
    unsigned short  sfi_hVPB;	                    /* VPB handle               */
    unsigned short  sfi_ctime;                      /* creation time            */
    unsigned short  sfi_cdate;                      /* creation date            */
    unsigned short  sfi_atime;                      /* access time              */
    unsigned short  sfi_adate;                      /* access date              */
    unsigned short  sfi_mtime;                      /* modification time        */
    unsigned short  sfi_mdate;                      /* modification date        */
    unsigned long   sfi_size;                       /* file size                */
    unsigned long   sfi_position;                   /* file pointer             */
    unsigned short  sfi_UID;	                    /* user ID                  */
    unsigned short  sfi_PID;                        /* PID                      */
    unsigned short  sfi_PDB;                        /* VDM PDB                  */
    unsigned short  sfi_selfsfn;                    /* SFN                      */
    unsigned char   sfi_tstamp;	                    /* time stamp flags         */
    unsigned short  sfi_type;	                    /* file type                */
    unsigned long   sfi_pPVDBFil;
    unsigned char   sfi_DOSattr;   /* DOS attr (hidden/system/readonly/archive) */
    LONGLONG        sfi_sizel;     /* 64-bit file size                          */
    LONGLONG        sfi_positionl; /* 64-bit file pointer                       */
 };

 struct	sffsd
 {
    char sfd_work[SFDWORKAREASIZE];               /* work area                  */
 };

 struct fsfsi
 {
    unsigned short fsi_hVPB;                      /* VPB handle                 */
 };

 #define FSFSD_WORK_SIZE    24

 struct fsfsd {
    char fsd_work[FSFSD_WORK_SIZE];               /* work area                  */
 };

 struct devfsd {
    unsigned long FSDRef;                         /* FSD ref. got from ATTACH   */
 };

 #define IOFL_WRITETHRU	    0x10
 #define IOFL_NOCACHE	    0x20

 int far pascal _loadds FS_ALLOCATEPAGESPACE(
    struct sffsi far *psffsi,
    struct sffsd far *psffsd,
    unsigned long     lSize,
    unsigned long     lWantContig
 );

 #define FSA_ATTACH         0x00
 #define FSA_DETACH         0x01
 #define FSA_ATTACH_INFO    0x02

 int far pascal _loadds FS_ATTACH(
    unsigned short usFlag,
    char far *pDev,
    void far *pvpfsd,
    void far *pdevfsd,
    char far *pParm,
    unsigned short far *pLen
 );

 int far pascal _loadds FS_CANCELLOCKREQUEST(
    struct sffsi far *psffsi,
    struct sffsd far *psffsd,
    void far *pLockRang
 );

 #define CD_EXPLICIT        0x00
 #define CD_VERIFY          0x01
 #define CD_FREE            0x02

 int far pascal _loadds FS_CHDIR(
    unsigned short flag,
    struct cdfsi far *pcdfsi,
    struct cdfsd far *pcdfsd,
    char far *pszDir,
    unsigned short iCurDirEnd
 );

 int far pascal _loadds FS_CHGFILEPTRL(
    struct sffsi far *psffsi,
    struct sffsd far *psffsd,
    LONGLONG llOffset,
    unsigned short usType,
    unsigned short IOflag
 );

 #define CFP_RELBEGIN        0x00
 #define CFP_RELCUR          0x01
 #define CFP_RELEND          0x02

 int far pascal _loadds FS_CHGFILEPTR(
    struct sffsi far *psffsi,
    struct sffsd far *psffsd,
    long lOffset,
    unsigned short usType,
    unsigned short IOflag
 );

 #define FS_CL_ORDINARY	     0
 #define FS_CL_FORPROC	     1
 #define FS_CL_FORSYS	     2

 int far pascal _loadds FS_CLOSE(
    unsigned short usType,
    unsigned short IOflag,
    struct sffsi far *psffsi,
    struct sffsd far *psffsd
 );

 #define FS_COMMIT_ONE       1
 #define FS_COMMIT_ALL       2

 int far pascal _loadds FS_COMMIT(
    unsigned short usType,
    unsigned short IOflag,
    struct sffsi far *psffsi,
    struct sffsd far *psffsd
 );

 int far pascal _loadds FS_COPY(
    unsigned short usMode,
    struct cdfsi far *pcdfsi,
    struct cdfsd far *pcdfsd,
    char far *pszSrcName,
    unsigned short iSrcCurrDirEnd,
    char far *pszDstName,
    unsigned short iDstCurrDirEnd,
    unsigned short flags
 );

 int far pascal _loadds FS_DELETE(
    struct cdfsi far *pcdfsi,
    struct cdfsd far *pcdfsd,
    char far *pFile,
    unsigned short iCurDirEnd
 );

 #define PGIO_FI_ORDER   0x01       /* force ops order      */

 #define PGIO_FO_DONE    0x01       /* op done              */
 #define PGIO_FO_ERROR   0x02       /* op returned an error */

 #define PGIO_ATTEMPTED  0x0f       /* op attempted         */
 #define PGIO_FAILED     0xf0       /* op failed            */

 struct PageCmd
 {
    unsigned char Cmd;   	    /* command code Read/Write/WriteVerify     */
    unsigned char Priority;	    /* the same codes like for request packets */
    unsigned char Status;	    /* status                                  */
    unsigned char Error;	    /* error code                              */
    unsigned long Addr;	            /* phys. or virt.                          */
    unsigned long FileOffset;       /* offset from the beginning of swap file  */
 };

 struct PageCmdHeader
 {
    unsigned char InFlags;          /* input flags                             */
    unsigned char OutFlags;         /* output flags - should be 0 on entry     */
    unsigned char OpCount;          /* operations count                        */
    unsigned char Pad;              /* align on DWORD boundary                 */
    unsigned long Reserved1;
    unsigned long Reserved2;
    unsigned long Reserved3;
    struct PageCmd PageCmdList[1];  /* commands list                           */
 };

 int far pascal _loadds FS_DOPAGEIO(
    struct sffsi far *psffsi,
    struct sffsd far *psffsd,
    struct PageCmdHeader far *pPageCmdList
 );

 void far pascal _loadds FS_EXIT(
    unsigned short uid,
    unsigned short pid,
    unsigned short pdb
 );

 #define FA_RETRIEVE       0x00
 #define FA_SET            0x01

 int far pascal _loadds FS_FILEATTRIBUTE(
    unsigned short flag,
    struct cdfsi far *pcdfsi,
    struct cdfsd far *pcdfsd,
    char far *pszName,
    unsigned short iCurDirEnd,
    unsigned short far *pAttr
 );

 #define FI_RETRIEVE       0x00
 #define FI_SET            0x01

 int far pascal _loadds FS_FILEINFO(
    unsigned short flag,
    struct sffsi far *psffsi,
    struct sffsd far *psffsd,
    unsigned short level,
    char far * pszData,
    unsigned short cbData,
    unsigned short IOflag
 );

 #define FILEIO_LOCK       0
 #define FILEIO_UNLOCK     1
 #define FILEIO_SEEK       2
 #define FILEIO_READ       3
 #define FILEIO_WRITE      4

 #pragma pack(1)

 struct CmdLock
 {
    USHORT Cmd;           /* 0                  */
    USHORT LockCnt;       /* locks count        */
    ULONG  TimeOut;       /* millisecon timeout */
 };

 struct CmdUnLock
 {
    USHORT Cmd;           /* 1                  */
    USHORT UnlockCnt;     /* unlocks count      */
 };

 struct CmdSeek
 {
    USHORT Cmd;           /* 2                  */
    USHORT Method;        /* 0 for absolute             */
                          /* 1 for self-relative        */
                          /* 2 for relative to EOF      */
    LONG   Position;      /* position                   */
    LONG   Actual;        /* actual position            */
 };

 struct CmdSeekL
 {
    USHORT Cmd;           /* 2                          */
    USHORT Method;        /* 0 for absolute             */
                          /* 1 for self-relative        */
                          /* 2 for relative to EOF      */
    LONGLONG Position;    /* position                   */
    LONGLONG Actual;      /* actual position            */
 };

 struct CmdIO
 {
    USHORT Cmd;           /* 3 for read, 4 for write    */
    void _far *Buffer;    /* buffer                     */
    USHORT BufferLen;     /* buffer length              */
    USHORT Actual;        /* actual bytes transferred   */

 };

 int far pascal _loadds FS_FILEIO(
    struct sffsi far *psffsi,
    struct sffsd far *psffsd,
    char far *cbCmdList,
    unsigned short pCmdLen,
    unsigned short far *poError,
    unsigned short IOPflag
 );

 struct Lock
 {
    USHORT Share;            /* whether the lock is shared     */
    ULONG  Start;            /* start offset of lock           */
    ULONG  Length;           /* bytes to be locked             */
 };

 struct UnLock
 {
    ULONG Start;             /* start offset of lock           */
    ULONG Length;            /* bytes to be locked             */
 };

 struct LockL
 {
    USHORT Share;            /* whether the lock is shared     */
    USHORT Pad;              /* 4 bytes pad                    */
    ULONGLONG Start;         /* start offset of lock           */
    ULONGLONG Length;        /* bytes to be locked             */
 };

 struct UnLockL
 {
    ULONGLONG Start;          /* start offset of lock          */
    ULONGLONG Length;         /* bytes to be locked            */
 };

 #pragma pack()

 #define LOCK_EXPIRED        0x0L
 #define LOCK_WAKEUP         0x1L
 #define LOCK_CANCELED       0x2L

 struct filelock
 {
    long FileOffset;   /* start offset of lock/unlock          */
    long RangeLength;  /* length of locked/unlocked are        */
 };

#ifndef _MSC_VER
 struct filelockl
 {
    long long FileOffset;   /* start offset of lock/unlock     */
    long long RangeLength;  /* length of locked/unlocked area  */
 };
#endif

 int far pascal _loadds FS_FILELOCKS(
    struct sffsi far *psffsi,
    struct sffsd far *psffsd,
    void far *pUnLockRange,
    void far *pLockRange,
    unsigned long timeout,
    unsigned long flags
 );

 #define FF_NOPOS          0x00
 #define FF_GETPOS         0X01

 int far pascal _loadds FS_FINDFIRST(
    struct cdfsi far *pcdfsi,
    struct cdfsd far *pcdfsd,
    char far *pName,
    unsigned short iCurDirEnd,
    unsigned short Attr,
    struct fsfsi far *pfsfsi,
    struct fsfsd far *pfsfsd,
    char far *pData,
    unsigned short cbData,
    unsigned short far *pcMatch,
    unsigned short level,
    unsigned short flags
 );

 int far pascal _loadds FS_FINDNEXT(
    struct fsfsi far *pfsfsi,
    struct fsfsd far *pfsfsd,
    char far *pData,
    unsigned short cbData,
    unsigned short far *pcMatch,
    unsigned short level,
    unsigned short flag
 );

 int far pascal _loadds FS_FINDCLOSE(
    struct fsfsi far *pfsfsi,
    struct fsfsd far *pfsfsd
 );

 int far pascal _loadds FS_FINDFROMNAME(
    struct fsfsi far *pfsfsi,
    struct fsfsd far *pfsfsd,
    char far *pData,
    unsigned short cbData,
    unsigned short far *pcMatch,
    unsigned short level,
    unsigned long position,
    char far *pName,
    unsigned short flags
 );

 int far pascal _loadds FS_FINDNOTIFYFIRST(
    struct cdfsi far *pcdfsi,
    struct cdfsd far *pcdfsd,
    char far *pName,
    unsigned short iCurDirEnd,
    unsigned short Attr,
    unsigned short far *pHandle,
    char far *pData,
    unsigned short cbData,
    unsigned short far *pcMatch,
    unsigned short level,
    unsigned long timeout
 );

 int far pascal _loadds FS_FINDNOTIFYNEXT(
    unsigned short handle,
    char far *pData,
    unsigned short cbData,
    unsigned short far *pcMatch,
    unsigned short infolevel,
    unsigned long timeout
 );

 int far pascal _loadds FS_FINDNOTIFYCLOSE(
    unsigned short handle
 );

 #define FLUSH_RETAIN           0x00
 #define FLUSH_DISCARD          0x01

 int far pascal _loadds FS_FLUSHBUF(
    unsigned short hVPB,
    unsigned short flag
 );

 #define FSCTL_ARG_FILEINSTANCE	0x01
 #define FSCTL_ARG_CURDIR       0x02
 #define FSCTL_ARG_NULL         0x03

 #define FSCTL_FUNC_NONE        0x00
 #define FSCTL_FUNC_NEW_INFO    0x01
 #define FSCTL_FUNC_EASIZE      0x02

 struct SF
 {
    struct sffsi far *psffsi;
    struct sffsd far *psffsd;
 };

 struct CD
 {
    struct cdfsi far *pcdfsi;
    struct cdfsd far *pcdfsd;
    char far *pPath;
    unsigned short iCurDirEnd;
 };

 union argdat
 {
    struct SF sf;
    struct CD cd;
 };

 int far pascal _loadds FS_FSCTL(
    union argdat far *pArgdat,
    unsigned short iArgType,
    unsigned short func,
    char far *pParm,
    unsigned short cbParm,
    unsigned short far *cbParmOut,
    char far *pData,
    unsigned short cbData,
    unsigned short far *plenDataOut
 );

 #define INFO_RETRIEVE		0x00
 #define INFO_SET		0x01

 int far pascal _loadds FS_FSINFO(
    unsigned short flag,
    unsigned short hVPB,
    char far *pData,
    unsigned short cbData,
    unsigned short level
 );

 int far pascal _loadds FS_INIT(
    char far *szParm,
    unsigned long pDevHelp,
    unsigned long far *pMiniFSD
 );

 int far pascal _loadds FS_IOCTL(
    struct sffsi far *psffsi,
    struct sffsd far *psffsd,
    unsigned short usCat,
    unsigned short usFunc,
    char far *pParm,
    unsigned short cbParm,
    unsigned far *pParmLenInOut,
    char far *pData,
    unsigned short cbData,
    unsigned far *pDataLenInOut
 );

 int far pascal _loadds FS_MKDIR(
    struct cdfsi far *pcdfsi,
    struct cdfsd far *pcdfsd,
    char far *pName,
    unsigned short iCurDirEnd,
    char far *pEABuf,
    unsigned short flags
 );

 #define MOUNT_MOUNT           0x00
 #define MOUNT_VOL_REMOVED     0x01
 #define MOUNT_RELEASE         0x02
 #define MOUNT_ACCEPT          0x03

 int far pascal _loadds FS_MOUNT(
    unsigned short flag,
    struct vpfsi far *pvpfsi,
    struct vpfsd far *pvpfsd,
    unsigned short hVPB,
    char far *pBoot
 );

 int far pascal _loadds FS_MOVE(
    struct cdfsi far *pcdfsi,
    struct cdfsd far *pcdfsd,
    char far *pSrc,
    unsigned short iSrcCurDirEnd,
    char far *pDst,
    unsigned short iDstCurDirEnd,
    unsigned short flags
 );

 int far pascal _loadds FS_NEWSIZEL(
    struct sffsi far *psffsi,
    struct sffsd far *psffsd,
    ULONGLONG cbLen,
    unsigned short IOflag
 );

 int far pascal _loadds FS_NEWSIZE(
    struct sffsi far *psffsi,
    struct sffsd far *psffsd,
    unsigned long cbLen,
    unsigned short IOflag
 );

 #define NMP_GetPHandState   0x21
 #define NMP_SetPHandState   0x01
 #define NMP_PipeQInfo       0x22
 #define NMP_PeekPipe        0x23
 #define NMP_ConnectPipe     0x24
 #define NMP_DisconnectPipe  0x25
 #define NMP_TransactPipe    0x26
 #define NMP_READRAW         0x11
 #define NMP_WRITERAW        0x31
 #define NMP_WAITPIPE        0x53
 #define NMP_CALLPIPE        0x54
 #define NMP_QNmPipeSemState 0x58

 struct	phs_param
 {
    short phs_len;
    short phs_dlen;
    short phs_pmode;
 };

 struct	npi_param
 {
    short npi_len;
    short npi_dlen;
    short npi_level;
 };

 struct	npr_param
 {
    short npr_len;
    short npr_dlen;
    short npr_nbyt;
 };

 struct	npw_param
 {
    short npw_len;
    short npw_dlen;
    short npw_nbyt;
 };

 struct	npq_param
 {
    short npq_len;
    short npq_dlen;
    long  npq_timeo;
    short npq_prio;
 };

 struct	npx_param
 {
    short               npx_len;
    unsigned short      npx_ilen;
    char far            *npx_obuf;
    unsigned short      npx_olen;
    unsigned short      npx_nbyt;
    long                npx_timeo;
 };

 struct	npp_param
 {
    short               npp_len;
    unsigned short      npp_dlen;
    unsigned short      npp_nbyt;
    unsigned short      npp_avl0;
    unsigned short      npp_avl1;
    unsigned short      npp_state;
 };

 struct	npt_param
 {
    short               npt_len;
    unsigned short      npt_ilen;
    char far            *npt_obuf;
    unsigned short      npt_olen;
    unsigned short      npt_nbyt;
 };

 struct	qnps_param
 {
    unsigned short      qnps_len;
    unsigned short      qnps_dlen;
    long                qnps_semh;
    unsigned short      qnps_nbyt;
 };

 struct	npc_param
 {
    unsigned short      npc_len;
    unsigned short      npc_dlen;
 };

 struct	npd_param
 {
    unsigned short      npd_len;
    unsigned short      npd_dlen;
 };

 union npoper
 {
    struct phs_param    phs;
    struct npi_param    npi;
    struct npr_param    npr;
    struct npw_param    npw;
    struct npq_param    npq;
    struct npx_param    npx;
    struct npp_param    npp;
    struct npt_param    npt;
    struct qnps_param   qnps;
    struct npc_param    npc;
    struct npd_param    npd;
 };

 int far pascal _loadds FS_NMPIPE(
    struct sffsi far *psffsi,
    struct sffsd far *psffsd,
    unsigned short OpType,
    union npoper far *pOpRec,
    char far *pData,
    char far *pName
 );

 #define FOC_NEEDEAS      0x1

 int far pascal _loadds FS_OPENCREATE(
    struct cdfsi far *pcdfsi,
    void far *data,			/* for remote devices:
				     struct devfsd far *
				   for local devices:
				     struct cdfsd far * */
    char far *pName,
    unsigned short iCurDirEnd,
    struct sffsi far *psffsi,
    struct sffsd far *psffsd,
    unsigned long ulOpenMode,   /* or fHandFlag	        */
    unsigned short usOpenFlag,
    unsigned short far *pusAction,
    unsigned short usAttr,
    char far *pEABuf,
    unsigned short far *pfgenFlag
 );

 #define PGIO_FIRSTOPEN 0x00000001
 #define PGIO_PADDR     0x00004000
 #define PGIO_VADDR     0x00008000

 int far pascal _loadds FS_OPENPAGEFILE (
    unsigned long far *pFlags,
    unsigned long far *pcMaxReq,
    char far          *pName,
    struct sffsi far  *psffsi,
    struct sffsd far  *psffsd,
    unsigned short    OpenMode,
    unsigned short    OpenFlag,
    unsigned short    Attr,
    unsigned long     Reserved
 );

 #define PI_RETRIEVE    0x00
 #define PI_SET         0x01

 #define VUN_PASS1      0x0000
 #define VUN_PASS2      0x0001
 #define ERROR_UNC_PATH_NOT_FOUND 0x0003

 int far pascal _loadds FS_PATHINFO(
    unsigned short flag,
    struct cdfsi far *pcdfsi,
    struct cdfsd far *pcdfsd,
    char far *pName,
    unsigned short iCurDirEnd,
    unsigned short level,
    char far *pData,
    unsigned short cbData
 );

 int far pascal _loadds FS_PROCESSNAME(
    char far *pNameBuf
 );

 int far pascal _loadds FS_READ(
    struct sffsi far *psffsi,
    struct sffsd far *psffsd,
    char far *pData,
    unsigned short far *pcbLen,
    unsigned short IOflag
 );

 int far pascal _loadds FS_RMDIR(
    struct cdfsi far *pcdfsi,
    struct cdfsd far *pcdfsd,
    char far *pName,
    unsigned short iCurDirEnd
 );

 int far pascal _loadds FS_SETSWAP(
    struct sffsi far *psffsi,
    struct sffsd far *psffsd
 );

 #define SD_BEGIN            0x00
 #define SD_COMPLETE         0x01

 int far pascal _loadds FS_SHUTDOWN(
    unsigned short usType,
    unsigned long ulReserved
 );

 int far pascal _loadds FS_VERIFYUNCNAME(
    unsigned short flag,
    char far *pName
 );

 int far pascal _loadds FS_WRITE(
    struct sffsi far *psffsi,
    struct sffsd far *psffsd,
    char far *pData,
    unsigned short far *pcbLen,
    unsigned short IOflag
 );

 int far pascal _loadds MFS_CHGFILEPTR(
    unsigned long offset,
    unsigned short type
 );

 int far pascal _loadds MFS_CLOSE(void);

 int far pascal _loadds MFS_INIT(
    void far *pBootData,
    char far *cbIO,
    long far *pVectorRIPL,
    void far *pBPB,
    unsigned long far *pMiniFSD,
    unsigned long far *pDump
 );

 int far pascal _loadds MFS_OPEN(
    char far *pName,
    unsigned long far *pcbSize
 );

 int far pascal _loadds MFS_READ(
    char far *pData,
    unsigned short far *pcbLen
 );

 int far pascal _loadds MFS_TERM(void);

 #endif /* __FSD_H_ */
