rem 
rem Build with MSC 6.0/ALP
rem
setlocal
@echo off
SET DEBUG=0
SET BOOTDRIVE=c:
SET COMSPEC=%BOOTDRIVE%\OS2\CMD.EXE
SET OS2_SHELL=%BOOTDRIVE%\OS2\CMD.EXE
SET DDK=c:\ddk
SET TOOLKIT=f:\os2tk45
SET INCLUDE=..\include;%DDK%\base\h
SET LIB=..\lib;%ddk%\base\lib;
SET PATH=%ddk%\base\tools;%PATH%
call svnrev.cmd
nmake -f makefile.msc %1 %2 %3 %4 %5 %6 %7 %8 %9
endlocal
