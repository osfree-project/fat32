rem 
rem Build with MSC 6.0/VAC/MASM
rem
setlocal
@echo off
SET DEBUG=0
SET COMSPEC=D:\OS2\CMD.EXE
SET OS2_SHELL=D:\OS2\CMD.EXE
SET LANG=en_US
SET CPP_DBG_LANG=CPP
rem SET DDK=f:\ddk
rem SET DDKTOOLS=f:\ddktools
SET TOOLKIT=f:\os2tk45
SET CXXMAIN=F:\Dev\vac365
rem SET MSC=%ddktools%\toolkits\msc60
rem SET MASM=%ddktools%\toolkits\masm60
rem SET CXXMAIN=%ddk%\base\tools\OS2.386\LX.386\BIN\vacpp
REM SET INCLUDE=..\include;%CXXMAIN%\include;%CXXMAIN%\INCLUDE\OS2;%include%
rem SET LIB=..\lib;%CXXMAIN%\lib;%LIB%
rem SET BEGINLIBPATH=%toolkit%\dll;%CXXMAIN%\DLL;%CXXMAIN%\RUNTIME
rem SET PATH=%CXXMAIN%\BIN;%MASM%\BINP;%PATH%
rem SET DPATH=%CXXMAIN%\msg;%CXXMAIN%\HELP;%CXXMAIN%\runtime;%CXXMAIN%\LOCALE;%CXXMAIN%;%dpath%
rem SET NLSPATHTEMP=%toolkit%\msg\%N;%CXXMAIN%\locale\%%N;%CXXMAIN%\msg;%nlspath%
rem SET NLSPATH=%NLSPATHTEMP%
rem SET NLSPATHTEMP=
call %CXXMAIN%\bin\setenv.cmd
rem call %MSC%\binp\setenv.cmd
call svnrev.cmd
nmake -f makefile.icc %1 %2 %3 %4 %5 %6 %7 %8 %9
endlocal
