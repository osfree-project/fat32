/*******************************************************************
*   portable.h - include file to enhance portability               *
*******************************************************************/

#ifndef PORTABLE_H
#define PORTABLE_H

#ifdef _MSC_VER

#if (_MSC_VER < 600)
#define _far      far
#define _near     near
#define _huge     huge
#define fastcall  pascal
#else
#pragma pack(1)
#define fastcall _fastcall
#endif
#define _loadds
#define _inline
#endif

#if defined(__RING3__)
#define FAR _far
#endif

#if defined(__WATCOMC__) || defined(__WATCOM)
#ifndef INCL_LONGLONG
#define INCL_LONGLONG
#endif
#endif

#if defined(__OS2__) && !defined(__16BITS__)
#define INCL_DOSPROCESS
#define INCL_DOSPROFILE
#define INCL_DOSSEMAPHORES
#define INCL_DOSDEVIOCTL
#define INCL_DOSDEVICES
#define INCL_DOSERRORS
#define INCL_DOSFILEMGR
#define INCL_DOSMODULEMGR
#define INCL_DOSDATETIME
#define INCL_DOSMISC
#define INCL_DOSNLS
#define INCL_VIO
#include <os2.h>
typedef ULONG   DWORD;
#elif defined(__16BITS__)
#ifndef INCL_LONGLONG
#pragma pack(1)
typedef struct _LONGLONG {
    ULONG ulLo;
    LONG  ulHi;
} LONGLONG, *PLONGLONG;

typedef struct _ULONGLONG {
    ULONG ulLo;
    ULONG ulHi;
} ULONGLONG, *PULONGLONG;
#pragma pack()
#else
typedef long long LONGLONG, *PLONGLONG;
typedef unsigned long long ULONGLONG, *PULONGLONG;
#endif
ULONGLONG _cdecl Div(ULONGLONG ullA, ULONGLONG ullB);
ULONGLONG _cdecl DivUL(ULONGLONG ullA, ULONG ulB);
ULONGLONG _cdecl DivUS(ULONGLONG ullA, USHORT usB);
LONGLONG  _cdecl iDiv(LONGLONG pllA, LONGLONG llB);
LONGLONG  _cdecl iDivL(LONGLONG llA, LONG lB);
LONGLONG  _cdecl iDivS(LONGLONG llA, SHORT sB);
LONGLONG  _cdecl iDivUL(LONGLONG llA, ULONG ulB);
LONGLONG  _cdecl iDivUS(LONGLONG llA, USHORT usB);
ULONGLONG _cdecl Mod(ULONGLONG ullA, ULONGLONG ullB);
ULONGLONG _cdecl ModUL(ULONGLONG ullA, ULONG ulB);
ULONGLONG _cdecl ModUS(ULONGLONG ullA, USHORT usB);
LONGLONG  _cdecl iMod(LONGLONG llA, LONGLONG llB);
LONGLONG  _cdecl iModL(LONGLONG llA, LONG lB);
LONGLONG  _cdecl iModS(LONGLONG llA, SHORT sB);
LONGLONG  _cdecl iModUL(LONGLONG llA, ULONG ulB);
LONGLONG  _cdecl iModUS(LONGLONG llA, USHORT usB);
ULONGLONG _cdecl Mul(ULONGLONG ullA, ULONGLONG ullB);
ULONGLONG _cdecl MulUL(ULONGLONG ullA, ULONG ulB);
ULONGLONG _cdecl MulUS(ULONGLONG ullA, USHORT usB);
LONGLONG  _cdecl iMul(LONGLONG llA, LONGLONG llB);
LONGLONG  _cdecl iMulL(LONGLONG llA, LONG lB);
LONGLONG  _cdecl iMulS(LONGLONG llA, SHORT sB);
LONGLONG  _cdecl iMulUL(LONGLONG llA, ULONG ulB);
LONGLONG  _cdecl iMulUS(LONGLONG llA, USHORT usB);
ULONGLONG _cdecl Add(ULONGLONG ullA, ULONGLONG ullB);
ULONGLONG _cdecl AddUL(ULONGLONG ullA, ULONG ulB);
ULONGLONG _cdecl AddUS(ULONGLONG ullA, USHORT usB);
ULONGLONG _cdecl Sub(ULONGLONG ullA, ULONGLONG ullB);
ULONGLONG _cdecl SubUL(ULONGLONG ullA, ULONG ulB);
ULONGLONG _cdecl SubUS(ULONGLONG ullA, USHORT usB);
LONGLONG  _cdecl iAdd(LONGLONG llA, LONGLONG llB);
LONGLONG  _cdecl iAddL(LONGLONG llA, LONG lB);
LONGLONG  _cdecl iAddS(LONGLONG llA, SHORT sB);
LONGLONG  _cdecl iAddUL(LONGLONG llA, ULONG ulB);
LONGLONG  _cdecl iAddUS(LONGLONG llA, USHORT usB);
LONGLONG  _cdecl iSub(LONGLONG llA, LONGLONG llB);
LONGLONG  _cdecl iSubL(LONGLONG llA, LONG lB);
LONGLONG  _cdecl iSubS(LONGLONG llA, SHORT sB);
LONGLONG  _cdecl iSubUS(LONGLONG llA, USHORT usB);
LONGLONG  _cdecl iSubUL(LONGLONG llA, ULONG ulB);
BOOL _cdecl Less(ULONGLONG ullA, ULONGLONG ullB);
BOOL _cdecl iLess(LONGLONG llA, LONGLONG llB);
BOOL _cdecl LessE(ULONGLONG ullA, ULONGLONG ullB);
BOOL _cdecl iLessE(LONGLONG llA, LONGLONG llB);
BOOL _cdecl LessUL(ULONGLONG ullA, ULONG ulB);
BOOL _cdecl LessUS(ULONGLONG ullA, USHORT usB);
BOOL _cdecl iLessL(LONGLONG llA, LONG lB);
BOOL _cdecl iLessS(LONGLONG llA, SHORT sB);
BOOL _cdecl iLessUS(LONGLONG llA, USHORT usB);
BOOL _cdecl iLessUL(LONGLONG llA, ULONG ulB);
BOOL _cdecl LessEUL(ULONGLONG ullA, ULONG ulB);
BOOL _cdecl LessEUS(ULONGLONG ullA, USHORT usB);
BOOL _cdecl iLessEL(LONGLONG llA, LONG lB);
BOOL _cdecl iLessES(LONGLONG llA, SHORT sB);
BOOL _cdecl iLessEUL(LONGLONG llA, ULONG ulB);
BOOL _cdecl iLessEUS(LONGLONG llA, USHORT usB);
BOOL _cdecl Greater(ULONGLONG ullA, ULONGLONG ullB);
BOOL _cdecl iGreater(LONGLONG llA, LONGLONG llB);
BOOL _cdecl GreaterE(ULONGLONG ullA, ULONGLONG ullB);
BOOL _cdecl iGreaterE(LONGLONG llA, LONGLONG llB);
BOOL _cdecl GreaterUL(ULONGLONG ullA, ULONG ulB);
BOOL _cdecl GreaterUS(ULONGLONG ullA, USHORT usB);
BOOL _cdecl iGreaterL(LONGLONG llA, LONG lB);
BOOL _cdecl iGreaterS(LONGLONG llA, SHORT sB);
BOOL _cdecl iGreaterUL(LONGLONG llA, ULONG ulB);
BOOL _cdecl iGreaterUS(LONGLONG llA, USHORT usB);
BOOL _cdecl GreaterEUL(ULONGLONG ullA, ULONG ulB);
BOOL _cdecl GreaterEUS(ULONGLONG ullA, USHORT usB);
BOOL _cdecl iGreaterEL(LONGLONG llA, LONG lB);
BOOL _cdecl iGreaterES(LONGLONG llA, SHORT sB);
BOOL _cdecl iGreaterEUL(LONGLONG llA, ULONG ulB);
BOOL _cdecl iGreaterEUS(LONGLONG llA, USHORT usB);
BOOL _cdecl Eq(ULONGLONG ullA, ULONGLONG ullB);
BOOL _cdecl EqUL(ULONGLONG ullA, ULONG ulB);
BOOL _cdecl EqUS(ULONGLONG ullA, USHORT usB);
BOOL _cdecl iEq(LONGLONG llA, LONGLONG llB);
BOOL _cdecl iEqL(LONGLONG llA, LONG lB);
BOOL _cdecl iEqS(LONGLONG llA, SHORT sB);
BOOL _cdecl iEqUL(LONGLONG llA, ULONG ulB);
BOOL _cdecl iEqUS(LONGLONG llA, USHORT usB);
BOOL _cdecl Neq(ULONGLONG ullA, ULONGLONG ullB);
BOOL _cdecl NeqUL(ULONGLONG ullA, ULONG ulB);
BOOL _cdecl NeqUS(ULONGLONG ullA, USHORT usB);
BOOL _cdecl iNeq(LONGLONG llA, LONGLONG llB);
BOOL _cdecl iNeqL(LONGLONG llA, LONG lB);
BOOL _cdecl iNeqS(LONGLONG llA, SHORT sB);
BOOL _cdecl iNeqUL(LONGLONG llA, ULONG ulB);
BOOL _cdecl iNeqUS(LONGLONG llA, USHORT usB);
void _cdecl Assign(PULONGLONG pullA, ULONGLONG ullB);
void _cdecl AssignUL(PULONGLONG pullA, ULONG ulB);
void _cdecl AssignUS(PULONGLONG pullA, USHORT usB);
void _cdecl iAssign(PLONGLONG pllA, LONGLONG llB);
void _cdecl iAssignL(PLONGLONG pllA, LONG lB);
void _cdecl iAssignS(PLONGLONG pllA, SHORT sB);
void _cdecl iAssignUL(PLONGLONG pllA, ULONG ulB);
void _cdecl iAssignUS(PLONGLONG pllA, USHORT usB);
char _far *ulltoa( ULONGLONG value, char _far *buffer, int radix );
char _far *lltoa( LONGLONG value, char _far *buffer, int radix );
typedef unsigned long   ULONG;
typedef ULONG *         PULONG;
typedef unsigned short  APIRET;
#elif defined(_WIN32)
typedef unsigned long long ULONGLONG;
typedef long long LONGLONG;
typedef unsigned long long *PULONGLONG;
typedef long long *PLONGLONG;
typedef unsigned long   APIRET;
#endif

typedef void    *HANDLE;

#ifdef __IBMC__ /* IBMC */
#pragma pack(1)
#define fastcall
#define _loadds
#endif

#define PUBLIC
#ifdef PRIVATE
#undef PRIVATE
#endif
#define PRIVATE	static
#define IMPORT		extern

#define PROTO(a)  a

#ifndef TRUE
#define FALSE		0
#define TRUE		(!FALSE)
#endif

#ifndef ON
#define OFF       0
#define ON        (!OFF)
#endif

#ifndef NULL
#define NULL		(void *)0
#endif

#ifndef NUL_CHAR
#define NUL_CHAR  '\0'
#endif

//#ifndef FAR
//#define FAR       _far
//#endif

//#ifndef NEAR
//#define NEAR      _near
//#endif

#ifndef __OS2__
#define FILE_NORMAL    0x0000
#define FILE_READONLY  0x0001
#define FILE_HIDDEN    0x0002
#define FILE_SYSTEM    0x0004
#define FILE_DIRECTORY 0x0010
#define FILE_ARCHIVED  0x0020
#define FILE_IGNORE    0x10000

#define CCHMAXPATH          260
#define CCHMAXPATHCOMP      256
#endif

#define _LNK_CONV

/*
   Standard types                      Prefix to be used for variable names.
*/

#if !defined(OS2DEF_INCLUDED) //|| defined(__RING3__)

//#if !defined(__16BITS__)
#if !defined(_INC_WINDOWS) && (!defined(__16BITS__) || defined(__RING3__))
typedef int             BOOL ;      /* b */
typedef BOOL *          PBOOL ;     /* pb */
#endif 
//#endif

#if !defined(__16BITS__) || defined(__RING3__)
typedef char            CHAR ;      /* c */
typedef signed short    SHORT ;     /* s */
typedef signed int      INT ;       /* i */
#endif

typedef CHAR *          PCHAR ;     /* pc */

typedef unsigned char   UCHAR ;     /* uc */
typedef UCHAR *         PCH ;       /* pc  Use this for charbuffs. (or PBYTE) */
typedef UCHAR *         PSZ ;       /* psz Use this for ASCIIZ strings. */

#if !defined(BYTE) && !defined(_INC_WINDOWS)
typedef unsigned char   BYTE ;      /* uc */
#endif

#if !defined(PBYTE) && !defined(_INC_WINDOWS)
typedef BYTE *          PBYTE ;     /* puc */
#else
#define PBYTE LPBYTE
#endif


typedef SHORT *         PSHORT ;    /* ps */

typedef unsigned short  USHORT ;    /* us */
typedef USHORT *        PUSHORT ;   /* pus */

#ifndef _INC_WINDOWS
typedef INT *           PINT ;      /* pi */
#else
#define PINT LPINT
#endif

#ifndef _INC_WINDOWS
typedef unsigned int    UINT ;      /* ui */
#endif
typedef UINT *          PUINT ;     /* pui */

#if !defined(LONG) && !defined(_INC_WINDOWS)
typedef signed long     LONG ;      /* l */
#endif
#ifndef _INC_WINDOWS
typedef LONG *          PLONG ;     /* pl */
#else
#undef LONG
#define LONG LPLONG
#endif

typedef unsigned long   ULONG ;     /* ul */
typedef ULONG *         PULONG ;    /* pul */

#if (_MSC_VER >= 600)
#ifndef _INC_WINDOWS
typedef void            VOID ;
#endif
#else
#define VOID            void
#endif

////typedef void _far *          PVOID ;     /* p */
//typedef void *          PVOID ;     /* p */


/*
   Macros
*/
/* This macro combines 2 shorts into 1 unsigned long... */
//#define MAKEULONG(l, h) ((ULONG)(((USHORT)(l)) | ((ULONG)((USHORT)(h))) << 16))
/* This macro combines an offset and a segment value into a far void pointer */
//#define MAKEP(seg, off) ((PFVOID)MAKEULONG(off, seg))

#endif /* OS2DEF_INCLUDED */

#ifndef FP_SEG
#define FP_SEG(fp) (*((unsigned _far *)&(fp)+1))
#define FP_OFF(fp) (*((unsigned _far *)&(fp)))
#endif


typedef BYTE _far *     PFBYTE ;    /* pfuc */

#if !defined(WORD) && !defined(_INC_WINDOWS)
typedef unsigned short  WORD ;      /* w */
#endif
#ifndef _INC_WINDOWS
typedef WORD *          PWORD ;     /* pw */
#else
#define PWORD LPWORD
#endif

typedef double          DOUBLE ;    /* dbl */
typedef DOUBLE *        PDOUBLE ;   /* pdbl */

typedef float           FLOAT ;     /* flt */
typedef FLOAT *         PFLOAT ;    /* pflt */

typedef unsigned short  FLAGS ;     /* f  16 bit flags */
typedef FLAGS *         PFLAGS ;    /* pf */

typedef void _far *     PFVOID ;    /* p Far Pointer to near void */

#ifndef __OS2__

#define ERROR_EA_LIST_TOO_LONG          256

#define FEA_NEEDEA   0x80
#define EAT_BINARY   0xFFFE

#define FIL_QUERYEASFROMLIST   3
#define FIL_QUERYEASFROMLISTL 13

typedef struct _GEA {
    BYTE cbName;
    CHAR szName[1];
} GEA, *PGEA;

typedef struct _GEALIST {
    ULONG cbList;
    GEA list[1];
} GEALIST, *PGEALIST;

typedef struct _FEA {
    BYTE fEA;
    BYTE cbName;
    USHORT cbValue;
} FEA, *PFEA;

typedef struct _FEALIST {
    ULONG cbList;
    FEA list[1];
} FEALIST, *PFEALIST;

typedef struct _EAOP {
    PGEALIST fpGEAList;
    PFEALIST fpFEAList;
    ULONG    oError;
} EAOP, *PEAOP;
#endif

#endif
