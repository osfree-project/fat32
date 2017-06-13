#define  INCL_BASE
#include <os2.h>

#include <string.h>
#include <malloc.h>
#include <stdio.h>
#include <conio.h>

int main(int argc, char **argv)
{
  char szObjName[260];
  PSZ pszModname  = "UFAT32";
  PSZ pszFunction = "CHKDSK";
  PSZ pszFunction2 = "LOADTRANSTBL";
  USHORT far pascal (*pfn)(int argc, char far * far *argv, char far * far *envp);
  USHORT far pascal (*pfn2)(BOOL fSilent, UCHAR ucSource);
  HMODULE hmod;
  SEL    datasel;
  char   far *env, far *cmd, far *p;
  char   far * far *Argv;
  USHORT rc;
  char far buf[512];
  int i, prgLen;

  // get data segment selector
  datasel = SELECTOROF(pszModname);

  rc = DosLoadModule(szObjName, sizeof(szObjName), pszModname, &hmod);

  if (rc)
    return rc;

  rc = DosGetProcAddr(hmod, pszFunction, (void far *)&pfn);

  if (rc)
    return rc;

  if (argc == 1)
  {
    // load unicode translate table
    rc = DosGetProcAddr(hmod, pszFunction2, (void far *)&pfn2);

    if (rc)
      return rc;

    (*pfn2)(TRUE, 2);
    return 0;
  }

  Argv = _fmalloc(argc * sizeof(char far *));

  cmd = MAKEP(datasel, argv[0]);

  _fstrcpy(buf, cmd);
  prgLen = _fstrlen(cmd) + 1;

  // skip program name
  while (*cmd++) ;

  // without this, no strings are output to screen by UFAT32.DLL
  cputs("\r");

  _fstrcpy(buf + prgLen, cmd);
  Argv[0] = buf;

  for (p = buf + prgLen, i = 1; i < argc; p++, i++)
  {
    while (*p && *p == ' ') p++;
    Argv[i] = p;
    while (*p && *p != ' ') p++;
    *p = '\0';
  }

  Argv[i] = NULL;

  //for (i = 0; Argv[i]; i++)
  //  printf("Argv[%d]=%s\n", i, Argv[i]);

  // execute CHKDSK
  (*pfn)(argc, Argv, NULL);

  _ffree(Argv);
  DosFreeModule(hmod);
  return 0;
}
