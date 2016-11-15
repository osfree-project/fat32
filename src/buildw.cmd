rem
rem Build with OpenWatcom tools
rem
setlocal
@echo off
set d=p
SET DEBUG=0
set WATCOM=f:\dev\watcom
set TOOLKIT=f:\os2tk45
set PATH=%toolkit%\bin;%watcom%\binp;\tools\bin;%path%
set BEGINLIBPATH=%toolkit%\dll;%watcom%\binp\dll;\tools\dll
call %WATCOM%\owsetenv.cmd
call svnrev.cmd
call envwic.cmd
wmake -h -f makefile.wcc %1
endlocal
