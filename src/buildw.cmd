rem
rem Build with OpenWatcom tools
rem
setlocal
@echo off
set d=p
SET DEBUG=0
SET COMSPEC=d:\os2\cmd.exe
SET OS2_SHELL=d:\os2\cmd.exe
set WATCOM=f:\dev\watcom
set TOOLKIT=f:\os2tk45
set PATH=%toolkit%\bin;%watcom%\binp;\tools\bin;%path%
set BEGINLIBPATH=%toolkit%\dll;%watcom%\binp\dll;\tools\dll
set rexx_dll=rexc
set rexxapi_dll=rexcapi
call %WATCOM%\owsetenv.cmd >nul 2>&1
call svnrev.cmd
call envwic.cmd
wmake -h -f makefile.wcc %1 %2 %3 %4 %5 %6 %7 %8 %9
endlocal
