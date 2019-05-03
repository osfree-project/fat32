#
# Build with OpenWatcom tools
#

OS=linux
NUL=/dev/null
CP=cp
RM="rm -rf"
SEP=/
DEBUG=0
WATCOM=/opt/watcom
PATH=$WATCOM/binp:$PATH
export OS NUL CP RM SEP DEBUG WATCOM PATH
. $WATCOM/owsetenv.sh >/dev/null 2>&1
. svnrev.sh
#. envwic.sh
wmake -h -f makefile.wcc $1 $2 $3 $4 $5 $6 $7 $8 $9
