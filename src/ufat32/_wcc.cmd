rem
rem Build with OpenWatcom tools
rem
setlocal
set WATCOM=f:\dev\watcom
set TOOLKIT=f:\os2tk45
set PATH=%toolkit%\bin;%path%
set BEGINLIBPATH=%toolkit%\dll
call %WATCOM%\owsetenv.cmd
wmake -f makefile.wcc
endlocal
