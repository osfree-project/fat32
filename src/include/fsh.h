 #ifndef __FSH_H__
 #define __FSH_H__

 int far pascal FSH_ADDSHARE(
    char far *pName,
    unsigned short usMode,
    unsigned short hVPB,
    unsigned long far *ulShare
 );

 int far pascal FSH_CALLDRIVER(
    void far *pPkt,
    unsigned short hVPB,
    unsigned short fControl
 );

 int far pascal FSH_CANONICALIZE(
    char far *pszPathName,
    unsigned short cbPathBuf,
    char far *pPathBuf,
    char far *pFlags
 );

 int far pascal FSH_CHECKEANAME(
    unsigned short level,
    unsigned long cbEAName,
    char far *pEAName
 );

 #define CE_ALLFAIL      0x0001
 #define CE_ALLABORT     0x0002
 #define CE_ALLRETRY     0x0004
 #define CE_ALLIGNORE    0x0008
 #define CE_ALLACK       0x0010

 #define CE_RETIGNORE    0x0000
 #define CE_RETRETRY     0x0001
 #define CE_RETFAIL      0x0003
 #define CE_RETACK       0x0004

 int far pascal FSH_CRITERROR(
    int cbMessage,
    char far *pMessage,
    int nSubs,
    char far *pSubs,
    unsigned short fAllowed
 );

 int far pascal FSH_DEVIOCTL(
    unsigned short FSDRaisedFlag,
    unsigned long hDev,
    unsigned short SFN,
    unsigned short usCat,
    unsigned short usFunc,
    char far *pParm,
    unsigned short cbParm,
    char far *pData,
    unsigned short cbData
 );

 #define DVIO_OPREAD       0x0000
 #define DVIO_OPWRITE      0x0001
 #define DVIO_OPBYPASS     0x0002
 #define DVIO_OPVERIFY     0x0004
 #define DVIO_OPHARDERR    0x0008
 #define DVIO_OPWRTHRU     0x0010
 #define DVIO_OPNCACHE     0x0020
 #define DVIO_OPRESMEM     0x0040

 #define DVIO_ALLFAIL      0x0001
 #define DVIO_ALLABORT     0x0002
 #define DVIO_ALLRETRY     0x0004
 #define DVIO_ALLIGNORE    0x0008
 #define DVIO_ALLACK       0x0010

 int far pascal FSH_DOVOLIO(
    unsigned short usOper,
    unsigned short fAllowed,
    unsigned short hVPB,
    char far *pData,
    unsigned short far *pcSec,
    unsigned long iSec
 );

 int far pascal FSH_DOVOLIO2(
    unsigned long hDev,
    unsigned short SFN,
    unsigned short usCat,
    unsigned short usFunc,
    char far *pParm,
    unsigned short cbParm,
    char far *pData,
    unsigned short cbData
 );

 int far pascal FSH_FINDCHAR(
    unsigned short nChars,
    char far *pChars,
    char far * far *ppStr
 );

 int far pascal FSH_FINDDUPHVPB(
    unsigned short hVPB,
    unsigned short far *pHVPB
 );

 #define FB_DISCNONE       0x0000
 #define FB_DISCCLEAN      0x0001

 int far pascal FSH_FLUSHBUF(
    unsigned short hVPB,
    unsigned short fDiscard
 );

 int far pascal FSH_FORCENOSWAP(
    unsigned short sel
 );

 int far pascal FSH_EXTENDTIMESLICE(void);

 int far pascal FSH_GETPRIORITY(void);

 int far pascal FSH_IOBOOST(void);

 #define GB_PRNOREAD       0x0001

 int far pascal FSH_GETOVERLAPBUF(
    unsigned short hVPB,
    unsigned long iSec1,
    unsigned long iSec2,
    unsigned long far *pisecBuf,
    char far * far *ppBuf
 );

 int far pascal FSH_GETVOLPARM(
    unsigned short hVPB,
    struct vpfsi far * far *ppVPBfsi,
    struct vpfsd far * far *ppVPBfsd
 );

 int far pascal FSH_INTERR(
    char far *pMsg,
    unsigned short cbMsg
 );

 int far pascal FSH_ISCURDIRPREFIX(
    char far *pName
 );

 void far pascal FSH_LOADCHAR(
    char far * far *ppStr,
    unsigned short far *pChar
 );

 void far pascal FSH_PREVCHAR(
    char far *pBeg,
    char far * far *ppStr
 );

 #define PB_OPREAD     0x0000
 #define PB_OPWRITE    0x0001

 int far pascal FSH_PROBEBUF(
    unsigned short usOper,
    char far *pData,
    unsigned short cbData
 );

 int far pascal FSH_QUERYSERVERTHREAD(void);

 int far pascal FSH_QUERYOPLOCK(void);

 #define QSI_SECSIZE       1
 #define QSI_PROCID        2
 #define QSI_THREADNO      3
 #define QSI_VERIFY        4

 int far pascal FSH_QSYSINFO(
    unsigned short usIndex,
    char far *pData,
    unsigned short cbData
 );

 int far pascal FSH_NAMEFROMSFN(
    unsigned short SFN,
    char far *pName,
    unsigned short far *pcbName
 );

 #define RPC_16BIT     0x0000
 #define RPC_32BIT     0x0001

 int far pascal FSH_REGISTERPERFCTRS(
    char far *pDataBlk,
    char far *pTextBlk,
    unsigned short fsFlags
 );

 int far pascal FSH_REMOVESHARE(
    unsigned long hShare
 );

 #define SA_FLDT           0x0001
 #define SA_FSWAP          0x0002

 #define SA_FRINGMASK      0x6000
 #define SA_FRING0         0x0000
 #define SA_FRING1         0x2000
 #define SA_FRING2         0x4000
 #define SA_FRING3         0x6000

 int far pascal FSH_SEGALLOC(
    unsigned short flags,
    unsigned long cbSeg,
    unsigned short far *pSel
 );

 int far pascal FSH_SEGFREE(
    unsigned short Sel
 );

 int far pascal FSH_SEGREALLOC(
    unsigned short Sel,
    unsigned long cbSeg
 );

 #define TO_INFINITE       0xFFFFFFFFL
 #define TO_NOWAIT         0x00000000L

 int far pascal FSH_SEMCLEAR(
    void far *pSem
 );

 int far pascal FSH_IOSEMCLEAR(
    void far *pSem
 );

 int far pascal FSH_SEMREQUEST(
    void far *pSem,
    unsigned long cTimeout
 );

 int far pascal FSH_SEMSET(
    void far *pSem
 );

 int far pascal FSH_SEMSETWAIT(
    void far *pSem,
    unsigned long cTimeout
 );

 int far pascal FSH_SEMWAIT(
    void far *pSem,
    unsigned long cTimeout
 );

 int far pascal FSH_SETVOLUME(
    unsigned short hVPB,
    unsigned long fControl
 );

 int far pascal FSH_STORECHAR(
    unsigned short chDBCS,
    char far * far *ppStr
 );

 int far pascal FSH_UPPERCASE(
    char far *pName,
    unsigned short cbPathBuf,
    char far *pPathBuf
 );

 int far pascal FSH_WILDMATCH(
    char far *pPat,
    char far *pStr
 );

 int far pascal FSH_YIELD(void);

 int far pascal MFSH_CALLRM(
    unsigned long far *pProc
 );

 int far pascal MFSH_DOVOLIO(
    char far *pData,
    unsigned short far *cSec,
    unsigned long iSec
 );

 int far pascal MFSH_INTERR(
    char far *pMsg,
    unsigned short cbMsg
 );

 int far pascal MFSH_LOCK(
    unsigned short Sel,
    unsigned long far *handle
 );

 int far pascal MFSH_PHYSTOVIRT(
    unsigned long Addr,
    unsigned short cbLen,
    unsigned short far *pSel
 );

 int far pascal MFSH_SEGALLOC(
    unsigned short flag,
    unsigned long cbSeg,
    unsigned short far *Sel
 );

 int far pascal MFSH_SEGFREE(
    unsigned short Sel
 );

 int far pascal MFSH_SEGREALLOC(
    unsigned short Sel,
    unsigned long cbSeg
 );

 int far pascal MFSH_SETBOOTDRIVE(
    unsigned short usDrv
 );

 int far pascal MFSH_UNLOCK(
    unsigned long handle
 );

 int far pascal MFSH_UNPHYSTOVIRT(
    unsigned short Sel
 );

 int far pascal MFSH_VIRT2PHYS(
    unsigned long VirtAddr,
    unsigned long far *pPhysAddr
 );

 #endif /* __FSH_H__ */
