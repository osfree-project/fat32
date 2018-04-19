_VENDOR=Netlabs
_VER=0.10
_VERSION=$(_VER).r$(%SVNREV)
FILEVER=@$#$(_VENDOR):$(_VERSION)$#@$#$#1$#$# $(%PROJSTR)::::0::@@

BINROOT  = $(ROOT)\bin
BLDROOT  = $(ROOT)\bld
LIBROOT  = $(BLDROOT)\lib
PROJ_BLD = $(BLDROOT)\$(PROJ)
BLDDIRS  = $(BINROOT) $(BLDROOT) $(LIBROOT) &
           $(BINROOT)\os2 $(BINROOT)\os2\boot $(BINROOT)\os2\dll &
           $(BINROOT)\os2\system $(BINROOT)\os2\system\trace &
           $(BINROOT)\os2\book $(BINROOT)\os2\docs $(BINROOT)\os2\docs\fat32 &
           $(BLDROOT)\util $(BLDROOT)\ifs $(BLDROOT)\ifs\libc $(BLDROOT)\partfilt $(BLDROOT)\ifsinf &
           $(BLDROOT)\ufat32 $(BLDROOT)\f32chk $(BLDROOT)\util\zlib $(BLDROOT)\util\qemu-img $(BLDROOT)\util\qemu-img\block &
           $(BLDROOT)\ufat32\win32 $(BINROOT)\win32 $(BINROOT)\win32\dll

CLEANUP  = $(PROJ_BLD)\*.obj $(PROJ_BLD)\*.obd $(PROJ_BLD)\*.obc $(PROJ_BLD)\*.lnk $(PROJ_BLD)\*.wmp &
           $(PROJ_BLD)\*.map $(PROJ_BLD)\*.ols $(PROJ_BLD)\*.err $(BLDROOT)\..\include\ver.h &
           $(BLDROOT)\lib\*.lib $(BINROOT)\zip-*.flg $(BINROOT)\wpi.flg
# compress executables
LXLITE   = 1
# enable exFAT support (change it to 0 to get a build without exFAT)
EXFAT    = 1

mainifs  = &
 fat32.ifs

dll      = &
 qemuimg.dll &
 uunifat.dll &
 ufat32.dll &
 ufat12.dll &
 ufat16.dll &
!ifeq EXFAT 1
 uexfat.dll
!else
 #
!endif

util     = &
 cachef32.cmd &
 cachef32.exe &
 f32parts.exe &
 f32mon.exe   &
 f32stat.exe  &
 f32chk.exe &
 f32mount.exe &
 fat32chk.exe &
 fat32fmt.exe &
 fat32sys.exe &
 qemu-img.exe

inf      = &
 fat32.inf

ifsinf   = &
 ifs.inf

docs     = &
 build.txt message.txt notes.txt &
 deamon.txt fat32.txt lesser.txt &
 license.txt os2fat32.txt problems.txt &

korean   = &
 system\country.kor docs\fat32\fat32.kor

adddrv   = &
 os2dasd.f32 partfilt.flt

sym      = &
 boot\fat32.sym system\trace\trc00fe.tff dll\uunifat.sym &
 cachef32.sym f32parts.sym f32mon.sym f32mount.sym &
 f32stat.sym f32chk.sym fat32chk.sym fat32fmt.sym fat32sys.sym &
 qemu-img.sym dll\qemuimg.sym

adddrvsym = &
 boot\partfilt.sym

adddrvdoc = &
 partfilt.doc partfilt.txt

distlist = &
!ifeq ROOT . # don't include autogenerated files to this list if make is run from a non-root
 $(p)os2\boot\fat32.ifs $(p)os2\boot\fat32.sym $(p)os2\system\trace\trc00fe.tff &
 $(p)os2\boot\partfilt.flt $(p)os2\boot\partfilt.sym &
 $(p)os2\dll\ufat32.dll $(p)os2\dll\uunifat.sym &
 $(p)os2\dll\ufat12.dll $(p)os2\dll\ufat16.dll &
 $(p)os2\dll\uunifat.dll &
!ifeq EXFAT 1
 $(p)os2\dll\uexfat.dll &
!endif
 $(p)os2\dll\qemuimg.dll $(p)os2\dll\qemuimg.sym &
 $(p)os2\qemu-img.exe $(p)os2\qemu-img.sym &
 $(p)os2\cachef32.exe $(p)os2\cachef32.sym &
 $(p)os2\f32parts.exe $(p)os2\f32parts.sym &
 $(p)os2\f32mon.exe $(p)os2\f32mon.sym &
 $(p)os2\f32stat.exe $(p)os2\f32stat.sym &
 $(p)os2\f32chk.exe $(p)os2\f32chk.sym &
 $(p)os2\f32mount.exe $(p)os2\f32mount.sym &
 $(p)os2\fat32chk.exe $(p)os2\fat32chk.sym &
 $(p)os2\fat32fmt.exe $(p)os2\fat32fmt.sym &
 $(p)os2\fat32sys.exe $(p)os2\fat32sys.sym &
 $(p)os2\book\fat32.inf $(p)os2\book\ifs.inf &
!endif
 $(p)os2\docs\fat32\partfilt.doc $(p)os2\docs\fat32\build.txt $(p)os2\docs\fat32\notes.txt &
 $(p)os2\docs\fat32\deamon.txt $(p)os2\docs\fat32\fat32.txt $(p)os2\docs\fat32\lesser.txt &
 $(p)os2\docs\fat32\license.txt $(p)os2\docs\fat32\os2fat32.txt $(p)os2\docs\fat32\problems.txt &
 $(p)os2\docs\fat32\partfilt.txt $(p)os2\docs\fat32\message.txt &
 $(p)os2\system\country.kor $(p)os2\docs\fat32\fat32.kor &
 $(p)os2\boot\os2dasd.f32 $(p)os2\cachef32.cmd
p = $(BINROOT)\
distfiles = $+ $(distlist) $-
p =
dist = $+ $(distlist) $-

distlist2 = &
!ifeq ROOT . # don't include autogenerated files to this list if make is run from a non-root
 $(p)win32\dll\ufat32.dll $(p)win32\dll\ufat32.sym &
 $(p)win32\fat32chk.exe $(p)win32\fat32chk.sym &
 $(p)win32\fat32fmt.exe $(p)win32\fat32fmt.sym &
 $(p)win32\fat32sys.exe $(p)win32\fat32sys.sym
!endif

p = $(BINROOT)\
distfiles2 = $+ $(distlist2) $-
p =
dist2 = $+ $(distlist2) $-

!ifneq EXFAT 1
suffix = -noexfat
!else
suffix = 
!endif

distname  = fat32-$(_VER)-r$(%SVNREV)$(suffix)
distfile1 = $(distname)-os2.zip
distfile2 = $(distname)-os2.wpi
distfile3 = $(distname)-win32.zip

AS=wasm
LNK=wlink op q
LIB=wlib -q
MAPCNV=$(ROOT)\mapsym.awk
IPFC=wipfc
WIC=wic

!ifdef _32BITS
CXX=wpp386
CC=wcc386
!else
CXX=wpp
CC=wcc
!endif

.SUFFIXES:
.SUFFIXES: .tff .tsf .flt .ifs .dll .exe .lib .lnk .ols .obc .obs .obd .obj .cpp .c .c16 .h .asm .sym .map .wmp .inf .ipf .bmp

all: $(BLDROOT)\bld.flg $(PROJ_BLD)\makefile.wcc dirs copy targets &
     $(BINROOT)\zip-os2.flg $(BINROOT)\zip-win32.flg $(BINROOT)\wpi.flg .symbolic

targets: .symbolic
 @for %t in ($(TARGETS)) do @wmake -h -f makefile.wcc %t

$(PROJ_BLD)\makefile.wcc: makefile.wcc
 @echo !include $(PATH)\makefile.wcc >$^@

dirs: .symbolic
 @for %d in ($(DIRS)) do @if exist %d @cd %d && wmake -h -f makefile.wcc targets

$(BLDROOT)\bld.flg:
 @for %d in ($(BLDDIRS)) do @if not exist %d @mkdir %d
 @wtouch $^@

$(BINROOT)\zip-os2.flg: $(distfiles)
 @echo ZIP      $(distfile1)
 @cd $(BINROOT)
 @echo "">zip-os2.flg
 @for %i in ($(dist)) do @zip -r $(distfile1) %i >>zip-os2.flg 2>&1
 @cd ..\$(PROJ)

$(BINROOT)\wpi.flg: $(distfiles)
 @echo WIC      $(distfile2)
 @cd $(BINROOT)
 @%create $(distname)-os2.pkg
 @%append $(distname)-os2.pkg $(distfile2) -a 1 -c.\os2\boot
 @for %file in ($(mainifs)) do @if exist os2\boot\%file @%append $(distname)-os2.pkg %file
 @%append $(distname)-os2.pkg 2 -c.\os2\dll
 @for %file in ($(dll)) do @if exist os2\dll\%file @%append $(distname)-os2.pkg %file
 @%append $(distname)-os2.pkg 3 -c.\os2
 @for %file in ($(util)) do @if exist os2\%file @%append $(distname)-os2.pkg %file
 @%append $(distname)-os2.pkg 4 -c.\os2\book
 @for %file in ($(inf)) do @if exist os2\book\%file @%append $(distname)-os2.pkg %file
 @%append $(distname)-os2.pkg 5 -c.\os2\docs\fat32
 @for %file in ($(docs)) do @if exist os2\docs\fat32\%file @%append $(distname)-os2.pkg %file
 @%append $(distname)-os2.pkg 6 -c.\os2
 @for %file in ($(korean)) do @if exist os2\%file @%append $(distname)-os2.pkg %file
 @%append $(distname)-os2.pkg 7 -c.\os2\boot
 @for %file in ($(adddrv)) do @if exist os2\boot\%file @%append $(distname)-os2.pkg %file
 @%append $(distname)-os2.pkg 8 -c.\os2
 @for %file in ($(sym)) do @if exist os2\%file @%append $(distname)-os2.pkg %file
 @%append $(distname)-os2.pkg 9 -c.\os2
 @for %file in ($(adddrvsym)) do @if exist os2\%file @%append $(distname)-os2.pkg %file
 @%append $(distname)-os2.pkg 10 -c.\os2\docs\fat32
 @for %file in ($(adddrvdoc)) do @if exist os2\docs\fat32\%file @%append $(distname)-os2.pkg %file
 @%append $(distname)-os2.pkg 11 -c.\os2\book
 @for %file in ($(ifsinf)) do @if exist os2\book\%file @%append $(distname)-os2.pkg %file
 @%append $(distname)-os2.pkg -s ..\lib\fat32_010.wis
 @$(WIC) @$(distname)-os2.pkg >wpi.flg 2>&1
 @cd ..\$(PROJ)

$(BINROOT)\zip-win32.flg: $(distfiles2)
 @echo ZIP      $(distfile3)
 @cd $(BINROOT)
 @echo "">zip-win32.flg
 @for %i in ($(dist2)) do @zip -r $(distfile3) %i >>zip-win32.flg 2>&1
 @cd ..\$(PROJ)

clean: .symbolic
 -@del $(CLEANUP) >nul 2>&1
 @for %d in ($(DIRS)) do @if exist %d cd %d && @wmake -h -f makefile.wcc clean

copy: $(BINROOT)\os2\boot\os2dasd.f32 $(BINROOT)\os2\system\country.kor &
 $(BINROOT)\os2\docs\fat32\partfilt.doc $(BINROOT)\os2\docs\fat32\fat32.kor $(BINROOT)\os2\docs\fat32\build.txt &
 $(BINROOT)\os2\docs\fat32\deamon.txt $(BINROOT)\os2\docs\fat32\fat32.txt $(BINROOT)\os2\docs\fat32\lesser.txt &
 $(BINROOT)\os2\docs\fat32\license.txt $(BINROOT)\os2\docs\fat32\os2fat32.txt $(BINROOT)\os2\docs\fat32\problems.txt &
 $(BINROOT)\os2\docs\fat32\partfilt.txt $(BINROOT)\os2\docs\fat32\message.txt &
 $(BINROOT)\os2\docs\fat32\notes.txt $(BINROOT)\os2\cachef32.cmd .symbolic 

$(BINROOT)\os2\boot\os2dasd.f32: $(ROOT)\lib\os2dasd.f32
 @copy $< $^@ >nul 2>&1

$(BINROOT)\os2\system\country.kor: $(ROOT)\lib\country.kor
 @copy $< $^@ >nul 2>&1

$(BINROOT)\os2\docs\fat32\partfilt.doc: $(ROOT)\doc\partfilt.doc
 @copy $< $^@ >nul 2>&1

$(BINROOT)\os2\docs\fat32\fat32.kor: $(ROOT)\doc\fat32.kor
 @copy $< $^@ >nul 2>&1

$(BINROOT)\os2\docs\fat32\build.txt: $(ROOT)\doc\build.txt
 @copy $< $^@ >nul 2>&1

$(BINROOT)\os2\docs\fat32\deamon.txt: $(ROOT)\doc\deamon.txt
 @copy $< $^@ >nul 2>&1

$(BINROOT)\os2\docs\fat32\fat32.txt: $(ROOT)\doc\fat32.txt
 @copy $< $^@ >nul 2>&1

$(BINROOT)\os2\docs\fat32\lesser.txt: $(ROOT)\doc\lesser.txt
 @copy $< $^@ >nul 2>&1

$(BINROOT)\os2\docs\fat32\license.txt: $(ROOT)\doc\license.txt
 @copy $< $^@ >nul 2>&1

$(BINROOT)\os2\docs\fat32\notes.txt: $(ROOT)\doc\notes.txt
 @copy $< $^@ >nul 2>&1

$(BINROOT)\os2\docs\fat32\os2fat32.txt: $(ROOT)\doc\os2fat32.txt
 @copy $< $^@ >nul 2>&1

$(BINROOT)\os2\docs\fat32\problems.txt: $(ROOT)\doc\problems.txt
 @copy $< $^@ >nul 2>&1

$(BINROOT)\os2\docs\fat32\partfilt.txt: $(ROOT)\doc\partfilt.txt
 @copy $< $^@ >nul 2>&1

$(BINROOT)\os2\docs\fat32\message.txt: $(ROOT)\doc\message.txt
 @copy $< $^@ >nul 2>&1

$(BINROOT)\os2\cachef32.cmd: $(ROOT)\lib\cachef32.cmd
 @copy $< $^@ >nul 2>&1

.inf: $(BINROOT)

.dll: $(BINROOT)

.exe: $(BINROOT)

.ifs: $(BINROOT)

.sym: $(BINROOT)

.lib: $(LIBROOT)

.obj: $(PROJ_BLD)

.obd: $(PROJ_BLD)

.lnk: $(PROJ_BLD)

.ols: $(PROJ_BLD)

.map: $(PROJ_BLD)

.wmp: $(PROJ_BLD)

.ipf: .

.bmp: .

.c: .

.c: ..

.asm: .

.asm: ..

.lnk.exe: .autodepend
 @echo LINK     $^.
 @$(LNK) @$<
!ifeq LXLITE 1
 @lxlite $@ 2>&1 >nul
!endif

.lnk.dll: .autodepend
 @echo LINK     $^.
 @$(LNK) @$<
!ifeq LXLITE 1
 @lxlite $@ 2>&1 >nul
!endif

.lnk.flt: .autodepend
 @echo LINK     $^.
 @$(LNK) @$<
!ifeq LXLITE 1
 @lxlite $@ 2>&1 >nul
!endif

.lnk.ifs: .autodepend
 @echo LINK     $^.
 @$(LNK) @$[@
!ifeq LXLITE 1
 @lxlite $@ 2>&1 >nul
!endif

.ols.lib: .autodepend
 @echo LIB      $^.
 @$(LIB) $^@ @$<

.asm.obj: .autodepend
 @echo AS       $^.
 @$(AS) $(AOPT) -fr=$^*.err -fo=$^@ $[@

.c.obj: .autodepend
 @echo CC       $^.
 @$(CC) $(COPT) -fr=$^*.err -fo=$^@ $<

.c.obd: .autodepend
 @echo CC       $^.
 @$(CC) $(COPT)  -d__DLL__ -bd -fr=$^*.err -fo=$^@ $<

.c.obc: .autodepend
 @echo CC       $^.
 @$(CC) $(COPT) -fr=$^*.err -fo=$^@ $<

.c.obs: .autodepend
 @echo CC       $^.
 @$(CC) $(COPT) -fr=$^*.err -fo=$^@ $<

.cpp.obj: .autodepend
 @echo CXX      $^.
 @$(CXX) $(COPT) -fr=$^*.err -fo=$^@ $[@

.ipf.inf: .autodepend
 @echo IPFC     $^.
 @ipfc -i $< $(BINROOT)$^@ >nul 2>&1
 #@wipfc -i $< -o $(BINROOT)$^@ >nul 2>&1

.wmp.map:
 @echo MAPCNV   $^.
 @awk -f $(MAPCNV) $< >$(PROJ_BLD)\$^.

.map.sym:
 @echo MAPSYM   $^.
 @mapsym $[@ >nul 2>&1
 @copy $^. $^: >nul 2>&1
 @del $^. >nul 2>&1
