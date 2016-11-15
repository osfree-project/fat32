_VENDOR=Netlabs
_VER=0.10a8
_VERSION=$(_VER):r$(%SVNREV)
FILEVER=@$#$(_VENDOR):$(_VERSION)$#$#built $(%DATE) on $(%HOSTNAME);0.1$#@

BINROOT  = $(ROOT)\bin
BLDROOT  = $(ROOT)\bld
LIBROOT  = $(BLDROOT)\lib
PROJ_BLD = $(BLDROOT)\$(PROJ)
BLDDIRS  = $(BINROOT) $(BLDROOT) $(LIBROOT) &
           $(BINROOT)\os2 $(BINROOT)\os2\boot $(BINROOT)\os2\dll &
           $(BINROOT)\os2\book $(BINROOT)\os2\docs $(BINROOT)\os2\docs\fat32 &
           $(BLDROOT)\util $(BLDROOT)\ifs $(BLDROOT)\ifs\libc $(BLDROOT)\partfilt $(BLDROOT)\ifsinf

CLEANUP  = $(PROJ_BLD)\*.obj $(PROJ_BLD)\*.obh $(PROJ_BLD)\*.lnk $(PROJ_BLD)\*.wmp &
           $(PROJ_BLD)\*.map $(PROJ_BLD)\*.ols $(PROJ_BLD)\*.err $(BLDROOT)\..\include\ver.h &
           $(BLDROOT)\lib\*.lib 

mainifs  = &
 fat32.ifs fat32.sym

dll      = &
 ufat32.dll ufat32.sym

util     = &
 cachef32.exe cachef32.sym &
 diskdump.exe diskdump.sym &
 diskinfo.exe diskinfo.sym &
 f32mon.exe f32mon.sym &
 f32stat.exe f32stat.sym &
 fat32chk.exe fat32chk.sym &
 fat32fmt.exe fat32fmt.sym

inf      = &
 fat32.inf

docs     = &
 partfilt.doc build.txt &
 deamon.txt fat32.txt lesser.txt &
 license.txt os2fat32.txt problems.txt &
 partfilt.txt

korean   = &
 country.kor fat32.kor

adddrv   = &
 os2dasd.f32 partfilt.flt partfilt.sym

distlist = &
 os2\boot\fat32.ifs os2\boot\fat32.sym &
 os2\dll\ufat32.dll os2\dll\ufat32.sym &
 os2\cachef32.exe os2\cachef32.sym &
 os2\diskdump.exe os2\diskdump.sym &
 os2\diskinfo.exe os2\diskinfo.sym &
 os2\f32mon.exe os2\f32mon.sym &
 os2\f32stat.exe os2\f32stat.sym &
 os2\fat32chk.exe os2\fat32chk.sym &
 os2\fat32fmt.exe os2\fat32fmt.sym &
 os2\book\fat32.inf &
 os2\docs\fat32\partfilt.doc os2\docs\fat32\build.txt &
 os2\docs\fat32\deamon.txt os2\docs\fat32\fat32.txt os2\docs\fat32\lesser.txt &
 os2\docs\fat32\license.txt os2\docs\fat32\os2fat32.txt os2\docs\fat32\problems.txt &
 os2\docs\fat32\partfilt.txt &
 os2\boot\country.kor os2\docs\fat32\fat32.kor &
 os2\boot\os2dasd.f32 os2\boot\partfilt.flt os2\boot\partfilt.sym

distname  = fat32-$(_VER)-r$(%SVNREV)
distfile1 = $(distname).zip
distfile2 = $(distname).wpi

AS=wasm
LNK=wlink op q
LIB=wlib -q
MAPCNV=..\mapsym.awk
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
.SUFFIXES: .flt .ifs .dll .exe .lib .lnk .ols .obh .obj .cpp .c .h .asm .sym .map .wmp .inf .ipf .bmp

all: $(BLDROOT)\bld.flg $(PROJ_BLD)\makefile.wcc dirs copy targets &
     $(BINROOT)\zip.flg $(BINROOT)\wpi.flg .symbolic

targets: .symbolic
 @for %t in ($(TARGETS)) do @wmake -h -f makefile.wcc %t

$(PROJ_BLD)\makefile.wcc: makefile.wcc
 @echo !include $(PATH)\makefile.wcc >$^@

dirs: .symbolic
 @for %d in ($(DIRS)) do @if exist %d @cd %d && wmake -h -f makefile.wcc targets

$(BLDROOT)\bld.flg:
 @for %d in ($(BLDDIRS)) do @mkdir %d
 @wtouch $^@

$(BINROOT)\zip.flg:
 @echo ZIP      $(distfile1)
 @cd $(BINROOT)
 @zip -r $(distfile1) $(distlist) >nul 2>&1
 @cd $(PATH)
 @wtouch $^@

$(BINROOT)\wpi.flg:
 @echo WIC      $(distfile2)
 @cd $(BINROOT)
 @%create $(distname).pkg
 @%append $(distname).pkg $(distfile2) -a 1 -c.\os2\boot
 @for %file in ($(mainifs)) do @if exist os2\boot\%file @%append $(distname).pkg %file
 @%append $(distname).pkg 2 -c.\os2\dll
 @for %file in ($(dll)) do @if exist os2\dll\%file @%append $(distname).pkg %file
 @%append $(distname).pkg 3 -c.\os2
 @for %file in ($(util)) do @if exist os2\%file @%append $(distname).pkg %file
 @%append $(distname).pkg 4 -c.\os2\book
 @for %file in ($(inf)) do @if exist os2\book\%file @%append $(distname).pkg %file
 @%append $(distname).pkg 5 -c.\os2\docs\fat32
 @for %file in ($(docs)) do @if exist os2\docs\fat32\%file @%append $(distname).pkg %file
 @%append $(distname).pkg 6 -c.\os2\boot
 @for %file in ($(korean)) do @if exist os2\boot\%file @%append $(distname).pkg %file
 @%append $(distname).pkg 7 -c.\os2\boot
 @for %file in ($(adddrv)) do @if exist os2\boot\%file @%append $(distname).pkg %file
 @%append $(distname).pkg -s ..\lib\fat32_010.wis
 @$(WIC) @$(distname).pkg >nul 2>&1
 @cd $(PATH)
 @wtouch $^@

clean: .symbolic
 -@del $(CLEANUP) >nul 2>&1
 @for %d in ($(DIRS)) do @if exist %d cd %d && @wmake -h -f makefile.wcc clean

copy: $(BINROOT)\os2\boot\os2dasd.f32 $(BINROOT)\os2\boot\country.kor &
 $(BINROOT)\os2\docs\fat32\partfilt.doc $(BINROOT)\os2\docs\fat32\fat32.kor $(BINROOT)\os2\docs\fat32\build.txt &
 $(BINROOT)\os2\docs\fat32\deamon.txt $(BINROOT)\os2\docs\fat32\fat32.txt $(BINROOT)\os2\docs\fat32\lesser.txt &
 $(BINROOT)\os2\docs\fat32\license.txt $(BINROOT)\os2\docs\fat32\os2fat32.txt $(BINROOT)\os2\docs\fat32\problems.txt &
 $(BINROOT)\os2\docs\fat32\partfilt.txt .symbolic 

$(BINROOT)\os2\boot\os2dasd.f32: $(ROOT)\lib\os2dasd.f32
 @copy $< $^@ >nul 2>&1

$(BINROOT)\os2\boot\country.kor: $(ROOT)\lib\country.kor
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

$(BINROOT)\os2\docs\fat32\os2fat32.txt: $(ROOT)\doc\os2fat32.txt
 @copy $< $^@ >nul 2>&1

$(BINROOT)\os2\docs\fat32\problems.txt: $(ROOT)\doc\problems.txt
 @copy $< $^@ >nul 2>&1

$(BINROOT)\os2\docs\fat32\partfilt.txt: $(ROOT)\doc\partfilt.txt
 @copy $< $^@ >nul 2>&1

.inf: $(BINROOT)

.dll: $(BINROOT)

.exe: $(BINROOT)

.ifs: $(BINROOT)

.sym: $(BINROOT)

.lib: $(LIBROOT)

.obj: $(PROJ_BLD)

.lnk: $(PROJ_BLD)

.ols: $(PROJ_BLD)

.map: $(PROJ_BLD)

.wmp: $(PROJ_BLD)

.ipf: .

.bmp: .

.c: .

.asm: .

.lnk.exe: .autodepend
 @echo LINK     @$^.
 @$(LNK) @$<

.lnk.dll: .autodepend
 @echo LINK     @$^.
 @$(LNK) @$<

.lnk.flt: .autodepend
 @echo LINK     @$^.
 @$(LNK) @$<

.lnk.ifs: .autodepend
 @echo LINK     @$^.
 @$(LNK) @$[@

.ols.lib: .autodepend
 @echo LIB      $^.
 @$(LIB) $^@ @$<

.asm.obj: .autodepend
 @echo AS       $^.
 @$(AS) $(AOPT) -fr=$^*.err -fo=$^@ $[@

.c.obj: .autodepend
 @echo CC       $^.
 @$(CC) $(COPT) -fr=$^*.err -fo=$^@ $<

.c.obh: .autodepend
 @echo CC       $^.
 @$(CC) $(COPT) -d__DLL__ -bd -fr=$^*.err -fo=$^@ $<

.cpp.obj: .autodepend
 @echo CXX      $^.
 @$(CXX) $(COPT) -fr=$^*.err -fo=$^@ $[@

.ipf.inf: .autodepend
 @echo IPFC     $^.
 @$(IPFC) -i $< -o $(BINROOT)$^@ >nul 2>&1

.wmp.map:
 @echo MAPCNV   $^.
 @awk -f $(MAPCNV) $< >$(PROJ_BLD)\$^.

.map.sym:
 @echo MAPSYM   $^.
 @mapsym $[@ >nul 2>&1
 @move $^. $^: >nul 2>&1
