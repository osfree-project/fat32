#! /bin/sh
#
# Setting the SVNREV env. variable to the current SVN revision.
# Optionally, writing to a file
#

outfile=$1

rev=`svn info | awk '/^Revision/ {print $2}'`

if [ ! -z "${rev}" ]; then
    h=`hostname`

    dt=`LANG=C; date '+%d %b %Y %T'`

    PROJSTR=`awk -v dt="$dt" -v h="$h" 'BEGIN {
        while (length(dt) < 25) {dt = dt " "}
        while (length(h) < 10) {h = h " "}
        printf "%s%s", dt, h
    }'`

    export PROJSTR="$PROJSTR" SVNREV="$rev"

    if [ ! -z "${outfile}" ]; then
        echo "#define _SVNREV ${rev}" >${outfile}
    fi

    # echo PROJSTR="${PROJSTR}"
    # echo SVNREV="${SVNREV}"
else
    echo "Revision line is not found"
fi
