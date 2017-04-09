#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <signal.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <process.h>

#define INCL_DOSNLS
#define INCL_DOSDEVIOCTL
#define INCL_DOS
#define INCL_DOSERRORS
#define INCL_DOSDEVICES
#define INCL_DOSPROCESS
#define INCL_VIO
#include <os2.h>
#include "portable.h"
#include "fat32def.h"

#define TIME_FACTOR 1

#define SHAREMEM	 "\\SHAREMEM\\CACHEF32"

PRIVATE VOID Handler(INT iSignal);
PRIVATE VOID InitProg(INT iArgc, PSZ rgArgv[]);
PRIVATE PSZ GetFSName(PSZ pszDevice);
/*
PRIVATE VOID APIENTRY LWThread(ULONG ulArg);
PRIVATE VOID APIENTRY EMThread(ULONG ulArg);
*/
PRIVATE VOID _LNK_CONV LWThread(PVOID ulArg);
PRIVATE VOID _LNK_CONV EMThread(PVOID ulArg);

PRIVATE BOOL IsDiskClean(PSZ pszDrive);
PRIVATE BOOL DoCheckDisk(BOOL fDoCheck);
PRIVATE BOOL ChkDsk(ULONG ulBootDisk, PSZ pszDrive);
PRIVATE PSZ  QueryMyFullPath( VOID );
PRIVATE BOOL StartMe(PSZ pszPath);
PRIVATE ULONG GetFAT32Drives(VOID);
PRIVATE BOOL IsDiskFat32(PSZ pszDisk);
PRIVATE VOID ShowRASectors(VOID);
PRIVATE BOOL SetRASectors(PSZ pszArg);
PRIVATE void WriteLogMessage(PSZ pszMessage);

int remount_all(void);

BOOL (*pLoadTranslateTable)(BOOL fSilent, UCHAR ucSource);

HMODULE   hMod = 0;

static PSZ rgPriority[]=
{
"No change",
"Idle time",
"Regular",
"Time critical",
"Foreground server"
};

static F32PARMS  f32Parms;
static BOOL 	 fActive = FALSE;
static BOOL 	 fLoadDeamon = TRUE;
static BOOL 	 fSayYes = FALSE;
static BOOL 	 fSilent = FALSE;
static ULONG	 ulNewCP = 0;
static PLWOPTS	 pOptions = NULL;
static BOOL 	 fForeGround;
static ULONG	 ulDriveMap = 0;

/******************************************************************
*
******************************************************************/
INT main(INT iArgc, PSZ rgArgv[])
{
APIRET rc;
ULONG ulParmSize;
ULONG ulDataSize;
BYTE  bPrevPrio;

   //DoCheckDisk(FALSE);

   InitProg(iArgc, rgArgv);
   if (fActive)
	  DosExit(EXIT_PROCESS, 0);

   if (fForeGround)
	  {
	  //DoCheckDisk(TRUE);
	  if (!f32Parms.usCacheSize && !f32Parms.fForceLoad )
		 printf("Cache size has been set to zero, lazy writer will not be started!\n");
	  else if (fLoadDeamon)
		 StartMe(rgArgv[0]);
	  DosExit(EXIT_PROCESS, 0);
	  }
   else
          {
          // remount all FAT disks on cachef32.exe start
          // (they may be mounted previously by the in-kernel FAT driver)
          remount_all();
          }

   rc = DosFSCtl(NULL, 0, &ulDataSize,
				 NULL, 0, &ulParmSize,
	  FAT32_STOPLW, "FAT32", -1, FSCTL_FSDNAME);

   rc = DosFSCtl(NULL, 0, &ulDataSize,
				 NULL, 0, &ulParmSize,
	  FAT32_STARTLW, "FAT32", -1, FSCTL_FSDNAME);
   if (rc)
	  {
	  printf("Starting LW failed, rc = %d\n", rc);
	  exit(1);
	  }

   signal(SIGBREAK, Handler);
   signal(SIGTERM, Handler);
   signal(SIGINT, Handler);

/*
   rc = DosCreateThread(&pOptions->ulEMTID, EMThread, 0L, 0, 8196);
   if (rc)
	  printf("DosCreateThread failed, rc = %d\n", rc);

   rc = DosCreateThread(&pOptions->ulLWTID, LWThread, 0L, 0, 8196);
   if (rc)
	  printf("DosCreateThread failed, rc = %d\n", rc);
*/
   pOptions->ulEMTID = _beginthread(EMThread,NULL,8192,NULL);
   if (pOptions->ulEMTID == -1)
	  printf("_beginthread failed, rc = %d\n", -1);

   pOptions->ulLWTID = _beginthread(LWThread,NULL,8192,NULL);
   if (pOptions->ulLWTID == -1)
	  printf("_beginthread failed, rc = %d\n", -1);

   ulParmSize = sizeof f32Parms;
   rc = DosFSCtl(
	  NULL, 0, &ulDataSize,
	  (PVOID)&f32Parms, ulParmSize, &ulParmSize,
	  FAT32_SETPARMS, "FAT32", -1, FSCTL_FSDNAME);

   bPrevPrio = pOptions->bLWPrio;
   while (!pOptions->fTerminate)
	  {
	  if (bPrevPrio != pOptions->bLWPrio)
		 {
		 DosSetPriority(PRTYS_THREAD,
			pOptions->bLWPrio, 0, pOptions->ulLWTID);
		 bPrevPrio = pOptions->bLWPrio;
		 }
	  DosSleep(5000);
	  }

   DosExit(EXIT_PROCESS, 0);

   return 0;
}

/******************************************************************
*
******************************************************************/
VOID _LNK_CONV LWThread(PVOID ulArg)
{
APIRET rc;
ULONG  ulParmSize;
ULONG  ulDataSize;

   rc = DosSetPriority(PRTYS_THREAD,
	  pOptions->bLWPrio, 0, 0);

   ulParmSize = sizeof (LWOPTS);
   ulDataSize = 0;

   rc = DosFSCtl(NULL, 0, &ulDataSize,
	  (PVOID)pOptions, ulParmSize, &ulParmSize,
	  FAT32_DOLW, "FAT32", -1, FSCTL_FSDNAME);

   pOptions->fTerminate = TRUE;

   ulArg = ulArg;
   rc = rc;
}

VOID _LNK_CONV EMThread(PVOID ulArg)
{
APIRET rc;
ULONG  ulParmSize;
ULONG  ulDataSize;

#if 1
//	 rc = DosSetPriority(PRTYS_THREAD,
//		PRTYC_TIMECRITICAL, PRTYD_MAXIMUM, 0);
#else
   rc = DosSetPriority(PRTYS_THREAD,
	  PRTYC_FOREGROUNDSERVER, PRTYD_MAXIMUM, 0);
#endif

   ulParmSize = sizeof (LWOPTS);
   ulDataSize = 0;

   rc = DosFSCtl(NULL, 0, &ulDataSize,
	  (PVOID)pOptions, ulParmSize, &ulParmSize,
	  FAT32_EMERGTHREAD, "FAT32", -1, FSCTL_FSDNAME);
   if (rc)
	  printf("EMThread: rc = %u\n", rc);

   ulArg = ulArg;
}

/******************************************************************
*
******************************************************************/
VOID Handler(INT iSignal)
{
	printf("Signal %d was received\n", iSignal);
	if (iSignal == SIGTERM)
	{
	   pOptions->fTerminate = TRUE;
	}
//	 exit(1);
}

/******************************************************************
*
******************************************************************/
VOID InitProg(INT iArgc, PSZ rgArgv[])
{
APIRET	  rc;
INT 	  iArg;
ULONG	  ulLVB;
USHORT	  uscbLVB;
ULONG	  ulDataSize;
ULONG	  ulParmSize;
BOOL	  fSetParms = FALSE;
BYTE      rgData[ 256 ];
ULONG	  ulParm;

   for (iArg = 1; iArg < iArgc; iArg++)
   {
	  strupr(rgArgv[iArg]);
	  if (( rgArgv[iArg][0] == '/' || rgArgv[iArg][0] == '-' ) &&
		  rgArgv[ iArg ][ 1 ] == 'S' )
	  {
		  fSilent = TRUE;
		  break;
	  }
   }

   rc = DosLoadModule(rgData, sizeof(rgData), "UFAT32.DLL", &hMod);
   if (rc)
      {
      printf("FAT32: Utility DLL not found (%s does not load).\n", rgData);
      printf("FAT32: No UNICODE translate table loaded!\n");
      return;
      }
   rc = DosQueryProcAddr(hMod, 0L,
      "LoadTranslateTable", (PFN *)&pLoadTranslateTable);
   if (rc)
      {
      printf("FAT32: ERROR: Could not find address of LoadTranslateTable.\n");
      return;
      }

   /*
	  Determine if we run in the foreground
   */
   rc = VioGetBuf(&ulLVB, &uscbLVB, (HVIO)0);
   if (rc)
	  fForeGround = FALSE;
   else
	  fForeGround = TRUE;

   if (fForeGround)
   {
	  if( !fSilent )
		printf("FAT32 cache helper version %s.\n", FAT32_VERSION);
   }
   else
	  WriteLogMessage("FAT32 task detached");

   rc = DosGetNamedSharedMem((PVOID *)&pOptions, SHAREMEM, PAG_READ|PAG_WRITE);
   if (!rc)
	  {
	  fActive = TRUE;
	  WriteLogMessage("Shared memory found!");
	  }
   else
	  {
	  rc = DosAllocSharedMem((PVOID *)&pOptions, SHAREMEM, sizeof (LWOPTS), PAG_COMMIT|PAG_READ|PAG_WRITE);
	  if (rc)
		  DosExit(EXIT_PROCESS, 1);
	  memset(pOptions, 0, sizeof pOptions);
	  pOptions->bLWPrio = PRTYC_IDLETIME;
	  WriteLogMessage("Shared memory allocated!");
	  }

   ulDataSize = sizeof f32Parms;
   rc = DosFSCtl(
	  (PVOID)&f32Parms, ulDataSize, &ulDataSize,
	  NULL, 0, &ulParmSize,
	  FAT32_GETPARMS, "FAT32", -1, FSCTL_FSDNAME);
   if (rc)
	  {
	  printf("DosFSCtl, FAT32_GETPARMS failed, rc = %d\n", rc);
	  DosExit(EXIT_PROCESS, 1);
	  }
   if (strcmp(f32Parms.szVersion, FAT32_VERSION))
	  {
	  printf("ERROR: FAT32 version (%s) differs from CACHEF32 version (%s)\n", f32Parms.szVersion, FAT32_VERSION);
	  DosExit(EXIT_PROCESS, 1);
	  }

   for (iArg = 1; iArg < iArgc; iArg++)
	  {
	  strupr(rgArgv[iArg]);
	  if (rgArgv[iArg][0] == '/' || rgArgv[iArg][0] == '-')
		 {
		 switch (rgArgv[iArg][1])
			{
			case '?' :
			   printf("USAGE: CACHEF32 [options]\n");
			   printf("/Q (Quit)\n");
			   printf("/N do NOT load lazy write deamon.\n");
			   printf("/D:diskidle in millisecs.\n");
			   printf("/B:bufferidle in millisecs.\n");
			   printf("/M:maxage in millisecs.\n");
			   printf("/R:d:,n sets read ahead sector count for drive d: to n.\n");
			   printf("/L:on|off set lazy writing on or off.\n");
			   printf("/P:1|2|3|4 Set priority of Lazy writer\n");
			   printf("/Y assume yes\n");
			   printf("/S do NOT display normal messages\n");
			   printf("/CP:cp load unicode translate table for cp\n");
			   printf("/F force lazy write deamon to be loaded\n");
			   DosExit(EXIT_PROCESS, 0);
			   break;

			case 'P':
			   if (rgArgv[iArg][2] != ':')
				  {
				  printf("Missing : after /P\n");
				  DosExit(EXIT_PROCESS, 1);
				  }
			   if (rgArgv[iArg][3] < '1' ||
				   rgArgv[iArg][3] > '4')
				  {
				  printf("Lazy write priority should be from 1 to 4!\n");
				  DosExit(EXIT_PROCESS, 1);
				  }
			   pOptions->bLWPrio = rgArgv[iArg][3] - '0';
			   break;


			case 'N':
			   fLoadDeamon = FALSE;
			   f32Parms.fForceLoad = FALSE;
			   fSetParms = TRUE;
			   break;

			case 'T':
			   printf("The /T option is no longer supported.\n");
			   printf("Please read the documentation.\n");
			   break;

			case 'Q' :
			   if (fActive)
				  {
				  if (pOptions->fTerminate)
					 printf("Terminate request already set!\n");
				  pOptions->fTerminate = TRUE;
				  printf("Terminating CACHEF32.EXE...\n");
				  DosExit(EXIT_PROCESS, 0);
				  }
			   printf("/Q is invalid, CACHEF32 is not running!\n");
			   DosExit(EXIT_PROCESS, 1);
			   break;
			case 'D':
			   if (rgArgv[iArg][2] != ':')
				  {
				  printf("ERROR: missing : in %s\n", rgArgv[iArg]);
				  DosExit(EXIT_PROCESS, 1);
				  }
			   ulParm = atol(&rgArgv[iArg][3]);
			   if (!ulParm)
				  {
				  printf("ERROR: Invalid value in %s\n", rgArgv[iArg]);
				  DosExit(EXIT_PROCESS, 1);
				  }
			   f32Parms.ulDiskIdle = ulParm / TIME_FACTOR;
			   fSetParms = TRUE;
			   break;

			case 'B':
			   if (rgArgv[iArg][2] != ':')
				  {
				  printf("ERROR: missing : in %s\n", rgArgv[iArg]);
				  DosExit(EXIT_PROCESS, 1);
				  }
			   ulParm = atol(&rgArgv[iArg][3]);
			   if (!ulParm)
				  {
				  printf("ERROR: Invalid value in %s\n", rgArgv[iArg]);
				  DosExit(EXIT_PROCESS, 1);
				  }
			   f32Parms.ulBufferIdle = ulParm / TIME_FACTOR;
			   fSetParms = TRUE;
			   break;

			case 'M':
			   if (rgArgv[iArg][2] != ':')
				  {
				  printf("ERROR: missing : in %s\n", rgArgv[iArg]);
				  DosExit(EXIT_PROCESS, 1);
				  }
			   ulParm = atol(&rgArgv[iArg][3]);
			   if (!ulParm)
				  {
				  printf("ERROR: Invalid value in %s\n", rgArgv[iArg]);
				  DosExit(EXIT_PROCESS, 1);
				  }
			   f32Parms.ulMaxAge = ulParm / TIME_FACTOR;
			   fSetParms = TRUE;
			   break;

			case 'R':
			   if (rgArgv[iArg][2] != ':')
				  {
				  printf("ERROR: missing : in %s\n", rgArgv[iArg]);
				  DosExit(EXIT_PROCESS, 1);
				  }
			   SetRASectors(&rgArgv[iArg][3]);
			   break;
			case 'F':
			   if( rgArgv[iArg][2] == '\0' )
			   {
				  f32Parms.fForceLoad = TRUE;
				  fSetParms = TRUE;
			   }
			   else if (rgArgv[iArg][2] == 'S' || rgArgv[iArg][2] == 'L' )
			   {
				  printf("Both /FS and /FL option is not supported any more.\n");
				  printf("Please read the documentation.\n");
				  break;
			   }
			   else
			   {
				  printf("ERROR: Unknown option %s\n", rgArgv[iArg]);
				  DosExit(EXIT_PROCESS, 1);
			   }
			   break;

			case 'L':
			   if (!stricmp(&rgArgv[iArg][2], ":ON"))
				  {
				  rc = DosFSCtl(NULL, 0, NULL,
							  NULL, 0, NULL,
					 FAT32_STARTLW, "FAT32", -1, FSCTL_FSDNAME);
				  if (rc)
					 printf("Warning: Lazy writing is already active or cachesize is 0!\n");
				  }
			   else if (!stricmp(&rgArgv[iArg][2], ":OFF"))
				  {
				  rc = DosFSCtl(NULL, 0, NULL,
							  NULL, 0, NULL,
					 FAT32_STOPLW, "FAT32", -1, FSCTL_FSDNAME);
				  if (rc)
					 printf("Warning: Lazy writing is not active!\n");
				  }
			   else
				  {
				  printf("ERROR: Unknown option %s\n", rgArgv[iArg]);
				  DosExit(EXIT_PROCESS, 1);
				  }
			   break;

			case 'Y':
			   fSayYes = TRUE;
			   break;

			case 'S':
			   break;

			case 'C':
			   if (( rgArgv[ iArg ][ 2 ] == 'P' ) && ( rgArgv[ iArg ][ 3 ] == ':' ))
			   {
				  ulParm = atol(&rgArgv[iArg][4]);
				  if( !ulParm )
				  {
					printf("ERROR: Invalid value in %s\n", rgArgv[iArg]);
					DosExit(EXIT_PROCESS, 1);
				  }

				  ulNewCP = ulParm;
				  break;
			   }

			default :
			   printf("ERROR: Unknown option %s\n", rgArgv[iArg]);
			   DosExit(EXIT_PROCESS, 1);
			   break;
			}

		 }
	  }

   if ( fForeGround && (*pLoadTranslateTable)(FALSE, TRUE))
   	  fSetParms = TRUE;

   if( hMod )
        DosFreeModule(hMod);

   if (fSetParms)
	  {
	  if (f32Parms.ulDiskIdle < f32Parms.ulBufferIdle)
		 {
		 printf("DISKIDLE must be greater than BUFFERIDLE\n");
		 DosExit(EXIT_PROCESS, 1);
		 }

	  ulParmSize = sizeof f32Parms;
	  rc = DosFSCtl(
		 NULL, 0, &ulDataSize,
		 (PVOID)&f32Parms, ulParmSize, &ulParmSize,
		 FAT32_SETPARMS, "FAT32", -1, FSCTL_FSDNAME);

	  if (rc)
		 {
		 printf("DosFSCtl FAT32_SETPARMS, failed, rc = %d\n", rc);
		 DosExit(EXIT_PROCESS, 1);
		 }
	  }

   ulDriveMap = GetFAT32Drives();
   if (!fActive)
	  {
	  if (!ulDriveMap)
		 {
		 if( !f32Parms.fForceLoad )
			{
			printf("FAT32: No FAT32 partitions found, aborting...\n");
			printf("FAT32: Use /F option to load lazy write deamon\n");
			DosExit(EXIT_PROCESS, 1);
			}
		 }
	  }

   /*
	  Query parms
   */

   if ( fActive /*|| !f32Parms.usCacheSize*/)
	  {
	  if (fActive)
		 {
		 printf("CACHEF32 is already running.\n");
		 printf("Current priority is %s.\n", rgPriority[pOptions->bLWPrio]);
		 }

	  if (!f32Parms.fLW)
		 printf("LAZY WRITING is NOT active!\n\n");
	  else
		 {
		 printf("\n");
		 printf("DISKIDLE  : %lu milliseconds.\n", f32Parms.ulDiskIdle * TIME_FACTOR);
		 printf("BUFFERIDLE: %lu milliseconds.\n", f32Parms.ulBufferIdle * TIME_FACTOR);
		 printf("MAXAGE    : %lu milliseconds.\n", f32Parms.ulMaxAge * TIME_FACTOR);
		 }

	  printf("\n");
	  ShowRASectors();
	  printf("\n");
	  if( f32Parms.usCacheSize )
		{
		if( f32Parms.fHighMem )
			printf("CACHE has all space in high memory.\n");
		printf("CACHE has space for %u sectors\n", f32Parms.usCacheSize);
		printf("CACHE contains %u sectors\n", f32Parms.usCacheUsed);
		printf("There are %u dirty sectors in cache.\n", f32Parms.usDirtySectors);
		if (f32Parms.usPendingFlush > 0)
			printf("%u sectors are in pending flush state.\n", f32Parms.usPendingFlush);
		printf("The cache hits ratio is %3d%%.\n",
			f32Parms.ulTotalReads > 0 ? f32Parms.ulTotalHits * 100 / f32Parms.ulTotalReads: 0UL);
		}

	  printf("FAT32.IFS has currently %u GDT segments allocated.\n",
		f32Parms.usSegmentsAllocated);

	  }
   return;
}

VOID ShowRASectors(VOID)
{
USHORT usIndex;
APIRET rc;
HFILE hDisk;
ULONG ulAction;
USHORT usRASectors;
ULONG  ulDataSize;

	  for (usIndex = 0; usIndex < 26; usIndex++)
		 {
		 ULONG Mask = 0x0001 << usIndex;
		 BYTE szDisk[3];

		 if (!(ulDriveMap & Mask))
			continue;
		 szDisk[0] = (BYTE)('A' + usIndex);
		 szDisk[1] = ':';
		 szDisk[2] = 0;


		 rc = DosOpen(szDisk,
			&hDisk,
			&ulAction,							/* action taken */
			0L, 								/* new size 	*/
			0L, 								/* attributes	*/
			OPEN_ACTION_OPEN_IF_EXISTS, 		/* open flags	*/
			OPEN_ACCESS_READONLY |				/* open mode	*/
			OPEN_SHARE_DENYNONE |
			OPEN_FLAGS_DASD,
			NULL);								/* ea data		*/

		 ulDataSize = sizeof usRASectors;
		 usRASectors = FALSE;
		 rc = DosDevIOCtl(hDisk,
			IOCTL_FAT32,
			FAT32_QUERYRASECTORS,
			NULL, 0, NULL,
			(PVOID)&usRASectors, ulDataSize, &ulDataSize);
		 if (rc)
			printf("DosDevIOCtl, FAT_QUERYRASECTORS for drive %s failed, rc = %d\n",
			   szDisk, rc);

		 DosClose(hDisk);
		 if (!rc)
			printf("Read-Ahead sector count for drive %s is %u.\n",
			   szDisk, usRASectors);
		 }

}

BOOL SetRASectors(PSZ pszArg)
{
APIRET rc;
HFILE hDisk;
ULONG ulAction;
USHORT usRASectors;
ULONG  ulDataSize;
BYTE   szDisk[3];

   if (pszArg[1] != ':')
	  {
	  printf("Invalid argument for /R option.\n");
	  DosExit(EXIT_PROCESS, 1);
	  }
   memset(szDisk, 0, sizeof szDisk);
   memcpy(szDisk, pszArg, 2);
   strupr(szDisk);
   pszArg+=2;
   if (*pszArg !=',')
	  {
	  printf("Comma missing in /R:d:,n\n");
	  DosExit(EXIT_PROCESS, 1);
	  }
   pszArg++;
   usRASectors = atoi(pszArg);

   rc = DosOpen(szDisk,
	  &hDisk,
	  &ulAction,						  /* action taken */
	  0L,								  /* new size	  */
	  0L,								  /* attributes   */
	  OPEN_ACTION_OPEN_IF_EXISTS,		  /* open flags   */
	  OPEN_ACCESS_READONLY |			  /* open mode	  */
	  OPEN_SHARE_DENYNONE |
	  OPEN_FLAGS_DASD,
	  NULL);							  /* ea data	  */

   if (rc)
	  {
	  printf("Cannot access drive %s, rc = %d\n",
		 szDisk, rc);
	  DosExit(EXIT_PROCESS, 1);
	  }

   ulDataSize = sizeof usRASectors;
   rc = DosDevIOCtl(hDisk,
	  IOCTL_FAT32,
	  FAT32_SETRASECTORS,
	  (PVOID)&usRASectors, ulDataSize, &ulDataSize,
	  NULL, 0, NULL);
   if (rc)
	  {
	  printf("DosDevIOCtl, FAT_SETRASECTORS for drive %s failed, rc = %d\n",
		 szDisk, rc);
	  DosExit(EXIT_PROCESS, 1);
	  }

   DosClose(hDisk);
   return TRUE;
}

/******************************************************************
*
******************************************************************/
PSZ GetFSName(PSZ pszDevice)
{
static BYTE Buffer[200];
ULONG ulBufferSize;
PFSQBUFFER2 fsqBuf = (PFSQBUFFER2)Buffer;
APIRET rc;

   ulBufferSize = sizeof Buffer;

   DosError(0);
   rc = DosQueryFSAttach(pszDevice,
	  1L,
	  FSAIL_QUERYNAME,
	  fsqBuf,
	  &ulBufferSize);
   DosError(1);
   if (rc)
	  return "";
   return fsqBuf->szName + fsqBuf->cbName + 1;
}

/******************************************************************
*
******************************************************************/
BOOL DoCheckDisk(BOOL fDoCheck)
{
ULONG ulBootDisk;
USHORT usIndex;

	  DosQuerySysInfo( QSV_BOOT_DRIVE, QSV_BOOT_DRIVE, &ulBootDisk, sizeof( ULONG ));
	  ulDriveMap = GetFAT32Drives();

	  for (usIndex = 0; usIndex < 26; usIndex++)
		 {
		 ULONG Mask = 0x0001 << usIndex;
		 BYTE szDisk[3];

		 if (!(ulDriveMap & Mask))
			continue;
		 szDisk[0] = (BYTE)('A' + usIndex);
		 szDisk[1] = ':';
		 szDisk[2] = 0;

		 if (!IsDiskClean(szDisk) && fDoCheck)
			if (!ChkDsk(ulBootDisk, szDisk))
			   return FALSE;
		 }

	  return TRUE;
}


/******************************************************************
*
******************************************************************/
BOOL ChkDsk(ULONG ulBootDisk, PSZ pszDisk)
{
APIRET rc;
static BYTE szObjName[255]={0};
static BYTE szProgram[255] = {"X:\\OS2\\CHKDSK.COM"};
static BYTE szArguments[512] ={0};
RESULTCODES Res;

   szProgram[0] = (BYTE)(ulBootDisk + '@');
   memset(szArguments, 0, sizeof szArguments);
   strcpy(szArguments, szProgram);
   sprintf(szArguments + strlen(szArguments) + 1,
	  "%s /F /C", pszDisk);

   rc = DosExecPgm(szObjName, sizeof szObjName,
	  EXEC_SYNC,
	  szArguments,
	  NULL,
	  &Res,
	  szProgram);
   if (rc)
	  {
	  printf("DosExecPgm Failed due to %s, rc = %d\n", szObjName, rc);
	  return FALSE;
	  }
   return TRUE;
}

/******************************************************************
*
******************************************************************/
BOOL IsDiskClean(PSZ pszDisk)
{
APIRET rc;
HFILE hDisk;
ULONG ulAction;
USHORT fClean;
ULONG  ulDataSize;

   rc = DosOpen(pszDisk,
	  &hDisk,
	  &ulAction,						  /* action taken */
	  0L,								  /* new size	  */
	  0L,								  /* attributes   */
	  OPEN_ACTION_OPEN_IF_EXISTS,		  /* open flags   */
	  OPEN_ACCESS_READONLY |			  /* open mode	  */
		OPEN_SHARE_DENYNONE |
		OPEN_FLAGS_DASD,
	  NULL);							  /* ea data	  */

   ulDataSize = sizeof fClean;
   fClean = FALSE;
   rc = DosDevIOCtl(hDisk,
	  IOCTL_FAT32,
	  FAT32_GETVOLCLEAN,
	  NULL, 0, NULL,
	  (PVOID)&fClean, ulDataSize, &ulDataSize);
   if (rc)
	  printf("DosDevIOCtl, FAT_GETVOLCLEAN failed, rc = %d\n", rc);

   DosClose(hDisk);
   if (rc)
	  return FALSE;

//	 if (!fClean)
//		printf("FAT32: Drive %s was improperly stopped.\n", pszDisk);


   return fClean;
}

/******************************************************************
*
******************************************************************/
PRIVATE PSZ QueryMyFullPath( VOID )
{
	PPIB	ppib;
	PCHAR	p;

	DosGetInfoBlocks( NULL, &ppib );
	for( p = ppib->pib_pchcmd - 2; *p; p-- )
	{
		;
	}

	return ( p + 1 );
}

/******************************************************************
*
******************************************************************/
BOOL StartMe(PSZ pszPath)
{
APIRET rc;
static BYTE szObjName[255];
static BYTE szArguments[512];
RESULTCODES Res;

   sprintf(szObjName,
	  " /P:%u", pOptions->bLWPrio );
   if( f32Parms.fForceLoad )
	  strcat( szObjName, " /F" );

   memset(szArguments, 0, sizeof szArguments);
   strcpy(szArguments, pszPath);
   strcpy( szArguments + strlen( szArguments ) + 1, szObjName );

   rc = DosExecPgm(szObjName, sizeof szObjName,
	  EXEC_BACKGROUND,
	  szArguments,
	  NULL,
	  &Res,
	  QueryMyFullPath());
   if (rc)
	  {
	  printf("FAT32: unable to start daemon (%s)\n. rc = %d for %s\n",
		 szArguments, rc, szObjName);
	  return FALSE;
	  }
   if( !fSilent )
	  printf("FAT32: Lazy write daemon started.\n");

   return TRUE;
}


ULONG GetFAT32Drives(VOID)
{
USHORT usIndex;
ULONG ulCurDisk;

   if (ulDriveMap)
	  return ulDriveMap;

   if (DosQueryCurrentDisk(&ulCurDisk, &ulDriveMap))
	  {
	  ulDriveMap = 0L;
	  return 0L;
	  }

   for (usIndex = 0; usIndex < 26; usIndex++)
	  {
	  ULONG Mask = 1L << usIndex;
	  BYTE szDisk[3];

	  if (!(ulDriveMap & Mask))
		 continue;

	  /*
		 Skip A: and B:
	  */

	  if (usIndex < 2)
		 {
		 ulDriveMap &= ~Mask;
		 continue;
		 }

	  szDisk[0] = (BYTE)('A' + usIndex);
	  szDisk[1] = ':';
	  szDisk[2] = 0;
	  if (!IsDiskFat32(szDisk))
		 ulDriveMap &= ~Mask;
	  }
   return ulDriveMap;
}

BOOL IsDiskFat32(PSZ pszDisk)
{
   strupr(pszDisk);
   if (!stricmp(GetFSName(pszDisk), "FAT32"))
	  {
	  return TRUE;
	  }
   return FALSE;
}

void WriteLogMessage(PSZ pszMessage)
{
#if 0
FILE *fp;

   return;

   fp = fopen("\\CACHEF32.LOG", "a");
   fprintf(fp, "%s\n", pszMessage);
   fclose(fp);
#endif

   pszMessage = pszMessage;
}
