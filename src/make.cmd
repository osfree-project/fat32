@echo off
@echo Building FAT32.IFS and UFAT32.DLL...
setlocal
set DDK=f:\lang\ddk
set DDKTOOLS=f:\lang\ddktools
set IBMC=f:\lang\ibmcpp
set SAVEINCLUDE=%INCLUDE%
set SAVEPATH=%PATH%
set PATH=%DDKTOOLS%\toolkits\msc60\binp;%SAVEPATH%
set INCLUDE=%DDKTOOLS%\toolkits\msc60\include;%DDK%\base\h
set CL=/B1c1l.exe /B2c2l.exe /B3c3l.exe
nmake DDK=%DDK% DDKTOOLS=%DDKTOOLS%
if errorlevel 1 goto exit
@echo Building 32 Bits helper programs
set PATH=%SAVEPATH%
set INCLUDE=%SAVEINCLUDE%
nmake /f makefile.32 IBMC=%IBMC%
:exit
endlocal
