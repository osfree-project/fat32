rem 
rem Build with MSC 6.0/VAC/MASM
rem
setlocal
SET DEBUG=0
SET COMSPEC=D:\OS2\CMD.EXE
SET OS2_SHELL=D:\OS2\CMD.EXE
SET LANG=en_US
rem SET DDK=f:\ddk
rem SET DDKTOOLS=f:\ddktools
rem SET TOOLKIT=f:\os2tk45
rem SET MSC=%ddktools%\toolkits\msc60
rem SET MASM=%ddktools%\toolkits\masm60
rem SET IBMC=%ddk%\base\tools\OS2.386\LX.386\BIN\vacpp
SET IBMC=f:\dev\vac365
rem SET INCLUDE=..\include;%include%
rem SET LIB=..\lib;%lib%
SET INCLUDE=..\include;%ibmc%\include;%include%
SET LIB=..\lib;%ibmc%\lib;%LIB%
SET HELP=%CXXMAIN%\HELP;%HELP%
SET IPFC=%CXXMAIN%\BIN;%IPFC%
SET CPP_DBG_LANG=CPP
SET BEGINLIBPATH=%ibmc%\DLL;%ibmc%\runtime;
SET PATH=%ibmc%\BIN;%PATH%
SET DPATH=%ibmc%\HELP;%ibmc%\locale;%ibmc%\msg;%DPATH%
SET NLSPATHTEMP=%ibmc%\locale\%%N;%ibmc%\msg\%%N
SET NLSPATH=%nlspathtemp%;%nlspath%
rem SET INCLUDE=%ibmc%\INCLUDE;%ibmc%\INCLUDE\OS2;%INCLUDE%
rem call %IBMC%\bin\setenv.cmd
rem call %MSC%\binp\setenv.cmd
nmake -f makefile.icc
endlocal