/* setting the SVNREV env. variable to the current SVN revision. Optionally, writing to a file */
call RxFuncAdd 'SysLoadFuncs', 'RexxUtil', 'SysLoadFuncs'
call SysLoadFuncs
PARSE ARG outfile otherarg
'@svn.exe info | rxqueue'
DO WHILE queued() <> 0
    queueLine = lineIN( "QUEUE:" )
    IF LEFT(queueLine,8) = 'Revision' THEN
    DO
        PARSE VALUE queueLine WITH "Revision: " rev
        call value 'SVNREV', rev, 'OS2ENVIRONMENT'
        call value 'DATE', date(), 'OS2ENVIRONMENT'
        if outfile <> '' THEN
        DO
            rc = SysFileDelete(outfile)
            rc = lineout(outfile, "db '."rev"', 0, '$', 0")
            rc = lineout(outfile, "_svn_revision dd "rev)
            rc = stream(outfile, 'c', 'close')
        END
        RETURN 0
    END
END /* do while queued() <> 0 */
SAY 'Revision line is not found'
RETURN 1
