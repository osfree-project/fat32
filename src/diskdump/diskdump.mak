# diskdump.mak
# Created by IBM WorkFrame/2 MakeMake at 1:31:25 on 6 April 1998
#
# The actions included in this make file are:
#  Compile::C++ Compiler
#  Link::Linker

.SUFFIXES:

.SUFFIXES: \
    .cpp .obj

.cpp.obj:
    @echo " Compile::C++ Compiler "
    icc.exe /Tl10 /Ss /Ti+ /O- /Gm /C %s

{f:\csrcos2\diskdump}.cpp.obj:
    @echo " Compile::C++ Compiler "
    icc.exe /Tl10 /Ss /Ti+ /O- /Gm /C %s

all: \
    .\diskdump.exe

.\diskdump.exe: \
    .\diskdump.obj
    @echo " Link::Linker "
    icc.exe @<<
     /B" /exepack:2 /de"
     /Fediskdump.exe
     .\diskdump.obj
<<

.\diskdump.obj: \
    f:\csrcos2\diskdump\diskdump.cpp
