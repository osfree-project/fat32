@echo off
@echo Building FAT32.IFS and UFAT32.DLL...
call c6
c:\msc\binp\nmake
@echo Building 32 Bits helper programs
call ibmc
c:\msc\binp\nmake /f makefile.32

