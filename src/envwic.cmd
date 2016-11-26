/*  wic.cmd:

    sets BEGINLIBPATH and PATH to the WarpIN directory.

    Put this on your PATH somewhere to be able to execute WIC from
    anywhere.

    (C) 2001 Ulrich M”ller.
*/

call RxFuncAdd 'SysLoadFuncs', 'RexxUtil', 'SysLoadFuncs'
call SysLoadFuncs

/* get WarpIN path from OS2.INI */
warpindir = SysIni("USER", "WarpIN", "Path")

if (warpindir = "ERROR:") then do
    say "envwic.cmd: The WarpIN path could not be found in OS2.INI. Terminating.";
    exit;
end;

/* this thing is null-terminated, so get rid of the null byte */
p = pos('0'x, warpindir);
if (p \= 0) then do
    warpindir = left(warpindir, p - 1);
end

/* compose new path */
newpath = warpindir||';'||value('PATH',,'OS2ENVIRONMENT')

'SET BEGINLIBPATH='warpindir';%BEGINLIBPATH%'
'SET PATH='||newpath';%PATH%'
