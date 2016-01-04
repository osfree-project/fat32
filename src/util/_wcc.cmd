rem
rem Build with OpenWatcom tools
rem
setlocal
set WATCOM=f:\dev\watcom
call %WATCOM%\owsetenv.cmd
wmake -f makefile.wcc
endlocal
