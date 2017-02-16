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

#if defined(__OS2__) && !defined(__16BITS__)
#define INCL_LONGLONG
#define INCL_DOSPROCESS
#define INCL_DOSPROFILE
#define INCL_DOSSEMAPHORES
#define INCL_DOSDEVIOCTL
#define INCL_DOSDEVICES
#define INCL_DOSERRORS
#define INCL_DOSFILEMGR
#define INCL_DOSMODULEMGR
#define INCL_DOSDATETIME
#define INCL_LONGLONG
#define INCL_DOSMISC
#define INCL_DOSNLS
#define INCL_VIO
#include <os2.h>
typedef ULONG   DWORD;
#elif defined(__16BITS__)
typedef unsigned long   ULONG;
typedef ULONG *         PULONG;
typedef unsigned short  APIRET;
#elif defined(_WIN32)
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

#if !defined(OS2DEF_INCLUDED) && !defined(__16BITS__)

#ifndef _INC_WINDOWS
typedef int             BOOL ;      /* b */
#endif 
typedef BOOL *          PBOOL ;     /* pb */


typedef char            CHAR ;      /* c */
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


typedef signed short    SHORT ;     /* s */
typedef SHORT *         PSHORT ;    /* ps */

typedef unsigned short  USHORT ;    /* us */
typedef USHORT *        PUSHORT ;   /* pus */

typedef signed int      INT ;       /* i */
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


#ifdef __16BITS__
typedef long long          LONGLONG;
typedef unsigned long long ULONGLONG;
#endif

#endif



