@echo off
@echo Building FAT32.IFS and UFAT32.DLL...
set SAVEINCLUDE=%INCLUDE%
set SAVEPATH=%PATH%
set PATH=%DDKTOOLS%\toolkits\msc60\binp;%SAVEPATH%
set INCLUDE=%DDKTOOLS%\toolkits\msc60\include;%DDK%\base\h
nmake DDK=%DDK% DDKTOOLS=%DDKTOOLS%
@echo Building 32 Bits helper programs
set PATH=%SAVEPATH%
set INCLUDE=%SAVEINCLUDE%
nmake /f makefile.32 IBMC=%IBMC%

