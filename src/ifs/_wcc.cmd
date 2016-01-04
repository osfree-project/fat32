rem
rem Build with OpenWatcom tools
rem
setlocal
set d=p
set WATCOM=p:\dev\watcom
set TOOLKIT=p:\os2tk45
set PATH=%toolkit%\bin;%watcom%\binp;\tools\bin;%path%
set BEGINLIBPATH=%toolkit%\dll;%watcom%\binp\dll;\tools\dll
call %WATCOM%\owsetenv.cmd
wmake -f makefile.wcc
endlocal
