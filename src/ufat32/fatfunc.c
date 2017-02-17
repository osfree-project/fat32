#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "fat32c.h"

#define RETURN_PARENT_DIR 0xFFFF

#define MODIFY_DIR_INSERT 0
#define MODIFY_DIR_DELETE 1
#define MODIFY_DIR_UPDATE 2
#define MODIFY_DIR_RENAME 3

#define LONGNAME_OFF         0
#define LONGNAME_OK          1
#define LONGNAME_MAKE_UNIQUE 2
#define LONGNAME_ERROR       3

BYTE rgValidChars[]="01234567890 ABCDEFGHIJKLMNOPQRSTUVWXYZ!#$%&'()-_@^`{}~";

ULONG ReadSector(PCDINFO pCD, ULONG ulSector, USHORT nSectors, PBYTE pbSector);
ULONG ReadCluster(PCDINFO pCD, ULONG ulCluster, PVOID pbCluster);
ULONG WriteSector(PCDINFO pCD, ULONG ulSector, USHORT nSectors, PBYTE pbSector);
ULONG WriteCluster(PCDINFO pCD, ULONG ulCluster, PVOID pbCluster);
ULONG ReadFatSector(PCDINFO pCD, ULONG ulSector);
ULONG WriteFatSector(PCDINFO pCD, ULONG ulSector);
ULONG  GetNextCluster(PCDINFO pCD, ULONG ulCluster, BOOL fAllowBad);
BOOL  GetDiskStatus(PCDINFO pCD);
ULONG GetFreeSpace(PCDINFO pCD);
BOOL  MarkDiskStatus(PCDINFO pCD, BOOL fClean);
ULONG FindDirCluster(PCDINFO pCD, PSZ pDir, USHORT usCurDirEnd, USHORT usAttrWanted, PSZ *pDirEnd);
ULONG FindPathCluster(PCDINFO pCD, ULONG ulCluster, PSZ pszPath, PDIRENTRY pDirEntry, PSZ pszFullName);
APIRET ModifyDirectory(PCDINFO pCD, ULONG ulDirCluster, USHORT usMode, PDIRENTRY pOld, PDIRENTRY pNew, PSZ pszLongName);
APIRET MarkFileEAS(PCDINFO pCD, ULONG ulDirCluster, PSZ pszFileName, BYTE fEAS);
USHORT GetSetFileEAS(PCDINFO pCD, USHORT usFunc, PMARKFILEEASBUF pMark);
BOOL fGetLongName(PDIRENTRY pDir, PSZ pszName, USHORT wMax, PBYTE pbCheck);
USHORT QueryUni2NLS( USHORT usPage, USHORT usChar );
BYTE GetVFATCheckSum(PDIRENTRY pDir);
APIRET MakeShortName(PCDINFO pCD, ULONG ulDirCluster, PSZ pszLongName, PSZ pszShortName);
PDIRENTRY fSetLongName(PDIRENTRY pDir, PSZ pszLongName, BYTE bCheck);
USHORT DBCSStrlen( const PSZ pszStr );
BOOL IsDBCSLead( UCHAR uch );
VOID Translate2OS2(PUSHORT pusUni, PSZ pszName, USHORT usLen);
USHORT Translate2Win(PSZ pszName, PUSHORT pusUni, USHORT usLen);
USHORT QueryNLS2Uni( USHORT usCode );
APIRET SetNextCluster(PCDINFO pCD, ULONG ulCluster, ULONG ulNext);
VOID MakeName(PDIRENTRY pDir, PSZ pszName, USHORT usMax);
USHORT GetFreeEntries(PDIRENTRY pDirBlock, ULONG ulSize);
PDIRENTRY CompactDir(PDIRENTRY pStart, ULONG ulSize, USHORT usEntriesNeeded);
VOID MarkFreeEntries(PDIRENTRY pDirBlock, ULONG ulSize);
ULONG GetFreeCluster(PCDINFO pCD);
APIRET SetFileSize(PCDINFO pCD, PFILESIZEDATA pFileSize);
USHORT RecoverChain2(PCDINFO pCD, ULONG ulCluster, PBYTE pData, USHORT cbData);
USHORT MakeDirEntry(PCDINFO pCD, ULONG ulDirCluster, PDIRENTRY pNew, PSZ pszName);
BOOL DeleteFatChain(PCDINFO pCD, ULONG ulCluster);
BOOL UpdateFSInfo(PCDINFO pCD);
ULONG MakeFatChain(PCDINFO pCD, ULONG ulPrevCluster, ULONG ulClustersRequested, PULONG pulLast);
USHORT MakeChain(PCDINFO pCD, ULONG ulFirstCluster, ULONG ulSize);
APIRET MakeFile(PCDINFO pCD, ULONG ulDirCluster, PSZ pszFile, PBYTE pBuf, ULONG cbBuf);
BOOL IsCharValid(char ch);

void set_datetime(DIRENTRY *pDir);

/******************************************************************
*
******************************************************************/
BOOL IsCharValid(char ch)
{
    return (BOOL)strchr(rgValidChars, ch);
}

/******************************************************************
*
******************************************************************/
/* Get the last-character. (sbcs/dbcs) */
int lastchar(const char *string)
{
    UCHAR *s;
    unsigned int c = 0;
    int i, len = strlen(string);
    s = (UCHAR *)string;
    for(i = 0; i < len; i++)
    {
        c = *(s + i);
        if(IsDBCSLead(( UCHAR )c))
        {
            c = (c << 8) + ( unsigned int )*(s + i + 1);
            i++;
        }
    }
    return c;
}

/******************************************************************
*
******************************************************************/
BOOL GetDiskStatus(PCDINFO pCD)
{
   PULONG pulStatus;
   BOOL   fStatus;

   if (ReadFatSector(pCD, 0L))
      return FALSE;

   pulStatus = (PULONG)pCD->pbFATSector + 1;

   if (*pulStatus & FAT32_CLEAN_SHUTDOWN)
      fStatus = TRUE;
   else
      fStatus = FALSE;

   return fStatus;
}

/******************************************************************
*
******************************************************************/
ULONG ReadFatSector(PCDINFO pCD, ULONG ulSector)
{
   APIRET rc;

   if (pCD->ulCurFATSector == ulSector)
      return 0;

   if (ulSector >= pCD->BootSect.bpb.BigSectorsPerFat)
      return ERROR_SECTOR_NOT_FOUND;

   rc = ReadSector(pCD, pCD->ulActiveFatStart + ulSector, 1,
      pCD->pbFATSector);
   if (rc)
      return rc;

   pCD->ulCurFATSector = ulSector;

   return 0;
}

/******************************************************************
*
******************************************************************/
ULONG WriteFatSector(PCDINFO pCD, ULONG ulSector)
{
   USHORT usFat;
   APIRET rc;

   if (pCD->ulCurFATSector != ulSector)
      return ERROR_SECTOR_NOT_FOUND;

   if (ulSector >= pCD->BootSect.bpb.BigSectorsPerFat)
      return ERROR_SECTOR_NOT_FOUND;

   for (usFat = 0; usFat < pCD->BootSect.bpb.NumberOfFATs; usFat++)
      {
      rc = WriteSector(pCD, pCD->ulActiveFatStart + ulSector, 1,
         pCD->pbFATSector);

      if (rc)
         return rc;

      if (pCD->BootSect.bpb.ExtFlags & 0x0080)
         break;

      ulSector += pCD->BootSect.bpb.BigSectorsPerFat;
      }

   return 0;
}

/******************************************************************
*
******************************************************************/
ULONG GetFreeSpace(PCDINFO pCD)
{
   ULONG ulSector;
   ULONG ulCluster;
   ULONG ulTotalFree;
   PULONG pulCluster;

   ulTotalFree = 0;
   for (ulCluster = 2; ulCluster < pCD->ulTotalClusters + 2; ulCluster++)
      {
      ulSector = ulCluster / 128;
      if (ulSector != pCD->ulCurFATSector)
         ReadFatSector(pCD, ulSector);
      pulCluster = (PULONG)pCD->pbFATSector + (ulCluster % 128);
      if ((*pulCluster & FAT_EOF) == 0)
         ulTotalFree++;
      }

   pCD->FSInfo.ulFreeClusters = ulTotalFree;
   UpdateFSInfo(pCD);

   return ulTotalFree;
}

/******************************************************************
*
******************************************************************/
BOOL MarkDiskStatus(PCDINFO pCD, BOOL fClean)
{
   static BYTE bSector[SECTOR_SIZE] = "";
   PULONG pulStatus;
   ULONG ulSector;
   USHORT usFat;
   PBYTE pbSector;

   if (pCD->ulCurFATSector != 0)
      {
      if (ReadSector(pCD, pCD->ulActiveFatStart, 1, bSector))
         return FALSE;
      pbSector = bSector;
      pulStatus = (PULONG)bSector + 1;
      }
   else
      {
      pulStatus = (PULONG)pCD->pbFATSector + 1;
      pbSector = pCD->pbFATSector;
      }

   if (fClean)
      (*pulStatus) |= FAT32_CLEAN_SHUTDOWN;
   else
      (*pulStatus) &= ~FAT32_CLEAN_SHUTDOWN;

   /*
      Trick, set fDiskClean to FALSE, so WriteSector
      won't set is back to dirty again
   */

   ulSector = 0L;
   for (usFat = 0; usFat < pCD->BootSect.bpb.NumberOfFATs; usFat++)
      {
      if (WriteSector(pCD, pCD->ulActiveFatStart + ulSector, 1, pbSector))
         return FALSE;
      if (pCD->BootSect.bpb.ExtFlags & 0x0080)
         break;
      ulSector += pCD->BootSect.bpb.BigSectorsPerFat;
      }

   return TRUE;
}

/******************************************************************
*
******************************************************************/
ULONG FindDirCluster(PCDINFO pCD,
   PSZ pDir,
   USHORT usCurDirEnd,
   USHORT usAttrWanted,
   PSZ *pDirEnd)
{
   BYTE   szDir[FAT32MAXPATH];
   ULONG  ulCluster;
   ULONG  ulCluster2;
   DIRENTRY DirEntry;
   PSZ    p;

   ulCluster = pCD->BootSect.bpb.RootDirStrtClus;
   if (strlen(pDir) >= 2)
      {
      if (pDir[1] == ':')
         pDir += 2;
      }

   if (*pDir == '\\')
      pDir++;

   p = strrchr(pDir, '\\');
   if (!p)
      p = pDir;
   memset(szDir, 0, sizeof szDir);
   memcpy(szDir, pDir, p - pDir);
   if (*p && p != pDir)
      pDir = p + 1;
   else
      pDir = p;

   if (pDirEnd)
      *pDirEnd = pDir;
   ulCluster = FindPathCluster(pCD, ulCluster, szDir, &DirEntry, NULL);
   if (ulCluster == FAT_EOF)
      return FAT_EOF;
   if (ulCluster != FAT_EOF && !(DirEntry.bAttr & FILE_DIRECTORY))
      return FAT_EOF;

   if (*pDir)
      {
      if (usAttrWanted != RETURN_PARENT_DIR && !strpbrk(pDir, "?*"))
         {
         ulCluster2 = FindPathCluster(pCD, ulCluster, pDir, &DirEntry, NULL);
         if (ulCluster2 != FAT_EOF && (DirEntry.bAttr & usAttrWanted) == usAttrWanted)
            {
            if (pDirEnd)
               *pDirEnd = pDir + strlen(pDir);
            return ulCluster2;
            }
         }
      }

   return ulCluster;
}

#define MODE_START  0
#define MODE_RETURN 1
#define MODE_SCAN   2

/******************************************************************
*
******************************************************************/
ULONG FindPathCluster(PCDINFO pCD, ULONG ulCluster, PSZ pszPath, PDIRENTRY pDirEntry, PSZ pszFullName)
{
BYTE szShortName[13];
PSZ  pszLongName;
PSZ  pszPart;
PSZ  p;
PDIRENTRY pDir;
PDIRENTRY pDirStart;
PDIRENTRY pDirEnd;
BOOL fFound;
USHORT usMode;
BYTE   bCheck;

   if (pDirEntry)
      {
      memset(pDirEntry, 0, sizeof (DIRENTRY));
      pDirEntry->bAttr = FILE_DIRECTORY;
      }
   if (pszFullName)
      {
      memset(pszFullName, 0, FAT32MAXPATH);
      if (ulCluster == pCD->BootSect.bpb.RootDirStrtClus)
         {
         pszFullName[0] = (BYTE)(pCD->szDrive[0] + 'A');
         pszFullName[1] = ':';
         pszFullName[2] = '\\';
         }
      }

   if (strlen(pszPath) >= 2)
      {
      if (pszPath[1] == ':')
         pszPath += 2;
      }

   pDirStart = malloc(pCD->ulClusterSize);
   if (!pDirStart)
      return FAT_EOF;
   pszLongName = malloc(FAT32MAXPATHCOMP * 2);
   if (!pszLongName)
      {
      free(pDirStart);
      return FAT_EOF;
      }
   memset(pszLongName, 0, FAT32MAXPATHCOMP * 2);
   pszPart = pszLongName + FAT32MAXPATHCOMP;

   usMode = MODE_SCAN;
   /*
      Allow EA files to be found!
   */
   while (usMode != MODE_RETURN && ulCluster != FAT_EOF)
      {
      usMode = MODE_SCAN;

      if (*pszPath == '\\')
         pszPath++;

      if (!strlen(pszPath))
         break;

      p = strchr(pszPath, '\\');
      if (!p)
         p = pszPath + strlen(pszPath);

      memset(pszPart, 0, FAT32MAXPATHCOMP);
      if (p - pszPath > FAT32MAXPATHCOMP - 1)
         {
         free(pDirStart);
         free(pszLongName);
         return FAT_EOF;
         }

      memcpy(pszPart, pszPath, p - pszPath);
      pszPath = p;

      memset(pszLongName, 0, FAT32MAXPATHCOMP);

      fFound = FALSE;
      while (usMode == MODE_SCAN && ulCluster != FAT_EOF)
         {
         ReadCluster(pCD, ulCluster, (void *)pDirStart);
         pDir    = pDirStart;
         pDirEnd = (PDIRENTRY)((PBYTE)pDirStart + pCD->ulClusterSize);

#ifdef CALL_YIELD
         //Yield();
#endif

         while (usMode == MODE_SCAN && pDir < pDirEnd)
            {
            if (pDir->bAttr == FILE_LONGNAME)
               {
               fGetLongName(pDir, pszLongName, FAT32MAXPATHCOMP, &bCheck);
               }
            else if ((pDir->bAttr & 0x0F) != FILE_VOLID)
               {
               MakeName(pDir, szShortName, sizeof szShortName);
               strupr(szShortName); // !!! @todo DBCS/Unicode
               if (strlen(pszLongName) && bCheck != GetVFATCheckSum(pDir))
                  memset(pszLongName, 0, FAT32MAXPATHCOMP);

                /* support for the FAT32 variation of WinNT family */
                if( !*pszLongName && HAS_WINNT_EXT( pDir->fEAS ))
                {
                    PBYTE pDot;

                    MakeName( pDir, pszLongName, sizeof( pszLongName ));
                    pDot = strchr( pszLongName, '.' );

                    if( HAS_WINNT_EXT_NAME( pDir->fEAS )) /* name part is lower case */
                    {
                        if( pDot )
                            *pDot = 0;

                        strlwr( pszLongName );

                        if( pDot )
                            *pDot = '.';
                    }

                    if( pDot && HAS_WINNT_EXT_EXT( pDir->fEAS )) /* ext part is lower case */
                        strlwr( pDot + 1 );
                }

               if (!strlen(pszLongName))
                  strcpy(pszLongName, szShortName);

               if (( strlen(pszLongName) && !stricmp(pszPart, pszLongName)) ||
                   !stricmp( pszPart, szShortName ))
                  {
                    if( pszFullName )
                        strcat( pszFullName, pszLongName );
                    fFound = TRUE;
                  }

               if (fFound)
                  {
                  ulCluster = (ULONG)pDir->wClusterHigh * 0x10000L + pDir->wCluster;
                  if (strlen(pszPath))
                     {
                     if (pDir->bAttr & FILE_DIRECTORY)
                        {
                        if (pszFullName)
                           strcat(pszFullName, "\\");
                        usMode = MODE_START;
                        break;
                        }
                     ulCluster = FAT_EOF;
                     }
                  else
                     {
                     if (pDirEntry)
                        memcpy(pDirEntry, pDir, sizeof (DIRENTRY));
                     }
                  usMode = MODE_RETURN;
                  break;
                  }
               memset(pszLongName, 0, FAT32MAXPATHCOMP);
               }
            pDir++;
            }
         if (usMode != MODE_SCAN)
            break;
         ulCluster = GetNextCluster(pCD, ulCluster, TRUE);
         if (!ulCluster)
            ulCluster = FAT_EOF;
         }
      }
   free(pDirStart);
   free(pszLongName);
   return ulCluster;
}

/******************************************************************
*
******************************************************************/
USHORT GetSetFileEAS(PCDINFO pCD, USHORT usFunc, PMARKFILEEASBUF pMark)
{
   ULONG ulDirCluster;
   PSZ   pszFile;

   ulDirCluster = FindDirCluster(pCD,
      pMark->szFileName,
      0xFFFF,
      RETURN_PARENT_DIR,
      &pszFile);
   if (ulDirCluster == FAT_EOF)
      return ERROR_PATH_NOT_FOUND;

   if (usFunc == FAT32_QUERYEAS)
      {
      ULONG ulCluster;
      DIRENTRY DirEntry;

      ulCluster = FindPathCluster(pCD, ulDirCluster, pszFile, &DirEntry, NULL);
      if (ulCluster == FAT_EOF)
         return ERROR_FILE_NOT_FOUND;
      pMark->fEAS = DirEntry.fEAS;
      return 0;
      }

   return MarkFileEAS(pCD, ulDirCluster, pszFile, pMark->fEAS);
}

/************************************************************************
*
************************************************************************/
APIRET MarkFileEAS(PCDINFO pCD, ULONG ulDirCluster, PSZ pszFileName, BYTE fEAS)
{
   ULONG ulCluster;
   DIRENTRY OldEntry, NewEntry;
   APIRET rc;

   ulCluster = FindPathCluster(pCD, ulDirCluster, pszFileName, &OldEntry, NULL);
   if (ulCluster == FAT_EOF)
      return ERROR_FILE_NOT_FOUND;
   memcpy(&NewEntry, &OldEntry, sizeof (DIRENTRY));
   if( HAS_OLD_EAS( NewEntry.fEAS ))
        NewEntry.fEAS = FILE_HAS_NO_EAS;
   NewEntry.fEAS = ( BYTE )(( NewEntry.fEAS & FILE_HAS_WINNT_EXT ) | fEAS );

   if (!memcmp(&NewEntry, &OldEntry, sizeof (DIRENTRY)))
      return 0;

   rc = ModifyDirectory(pCD, ulDirCluster,
      MODIFY_DIR_UPDATE, &OldEntry, &NewEntry, NULL);

   return rc;
}

/************************************************************************
*
************************************************************************/
APIRET ModifyDirectory(PCDINFO pCD, ULONG ulDirCluster, USHORT usMode, PDIRENTRY pOld, PDIRENTRY pNew, PSZ pszLongName)
{
   PDIRENTRY pDirectory;
   PDIRENTRY pDir2;
   PDIRENTRY pWork, pWork2;
   PDIRENTRY pMax;
   USHORT    usEntriesNeeded;
   USHORT    usFreeEntries;
   DIRENTRY  DirNew;
   ULONG     ulCluster;
   ULONG     ulPrevCluster;
   ULONG     ulNextCluster;
   PDIRENTRY pLNStart;
   APIRET    rc;
   USHORT    usClusterCount;
   BOOL      fNewCluster;

   //printf("pszLongName(0)=%s\n", pszLongName);

   if (usMode == MODIFY_DIR_RENAME ||
       usMode == MODIFY_DIR_INSERT)
      {
      if (!pNew || !pszLongName)
         return ERROR_INVALID_PARAMETER;

      memcpy(&DirNew, pNew, sizeof (DIRENTRY));
      if ((pNew->bAttr & 0x0F) != FILE_VOLID)
         {
         rc = MakeShortName(pCD, ulDirCluster, pszLongName, DirNew.bFileName);
         //printf("pszLongName(1)=%s\n", pszLongName);
         if (rc == LONGNAME_ERROR)
            return ERROR_FILE_EXISTS;
         memcpy(pNew, &DirNew, sizeof (DIRENTRY));

         if (rc == LONGNAME_OFF)
            pszLongName = NULL;
         }
      else
         pszLongName = NULL;

      usEntriesNeeded = 1;
      if (pszLongName)
         {
#if 0
         usEntriesNeeded += strlen(pszLongName) / 13 +
            (strlen(pszLongName) % 13 ? 1 : 0);
#else
         usEntriesNeeded += ( DBCSStrlen( pszLongName ) + 12 ) / 13;
#endif
         }
      }

   if (usMode == MODIFY_DIR_RENAME ||
       usMode == MODIFY_DIR_DELETE ||
       usMode == MODIFY_DIR_UPDATE)
      {
      if (!pOld)
         return ERROR_INVALID_PARAMETER;
      }

   pDirectory = (PDIRENTRY)malloc(2 * pCD->ulClusterSize);
   if (!pDirectory)
      return ERROR_NOT_ENOUGH_MEMORY;

   memset(pDirectory, 0, pCD->ulClusterSize);
   pDir2 =(PDIRENTRY)((PBYTE)pDirectory + pCD->ulClusterSize);
   memset(pDir2, 0, pCD->ulClusterSize);
   pMax = (PDIRENTRY)((PBYTE)pDirectory + pCD->ulClusterSize * 2);

   ulCluster = ulDirCluster;
   pLNStart = NULL;
   ulPrevCluster = FAT_EOF;
   usClusterCount = 0;
   fNewCluster = FALSE;
   while (ulCluster != FAT_EOF)
      {
#ifdef CALL_YIELD
      //Yield();
#endif

      usClusterCount++;
      if (!fNewCluster)
         {
         rc = ReadCluster(pCD, ulCluster, pDir2);
         if (rc)
            {
            free(pDirectory);
            return rc;
            }
         }
      else
         {
         memset(pDir2, 0, pCD->ulClusterSize);
         fNewCluster = FALSE;
         }

      switch (usMode)
         {
         case MODIFY_DIR_RENAME :
         case MODIFY_DIR_UPDATE :
         case MODIFY_DIR_DELETE :

            /*
               Find old entry
            */

            pWork = pDir2;
            while (pWork != pMax)
               {
               if (pWork->bFileName[0] && pWork->bFileName[0] != DELETED_ENTRY)
                  {
                  if (pWork->bAttr == FILE_LONGNAME)
                     {
                     if (!pLNStart)
                        pLNStart = pWork;
                     }
                  else if ((pWork->bAttr & 0x0F) != FILE_VOLID)
                     {
                     if (!memcmp(pWork->bFileName, pOld->bFileName, 11) &&
                         pWork->wCluster     == pOld->wCluster &&
                         pWork->wClusterHigh == pOld->wClusterHigh)
                        {
                        if (!pLNStart)
                           pLNStart = pWork;
                        break;
                        }
                     pLNStart = NULL;
                     }
                  else
                     pLNStart = NULL;
                  }
               else
                  pLNStart = NULL;
               pWork++;
               }

            if (pWork != pMax)
               {
               switch (usMode)
                  {
                  case MODIFY_DIR_UPDATE:
                     memcpy(pWork, pNew, sizeof (DIRENTRY));
                     rc = WriteCluster(pCD, ulCluster, (void *)pDir2);
                     if (rc)
                        {
                        free(pDirectory);
                        return rc;
                        }
                     ulCluster = FAT_EOF;
                     break;

                  case MODIFY_DIR_DELETE:
                  case MODIFY_DIR_RENAME:
                     pWork2 = pLNStart;
                     while (pWork2 < pWork)
                        {
                        pWork2->bFileName[0] = DELETED_ENTRY;
                        pWork2++;
                        }
                     pWork->bFileName[0] = DELETED_ENTRY;

                     /*
                        Write previous cluster if LN start lies there
                     */
                     if (ulPrevCluster != FAT_EOF &&
                        pLNStart < pDir2)
                        {
                        rc = WriteCluster(pCD, ulPrevCluster, (void *)pDirectory);
                        if (rc)
                           {
                           free(pDirectory);
                           return rc;
                           }
                        }

                     /*
                        Write current cluster
                     */
                     rc = WriteCluster(pCD, ulCluster, (void *)pDir2);
                     if (rc)
                        {
                        free(pDirectory);
                        return rc;
                        }

                     if (usMode == MODIFY_DIR_DELETE)
                        ulCluster = FAT_EOF;
                     else
                        {
                        usMode = MODIFY_DIR_INSERT;
                        ulCluster = ulDirCluster;
                        ulPrevCluster = FAT_EOF;
                        usClusterCount = 0;
                        pLNStart = NULL;
                        continue;
                        }
                     break;
                  }
               }

            break;

         case MODIFY_DIR_INSERT:
            if (ulPrevCluster != FAT_EOF && GetFreeEntries(pDirectory, pCD->ulClusterSize * 2) >= usEntriesNeeded)
               {
               BYTE bCheck = GetVFATCheckSum(&DirNew);

               pWork = (PDIRENTRY)CompactDir(pDirectory, pCD->ulClusterSize * 2, usEntriesNeeded);
               //printf("pszLongName(2)=%s\n", pszLongName);
               pWork = (PDIRENTRY)fSetLongName(pWork, pszLongName, bCheck);
               //printf("pszLongName(3)=%s\n", pszLongName);
               memcpy(pWork, &DirNew, sizeof (DIRENTRY));

               rc = WriteCluster(pCD, ulPrevCluster, (void *)pDirectory);
               if (rc)
                  {
                  free(pDirectory);
                  return rc;
                  }

               rc = WriteCluster(pCD, ulCluster, (void *)pDir2);
               if (rc)
                  {
                  free(pDirectory);
                  return rc;
                  }
               ulCluster = FAT_EOF;
               break;
               }

            usFreeEntries = GetFreeEntries(pDir2, pCD->ulClusterSize);
            if (usFreeEntries >= usEntriesNeeded)
               {
               BYTE bCheck = GetVFATCheckSum(&DirNew);

               //printf("pszLongName(4)=%s\n", pszLongName);
               pWork = (PDIRENTRY)CompactDir(pDir2, pCD->ulClusterSize, usEntriesNeeded);
               //printf("pszLongName(5)=%s\n", pszLongName);
               pWork = (PDIRENTRY)fSetLongName(pWork, pszLongName, bCheck); ////
               memcpy(pWork, &DirNew, sizeof (DIRENTRY));
               rc = WriteCluster(pCD, ulCluster, (void *)pDir2);
               if (rc)
                  {
                  free(pDirectory);
                  return rc;
                  }
               ulCluster = FAT_EOF;
               break;
               }
            else if (usFreeEntries > 0)
               {
               MarkFreeEntries(pDir2, pCD->ulClusterSize);
               rc = WriteCluster(pCD, ulCluster, (void *)pDir2);
               if (rc)
                  {
                  free(pDirectory);
                  return rc;
                  }
               }

            break;
         }

      if (ulCluster != FAT_EOF)
         {
         ulPrevCluster = ulCluster;
         memmove(pDirectory, pDir2, pCD->ulClusterSize);
         if (pLNStart)
            pLNStart = (PDIRENTRY)((PBYTE)pLNStart - pCD->ulClusterSize);


         ulNextCluster = GetNextCluster(pCD, ulCluster, TRUE);
         if (!ulNextCluster)
            ulNextCluster = FAT_EOF;
         if (ulNextCluster == FAT_EOF)
            {
            if (usMode == MODIFY_DIR_UPDATE ||
                usMode == MODIFY_DIR_DELETE ||
                usMode == MODIFY_DIR_RENAME)
               {
               free(pDirectory);
               return ERROR_FILE_NOT_FOUND;
               }

            ulNextCluster = SetNextCluster(pCD, ulCluster, FAT_ASSIGN_NEW);
            if (ulNextCluster == FAT_EOF)
               {
               free(pDirectory);
               return ERROR_DISK_FULL;
               }
            fNewCluster = TRUE;
            }
         ulCluster = ulNextCluster;
         }
      }

   free(pDirectory);
   return 0;
}

/************************************************************************
*
************************************************************************/
BOOL fGetLongName(PDIRENTRY pDir, PSZ pszName, USHORT wMax, PBYTE pbCheck)
{
   BYTE szLongName[30] = "";
   USHORT uniName[15] = {0};
   USHORT wNameSize;
   USHORT usIndex;
   PLNENTRY pLN = (PLNENTRY)pDir;

   memset(szLongName, 0, sizeof szLongName);
   memset(uniName, 0, sizeof uniName);

   wNameSize = 0;
   if (pLN->bVFATCheckSum != *pbCheck)
      {
      memset(pszName, 0, wMax);
      *pbCheck = pLN->bVFATCheckSum;
      }

   for (usIndex = 0; usIndex < 5; usIndex ++)
      {
      if (pLN->usChar1[usIndex] != 0xFFFF)
         uniName[wNameSize++] = pLN->usChar1[usIndex];
      }
   for (usIndex = 0; usIndex < 6; usIndex ++)
      {
      if (pLN->usChar2[usIndex] != 0xFFFF)
         uniName[wNameSize++] = pLN->usChar2[usIndex];
      }
   for (usIndex = 0; usIndex < 2; usIndex ++)
      {
      if (pLN->usChar3[usIndex] != 0xFFFF)
         uniName[wNameSize++] = pLN->usChar3[usIndex];
      }

   Translate2OS2(uniName, szLongName, sizeof szLongName);

   wNameSize = strlen( szLongName );
   if (strlen(pszName) + wNameSize > wMax)
      return FALSE;

   memmove(pszName + wNameSize, pszName, strlen(pszName) + 1);
   memcpy(pszName, szLongName, wNameSize);
   return TRUE;
}


/******************************************************************
*
******************************************************************/
BYTE GetVFATCheckSum(PDIRENTRY pDir)
{
   BYTE bCheck;
   INT  iIndex;

   bCheck = 0;
   for (iIndex = 0; iIndex < 11; iIndex++)
      {
      if (bCheck & 0x01)
         {
         bCheck >>=1;
         bCheck |= 0x80;
         }
      else
         bCheck >>=1;
      bCheck += pDir->bFileName[iIndex];
      }

   return bCheck;

}

/******************************************************************
*
******************************************************************/
APIRET MakeShortName(PCDINFO pCD, ULONG ulDirCluster, PSZ pszLongName, PSZ pszShortName)
{
   USHORT usLongName;
   PSZ pLastDot;
   PSZ pFirstDot;
   PSZ p;
   USHORT usIndex;
   BYTE szShortName[12];
   PSZ  pszUpper;
   APIRET rc;

   usLongName = LONGNAME_OFF;
   memset(szShortName, 0x20, 11);
   szShortName[11] = 0;

   /*
      Uppercase the longname
   */
   usIndex = strlen(pszLongName) + 5;
   pszUpper = malloc(usIndex);
   if (!pszUpper)
      return LONGNAME_ERROR;

   strupr(pszLongName); // !!! @todo DBCS/Unicode

   /* Skip all leading dots */
   p = pszUpper;
   while (*p == '.')
      p++;

   pLastDot  = strrchr(p, '.');
   pFirstDot = strchr(p, '.');

   if (!pLastDot)
      pLastDot = pszUpper + strlen(pszUpper);
   if (!pFirstDot)
      pFirstDot = pLastDot;

   /*
      Is the name a valid 8.3 name ?
   */
   if ((!strcmp(pszLongName, pszUpper)) &&
      pFirstDot == pLastDot &&
      pLastDot - pszUpper <= 8 &&
      strlen(pLastDot) <= 4)
      {
      PSZ p = pszUpper;

      if (*p != '.')
         {
         while (*p)
            {
            if (*p < 128 && ! IsCharValid(*p) && *p != '.')
               break;
            p++;
            }
         }

      if (!(*p))
         {
         memset(szShortName, 0x20, sizeof szShortName);
         szShortName[11] = 0;
         memcpy(szShortName, pszUpper, pLastDot - pszUpper);
         if (*pLastDot)
            memcpy(szShortName + 8, pLastDot + 1, strlen(pLastDot + 1));

         memcpy(pszShortName, szShortName, 11);
         free(pszUpper);
         return usLongName;
         }
      }

   usLongName = LONGNAME_OK;

   if (pLastDot - pszUpper > 8)
      usLongName = LONGNAME_MAKE_UNIQUE;

   szShortName[11] = 0;


   usIndex = 0;
   p = pszUpper;
   while (usIndex < 11)
      {
      if (!(*p))
         break;

      if (usIndex == 8 && p <= pLastDot)
         {
         if (p < pLastDot)
            usLongName = LONGNAME_MAKE_UNIQUE;
         if (*pLastDot)
            p = pLastDot + 1;
         else
            break;
         }

      while (*p == 0x20)
         {
         usLongName = LONGNAME_MAKE_UNIQUE;
         p++;
         }
      if (!(*p))
         break;

      if (*p >= 128)
         {
         szShortName[usIndex++] = *p;
         }
      else if (*p == '.')
         {
         /*
            Skip all dots, if multiple dots are in the
            name create an unique name
         */
         if (p == pLastDot)
            usIndex = 8;
         else
            usLongName = LONGNAME_MAKE_UNIQUE;
         }
      else if (IsCharValid(*p))
         szShortName[usIndex++] = *p;
      else
         {
         szShortName[usIndex++] = '_';
         }
      p++;
      }
   if (strlen(p))
      usLongName = LONGNAME_MAKE_UNIQUE;

   free(pszUpper);

   p = szShortName;
   for( usIndex = 0; usIndex < 8; usIndex++ )
      if( IsDBCSLead( p[ usIndex ]))
         usIndex++;

   if( usIndex > 8 )
      p[ 7 ] = 0x20;

   p = szShortName + 8;
   for( usIndex = 0; usIndex < 3; usIndex++ )
      if( IsDBCSLead( p[ usIndex ]))
         usIndex++;

   if( usIndex > 3 )
      p[ 2 ] = 0x20;

   if (usLongName == LONGNAME_MAKE_UNIQUE)
      {
      USHORT usNum;
      BYTE   szFileName[25];
      BYTE   szNumber[18];
      ULONG ulCluster;
      PSZ p;
      USHORT usPos1, usPos2;

      for (usPos1 = 8; usPos1 > 0; usPos1--)
         if (szShortName[usPos1 - 1] != 0x20)
            break;

      for (usNum = 1; usNum < 32000; usNum++)
         {
         memset(szFileName, 0, sizeof szFileName);
         memcpy(szFileName, szShortName, 8);

         /*
            Find last blank in filename before dot.
         */

         memset(szNumber, 0, sizeof szNumber);
         itoa(usNum, szNumber, 10);

         usPos2 = 7 - (strlen(szNumber));
         if (usPos1 && usPos1 < usPos2)
            usPos2 = usPos1;

         for( usIndex = 0; usIndex < usPos2; usIndex++ )
            if( IsDBCSLead( szShortName[ usIndex ]))
               usIndex++;

         if( usIndex > usPos2 )
            usPos2--;

         strcpy(szFileName + usPos2, "~");
         strcat(szFileName, szNumber);

         if (memcmp(szShortName + 8, "   ", 3))
            {
            strcat(szFileName, ".");
            memcpy(szFileName + strlen(szFileName), szShortName + 8, 3);
            p = szFileName + strlen(szFileName);
            while (p > szFileName && *(p-1) == 0x20)
               p--;
            *p = 0;
            }
         ulCluster = FindPathCluster(pCD, ulDirCluster, szFileName, NULL, NULL);
         if (ulCluster == FAT_EOF)
            break;
         }
      if (usNum < 32000)
         {
         p = strchr(szFileName, '.');
#if 0
         if (p && p - szFileName < 8)
            memcpy(szShortName, szFileName, p - szFileName);
         else
            memccpy(szShortName, szFileName, 0, 8 );
         }
#else
         if( !p )
            p = szFileName + strlen( szFileName );

         memcpy(szShortName, szFileName, p - szFileName);
         memset( szShortName + ( p - szFileName ), 0x20, 8 - ( p - szFileName ));
#endif
         }
      else
         return LONGNAME_ERROR;
      }
   memcpy(pszShortName, szShortName, 11);
   return usLongName;
}

/******************************************************************
*
******************************************************************/
PDIRENTRY fSetLongName(PDIRENTRY pDir, PSZ pszLongName, BYTE bCheck)
{
   USHORT usNeededEntries;
   PDIRENTRY pRet;
   BYTE bCurEntry;
   PLNENTRY pLN;
   USHORT usIndex;
   USHORT uniName[13];
   USHORT uniEnd[13];
   PUSHORT p;

   //printf("pszLongName(6)=%s\n", pszLongName);

   if (!pszLongName || !strlen(pszLongName))
      return pDir;

#if 0
   usNeededEntries = strlen(pszLongName) / 13 +
      (strlen(pszLongName) % 13 ? 1 : 0);
#else
   usNeededEntries = ( DBCSStrlen( pszLongName ) + 12 ) / 13;
#endif

   if (!usNeededEntries)
      return pDir;

   pDir += (usNeededEntries - 1);
   pRet = pDir + 1;
   pLN = (PLNENTRY)pDir;

   bCurEntry = 1;
   //printf("pszLongName(7)=%s\n", pszLongName);
   while (*pszLongName)
      {
#if 0
      USHORT usLen;
#endif
      pLN->bNumber = bCurEntry;
      if (DBCSStrlen(pszLongName) <= 13)
         pLN->bNumber += 0x40;
      pLN->wCluster = 0L;
      pLN->bAttr = FILE_LONGNAME;
      pLN->bReserved = 0;
      pLN->bVFATCheckSum = bCheck;

#if 0
      usLen = strlen(pszLongName);
      if (usLen > 13)
         usLen = 13;
#endif

      memset(uniEnd, 0xFF, sizeof uniEnd);
      memset(uniName, 0, sizeof uniName);

      //printf("pszLongName(8)=%s\n", pszLongName);
      pszLongName += Translate2Win(pszLongName, uniName, 13);
      //printf("pszLongName(9)=%s\n", pszLongName);

      p = uniName;
      for (usIndex = 0; usIndex < 5; usIndex ++)
         {
         pLN->usChar1[usIndex] = *p;
         if (*p == 0)
            p = uniEnd;
         p++;
         }

      for (usIndex = 0; usIndex < 6; usIndex ++)
         {
         pLN->usChar2[usIndex] = *p;
         if (*p == 0)
            p = uniEnd;
         p++;
         }

      for (usIndex = 0; usIndex < 2; usIndex ++)
         {
         pLN->usChar3[usIndex] = *p;
         if (*p == 0)
            p = uniEnd;
         p++;
         }

      pLN--;
      bCurEntry++;
      }

   return pRet;
}

/******************************************************************
*
******************************************************************/
USHORT DBCSStrlen( const PSZ pszStr )
{
   USHORT usLen;
   USHORT usIndex;
   USHORT usRet;

   usLen = strlen( pszStr );
   usRet = 0;
   for( usIndex = 0; usIndex < usLen; usIndex++ )
      {
         if( IsDBCSLead( pszStr[ usIndex ]))
            usIndex++;

         usRet++;
      }

   return usRet;
}


/******************************************************************
*
******************************************************************/
APIRET SetNextCluster(PCDINFO pCD, ULONG ulCluster, ULONG ulNext)
{
   PULONG pulCluster;
   BOOL fUpdateFSInfo;
   ULONG ulReturn;
   APIRET rc;

   ulReturn = ulNext;
   if (ulCluster == FAT_ASSIGN_NEW)
      {
      /*
         A new seperate CHAIN is started.
      */
      ulCluster = GetFreeCluster(pCD);
      if (ulCluster == FAT_EOF)
         return FAT_EOF;
      ulReturn = ulCluster;
      ulNext = FAT_EOF;
      }

   else if (ulNext == FAT_ASSIGN_NEW)
      {
      /*
         An existing chain is extended
      */
      ulNext = SetNextCluster(pCD, FAT_ASSIGN_NEW, FAT_EOF);
      if (ulNext == FAT_EOF)
         return FAT_EOF;
      ulReturn = ulNext;
      }

   if (ReadFatSector(pCD, ulCluster / 128))
      return FAT_EOF;

   fUpdateFSInfo = FALSE;
   pulCluster = (PULONG)pCD->pbFATSector + (ulCluster % 128);
   if (*pulCluster && !ulNext)
      {
      fUpdateFSInfo = TRUE;
      pCD->FSInfo.ulFreeClusters++;
      }
   if (*pulCluster == 0 && ulNext)
      {
      fUpdateFSInfo = TRUE;
      pCD->FSInfo.ulNextFreeCluster = ulCluster;
      pCD->FSInfo.ulFreeClusters--;
      }

   *pulCluster = ulNext;

   rc = WriteFatSector(pCD, ulCluster / 128);
   if (rc)
      return FAT_EOF;

   if (fUpdateFSInfo)
      UpdateFSInfo(pCD);

   return ulReturn;
}

/******************************************************************
*
******************************************************************/
VOID MakeName(PDIRENTRY pDir, PSZ pszName, USHORT usMax)
{
   PSZ p;
   BYTE szExtention[4];

   memset(pszName, 0, usMax);
   strncpy(pszName, pDir->bFileName, 8);
   p = pszName + strlen(pszName);
   while (p > pszName && *(p-1) == 0x20)
      p--;
   *p = 0;

   memset(szExtention, 0, sizeof szExtention);
   strncpy(szExtention, pDir->bExtention, 3);
   p = szExtention + strlen(szExtention);
   while (p > szExtention && *(p-1) == 0x20)
      p--;
   *p = 0;
   if (strlen(szExtention))
      {
      strcat(pszName, ".");
      strcat(pszName, szExtention);
      }
}

/******************************************************************
*
******************************************************************/
USHORT GetFreeEntries(PDIRENTRY pDirBlock, ULONG ulSize)
{
   USHORT usCount;
   PDIRENTRY pMax;
   BOOL bLoop;

   pMax = (PDIRENTRY)((PBYTE)pDirBlock + ulSize);
   usCount = 0;
   bLoop = pMax == pDirBlock;
   while (( pDirBlock != pMax ) || bLoop )
      {
      if (!pDirBlock->bFileName[0] || pDirBlock->bFileName[0] == DELETED_ENTRY)
         usCount++;
      bLoop = FALSE;
      pDirBlock++;
      }

   return usCount;
}

/******************************************************************
*
******************************************************************/
PDIRENTRY CompactDir(PDIRENTRY pStart, ULONG ulSize, USHORT usEntriesNeeded)
{
   PDIRENTRY pTar, pMax, pFirstFree;
   USHORT usFreeEntries;
   BOOL bLoop;

   pMax = (PDIRENTRY)((PBYTE)pStart + ulSize);
   bLoop = pMax == pStart;
   pFirstFree = pMax;
   usFreeEntries = 0;
   while (( pFirstFree != pStart ) || bLoop )
      {
      if (!(pFirstFree-1)->bFileName[0])
         usFreeEntries++;
      else
         break;
      bLoop = FALSE;
      pFirstFree--;
      }

   if ((( pFirstFree == pStart ) && !bLoop ) || (pFirstFree - 1)->bAttr != FILE_LONGNAME)
      if (usFreeEntries >= usEntriesNeeded)
         return pFirstFree;

   /*
      Leaving longname entries at the end
   */
   while ((( pFirstFree != pStart ) || bLoop ) && (pFirstFree - 1)->bAttr == FILE_LONGNAME)
   {
      bLoop = FALSE;
      pFirstFree--;
   }

   usFreeEntries = 0;
   pTar = pStart;
   while ((( pStart != pFirstFree ) || bLoop ) && usFreeEntries < usEntriesNeeded)
      {
      if (pStart->bFileName[0] && pStart->bFileName[0] != DELETED_ENTRY)
         {
         if (pTar != pStart)
            *pTar = *pStart;
         pTar++;
         }
      else
         usFreeEntries++;

      bLoop = FALSE;
      pStart++;
      }
   if (pTar != pStart)
      {
#if 1
      USHORT usEntries = 0;
      PDIRENTRY p;

      for( p = pStart; ( p != pFirstFree ) /*|| bLoop */; p++ )
        {
            /*bLoop = FALSE;*/
            usEntries++;
        }
      memmove(pTar, pStart, usEntries * sizeof (DIRENTRY));
#else
      memmove(pTar, pStart, (pFirstFree - pStart) * sizeof (DIRENTRY));
#endif
      pFirstFree -= usFreeEntries;
      memset(pFirstFree, DELETED_ENTRY, usFreeEntries * sizeof (DIRENTRY));
      }

   return pFirstFree;
}

/******************************************************************
*
******************************************************************/
VOID MarkFreeEntries(PDIRENTRY pDirBlock, ULONG ulSize)
{
   PDIRENTRY pMax;

   pMax = (PDIRENTRY)((PBYTE)pDirBlock + ulSize);
   while (pDirBlock != pMax)
      {
      if (!pDirBlock->bFileName[0])
         pDirBlock->bFileName[0] = DELETED_ENTRY;
      pDirBlock++;
      }
}

/******************************************************************
*
******************************************************************/
ULONG GetFreeCluster(PCDINFO pCD)
{
   ULONG ulStartCluster;
   ULONG ulCluster;
   BOOL fStartAt2;

   if (pCD->FSInfo.ulFreeClusters == 0L)
      return FAT_EOF;

   fStartAt2 = FALSE;
   ulCluster = pCD->FSInfo.ulNextFreeCluster + 1;
   if (!ulCluster || ulCluster >= pCD->ulTotalClusters + 2)
      {
      fStartAt2 = TRUE;
      ulCluster = 2;
      ulStartCluster = pCD->ulTotalClusters + 2;
      }
   else
      ulStartCluster = ulCluster;

   while (GetNextCluster(pCD, ulCluster, TRUE))
      {
      ulCluster++;
      if (fStartAt2 && ulCluster >= ulStartCluster)
         return FAT_EOF;

      if (ulCluster >= pCD->ulTotalClusters + 2)
         {
         if (!fStartAt2)
            {
            ulCluster = 2;
            fStartAt2 = TRUE;
            }
         else
            return FAT_EOF;
         }
      }
   return ulCluster;
}

/******************************************************************
*
******************************************************************/
APIRET SetFileSize(PCDINFO pCD, PFILESIZEDATA pFileSize)
{
   ULONG ulDirCluster;
   PSZ   pszFile;
   ULONG ulCluster;
   DIRENTRY DirEntry;
   DIRENTRY DirNew;
   ULONG ulClustersNeeded;
   ULONG ulClustersUsed;
   APIRET rc;

   ulDirCluster = FindDirCluster(pCD,
      pFileSize->szFileName,
      0xFFFF,
      RETURN_PARENT_DIR,
      &pszFile);

   if (ulDirCluster == FAT_EOF)
      return ERROR_PATH_NOT_FOUND;

   ulCluster = FindPathCluster(pCD, ulDirCluster, pszFile, &DirEntry, NULL);
   if (ulCluster == FAT_EOF)
      return ERROR_FILE_NOT_FOUND;
   if (!ulCluster)
      pFileSize->ulFileSize = 0L;

   ulClustersNeeded = pFileSize->ulFileSize / pCD->ulClusterSize;
   if (pFileSize->ulFileSize % pCD->ulClusterSize)
      ulClustersNeeded++;

   if (pFileSize->ulFileSize > 0 )
      {
      ulClustersUsed = 1;
      while (ulClustersUsed < ulClustersNeeded)
         {
         ULONG ulNextCluster = GetNextCluster(pCD, ulCluster, TRUE);
         if (!ulNextCluster)
            break;
         ulCluster = ulNextCluster;
         if (ulCluster == FAT_EOF)
            break;
         ulClustersUsed++;
         }
      if (ulCluster == FAT_EOF)
         pFileSize->ulFileSize = ulClustersUsed * pCD->ulClusterSize;
      else
         SetNextCluster(pCD, ulCluster, FAT_EOF);
      }

   memcpy(&DirNew, &DirEntry, sizeof (DIRENTRY));
   DirNew.ulFileSize = pFileSize->ulFileSize;

   if (!pFileSize->ulFileSize)
      {
      DirNew.wCluster = 0;
      DirNew.wClusterHigh = 0;
      }


   rc = ModifyDirectory(pCD, ulDirCluster, MODIFY_DIR_UPDATE,
      &DirEntry, &DirNew, NULL);

   return rc;
}

/******************************************************************
*
******************************************************************/
USHORT RecoverChain2(PCDINFO pCD, ULONG ulCluster, PBYTE pData, USHORT cbData)
{
   DIRENTRY DirEntry;
   BYTE     szFileName[14];
   USHORT   usNr;

   memset(&DirEntry, 0, sizeof (DIRENTRY));

   memcpy(DirEntry.bFileName, "FILE0000CHK", 11);
   strcpy(szFileName, "FILE0000.CHK");
   for (usNr = 0; usNr < 9999; usNr++)
      {
      USHORT iPos = 7;
      USHORT usNum = usNr;

         while (usNum)
            {
            szFileName[iPos] = (BYTE)((usNum % 10) + '0');
            usNum /= 10;
            iPos--;
            }
         if (FindPathCluster(pCD, pCD->BootSect.bpb.RootDirStrtClus,
            szFileName, NULL, NULL) == FAT_EOF)
            break;
      }
   if (usNr == 9999)
      return ERROR_FILE_EXISTS;
   memcpy(DirEntry.bFileName, szFileName, 8);
   if (pData)
      strncpy(pData, szFileName, cbData);

   set_datetime(&DirEntry);

   DirEntry.wCluster = LOUSHORT(ulCluster);
   DirEntry.wClusterHigh = HIUSHORT(ulCluster);
   while (ulCluster != FAT_EOF)
      {
      ULONG ulNextCluster;
      DirEntry.ulFileSize += pCD->ulClusterSize;
      ulNextCluster = GetNextCluster(pCD, ulCluster, TRUE);
      if (!ulNextCluster)
         {
         SetNextCluster(pCD, ulCluster, FAT_EOF);
         ulCluster = FAT_EOF;
         }
      else
         ulCluster = ulNextCluster;
      }

   return MakeDirEntry(pCD,
      pCD->BootSect.bpb.RootDirStrtClus,
      &DirEntry, szFileName);
}

/******************************************************************
*
******************************************************************/
USHORT MakeDirEntry(PCDINFO pCD, ULONG ulDirCluster, PDIRENTRY pNew, PSZ pszName)
{
   set_datetime(pNew);

   return ModifyDirectory(pCD, ulDirCluster, MODIFY_DIR_INSERT,
      NULL, pNew, pszName);
}

/******************************************************************
*
******************************************************************/
BOOL DeleteFatChain(PCDINFO pCD, ULONG ulCluster)
{
   ULONG ulNextCluster;
   ULONG ulSector;
   PULONG pulCluster;
   ULONG ulClustersFreed;
   APIRET rc;

   if (!ulCluster)
      return TRUE;

   ulSector = ulCluster / 128;
   ReadFatSector(pCD, ulSector);
   ulClustersFreed = 0;
   while (!(ulCluster >= FAT_EOF2 && ulCluster <= FAT_EOF))
      {
#ifdef CALL_YIELD
      //Yield();
#endif

      if (!ulCluster || ulCluster == FAT_BAD_CLUSTER)
         {
         break;
         }
      ulSector = ulCluster / 128;
      if (ulSector != pCD->ulCurFATSector)
         {
         rc = WriteFatSector(pCD, pCD->ulCurFATSector);
         if (rc)
            return FALSE;
         ReadFatSector(pCD, ulSector);
         }
      pulCluster = (PULONG)pCD->pbFATSector + (ulCluster % 128);

      ulNextCluster = *pulCluster;
      *pulCluster = 0L;
      ulClustersFreed++;
      ulCluster = ulNextCluster;
      }
   rc = WriteFatSector(pCD, pCD->ulCurFATSector);
   if (rc)
      return FALSE;

   pCD->FSInfo.ulFreeClusters += ulClustersFreed;
   UpdateFSInfo(pCD);

   return TRUE;
}


/******************************************************************
*
******************************************************************/
BOOL UpdateFSInfo(PCDINFO pCD)
{
   static BYTE bSector[SECTOR_SIZE] = "";

   if (pCD->BootSect.bpb.FSinfoSec == 0xFFFF)
      return TRUE;

   if (!ReadSector(pCD, pCD->BootSect.bpb.FSinfoSec, 1, bSector))
      {
      memcpy(bSector + FSINFO_OFFSET, (void *)&pCD->FSInfo, sizeof (BOOTFSINFO));
      if (!WriteSector(pCD, pCD->BootSect.bpb.FSinfoSec, 1, bSector))
         return TRUE;
      }

   return FALSE;
}

/******************************************************************
*
******************************************************************/
ULONG MakeFatChain(PCDINFO pCD, ULONG ulPrevCluster, ULONG ulClustersRequested, PULONG pulLast)
{
ULONG  ulCluster;
ULONG  ulFirstCluster;
ULONG  ulStartCluster;
ULONG  ulLargestChain;
ULONG  ulLargestSize;
ULONG  ulReturn;
ULONG  ulSector;
PULONG pulCluster;
BOOL   fStartAt2;
BOOL   fContiguous;

   if (!ulClustersRequested)
      return FAT_EOF;

   if (pCD->FSInfo.ulFreeClusters < ulClustersRequested)
      {
      return FAT_EOF;
      }

   ulReturn = FAT_EOF;
   fContiguous = TRUE;
   for (;;)
      {
      ulLargestChain = FAT_EOF;
      ulLargestSize = 0;

      ulFirstCluster = pCD->FSInfo.ulNextFreeCluster + 1;
      if (ulFirstCluster < 2 || ulFirstCluster >= pCD->ulTotalClusters + 2)
         {
         fStartAt2 = TRUE;
         ulFirstCluster = 2;
         ulStartCluster = pCD->ulTotalClusters + 3;
         }
      else
         {
         ulStartCluster = ulFirstCluster;
         fStartAt2 = FALSE;
         }

      for (;;)
         {
#ifdef CALL_YIELD
         //Yield();
#endif
         /*
            Find first free cluster
         */
         while (ulFirstCluster < pCD->ulTotalClusters + 2)
            {
            ulSector = ulFirstCluster / 128;
            pulCluster = (PULONG)pCD->pbFATSector + (ulFirstCluster % 128);
            if (ulSector != pCD->ulCurFATSector)
               ReadFatSector(pCD, ulSector);
            if (!(*pulCluster))
               break;
            ulFirstCluster++;
            }

         if (fStartAt2 && ulFirstCluster >= ulStartCluster)
            break;

         if (ulFirstCluster >= pCD->ulTotalClusters + 2)
            {
            if (fStartAt2)
               break;
            ulFirstCluster = 2;
            fStartAt2 = TRUE;
            continue;
            }


         /*
            Check if chain is long enough
         */

         for (ulCluster = ulFirstCluster ;
                  ulCluster < ulFirstCluster + ulClustersRequested &&
                  ulCluster < pCD->ulTotalClusters + 2;
                        ulCluster++)
            {
            ulSector = ulCluster / 128;
            pulCluster = (PULONG)pCD->pbFATSector + (ulCluster % 128);
            if (ulSector != pCD->ulCurFATSector)
               ReadFatSector(pCD, ulSector);
            if (*pulCluster)
               break;
            }

         if (ulCluster != ulFirstCluster + ulClustersRequested)
            {
            /*
               Keep the largests chain found
            */
            if (ulCluster - ulFirstCluster > ulLargestSize)
               {
               ulLargestChain = ulFirstCluster;
               ulLargestSize  = ulCluster - ulFirstCluster;
               }
            ulFirstCluster = ulCluster;
            continue;
            }

         /*
            Chain found long enough
         */
         if (ulReturn == FAT_EOF)
            ulReturn = ulFirstCluster;

         if (MakeChain(pCD, ulFirstCluster, ulClustersRequested))
            goto MakeFatChain_Error;

         if (ulPrevCluster != FAT_EOF)
            {
            if (SetNextCluster(pCD, ulPrevCluster, ulFirstCluster) == FAT_EOF)
               goto MakeFatChain_Error;
            }

         if (pulLast)
            *pulLast = ulFirstCluster + ulClustersRequested - 1;
         return ulReturn;
         }

      /*
         We get here only if no free chain long enough was found!
      */
      fContiguous = FALSE;

      if (ulLargestChain != FAT_EOF)
         {
         ulFirstCluster = ulLargestChain;
         if (ulReturn == FAT_EOF)
            ulReturn = ulFirstCluster;

         if (MakeChain(pCD, ulFirstCluster, ulLargestSize))
            goto MakeFatChain_Error;

         if (ulPrevCluster != FAT_EOF)
            {
            if (SetNextCluster(pCD, ulPrevCluster, ulFirstCluster) == FAT_EOF)
               goto MakeFatChain_Error;
            }

         ulPrevCluster        = ulFirstCluster + ulLargestSize - 1;
         ulClustersRequested -= ulLargestSize;
         }
      else
         break;
      }

MakeFatChain_Error:

   if (ulReturn != FAT_EOF)
      DeleteFatChain(pCD, ulReturn);

   return FAT_EOF;
}

/******************************************************************
*
******************************************************************/
USHORT MakeChain(PCDINFO pCD, ULONG ulFirstCluster, ULONG ulSize)
{
ULONG ulSector;
ULONG ulLastCluster;
PULONG pulCluster;
ULONG  ulCluster;
USHORT rc;

   ulLastCluster = ulFirstCluster + ulSize - 1;

   ulSector = ulFirstCluster / 128;
   if (ulSector != pCD->ulCurFATSector)
      ReadFatSector(pCD, ulSector);

   for (ulCluster = ulFirstCluster; ulCluster < ulLastCluster; ulCluster++)
      {
      ulSector = ulCluster / 128;
      if (ulSector != pCD->ulCurFATSector)
         {
         rc = WriteFatSector(pCD, pCD->ulCurFATSector);
         if (rc)
            return rc;
         ReadFatSector(pCD, ulSector);
         }
      pulCluster = (PULONG)pCD->pbFATSector + (ulCluster % 128);
      if (*pulCluster)
         {
         return ERROR_SECTOR_NOT_FOUND;
         }
      *pulCluster = ulCluster + 1;
      }

   ulSector = ulCluster / 128;
   if (ulSector != pCD->ulCurFATSector)
      {
      rc = WriteFatSector(pCD, pCD->ulCurFATSector);
      if (rc)
         return rc;
      ReadFatSector(pCD, ulSector);
      }
   pulCluster = (PULONG)pCD->pbFATSector + (ulCluster % 128);
   if (*pulCluster)
      {
      return ERROR_SECTOR_NOT_FOUND;
      }

   *pulCluster = FAT_EOF;
   rc = WriteFatSector(pCD, pCD->ulCurFATSector);
   if (rc)
      return rc;

   pCD->FSInfo.ulNextFreeCluster = ulCluster;
   pCD->FSInfo.ulFreeClusters   -= ulSize;

   return 0;
}

/******************************************************************
*
******************************************************************/
APIRET MakeFile(PCDINFO pCD, ULONG ulDirCluster, PSZ pszFile, PBYTE pBuf, ULONG cbBuf)
{
   ULONG ulClustersNeeded;
   ULONG ulCluster;
   DIRENTRY OldEntry, NewEntry;
   APIRET rc;
   char file_exists = 0;
   int i;

   if (cbBuf)
      {
      ulClustersNeeded = cbBuf / pCD->ulClusterSize +
         (cbBuf % pCD->ulClusterSize ? 1 : 0);

      ulCluster = FindPathCluster(pCD, ulDirCluster, pszFile, &OldEntry, NULL);

      if (ulCluster != FAT_EOF)
         {
         file_exists = 1;
         memcpy(&NewEntry, &OldEntry, sizeof(DIRENTRY));
         DeleteFatChain(pCD, ulCluster);
         }
      else
         memset(&NewEntry, 0, sizeof(DIRENTRY));

      ulCluster = MakeFatChain(pCD, FAT_EOF, ulClustersNeeded, NULL);

      if (ulCluster != FAT_EOF)
         {
            NewEntry.wCluster = LOUSHORT(ulCluster);
            NewEntry.wClusterHigh = HIUSHORT(ulCluster);
            NewEntry.ulFileSize = cbBuf;

            set_datetime(&NewEntry);
         }
      else
         {
            rc = ERROR_DISK_FULL;
            goto MakeFileEnd;
         }
      }

   if (! file_exists)
      rc = MakeDirEntry(pCD, ulDirCluster, &NewEntry, pszFile);
   else
      rc = ModifyDirectory(pCD, ulDirCluster, MODIFY_DIR_UPDATE, &OldEntry, &NewEntry, pszFile);

   if (rc)
      goto MakeFileEnd;

   for (i = 0; i < ulClustersNeeded; i++)
      {
      rc = WriteCluster(pCD, ulCluster, pBuf);

      if (rc)
         goto MakeFileEnd;

      pBuf += pCD->ulClusterSize;
      ulCluster = GetNextCluster(pCD, ulCluster, TRUE);
      }

      UpdateFSInfo(pCD);

MakeFileEnd:
   return rc;
}
