rem 
rem Build with MSC 6.0/VAC/MASM
rem
setlocal
@echo off
SET DEBUG=0
SET BOOTDRIVE=d:
SET COMSPEC=%BOOTDRIVE%\OS2\CMD.EXE
SET OS2_SHELL=%BOOTDRIVE%\OS2\CMD.EXE
SET DDK=f:\ddk
SET DDKTOOLS=f:\ddktools
SET TOOLKIT=f:\os2tk45
SET MSC=%ddktools%\toolkits\msc60
SET MASM=%ddktools%\toolkits\masm60
rem SET IBMC=%ddk%\base\tools\OS2.386\LX.386\BIN\vacpp
SET INCLUDE=..\include;%include%
SET LIB=..\lib;%ddk%\base\lib;%LIB%
SET PATH=%MASM%\BINP;%PATH%
rem call %IBMC%\bin\csetenv.cmd
call %MSC%\binp\setenv.cmd
call svnrev.cmd
nmake -f makefile.msc %1 %2 %3 %4 %5 %6 %7 %8 %9
endlocal
