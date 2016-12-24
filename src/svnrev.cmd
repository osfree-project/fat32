/* setting the SVNREV env. variable to the current SVN revision. Optionally, writing to a file */
PARSE ARG outfile otherarg
'@svn.exe info | rxqueue'
DO WHILE queued() <> 0
    queueLine = lineIN( "QUEUE:" )
    IF LEFT(queueLine,8) = 'Revision' THEN
    DO
        PARSE VALUE queueLine WITH "Revision: " rev
        call value 'SVNREV', rev,  'OS2ENVIRONMENT'
        call value 'PROJSTR', left(date() time(),25)left(value('HOSTNAME',,'OS2ENVIRONMENT'),10), 'OS2ENVIRONMENT'
        if outfile <> '' THEN
        DO
            '@del ' || outfile || ' >nul 2>&1'
            rc = stream(outfile, 'c', 'open write')
            rc = lineout(outfile, "#define _SVNREV " || rev)
            rc = stream(outfile, 'c', 'close')
        END
        RETURN 0
    END
END /* do while queued() <> 0 */
SAY 'Revision line is not found'
RETURN 1
