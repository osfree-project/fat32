#define INCL_DOSERRORS
#include <os2.h>

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>

int format(int argc, char *argv[], char *envp[]);
int chkdsk(int argc, char *argv[], char *envp[]);
int recover(int argc, char *argv[], char *envp[]);
int sys(int argc, char *argv[], char *envp[]);

void cleanup(void);

short _Far16 _Pascal _loadds CHKDSK(short argc, char *_Seg16 *_Seg16 argv, char *_Seg16 *_Seg16 envp)
{
   short Argc = argc;
   char  **Argv = NULL;
   char  **Envp = NULL;
   char  * _Seg16 * _Seg16 p;
   short rc;
   int   i, n;

   Argv = (char **)malloc((Argc + 1) * sizeof(char * _Seg16));

   for (i = 0, p = argv; i < argc; i++, p++)
       Argv[i] = (char *)(*p);

   Argv[argc] = NULL;

   for (n = 0, p = envp; p && *p; n++, p++) ;

   Envp = (char **)malloc((n + 1) * sizeof(char * _Seg16));

   for (i = 0, p = envp; i < n; i++, p++)
       Envp[i] = (char *)(*p);

   Envp[n] = NULL;
   
   rc = chkdsk(Argc, Argv, Envp);

   free(Envp); free(Argv);
   return rc;
}

short _Far16 _Pascal _loadds FORMAT(short argc, char *_Seg16 *_Seg16 argv, char *_Seg16 *_Seg16 envp)
{
   short Argc = argc;
   char  **Argv;
   char  **Envp;
   char  * _Seg16 * _Seg16 p;
   short rc;
   int   i, n;

   Argv = (char **)malloc(Argc * sizeof(char * _Seg16));

   for (i = 0, p = argv; i < argc; i++, p++)
       Argv[i] = (char *)(*p);

   for (n = 0, p = envp; p && *p; n++, p++) ;

   Envp = (char **)malloc(n * sizeof(char * _Seg16));

   for (i = 0, p = envp; i < n; i++, p++)
       Envp[i] = (char *)(*p);
   
   rc = format(Argc, Argv, Envp);

   free(Envp); free(Argv);

   return rc;
}

short _Far16 _Pascal _loadds RECOVER(short argc, char *_Seg16 *_Seg16 argv, char *_Seg16 *_Seg16 envp)
{
   short Argc = argc;
   char  **Argv;
   char  **Envp;
   char  * _Seg16 * _Seg16 p;
   short rc;
   int   i, n;

   Argv = (char **)malloc(Argc * sizeof(char * _Seg16));

   for (i = 0, p = argv; i < argc; i++, p++)
       Argv[i] = (char *)(*p);

   for (n = 0, p = envp; p && *p; n++, p++) ;

   Envp = (char **)malloc(n * sizeof(char * _Seg16));

   for (i = 0, p = envp; i < n; i++, p++)
       Envp[i] = (char *)(*p);
   
   rc = recover(Argc, Argv, Envp);

   free(Envp); free(Argv);

   return NO_ERROR;
}

short _Far16 _Pascal _loadds SYS(short argc, char *_Seg16 *_Seg16 argv, char *_Seg16 *_Seg16 envp)
{
   short Argc = argc;
   char  **Argv;
   char  **Envp;
   char  * _Seg16 * _Seg16 p;
   short rc;
   int   i, n;

   Argv = (char **)malloc(Argc * sizeof(char * _Seg16));

   for (i = 0, p = argv; i < argc; i++, p++)
       Argv[i] = (char *)(*p);

   for (n = 0, p = envp; p && *p; n++, p++) ;

   Envp = (char **)malloc(n * sizeof(char * _Seg16));

   for (i = 0, p = envp; i < n; i++, p++)
       Envp[i] = (char *)(*p);
   
   rc = sys(Argc, Argv, Envp);

   free(Envp); free(Argv);

   return NO_ERROR;
}

#define SIG_CTRLC        1
#define SIG_BROKENPIPE   2
#define SIG_KILLPROCESS  3
#define SIG_CTRLBREAK    4
#define SIG_PFLG_A       5
#define SIG_PFLG_B       6
#define SIG_PFLG_C       7

static void show_sig_string(int s)
{
    char *str = "";
    switch (s)
    {
        case SIG_CTRLC:  
          str="SIG_CTRLC";
          break;
        case SIG_CTRLBREAK: 
          str="SIG_CTRLBREAK";
          break;
        case SIG_KILLPROCESS:  
          str="SIG_KILLPROCESS";
          break;
        case SIG_BROKENPIPE:   
          str="SIG_BROKENPIPE";
          break;
        case SIG_PFLG_A:   
          str="SIG_PFLG_A";
          break;
        case SIG_PFLG_B:   
          str="SIG_PFLG_B";
          break;
        case SIG_PFLG_C:   
          str="SIG_PFLG_C";
          break;
        default:
          ;
    }
    printf("\n\nSignal: %d = %s\n", s, str);
}

// the 16-bit signal handler
void _Far16 _Pascal _loadds HANDLER(USHORT x, USHORT iSignal)
{
   show_sig_string(iSignal);

   // remount disk for changes to take effect
   cleanup();
   exit(1);
}

// 16-bit exception handler
int _Far16 _Pascal _loadds HANDLER2(void)
{
   printf("\n\nException has caught\n");

   // remount disk for changes to take effect
   cleanup();
   exit(1);

   return 0;
}
