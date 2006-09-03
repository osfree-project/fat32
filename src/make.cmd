@echo off
SETLOCAL
@echo Building FAT32.IFS and UFAT32.DLL...
set DDKTOOLS=d:\ddktools
set DDK=d:\ddk
SETLOCAL
set PATH=%DDKTOOLS%\toolkits\msc60\binp;%DDKTOOLS%\toolkits\masm60\binp;%PATH%
set INCLUDE=%DDKTOOLS%\toolkits\msc60\include;%DDK%\base\h;%DDK\base\inc
nmake /nologo /s /f makefile DDK=%DDK% DDKTOOLS=%DDKTOOLS% DEBUG=1
ENDLOCAL
set IBMC=d:\ibmcpp
@echo Building 32 Bits helper programs...
nmake /nologo /s /f makefile.32 IBMC=%IBMC% DEBUG=1
ENDLOCAL
