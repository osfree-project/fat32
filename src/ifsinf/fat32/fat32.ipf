:userdoc.
:docprof toc=123456.
:title.FAT32.IFS

:h1 id=0 res=30000.Introduction
:artwork name='img0.bmp' align=center. 

.* :artwork name='img1.bmp' align=center. 

:lines align=center.
:hp2. Version 0.10 :ehp2.
:elines.

:p.
:p.

:p.:hp2.The universal FAT (FAT12/FAT16/FAT32/exFAT) IFS DRIVER 
FOR OS/2 and OS/2-based systems&colon. :ehp2.  

:p.This project is based on the original FAT32 source code by Henk Kelder who 
was kind enough to release the source to Netlabs (netlabs&per.org) in January 2002 
because he no longer uses OS/2&per.  As a result, it is now a Netlabs project with the 
full source code always available&per. 

:p.The FAT32 homepage at http&colon.//trac&per.netlabs&per.org/fat32/ provides both 
a simple zip file and a WarpIN package which will install FAT32 without much effort 
on the users part&per.

:p. Please note that Arca Noae distributes a stripped-down version without exFAT
support, for legal reasons&per. To get a full-featured version, please refer to the
official project home page, which is mentioned above&per.

:p.:hp2.Authors&colon. :ehp2.  
.br 
-Henk Kelder&colon. original author 
.br 
-Brian Smith&colon. contributor 
.br 
-KO Myung-Hun&colon. main developer for the FAT32 IFS driver 
.br 
-Alfredo Fern�ndez D�az&colon. creator of the WarpIN Script for FAT32 and 
contributor 
.br 
-David Graser&colon. creator of the FAT32&per.INF file 
.br 
-Lars Erdmann&colon. developer
.br
-Valery V. Sedletski&colon. FAT32 IFS version 0.10 developer
.br 

:p.:hp2.User's and developer's resources&colon. :ehp2.
.br
-Project bugtracker&colon. http&colon.//trac&per.netlabs&per.org/fat32/report/6
.br
-Mailing list for users&colon. https&colon.//groups&per.yahoo&per.com/neo/groups/fat32user/info
.br
-Mailing list for developers&colon. https&colon.//groups&per.yahoo&per.com/neo/groups/fat32dev/info
.br
-SVN repository&colon. http&colon.//svn&per.netlabs&per.org/repos/fat32/trunk
.br
-Builds archive&colon. ftp&colon.//osfree&per.org/upload/fat32/
.br

:p.Remember that patches are always welcome&per. If anyone would like to 
contribute, feel free to contact Netlabs&per.

:p.
:p.
:p.:link reftype=hd refid=1.DISCLAIMER :elink.
.br 

:h1 id=1 res=30001.Disclaimer
:artwork name='img2.bmp' align=center. 
:p.
:p.
:p.

:p.:hp2.I allow you to use all software in this package freely under the condition 
that the author, contributors, and myself are in no way responsible for any damage 
or loss you may suffer&per. 

:p.You should be aware of the fact that FAT32&per.IFS might damage the data 
stored on your hard disks&per. 

:p.If you cannot agree to these conditions, you should NOT use FAT32&per.IFS! 

:p.Adrian Gschwend &atsign. netlabs&per.org :ehp2.  

:h1 id=2 res=30002.Making OS/2 Recognize FAT32 Partitions

:p.:hp2.MAKING OS/2 RECOGNIZE FAT32 PARTITIONS&colon. :ehp2.  

:p.EComStation and OS/2 by themselves do not recognize FAT32 partitions&per.  
This means that installing the FAT32 IFS (installable file system) is useless if we 
cannot make OS/2 recognize them&per. There are several ways this is done&per. 

:p.:link reftype=hd refid=3.Logical Volume Manager (LVM) :elink.
.br 
:link reftype=hd refid=4.Partition Support Packages for non-LVM Systems :elink.
.br 

:p.:hp2.Note&colon.  :ehp2.Once recognized, FAT32 partitions already formatted by Windows 
systems should be readily accessible after this FAT32 driver is installed and loaded
&per. 

:p.  
:h2 id=3 res=30003.Logical Volume Manager (LVM)

:p.:hp2.LOGICAL VOLUME MANAGER (LVM)&colon. :ehp2.

:p.EComstation, Warp Server for e-business, and the convenience packages come 
with a Logical Volume Manager (LVM) to recognize the FAT32&per.IFS&per. 
.br 

:p.The Logical Volume Manager (LVM), a disk management tool, replaces the Fixed 
Disk utility (FDISK)&per. It is used to create volumes and partitions, assign drive 
letters, span a disk across physical hard drives, and define volumes that include 
multiple hard drives 
.br 

.br 
LVM must be used to create either a &apos.bootable volume&osq. or a &osq.
compatibility volume&osq. for your FAT32 partitions&per.  If the FAT32 partition contains a 
version of the Windows operating system, and you want to boot to it, then you must use 
LVM to create a &osq.bootable volume&osq.&per. In addition, the IBM Boot Manager is 
also required to boot to your Win9x partition&per. If the FAT32 partition contains 
data that you wish to manage, then use LVM to create a &osq.compatibility volume
&osq.&per. It is also possible to run Windows programs from a bootable or 
compatibility volume using ODIN&per.  After the creation of the volumes, assign them drive 
letters&per.

:p.:hp2.Warning&colon. :ehp2.:hp8.Do not use DANIDASD&per.DMD, PARTFILT&per.FLT, or the modified 
OS2DASD&per.DMD with these systems! :ehp8.
:p.
:p.
:p.:link reftype=hd refid=4.PARTITION SUPPORT PACKAGES FOR NON-LVM SYSTEMS :elink.
.br 

:h2 id=4 res=30004.Partition Support Packages for non-LVM Systems

:p.:hp2.PARTITION SUPPORT PACKAGES FOR NON-LVM SYSTEMS&colon. :ehp2.

:p.For this installable file system to access FAT32 media, first OS/2 has to 
be able to assign them a drive letter&per. You already can do this in LVM systems
&per. For non-LVM systems you need a &osq.partition support&osq. package&per.  
Presently, there are three such packages available&per. 
.br 

:color fc=black.:color bc=yellow.
:p. 1&per.:link reftype=hd refid=5.Daniela Engert&apos.s enhanced DANIDASD&per.DMD:elink., a replacement for the 
OS2DASD&per.DMD found in OS/ 2 3 and 4&per. This is the preferred choice for non-LVM 
systems&per. :color fc=default.:color bc=default.

:p. 2&per.Deon van der Westhuysen&apos.s :link reftype=hd refid=18.PARTFILT&per.FLT:elink., a BASEDEV FILTER to 
&osq.fake&osq. partitions that are not normally unsupported by OS/2&per. It is 
obsolete with :link reftype=hd refid=5.DANIDASD&per.DMD :elink.being a much better choice&per. 

:p. 3&per.Henk Kelder&apos.s modified version of :link reftype=hd refid=30.OS2DASD&per.DMD&per.:elink., another 
replacement for the OS2DASD&per.DMD found in OS/2 3 and 4&per. It too is obsolete with 
DANIDASD&per.DMD being a much better choice&per. 
.br 

:h3 id=5 res=30005.Daniela Engert's Enhanced DASD Manager

:p.:hp2.SYNOPSIS&colon. :ehp2.  

:p.Slightly enhanced version of the OS/2 DASD manager OS2DASD&per.DMD to ease 
coexistence with WinXY systems&per. Supports some limited form of logical volume management
&per. 

:p.

:p.:hp2.DESCRIPTION&colon. :ehp2.  

:p.This release of Daniela&apos.s DASD Manager driver is based on the OS2DASD
&per.DMD sources provided by IBM on their DDK site&per. It has the latest feature set 
as found in the current fixpacks&per. 

:p.:link reftype=hd refid=6.AVAILABILITY&colon. :elink.
.br 

:h4 id=6 res=30017.Availability

:p.:hp2.AVAILABILITY&colon. :ehp2.

:p.Daniela&apos.s driver can be found on Hobbes at&colon. 

:p.:hp9.http&colon.//hobbes&per.nmsu&per.edu/pub/os2/system/drivers/storage/
danidasd144&per.zip :ehp9.
.br 

:p.There may eventually be a WarpIN (wpi) version available at Hobbes in the 
same location as the zip file&per. 
:p.:link reftype=hd refid=7.CAUTION&colon. :elink.
.br 

:h4 id=7 res=30006.Caution

:p.:hp2.CAUTION&colon. :ehp2.  

:p.DaniDASD&per.DMD is made for non-LVM systems only! 
.br 
It does not work with OS2LVM&per.DMD! 
.br 
It cannot overcome limitations of FDISK or OS/2 Bootmanager! 
.br 

:p.:link reftype=hd refid=8.ENHANCEMENTS&colon. :elink.
.br 

:h4 id=8 res=30007.Enhancements

:p.:hp2. DANIELA&apos.S ENHANCEMENTS ARE&colon. :ehp2.

:p.It supports extendedX partitions (type 0F) and Linux extended partitions (
type 8F, even more clueless and useless than the Microsoft invented type 0F !) 
exactly the same as regular extended partitions (type 05)&per. ExtendedX partitions are 
common today because of disks larger than 8GiB&per. 

:p.It supports FAT32 partitions (type 0B and 0C) exactly as IFS partitions (
type 07, used by HPFS and NTFS)&per. This way FAT32 partitions show up in &osq.
correct&osq. order (w&per.r&per.t&per. the disk layout), and are assigned drive letters 
&osq.as expected&osq.&per. PARTFILT&per.FLT is no longer necessary to make FAT32 
partitions accessible, but Henk Kelder&apos.s FAT32&per.IFS is still required&per. 

:p.It supports FAT16X partitions (type 0E) exactly as big FAT16 (type 06) 
partitions&per. 
:p.It supports up to eight additional partition types as specified by by the 

user&per. This is meant for making hidden partitions (types 1x) or foreign file 
systems like Linux&apos.s ext2 (type 83) visible to the DASD manager&per. You have to 
tell DaniDASD&per.DMD the additional partition types by means of the option 

:p./AT&colon.<type>{,<type>{,<type>&per.&per.&per.}} (<type> must be two digits 
hexadecimal!) 
.br 

:p.To summarize the notion of acceptable partition types, have a look at this 
table&colon. 

:p.:hp2.DASD manager :ehp2.:hp2.partition types supported :ehp2.
.br 

.br 
:hp2.extended regular :ehp2.
.br 
:hp2.OS2DASD (pre-LVM) :ehp2.05  01, 04, 06, 07 
.br 
:hp2.OS2DASD (LVM) :ehp2.05, 0F 01, 04, 06, 07, 35, [+ other] 
.br 
:hp2.DaniDASD :ehp2.05, 0F, 8F 01, 04, 06, 07, 0B, 0C, 0E, [+ other] 
.br 

:p.If an additional partition type is a &osq.hidden&osq. partition (1x), then a 
matching partition is propagated to its &osq.not-hidden&osq. counterpart (0x) and the 
acceptance rules above apply&per. 

:p.:link reftype=hd refid=9.ENHANCED OPTIONS :elink.
.br 

:h5 id=9 res=30008.Enhanced Options

:p.:hp2.ENHANCED OPTIONS&colon. :ehp2.  

:p.It can overcome boot problems which result in &osq.can&apos.t operate hard 
disk&osq. messages&per. This is common if OS/2 is booted without proper help of OS/2 
Bootmanager which is required to notify OS/2 of the *correct* boot drive letter&per. If 
there is no boot manager with this capability available (e&per.g&per. when booting 
from removable media), or the boot manager fails to figure out the correct boot 
drive letter, you can tell DaniDASD&per.DMD the correct one by means of the option 

:p./BD&colon.<drive letter> 

:p.which will override the boot drive letter assignment from the first boot 
stage&per. 

:p.:hp2.Example&colon.  :ehp2.In a mixed SCSI/EIDE environment with two SCSI adapters and 
and two different EIDE controllers involved (her current setup at home), the ATAPI 
ZIP250 drive gets assigned drive letter M&colon.&per. 
.br 
To boot from this unit, she has this line in CONFIG&per.SYS of the ZIP boot 
floppy 

:p.BASEDEV=DaniDASD&per.DMD /BD&colon.M 

:p.:hp2.CAUTION&colon.
.br 
:ehp2.It looks like the WPS doesn&apos.t honour this setting so that it is pretty much 
restricted to console sessions&per. 

:p.It can create a logical volume map independently from the physical volumes (
partitions)&per. 

:p.:hp2.CAUTION&colon.
.br 
:ehp2.When this option is in effect, DaniDASD no longer follows the old OS2DASD rule 
of accepting only the first acceptable partition in the primary partition table
&per. Instead, the LVM rule of accepting all acceptable primary partitions applies! 

:p.After scanning the *fixed* disks for acceptable partitions following the 
rules above, a logical volume mapping is created from these partitions&per. This is 
done by means of a map table&per. This table lists the partitions which are to be 
given a drive letter in ascending, consecutive order, starting from drive letter C
&colon.&per. A particular physical partition is indicated by a two-letter code <
physical disk number><physical partition number>&per. Code &osq.Aa&osq. indicates the 
first partition on the first disk, &osq.Ab&osq. the second partition on the same disk
, &osq.Bc&osq. the third partition on the second disk, and so on&per. The 
numbering of disks and partitions is given by the regular scanning order of DaniDASD&per. 

:p.Each entry in the mapping table must be unique, entries without an existing 
partition are ignored&per. 

:p.Example&colon. 

:p.BASEDEV=DaniDASD&per.DMD /MT&colon.Aa,Ad,Ba,Bb 

:p.creates logical volumes C&colon.&per.&per.F&colon. from the indicated 
physical partitions&per. 

:p.:hp2.CAUTION&colon.
.br 
:ehp2.This does *not* apply to removable media units! Their drive letters *always* 
follow the fixed disk ones&per. 

:p.:hp2.CAUTION&colon.
.br 
:ehp2.Remapping visible FAT and HPFS primary partitions may fail! 

:p.May replace OS2PCARD&per.DMD&per. If you are running PCM2ATA&per.ADD, either 
remove the /!DM option there or add /PC to the DaniDASD&per.DMD command line&per. In 
case of DaniS506&per.ADD handling PCCard ATA units, no option is required&per. 
.br 

:p.:link reftype=hd refid=10.COMMAND LINE OPTIONS :elink.
.br 

:h4 id=10 res=30009.Command Line Options

:p.:hp2.COMMAND LINE OPTIONS&colon. :ehp2.

:p.This driver supports all switches and options OS2DASD&per.DMD supports&per. 
For an explanation of these look into the OS/2 online help (by picking &apos.OS/2 
Commands by name&apos./ &apos.BASEDEV&per.&per.&per.&apos./&apos.OS2DASD&per.&per.&per.
&apos.) and the updates found in the fixpak documentation&per. 

:p.For the sake of completeness, these are the OS2DASD options&colon. 

:p.:hp7.Options :ehp7.:hp7.Descriptions :ehp7.
.br 

:p.:hp2./of :ehp2.treat optical units like disk units (i&per.e&per. MO drives as additional 
removable disks) 

:p.:hp2./lf :ehp2.treat removable disks like large floppies (i&per.e&per. not partitioned) 

:p.:hp2./mp :ehp2.reserve a number partitions for removable media units (see OS2DASD docs 
for details 
.br 

:p.:link reftype=hd refid=11.DDINSTAL INSTALLATION :elink.
.br 

:h4 id=11 res=30010.DDInstal Installation

:p.:hp2.DDINSTAL INSTALLATION&colon. :ehp2.  

:p.Of the three foreign partition support packages, this is the preferred choice 
for installation&per. 

:p.For an upgrade from a previously installed OS2DASD&per.DMD driver, just copy 
DaniDASD&per.DMD to bootdrive&colon.&bsl.OS2&bsl.BOOT and rename OS2DASD&per.DMD to 
DaniDASD&per.DMD in your CONFIG&per.SYS&per. 

:p.BASEDEV=DaniDASD&per.DMD 

:p.If you prefer chasing icons, you can do the copy and the modification of 
CONFIG&per.SYS using DDINSTAL (&osq.OS/2 System&osq./&osq.System Setup&osq./&osq.
Install/Remove&osq. /&osq.Device Driver&osq.)&per. Due to limits in DDINSTAL, you have 
to disable the previously installed OS2DASD&per.DMD driver in CONFIG&per.SYS 
manually, either by deleting that line or putting a &apos.rem&apos. statement in front 
of it&per. 

:p.:link reftype=hd refid=12.FILES :elink.
.br 

:h4 id=12 res=30011.Files

:p.:hp2.FILES&colon. :ehp2.  
.br 

:p.:hp7.Files :ehp7.:hp7.Descriptions :ehp7.
.br 

:p.:hp2.DaniDASD&per.DMD :ehp2.The driver itself 

:p.:hp2.DaniDASD&per.DDP :ehp2.The device driver profile for DDINSTAL 

:p.:hp2.DaniDASD&per.DOC :ehp2.A Document containing the information about DaniDASD 

:p.:hp2.cmprssd&bsl.DaniDASD&per.DMD :ehp2.The same as above, but LXlite compressed&per. 
This one is *not* guaranteed to work on every system&per. 
.br 

:p.:link reftype=hd refid=13.DISCLAIMER :elink.
.br 

:h4 id=13 res=30012.Disclaimer

:p.:hp2.DANIELA ENGERT&apos.S DISCLAIMER&colon. :ehp2.
.br 

:p.YOU ARE USING THIS PROGRAM AT YOUR OWN RISK! I don&apos.t take any 
responsibility for damages, problems, custodies, marital disputes, etc&per. resulting from use
, inability to use, misuse, possession or non-possession of this program directly 
or indirectly&per. I also don&apos.t give any warranty for bug-free operation, 
fitness for a particular purpose or the appropriate behavior of the program concerning 
animals, programmers and little children&per. 

:p.THE SOFTWARE IS PROVIDED ``AS IS&apos.&apos. AND WITHOUT ANY EXPRESS OR 
IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF 
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE&per. THE ENTIRE RISK AS TO THE QUALITY AND 
PERFORMANCE OF THE 
.br 
PROGRAM IS WITH YOU&per.  SHOULD THE PROGRAM PROVE DEFECTIVE, YOU ASSUME THE COST 
OF ALL NECESSARY SERVICING, REPAIR OR CORRECTION&per. 

:p.Or, in a few words&colon. 
.br 
If its good, I am responsible&per. 
.br 
If its bad, its all your fault&per. ;) 

:p.Permission is granted to redistribute this program free of charge, provided 
it is distributed in the full archive with unmodified contents and no profit 
beyond the price of the media on which it is distributed is made&per. Exception to the 
last rule&colon. It may be included on freeware/shareware collections on CD-ROM, as 
well as on magazine cover CD-ROMs&per. 

:p.All trademarks mentioned anywhere around here are property of their owners 
and the like &per.&per.&per. 

:p.
.br 
Daniela Engert <dani&atsign.ngrt 

:p.:link reftype=hd refid=14.COPYRIGHT AND STATUS :elink.
.br 

:h4 id=14 res=30013.Copyright and Status

:p.:hp2.COPYRIGHT&colon. :ehp2.

:p.     DaniDASD - Copyright (c) 2001 Daniela Engert&per. All rights reserved&per. 

:p.:hp2.STATUS OF PROGRAM&colon. :ehp2.

:p.     DaniDASD is freeware&per. 

:p.:link reftype=hd refid=15.AUTHOR :elink.
.br 

:h4 id=15 res=30014.Contact

:p.:hp2.AUTHOR&colon. :ehp2.  
.br 

.br 
     Daniela Engert 
:p.     Internet&colon. &osq.Daniela Engert&osq. <dani&atsign.ngrt&per.de>        (preferred
) 
.br 
     Fidonet&colon. 2&colon.2490/2575 (no debug support here) 
:p.:link reftype=hd refid=16.BUGS :elink.
.br 

:h4 id=16 res=30015.Bugs

:p.:hp2.BUGS&colon. :ehp2.  

:p.Hopefully none, but who knows &per.&per.&per. 
.br 
Up to date, not a single problem was reported &colon. 

:p.Many people refer to driver revisions by file dates&per. Don&apos.t do that 
for these reasons&colon. 

:p.- She doesn&apos.t keep track of file dates 
.br 
- She builds several drivers almost each day 
.br 
- file dates can easily be changed 
.br 
- there are many different calendars in this world 

:p.The only valid reference is the driver&apos.s version number&per. It is shown 
when you run &osq.BLDLEVEL DaniDASD&per.DMD&osq. which will output something like 
this&colon. 

:p.
:cgraphic.
Signature&colon.     &atsign.#DANI&colon.1&per.0#&atsign.##1## 17&per.2&per.2001 19&colon.46&colon.21
Vendor&colon.        DANI
Revision&colon.      1&per.00
Date/Time&colon.     17&per.2&per.2001 19&colon.46&colon.21
Build Machine&colon. Nachtigall
File Version&colon.  1&per.0&per.0 <������������������Ŀ
Description&colon.   OS/2 DASD Device Manager  �
                                         �
This is the full version number  ���������

:ecgraphic.

:p.She has no means to identify traps occurring outside this driver (module name 
different from &apos.Disk DD&apos. or not present)&per. If a trap occurs within this 
driver, she needs the following information&colon. CS&colon.EIP, EAX-EDI, DS and ES
&per. Her test systems at home (VIA and ALi) and a couple of other machines at work (
different INTEL chips) run flawlessly with this driver, so she has virtually no chance to 
reproduce errors&per. 

:p.:link reftype=hd refid=17.BUG REPORTS :elink.
.br 

:h5 id=17 res=30016.Bug Reports

:p.:hp2.BUG REPORTS&colon. :ehp2.  

:p.Suggestions and bug-reports are always welcome&per. Well &per.&per.&per. bug-
reports are perhaps not *that* welcome &per.&per.&per. 

:p.
:h3 id=18 res=30018.PARTFILT.FLT

:p.:hp2.PARTFILT&per.FLT&colon. :ehp2.
.br 

:p.PARTFILT is based on the excellent work of Deon van der Westhuysen&per. Henk 
Kelder made only minor modifications to it&per. The source code is available under GPL 
conditions and can now be downloaded from the Netlabs&per. 

:p.Also, once PARTFILT is installed FDISK, Partition Magic and other Partition 
tools can no longer be trusted&per. Do not use these tools once PARTFILT is installed
&per. 

:p.PARTFILT&per.FLT is a BASEDEV FILTER device that is able to present (fake) 
partition types that are normally unsupported by OS/2 in such a way to OS/2 that IFS
&apos.s can be loaded on these partitions&per. Such virtualized partition will always 
be mounted after the non-virtualized partitions&per. 

:p.The best location in the config&per.sys seems to differ depending on your 
configuration&per. Some state FAT32&per.IFS will only work is PARTFILT is the first basedev, 
others claim it only works if PARTFILT is the last one&per. 

:p.A specific problem was reported when using a SCSI power save basedev that 
only seemed to work if PARTFILT was the last basedev&per. 

:p.:link reftype=hd refid=19.AVAILABILITY :elink.
.br 

:h4 id=19 res=30029.Availability

:p.:hp2.AVAILABILITY&colon. :ehp2.

:p.The partfilt&per.flt driver will NO longer be distributed with the FAT32 IFS 
driver&per.  If needed, it can be extracted from Henk&apos.s latest binary release and 
manually installed entirely at the user&apos.s own risk&per. Henk Kelder&apos.s last 
release can be found on Hobbes at&colon. 

:p.:hp9.http&colon.//hobbes&per.nmsu&per.edu/pub/os2/system/drivers/filesys/os2fat32
&per.zip :ehp9.

:p.:link reftype=hd refid=20.INSTALLATION :elink.
.br 

:h4 id=20 res=30019.Installation

:p.:hp2.INSTALLATION&colon. :ehp2.

:p.:hp2.Note&colon.  :ehp2.This driver is obsolete with Daniela&apos.s DaniDASD&per.DMD 
being the better choice&per.  However, if one prefers this driver, it can be installed 
doing the following&per. 

:p.Copy the partfilt&per.flt driver to your &bsl.OS2&bsl.Boot directory&per. 
.br 

.br 
Add the following line to the config&per.sys file&colon. 
:p.BASEDEV=PARTFILT&per.FLT /P 0B [/W] 
.br 

.br 
(The /W should not be specified if you do not want write access) (The best 
location seems to differ depending on your configuration&per. Some state FAT32&per.IFS 
will only work is PARTFILT is the first basedev, other claim it only works if 
PARTFILT is the last one&per.) PARTFILT&per.FLT might need other options IF you have 
converted an existing FAT16 partition to FAT32 or if you want to influence the drive 
order! This manual install will add FAT32 partition after all existing drives&per. 

:p.Reboot&per. 

:p.:link reftype=hd refid=21.SUPPORTED FILE SYSTEMS :elink.
.br 

:h4 id=21 res=30020.Supported File Systems

:p.:hp2.SUPPORTED FILE SYSTEMS&colon. :ehp2.

:p.Currently, the filter can only be used for two IFS&apos.s&colon. the FAT32
&per.IFS and the Linux IFS&per. 

:p.Also, PARTFILT&per.FLT can be used for other purposes such as making not-
visible partitions visible or using multiple primary partitions&per. 

:p.:link reftype=hd refid=22.PARTFILT OPTIONS&per. :elink.
.br 

:h4 id=22 res=30021.Partfilt Options

:p.:hp2.PARTFILT OPTIONS&colon. :ehp2.

:p.:hp7.Options :ehp7.:hp7.Descriptions :ehp7.
.br 

.br 
:hp2./Q :ehp2.Load quietly 
.br 

.br 
:hp2./W :ehp2.Enables Writing to the faked partitions&per. Without this option the faked 
partitions are read-only&per. 
.br 

.br 
:hp2./A :ehp2. This option does two things&colon. 
.br 
Disables OS/2 to access all partitions, but&colon. 
.br 
Virtualizes (or fakes) all known partitions&per. Known partitions are the 
normal FAT partitions, IFS (=mainly HPFS) partitions and the partitions specified with 
the /P option&per. All primary partitions of known types are also virtualized, and 
will be accessable from OS/2&per. This option must be used in conjunction with the /
M option&per. When this option is specified, the /W option is automatically set, 
because otherwise OS/2 will not boot&per. 
.br 

.br 
:hp2./M <mountlist> :ehp2.Specifies the order in which partitions must be mounted&per. Must 
be used with the /M option&per. 

:p.:hp2.Warning&colon. :ehp2.Incorrect usage of the /A and /M options could make your 
system unbootable&per. 

:p.=> USING THE /A and /M OPTIONS is not advised! <= 
.br 

:p.if you need more information on these options, just read on&per. 
.br 

.br 
:hp2./P <partition types to fake> :ehp2.This is option is used to tell PARTFILT which 
partition type are to be faked&per. You should NOT use partition types already supported 
by OS/2 since this would result in a single partition being mounted two times&per. 
The list should consist of partition type numbers (in hexadecimal), separated by 
comma&apos.s&per. 
.br 

:p.:link reftype=hd refid=23.THE PARTITION TYPE NUMBERS :elink.
.br 

:h4 id=23 res=30022.Partition Type Numbers

:p.:hp2.THE PARTITION TYPE NUMBERS&colon. :ehp2.

:p.The partition type number are as follows&colon. 
.br 
  
.br 
     01   FAT12       (supported by OS/2) 
.br 
     02   XENIX_1 
.br 
     03   XENIX_2 
.br 
     04   FAT16       (supported by OS/2) 
.br 
     05   EXTENDED 
.br 
     06   HUGE        (supported by OS/2) 
.br 
     07   IFS         (supported by OS/2) 
.br 
     0A   BOOTMANAGER 
.br 
     0B   FAT32 
.br 
     0C   FAT32_XINT13 or FAT32X 
.br 
     0E   XINT13 
.br 
     0F   XINT13_EXTENDED 
.br 
     41   PREP 
.br 
     63   UNIX 
.br 
     83   LINUX 

:p.     10   Hidden partition (bits OR&apos.d with partition type) 
:p.To make PARTFILT&per.FLT fake a FAT32 partition the /P option should be /P 0B
&per. 
.br 
To make PARTFILT&per.FLT fake a FAT32X partition the /P option should be /P 0C
&per. 
.br 
To make PARTFILT&per.FLT fake a LINUX partition the /P option should be /P 83
&per. 
.br 
Or you can use a combination of the types e&per.g&per. /P 0B,0C 

:p.:link reftype=hd refid=24.HOW OS/2 SCANS PARTITIONS :elink.
.br 

:h4 id=24 res=30023.How OS/2 Scans Partitions

:p.:hp2.HOW OS/2 SCANS FOR PARTITIONS&colon. :ehp2.
.br 

:p.During the boot process of OS/2 partitions are scanned twice, each using the 
same algorithm to detect partitions and if OS/2 supports the found partition types 
drive letters are assigned to them&per. 

:p.The first scan takes place during initial boot&per. (PARTFILT has no effect 
on this scan!) The main purpose of this scan seems to be to detect the OS/2 boot 
drive and to assign a drive letter to it&per. For OS/2 to be able to boot this drive 
letter may NOT change later on during the second scan&per. 

:p.The second scan takes place while initializing the file system&per. Via calls 
to OS2DASD&per.DMD the partitions are scanned and drive letters are assigned&per. 
Only this second scan is influenced by PARTFILT&per. 

:p.So whatever you do, you must make sure that in both scans the OS/2 boot drive 
gets the same drive letters assigned&per. 

:p.If your FAT32 partition is not a primary partition and you don&apos.t care 
what drive letter the FAT32 partition gets you may stop reading here&per. Simply do 
not specify the /A or /M options and the FAT32 partition will get a drive letter 
higher then all partition normally recognized by OS/2&per. 

:p.But if your FAT32 partition is a primary partition, or you want the FAT32 
partition to have a drive letter before the OS/2 boot drive you will need to do some 
extra work&per. 

:p.Just for the record the normal assignment order of OS/2 is&colon. &per. 

:p. 1&per.Current active) Primary partition on first HD 
.br 
 2&per.Current active) Primary partition on second HD 
.br 
 3&per.and so on&per.&per.&per. 
.br 
 4&per.All extended partitions on first HD 
.br 
 5&per.All extended partitions on second HD 
.br 
 6&per.And so on&per.&per.&per. 
.br 
 7&per.Removeable drivesFAT32 
.br 

:p.Should you need to use the /M parameter with PARTFILT you should know that the 
sequence of numbers used as arguments are different from the normal order OS/2 uses&per. 
Here&apos.s where F32PARTS cFAT32omes along&per. F32PARTS shows the seq# as used by 
PARTFILT&per. See below&per.FAT32 

:p.Now suppose you have the following scenario&colon. 

:p.C&colon. is FAT16 (Primary) 
.br 
D&colon. is HPFS  (Extended)FAT32 

:p.and you consider converting the FAT16 partition to FAT32 (with partition 
magic) the following will happen after the conversion&colon. 

:p.During the first scan the FAT32 partition is skipped and the OS/2 boot drive 
will get C&colon. assigned&per. During the 2nd scan FAT32 is recognized because you 
have loaded PARTFILT and is assigned C&colon.&per. Your HPFS partition will get D
&colon. assigned and OS/2 will not boot because OS/2 has already decided to go for C
&colon. but cannot find its stuff there&per. 

:p.There are two solutions&colon. 

:p.Reinstall OS/2 on the HPFS partition without PARTFILT installed&per. OS/2 
will install everything on C&colon. (HPFS)&per. Later you could add PARTFILT and 
FAT32&per.IFS without the /A and /M options and the FAT32 partition will become D
&colon.&per. 

:p.Add a fake (preferably HPFS) partition between the FAT32 and HPFS partition
&per. During the 1st scan, this partition will get C&colon. and your boot partition 
will get D&colon.&per. 
.br 
Load PARTFILT with the /A and /M options, where in my example the mountlist 
should be&colon. /M 0,2&per. 
.br 

:p.Explanation&colon. 
.br 
With the Fake partition installed the following partitions exist&colon. 
.br 
0 - FAT32 
.br 
1 - fake HPFS 
.br 
2 - HPFS (boot) 

:p.By not specifying seq# 1 in the mountlist, PARTFILT will not virtualize this 
partition and OS/2 will not assign a drive to it&per. By using a HPFS partition, Windows 
95 will not recognize the fake partition and will not assign a drive letter to it
&per. 

:p.:link reftype=hd refid=25.ABOUT HIDDEN PARTITIONS :elink.
.br 

:h4 id=25 res=30024.About Hidden Partitions

:p.:hp2.ABOUT HIDDEN PARTITIONS&colon. :ehp2.

:p.PARTFILT can also be used to make hidden partitions visible to OS/2&per. Here 
the following mechanism is used&colon. 

:p.If /A is not used, only the types specified after /P are virtualized&per. (
The /A switch controls whether or not to virtualize all partitions&per.) 

:p.PARTFILT always unhides the partitions it virtualizes&per. 

:p.For the partition types PARTFILT virtualizes the following rules apply&colon. 

:p.Normal partition types (types 1, 4, 6, 7, but also 11, 14, 16 and 17) are 
reported to OS/2 with their actual -unhidden- partition type&per. 

:p.Any other partition types specified after /P are reported as un-hidden IFS 
partitions&per. 

:p.Any other partition types NOT specified after /P are reported as their actual 
-unhidden- type&per. Note that this will only happen if you use the /A argument
&per. 

:p.Keep in mind that if you specify /A you must also use the /M argument to tell 
PARTFILT which partitions you want to mount&per. 

:p.:link reftype=hd refid=26.F32PARTS&per.EXE :elink.
.br 

:h4 id=26 res=30025.F32PARTS.EXE (formerly DISKINFO.EXE)

:p.:hp2.F32PARTS&per.EXE (formerly DISKINFO&per.EXE)&colon. :ehp2.

:p.When run with no options, F32PARTS will scan and show all partitions&per. The 
following options are available&colon. 

:p.:hp7.Options :ehp7.:hp7.Descriptions :ehp7.

:p.:hp2./V :ehp2.Verbose mode&per. Show more info on FAT32 partitions&per. 

:p.:hp2./W :ehp2.Show the boot sector of FAT32 partitions&per. Only if /V is specified&per. 

:p.:hp2./P :ehp2.Allows you to specify a list of partition types that should also get a 
partition sequence number&per. 
.br 

:p. See :link reftype=hd refid=18.PARTFILT :elink.for more information&per. 

:p.:link reftype=hd refid=27.TWO EXAMPLES OF PARTFILT OUTPUT :elink.
.br 

:h4 id=27 res=30026.Two Examples of Partfilt Output

:p.:hp2.TWO EXAMPLES OF PARTFILT OUTPUT&colon. :ehp2.

:p.:link reftype=hd refid=28.EXAMPLE OF PARTFILT WITHOUT OPTIONS :elink.

:p.:link reftype=hd refid=29.EXAMPLE OF PARTFILT OUTPUT WITH /P 0B AS ARGUMENT :elink.
.br 

:h5 id=28 res=30027.Example of Partfilt Output without Options

:p.:hp2.EXAMPLE OF PARTFILT OUTPUT WITHOUT OPTIONS&colon. :ehp2.

:p.There are 2 disks 
.br 
=== Scanning physical disk 1&per.=== 
.br 
0&colon.P   06 HUGE           Strt&colon.H&colon.     1 C&colon.   0 S&colon.   1 End&colon.H&colon.   
127 C&colon. 258 S&colon.  63 
.br 
-&colon.PA  0A BOOTMANAGER    Strt&colon.H&colon.     0 C&colon. 524 S&colon.   1 End
&colon.H&colon.   127 C&colon. 524 S&colon.  63 
.br 
1&colon.LB  06 HUGE           Strt&colon.H&colon.     1 C&colon. 259 S&colon.   1 End&colon.H
&colon.   127 C&colon. 387 S&colon.  63 
.br 
2&colon.LB  07 IFS            Strt&colon.H&colon.     1 C&colon. 388 S&colon.   1 End&colon.H
&colon.   127 C&colon. 523 S&colon.  63 
.br 
=== Scanning physical disk 2&per.=== 
.br 
-&colon.L   0B FAT32          Strt&colon.H&colon.     1 C&colon.   1 S&colon.   1 End&colon.H
&colon.   127 C&colon. 381 S&colon.  63 
.br 
-&colon.L   0B FAT32          Strt&colon.H&colon.     1 C&colon. 382 S&colon.   1 End&colon.H
&colon.   127 C&colon. 524 S&colon.  63 
.br 

:cgraphic.
� ��� �
� ��� � Partition type (the number the specify after /P to get this
� ���                   partition type handled by PARTFILT)
� ����� H = Hidden partition
� ����� A = Active / B = Bootable via bootmanager
� ����� P = Primary / L = Logical (extended)
������� Seq # to be used in the OPTIONAL /M argument for PARTFILT

:ecgraphic.

:p.

:p.2 FAT32 partitions found! 
.br 
WARNING&colon. /P not specified&per. 
.br 
         Only &apos.normal&apos. partitions are assigned a partition sequence number! 

:p.:link reftype=hd refid=29.EXAMPLE OF PARTFILT OUTPUT WITH /P 0B AS ARGUMENT :elink.
.br 

:h5 id=29 res=30028.Example of Partfilt Output with /P 0B as Argument

:p.:hp2.EXAMPLE OF PARTFILT OUTPUT WITH /P 0B AS ARGUMENT&colon. :ehp2.

:p.Also including partition types 0B&per. 
.br 
There are 2 disks 
.br 
=== Scanning physical disk 1&per.=== 
.br 
0&colon.P   06 HUGE           Strt&colon.H&colon.     1 C&colon.   0 S&colon.   1 End&colon.H&colon.   
127 C&colon. 258 S&colon.  63 
.br 
-&colon.PA  0A BOOTMANAGER    Strt&colon.H&colon.     0 C&colon. 524 S&colon.   1 End
&colon.H&colon.   127 C&colon. 524 S&colon.  63 
.br 
1&colon.LB  06 HUGE           Strt&colon.H&colon.     1 C&colon. 259 S&colon.   1 End&colon.H
&colon.   127 C&colon. 387 S&colon.  63 
.br 
2&colon.LB  07 IFS            Strt&colon.H&colon.     1 C&colon. 388 S&colon.   1 End&colon.H
&colon.   127 C&colon. 523 S&colon.  63 
.br 
=== Scanning physical disk 2&per.=== 
.br 
3&colon.L   0B FAT32          Strt&colon.H&colon.     1 C&colon.   1 S&colon.   1 End&colon.H
&colon.   127 C&colon. 381 S&colon.  63 
.br 
4&colon.L   0B FAT32          Strt&colon.H&colon.     1 C&colon. 382 S&colon.   1 End&colon.H
&colon.   127 C&colon. 524 S&colon.  63 
.br 

:cgraphic.
� ��� �
� ��� � Partition type (the number the specify after /P to get this
� ���                   partition type handled by PARTFILT)
� ����� H = Hidden partition
� ����� A = Active / B = Bootable via bootmanager
� ����� P = Primary / L = Logical (extended)
������� Seq # to be used in the OPTIONAL /M argument for PARTFILT

:ecgraphic.

:p.

:p.2 FAT32 partitions found! 

:p.:link reftype=hd refid=28.EXAMPLE OF PARTFILT WITHOUT OPTIONS :elink.
.br 

:h3 id=30 res=30030.OS2DASD.F32

:p.:hp2.OS2DASD&per.F32&colon. :ehp2.

:p.This is a modified version of OS2DASD&per.DMD found in OS/2 and is an 
alternative for using PARTFILT&per.FLT&per. This modified driver also allows FAT32 
partitions to be recognized by OS/2&per. 

:p.:hp2.Note&colon.  :ehp2.This driver is NOT at the latest level and will probably NOT 
support the latest features like removable disks, etc&per.&per.&per. 

:p.:link reftype=hd refid=31.AVAILABILITY :elink.
.br 

:h4 id=31 res=30032.Availability

:p.:hp2.AVAILABILITY&colon. :ehp2.

:p.The OS2DAD&per.F32 driver will NO longer be distributed with the FAT32 IFS 
driver&per.  If needed, it can be extracted from Henk&apos.s latest binary release and 
manually installed entirely at the user&apos.s own risk&per. Henk Kelder&apos.s last 
release can be found on Hobbes at&colon. 

:p.:hp9.http&colon.//hobbes&per.nmsu&per.edu/pub/os2/system/drivers/filesys/os2fat32
&per.zip :ehp9.

:p.:link reftype=hd refid=32.INSTALLATION :elink.
.br 

:h4 id=32 res=30031.Installation

:p.:hp2.INSTALLATION&colon. :ehp2.

:p.:hp2.Note&colon.  :ehp2.This driver is obsolete with Daniela&apos.s DaniDASD&per.DMD 
being the better choice&per.  However, if one prefers this driver, it can be installed 
doing the following&per. 

:p.To install, rename you OS2DASD&per.DMD to something else, such as Old_os2dasd
&per.dmd&per. 
.br 

.br 
Copy the OS2DASD&per.F32 to the &bsl.OS2&bsl.Boot directory&per. 
.br 

.br 
Rename it OS2DASD&per.DMD&per. 
.br 

.br 
Reboot&per. 
:p.:link reftype=hd refid=31.AVAILABILITY :elink.
.br 
  
:h1 id=33 res=30077.Formatting FAT32 Volumes

:p.:hp2.Formatting FAT32 Volumes :ehp2.

:p.Formatting FAT32 can be done numerous ways&per.  Recently, the only way to 
format a volume FAT32 under either eComStation or OS/2 was to use DFSee and F32blank 
together&per. The procedure is as follows&colon. 

:ol.
:li.Find out the volume relevant data&colon. Heads, Sectors, Starting point 
and Size using DFSee 
:li.Feed F32Blank with that data to generate a file with blank FATs suitable 
for the volume 
:li.Detach the volume (with LVM it&apos.s called &apos.hide&apos. or similar) 
:li.Use DFSee to overwrite the volume with the file contents, using &osq.wrim
&osq.&per. 
:li.Attach again the volume&per. 
:li.If after this you can&apos.t read/write properly the volume or it appears 
as not empty, then you MUST reboot and check it again&per. 
:li.If you don&apos.t like DFSee go and find something else capable of doing 
the job&per. 
:eol. 

:p.If this is too complicated for some people, the USB media can also be formatted 
using one of the Window&apos.s versions&per. Each Windows version has it own built in 
limitations&per. 

:ul.
:li.Win95R2 <= 16 GB 
:li.Windows 98 second addition - Volumes < 128 GB and > 512 MB 
:li.Windows ME - 512 MB to 2 TB&per. 
:li.Windows XP <= 32 GB&per. 
:eul. 

:p.Other formatting alternatives 

:ul.
:li.FreeDOS <= 16 GB 
:li.Partition Commander versions 8 and 9 (Limitations unknown) 
:eul.

:p.Native OS/2 programs, allowing to format FAT32 disks:

:ul.
:li.Reformat, by Glassman (http&colon.//ru&per.ecomstation&per.ru/projects/disktools/?action=reformat)
:li.DFSee 8.x and later, by Jan van Wijk (http&colon.//www&per.dfsee&per.com/)
:eul.

:p.Also, except those, there is QSINIT, by _dixie_, which is able 
to format its ramdisks and hard disk partitions into FAT/FAT32/exFAT filesystems&per.

:p. Since FAT32&per.IFS 0&per.10, new :link reftype=hd refid=30048.FORMAT :elink. routine is supported&per. We ported to OS/2 a
Windows program called Fat32Format (http&colon.//www&per.ridgecrop&per.demon&per.co&per.uk/index&per.htm?fat32format&per.htm)&per. 
Now it is enhanced to support all FAT flavours supported, that is&colon. 
FAT12, FAT16, FAT32, EXFAT&per.
.br

:p. The procedure of formatting is straightforward&colon. you just use FORMAT&per.COM
with /FS&colon.<file system type>

.br
  
:p. Where <file system type> is one of the following&colon. FAT12|FAT16|FAT32|EXFAT

:p. Note that, by specifying the "FAT" file system type to FORMAT&per.COM,
you still can access IBM's standard FAT FORMAT routine, which supports checking for bad
sectors and formatting on physical level (our format supports only "quick"/"high-level" format
at the moment, but it has its own advantages too).

:p. Also, there is PMFORMAT, written by Alex Taylor&per. It is a PM frontend for FORMAT routine
of uunifat&per.dll. This frontend is included in ArcaOS&per.

:h1 id=34 res=30033.Installation/Deinstallation

:p.:hp2.INSTALL/DEINSTALL&colon. :ehp2.

:p.:link reftype=hd refid=35.WARPIN INSTALL&bsl.DEINSTALL :elink.
.br 

:h2 id=35 res=30034.WarpIn Install/Deinstall

:p.:hp2.WARPIN INSTALL&bsl.DEINSTALL&colon. :ehp2.

:p.:link reftype=hd refid=36.WARPIN INSTALL :elink.

:p.:link reftype=hd refid=38.WARPIN DEINSTALL :elink.
.br 

:h3 id=36 res=30035.WarpIN Installation

:p.:hp2. WARPIN INSTALLATION OF FAT32 DRIVER&colon. :ehp2.  

:p.The latest version of WarpIN is required and can be downloaded from&colon. 

:lines align=center.
.br
.br
:hp2.http&colon.//www&per.xworkplace&per.org:ehp2.
.br 
:elines.

:p. 1&per.Install WarpIN if not already installed&per. 

:p. 2&per.If using a LVM system, create a compatibility volume and assign it a 
drive letter&per. 

:p. 3&per.Go to the location where you downloaded the *&per.wpi file&per. 

:p. 4&per.Double click to start the installation&per. 

:p. 5&per.When installation is completed, the installation will have copied the 
following files to your OS/2 partition&colon. 

:p.:hp2.Note&colon.  :ehp2.The placement of files is not relevant to install/deinstall&per. 
The &per.wpi will take care of that&per. The default directories will be BIN, BOOK, 
DLL, and BOOT under <ESES_dir> if the base ESE is installed, otherwise the defaults 
will be ?&colon.&bsl.OS2, ?&colon.&bsl.OS2&bsl.BOOK, ?&colon.&bsl.OS2&bsl.DLL, and ?
&colon.&bsl.OS2&bsl.BOOT&per. If you choose other paths, WarpIN will add them to your 
PATH/LIBPATH, etc&per.&per. 

:p.Possible directories of file placement&colon. 

:p.:hp7.Files :ehp7.:hp7.Destination :ehp7.
.br 

.br 
CACHEF32&per.EXE &bsl.TOOLS&bsl.SYSTEM&bsl.BIN 
.br 
DISKDUMP&per.EXE &bsl.TOOLS&bsl.SYSTEM&bsl.BIN 
.br 
F32CHK&per.EXE &bsl.TOOLS&bsl.SYSTEM&bsl.BIN 
.br 
F32MON&per.EXE &bsl.TOOLS&bsl.SYSTEM&bsl.BIN 
.br 
F32MOUNT&per.EXE &bsl.TOOLS&bsl.SYSTEM&bsl.BIN 
.br 
F32PARTS&per.EXE &bsl.TOOLS&bsl.SYSTEM&bsl.BIN 
.br 
F32STAT&per.EXE &bsl.TOOLS&bsl.SYSTEM&bsl.BIN 
.br 
FAT32CHK&per.EXE &bsl.TOOLS&bsl.SYSTEM&bsl.BIN 
.br 
FAT32FMT&per.EXE &bsl.TOOLS&bsl.SYSTEM&bsl.BIN 
.br 
FAT32SYS&per.EXE &bsl.TOOLS&bsl.SYSTEM&bsl.BIN 
.br 
QEMU-IMG&per.EXE &bsl.TOOLS&bsl.SYSTEM&bsl.BIN 
.br 

.br 
OS2DASD&per.F32 &bsl.TOOLS&bsl.SYSTEM&bsl.BOOT 
.br 
PARTFILT&per.FLT &bsl.TOOLS&bsl.SYSTEM&bsl.BOOT 
.br 
FAT32&per.IFS &bsl.TOOLS&bsl.SYSTEM&bsl.BOOT 
.br 
LOOP&per.ADD &bsl.TOOLS&bsl.SYSTEM&bsl.BOOT 
.br 

.br 
QEMUIMG&per.DLL &bsl.TOOLS&bsl.SYSTEM&bsl.DLL 
.br 
UEXFAT&per.DLL &bsl.TOOLS&bsl.SYSTEM&bsl.DLL 
.br 
UFAT12&per.DLL &bsl.TOOLS&bsl.SYSTEM&bsl.DLL 
.br 
UFAT16&per.DLL &bsl.TOOLS&bsl.SYSTEM&bsl.DLL 
.br 
UFAT32&per.DLL &bsl.TOOLS&bsl.SYSTEM&bsl.DLL 
.br 
UUNIFAT&per.DLL &bsl.TOOLS&bsl.SYSTEM&bsl.DLL 
.br 

.br 
FAT32&per.INF &bsl.TOOLS&bsl.SYSTEM&bsl.BOOK 
.br 
IFS&per.INF &bsl.TOOLS&bsl.SYSTEM&bsl.BOOK 
.br 

.br 
FAT32&per.TXT &bsl.TOOLS&bsl.SYSTEM&bsl.DOCS
.br 

.br 
FAT32&per.KOR &bsl.OS2&bsl.SYSTEM (Only if you chose to install 
Korean support) 
.br 

.br 
TRC00FE&per.TFF &bsl.OS2&bsl.SYSTEM&bsl.TRACE (Only if you need system trace files for debugging)
.br 

.br 
COUNTRY&per.KOR &bsl.OS2&bsl.SYSTEM  (Only if you chose to install Korean support
) 
.br 

.br 
CACHEF32&per.CMD &bsl.TOOLS&bsl.SYSTEM&bsl.BIN (Only if you chose to install 
Korean support) 

:p.WarpIN also adds the following lines to the config&per.sys file&colon. 

:p.IFS=X&colon.&bsl.TOOLS&bsl.SYSTEM&bsl.BIN&bsl.FAT32&per.IFS /cache&colon.2048 /h /q /ac&colon.* /largefiles
.br 
CALL=X&colon.&bsl.TOOLS&bsl.SYSTEM&bsl.BIN&bsl.CACHEF32&per.exe /f /p&colon.2 /m&colon.50000 /b&colon.250 /d&colon.5000

:p.Where X&colon. is the partition that the IFS will be installed to&per. 

:p.:hp2.Note&colon.  :ehp2.WarpIN makes a backup of your config&per.sys file to your root 
directory&per. If the system has problems booting up after installation, you can always 
boot to a command prompt using the boot recovery menu&per. Replace the modified 
config&per.sys file with the backup that was made during the fat32 install to get you 
back up and running again&per. 

:p. 6&per.Reboot 
.br 

:p.:link reftype=hd refid=38.WARPIN FAT32 DEINSTALLATION :elink.
.br 
  
:h3 id=37 res=37930.Korean Support for non-Korean Systems

:p.:hp2.KOREAN SUPPORT for NON-KOREAN SYSTEMS&colon. :ehp2.

:p.Besides copying the standard FAT32 files to the selected location(s) and 
making the default config&per.sys entries (See WARPIN INSTALLATION OF FAT32 DRIVER), 
it will install COUNTRY&per.KOR but NOT point the config&per.sys to use it&per. 
You have to do so manually, and in case of problems, the fallback Alt+F1,F2 
solution will still point to the original COUNTRY&per.SYS and thus will be safe&per. See :link reftype=hd refid=20041.
FILES IN THIS VERSION):elink.&per. It also 

:p.adds a Korean text dealing with FAT32 information&per. 

:p.copies a CACHEF32&per.CMD file to the hard drive which contains the 
following script&colon. 

:p.-chcp 949 

:p.-CACHEF32&per.EXE /y   

:p.adds the following entry to the startup&per.cmd file found in the root 
directory 

:p.-cmd /C CACHEF32&per.CMD 

:p.:hp2.Note&colon.  :ehp2.The reason cmd /c is used is because 4os2 does not support CHCP 
correctly&per. 
.br 

:p.:hp2.IMPORTANT!  :ehp2.If you install Korean support for non-Korean systems, you&apos.ll 
have to manually edit your CONFIG&per.SYS and change the COUNTRY settings from 

:p.COUNTRY=xyz,?&colon.&bsl.OS2&bsl.SYSTEM&bsl.COUNTRY&per.SYS 

:p.to 

:p.COUNTRY=xyz,<Install Path>&bsl.COUNTRY&per.KOR 

:p.where you have to substitute xyz and <Install Path> with appropriate values
&per. In case of problems, the fallback Alt+F1,F2 solution will still point to the 
original COUNTRY&per.SYS and thus will be safe&per. See :link reftype=hd refid=20041.&osq.Files in this version&osq.:elink.
&per. 

:p.:hp2.Note&colon.  :ehp2.Korean filenames are displayed correctly only in CP949, but 
filenames are manipulated correctly regardless of current codepage&per.   
:h3 id=38 res=30036.WarpIN Deinstallation

:p.:hp2.DEINSTALLATION USING WARPIN&colon. :ehp2.  

:p.:font facename='Helv' size=14x14.:hp2.If the Fat32 IFS was installed using WarpIN, use WarpIN to uninstall it and 
its associated files:ehp2.:font facename=default.:hp2.&per. :ehp2.

:p.:hp2.To Uninstall&colon. :ehp2.

:p. 1&per.Start WarpIN 

:p. 2&per. Under the Application column, find the :hp2.IFS:ehp2.&per. Check the Vendor 
column and make sure it states &osq.:hp2.Netlabs:ehp2.&osq.&per. 

:p. 3&per.Select&per.  The WarpIN data base should show the following information
&colon. 

:p.:hp2.Note&colon.  :ehp2.The number of files to uninstall will vary depending on the FAT32 
version installed and if Korean support was installed&per. 

:p.:hp2.List :ehp2.:hp2.Information :ehp2.
.br 
Author&per. Netlabs&per. 
.br 
Version number&colon. 0&per.0&per.99&per.2 
.br 
Installation date&colon. The date you installed it Target directory&colon.   <
drive>&colon.&bsl.OS2 
.br 
Files&colon. 8 

:p. 4&per.Right click on &osq.:hp2.IFS:ehp2.&osq.&per. 

:p. 5&per.Select &osq.:hp2.Deinstall package&per.&per.&per.:ehp2.&osq.&per.  A message box 
should come up listing what will be deleted and undone&per. 

:p. 6&per.Select &osq.:hp2.OK:ehp2.&osq. 
.br 

:p.:hp2.Note&colon.  :ehp2.WarpIN sometimes has problems deleting the added config&per.sys 
entries&per.  This usually happens after you install over a previous version&per. If 
this happens, you will have to manually edit your config&per.sys file and remove the 
following entries&per. 

:p.:hp2.IFS=X&colon.&bsl.TOOLS&bsl.SYSTEM&bsl.BOOT&bsl.FAT32&per.IFS /Q 
.br 
CALL=X&colon.&bsl.TOOLS&bsl.SYSTEM&bsl.BIN&bsl.CACHEF32&per.EXE :ehp2.:font facename=default.
.br 

:p.:link reftype=hd refid=36.WARPIN INSTALLATION FAT32 :elink.
.br 

:h2 id=39 res=30037.Manual Mangling

:p.:hp2.MANUAL MANGLING&colon. :ehp2.

:p.If archived in zip form, unzip the files to a temporary directory&per. If in 
WarpIN format, do the following&colon. 

:p.Copy the *&per.wpi file to a temporary directory&per.  Copy the files &osq.wic
&per.exe&osq. and &osq.wpirtl&per.dll&osq. from your WarpIN directory to the 
temporary directory where you copied the *&per.wpi file&per. To extract the fat32 files, 
open a command prompt, change to the temporary directory, and type the following 
command&colon. 

:p.wic&per.exe *&per.wpi -x (x is lowercase)&per. 

:p.Replace the *&per.wpi with the name of the fat32 wpi file&per. 

:p.After the files have been extracted, do the following&colon. 

:p.:hp2.Note&colon.  :ehp2.If you installed the FAT32 driver version 0&per.96 or higher 
using WarpIN and now wish to install the update manually, check your WarpIN database 
to see where each file is located&per. Copy the files to the locations (
directories) indicated in the WarpIN database&per. 

:p.If you installed the FAT32 driver versions 0&per.95 or earlier either 
manually or using WarpIN, copy the following files to your &bsl.OS2 directory&colon. 

:p.CACHEF32&per.EXE 
.br 
DISKDUMP&per.EXE 
.br 
F32CHK&per.EXE 
.br 
F32MON&per.EXE 
.br 
F32MOUNT&per.EXE 
.br 
F32PARTS&per.EXE 
.br 
F32STAT&per.EXE 
.br
FAT32CHK&per.EXE
.br
FAT32FMT&per.EXE
.br
FAT32SYS&per.EXE
.br
QEMU-IMG&per.EXE
.br

:p.Copy the following files to your &bsl.OS2&bsl.DLL directory&colon. 
 
:p.UUNIFAT&per.DLL / UFAT12&per.DLL / UFAT16&per.DLL/ UFAT32&per.DLL / UEXFAT&per.DLL / QEMUIMG&per.DLL

:p.Copy the following files to your &bsl.OS2&bsl.BOOT&colon.

:p.FAT32&per.IFS

:p.LOOP&per.ADD
.br

:p.Copy documentation files to your &bsl.OS2&bsl.DOCS&bsl.FAT32 directory&colon.
.br

:p.Copy *&per.INF files to your &bsl.OS2&bsl.BOOK directory&colon.
.br
IFS&per.INF
.br
FAT32&per.INF
.br

:p.There is a document for Korean based on version 0&per.94 document&per.  This 
is not needed unless you are Korean&per. 
 
:p.FAT32&per.KOR 

:p.The following file is a patched version of the &osq.country&per.sys&osq. from 
WSEB fixpack 2&per.  Strangely, &osq.country&per.sys&osq. of WSeB contains the wrong 
information for Korean, especially DBCS lead byte infos&per.  Without this, Korean cannot 
use filenames consisting of Korean characters&per.  Replacement the country&per.sys 
file found on Korean systems is a must for the FAT32&per.IFS to work correctly&per.  
On non-Korean systems, replacement of the country&per.sys is not necessary, but 
can be done if the installer wants without any harm to their systems&per. To use, 
rename the &osq.country&per.sys&osq. found in OS2&bsl.SYSTEM directory and replace it 
with the &osq.country&per.kor&osq., renaming it &osq.country&per.sys&osq.&per. 
 
:p.COUNTRY&per.KOR 
.br 

:p.This script adds Korean CODEPAGE support for non-Korean systems&per. Copy it 
to a location listed in the PATH statement of your config&per.sys&per.  Add its 
location to the STARTUP&per.CMD found in your root directory&per.  If you have no STARTUP
&per.CMD, make one&per. 
 
:p.CACHEF32&per.CMD 

:p.Make the following changes to the CONFIG&per.SYS&colon. 

:p.IFS=x&colon.&bsl.OS2&bsl.BOOT&bsl.FAT32&per.IFS [:link reftype=hd refid=43.options:elink.] 
 
:p.(Install this one anywhere AFTER IFS=HPFS&per.IFS) 

:p.CALL=x&colon.&bsl.OS2&bsl.CACHEF32&per.EXE [:link reftype=hd refid=46.options:elink.] 

:p.:hp2.Note&colon.  :ehp2.Make sure this is a CALL and NOT a RUN&per. 

:h1 id=40 res=30038.FAT32 IFS Components and Switches

:p.:hp2.FAT32 IFS COMPONENTS AND SWITCHES&colon. :ehp2.

:ul.

:li.:link reftype=hd refid=20041.FILES IN THIS VERSION :elink.

:li.:link reftype=hd refid=20042.FAT32&per.IFS :elink.

:li.:link reftype=hd refid=20043.LOOP&per.ADD :elink.

:li.:link reftype=hd refid=20045.CACHEF32&per.EXE :elink.

:li.:link reftype=hd refid=20047.UUNIFAT&per.DLL and forwarders :elink.

:li.:link reftype=hd refid=20055.QEMUIMG&per.DLL :elink.

:li.:link reftype=hd refid=20048.FAT32FMT&per.EXE :elink. 

:li.:link reftype=hd refid=20049.FAT32CHK&per.EXE :elink. 

:li.:link reftype=hd refid=20050.FAT32SYS&per.EXE :elink. 

:li.:link reftype=hd refid=20051.F32CHK&per.EXE :elink. 

:li.:link reftype=hd refid=20052.F32MON&per.EXE :elink.

:li.:link reftype=hd refid=20053.F32STAT&per.EXE :elink. 

:li.:link reftype=hd refid=20054.F32MOUNT&per.EXE :elink. 

:eul.

:h2 id=20041 res=30039.Files in this Version

:p.:hp2.FILES IN THIS VERSION&colon. :ehp2.

:p.:hp7.Files :ehp7.:hp7.Description :ehp7.

:p.:hp2.FAT32&per.INF :ehp2.This file&per. 

:p.:hp2.IFS&per.INF :ehp2.Updated IBM&apos.s IFS document&per. 

:p.:hp2.FAT32&per.TXT :ehp2.Fat32 information in plain ASCII txt format (may be outdated)
&per. 

:p.:hp2.:link reftype=hd refid=20042.FAT32&per.IFS :elink.:ehp2.The actual IFS&per. 

:p.:hp2.:link reftype=hd refid=20043.LOOP&per.ADD :elink.:ehp2.The disk image block device driver&per. 

:p.:hp2.:link reftype=hd refid=18.PARTFILT&per.FLT:elink.:ehp2.Partition support filter (non-LVM systems, only)&per. 

:p.:hp2.:link reftype=hd refid=20045.CACHEF32&per.EXE :elink.:ehp2.The cache helper program&per. 

:p.:hp2.:link reftype=hd refid=20047.UUNIFAT&per.DLL and forwarders :elink.:ehp2.Modules needed to run CHKDSK/FORMAT/SYS on FAT32 partition&per. 

:p.:hp2.:link reftype=hd refid=20055.QEMUIMG&per.DLL :elink.:ehp2.Disk images support (ported from QEMU)&per. 

:p.:hp2.:link reftype=hd refid=20051.F32CHK&per.EXE :elink.:ehp2. A boot disk autocheck helper&per. 

:p.:hp2.:link reftype=hd refid=20053.F32STAT&per.EXE :elink.:ehp2. A program to change the DIRTY flag of FAT32 partitions&per. 

:p.:hp2.:link reftype=hd refid=20054.F32MOUNT&per.EXE :elink. :ehp2. A mounter program for file system images&per.

:p.:hp2.:link reftype=hd refid=20052.F32MON&per.EXE :elink.:ehp2. (Formerly MONITOR&per.EXE) A program to monitor what FAT32
&per.IFS is doing&per. 

:p.:hp2.:link reftype=hd refid=26.F32PARTS&per.EXE :elink.:ehp2.(Formerly DISKINFO&per.EXE) A diagnostic program that will 
scan for and show all partitions&per. 

:p.:hp2.:link reftype=hd refid=20048.FAT32FMT&per.EXE :elink.:ehp2.Disk formatter standalone version&per. 

:p.:hp2.:link reftype=hd refid=20049.FAT32CHK&per.EXE :elink.:ehp2.Disk checker standalone version&per. 

:p.:hp2.:link reftype=hd refid=20050.FAT32SYS&per.EXE :elink.:ehp2.Boot loader installer standalone version&per. 

:p.:hp2.* FAT32&per.KOR :ehp2. Is a document for Korean based on version 0&per.94 document
&per. 

:p.:hp2.* COUNTRY&per.KOR :ehp2. A patched version of the &osq.country&per.sys&osq. from 
WSEB fixpack 2&per. Strangely, &osq.country&per.sys&osq. of WSeB contains the wrong 
information for Korean, especially DBCS lead byte infos&per. Without this, Korean cannot 
use filenames consisting of Korean chars&per.  To use, rename the &osq.country&per.
sys&osq. found in OS2&bsl.SYSTEM directory and replace it with the &osq.country
&per.kor&osq., renaming it &osq.country&per.sys&osq.&per. 

:p.:hp2.* CACHEF32&per.CMD :ehp2.Script for loading Korean Codepage and CACHEF32&per.EXE 
.br 

:h2 id=20042 res=30040.FAT32.IFS

:p.:hp2.FAT32&per.IFS&colon. :ehp2.

:p.FAT32&per.IFS is the actual Installable File Systems (IFS) driver&per. It is 
installed by specifying the path and complete file name of the file system driver in your 
CONFIG&per.SYS file like this &colon.   
:cgraphic.


IFS =  ������������������������� filename ����������������´
         �� drive �� �� path ��             �� arguments ��


:ecgraphic.

:p.:hp2.Examples :ehp2.

:p.The defaults (i&per.e&per.e no parameters specified ) are a cache of 1024KB 
and no messages&per. 

:p.IFS=X&colon.&bsl.TOOLS&bsl.SYSTEM&bsl.BIN&bsl.FAT32&per.IFS /Q 

:p.

:p.To set a cache of to 2048KB with Extended Attributes support, type the 
following in your CONFIG&per.SYS and restart your system&per. 
:p.IFS=X&colon.&bsl.TOOLS&bsl.SYSTEM&bsl.BIN&bsl.FAT32&per.IFS  /cache&colon.2048 
/EAS 

:p.:hp2.Note&colon.  :ehp2.If no FAT32 partition is found on system startup, the driver will 
not load&per. 

:p.:link reftype=hd refid=43.FAT32&per.IFS Options :elink.
.br 

:h3 id=43 res=30041.FAT32 Options

:p.:hp2.FAT32 IFS OPTIONS&colon. :ehp2.  

:p.

:p.:hp7.Options :ehp7.:hp7.Description :ehp7.

:p.:hp2./Q :ehp2.Quiet, no messages&per.   

:p.:hp2./H :ehp2.Allocates cache memory into high memory (>16MB)&per. Option added starting 
with version 0&per.99&per.   

:p.:hp2./CALCFREE :ehp2.Because FAT32 always calculates free space on mount of USB 
removable media, recognition of the media was taking a long time&per.  The default now for 
the FAT32 driver is not to calculate free space unless this option is enabled or if 
free space info was not stored on disk&per.   

:p.:hp2./CACHE&colon.nnnn :ehp2.Specifies the cache size in kilobytes&per. If omitted the 
default cache size is 1024KB&per. Maximum cache size is 2048KB&per. *   

.br 

:p.:hp2.Note&colon.  :ehp2.Cache memory is allocated as FIXED memory, so if you have less 
than 16MB a setting of 512KB or less for this option is suggested&per.

:p.:hp2./AUTOCHECK&colon.<drive letter list> :ehp2.Specifies the drive letters to check on boot&per. If /AUTOCHECK&colon.* 
is specified, CHKDSK will check all FAT32 disks for dirty flag present, and if it is, CHKDSK is run on that drive&per.
The drive letters can be specified also one by one, like this: /AUTOCHECK&colon.cdwx&per. Also, there's the possibility
to force chkdsk run on boot, regardless of the dirty flag, by specifying the plus sign after the drive letter&colon. 
/AUTOCHECK&colon.cdf+h&per. So that, disk f&colon. will be checked forcibly&per.

:p.:hp2./AC&colon.<drive letter list> :ehp2.The same as /AUTOCHECK&colon.<drive letter list>&per.

:p.:hp2./LARGEFILES :ehp2.Large files support&colon. Support files of size >2 GB&per. It is off, by default&per.
Note that the file size limit for FAT32 set by Microsoft is 4GB (as file size is an unsigned long integer)&per. But 16-bit
IFS drivers have another limit of 2 GB per file, because file position is signed long integer. Starting from OS/2 Warp Server
for e-Business, IBM introduced support for files > 2 GB with 64-bit file position&per. So, the file size could be almost 
unlimited, theoretically&per. This switch defines, whether to use the support for large files or not&per. If this switch is
specified, but large file API is unavailable, the large file support forced to be disabled. This prevents trying to use
the missing 64-bit fields for file size and position on older systems&per.

:p.:hp2./FAT[&colon.[-]<drive letter list>] :ehp2.Enable FAT12/FAT16 support&per. You can optionally specify the drive
letters which are needed to mount&per. (Like /fat&colon.abcd or /fat&colon.* or /fat&colon.-abcd)&per. Minus denotes that
the following drive letters should be disabled, so this is a "blacklist"-type mask&per.

:p.:hp2./FAT32&colon.[-]<drive letter list> :ehp2.Enable FAT32 support for a list of drive letters&per. Minus denotes that
the following drive letters should be disabled, so this is a "blacklist"-type mask&per.

:p.:hp2./EXFAT[&colon.[-]<drive letter list>] :ehp2.Enable exFAT support&per. You can optionally specify the drive
letters which are needed to mount&per. (Like /exfat&colon.abcd or /exfat&colon.* or /exfat&colon.-abcd)&per. There is a support for exFAT
filesystems&per. (beta-quality)&per. Minus denotes that the following drive letters should be disabled, so this is a 
"blacklist"-type mask&per.

:p.:hp2./PLUS :ehp2. Enable :link reftype=hd refid=200702.Support for large files, bigger than 4 GB (FAT+) :elink.

:p.:hp2.Note&colon.  :ehp2.If /FAT or /EXFAT or /FAT32 switches are not specified, all FAT32 disks are mounted
by default&per. FAT12/FAT16 should work on any media&per. Floppies can be mounted too&per. Also, so far, we tested
a CD with FAT16 filesystem burned on it, it works too&per. We just created the test filesystem with QSINIT, with 
2048 bytes per sector (QSINIT supports such an option), then saved it as an image and burned it as a standard
ISO image&per. Also, virtual floppy or hard disk should work too&per. So far, the following drivers were successfully
tested with fat32.ifs&colon. VDISK&per.SYS by IBM (included into each OS/2 system), VFDISK&per.SYS by Daniela Engert and Lars
Erdmann, and SVDISK&per.SYS by Albert J. Shan, HD4DISK&per.ADD by _dixie_ (included with QSINIT boot loader)&per. The latter
is successfully tested both with FAT32 PAE ramdisks and with EXFAT ramdisks in lower memory&per.

:p.:hp2./MONITOR :ehp2.Set F32MON ON by default&per. If omitted F32MON is OFF&per. See :link reftype=hd refid=20052.
F32MON&per.EXE :elink.for more information&per. 

:p.:hp2./RASECTORS&colon.n :ehp2.Read Ahead Sectors&per. Specifies the minimum number of 
sectors to be read per read action and placed in the cache&per. If omitted the default 
differs per volume and equals 2 times the number of sectors per cluster&per. The 
maximum threshold value used is 4 times the number of sectors per cluster&per. The 
maximum value of RASECTORS has been changed from 4 times the number of sectors per 
cluster to 128 starting with version 0&per.96&per. 

:p.:hp2.Note&colon.  :ehp2.The actual sector IO per read action is NOT determined by an 
application, but by the IFS&per. For FAT access, single sector reads are done&per. For 
Directory and Files IO reads are done on a cluster base&per. By setting the RASECTORS, 
you can define the minimum number of sectors the IFS will read from disk and place 
in the cache&per. 

:p.:hp2./EAS :ehp2.Make :link reftype=hd refid=20042.FAT32&per.IFS :elink.support :link reftype=hd refid=62.EXTENDED ATTRIBUTES :elink.

:p.:hp2./READONLY :ehp2. Set file system read only, if it's dirty&per. This is the alternative 
behaviour (was the default one in older FAT32&per.IFS versions)&per. From now, by default, 
file system access is denied, until it is checked&per. Instead of checking the file system, 
you can force it clean with the :link reftype=hd refid=20053.F32STAT&per.EXE :elink. utility&per.

:p.:hp2.IMPORTANT&colon.  :ehp2.:hp2.Starting with version 0&per.97, CHKDSK /F must first be run 
on each FAT32 partition before adding EA support to the FAT32 IFS driver&per. This 
step is required starting with this version due to the EA mark byte being changed 
for compatibility with WinNT family&per. 

:p.Starting with version 0&per.98, EA support is turned OFF by default in WarpIN 
installations&per. 

:p.You can use F32STAT&per.EXE to run CHKDSK automatically on next boot by doing 
the following&per. 

:p.F32STAT&per.EXE x&colon. /DIRTY 

:p.Where, x&colon. is your FAT32 drive which has files with EA&per. 

:p.F32STAT&per.EXE supports only one drive at one time&per. So if you want to 
change the state of one more drive, you should run F32STAT&per.EXE, respectively&per. 
i&per.e&per., FAT32 drive is C&colon. D&colon.&per. You should run F32STAT&per.
EXE two times for C and D as the following&per. 

:p.F32STAT&per.EXE C&colon. /DIRTY 
.br 
F32STAT&per.EXE D&colon. /DIRTY :ehp2.

:p.
.br 
  
:p.:link reftype=launch object='e.exe' data='e.exe config.sys'.:hp3.EDIT YOUR CONFIG&per.SYS :ehp3.:elink.  

:p.:link reftype=hd refid=44.FREE SPACE :elink.
.br 

:h3 id=44 res=30042.Free Space

:p.:hp2.FREE SPACE&colon. :ehp2.

:p.On most FAT32 drives the amount of free space is stored&per. FAT32&per.IFS 
will only redetermine the amount of free space if&colon. 

:p.The disk was marked dirty on boot&per. 
.br 
The free space is set to -1 on the disk&per. 
.br 
The free space is not available on the disk&per. 
.br 

:p.FAT32&per.IFS will internally keep track of the free space and update it on 
disk on shutdown&per. 

:h2 id=20043 res=30046.LOOP&per.ADD

:p.:hp2.LOOP&per.ADD&colon. :ehp2.

:p.LOOP&per.ADD is the block device driver (Adapter Device Driver) for mounting disk images
laying on an another file system&per. It allows to mount a disk image with any available
IFS (besides FAT32&per.IFS)&per. So, after attaching the image with :link reftype=hd refid=20054.F32MOUNT&per.EXE
:elink., a block device appears in the system, which gets a drive letter assigned and then
the kernel calls all IFS FS_MOUNT entry points, until it gets mounted&per.

:p.LOOP&per.ADD contains the same functionality that FAT32&per.IFS contains, needed to mount
disk images&per. For successful image mount, you need a running instance of :link reftype=hd 
refid=20045.CACHEF32&per.EXE :elink. which loads :link reftype=hd refid=20055.QEMUIMG&per.DLL 
:elink. to mount disk images&per. So that, as in FAT32&per.IFS case, we have CACHEF32&per.EXE
running a loop, polling LOOP&per.ADD with a "GET_REQ" ioctl. If an  OPEN/CLOSE/READ/WRITE 
command is available, it executes it and returns back into the driver with an "DONE_REQ" 
ioctl&per. An actual file system is mounted to a block device served by LOOP&per.ADD&per. 

:p.:hp2.Syntax&colon. :ehp2.

:cgraphic.


BASEDEV =  �� LOOP.ADD ����������������´
                         �� arguments ��


:ecgraphic.

:p.:hp7.Options :ehp7.:hp7.Description :ehp7.

:p.:hp2./UNITS&colon.<number of units> :ehp2.Number of units to serve&per.   

:p.This specifies how many images can be mounted at once&per. For each image a block device
should be allocated&per. The number of block devices is fixed at the moment of loading the 
driver&per.

:p.:hp2.Examples :ehp2.

:cgraphic.

BASEDEV=LOOP.ADD /UNITS&colon.3

:ecgraphic.

:p.
:h2 id=20045 res=30043.CACHEF32.EXE

:p.:hp2.THE PURPOSE OF CACHEF32&per.EXE :ehp2.

:p.CACHEF32&per.EXE is a helper program with the following functions&colon. 

:ul.

:li.Check DISK state on boot, run CHKDSK if needed (but since FAT32&per.IFS 
versiom 0&per.10, CHKDSK is no more run from CACHEF32&per.EXE&per. It is run
from the IFS FS_INIT routine, as it should be)&per.
:li.Start the LAZY WRITE daemon&per. 
:li.Set CACHE and READ-AHEAD parameters&per. 
:li.Set Longname behavior&per. 
:li.Load a CP to UNICODE translate table for longnames and the default codepage
&per. (Unicode translate table loading is moved to :link reftype=hd refid=20047.
UUNIFAT&per.DLL :elink.&per. Now CACHEF32&per.EXE just calls corresponding function
in this DLL&per.)
:li.CACHEF32&per.EXE also runs a special helper thread executing a loop polling the IFS 
for OPEN/READ/WRITE/CLOSE commands&per. So, this thread executes these commands and returns 
results back to the IFS&per. This is required to support reading/writing sectors from/to 
disk images laying on another file system&per. For reading the disk images, CACHEF32&per.EXE 
uses :link reftype=hd refid=20055.QEMUIMG&per.DLL :elink., which contains the disk image 
access code ported from QEMU (http&colon.//www&per.qemu&per.org/)&per.
:li.CACHEF32&per.EXE is also running another thread, executing a second loop polling
the :link reftype=hd refid=20043.LOOP&per.ADD :elink. for OPEN/READ/WRITE/CLOSE requests&per.
So, this thread executes these commands and returns results back to the driver&per.
This is required to support reading/writing sectors from/to disk images laying on 
another file system&per. For reading the disk images, CACHEF32&per.EXE uses 
:link reftype=hd refid=20055.QEMUIMG&per.DLL :elink., which contains the disk 
image access code ported from QEMU (http&colon.//www&per.qemu&per.org/)&per.
:li.CACHEF32&per.EXE also forces all FAT12/FAT16 drives to be remounted on startup&per.
This is needed to cause these drives remounted by FAT32&per.IFS, instead of the
in-kernel FAT driver&per. This is to avoid manual remount each time&per.
:li.CACHEF32&per.EXE also starts F32MOUNT&per.EXE with "/a" parameter, to automatically
mount all images specified in d&colon.\os2\boot\fstab&per.cfg on startup, and also starts
F32MOUNT&per.EXE with "/u" parameter on exit, to automatically unmount all images&per.

:eul. 

:p.When run in the foreground and CACHEF32 is already running, it displays the 
CACHE parameters and allows you to modify the values&per. If no other copy of 
CACHEF32 is running, it detaches a background copy&per. 

:p.When run in the background (detached), CACHEF32 will act as lazywrite daemon
&per. :font facename=default.  

:p.:link reftype=hd refid=46.See CACHEF32 Options :elink.
.br 

:h3 id=46 res=30067.CACHEF32 Options

:p.:hp2.CACHEF32 OPTIONS&colon. :ehp2.  

:p.:hp7.Options :ehp7.:hp7.Description :ehp7.

:p.:hp2./? :ehp2.Shows help&per. 

:p.:hp2./Q :ehp2.Terminates CACHEF32&per. CACHEF32 will be unloaded from memory, lazy 
writing will stop&per. (Performance will degrade)&per. This option is ultilized from 
the command prompt&per. 

:p.:hp2./N :ehp2.Runs CACHEF32 without starting the deamon in the background&per. 

:p.:link reftype=fn refid=fn78.:hp2./D&colon.nn :ehp2.:elink.Sets the DISKIDLE value&per. See OS/2 online help on CACHE&per. 

:p.:link reftype=fn refid=fn79.:hp2./B&colon.nn :ehp2.:elink.Sets the BUFFERIDLE value&per. See OS/2 online help on CACHE&per. 

:p.:link reftype=fn refid=fn80.:hp2./M&colon.nn :ehp2.:elink.Sets the MAXAGE value&per. See OS/2 online help on CACHE&per. 

:p.:hp2./R&colon.d&colon.n :ehp2.Set RASECTORS for drive d&colon. to n&per. 

:p.:link reftype=fn refid=fn81.:hp2./L&colon.ON|OFF :ehp2.:elink.Set lazy writing ON or OFF, default is ON   

:p.:hp2./P&colon.1|2|3|4 :ehp2.Set priority for lazy writer&per. 1 is lowest, 4 is highest
&per. Default 1 (= idle-time)&per. This might be handy if the lazy-writer doesn&apos.
t seem to get any CPU due to heavy system load&per. 

:p.:hp2./Y :ehp2.Assume Yes 

:p.:hp2./S :ehp2.Do NOT display normal messages 

:p.:hp2./CP :ehp2.Specify codepage in paramter   

:p.:hp2./F :ehp2.Forces CACHEF32&per.EXE to be loaded even if no FAT32 partition is found 
on startup&per. Added starting with version 0&per.99&per.   
.br 

:p.:hp2.Note&colon.  :ehp2.The /T option was removed in version 0&per.83&per. 
.br 
The /FS option was removed in version 0&per.98&per. 
.br 
The /FL option was removed in version 0&per.98&per. 

:p.

:p.:link reftype=launch object='e.exe' data='e.exe config.sys'.:hp3.EDIT YOUR CONFIG&per.SYS :ehp3.:elink.
.br 
  
:h2 id=20047 res=30044.UUNIFAT.DLL and forwarders

:p.:hp2.UUNIFAT&per.DLL and four forwarder DLL&apos.s&colon. 
UFAT12&per.DLL, UFAT16&per.DLL, UFAT32&per.DLL, UEXFAT&per.DLL&colon. :ehp2.

:p.UUNIFAT&per.DLL is FAT file system utility DLL&per. It currently supports CHKDSK, 
FORMAT and SYS routines&per. Previously, there was one DLL, UFAT32&per.DLL and it 
supported only FAT32 filesystem&per. Now FAT32&per.IFS supports different kinds of 
FAT, like&colon. FAT12, FAT16, FAT32, exFAT&per. Since then, all functionality was 
moved to a single UUNIFAT&per.DLL&per.

:p.If an user calls :hp2.format d: /fs&colon.fat12:ehp2., UFAT12&per.DLL is loaded&per. 
UFAT12 forwards all calls to the same routines in UUNIFAT&per. So, all 
filesystems are handled by UUNIFAT&per.DLL&per. Note that all four kinds 
of FAT are handled by a single code, because they are very similar, and 
use the same routines, with a few differences&per. So creating four 
almost identical DLL&apos.s would be a bad idea, the same way as creating 
four IFS drivers for each FAT flavour&per.

:p.Except for CHKDSK, SYS, FORMAT routines, and a stub for a RECOVER routine,
UUNIFAT&per.DLL also contains a function for loading the Unicode translate table&per.
It is called by :link reftype=hd refid=20045. CACHEF32&per.EXE :elink. and 
also, it is used by CHKDSK and SYS code in UUNIFAT&per.DLL itself, to allow
using Unicode long file names&per.

:p.:hp2.UUNIFAT&per.DLL contains the following exported 16-bit functions&colon. :ehp2.

:p.short _Far16 _Pascal _loadds CHKDSK(short argc, char *_Seg16 *_Seg16 argv, char *_Seg16 *_Seg16 envp);
.br
short _Far16 _Pascal _loadds RECOVER(short argc, char *_Seg16 *_Seg16 argv, char *_Seg16 *_Seg16 envp);
.br
short _Far16 _Pascal _loadds SYS(short argc, char *_Seg16 *_Seg16 argv, char *_Seg16 *_Seg16 envp);
.br
short _Far16 _Pascal _loadds FORMAT(short argc, char *_Seg16 *_Seg16 argv, char *_Seg16 *_Seg16 envp);
.br
BOOL _Far16 _Pascal _loadds LOADTRANSTBL(BOOL fSilent, UCHAR ucSource);
.br

:p.:hp2.And also the 32-bit versions of the same functions&colon. :ehp2.

:p.int chkdsk(int argc, char *argv[], char *envp[]);
.br
int recover(int argc, char *argv[], char *envp[]);
.br
int sys(int argc, char *argv[], char *envp[]);
.br
int format(int argc, char *argv[], char *envp[]);
.br
BOOL LoadTranslateTable(BOOL fSilent, UCHAR ucSource);
.br

:p.These could be used in the future pure 32-bit/64-bit versions of
CHKDSK&per.COM, RECOVER&per.COM, SYSINSTX&per.COM, FORMAT&per.COM 
frontend programs&per.

:h3 id=30047 res=31045.CHKDSK

:p.:hp2.CHKDSK:ehp2.  

:p.The UUNIFAT&per.DLL CHKDSK routine is called by chkdsk.com whenever it is issued 
for a FAT drive&per.

:p.When CHKDSK is started, there are two lines like this&colon.

:p.:hp2.The type of file system for the disk is FAT32&per. :ehp2.

:p.This one is on the second line&per. And then, after a couple of lines there&apos.s another like

:p.:hp2.The type of file system for the disk is exFAT&per. :ehp2.

:p.The former is output by CHKDSK&per.COM frontend program and it shows the IFS name&colon. FAT32&per.

:p.The latter is output by UUNIFAT&per.DLL and is the autodetected actual file system type&per. It can
be used to see the actual FS name, when you&apos.re in trouble&per.

:p.For CHKDSK the following options are implemented&colon. 

:p.:hp7.Options :ehp7.:hp7.Description :ehp7.

:p.:hp2./F :ehp2.Fixes problems (Currently UUNIFAT&per.DLL only fixes lost clusters, and an 
incorrect free space count&per.) 

:p.:hp2./C :ehp2.Causes lost clusters to be automatically converted to files if the drive 
was in an inconsistent state at boot (No questions asked)&per. 

:p.:hp2./V&colon.1 :ehp2.Causes CHKDSK to show fragmented files&per. 

:p.:hp2./V[&colon.2] :ehp2.Causes CHKDSK to show details on all files checked&per.(default) 

:p.:hp2./P :ehp2.Serves for using with a PM frontend, like pmchkdsk.exe&per. If specified, it writes
additional messages to stdout, then a PM frontend parses them and updates its controls&per.

:p.:hp2./M&colon.<mountpoint> :ehp2.Specify mountpoint of a mounted disk image&per. This setting
overrides the disk name&per. Disk name should point to a FAT disk, for UUNIFAT&per.DLL to be started&per.

:p.:hp2.Note&colon. :ehp2.At the moment, CHKDSK supports checking FAT12/FAT16/FAT32/EXFAT filesystems&per.

.br 
  
:p.:link reftype=launch object='cmd.exe' data='cmd.exe  '.:hp3.START COMMAND PROMPT :ehp3.
.br 

:elink.

:p.The CHKDSK process does the following checks&colon. 

:p.-Compares all copies of the FATs; 

.br 
-Checks for each file the file allocation; 
.br 
-Checks per file or directory the VFAT long filename; 
.br 
-Checks for, and if /F is specified, repairs lost clusters&per. 
.br 
-Checks for, and if /F is specified, repairs cross-linked files&per. 
.br 
-Checks free space, and if /F is specified, corrects an incorrect setting&per. 
.br 
-Checks for lost extended attributes&per. 
.br 

:p.:link reftype=hd refid=48.CLEAN SHUTDOWN CHKDSK :elink.
.br 

:h4 id=48 res=30045.Clean Shutdown CHKDSK

:p.:hp2.CLEAN SHUTDOWN CHKDSK&colon. :ehp2.

:p.If Windows 95 (OSR2) or later shuts down properly, the clean shutdown status 
of the disk is physically written on the disk&per. On next boot this state is 
checked, and if the disk is not shutdown properly, SCANDISK is run&per. 

:p.FAT32&per.IFS also supports this feature&per. When :link reftype=hd refid=20045.CACHEF32 :elink.is called from 
the config&per.sys, it checks, via a call to the IFS the state of each FAT32 drive
&per. For each drive that is not shutdown properly, CHKDSK is fired&per. If no errors 
are found, or if only lost clusters were found and repaired, the drive is marked ok
&per. 

:p.If CHKDSK cannot solve the problem, the drive state is left dirty, and NO 
FILES CAN BE OPENED AND NO DIRECTORIES CAN BE ADDED OR REMOVED&per. Shutting down the 
disk, leaves the disk marked as not properly shutdown&per. You should boot to 
Windows and run SCANDISK on the drive to fix the remaining problems&per. 

:p.F32STAT however, allows you for set the drive status, bypassing the normal 
handling of FAT32&per.IFS&per. See the description of :link reftype=hd refid=20053.F32STAT :elink.for more information&per. 

:h3 id=30048 res=31046.FORMAT

:p.:hp2.FORMAT:ehp2.  

:p.The UUNIFAT&per.DLL FORMAT routine is called by format&per.com frontend program whenever it is issued 
for a FAT filesystem&per. 

:p.Except for UUNIFAT&per.DLL, there are UFAT12&per.DLL, UFAT16&per.DLL, UFAT32&per.DLL, UEXFAT&per.DLL, which are forwarders to UUNIFAT&per.DLL&per. They
are needed to support formatting with "/fs&colon.fat12", "/fs&colon.fat16", "/fs&colon.fat32", "/fs&colon.exfat" switches&per. All functionality
is implemented in the main DLL, and is redirected to it&per.

:p.The FORMAT routine was ported to OS/2 from Windows platform and extended to support FAT12, FAT16 and EXFAT filesystems&per.
The Fat32Format program was used as a base, see http&colon.//www&per.ridgecrop&per.demon&per.co&per.uk/index&per.htm?fat32format&per.htm &per.

:p. The Fat32Format program is licensed as GPL too, like FAT32&per.IFS is&per.

:p. Fat32Format 1&per.07 is (c) Tom Thornhill 2007,2008,2009&per.

:p.For FORMAT the following options are implemented&colon.

:p.:hp7.Options :ehp7.:hp7.Description :ehp7.

:p.:hp2./? :ehp2.Show help&per.

:p.:hp2./H :ehp2.Show help&per.

:p.:hp2./R&colon.<number> :ehp2.Number of reserved sectors&per.

:p.:hp2./V&colon.<volume label> :ehp2.Set volume label (Max&per. 11 characters)&per.

:p.:hp2./P :ehp2.Serves for using with a PM frontend, like pmformat.exe&per. If specified, it writes
additional messages to stdout, then a PM frontend parses them and updates its controls&per.

:p.:hp2./C&colon.<number> :ehp2.Cluster size&per. The following cluster sizes are
supported&colon.

:p.:hp2./M&colon.<mountpoint> :ehp2.Specify mountpoint of a mounted disk image&per. This setting
overrides the disk name&per. Disk name should point to a FAT disk, for UUNIFAT&per.DLL to be started&per.

:p.:hp2./L:ehp2. Low level format for CD&apos.s/DVD&apos.s/BD&apos.s and floppies&colon. (experimental feature)

:p.:hp2.FAT12 :ehp2.

:cgraphic.
 <number>  Max. size
 ____________________
 1         4MB
 2         8MB
 4         16MB
 8         32MB
 16        64MB
 32        128MB
 64        256MB
:ecgraphic.

:p.:hp2.FAT16 :ehp2.

:cgraphic.
 <number>  Max. size
 ____________________
 1         64MB
 2         128MB
 4         256MB
 8         512MB
 16        1024MB
 32        2048MB
 64        4096MB
:ecgraphic.

:p.:hp2.FAT32 :ehp2.

:cgraphic.
 <number>  Max. size
 ____________________
 1         128GB
 2         256GB
 4         8192GB
 8         16GB
 16        32GB
 32        >32GB
 64        >32GB
:ecgraphic.

:p.:hp2.exFAT :ehp2.

:cgraphic.
 <number>  Max. size
 ____________________
 4         256MB
 32       1024MB
 128       >32GB
:ecgraphic.

:p.For FAT32, maximum is 128 sectors per cluster -- 64K clusters&per. More than 64K per cluster is
not allowed by FAT32 spec&per. Clusters, larger than 64KB, are supported by exFAT&per. Up to 32 MB clusters
are supported&per. For FAT12/FAT16/FAT32, clusters should be, generally, less than or equal to 32 KB,
but WinNT supported a FAT32 variant with 64 KB per cluster&per. Note that Win9x could behave unpredictably
in case it tries to mount a FAT drive with 64 KB cluster&per. This is because number of clusters in these OS&apos.es
is a signed byte, so a division by zero could occur&per. So, if you use DOS/Win9x, please avoid using 64 KB clusters&per.
Fat32&per.ifs supports large clusters, too, by splitting clusters, larger than 32 KB to 32 KB or lesser blocks&per.
(There is a limitation of a 16-bit driver, which is the case for fat32&per.ifs, that we cannot allocate more than 64 KB
of memory at once, so, we need to split to smaller blocks)&per.

:p.FAT12 supports 32 MB max&per. volume size, 4 GB max&per. file size&per.
.br
FAT16 supports 2 GB max&per. volume size, 4 GB max&per. file size&per.
.br
FAT32 supports 8 TB max&per. volume size, 4 GB max&per. file size&per.
.br
exFAT supports 128 PB max&per. volume size, 16 EB max&per. file size&per.
.br

:p. Note that if we&apos.ll increase a cluster size to 64 KB (instead of a 32 KB), then maximum volume size for
FAT12/16/32 will increase twice&per. Also note that if using a standard MBR partition scheme, the maximum volume size
for FAT32 and exFAT should be reduced to 2 TB (it is limited with a 32-bit LBA in partition table)&per. If we&apos.ll
use GPT partition table, then we can use larger volume sizes (but note that OS/2 is limited to an MBR partition
scheme, yet)&per.

:p.:hp2./FS&colon.FAT12|FAT16|FAT32|EXFAT :ehp2.FAT12/FAT16 disks can be formatted using FORMAT or checked using CHKDSK too&per.
For FORMAT, "/fs&colon.fat12" or "/fs&colon.fat16" needs to be specified too&per. "format d: /fs&colon.fat" still invokes
the standard IBM's FAT format routine&per. So, if you want our new FORMAT routine, just specify "/fs&colon.fat12"
or "/fs&colon.fat16" instead of "/fs&colon.fat"&per. Also, "/fs&colon.fat32" is supported for formatting into FAT32&per.
If the "/fs" switch is not specified, the filesystem FAT12/FAT16/FAT32/exFAT is chosen based on the volume size&per. If it is
less than 4 MB, it is FAT12&per. If it is less than 2 GB, it is FAT16&per. If it is less than 32 GB, it is FAT32&per.
And if it is larger than 32 GB, exFAT is chosen by default&per. Formatting to exFAT is supported too&per. For that,
you should specify "/fs&colon.exfat" (possibly, together with /c&colon.<cluster size>)&per.

:h3 id=30049 res=31047.SYS

:p.:hp2.SYS:ehp2.  

:p.The UUNIFAT&per.DLL SYS routine is called by sysinstx.com whenever it is issued 
for a FAT drive&per. 

:p.The SYS command has no parameters. At this time, it only writes the bootblock
and several FreeLDR main files&per. No OS2BOOT is installed at this moment as it is not yet
implemented&per. You can add more files from standard FreeLDR installation, if you need&per.

:p.The SYS command is supported for FAT12/FAT16/FAT32/exFAT&per.

:p.:hp2./M&colon.<mountpoint> :ehp2.Specify mountpoint of a mounted disk image&per. This setting
overrides the disk name&per. Disk name should point to a FAT disk, for UUNIFAT&per.DLL to be started&per.

:h2 id=20055 res=30069.QEMUIMG&per.DLL

:p.:hp2.QEMUIMG&per.DLL&colon. :ehp2.

:p.QEMUIMG&per.DLL contains support for different disk image formats ported from QEMU 
(http&colon.//www&per.qemu&per.org/)&per. It allows FAT32&per.IFS to mount the FAT 
(FAT12/FAT16/FAT32/exFAT) filesystems contained in files of the following 
formats&colon.

:ul.
:li.RAW
:li.Bochs
:li.Linux Cloop (CryptoLOOP, the format used by Knoppix LiveCD)
:li.Macintosh &per.dmg
:li.VirtualPC &per.vhd
:li.VMWare &per.vmdk
:li.Parallels
:li.QEMU vvfat, qcow, qcow2 formats
:li.VBox &per.vdi
:eul.

:p.Also, mounting of non-FAT images is possible with :link reftype=hd refid=20043.LOOP&per.ADD :elink. 
block device driver, which is supplied together with FAT32&per.IFS&per. 

:p.These images are mostly virtual machine disk images of different VM's like Bochs, VirtualPC, 
VirtualBox, QEMU, VMWare etc&per. Also, RAW images are supported, like diskette 
images&per. See :link reftype=hd refid=20054.F32MOUNT&per.EXE
:elink. for more details on how to mount different file system images with 
FAT32&per.IFS&per.

:p.For successful mounting of the disk images, a running :link reftype=hd refid=20045.CACHEF32&per.EXE :elink. 
daemon is required&per. The daemon contains a special thread polling the IFS for OPEN/READ/WRITE/CLOSE 
requests, executing these requests and returning the results back, via a shared memory buffer&per.
Also, there is a similar thread polling :link reftype=hd refid=20043.LOOP&per.ADD :elink. for the 
same commands&per.

:p.QEMUIMG&per.DLL is a shared component&per. It is loaded by both :link reftype=hd 
refid=20045.CACHEF32&per.EXE :elink. and :link reftype=hd refid=20054.F32MOUNT&per.EXE :elink.

:h2 id=20048 res=32044.FAT32FMT&per.EXE

:p.:hp2.FAT32FMT&per.EXE&colon. :ehp2.

:p.FAT32FMT&per.EXE is a standalone version of Fat32Format&per. It is equivalent to FORMAT routine and
takes the same parameters&per. (Look here&colon. :link reftype=hd refid=30048.FORMAT&per.EXE :elink. for more info)&per.

:h2 id=20049 res=32045.FAT32CHK&per.EXE

:p.:hp2.FAT32CHK&per.EXE&colon. :ehp2.

:p.FAT32CHK&per.EXE is a standalone version of FAT CHKDSK&per. It is equivalent to CHKDSK routine and
takes the same parameters&per. (Look here&colon. :link reftype=hd refid=30047.CHKDSK&per.EXE :elink. for more info)&per.

:h2 id=20050 res=32046.FAT32SYS&per.EXE

:p.:hp2.FAT32SYS&per.EXE&colon. :ehp2.

:p.FAT32SYS&per.EXE is a standalone version of FAT SYS/SYSINSTX&per. It is equivalent to SYS routine and
takes the same parameters&per. (Look here&colon. :link reftype=hd refid=30049.SYSINSTX&per.EXE :elink. for more info)&per.

:h2 id=20051 res=32051.F32CHK&per.EXE

:p.:hp2.F32CHK&per.EXE&colon. :ehp2.  

:p.F32CHK&per.EXE is a helper for CHKDSK&per. It is needed for disk autocheck on boot&per. It takes
the same parameters as CHKDSK itself&per. It is run by FAT32&per.IFS init routine&per.

:h2 id=20052 res=32048.F32MON.EXE (formerly MONITOR.EXE)

:p.:hp2.F32MON&per.EXE (formerly MONITOR&per.EXE)&colon. :ehp2.  

:p.F32MON will show (most) FAT32 actions on screen&per. This program is intended 
for troubleshooting&per. Using F32MON will degrade performance since FAT32 must 
send monitoring information to an internal buffer&per. This internal buffer is only 
4096 bytes large, so if monitoring is on, but F32MON does not run, logging 
information is lost&per. However, this will only occur if /MONITOR is specified after the 
IFS= line for FAT32&per.IFS&per. 

:p.If the /MONITOR command is not specified in the config&per.sys after the :link reftype=hd refid=20042.IFS= 
statement:elink., monitoring is OFF by default, but starting F32MON once will activate 
monitoring&per. 

:p.Also, see :link reftype=hd refid=200705.Debugging hangs or traps, or any other problems&per. :elink.
for more information on methods of debugging FAT32&per.IFS in case of problems&per. Here is
described, how to use OS/2 TRACE facility, or QSINIT or OS/4 log buffer and COM port for 
debugging&per.

:p.When F32MON runs, information is shown on the screen, and also written to 
FAT32&per.LOG in the current directory&per. 

:p.When F32MON terminates, the internal monitoring is switched off&per. The 
syntax for F32MON is&colon. 

:p.F32MON [tracemask] [/S] 

:p.If tracemask is omitted it is set to 1&per. 

:p.The tracemask exists of a specific bit to show certain types of information
&per. This way, you can monitor selectively&per. 

:p.The following values for tracemask exist&colon. 

:p.1 - Shows all calls to FS_xxxxx entry points and the return values 
.br 
2 - Shows additional (internal) function calls 
.br 
4 - Shows cache related (lazy writing) function calls 
.br 
8 - Shows internal memory handling (malloc &amp. free) memory calls 
.br 
16 - Shows FS_FINDFIRST/FS_FINDNEXT calls 
.br 
32 - Shows some other calls 
.br 
64 - Shows extended attribute handling 
.br 

:p.You should add values to see multiple groups&colon. 

:p.e&per.g&per. 
.br 
You want to see both FS_xxxx calls and cache related information&per. As 
tracemask you should use 1 + 4 = 5&per. 

:p.:hp7.Options :ehp7.:hp7.Description :ehp7.

:p.:hp2./S :ehp2.  - Runs without output to the screen, but only to FAT32&per.LOG&per. This 
is useful if F32MON sends so much messages to the screen that the program can
&apos.t keep up with the IFS&per. Using /S only sends the output to FAT32&per.LOG so 
no time is lost in screen handling&per. 
.br 
  
:p. :link reftype=launch object='cmd.exe' data='cmd.exe  '. :hp3.START COMMAND PROMPT :ehp3.
.br 
:elink.  
:h2 id=20053 res=30047.F32STAT.EXE

:p.:hp2.THE PURPOSE OF F32STAT&per.EXE&colon. :ehp2.  

:p.F32STAT can be used to query the clean shutdown state of a FAT32 drive&per. 
It also allows you to alter the clean shutdown state&per. You could use this 
feature if FAT32&per.IFS blocks access to the disk because it is dirty on boot, and 
CHKDSK could not solve the problem&per. 

:p.The syntax is&colon. 

:p.   F32STAT drive&colon. [:link reftype=hd refid=51.options:elink.] 

:p.F32STAT&per.EXE supports only one drive at one time&per. So if you want to 
change the state of one more drive, you should run F32STAT&per.EXE, respectively&per. 
i&per.e&per., FAT32 drive is C&colon. D&colon.&per. You should run F32STAT&per.
EXE two times for C and D as the following&per. 

:p.F32STAT&per.EXE C&colon. /DIRTY 
.br 
F32STAT&per.EXE D&colon. /DIRTY 

:p.:link reftype=launch object='cmd.exe' data='cmd.exe  '.:hp3.START COMMAND PROMPT :ehp3.
.br 
:elink.  
:h3 id=51 res=30048.F32STAT Options

:p.:hp2.F32STAT&per.EXE OPTIONS&colon. :ehp2.  

:p.:hp7.Options :ehp7.:hp7.Description :ehp7.

:p.:hp2./CLEAN :ehp2. - Inform FAT32&per.IFS that the disk was clean on boot and may be used
&per. The disk itself will be marked as clean on a succesfull shutdown&per. (The 
internal dirty flag FAT32&per.IFS uses will be cleared&per.) 

:p.:hp2./FCLEAN :ehp2.  - Inform FAT32&per.IFS that the disk was clean on boot and may be 
used&per. The disk itself will also be marked as clean at that moment&per. The 
internal dirty flag FAT32&per.IFS uses will be cleared, but the marking on disk will 
also be set ok&per.) 

:p.:hp2./DIRTY   :ehp2. - Inform FAT32&per.IFS to set its internal dirty flag, and mark the 
drive dirty on disk&per. On shutdown the drive will be left dirty, so when  booting 
Windows 95 (OSR2) and later Windows versions, SCANDISK will be started&per. 
.br 
  
:p.:link reftype=launch object='cmd.exe' data='cmd.exe  '.:hp3.START COMMAND PROMPT :ehp3.
.br 
:elink.  

:h2 id=20054 res=30074.F32MOUNT&per.EXE

:p.:hp2.F32MOUNT&per.EXE&colon. :ehp2.

:p. F32MOUNT&per.EXE allows to mount the file system images, e&per.g&per., diskette images and hard
disk images, or partitions inside hard disk images&per. Also, we have ported a library from QEMU,
allowing to access data inside virtual machine's hard disk images, including VirtualPC (&per.VHD),
VMWare (&per.VMDK), VirtualBox (&per.VDI), QEMU (QCOW and QCOW2), Macintosh DMG, Bochs, Linux CLOOP
(images used by Knoppix LiveCD), Parallels, VVFAT, and plain RAW images&per.

:p. The images can be mounted at the directory, serving as a mount point&per. The directory should
be on a FAT12/FAT16/FAT32/exFAT drive, and the mounted filesystem should be FAT12/FAT16/FAT32/exFAT
too&per. So, you can access the image filesystem by changing the directory to the selected mount
point subdirectory&per.

:p.Also, with LOOP&per.ADD, there is a possibility to mount images with any file system 
(besides FAT12/FAT16/FAT32/exFAT) on a drive letter&per.

:p. The IFS uses :link reftype=hd refid=20045.CACHEF32&per.EXE :elink. as a helper&per. It 
loads the :link reftype=hd refid=20055. QEMUIMG&per.DLL :elink. library, which contains 
functions to read data inside the virtual VM disk images of the above mentioned formats&per.

:p.The syntax is the following&colon.

:p.:hp2.[c&colon.\] f32mount [/a | /u] [d&colon.\somedir\somefile&per.img [<somedir> | /block] [/d] [/p&colon.<partition no>][/o&colon.<offset>][/f&colon.<format>][/s&colon.<sector size>]] :ehp2.

:p. So, for the mount command, the first parameter is an image path (absolute or relative), the second 
is the mount point (absolute or relative, too), or a "/block" parameter, which specifies that we should
mount to a block device with LOOP&per.ADD&per. In the latter case, the image will be mounted to a drive
letter, and F32MOUNT&per.EXE refreshes removable media, and the image gets mounted to a drive letter, with
any available IFS (not only FAT32&per.IFS).

:p.Then the following options are possible&colon.

:p.:hp2. /a :ehp2. - automount all images specified in d&colon.\os2\boot\fstab&per.cfg (where d&colon. is OS/2 boot 
drive letter)&per.

:p.:hp2. /u :ehp2. - autounmount all images specified in d&colon.\os2\boot\fstab&per.cfg (where d&colon. is OS/2 boot 
drive letter)&per.

:p.:hp2. /d :ehp2. - specifies that we're unmouning the image, or a mountpoint, specified as a first parameter&per.

:p.:hp2. /p&colon.<partition no> :ehp2. - specifies partition number to mount&per. Numbers 1&per.&per.4
specify primary partitions; nubmers > 4 specify logical partitions&per. E&per.g&per., 5 is the first
logical partition, 6 is second logical partition, etc&per.

:p.:hp2. /o&colon.<offset> :ehp2. - specifies the offset of partition table from the start of the image&per.
This should be specified if you have a RAW image having a special header right before the partition
table&per. This option can be used in conjunction with the /p option&per. The offset could be either
decimal, or hexadecimal&per. Hexadecimal offsets start with "0x"&per.

:p.:hp2. /f&colon.<format> :ehp2. - force the use of specified format, instead of autodetecting
it, which is the default&per. Usually, the format is autodetected by the QEMUIMG&per.DLL library&per.
The following format names are defined&colon. :hp2.Raw/bochs/cloop/dmg/vpc/vmdk/parallels/vvfat/qcow/qcow2/vdi&per. :ehp2.

:p.:hp2. /s&colon<sector size> :ehp2. - force sector size value&per. Note that currently, OS2DASD&per.DMD
does not support sector size values other than 512 bytes, and attempt to specify the value of, for
example, 4096 bytes will cause a trap in OS2DASD&per.DMD&per.

:p.:hp2. Examples&colon. :ehp2.

:p.:hp2. [c&colon.\] f32mount win32&per.dsk w&colon.\tmp\0 :ehp2.

:p. - mounts a diskette image&per.

:p.:hp2. [c&colon.\] f32mount flash-512mb&per.img w&colon.\tmp\0 /p&colon.1 :ehp2.

:p. - mounts the first primary partition on a 512 MB flash disk image&per.

:p.:hp2. [c&colon.\] f32mount win98&per.vhd w&colon.\tmp\0 /p&colon.5 :ehp2.

:p. - mounts the first logical partition on the VirtualPC image with Win 98 installed&per.

:p.:hp2. [c&colon.\] f32mount hdimage&per.freedos w&colon.\tmp\0 /p&colon.1 /o&colon.0x80 /f&colon.raw :ehp2.

:p. - mounts the first primary partition on a DOSEMU FreeDOS image (having a 0x80-byte header before
the partition table&per.)&per. Force format to RAW&per.

:p.The images are unmounted with the following command&colon.

:p.:hp2. [c&colon.\] f32mount hdimage&per.freedos /d:ehp2.

:p.or

:p.:hp2. [c&colon.\] f32mount w&colon.\tmp\0 /d:ehp2.

:p.or, in case the image was mounted to the block device,

:p.:hp2. [c&colon.\] eject j&colon.:ehp2.

:p. where j&colon. is a block device drive letter&per.

:p.:hp2. [c&colon.\] f32mount /a:ehp2.

:p. automatically mount all images specified in d&colon.\os2\boot\fstab&per.cfg

:p.:hp2. [c&colon.\] f32mount /u:ehp2.

:p. automatically unmount all images specified in d&colon.\os2\boot\fstab&per.cfg

:p. d&colon.\os2\boot\fstab&per.cfg file specifies what images should be automounted/autounmounted,
and with which options. Each line should be the same as f32mount&per.exe file command line looks like,
except for the first "f32mount" word, which is omitted&per. For example, the following line&colon.

:p. l&colon.\data\vm\img\z&per.iso /block

:p. will automount l&colon.\data\vm\img\z&per.iso file at the drive letter&per. The line &colon.

:p. l&colon.\data\vm\img\winxp&per.vhd w&colon.\tmp\0 /p&colon.1

:p. will automount the l&colon.\data\vm\img\winxp&per.vhd file at the w&colon.\tmp\0
mount point on the FAT drive w&colon., 1st primary partition will be mounted&per.

:p. Also, except for "mount" lines, comment lines are possible, which sholud start with "#"
or ";" symbols&per. These lines are ignored&per.

:h1 id=52 res=30049.Current Status and Features

:p.:hp2.CURRENT STATUS AND FEATURES&colon. :ehp2.

:p.:link reftype=hd refid=100.FEATURES :elink.

:p.:link reftype=hd refid=101.TODO :elink.

:p.:link reftype=hd refid=53.LIMITATIONS :elink.

:p.:link reftype=hd refid=54.REMOVABLE MEDIA :elink.

:p.:link reftype=hd refid=57.LONG FILE NAMES :elink.

:p.:link reftype=hd refid=62.EXTENDED ATTRIBUTES :elink.

:p.:link reftype=hd refid=66.DRIVER INTERFACE :elink.

:p.:link reftype=hd refid=69.PERFORMANCE :elink.

:p.:link reftype=hd refid=20069.FAQ :elink.

:p.:link reftype=hd refid=20070.HOWTO&apos.s :elink.
.br 

:h2 id=100 res=30100.Features

:p.:hp2.Features :ehp2.

:ul.

:li.64 KB clusters support on FAT12/FAT16/FAT32, clusters of up to 32 MB on exFAT
:li.support for files up to 4 GB on standard FAT12/FAT16/FAT32
:li.support for FAT+ extension (a /plus switch is required), so that, 64-bit file
size is possible on FAT32
:li.support for VFAT LFN&apos.s on FAT12/FAT16 (for that, you should specify /fat key for fat32.ifs).
:li.short filenames support for a DOS session for FAT12/FAT16/FAT32/exFAT&per. Filename conversion
short <-> long&per. So that, all files on a FAT volume are accessible from a VDM&per.
:li.EA&apos.s support (/eas switch for FAT32&per.IFS is required)
:li.exFAT support ("/exfat" switch is required).)
:li.support for media with different sector sizes, up to 4096 bytes (according
to BIOS parameter block)
:li.supported different media, like floppies (physical and virtual), CD's, 
flash sticks and hard disks with FAT12/FAT16/FAT32/exFAT filesystems. For 
all these media types, FORMAT/CHKDSK/SYS should work. Also, virtual disk
drivers like virtual floppies (vfdisk&per.sys by Daniela Engert), virtual 
hard disks (vdisk&per.sys by IBM, svdisk&per.sys by Albert J&per. Shan,
hd4disk&per.add by _dixie_) are tested to be working successfully&per.
See :link reftype=hd refid=200701.VFDISK&per.SYS/VDISK&per.SYS/SVDISK&per.SYS/HD4DISK&per.ADD 
virtual disks :elink. for more details&per.
:li.support for mounting FAT12/FAT16/FAT32/exFAT filesystem images on a 
FAT12/FAT16/FAT32/exFAT disk subdirectory. raw/bochs/cloop/dmg/vpc/vmdk/parallels/vvfat/qcow/qcow2/vdi 
formats are supported. (qemu code was used for support of VM images). For 
partitioned images, a partition number could be specified. Support for 
CHKDSK/FORMAT/SYSINSTX on a mounted image (this feature is alpha-quality, yet). 
:li.Support for mounting not-FAT disk images to a drive letter with :link reftype=hd 
refid=20043.LOOP&per.ADD :elink. driver&per.

:eul.

:h2 id=101 res=30101.Todo List

:p.:hp2.Todo List :ehp2.

:ul.
:li.Swapping to FAT12/FAT16/FAT32/exFAT partition support
:li.Booting from FAT12/FAT16/FAT32/exFAT support
:li.Performance enhancements and proper caching
:li.Convert FAT32&per.IFS to a 32-bit FS and port it to osFree, so that, as a side effect,
64-bit file seek will work&per.
:eul.

:h2 id=53 res=30050.Limitations

:p.:hp2.LIMITATIONS&colon. :ehp2.

:ul.

:li.Logically, FAT32 should now support up to 2 terabytes partitions in size
&per.  However, we cannot be sure that MS does not use another combination of sectors 
per cluster and bytes per sector&per. If this is the case, the supported size could 
be smaller&per. A FAT32 partition can contain files up to 4GB in size&per. The 4GB 
is the limit of FAT32 file system itself&per. Files less than 2GB can be copied or 
moved to a FAT32 partition&per. Files larger than 2GB cannot be read by some programs 
such as xcopy&per. It appears that xcopy itself does not support files greater than 
2GB&per. :hp2.Note :ehp2.by Valery Sedletski&colon. I tested FAT32&per.IFS on hard disks with 
sizes up to 1 TB -- I have a 1 TB USB hard disk formatted as FAT32, and it works fine&per.
Also, flash sticks can have unlimited size -- 2 GB is a limit for SD cards, 32 GB is the 
limit for SDHC cards, and 2 TB is the limit for SDXC cards&per. SDXC cards are formatted
as exFAT on the factory, but can be optionally reformatted to FAT32, if preferred&per.
I have a 128 GB microSDXC card, which I reformatted as FAT32, it works fine with 
FAT32&per.IFS&per. Note that FAT32&per.IFS supports exFAT too&per. 2 GB file size 
is the limit of some softrware, not supporting 64-bit file size&per. So, such software, 
including most 16-bit IFS-es, support 32-bit signed file size, 2^31 == 2 GB&per. 
But currently, we added support for 64-bit file size (aka "large file support") 
into FAT32&per.IFS&per. Large file support is enabled on FAT32&per.IFS, if 
"/largefile" is specified in FAT32&per.IFS command line&per. FAT family 
filesystems (FAT12/FAT16/FAT32), however, have a 32-bit field for file size 
in each directory entry, which limits file size to 4 GB == 2^32 bytes&per. A 
cluster chain in the FAT table, however, can be unlimited&per. This allows 
for files with unlimited size, if we&apos.ll store full 64-bit file size elsewhere, 
but not in the directory entry&per. :link reftype=hd refid=200702.FAT+ :elink. 
extension stores bits 32-34 in unused 0-2 bits of "EA mark byte"&per. This 
allows for file sizes up to 32 GB&per. If the file size is even bigger, 
the full 64-bit file size can be stored in special FAT_PLUS_FSZ EA&per. 
EA&apos.s should be enabled with "/eas" switch, of course&per. Also, for 
:link reftype=hd refid=200702.FAT+ :elink. support to be enabled, 
you&apos.ll need to add the "/plus" switch to FAT32&per.IFS command 
line&per. Note that file size is 64-bit in exFAT, so extensions like 
:link reftype=hd refid=200702.FAT+ :elink. are not required here&per.

:li.Starting from FAT32&per.IFS v&per. 0&per.10, now clusters of sizes up to 64 KB
are supported on FAT12/FAT16/FAT32&per. However, MS discourages the use of
64 KB clusters, because such clusters may crash older MS OS-es like Win95
or MSDOS, causing division by zero&per. But if you don't have such OS-es
on your machine, such filesystems with 64 KB clusters may be useful, for
example, you can use a :link reftype=hd refid=200700. 4 GB FAT16 partition 
as a SADUMP partition :elink.&per. Clusters of sizes up to 32 MB are 
supported on exFAT partitions&per.

:li.FAT32&per.IFS previously supported only disks with a sector size of 
512 bytes&per. However, this has been changed in FAT32&per.IFS ver&per. 
0&per.10&per. CD/DVD/BD have 2048-byte sectors; MO drives have 1024-byte 
sectors&per. Also, some modern hard disks support 4096-byte sectors, 
though, these big sectors are seen by disk firmware, only&per. Every OS still 
sees 512-byte "virtual" sectors&per. Also, I heard of some unusual USB hard disks
with 4096-byte sectors, visible to an OS&per. Such hard drives are, of course, 
currently unsupported by OS/2, but this can change in the future&per. 
FAT32&per.IFS supports non-512-byte sectors&per. Sector size is detected
according to BIOS parameter block&per. But such non-512-byte sectors are 
supported on CD/DVD/BD and MO drives, only&per. Hard disks are limited 
to 512-byte sectors on OS/2&per.)&per. Yes, FAT32&per.IFS supports 
not only hard disks, but other media like MO drives (they are not 
tested, though), floppies or CD/DVD/BD disks&per. See :link reftype=hd 
refid=200704.Using CDRW&apos.s with FAT16/FAT32/exFAT filesystem&per. :elink.
for more info&per.

:li.You cannot BOOT OS/2 yet from a FAT32 partition as it done for HPFS/JFS&per. 
This can change some day, though&per. Currently, SYSINSTX installs the FreeLDR 
bootblock, so you can boot it from that partition&per. Later, a conventional 
miniFSD support will be added, so you&apos.ll be able to boot OS/2 the same way
as HPFS/JFS&per. Note also that booting OS/2 from FAT (FAT12/FAT16/FAT32/exFAT) 
is possible with FreeLDR and its OS/2 booter, bootos2&per.mdl (used in Team Boot/2 
boot disk -- universal OS/2 rescue/live system, supporting CD/flash/harddisk/etc 
media)&per.

:li.You cannot place the SWAPPER&per.DAT on a FAT32 partition yet&per. However,
there is some incomplete swapping support written, and some day it will support
swapping on FAT (FAT12/FAT16/FAT32/exFAT) partitions&per.

:li.CHKDSK can diagnose a disk, but will only FIX lost clusters and an incorrect 
free space count&per. For all other errors, you&apos.ll need to run Windows and 
start SCANDISK to fix the problem&per. 

:li.CHKDSK will always convert lost clusters to files and NEVER to directories
&per. If you want that, use WinNT CHKDSK&per. :hp2.Note :ehp2. by Valery Sedletski&colon.
In FAT32&per.IFS v&per. 0&per.10, CHKDSK saves lost chains into new subdirectory
on each CHKDSK run&per. So, this limitation also has been overcome&per.

:li.The RECOVER command is not supported&per. FORMAT and SYSINSTX are supported
in FAT32&per.IFS v&per. 0&per.10, since some revision&per. SYSINSTX, however,
still does not support os2boot (a miniFSD)&per. Currently, SYSINSTX only writes
a bootblock of FreeLDR (a bootloader developed by the osFree project)&per.

:li.Only last access date (and not last access time) is maintained by FAT32&per.
IFS&per. This is similar to Win95 (OSR2)&per. 

:li.Long filenames are not by default supported in DOS and Win-OS/2 sessions, 
they use only the shortnames&per. Please see :link reftype=hd refid=60.&apos.LONG FILENAMES IN OS/2 AND DOS 
SESSIONS&apos.&per. :elink.

:li.You&apos.d better NOT change codepages on the fly IF you have open files 
with filenames that contain extended ASCII characters&per. 

:li.If you are using :link reftype=hd refid=18.PARTFILT&per.FLT :elink.then FDISK, Partition Magic or any 
partition maintenance tools will show non-existing drives and other nonsense&per. 

:li.This version needs the native NLS support from OS/2&per. This means Warp 3 
fixpack 26 or higher or Warp 4&per. (You must have the LANGUAGE directory)&per.

:eul. 
  
:h2 id=54 res=30075.Removable Media

:p.:hp2.REMOVABLE MEDIA&colon. :ehp2.  

:p.There are no problems accessing FAT32 volumes found in USB removable media, 
or regular partitionable media (i&per.e&per. hard disks) accessed via an USB 
enclosure or PCMCIA converter card&per. 

:p.USB flash media normally come formatted FAT and thus are NOT accessed through 
the FAT32 driver&per.  For the FAT32 driver to work with these devices, they would 
have to be formatted FAT32&per.  However, according to Microsoft&apos.s FAT32 specs, 
FAT32 volumes can be legally generated only for media over 512 MB, which is beyond 
the size of most of the current Flash media, such as those found in digital cameras
&per. 

:p.:hp2.Note&colon.:ehp2. The above paragraph is obsolete these days&per. The days when flash disk
sizes were about 512 MB have gone many years ago&per. Now flash disks are some tens of GB in size&per.
So, for current media sizes, FAT32 is optimal file system&per. Or, exFAT could be used too, for carrying
large files, bigger than 4 GB&per. But since FAT32&per.IFS now supports VFAT on FAT12/FAT16 volumes, the
old FAT16 flash disks with sizes like 512 MB can be used with FAT32&per.IFS too&per. This is tested successfully
too&per. For example, I have two flash sticks, one is 512 MB, the second is 2 GB&per. Both working like a charm&per.

:p.Also, there were FAT16-only old mp3 players, like I had in the beginning of 2000&apos.s&per. It's size was 256 MB
and it was formatted with FAT16&per. It looks that FAT32 is unsupported by the firmware&per. These days, I copied files
via WPS with creating the &per.LONGNAME EA&apos.s, then used vfat2ea utility to convert &per.LONGNAME EA&apos.s to VFAT
LFN&apos.s&per. Now this technique is obsolete&per. FAT16 with VFAT LFN&apos.s can be directly supported by FAT32&per.IFS&per.

:p.FAT32&per.IFS should work with any fixed or removable media of any size&per. So far, it is tested by me with disks up to
1 TB in size&per. I have a 1 TB USB 2.5" removable hard disk formatted with FAT32, for backup and exchange purposes&per. Everything
works fine&per. Also, nothing prevents to use disks up to 2 TB (with MBR partition scheme&per. With exFAT, or GPT partition scheme,
it could be even larger&per.)&per.

:p.For LVM systems, one would also need to assign a volume letter to the drive 
before accessing the files on it&per. 

:p.:hp2.Note&colon.  :ehp2.If the FAT32 driver is installed, the file system will load 
whether one has a FAT32 partition or not&per. Thus, if a removable FAT32 drive is 
plugged in or inserted, the FAT32 media should be found, provided the removabe drive(s) 
are supported by the USB drivers or whatever other drivers are necessary for the 
drive(s) to be found and recognized&per. 

:p.Some formatted FAT32 USB flash media work with the driver while others do not
&per.   

:p.:hp2.IMPORTANT&colon.  :ehp2.Be sure and install the latest USB drivers from either the 
eComStation web site or IBM&apos.s Software Choice&per. If you are still unable to access 
your Removable media, download Chris Wohlgemuth&apos.s USB driver package and 
replace the IBM mass storage device with Chris&apos.s driver&per.  Be sure and read the 
documentation on using his driver(s)&per. 

:p.:hp2.Addition&colon.:ehp2. Nowadays, the above sources of drivers are obsolete&per. Chris
Wohlgemuth&apos.s USB drivers are obsolete too&per. Please use Lars&apos. Erdmann&apos.s USB
drivers from Hobbes (search them by "usbdrv" keyword)&per.

:p.See :link reftype=hd refid=56.GETTING THE FAT32 DRIVER TO WORK WITH REMOVABLE MEDIA :elink.  

:h3 id=55 res=30078.Types of USB Mass Storage Devices

:hp2.TYPES OF USB MASS STORAGE DEVICES :ehp2.

:p.There are two kinds of USB removable media&colon. partitionable media and 
large Floppy media&per. Many modern USB devices work as both and can be switched from 
one mode to another&per. This can be done either in some device-specific way (such 
as a hardware switch, software utilities or a combination of both) or in most 
compatible devices by erasing or creating partition tables on the device with an advanced 
disk tool (such as DFSee) which bypasses system detection of the device as a fixed 
type, and then detaching and re-attaching the device so changes are recognized&per. 

:p.Partitionable USB drives are seen by the system as if they were &osq.normal
&osq. hard disks, so to get them to work with OS/2, you have the usual 
limitations&colon. 

:ul.

:li.OS2DASD&per.DMD must be at a certain level for large drives to be fully 
recognized&per. 
:li.Also, volumes must be created (and have sticky drive letters assigned if 
they are to be accessed) either from existing partitions, or from unassigned space 
areas within the &osq.disk&osq.&per. 
:li.Partitions on these devices should also have 
their proper types set (i&per.e&per. 0B/0C for FAT32, 07 for HPFS/NTFS/exFAT, 06 for FAT16, 
etc&per.) in order to avoid problems&per.

:eul.

:p. With recent versions of Lars&apos. Erdmann&apos.s USB drivers, Large floppies with FAT/FAT32/exFAT filesystem
can be emulated on USBMSD driver level&per. The driver just emulates the 1st track with MBR and partition table, and
DLAT sector with LVM info&per. This allows to see Large Floppy media created by Windows, with FAT32&per.IFS&per.
See also :link reftype=hd refid=200703.Notes on large floppy media&per. :elink.

:h3 id=56 res=30076.Getting the FAT32 Driver to Work with Removable Media

:hp2.GETTING THE FAT32 DRIVER TO WORK WITH REMOVABE MEDIA :ehp2.

:p.The below link to an archive VOICE newsletter is a good source for 
information&per. 

:p.:hp9.http&colon.//www&per.os2voice&per.org/VNL/past_issues/VNL0606H/feature_2&per.html :ehp9.  

:p. 1&per. Your motherboard bios must support USB devices&per. Make sure USB support is enabled in
the motherboard bios&per.

:p. 2&per. The USB drivers by IBM work best with the most recent os2krnl, os2ldr, and ibmdasd
drivers&per. Install them next if you have not already done so&per.

:p. 3&per. Always update to IBM&apos.s latest USB drivers&per. Reboot and test USB devices&per. If they work,
you need go no further&per. If not, proceed to to next step&per.

:p. 4&per. Check to make sure that you have lines similar to those listed below in your config&per.sys&per.
I like using the /V option because it tells one whether the necessary drivers are loaded
or not at bootup&per.  The /V option can always be removed later when your USB device are
working properly&per.

:p.REM BASEDEV=USBUHCD&per.SYS /V
.br
BASEDEV=USBOHCD&per.SYS /V
.br
BASEDEV=USBOHCD&per.SYS /V
.br
BASEDEV=USBEHCD&per.SYS /V
.br
BASEDEV=USBD&per.SYS
.br
BASEDEV=USBHID&per.SYS
.br
BASEDEV=USBMSD&per.ADD /FLOPPIES&colon.0 /REMOVABLES&colon.2 /V
.br
rem BASEDEV=USBCDROM&per.ADD
.br

:p. Keep a copy of the file "hcimonit.exe" in a directory found in your config&per.sys Path statement&per.
This file normally comes with the IBM&apos.s USB drivers&per. This program will list the type of host
controllers found on your system&per.  At a command prompt, type "hcimonit.exe"&per.

:p. In the example below, this computer has 2 USB OHCI host controllers and 1 USB EHCI host
controller&per.  Thus, the following three lines would need to be found and if, not added
to the config&per.sys file&per.

:p.BASEDEV=USBOHCD.SYS /V
.br
BASEDEV=USBOHCD.SYS /V
.br
BASEDEV=USBEHCD.SYS /V
.br

:p. 5&per. Create a compatibility volume using LVM if you have not already done so&per. Check to see if
your drive is recognized by LVM&per. Removable drives usually have an * instead of a drive letter&per.
I suggest that you assign a permanent drive letter&per. I have found that some USB media won&apos.t be
recognized unless they are assigned a drive letter&per. If your media still is not recognized, go
to next step&per.

:p. 6&per. If the drive worked in the past, try running "chkdsk <drive>&colon. /F"&per.  Continue on if your
drive still doesn&apos.t work&per.

:p. 7&per. Check to see if you have the following statement in your config&per.sys file&per.

:p.BASEDEV=OS2PCARD.DMD
.br

:p. If it does exist, try remming it out and rebooting&per. Try your USB devices again&per. If your USB
devices are found and functioning properly, you need go no further&per.

:p.:hp2.Note&colon. :ehp2. It has been reported that the Card bus 8 driver does not conflict with the USB drivers&per.
However, other problems are introduced&per. eCS 1&per.2 comes with the Card bus 5 driver&per. Thus, if you
are using eCS 1&per.2, you will either need to rem out the driver or install the Card bus 8 driver&per.
If you still don&apos.t have your device(s) working, read further&per.

:p. 8&per. Some USB flash media comes with security software&per. With this software on the media, the drive(s)
cannot be read by the IBM drivers (LVM included)&per. To overcome this, try the following&per.

:p. a&per. Get to a Windows machine and reformat the drive&per.

:p. b&per. Then boot to DFSee (in my opinion, this is a must utility for every OS/2 user)&per.
If you don&apos.t have it, download the latest version&per. It can be used in demo mode&per.

:p. c&per. Using DFSee, create a new Master Boot Record with the tables erased&per.

:p.DFSOS2&per.EXE > Mode=fdisk > New MBR code, ERASE tables > Select the correct drive > OK

:p.:hp2.Note&colon. :ehp2.   You possibly could skip step A and start with B&per. I have not tried it&per.

:p. 9&per. Use LVM to create a new partition&per. LVM (Logical Volume Manager) in this instance refers to
either the command line LVM or the WPS LVMGUI&per. Using the command line LVM, the partition must
be created while in "Physical View"&per.  Using the LVMGUI, the partition can be created from either
"Logical or Physical views"&per.

:p. 10&per. Make the drive Primary&per.

:p.11&per. Using LVM, make the media a compatibility volume&per. Using the command line LVM, the capibility
volume must be created in "Logical View"&per. LVMGUI can create the volume from either "Physical or
Logical" views&per.

:p.LVMGUI > Volume > Create Volume > Create non-bootable volume > Create Compatibility Volume

:p.12&per.Format the drive&per.

:p.The media can be formatted FAT, HPFS, FAT32, or JFS&per. Formatting FAT32 can be done numerous ways&per.
Presently, the only way to format a volume FAT32 under either eComStation or OS/2 is to use DFSee
and F32blank together&per. The procedure is as follows&colon.

:p. a&per. Find out the volume relevant data&colon. Heads, Sectors, Starting point and Size using DFSee
.br
 b&per. Feed F32Blank with that data to generate a file with blank FATs suitable for the volume
.br
 c&per. Detach the volume (with LVM it&apos.s called &apos.hide&apos. or similar)
.br
 d&per. Use DFSee to overwrite the volume with the file contents, using "wrim"&per.
.br
 e&per. Attach again the volume&per.
.br
 f&per. If after this you can&apos.t read/write properly the volume or it appears as not empty, then
   you MUST reboot and check it again&per.
.br
 g&per. If you don&apos.t like DFSee go and find something else capable of doing the job&per.
.br

:p.If this is too complicated for some people, the USB media can be formatted using one of the
Windows versions&per. Each Windows version has it own built in limitations&per.

:p.Win95R2 <= 16 GB
.br
Windows 98 second addition - Volumes < 128 GB and > 512 MB
.br
Windows ME - 512 GB to 2 TB.
.br
Windows XP <= 32 GB.
.br

:p.:hp2.Other formatting alternatives :ehp2.

:p.FreeDOS <= 16 GB
.br
Partition Commander versions 8 and 9 (Limitations unknown)
.br

:p.:hp2.Note&colon. :ehp2. After formatting, always run chkdsk under OS/2 if using the OS/2 FAT32 driver&per. This
is probably true for any filesystem used&per.

:p.If you still cannot get your drive to work, you can try the next steps&per.

:p.13&per. Download Chris Wohlgemuth&apos.s USB driver package (cw-usbmsd-v1_2b&per.zip) and install his
USB mass storage driver (CWUSBMSD&per.ADD), replacing the IBM driver&per. Copy his CWUSBMSD&per.ADD
driver to the OS2&bsl.BOOT directory&per. Rem out IBM&apos.s USBMSD&per.ADD driver&per. Add a line similar to
the following to your config&per.sys file&per. Be sure and read his documentation&per.

:p.:hp2.Example&colon. :ehp2.

:p.REM BASEDEV=USBUHCD&per.SYS
.br
BASEDEV=USBEHCD&per.SYS /V
.br
BASEDEV=USBOHCD&per.SYS /V
.br
BASEDEV=USBOHCD&per.SYS /V
.br
BASEDEV=USBD&per.SYS
.br
BASEDEV=USBHID&per.SYS
.br
REM BASEDEV=USBMSD&per.ADD /REMOVABLES&colon.3
.br
BASEDEV=CWUSBMSD&per.ADD /FLOPPIES&colon.0 /REMOVABLES&colon.4 /FIXED_DISKS&colon.2 /FORCE_TO_REMOVABLE
.br

:p.Adjust the REMOVABLES options to the max number of removables you my have plugged in
at one time&per. Be aware that LVM will still list drives whether you have them plugged
in or not&per. These drives will show up as 96 MB volumes&per.

:p.14&per. Reboot and see if your device(s) are now recognized&per. If not try the next step&per.

:p.15&per. Download M&per. Kiewitz&apos.s driver package, MMPORTv1&per.zip, from Hobbes&per. You will need
to replace IBM&apos.s USBD&per.SYS driver with Martin&apos.s USBD&per.SYS driver&per. Attempting to use
this driver has risks&per. To be safe, rename the IBM driver to something like
USBD_BCKUP&per.SYS&per. That way if something goes wrong, you can either boot to a
command prompt or a maintenance partition to copy the IBM driver back over
Martin&apos.s driver&per. Martin&apos.s driver can trap on boot up&per. What Martin&apos.s driver
does different is that it attempts to find USB media the Microsoft way instead
of the USB Standards way&per. When it works, it can be a life saver&per. If it doesn&apos.t
work with the latest IBM drivers, you might try downloading some of the earlier
versions of the USB drivers from the eComStation ftp site&per. The closer the IBM
drivers are to the date of his released driver, the greater the chance of his
driver working&per. Since he does not have access to the latest code, he had to use
the last available code from the DDK site&per. If you have a usbcalls&per.dll on
your eComStation system, replace it with Martin&apos.s fixed usbcalls&per.dll&per. The driver
has been patched to fix a nasty bug&per.

:p.Hopefully one of the above techniques will solve your problem&per.

:p.:hp2.IMPORTANT&colon. :ehp2.   If plugging the drive into different computers, make sure ALL the USB
drivers are the same on these systems&per. File corruption can result when using
different drivers and versions&per.

:p.Always "EJECT" media before removing (unplugging)&per.  To do otherwise can cause
file corruption requiring "chkdsk" to be run&per. If the corruption is too bad,
Windows&apos. "scandisk" will have to be run on the drive&per.  Be aware that Windows XP
FAT32 scandisk is broken&per.  It fails to fix problems that earlier versions of
Windows 98 and ME were able to fix&per.  It will no longer fix mismatched FAT chains&per.

:p.:link reftype=hd refid=70.See also TROUBLE SHOOTING :elink.
.br 
  
:h2 id=57 res=30051.Long File names

:p.:hp2.LONG FILE NAMES&colon. :ehp2. &per. 

:p.:link reftype=hd refid=58.COMPATIBILITY WITH WINDOWS :elink.

:p.:link reftype=hd refid=59.WINDOWS OS/2 CHARSET :elink.

:p.:link reftype=hd refid=60.OS/2 AND DOS SESSIONS :elink.

:p.:link reftype=hd refid=61.LFN TRANSFERS ACROSS A LAN :elink.
.br 

:h3 id=58 res=30052.Compatibility with Windows

:p.:hp2.COMPATIBILITY WITH WINDOWS&colon. :ehp2.  

:p.As far as the original author, Henk Kelder could tell, FAT32&per.IFS is fully 
compatible with the FAT32 support in Windows&per. VFAT longnames could be used on his PC
&per. Windows did not complain (anymore!) about long filenames created with FAT32
&per.IFS&per. The numeric tailed short names also seemed to work ok&per. (The numeric 
tail option cannot be switched off!) 

:p.File names are, as in Windows, case preserving (in OS/2 sessions)&per. 
Creating a name in lower case will result in the file having a VFAT longname, even if 
the name conforms to 8&per.3&per. The case will be preserved&per. 

:p.Last access dates are maintained by FAT32&per.IFS&per. (but not the last 
access time since Win95 doesn&apos.t support it) You can see these when using the 
detailed view of the drive object&per. 

:p.The possibility exists that files created by any system of the WinNT family 
such as Win2X and WinX can be recognized as having Extended Attributes by the FAT32 
driver&per. 

:p.Although this should be fixed in the latest Netlabs version, the possibility 
still remains that the FAT32 driver recognizes a file having a non-zero in its EA 
field as an EA file, even though such a file is not an EA file&per. Why? Only a file 
having 0x40 or 0x80 in its EA field should be recognized as an EA file&per. One cannot 
be sure that files created by any of the WinNT family do not have 0x40 or 0x80 in 
their EA fields&per. Fortunately, this is a remote possible&per.  If such a file is 
found, CHKDSK will complain about this&per. 

:p.See :link reftype=hd refid=63.&osq.The Mark Byte&osq. :elink.

:p.
:h3 id=59 res=30053.Windows OS/2 Charset

:p.:hp2.WINDOWS OS/2 CHARACTER SET&colon. :ehp2.

:p.OS/2 uses standard character sets&per. Such a character set is called a 
CODEPAGE&per. 

:p.By default Windows long file names (VFAT) are stored in UNICODE&per. 

:p.Since Warp 3 (fixpack 26?) OS/2 contains NLS support&per. :link reftype=hd refid=20045.CACHEF32&per.
EXE :elink.

:p.Keep in mind that a table for only ONE codepage is loaded&per. Should you 
change codepages (using CHCP) you must rerun :link reftype=hd refid=20045.CACHEF32 :elink.to load a new table, but keep in 
mind OS/2 keeps different codepages per session so if you use CHCP to change the CP 
that CP is only only valid for that session&per. 

:p.
:h3 id=60 res=30054.OS/2 and DOS sessions

:p.:hp2.OS/2 AND DOS SESSIONS&colon. :ehp2.

:p.In the initial release, long file names were only shown in OS/2 sessions, but 
in DOS sessions, the short filename equivalent was shown&per. 

:p.However, this could lead to big problems&per. 

:p.:hp2.Example&colon.   :ehp2.
.br 
In an OS/2 session, directory &bsl.DevStudio was current, from a DOS session the 
directory &bsl.DEVSTU~1 could be removed since OS/2 did not know that DevStudio and 
DEVSTU~1 were in fact the same directories&per. 

:p.The same problem could occur when opening files in both OS/2 and DOS&per. No 
proper multiuser handle would take place since OS/2 doesn&apos.t know that files with 
different names are the same&per. 

:p.To solve this problem I&apos.ve done the following&colon. 

:p.Via a setting, FAT32&per.IFS can be told to&colon. 

:p.Translate all long filenames internally to their short equivalences OR 
.br 
Use the long names internally, but hide all files or directories with long 
names from DOS sessions&per. 
.br 
This setting can be changed (on the fly) with :link reftype=hd refid=20045.CACHEF32&per.EXE:elink.&per. 

:p.When using short names internally the following drawbacks occur&colon. 

:p.Current directory is shown as a short name (command line only) 
.br 
When deleting (long) files from the command line, the WPS doesn&apos.t pickup 
the deletions&per. 
.br 

:p.When using long names internally the following drawbacks occur&colon. 

:p.Files and directories with long names are not visible in DOS sessions&per. 
However, this is the same as with HPFS&per. 
.br 

:p.
:h3 id=61 res=30055.LFN Transfers Across a LAN

:p.:hp2.LFN TRANSFERS ACROSS A LAN&colon. :ehp2.

:p.The FAT32 driver has nothing, at all, to do with the Windows machine serving 
up short file names to an OS/2 machine, across a LAN&per. That problem is still 
there, and it is caused by the Windows LAN support&per. 

:p. In short&colon. if the file operations are performed by a Windows machine on 
an OS/2 machine, everything will go fine&per. Operations made by an OS/2 PC 
against Windows remote machines are likely to suffer problems with long file names&per. 
To be more precise&colon. 

:p.If you have Windows NT, Windows 2000, Windows XP or newer, your long file 
names (LFNs) are fully interoperable between OS/2 and a Windows system over a 
network&per. 

:p.If your Windows system is older than that (Windows 95, 98, and ME), you can 
work flawlessly with LFNs on a remote OS/2 machine from the Windows PC; however, 
it doesn&apos.t work the other way round&per. I&per.e&per. the Windows remote 
machine will show LFNs to its other Windows pals in the network, but your OS/2 
machine will be served just short filenames instead&per. 

:p.:hp2.Why? :ehp2.

:p.This is because of how each PC on the LAN identifies itself and its 
capabilities to the other PCs in the network&per. OS/2 says to the other machines it uses 
LM10 for compatibility&apos.s sake, which makes win9x machines treat this one like 
it were DOS+W3&per.11 (i&per.e&per. no LFNs)&per. According to IBM, it can do LM30 
perfectly - but do NOT try to patch the binaries or you&apos.ll get a trap&per.  Oh well, 
this is just plain IBM&apos.s fault&per. 

:p.OK, OS/2 says it&apos.s LM10 and we can&apos.t blame Microsoft for that
&per. However, other LAN OS/2 pals have no difficulties whatsoever identifying the 
OS/2 behind this &apos.LM10&apos. label and allowing LFN operation without trouble
&per. 

:p.As of 2003 Q2 there hasn&apos.t been a release of this patch for win9x 
networking (as a patch for the main OS network subsystem, perhaps it is in the form of 
some BM windows client for OS/2 peer or something, i&per.e&per. not from M$ 
themselves)&per. 

:p.
:h2 id=62 res=30056.Extended Attributes

:p.:hp2.EXTENDED ATTRIBUTES&colon. :ehp2.

:p.Since version 0&per.70, FAT32&per.IFS supports EXTENDED ATTRIBUTES&per. 

:p.For FAT32&per.IFS to support Extended Attributes /EAS MUST be specified after 
the IFS=&per.&per.&per.&per.&bsl.FAT32&per.IFS line in the config&per.sys&per. 

:p.Extended Attributes are implemented in the following manner&colon. 

:p.For each file or directory that has extended attributes, a file is created 
with a name that consists of the file or directory name the EAs belongs to followed 
by &apos. EA&per. SF&apos.&per.  So if a file called &apos.FILE&apos. has extended 
attributes, these attributes are stored in a file called &apos.FILE EA&per. SF&apos.&per. 

:p.These EA files are given the hidden, read-only and system attributes&per. 

:p.FAT32&per.IFS will not show these files in a directory listing, but Win9x and 
later can show them&per. 

:p.:hp2.IMPORTANT&colon.  :ehp2.:hp2.Starting with version 0&per.97, CHKDSK /F must first be run 
on each FAT32 partition before adding EA support to the FAT32 IFS driver&per. This 
step is required starting with this version due to the EA mark byte being changed 
for compatibility with the WinNT family&per. 

:p.You can use F32STAT&per.EXE to run CHKDSK automatically on next boot by doing 
the following&colon. 

:p.F32STAT&per.EXE x&colon. /DIRTY 

:p.Where, x&colon. is your FAT32 drive which has files with EA&per. 

:p.F32STAT&per.EXE supports only one drive at one time&per. So if you want to 
change the state of one more drive, you should run F32STAT&per.EXE, respectively&per. 
i&per.e&per., FAT32 drive is C&colon. D&colon.&per. You should run F32STAT&per.
EXE two times for C and D as the following&per. 

:p.F32STAT&per.EXE C&colon. /DIRTY 
.br 
F32STAT&per.EXE D&colon. /DIRTY :ehp2.

:p.

:p.:link reftype=hd refid=63.THE MARK BYTE :elink.

:p.:link reftype=hd refid=64.THE DRAWBACKS :elink.

:p.:link reftype=hd refid=65.THE ADVANTAGES :elink.
.br 

:h3 id=63 res=30057.The Mark Byte

:p.:hp2.THE MARK BYTE&colon. :ehp2.

:p.To speed things up a bit, each file having extended attributes is marked by 
FAT32&per.IFS&per. For this mark, an apparent unused byte in the directory entry is 
used&per. The value for this byte is set to 0x40 for files having normal EAs, to 
0x80 for files having critical EAs, and to 0x00 for files not having EAs at all&per. 

:p.(Please note that files with critical EAs can not be opened by programs not 
able to handle EAs, like DOS programs&per.) 

:p.This byte (directly following the files attribute) is not modified while 
running Windows and by neither SCANDISK or DEFRAG, but theoretically, other programs 
running under Windows could modify it&per. 

:p.If another program sets the value to 0x00 for a file that has EAs these EAs 
will no longer be found using DosFindFirst/Next calls only&per. The other OS2 calls 
for retrieving EAs (DosQueryPathInfo, DosQueryFileInfo and DosEnumAttribute) do not 
rely on this byte&per. 

:p.Also, the opposite could, again, theoretically occur&per. Files not having 
EAs could be marked as having EAS&per. In this situation only the performance of 
directory scans will be decreased&per. 

:p.However, both situations are checked and if necessary corrected by CHKDSK
&per. 

:p.:link reftype=hd refid=64.THE DRAWBACKS :elink.

:p.:link reftype=hd refid=65.THE ADVANTAGES :elink.
.br 

:h3 id=64 res=30058.The Drawbacks

:p.:hp2.THE DRAWBACKS&colon. :ehp2.

:p.Currently, the drawback of using Extended Attributes is that directory scan 
performance has slightly decreased&per. 

:p.The overhead on opening or accessing individual files is hardly noticeable
&per. 

:p.If you do not really need extended attribute support then simply do not 
specify /EAS after the IFS line in the config&per.sys&per. 

:p.:link reftype=hd refid=63.THE MARK BYTE :elink.

:p.:link reftype=hd refid=65.THE ADVANTAGES :elink.
.br 

:h3 id=65 res=30059.The Advantages

:p.:hp2.THE ADVANTAGES&colon. :ehp2.

:p.The advantages of FAT32&per.IFS supporting extended attributes are&colon. 

:p.The WPS heavily uses EAS to store folder and file settings&per. Without EAS, 
the WPS will not remember settings across boots&per. 
.br 
REXX &per.CMD files must be tokenized on each run, thereby reducing performance
&per. With EAS the tokenized version of the &per.CMD will be stored in EAs&per. 
.br 

:p.If you can live with the small loss in performance while doing directory 
scans, it is advised you specify /EAS after the IFS line in the CONFIG&per.SYS&per. 

:p.If you do not really need extended attribute support, and you cannot accept 
the decrease in directory scan performance, then simply do not specify /EAS after 
the IFS line in the config&per.sys&per. 

:p.:link reftype=hd refid=63.THE MARK BYTE :elink.

:p.:link reftype=hd refid=64.THE DRAWBACKS :elink.
.br 

:h2 id=66 res=30060.Driver Interface

:p.:hp2.DRIVER INTERFACE&colon. :ehp2.

:p.:link reftype=hd refid=67.IOCTL SUPPORT :elink.

:p.:link reftype=hd refid=68.SUPPORTED FUNCTIONS :elink.
.br 

:p.
:h3 id=67 res=30061.IOCTL Support

:p.:hp2.IOCTL SUPPORT&colon. :ehp2.

:p.IOCTL calls (category 8) are now passed through to OS2DASD&per.  All calls 
supported by OS2DASD&per.DMD are now also supported by the IFS&per. 

:p.:link reftype=hd refid=68.SUPPORTED FUNCTIONS :elink.
.br 

:h3 id=68 res=30062.Supported functions

:p.:font facename='Helv' size=14x14.:hp2.SUPPORTED FUNCTIONS&colon. :ehp2.

:p.:hp7.Functions :ehp7.:hp7.Yes/No/Other :ehp7.

:p.FS_ALLOCATEPAGESPACE&colon. No 

:p.FS_ATTACH&colon. No 

:p.FS_CANCELLOCKREQUEST&colon. No, function is implemented in the KERNEL 

:p.FS_CHDIR&colon. Yes 

:p.FS_CHGFILEPTR&colon. Yes 

:p.FS_CLOSE&colon. Yes 

:p.FS_COMMIT&colon. Yes 

:p.FS_COPY&colon. Partly, unsupported actions are simulated by command shell 

:p.FS_DELETE&colon. Yes 

:p.FS_DOPAGEIO&colon. No 

:p.FS_EXIT&colon. Yes 

:p.FS_FILEATTRIBUTE&colon. Yes 

:p.FS_FILEINFO&colon. Yes 

:p.FS_FILEIO&colon. No 

:p.FS_FILELOCKS&colon. No, function is implemented in the KERNEL 

:p.FS_FINDCLOSE&colon. Yes 

:p.FS_FINDFIRST&colon. Yes 

:p.FS_FINDFROMNAME&colon. Yes 

:p.FS_FINDNEXT&colon. Yes 

:p.FS_FINDNOTIFYCLOSE&colon. Obsolete in OS/2 WARP 

:p.FS_FINDNOTIFYFIRST&colon. Obsolete in OS/2 WARP 

:p.FS_FINDNOTIFYNEXT&colon. Obsolete in OS/2 WARP 

:p.FS_FLUSHBUF&colon. Yes 

:p.FS_FSCTL&colon. Yes 

:p.FS_FSINFO&colon. Yes 

:p.FS_INIT&colon. Yes 

:p.FS_IOCTL&colon. Yes - LOCK &caret. UNLOCK, others are passed to OS2DASD&per. 

:p.FS_MKDIR &colon. Yes 

:p.FS_MOUNT&colon. Yes 

:p.FS_MOVE&colon. Yes 

:p.FS_NEWSIZE&colon. Yes 

:p.FS_NMPIPE&colon. No 

:p.FS_OPENCREATE&colon. Yes 

:p.FS_OPENPAGEFILE&colon. No 

:p.FS_PATHINFO&colon. Yes 

:p.FS_PROCESSNAME&colon. Yes 

:p.FS_READ&colon. Yes 

:p.FS_RMDIR&colon. Yes 

:p.FS_SETSWAP&colon. No 

:p.FS_SHUTDOWN&colon. Yes 

:p.FS_VERIFYUNCNAME&colon. No 

:p.FS_WRITE&colon. Yes 
.br 

:p.:link reftype=hd refid=67.IOCTL SUPPORT :elink.
.br 

:h2 id=69 res=30063.Performance

:p.:hp2.PERFORMANCE&colon. :ehp2.

:p.All of the code is in plain 16 bits C (All of OS/2&apos.s IFS&apos.s are 16 
bits!)&per. No assembly language code is used&per. 

:p.The :link reftype=hd refid=20052.F32MON :elink.function takes a lot of time&per. Be sure to switch if off if you 
don&apos.t need it&per. 

:p.You should probably experiment with the :link reftype=hd refid=46.CACHEF32 options :elink.to get the best 
performance for your situation&per. 

:p.The default for the LAZY WRITER is idle-time priority (/P&colon.1)&per. You 
might like to experiment with the /P option as well, especially if you have 
performance problems with :link reftype=hd refid=20042.FAT32&per.IFS:elink.&per. 

:p.For best performance it is advised to keep the disk as defragmented as 
possible&per. Use Windows 9x or later versions defrag to defrag the disk&per. 

:h2 id=20069 res=32063.FAQ

:p.:hp2.FAQ&colon. :ehp2.

:p.:hp2.Q:ehp2.&colon. Why did you not created three different IFS drivers for FAT, FAT32, EXFAT?
.br
:hp2.A:ehp2.&colon. Because all FAT flavours are very similar&per. Many code used by each of them is common&per. The differences
are minor&per. FAT drives differ from FAT32 ones only in that&colon. 1) BIOS parameter block is different, 2) Root
directory is located in reserved area, so it is not assigned cluster numbers. We were needed to handle FAT root directory
specially, while in FAT32 it starts in common cluster heap area, usually, with cluster no&per. 2&per. 3) FAT entries
in FAT12 are 12-bit (so, two FAT entries compose three bytes)&per. In FAT32 they are all 32-bit&per. Otherwise, all three
FAT flavours are the same&per. VFAT long filenames are the same in FAT12/16/32&per. Only EXFAT has different LFN format&per.
Many code is common&per. So, creating three or four almost identical drivers is a waste of machine
resources and unneeded duplication&per. EXFAT is more different that the first three FS&apos.es&per. It has different boot
sector structure, different FAT bitness (it is 32-bit, like FAT32 is, but it uses all 32 bits of each FAT entry&per. FAT32
uses only first 28 bits of a FAT entry&per. The 4 most significant bits are zeroes&per.)&per. Also, it introduces a bitmap
for tracking a free space, which is used for that, instead of a FAT table. FAT table is not used at all if a file is marked
as non-fragmented. So, no need to follow a FAT chain if a file is non-fragmented. You just need to know a starting cluster,
and a file length, to correctly read such a file&per. And finally, EXFAT uses different directory entry format, than the
first three FAT flavours&per. But still, the amount of modifications is not too big: fat32.ifs without EXFAT support is just
less than 20 KBytes smaller than a full-featured version&per. Also note that for the same reasons, FAT32&per.IFS uses a common
UUNIFAT&per.DLL utility DLL with four forwarders&colon. UFAT12&per.DLL, UFAT16&per.DLL, UFAT32&per.DLL, UEXFAT&per.DLL&per.

.br
:p.:hp2.Q:ehp2.&colon. Why FAT32&per.IFS always shows a file system name being FAT32? Why not show the actual file system type?
.br
:hp2.A:ehp2.&colon. What is shown in file managers&apos. info panels is a value returned by a DosQueryFSAttach API&per. It is
taken by the kernel from an IFS&apos. FS_NAME exported variable&per. (Please read about it in IFS&per.INF file)&per. This value
is 8 bytes long and should not change since IFS is initialized&per. So, we are unable to change it to the "actual" file system
name&per. It is a string used for IFS identification&per. If an IFS supports more than one filesystem, this variable should be 
the same&per. Examples of such IFS&apos.es are Netdrive and vfat-os2&per.ifs&per. Netdrive always shows "Netdrive" as its 
identification&per. Vfat-os2&per.ifs used the "vfat" word for that purpose&per. Except for VFAT drives, it also supported
limited read only NTFS access&per. But the IFS identification still was "vfat"&per. The same is done by FAT32&per.IFS&per.
There were suggestions to divide it to three different IFS drivers, for FAT, FAT32 and EXFAT&per. But this is not feasible
for the reasons, mentioned in previous question/answer&per.

.br
:p.:hp2.Q:ehp2.&colon. How do I determine the actual file system type of my media?
.br
:hp2.A:ehp2.&colon. The only means to determine the actual file system type is to run CHKDSK (probably, without the /f 
parameter)&per. Then you can see the actual FS type is its report, at the beginning (please, look at 
:link reftype=hd refid=30047.CHKDSK :elink. article)&per. After a report header appears, you can press "Ctrl-C" to interrupt CHKDSK&per.

.br
:p.:hp2.Q:ehp2.&colon. Why did you supported EXFAT? It is proprietary Microsoft technology and they extort money from anybody
using this technology in his products&per. Do you fear Microsoft?
.br
:hp2.A:ehp2.&colon. Yes, we beware the money extortion (called a license fees) from Micro$oft side&per. Nobody is insured 
from that&per. But, for free software, which is FAT32&per.IFS, it should be no problem&per. Free versions of Linux, like Debian
is, are safely distributing a (FUSE) version of their EXFAT driver&per. Commercial Linux vendors, Like SuSe and Redhat, are paying
MS license fees, it seems (or don't distribute an EXFAT driver with their OS&apos.es)&per. Arca Noae, being a commercial OS/2 vendor,
wants to be as far as possible from patented MS stuff&per. But we made, by their request, a version of FAT32&per.IFS without an
EXFAT support&per. We hope it will be sufficient for them to avoid MS claims&per. Anyway, MS caused the IT industry to use their 
patented proprietary EXFAT on SDXC cards (all SD cards larger than 32 GB)&per. So, if we'll not have support for it in our OS,
we would not use any newer devices with OS/2, like cameras, video registrators, smartphones etc. With FAT32 only, we should stick
with files not exceeding 4 GB (fat32.ifs now supports up to 4 GB&per. Previously, it supported only 2 GB files, because it is a 16-bit 
driver&per. Now with EXFAT support, more than 4 GB is supported, so it should be not a problem)&per. Without support for files
> 4GB, the video recording in newer cameras would be limited with about 30 mins of video&per. This is if you use high quality recording&per.
Additionally, some devices are not guaranteed to support FAT32 on SDXC&per. They could support EXFAT only, and this could be
a big problem without EXFAT support&per. So, EXFAT support is important, indeed, despite the fact that it is patented and
proprietary&per.

.br
:p.:hp2.Q:ehp2.&colon. Why cannot I seek beyond a 2 GB limit on large files?
.br
:hp2.A:ehp2.&colon. Because support for 64-bit seek seems to be incomplete for 16-bit IFS drivers&per. The sequential
file access works (I can copy big files to/from a FAT32/exFAT drive, but I cannot enter big &per.zip files, for example)&per.
For that, there is a FS_CHGFILEPTRL export in FAT32&per.IFS, but it does not get called by a kernel&per. The kernel calls
the standard FS_CHGFILEPTR entry point, which is limited to max&per. 2 GB file position&per. The file position gets truncated&per.
The 32-bit FS32_CHGFILEPTRL function with 32-bit IFS&apos.es, though, works&per. This seems to be an unfinished feature&per. So,
I see two solutions here&colon. 1) convert FAT32&per.IFS to a 32-bit driver, or, at least, export some necessary 32-bit
entry points&per. 2) modify OS/2 kernel so that it will call FS_CHGFILEPTRL, as required&per. The latter variant is possible
if I'd convince OS/4 developers to do such an enhancement&per. But it will not work for those who stick with IBM&apos.s kernels&per.

.br
:p.:hp2.Q:ehp2.&colon. Why kLIBC symlinks are not working with FAT32&per.IFS?
.br
:hp2.A:ehp2.&colon. kLIBC developer thinks that EA support is inefficient on FAT32&per.IFS, so he 
disabled Unix EA&apos.s (for Unix access attributes, symlinks and similar functionality) on 
FAT32&per.IFS&per. In the next kLIBC versions, this functionality can be selectively enabled/disabled 
per drive&per. But by now, it is disabled in kLIBC&per. So, this is not a FAT32&per.IFS problem&per.
All required prerequisites of FS are present&per. Only EA support is required for Unix access permissions,
symlinks ans similar functionality&per. As far as I can see, EA support is good enough in FAT32&per.IFS&per.
At least, I can copy OS/2 desktop to FAT32 or exFAT, and it works quite well&per. WPS objects are working,
Volkov Commander and Dos Navigator in a DOS session see the &per.LONGNAME EA's and show long file names&per.
This works quick enough&per. What Knut meant by "inefficiency", I don&apos.t know, perhaps, this is maybe
FAT32&per.IFS is slow because of lack of proper caching&per. Otherwise, it works quite well&per. I hope
this will work in the future&per. I have my OS/2 bootable flash stick, which has kLIBC port tree 
installed&per. So, I&apos.d like to have symlinks working on it&per. But unfortunately, it 
doesn&apos.t&per.

:h2 id=20070 res=32064.HOWTO&apos.s

:p.:link reftype=hd refid=200699.How to prepare flash sticks for use with OS/2 :elink.

:p.:link reftype=hd refid=200700.Using a 4 GB FAT16 partition with 64 KB cluster for StandAlone DUMPs :elink.

:p.:link reftype=hd refid=200701.VFDISK&per.SYS/VDISK&per.SYS/SVDISK&per.SYS/HD4DISK&per.ADD virtual disks :elink.

:p.:link reftype=hd refid=200702.Support for files > 4 GB on FAT/FAT32 (FAT+) :elink.

:p.:link reftype=hd refid=200703.Notes on large floppy media&per. :elink.

:p.:link reftype=hd refid=200704.Using CDRW&apos.s with FAT16/FAT32/exFAT filesystem&per. :elink.

:p.:link reftype=hd refid=200705.Debugging hangs or traps, or any other problems&per. :elink.

:p.:link reftype=hd refid=200706.Working with FAT filesystems, mounted to subdirectories&per. :elink.
.br 

:h3 id=200699 res=32062.How to prepare flash sticks for use with OS/2

:p.:hp2.How to prepare flash sticks for use with OS/2&colon. :ehp2.

:p.File systems have their &apos.FS_MOUNT&apos. routines called by the kernel&per. 
The kernel calls them in sequence, until some filesystem returns successful status&per.
If no IFS&apos.es returned success, the fallback filesystem is called, which is in-kernel
FAT driver&per. Also, in LVM-based systems, a drive letter is assigned according to LVM
DLAT tables (Drive Letter Assignment Tables). If these tables are missing, the kernel
only seems to call the in-kernel FAT driver&per. The same happens in case the kernel
(together with OS2DASD) detects that the unit is a large floppy&per. 

:p.So, the in-kernel FAT driver gets mounted&per. Because media is not FAT indeed, 
it cannot be accessed&per. So, it looks like the drive is empty&per. When you open 
such drive in FC/2, or change to this drive as a current one in CMD&per.EXE, 
ERROR_NON_DOS_DISK == 26  is displayed&per. This means that no IFS had been 
mounted this drive&per. The in-kernel FAT driver attempted to mount it, and 
it doesn't recognise it, hence returning the ERROR_NON_DOS_DISK error&per. 
So, this situation can be easily recognised by its error code, 26&per.

:p.To get your filesystem mounted by your IFS, you should assign it an explicit
drive letter&per. The drive letter can be assigned via LVM&per.EXE, or with DFSee,
or any similar tool&per.

:p.LVM&per.EXE is, however, very picky regarding the disk geometries in the
Partition Table&per. IBM just refuses to do anything with the disk with a geometry
disliked by LVM&per.EXE&per. IBM has its own reasons to do so, for example, they
don't want to be responsible for any possible data losses&per. But this can cause
many problems with flash sticks, or with modern hard disks with ADF (Advanced
Disk Format) technology&per. Such disks have weird geometries&per. Also, all large
disks (larger than 512 GB) are assigned weird geometries too&per. Disks of sizes
from range 512 GB..1 TB have 127 sectors per track&per. Disks of sizes 1 TB..2 TB
have 255 sectros per track&per. The usual sectors per track value is 63, and it
is the only value which LVM.EXE accepts&per. So, for such disks, another tool is
required&per. Such tools are&colon. 

:ol.
:li.ALVM (Modified LVM by Evgen Kotsuba), which has some relaxed restrictions, 
compared to IBM&apos.s version of LVM&per.EXE&per.
:li.MiniLVM, written by Alex Taylor specially for eComStation&per.
:li.Disk manager from QSINIT, by _dixie_
:li.DFSee, written by Jan van Wijk
:eol.

:p.From these four utilities the most universal and powerful is DFSee&per.
It allows to do many useful operations, like&colon.

:ul.
:li.Wipe sectors on disk
:li.Rewrite MBR (either with new boot code, keeping partition tables, or vice
versa, erasing the Partition Table)
:li.Fix some fields in bootsector, like Hidden Sectors, and other values
:li.Editing sectors
:li.Editing partition tables
:li.Creating/editing LVM info
:li.Creating or deleting partitions
:li.Formatting FAT32 file systems
:li.etc etc etc
:eul.

:p.The most common operation when preparing flash disks for use with OS/2 is
assigning/editing the LVM info&per. In most cases it is sufficient to make a
friend&apos.s or colleague&apos.s flash stick accessible from your OS/2 system&per. Note
that DFSee allows to perform this operation nondestructively, so that usually,
no need to erase the beginning of the stick (which could be not acceptable if
that stick is not your own one, but friend&apos.s, colleague&apos.s or your
client&apos.s)&per. This is because LVM info resides in an unused disk area,
like a sector in zero disk track right before the boot sector&per.

:p.I did this operation with DFSee many times, and I never had problems with
that&per. This can be done with any existing file systems like FAT/FAT32/exFAT/
NTFS/HPFS/JFS/etc&per. DFSee is better than other tools because it can
accept various disk geometries, in cases when other tools refuse to do so&per.
They simply won&apos.t allow you to do anything with your stick, and scream
about incorrect disk geometry, corrupted disk etc&per.

:p.Some problems can occur in some rare cases, like Big Floppy media (there
is a workaround for this problem, see the :link reftype=hd refid=200703.Notes 
on large floppy media&per. :elink. section for more details) or another 
strange case&colon. I had a SanDisk stick with mismatched "Hidden Sectors" 
in the boot sector, and the offset of the beginning of that partition in 
the Partition Table&per. The former was a standard 63 sectors, but the 
latter was some bigger value, aligned to flash controller block size&per. 
This was ignored on other OS&apos.es, but it confused OS/2 disk subsystem&per. 
So, unfortunately, I needed to erase and repartition this stick, after 
which it worked fine under OS/2&per.

:p.However, it seems to be very rare unusual case&per. Most flash sticks
can be made working under OS/2 by only assigning the LVM info with an 
explicit drive letter&per.

:p.To add the LVM info, you need to call the Edit->LVM Information->...&per.
Pick your disk from the pull-down menu&per. Here, the partition name, volume name 
and a drive letter can be set up&per. 

:p.It is recommended to choose the explicit drive letter for your disk&per.
This drive letter is carried together with your flash stick to any machine
you plug it in&per. If this drive letter is occupied on that machine, the
next available drive letter will be chosen&per. Also, it is possible to 
select a "*:" as a drive letter&per. This instructs LVM to assign the first
available drive letter&per. Though, an explicit drive letter should be preferred,
because "*:" may not work reliably in some cases&per.

:h3 id=200700 res=32065.Using a 4 GB FAT16 partition with 64 KB cluster for StandAlone DUMPs

:p.:hp2.Using a 4 GB FAT16 partition with 64 KB cluster for StandAlone DUMPs&colon. :ehp2.

:p. It was discovered by me, that standard FAT OS2DUMP can be used to dump to a 4 GB FAT16 partition with 64 KB
clusters&per. So, you create a 4 GB partition, with BIOS type 6 (FAT16 BIG), and a volume label "SADUMP"&per.
Such a file system can be created with a command&colon.

.br
.br

:p.:hp2. format /fs&colon.fat16 /c&colon.128 /v&colon.sadump :ehp2.

.br
.br

:p. Where 128 is the number of sectors per cluster&colon. 64 KB == 128 sectors&per.

:p. Then you add the line&colon.

.br
.br

:p.:hp2. TRAPDUMP=R0,I&colon. :ehp2.

.br
.br

:p. to your CONFIG&per.SYS (where I&colon. is your 4 GB FAT16 drive) and reboot&per. You can test if creating
the standalone dump begins by pressing a Ctrl-Alt-NumLock-NumLock (or Ctrl-Alt-F10-F10)&per.

:p. The dumps of > 2GB are created successfully&per. I have 4 GB of RAM and a 3.7 GB dump file
creates just fine&per. The 4 GB FAT16 SADUMP partition is mounted fine by FAT32&per.IFS too&per.
It can then be copied to a JFS partition and viewed by pmdf&per.exe&per.

:p. Note that at the moment, in FAT32&per.IFS, 64-bit seek does not work because a FS_CHGFILEPTRL
routine does not gets called by the kernel&per. It looks that Scott Garfinkle not finished his 64-bit
file support for 16-bit IFS&apos.es, which he began making with DUMPFS&per.IFS&per.

:p. DUMPFS&per.IFS is a pure 16-bit IFS with support of files larger than 2 GB&per. Like with FAT32&per.IFS,
big files can be copied successfully from a DUMPFS drive, but cannot be seeked beyond the 2 GB limit&per.
It looks that Scott implemented some minimal functionality and did not finished the seek support&per.
So, the 64-bit seek works only for 32-bit IFS-es, at the moment&per.

:p. As you could notice, you can now use FAT32&per.IFS instead of DUMPFS&per.IFS&per. 

:p. The only problem with this is that if your machine has 4 GB of RAM, the full memory dump takes 
about a 30 minutes, which is very long&per. So, I'd advice to limit the available RAM to say, 512 MB,
using a QSINIT loader (aka "Tetris")&per. This can be achieved by adding a ",memlimit=512" string to
kernel parameters&per. After taking a dump, you can remove the "memlimit" string from os2ldr&per.ini,
or leave it in a special menu item line which is used specially for dumping memory&per.

:h3 id=200701 res=32066.VFDISK&per.SYS/VDISK&per.SYS/SVDISK&per.SYS/HD4DISK&per.ADD virtual disks

:p.:hp2.VFDISK&per.SYS/VDISK&per.SYS/SVDISK&per.SYS/HD4DISK&per.ADD virtual disks :ehp2.

:p. These drivers should work ok with latest FAT32&per.IFS versions&per. There are no special tips, everything
should work out of the box&per. You just enable FAT support in FAT32&per.IFS via the /fat command line switch&per.
FAT32&per.IFS then should mount on all FAT drives, including physical and virtual floppies&per. Also, FAT32 or exFAT
ramdisks, like PAE ramdisk, or any other &per.ADD-based virtual harddisk, should work&per. Successfully tested so far
are FORMAT/CHKDSK/SYSINSTX/LOADDSKF/SAVEDSKF/DISKCOPY on such drives&per. It should work the same way IBM&apos.s FAT works,
the bonus feature is VFAT long file names support&per.

:h3 id=200702 res=32067.Support for files > 4 GB on FAT/FAT32 (FAT+)

:p.:hp2.Support for files > 4 GB on FAT/FAT32 (FAT+) :ehp2.

:p. DOS people were always limited with FAT filesystem (FAT12/FAT16/FAT32) and it was always a big problem
that the file size is limited to a 32-bit value in FAT&per. Video files and VM images are known examples of
big files, for which a 32-bit value (which limits the file size to 4 GB) is insufficient&per. So, DR-DOS developers
found out a way to store the files bigger than 4 GB on FAT volumes&per. The specification was called FAT+&per.
This specification (http&colon.//www&per.fdos&per.org/kernel/fatplus&per.txt) adds 6 more bits to a file size
by using some reserved bits in the directory entry&per. 2^(32+6) == 256 GB is maximum file size allowed. The FAT
chain size itself is not limited by the FAT spec, limited is only the file size, which is addressed in the FAT+ spec&per.

:p.The reserved byte is 12-th from the beinning of a directory entry. Two bits of this byte, 3rd and 4th are
reserved by WinNT for specifying that the file name or extension of this file is lower case (no LFN&apos.s are used,
the file has 8&per.3 filename)&per. The bits 6th and 7th are used by fat32&per.ifs for marking file as having EA&apos.s
or having critical EA&apos.s&per. For files having EA&apos.s, bits 6th and 7th are occupied too&per. So, bit 5th now
means that the file size is set in special EA&per. The FAT+, version 3 spec suggests to use a FAT+FSZ EA for storing
the 64-bit size value in EA&apos.s&per. As I can see, "+" sign in the EA name gives a ERROR_INVALID_EA_NAME error
when trying to manipulate with such a file&per. So, I decided to use the FAT_PLUS_FSZ EA instead (which violates
current spec, but it seems the spec is incorrect here, because it sugests to use an invalid EA name)&per.

:p. So far, FAT+ support is now working&per. I was able to create a 40 GB file indeed, and it could be copied to
another volume&per. The FAT+ support is enabled by the "/plus" command line switch&per. Note that /plus switch 
enables large files support too&per. Also, storing files with more than 35-bit size requires EA support (/eas switch)
to be enabled&per. 35-bit size uses bits 0-2 of 12th reserved byte (EA mark byte)&per. 2^35 == 32 GB is the maximum
file size with EA&apos.s disabled&per.

:h3 id=200703 res=32068.Notes on large floppy media&per.

:p.:hp2.Notes on large floppy media&per. :ehp2.

:p. Media without any partitions created, but with a single file system, is called Large Floppy&per.
Such media has no partition table and is beginning from a boot sector, like ordinary floppy disk, but
it is much larger (some gigabytes)&per.

:p. Such media is frequently created by Windows&per. As it was known, OS2DASD&per.DMD/OS2LVM&per.DMD think
that large floppies must be < 2 GB and formatted with FAT16&per. So, when OS/2 sees a big floppy, it mounts
the kernel FAT driver onto it&per. So, there is currently no way to make OS/2 properly understand Large Floppies,
except for OS2DASD/OS2LVM rewriting&per.

:p. But there is a workaround created by Lars Erdmann in his version of usbmsd&per.add&per. Lars emulates the
0 track together with partition table and DLAT sector with LVM info with default drive letter&per. So, the system
actually sees some emulated partitionable medium, instead of the Large Floppy&per. This is done transparently
when usbmsd&per.add sees the media beginning from the boot sector&per. Such media can be repartitioned by
erasing the boot sector with zeroes&per. Then usbmsd&per.add disables emulation and it then works as usual&per.

:p.Large floppies can be safely formatted with FAT16/FAT32/exFAT and used with fat32&per.ifs&per.

:p.To create a Large Floppy in OS/2, you&apos.ll need to erase the medium with DFSee first&per. For that,
you need to select the whole disk with File->Open object to work with->Disk->Your disk&per. Then wipe it
with command&colon.

:p.:hp2. wipe "" 0 2048 :ehp2.

:p. where "" is an empty pattern&per. "0" is a starting sector, and "2048" is wipe size in sectors&per. After
that, you can quit DFSee with "q" command, eject the disk, reinsert it and then you can format it again as a
whole with the

:p.:hp2. fat32fmt o&colon. /fs&colon.fat32 /v&colon.<volume label> :ehp2.

:p.command&per. Note that the FORMAT command will not work in this case, because FORMAT&per.COM frontend does not
like such media and will show an error&per.

:p.If you want to reformat the medium back as PRM, you&apos.ll need to erase it again, eject, reinsert it,
create a partition on it, then reformat with FORMAT command&per.

:h3 id=200704 res=32069.Using CDRW&apos.s with FAT16/FAT32/exFAT filesystem&per.

:p.:hp2.Using CDRW&apos.s with FAT16/FAT32/exFAT filesystem&per. :ehp2.

:p.It was discovered that if I create a FAT filesystem image with 2048 bytes per sector (it can be done by
QSINIT bootloader utilities, for example), and burn it onto a CDRW disk, it can be successfully mounted by
FAT32&per.IFS&per. Moreover, Windows understands such CD&apos.s too&per. This could be treated as just a
curious fact, but it can serve as a test for non-512 byte sectors support, too, which is very useful&per.

:p.Also, with a little additional code for reading/writing CD&apos.s sectors (which was actually taken as is
from my VirtualBox port, where you can run CD writing software in VBox, and burn CD&apos.s with a CD passthrough
feature), FORMAT and CHKDSK could be taught to work with CDRW&apos.s too&per. This allows to format a CDRW as
a big diskette, with a FAT filesystem&per. So, it works the same way UDF formatted CD&apos.s work&per.

:p.Also, I noticed that the current OS/2 UDF 2&per.16 has problems with formatting BD-RE (Blu-ray
rewritable), dual layer, having 50 GB in size&per. It&apos.s possible, because it does not know such
a medium type&per. Single-layer BD-RE&apos.s are formatted with UDF successfully&per. These 
dual-layer disks can be formatted with FAT32 or exFAT with fat32&per.ifs without any problems, 
as we don&apos.t check the medium type&per. This can be used if you want to use big sized 
BD-RE&apos.s as a large diskette&per. We have tested CD-RW/DVD+-RW/BD-RE disks successfully&per.

:p.The disk should be low-level formatted first&per. For example, with UDF FORMAT utility, like this&colon.

:p.:hp2.format u&colon. /fs&colon.udf /l /y /nofmt :ehp2.

:p.This will allow to create a low level info required for accessing the CD in packet mode (which allows to
write sectors at once, but not tracks/sessions)&per. Then you can format the CD into a FAT filesystem, instead
of UDF&colon.

:p.:hp2.format u: /fs&colon.fat32 /v&colon.fat32cd :ehp2.

:p.Also, we added the experimental support for low level formatting floppies and CD&apos.s. So, you also can
try this single command, istead of the two above commands&colon.

:p.:hp2.format u&colon. /fs&colon.fat32 /v&colon.fat32cd /l :ehp2.

:p. You may discover that after such an operation, programs like FC/2 or the WPS see the CD as a FAT32 filesystem&per.
They will see that a 640 MB CDRW disk has about 603 MB of available space&per. You can copy/delete/move files as usual&per.
You can also CHKDSK this media, it will be able to fix lost clusters etc&per. SYSINSTX utility works too, but my BIOS
was unable to boot from a non-iso9660 CD&per. 

:p.After writing the CD you need to do "unlock u&colon.", then "eject u&colon."&per. Both utilites are supplied with UDF
filesystem archive&per.


:h3 id=200705 res=32070.Debugging hangs or traps, or any other problems&per.

:p.:hp2.Debugging hangs or traps, or any other problems&per. :ehp2.

:p.For viewing the FAT32&per.IFS logs, there is a :link reftype=hd refid=20052.F32MON&per.EXE :elink. 
utility&per. Once started, it enables logging and logs are displayed in its window, plus are saved 
additionally to a FAT32&per.LOG file in the current directory&per. Also, logging
can be enabled from the beginning (since IFS is started) with the /MONITOR switch
specified to FAT32&per.IFS&per.

:p.Since some FAT32&per.IFS version, except for a F32MON&per.EXE log buffer,
the log is also sent to a COM port and OS/4 or QSINIT log buffer too&per. Also, 
trace records are sent to a trace buffer in parallel, so if OS/4 kernel, QSINIT or COM
port are unavailable, you can use OS/2 TRACE facility to collect the information, 
preceeding the trap or hang&per.

:p.FAT32&per.IFS uses 0xfe as a TRACE MAJOR number, so you need to copy the trc00fe.tff
file to your \os2\system\trace subdirectory&per.

:p.Tracing can be enabled with the TRACE ON command, and the buffer contents can be grabbed
with the TRACEGET utility&per. Also, you need to run the F32MON&per.EXE utility, or specify
the /MONITOR switch to FAT32&per.IFS for the messages to be sent to the trace bufer&per. TRACE
can be used if you use IBM kernel&per. On OS/4 kernels, the TRACE support is missing&per.
On OS/4 kernels, the internal OS/4 log buffer is preferred&per. It can have very large sizes,
up to some tens of megabytes&per. So, the messages are unlikely to be lost&per.

If the system is trapped or hanged, and only a TRAPDUMP is available, you can extract the
TRACE buffer from a TRAPDUMP with PMDF utility (the .T command)&per.

:p.Except for using the TRACE facility, FAT32&per.IFS also checks for OS/4 kernel and QSINIT 
loaders first, and if one of them is found, it sends the logging info to these log buffers, 
through a pointer in OS2LDR DOSHLP code&per. The OS/4 log buffer can be taken with a

:p.:hp2.copy kernlog$ log&per.txt :ehp2.

:p.command&per. If you have QSINIT installed, but not the OS/4 kernel, you can grab the 
log buffer with the

:p.:hp2.copy oemhlp$ log&per.txt :ehp2.

:p.command&per. If you have ACPI&per.PSD installed, OEMHLP$ driver is replaced by ACPI&per.PSD,
original OEMHLP$ is renamed to ___HLP$&per. So, you can still grab it via the

:p.:hp2.copy ___hlp$ log&per.txt :ehp2.

:p.command&per. If neither QSINIT or OS/4 kernel are available, the log is still directly
sent to the COM port&per. In all three cases, if you need to grab the log from the very
beginning (since the IFS initialization), you need to attach a terminal emulator to the
COM port. Also, for that, you need to specify the /MONITOR switch in FAT32&per.IFS command
line&per. As mentioned above, the F32MON&per.EXE buffer is only 8192 bytes long&per. So
with enough luck, you can avoid using the COM port&per. You just specify the /MONITOR
switch to FAT32&per.IFS, and as soon as possible when OS/2 is booted, start F32MON&per.EXE&per.
So, you can view the log buffer in via this utility&per.

:p.Alternatively (the prefferred way), if you have QSINIT or OS/4 kernel installed, you can grab
the log buffer with COPY command, as described above&per. If your system traps, or hangs and can&apos.t
be booted until the desktop, you can attach the terminal emulator to your COM port, and see the log
in its windows&per. Good terminal emulators also support logging&per. You can use OS/2 or Linux
or Windows, or any other system as a debug terminal&per. Also, it is possible to use a laptop
without a COM port, as a debug terminal&per. For that, just use an USB2Serial Cable, plus a 
null-modem cable&per. You plug a null-modem cable in a 9-pin end of an USB2Serial Cable,
and other end to the debuggee system COM port&per.

:p.It is preferred to use a cable with a Prolific PL2303-compatible chip, if you want to use
OS/2 as a debug terminal&per. You can use latest Lars Erdmann's USBCOM&per.SYS as an USB
COM port driver&per. It supports that chip&per.

:h3 id=200706 res=32071.Working with FAT filesystems, mounted to subdirectories&per.

:p.:hp2.Working with FAT filesystems, mounted to subdirectories&per. :ehp2.

:p.With the support of FAT file systems, mounted to subdirectories on another FAT drive, 
we are able to have a number of file systems, which have no drive letters assigned. Instead,
they are assigned the mount points, which are subdirectories on other drives. In order to
change to these drives, FAT32&per.IFS needs to have control over these drives. So, they 
also need to be FAT12/FAT16/FAT32/exFAT&per.

:p.Except for browsing directories on these drives, having no drive letters assigned, and
copying/moving/deleting/etc. files on them, there should also be a possibility to do
CHKDSK/FORMAT/SYS on these drives&per. As they don&apos.t have drive letters assigned,
we need to have possibility to pass the mount point to CHKDSK/FORMAT/SYS routines&per.

:p.These routines accept drive letter as an argument, though&per. So, we introduced a new
command line switch (which is present in all three utilities), namely, the "/M&colon.<mountpoint>"
switch&per. It is specifies like this&colon.

:p.:hp2.format u&colon. /fs&colon.fat32 /m&colon.v&colon.\tmp\0 :ehp2.

:p.where v&colon.\tmp\0 is a mount point, on which our FS is mounted&per. The drive u&colon. is ignored,
though it should be any FAT drive, in order for uunifat&per.dll to receive control&per. The
CHKDSK command is specified the same way&colon.

:p.:hp2.chkdsk u&colon. /f /m&colon.v&colon.\tmp\0 :ehp2.

:p.And likewise for the SYS command&colon.

:p.:hp2.sysinstx u&colon. /m&colon.v&colon.\tmp\0 :ehp2.

:p.
:h1 id=70 res=30064.Trouble Shooting
:artwork name='img3.bmp' align=center. 

:p.

:p.:hp2.It seems like it is taking forever for the system to write to the removable 
drive :ehp2.

:p.Disable the CACHEF32 by either remming out the CACHEF32&per.EXE statement in 
the config&per.sys file or by adding the following option /L&colon.OFF to the 
CACHEF32 statement&per. One can temporarily disable lazywrite by going to a command 
prompt and typing CACHEF32&per.EXE /L&colon.OFF 
.br 
  
:p.:hp2.During installation, one or more of the packaged driver files fails to 
install :ehp2.

:p.If installing over an earlier version, check the attribute of the file(s) in 
question to make sure the files(s) attributes are not marked read only&per.  If this is 
the case, remove the read only attribute from the files and install again&per. 
.br 


:p.:hp2.If no FAT32 drive or partition is found, check the following&colon. :ehp2.

:p.Did you reboot after applying the driver?  Since entries were made to the 
config&per.sys file, a reboot is necessary for the driver to be loaded&per. 
.br 
On :link reftype=hd refid=3.LVM :elink.(Logical Volume Manager) systems, did you use LVM to create a 
compatibility volume and assign it a drive letter?  FDisk is no longer used on these systems
&per. 

:p.On a non-LVM system, did you use one of the :link reftype=hd refid=4.Partition Support Packages for 
non-LVM systems:elink.? One of these packages is necessary in order for drive letters to 
be assigned to the FAT32 partition&per. 
.br 
If a new drive letter is not assigned, OS2DASD&per.DMD failed)&per. 

:p.If you are not sure, try the /MONITOR parameter after FAT32&per.IFS, and 
after reboot look with F32MON for FS_MOUNT  calls&per. Send the results to Netlabs
&per. 
.br 
If a new drive letter is assigned, but FAT32&per.IFS fails, please run :link reftype=hd refid=26.F32PARTS :elink.
.br 
If you have a TRAP, please send it with the CS&colon.IP value of the trap&per. 
.br 

:p.:hp2.The FAT32 drive is found, but the operating system is unable to read or write 
to the drive :ehp2.

:p.Try runnng chkdsk <drive>&colon. /F on the drive in question&per.  It could 
be that the drive still contains a dirty flag&per. 
.br 

:p.:hp2.In addition to being unable to read/write to the drive, OS/2 tells you 
the unit is 32Mb large or some other incorrect size :ehp2.

:p.Try creating a new Master Boot Record (MBR) with the partition tables erased
&per. DFSee is a good program to do this&per.  If you have LVM, then create a new 
partition using it&per.  If you do not have LVM, then use OS/2&apos.s FDISK, to create the 
new partition&per.  Remember, for non-removable hard drives using LVM, a 
compatibility volume must be created&per.  Unfortunately, this means that all data on the 
drive will be lost&per. Backup or copy your data and files to another drive&per. 
.br 

:p.:hp2.When you double click on the drive formatted FAT32, you get a pop-up that 
says &osq.No objects were found that matched the specified find criteria&osq. or 
&osq.The drive or diskette is not formatted correctly&per.&osq. :ehp2.

:p.Try the two previous answers to see if this corrects your problem&per. 
.br 

:p.Also, check out the sections dealing with :link reftype=hd refid=54.REMOVABLE MEDIA :elink.for more possible 
solutions to the above problems&per. 

:p.:hp2.You do not use or own a version of Windows, so how can you format the 
partition FAT32 :ehp2.  

:p. :link reftype=hd refid=33.See FORMATTING FAT32 VOLUMES :elink.
.br 

:p.:hp2. You have EA support enabled, but EA support does not work properly :ehp2.

:p.If you are having trouble with using EA,s and you are running version 0&per.
97 or later, run chkdsk <drive>&colon. / F&per. Starting with version 0&per.97 and 
later, chkdsk must be run prior adding EA support&per. 
.br 

:p.:hp2.At bootup, you receive errors about file versions being different :ehp2.

:p.Check to make sure you don&apos.t have more than one copy of FAT32&per.IFS 
or CACHEF32&per.EXE on your system&per. Also, check to make sure you do not have 
files from different versions installed&per. 
.br 

:p.:hp2.During bootup, you receive a system trap 

:p.The system detected an internal processing error at location ##1200&colon.
04ba 
.br 
- 0002&colon.04ba&per. 
.br 
65535,9051 
.br 
FAT32&colon.FSH_FORCENOSWAP on DATA Segment failed, rc=8 

:p.068606a0 
.br 
Internal Revision 14&per.100c_W4 :ehp2.

:p.If you receive an error message similar to this, try adding the /H option to 
the 

:p.IFS=D&colon./OS2/BOOT/FAT32&per.IFS 

:p.statement in the config&per.sys file and rebooting&per. 
.br 
  
:p.:hp2.Your formatted FAT32 USB drive shows a drive no greater than 32GB although 
the drive is physically much larger&per. :ehp2.  

:p.USBMSD&per.ADD only reads drive geometry from a device and reports to 
OS2DASD&per. Unfortunately many new USB msd devices report CHS incorrectly or don&apos.
t return it at all&per. CHS is the base of OS/2 geometry detection and if it is 
returned incorrectly then OS2DASD recalculates it to something strange&per. OS2DASD 
contains default CHS values and if you multiple them you receive 32GB&per. You need to 
replace your old OS2DASD with the latest one&per.  If this doesn&apos.t work, you can 
try Chris Wohlgemuth&apos.s USB driver (CWUSMSD&per.ADD)&per.  CW recalculates 
geometry and reports the recalculated one and not really received from a device&per. 
.br 
  
:p.:hp2.During bootup, you receive a LINALLOC FAILED RC=32776 or other linalloc error 
message :ehp2.

:p.Starting with version 0&per.99, a /H option can be added to the FAT32&per.
IFS statement in the CONFIG&per.SYS file to load cache memory into high memory&per.  
This should eliminate any LINALLOC FAILED error that has previously been occurring
&per. 
.br 
  
:p.:hp2.You prefer using one of the earlier versions of the FAT32 driver&per. What 
else can you do to get rid of the LINALLOC FAILED message? :ehp2.

:p.There are 5 things you can do&colon. 

:p.Just ignore the error message 
.br 
Don&apos.t use VFdisk 
.br 
Add the statement EARLYMEMINIT=TRUE to the config&per.sys file&per. 

:p.This setting allows device drivers, etc&per., access to the memory above 16mb 
early in boot&per. Previously, this was only available after Device Driver and IFS 
initialization was completed&per. This setting requires an OS2KRNL dated 2002 or later&per. :color fc=default.:color bc=default.:hp8.

:p.Warning&colon. This setting has various implications when enabled&per.

:p. 1&per.Don&apos.t use with ISA cards with (busmaster-) DMA features&per. 
.br 
 2&per.The Universal audio driver does not work with this setting (Trap 0008)
&per. 
.br 
 3&per.AHA154X&per.ADD may do bad things to your system&per. 
.br 
 4&per.There may be some settings of HPFS386 cache that are incompatible&per. :ehp8.

:p.Try lowering the cache setting to 1034 MB or 512MB in the IFS=FAT32&per.IFS 
statement found in the config&per.sys 
.br 
Add the following VDisk setting to the config&per.sys file&per.   

:p.DEVICE=<PATH>&bsl.VFDISK&per.SYS 0 

:p.where &osq.0&osq. loads VFDisk as an &osq.Ejected drive&osq.&per. 

:p.With this setting, vfdisk loads the driver into memory, but doesn&apos.t 
reserve memory for a virtual floppy&per.  When one needs a virtual floppy, then use the 
control object &osq.vfctrlpm&per.exe&osq. to make one&per. 

:p.To start your virtual disk automatically on startup, you can make a program 
object of the &osq.VFCTRL&per.EXE&osq. program object and move the program object to 
the Startup folder&per. RIght click the &osq.VFCTRL&osq. program object and in its 
parameters field put the drive&colon. <media type>&per. 
.br 

:p.

:p.:hp2.Example&colon.  :ehp2.S&colon. 4 

:p.A second way to do this would be to add VFCTRL&per.EXE drive&colon. <media 
type> to the STARTUP&per.CMD found in the systems root directory&per. 

:p.

:p.:hp2.Example&colon.  :ehp2.VFCTRL&per.EXE S&colon. 4 

:p.A third way would be to add 

:p.CALL=<path>&bsl.VFCTRL&per.EXE drive&colon. <media type> 

:p.to your CONFIG&per.SYS&per. 

:p.

:p.:hp2.Example&colon.  :ehp2.CALL=C&colon.&bsl.OS2&bsl.VFCTRL&per.EXE S&colon. 4 

:p.:hp2.Note&colon.  :ehp2.VFDISK&per.SYS assigns the drive letter without input from you
&per.  As a result, you will need to determine what drive letter VFDISK&per.SYS 
assigns before doing the above three examples&per.   

:p.

:p.:hp2.VFDISK Media Type :ehp2.

:p.0 Ejected Drive 

:p.1 1&per.44 Mb 3&per.5&osq. Floppy 

:p.2 1&per.20 Mb 5&per.25&osq. Floppy 

:p.3 720 Kb 3&per.5&osq. Floppy 

:p.4 2&per.88 Mb 3&per.5&osq. Floppy 

:p.5 360 Kb 5&per.25&osq. Floppy 

:p.6 1&per.84 Mb 3&per.5&osq. Floppy 

.br 

:p.

:p.:hp2.Note&colon.  :ehp2.If you have a program that doesn&apos.t work or returns errors, 
please run F32MON&per.EXE while you execute the program&per. After the error has 
occurred, terminate :link reftype=hd refid=20052.F32MON :elink.and send a message describing what the problem is, as 
detailed as possible, and include the FAT32&per.LOG that was created by F32MON&per. to 
Netlabs or the Yahoo FAT32USER group&per.   
:h1 id=71 res=30065.History

:p.:hp2.HISTORY&colon. :ehp2.  

:p.:hp2.Version 0&per.10&colon. :ehp2.

:p.:hp2.Revision r313 (valerius, Mon, 25 Sep 2017)&colon. :ehp2.

:ul compact.
:li. fat32&per.inf, ifs&per.inf&colon. Fixed errors and warnings in the IPF code&per.
:eul.

:p.:hp2.Revision r312 (valerius, Sun, 24 Sep 2017)&colon. :ehp2.

:ul compact.
:li. ifs&per.inf&colon. Additions to documentation&per.
:eul.

:p.:hp2.Revision r311 (valerius, Fri, 22 Sep 2017)&colon. :ehp2.

:p. CHKDSK fixes/enhancements
:ul compact.
:li. Support for fixing crosslinked clusters.
:li. Support for fixing incorrect "&per." and "&per.&per." entries (wrong cluster number)&per.
:eul.

:p.:hp2.Revision r310 (valerius, Sun, 17 Sep 2017)&colon. :ehp2.

:ul compact.
:li. fat32.ifs&colon. Preliminary changes for /eas2 switch (ea data&per. sf support)&per.
:eul.

:p.:hp2.Revision r309 (valerius, Sun, 17 Sep 2017)&colon. :ehp2.

:ul compact.
:li. SYS&colon. Add preliminary exFAT support (bootblock does not fit into 8 sectors, yet)&per.
:eul.

:p.:hp2.Revision r308 (valerius, Sun, 17 Sep 2017)&colon. :ehp2.

:ul compact.
:li. fat32&per.inf&colon. Documentation update&per.
:eul.

:p.:hp2.Revision r307 (valerius, Mon, 11 Sep 2017)&colon. :ehp2.

:ul compact.
:li. Add forgotten file&per.
:eul.

:p.:hp2.Revision r306 (valerius, Sun, 10 Sep 2017)&colon. :ehp2.

:ul compact.
:li. Fix for #58&colon. lost EA files are recovered with paths prepended to a longname&per.
:eul.

:p.:hp2.Revision r305 (valerius, Sun, 10 Sep 2017)&colon. :ehp2.

:ul compact.
:li. 64-bit bit ops&colon. Add forgotten assembler files&per.
:li. FAT+&colon. Change FAT+FSZ EA to FAT_PLUS_FSZ, to avoid ERROR_INVALID_EA_NAME error
 when copying this file&per.
:eul.

:p.:hp2.Revision r304 (valerius, Sun, 10 Sep 2017)&colon. :ehp2.

:ul compact.
:li. Delete unneeded tracepoints from FS_MOUNT&per.
:li. Make ulDataLen equal to ulValidDataLen on exFAT (instead of being equal
 to rounded up to a cluster size value)&per.
:eul.

:p.:hp2.Revision r303 (valerius, Sun, 10 Sep 2017)&colon. :ehp2.

:ul compact.
:li. FAT+ support in the IFS and CHKDSK&per.
:eul.

:p.:hp2.Revision r302 (valerius, Sun, 10 Sep 2017)&colon. :ehp2.

:ul compact.
:li. change IBM's DDK headers to our own ones&per.
:eul.

:p.:hp2.Revision r301 (valerius, Wed, 30 Aug 2017)&colon. :ehp2.

:p. fat32.ifs&colon. exFAT fixes

:ul compact.
:li. Avoid using CritMessage, because it&colon. 1) terminates calling program, with general
 use 2) Shows an unremovable popup, when in boot time&per. So, avoid using CritMesage&per.
:li. Add a filename argument to ModifyDirectory(MODIFY_DIR_UPDATE), instead of NULL, so
 that it should look up dir entries without a cluster name (zero-size files) correctly&per.
 So, always pass pszFile argument&per.
:li. Fix CompactDir1, so it is now correctly compacts the directory&per.
:li. TranslateName fixes for exFAT case&per.
:li. ModifyDirectory fixes (support for dir entry set crossing the block boundary), exFAT case
:li. Disable importing two KEE calls (repair Merlin/Warp3 compatibility)
:li. free up the pDirSHInfo on file close (was on FS_OPENCREATE exit), so that, no trap when
  it is accessed on FS_COMMIT after being freed&per.
:eul.

:p.:hp2.Revision r300 (valerius, Wed, 30 Aug 2017)&colon. :ehp2.

:p. &per.INF documentation additions and corrections
.br

:p.:hp2.Revision r299 (valerius, Mon, 31 Jul 2017)&colon. :ehp2.

:ul compact.
:li. Make TranslateName implementation for exFAT&per.
:li. Fix loaddskf on a FAT12 floppy mounted by fat32&per.ifs&per. So, SetVPB
  kernel routine, which overwrote vpfsd with FAT-specific data, doesnt
  gets called, and hence, no IPE when fat32&per.ifs detects it&per. Also, prevent
  calling MarkDiskStatus in FS_FLUSHBUF and FS_SHUTDOWN on FAT12 removables,
  this avoids another trap in DoVolIO kernel routine when doing FS_FLASHBUF&per.
:li. Fix a typo in DeleteFatChain, so ulBmpSector is used, instead of ulSector&per.
:eul.
 
:p.:hp2.Revision r298 (valerius, Fri, 28 Jul 2017)&colon. :ehp2.

:p. A fix for sectros per FAT < 3&per. Now it should work with 360 KB diskettes&per.
.br

:p.:hp2.Revision r297 (valerius, Tue, 25 Jul 2017)&colon. :ehp2.

:ul compact.
:li. Change USHORT to ULONG in some places, to avoid overflow in case of FAt12/FAT16
 partitions with a 64 KB cluster&per. Otherwise we could get division by zero in some
 cases&per.
:eul.
 
:p.:hp2.Revision r296 (valerius, Sun, 23 Jul 2017)&colon. :ehp2.

:p. fat32&per.ifs&colon. Add system trace support&per.
.br

:p.:hp2.Revision r295 (valerius, Thu, 20 Jul 2017)&colon. :ehp2.

:ul compact.
:li. Disable Lars Erdman&apos.s code for searching DevCaps and Strat2 routine if
 it is normally not found&per. This found strat2 even in case no strat2 is
 presented by the disk driver (like hd4disk&per.add with an "/1" parameter)&per.
:li. Consider using fSilent flag when loading Unicode Translate Table, instead of
 being always verbose&per.
:li. Set pVolInfo->fDiskClean to FALSE in WriteSector if Dirty flag in the 1th FAT
 element is set to not do it on every WriteSector call&per.
:li. Swapping code updates
:li. FS_FILEIO implementation
:eul.
 
:p.:hp2.Revision r294 (valerius, Mon, 17 Jul 2017)&colon. :ehp2.

:p. fat32&per.ifs&colon. exFAT fixes&per.

:ul compact.
:li. Add required parentheses in functions for calculating checksums&per.
 Now checksums should be ok&per.
:li. Fix a bug with deleting file while renaming it&per.
:eul.

:p.:hp2.Revision r293 (valerius, Sun, 16 Jul 2017)&colon. :ehp2.

:p. FORMAT&colon. Revert previous commit, as it is wrong&per. params->sectors_per_cluster fit large cluster 
numbers, while dp&per.SectorsPerCluster is not&per. And it can be zero&per.
.br

:p.:hp2.Revision r292 (valerius, Sun, 16 Jul 2017)&colon. :ehp2.

:p. FORMAT&colon. Use dp&per.SectorsPerCluster instead of params->sectors_per_cluster (it may be zero in some 
cases, so it cause FORMAT trap)&per.
.br

:p.:hp2.Revision r292 (valerius, Sun, 16 Jul 2017)&colon. :ehp2.

:p. fat32&per.ifs&colon. Some minor fixes

:ul compact.
:li. Initialize several variables to NULL&per. So that, no error occurs
 when it tries to free() these variables&per. This fixes ERROR_NOT_COPY returned
 in FM/2 and the file manager copies everything well between two FAT drives&per.
:li. Trying to fix errors with swapping
:li. Minimize FAT access semaphore acquiring time&per. So, less probability for
 timeouts on the semaphore&per.
:eul.

:p.:hp2.Revision r290 (valerius, Mon, 10 Jul 2017)&colon. :ehp2.

:ul compact.
:li. Fix a buffer overrun in FS_READ&per.
:li. Fix seconds vs twosecs error in timestamps&per.
:li. Fix erroneous counting of files in the directory on
 file delete in FS_RMDIR&per. Wrong nomber of files counted&per.
:eul.

:p.:hp2.Revision r289 (valerius, Thu, 06 Jul 2017)&colon. :ehp2.

:ul compact.
:li. Fix skipping 2nd cluster on exFAT when cluster size is 128+ KB when
 reading and writing files&per.
:li. Make FORMAT create FS'es with 128 KB or more clusters&per. Support the case
 with a very small (1kb or so) clusters, when the allocation bitmap uses
 many clusters, so they don't fit into one FAT sector&per.
:li. Add support for adding timestamps for files created by CHKDSK&per.
:eul.

:p.:hp2.Revision r288 (valerius, Tue, 04 Jul 2017)&colon. :ehp2.

:p. FORMAT: Make volumes formatted with our FORMAT, understandable by winxp exFAT driver&per.
.br

:p.:hp2.Revision r287 (valerius, Tue, 04 Jul 2017)&colon. :ehp2.

:p. fat32&per.ifs&colon. Fix seeking to a cluster for exFAT&per. Now archives are entered correctly&per.
.br

:p.:hp2.Revision r286 (valerius, Mon, 03 Jul 2017)&colon. :ehp2.

:p. fat32&per.ifs&colon. Fix some problems remained in previous revision&per.

:ul compact.
:li. Fix a trap 000d caused by unzeroed variables&per.
:li. Move check for bad sectors from ReadSector to ReadBlock and
 WriteBlock&per.
:li. Fix allocating new clusters in SetNextCluster in exFAT code&per.
:li. Change unlink() to DelFile() and avoid calling filesystem
 access functions in CHKDSK when FS is not accessible&per.
:eul.

:p.:hp2.Revision r285 (valerius, Sun, 02 Jul 2017)&colon. :ehp2.

:p. ifs&per.inf&colon. Documentation updates&per.
.br

:p.:hp2.Revision r284 (valerius, Sun, 02 Jul 2017)&colon. :ehp2.

:p. fat32&per.ifs&colon. Make more conservative stack usage&per.

:ul compact.
:li. Use less stack space&per. Change all local buffers to dynamically
  allocated buffers&per. This should fix kernel stack overflow errors&per.
:eul.

:p.:hp2.Revision r283 (valerius, Tue, 20 Jun 2017)&colon. :ehp2.

:p. CHKDSK&colon. Additional exFAT-related fixes&per.
.br

:p.:hp2.Revision r282 (valerius, Sun, 18 Jun 2017)&colon. :ehp2.

:ul compact.
:li. Different exFAT-related additions for CHKDSK
:li. Return "unimplemented" on SYSINSTX attempts
:li. Attempts to fix traps in malloc() in exFAT CHKDSK code&per. It appears 
 that heap is corrupted somewhere&per. Also, added experimental mem_alloc2() function,
 to allocate memory via a CHKDSK&per.SYS driver (non-working, yet)&per.
:eul.

:p.:hp2.Revision r281 (valerius, Sun, 18 Jun 2017)&colon. :ehp2.

:p. fat32&per.ifs&colon. Identtify itself as FAT32, not UNIFAT anymore&per.

:ul compact.
:li. Some programs treat FAT32 drives specially and know nothing about UNIFAT&per.
 Though it is wrong, I don't want break such programs&per. So, since now, I disable
 IFS identification as UNIFAT&per. Now it should be FAT32, as previous&per.
:li. Return back the bad sectors checking in ReadSector2, and return ERROR_SECTOR_NOT_FOUND
 to avoid reading zero sectors&per.
:eul.

:p.:hp2.Revision r280 (valerius, Wed, 14 Jun 2017)&colon. :ehp2.

:p. fat32&per.ifs&colon. Some previously forgotten fixes&per.

:ul compact.
:li. Move InitCache back from FS_INIT to FS_MOUNT (as it uses some
 DevHelp&apos.s which cannot be used in r3 context)&per.
:li. More proper handling of exFAT dir entries in two places&per.
:li. Some forgotten declarations in headers&per.
:eul.

:p.:hp2.Revision r279 (erdmann, Tue, 13 Jun 2017)&colon. :ehp2.

:p. merge Valerys changes
.br

:p.:hp2.Revision r278 (valerius, Tue, 13 Jun 2017)&colon. :ehp2.

:p. CHKDSK&colon. prepare CHKDSK for exFAT support&per.
.br

:p.:hp2.Revision r277 (valerius, Tue, 13 Jun 2017)&colon. :ehp2.

:p. CHKDSK&colon. repair PMCHKDSK&per.
.br

:p.:hp2.Revision r276 (valerius, Thu, 08 Jun 2017)&colon. :ehp2.

:p. CHKDSK: Fix CHKDSK logic error, introduced in previous commit&per.

:ul compact.
:li. Fix incorrect logic in ChkDskMain&colon. CHKDSK has quit instead of checking
 the disk&per.
:eul.

:p.:hp2.Revision r275 (valerius, Thu, 08 Jun 2017)&colon. :ehp2.

:p. fat32&per.ifs&colon. Fix minor error occured with MSC&per.
.br

:p.:hp2.Revision r274 (valerius, Thu, 08 Jun 2017)&colon. :ehp2.

:p. fat32&per.ifs&colon. Define whether to use static or dynamic buffers via a define&per.

:ul compact.
:li. If USE_STATIC_BUFS is set, use static variables for sector buffers in
 ReadSector2, FindPathCluster0/1, ModifyDirectory0/1, TranslateBuffer&per.
 Access to these buffers is got via a semaphore&per. If USE_STATIC_BUFS is
 not set, create buffers via malloc and dispose of them via free&per. Here,
 the problem with trap in malloc is present&per. There is a trap 0008 somewhere
 in FSH_SEGALOC, maybe because of too much selectors allocated (a hypothesis)&per.
 Possible solution&colon. rewrite malloc/free to use less selectors (not to
 allocate a selector to each small piece of memory, try to cover more memory
 by a single selector)&per.
:li. Disk autocheck enhancements&per. In f32chk.exe, add checks for pointer to 
 argv[i] != 0 when parsing CHKDSK command line&per. Don&apos.t output &apos.\n&apos.&apos.s after
 checking each disk&per. Make output look more beautiful&per.
:eul.

:p.:hp2.Revision r273 (valerius, Wed, 07 Jun 2017)&colon. :ehp2.

:p. fat32&per.ifs&colon. Split fat32&per.c to two files, to avoid the Segment too large error&per.
.br

:p.:hp2.Revision r272 (valerius, Sun, 04 Jun 2017)&colon. :ehp2.

:p. build system&colon. Fix a minor build error with cmd.exe as a shell&per.
.br

:p.:hp2.Revision r271 (valerius, Sat, 03 Jun 2017)&colon. :ehp2.

:p. fat32&per.ifs&colon. Use dynamically allocated buffers for the time being&per.
.br

:p.:hp2.Revision r270 (erdmann, Fri, 02 Jun 2017)&colon. :ehp2.

:p. revert change in ReadSector2&colon. it&apos.s possible to read from before the actual data sectors (reading FAT for example)
.br

:p.:hp2.Revision r269 (erdmann, Thu, 01 Jun 2017)&colon. :ehp2.

:p. merging Valerys changes
.br

:p.:hp2.Revision r268 (valerius, Thu, 01 Jun 2017)&colon. :ehp2.

:p. fat32&per.ifs&colon. #ifdef the unused variables out&per. Enable/disable exFAT support in MSC makefile&per.
.br

:p.:hp2.Revision r267 (valerius, Thu, 01 Jun 2017)&colon. :ehp2.

:p. fat32&per.ifs&colon. Fix broken edits from r266&per.
.br

:p.:hp2.Revision r266 (valerius, Wed, 31 May 2017)&colon. :ehp2.

:p. Enable creating the builds without exFAT support&per.
.br

:p.:hp2.Revision r265 (erdmann, Wed, 31 May 2017)&colon. :ehp2.

:p. fixing "GetChainSize" (usSectorsRead was not properly initialized, also throw away all garbage), move 
macros "Cluster2Sector" and "Sector2Cluster" into the central header file
.br

:p.:hp2.Revision r264 (erdmann, Wed, 31 May 2017)&colon. :ehp2.

:p. fixing a fatal hang in "ReadSector2" but we still fail to execute successfully because the requested sector 
sometimes is located before the start of the partition which it should not be&per.&per.&per.
.br

:p.:hp2.Revision r263 (erdmann, Wed, 31 May 2017)&colon. :ehp2.

:p. readd "SemRequest" routine
.br

:p.:hp2.Revision r262 (erdmann, Wed, 31 May 2017)&colon. :ehp2.

:p. merging Valerys changes to allow building with MS C
.br

:p.:hp2.Revision r261 (erdmann, Wed, 31 May 2017)&colon. :ehp2.

:p. properly preset some return values
.br

:p.:hp2.Revision r260 (valerius, Wed, 31 May 2017)&colon. :ehp2.

:p. fat32&per.ifs&colon. Make MSC build working

:ul compact.
:li. Add missing "else" statement in "vsprintf"&per. So that, "%llx" format string is now processed
 correctly&per.
:li. Support for compilers without "long long" type support&per. In particular, fixed build with
 MSC compiler&per.
:li. Trying to fix problems with traps when intensively using malloc/free&per. Now I changed buffers,
 allocated via malloc, in several places, to static buffers&per. For example, in ReadSector,
 FindPathCluster, ModifyDirectory, TranslateName&per. The buffers are accessed by acquiring 
 a corresponding semaphore&per. However, it slows down the performance&per.
:eul.

:p.:hp2.Revision r259 (erdmann, Fri, 26 May 2017)&colon. :ehp2.

:p. set CPU type properly to support "pause" instruction
.br

:p.:hp2.Revision r258 (erdmann, Fri, 26 May 2017)&colon. :ehp2.

:p. implementing spinlocks (we need something that can be called nested which the OS provided ones won&apos.t support), 
also move definitions of routines "GetFatAccess" and "ReleaseFat" to common header file "fat32ifs&per.h" so that we don&apos.t 
have to change the prototype at two different places
.br

:p.:hp2.Revision r257 (erdmann, Tue, 23 May 2017)&colon. :ehp2.

:p. remove all Windows related build products
.br

:p.:hp2.Revision r256 (erdmann, Tue, 23 May 2017)&colon. :ehp2.

:p. merging Valerys latest changes
.br

:p.:hp2.Revision r255 (valerius, Fri, 19 May 2017)&colon. :ehp2.

:p. fat32&per.ifs&colon. Fix file creation&per. (There were Entry type not set)&per.
.br

:p.:hp2.Revision r254 (valerius, Wed, 17 May 2017)&colon. :ehp2.

:p. fat32&per.ifs&colon. Return back the old variant of pre-DOS-4&per.0 FAT BPB detection&per. Otherwise NTFS volumes 
could be erroneously mounted&per.
.br

:p.:hp2.Revision r253 (valerius, Wed, 17 May 2017)&colon. :ehp2.

:p. fat32&per.ifs&colon. Move several fields from FINDINFO to FINFO, so now FINDINFO does not exceed the limit of 24 bytes&per.
.br

:p.:hp2.Revision r252 (erdmann, Tue, 16 May 2017)&colon. :ehp2.

:p. revert change in "FS_FILEINFO"&colon. if largefile support is active, psffsi->sfi_sizel is also used for FIL_STANDARD 
and FIL_QUERYEASIZE
.br

:p.:hp2.Revision r251 (erdmann, Tue, 16 May 2017)&colon. :ehp2.

:p. Created branch lars&per.
.br

:p.:hp2.Revision r250 (valerius, Sun, 14 May 2017)&colon. :ehp2.

:ul compact.
:li. Fixes for #40 with FS_FINDFIRST/FS_FINDNEXT regarding FIL_QUERYEASFROMLIST/FIL_QUERYEASFROMLISTL&colon.
 return a minimum required FEALIST (otherwise a loop calling FS_FINDNEXT is observed)&per.
:li. The same fixes for exFAT case
:li. Fix returning EA's on exFAT&per. Now WPS window with exFAT disk is opened successfully (but WPS
 still traps then, because creating files does not work - it tries to create WP ROOT&per. SF)
 Author&colon. Lars Erdmann (with exFAT-related fixes by valerius)&per.
:eul.

:p.:hp2.Revision r249 (valerius, Mon, 08 May 2017)&colon. :ehp2.

:ul compact.
:li. Additional fixes in FS_MOUNT regarding the Volume Label and Serial number setting
:li. Identify itself as UNIFAT if /fat or /exfat are set, instead of FAT32 (to avoid
 confusion when FAT16 drives are reported as being FAT32)
:li. Added several forgotten changes regarding FIL_QUERYEASFROMLISTL vs FIL_QUERYEASFROMLIST
:eul.

:p.:hp2.Revision r248 (valerius, Tue, 02 May 2017)&colon. :ehp2.

:p. FORMAT&colon. exFAT support in FORMAT&per.
.br

:p.:hp2.Revision r247 (valerius, Tue, 02 May 2017)&colon. :ehp2.

:p. fat32&per.ifs&colon. Additional exFAT-related fixes&per.
.br

:p.:hp2.Revision r246 (valerius, Tue, 02 May 2017)&colon. :ehp2.

:p. fat32&per.ifs&per. Additional exFAT-related fixes&per.
.br

:p.:hp2.Revision r245 (valerius, Tue, 02 May 2017)&colon. :ehp2.

:p. fat32&per.ifs&colon. FS_MOUNT fixes&colon. support for diskettes without a BPB and VolLabel/VolSerNo-related fixes&per.
.br

:p.:hp2.Revision r244 (valerius, Tue, 02 May 2017)&colon. :ehp2.

:p. fat32&per.ifs&colon. Use dup VPB only if special magic is present&per.
.br

:p.:hp2.Revision r243 (valerius, Tue, 02 May 2017)&colon. :ehp2.

:p. fat32&per.ifs&colon. Use dup VPB only if special magic is present&per.
.br

:p.:hp2.Revision r242 (valerius, Sun, 23 Apr 2017)&colon. :ehp2.

:p. fat32&per.ifs&colon. Additional exFAT-related enhancements&per.
.br

:p.:hp2.Revision r241 (valerius, Wed, 19 Apr 2017)&colon. :ehp2.

:p. fat32&per.ifs&colon. exFAT-related updates&per.
.br

:p.:hp2.Revision r240 (valerius, Wed, 19 Apr 2017)&colon. :ehp2.

:p. docs&colon. Documentation update&per.
.br

:p.:hp2.Revision r239 (valerius, Wed, 19 Apr 2017)&colon. :ehp2.

:p. ufat32&per.dll&colon. Check for recommended BPB if no real one&per.
.br

:p.:hp2.Revision r238 (valerius, Wed, 12 Apr 2017)&colon. :ehp2.

:p. fat32&per.ifs&colon. exFAT support&colon. Support for FS_FSINFO/FS_CHDIR and FS_FIND[FIRST/NEXT/CLOSE]&per.
.br

:p.:hp2.Revision r237 (valerius, Wed, 12 Apr 2017)&colon. :ehp2.

:p. FORMAT&colon. Autodetect the FAT type based on disk size&per.
.br

:p.:hp2.Revision r236 (valerius, Sun, 09 Apr 2017)&colon. :ehp2.

:p. cachef32&per.exe&colon. Force FAT12/FAT16 disks remount on cachef32 start (so, we got fat32&per.ifs mounted on them)&per.
.br

:p.:hp2.Revision r235 (valerius, Sun, 09 Apr 2017)&colon. :ehp2.

:p. fat32&per.ifs&colon. Fix MarkDiskStatus (disk status was written into incorrect FAT sector)&per.
.br

:p.:hp2.Revision r234 (valerius, Sat, 08 Apr 2017)&colon. :ehp2.

:p. fat32&per.ifs&colon. Don&apos.t set disk status after checking/formatting disk&per.

:p. Don&apos.t update dirty flag after CHKDSK/FORMAT/etc, so we don&apos.t get an
error during unmount, and then, we don&apos.t get a SIGKILL signal when
doing DSK_REDETERMINEMEDIA ioctl on remount&per. So, now CHKDSK/FORMAT
terminates successfully&per.
.br

:p.:hp2.Revision r233 (valerius, Fri, 07 Apr 2017)&colon. :ehp2.

:p. SYS&colon. SYS support for FAT12/FAT16&per.
.br

:p.:hp2.Revision r232 (valerius, Fri, 07 Apr 2017)&colon. :ehp2.

:p. docs&colon. Add notes&per.txt&per.
.br

:p.:hp2.Revision r231 (valerius, Fri, 07 Apr 2017)&colon. :ehp2.

:p. FORMAT&colon. Make FORMAT support formatting to FAT12/FAT16&per.
.br

:p.:hp2.Revision r230 (valerius, Fri, 07 Apr 2017)&colon. :ehp2.

:p. Non-512 byte sector compatibility fixes&per. Now it works with CD&apos.s with FAT filesystem too&per.
.br

:p.:hp2.Revision r229 (valerius, Wed, 05 Apr 2017)&colon. :ehp2.

:p. Read three sectors of FAT at once (to read a whole number of FAT12 entries)&per.
.br

:p.:hp2.Revision r228 (valerius, Wed, 05 Apr 2017)&colon. :ehp2.

:p. build system&colon. Fix deps in makefiles&per.
.br

:p.:hp2.Revision r227 (valerius, Sun, 02 Apr 2017)&colon. :ehp2.

:p. fat32&per.ifs&colon. Correct a small syntax error&per.
.br

:p.:hp2.Revision r226 (valerius, Sun, 02 Apr 2017)&colon. :ehp2.

:p. UFAT32&per.DLL&colon. FAT12/FAT16 fixes&per.
.br

:p.:hp2.Revision r225 (valerius, Sun, 02 Apr 2017)&colon. :ehp2.

:p. fat32&per..ifs&colon. FAT12/FAT16 fixes&per.
.br

:p.:hp2.Revision r224 (valerius, Sun, 02 Apr 2017)&colon. :ehp2.

:p. build system&colon. Build system improvements&per.
.br

:p.:hp2.Revision r223 (valerius, Sun, 02 Apr 2017)&colon. :ehp2.

:p. fat32&per.ifs&colon. 32-bit entry points&per. Add forgotten file&per.
.br

:p.:hp2.Revision r222 (valerius, Sun, 02 Apr 2017)&colon. :ehp2.

:p. build system&colon. Compress binaries with lxlite&per.
.br

:p.:hp2.Revision r221 (valerius, Sun, 02 Apr 2017)&colon. :ehp2.

:p. CHKDSK&colon. Save lost cluster chains into a separate directory&per.
.br

:p.:hp2.Revision r220 (valerius, Sun, 02 Apr 2017)&colon. :ehp2.

:p. fat32&per.ifs&colon. Succeed the IFS init in case of Merlin kernels and large file support is enabled&per.
.br

:p.:hp2.Revision r219 (valerius, Sun, 02 Apr 2017)&colon. :ehp2.

:p. FS utilities&colon. move unicode translate table loading to UFAT32&per.DLL and export it&per.
.br

:p.:hp2.Revision r218 (valerius, Sun, 02 Apr 2017)&colon. :ehp2.

:p. FS utilities&colon. move unicode translate table loading to UFAT32&per.DLL and export it&per.
.br

:p.:hp2.Revision r217 (valerius, Sun, 02 Apr 2017)&colon. :ehp2.

:p. fat32&per..ifs&colon. Autocheck enhancements&per.
.br

:p.:hp2.Revision r216 (valerius, Sun, 02 Apr 2017)&colon. :ehp2.

:p. fat32&per.ifs&colon. Preliminary changes to add the 32-bit entry points&per.
.br

:p.:hp2.Revision r215 (valerius, Sun, 02 Apr 2017)&colon. :ehp2.

:p. UFAT32&per.DLL&colon. Add forgotten file&per.
.br

:p.:hp2.Revision r214 (valerius, Fri, 17 Mar 2017)&colon. :ehp2.

:p. fat32&per.ifs&colon. Add command line switches to set mounted disks lists/masks&per.
.br

:p.:hp2.Revision r213 (valerius, Fri, 16 Mar 2017)&colon. :ehp2.

:p. Add FAT12/FAT16 support&per.

:p. Refactor code to support generic FAT, including FAT12/FAT16/FAT32/exFAT&per.
This time, add support for FAT12/FAT16 to fat32&per.ifs and CHKDSK&per.
.br

:p.:hp2.Revision r212 (valerius, Sun, 12 Mar 2017)&colon. :ehp2.

:p. fat32&per.ifs&colon. Fix looping when inserting a directory entry&per.
.br

:p.:hp2.Revision r211 (valerius, Sat, 11 Mar 2017)&colon. :ehp2.

:p. util&colon. Rename again diskinfo -> f32parts, remove diskdump from packages&per.
.br

:p.:hp2.Revision r210 (valerius, Sat, 11 Mar 2017)&colon. :ehp2.

:p. fat32&per.ifs&colon. Fix uninitialized variable&per.
.br

:p.:hp2.Revision r209 (valerius, Sat, 11 Mar 2017)&colon. :ehp2.

:p. fat32&per.ifs&colon. Fix #25&per. (executable file corruption)&per.
.br

:p.:hp2.Revision r208 (valerius, Sat, 11 Mar 2017)&colon. :ehp2.

:p. fat32&per.ifs&colon. Support for subcluster blocks&per.

:p. Read or write clusters by blocks of 32 KB or less&per. This allows to use clusters,
bigger than 32 KB&per. So that, 64 KB clusters in FAT32, or clusters up to 32 MB in
exFAT, could be supported with a 16-bit FAT driver&per.
.br

:p.:hp2.Revision r207 (valerius, Thu, 02 Mar 2017)&colon. :ehp2.

:p. UFAT32.DLL&colon. Add 16-bit signal handler&per.

:p. Add 16-bit signal handler, in case we&apos.re using 16-bit frontend
program&per. So that, we can interrupt it by Ctrl-C and disk will
remount&per. Add Help for CHKDSK and SYS&per. Run CHKDSK/SYS only in 
case the disk is FAT32 (check BPB for "FAT32   " string)&per.
.br

:p.:hp2.Revision r206 (valerius, Tue, 28 Feb 2017)&colon. :ehp2.

:p. UFAT32&per.DLL&colon. Support for creating chkdsk&per.old&per.
.br

:p.:hp2.Revision r205 (valerius, Tue, 28 Feb 2017)&colon. :ehp2.

:p. UFAT32&per.DLL&colon. Enhancements for messages output to log&per.
.br

:p.:hp2.Revision r204 (valerius, Mon, 20 Feb 2017)&colon. :ehp2.

:p. UFAT32&per.DLL&colon. Fix date/time when creating/modifying dir&per. entries&per.
.br

:p.:hp2.Revision r203 (valerius, Mon, 20 Feb 2017)&colon. :ehp2.

:p. UFAT32&per.DLL&colon. Try not to use SECTORIO ioctl&per.

:p. Try not to use SECTORIO ioctl, use ordinary byte mode,
instead of sector mode&per. Also, recode win32 API error descriptions 
from ANSI to OEM codepage&per. And, use a buffer with zero padding
for filename, when calling MakeDirEntry()&per.

:p.:hp2.Revision r202 (valerius, Sat, 18 Feb 2017)&colon. :ehp2.

:p. UFAT32&per.DLL&colon. More sector size related changes&per.
.br

:p.:hp2.Revision r201 (valerius, Sat, 18 Feb 2017)&colon. :ehp2.

:p. CHKDSK&colon. Fix CHKDSK after r198&per.

:p. The bootsector was read when sector size in BPB was uninitialized&per.
So, do get_drive_params() first&per.

:p.:hp2.Revision r200 (valerius, Fri, 17 Feb 2017)&colon. :ehp2.

:p. Build system&colon. include those &per.wmp lines to &per.map, which have address with + at the end&per.
.br

:p.:hp2.Revision r199 (valerius, Fri, 17 Feb 2017)&colon. :ehp2.

:p. FORMAT&colon. Don&apos.t set partition type if disk is GPT&per.
.br

:p.:hp2.Revision r198 (valerius, Fri, 17 Feb 2017)&colon. :ehp2.

:p. UFAT32&per.DLL&colon. Use sector size from BPB instead of hardcoded SECTOR_SIZE&per.
.br

:p.:hp2.Revision r197 (valerius, Thu, 16 Feb 2017)&colon. :ehp2.

:p. SYS&colon. Add dummy os2boot&per.
.br

:p.:hp2.Revision r196 (valerius, Thu, 16 Feb 2017)&colon. :ehp2.

:p. packages&colon. Packaging fix&per.
.br

:p.:hp2.Revision r195 (valerius, Thu, 16 Feb 2017)&colon. :ehp2.

:p. SYS&colon. Add SYS standalone version&per.
.br

:p.:hp2.Revision r194 (valerius, Thu, 16 Feb 2017)&colon. :ehp2.

:p. Add f32chk directory, which was not committed, for some reason&per.
.br

:p.:hp2.Revision r193 (valerius, Thu, 16 Feb 2017)&colon. :ehp2.

:p. UFAT32&per.DLL&colon. Make FORMAT and CHKDSK more portable&per. 

:p. Make FS utilities more portable&per. Move all system-dependent functions to os2&per.c&per.
Add CHKDSK standalone version&per. Also, FORMAT and CHKDSK now successfully build
for win32 target&per. Rename fat32chk&per.exe to f32chk&per.exe&per. Now CHKDSK and FORMAT
standalone versions are called fat32chk&per.exe and fat32fmt&per.exe, for consistency&per.
Add win32 makefile&per.
.br

:p.:hp2.Revision r192 (valerius, Tue, 14 Feb 2017)&colon. :ehp2.

:p. build system&colon. Add forgotten changes from previous commit&per.
.br

:p.:hp2.Revision r191 (valerius, Tue, 14 Feb 2017)&colon. :ehp2.

:p. UFAT32&per.DLL&colon. Use more portable API&apos.s&per.

:p. Use libc and other system-independent interfaces instead of OS/2 API&apos.s&per. This should
be useful to create DOS and win32 versions of CHKDSK and SYSINSTX, together with
FORMAT&per. Also, done some code cleanup&per.
.br

:p.:hp2.Revision r190 (valerius, Mon, 13 Feb 2017)&colon. :ehp2.

:p. CHKDSK&colon. Change more messages to standard oso001&per.msg messages&per.
.br

:p.:hp2.Revision r189 (valerius, Mon, 13 Feb 2017)&colon. :ehp2.

:p. CHKDSK&colon. Change more messages to standard oso001&per.msg messages&per.
.br

:p.:hp2.Revision r188 (valerius, Mon, 13 Feb 2017)&colon. :ehp2.

:p. UFAT32&per.DLL&colon. Manage all messages (to screen/CHKDSK log file, from oso001&per.msg and other messages) 
from one point&per.
.br

:p.:hp2.Revision r187 (valerius, Mon, 13 Feb 2017)&colon. :ehp2.

:p. CHKDSK&colon. Make UFAT32&per.DLL silent if we&apos.re doing autocheck and FS is clean&per.
.br

:p.:hp2.Revision r186 (valerius, Mon, 13 Feb 2017)&colon. :ehp2.

:p. UFAT32&per.DLL&colon. Support for CHKDSK/FORMAT /P command line switch&per.
.br

:p.:hp2.Revision r185 (valerius, Sun, 12 Feb 2017)&colon. :ehp2.

:p. CHKDSK, fat32&per.ifs&colon. Change USHORT->ULONG in several places, to support 64k clusters&per.
.br

:p.:hp2.Revision r184 (valerius, Sat, 11 Feb 2017)&colon. :ehp2.

:p. fat32&per.ifs&colon. Allow read/write/setfileptr on volumes opened in DASD mode&per.
.br

:p.:hp2.Revision r183 (valerius, Sat, 11 Feb 2017)&colon. :ehp2.

:p. CHKDSK&colon. Add missing braces&per.
.br

:p.:hp2.Revision r182 (valerius, Sat, 11 Feb 2017)&colon. :ehp2.

:p. partfilt&per.flt&colon. Eliminate compiler warnings&per.
.br

:p.:hp2.Revision r181 (valerius, Sat, 11 Feb 2017)&colon. :ehp2.

:p. CHKDSK&colon. Don&apos.t use file API&apos.s in CHKDSK&per.

:p. Avoid using API&apos.s like DosQueryPathInfo in CHKDSK, because the volume
is remounted in MOUNT_ACCEPT mode and file API&apos.s are unavailable, and
an attempt to use them may lead to a trap&per. Use internal CHKDSK 
functions instead&per.
.br

:p.:hp2.Revision r180 (valerius, Sat, 11 Feb 2017)&colon. :ehp2.

:p. fat32&per.ifs&colon. Deny access to a volume if it is mounted in MOUNT_ACCEPT mode, thus avoiding a trap&per.
.br

:p.:hp2.Revision r179 (valerius, Fri, 10 Feb 2017)&colon. :ehp2.

:p. CHKDSK&colon. Enable creating of chkdsk&per.log without using DosWrite&per.

:p. Add support for creating a file by CHKDSK on it&apos.s own, without using file
API&apos.s like DosWrite, thus avoiding the use of IFS services&per. So, we can use 
this at autocheck time, when IFS is not initialized, yet&per.
.br

:p.:hp2.Revision r178 (valerius, Fri, 10 Feb 2017)&colon. :ehp2.

:p. fat32&per.ifs&colon. Allow opening a dirty volume with OPEN_FLAGS_DASD to allow checking/formatting it&per.
.br

:p.:hp2.Revision r177 (valerius, Fri, 10 Feb 2017)&colon. :ehp2.

:p. fat32&per.ifs&colon. Don&apos.t let trying to insert a new entry in a full directory cluster, and thus, exceeding 
a segment limit (which could lead to a trap)&per.
.br

:p.:hp2.Revision r176 (valerius, Fri, 10 Feb 2017)&colon. :ehp2.

:p. fat32&per.ifs&colon. Fix failing to discover large file API presence&per.
.br

:p.:hp2.Revision r175 (valerius, Mon, 06 Feb 2017)&colon. :ehp2.

:p. CHKDSK&colon. CHKDSK log support&per.
.br

:p.:hp2.Revision r174 (valerius, Sun, 05 Feb 2017)&colon. :ehp2.

:p. CHKDSK&colon. Add fflush(stdout) for the user to see the lost clusters message&per.
.br

:p.:hp2.Revision r173 (valerius, Sun, 05 Feb 2017)&colon. :ehp2.

:p. CHKDSK&colon. Set timestamp for recovered lost chains&per.
.br

:p.:hp2.Revision r172 (valerius, Sun, 05 Feb 2017)&colon. :ehp2.

:p. fat32&per.ifs&colon. Don&apos.t allow disk access if it is dirty&per.
.br

:p.:hp2.Revision r171 (valerius, Sun, 05 Feb 2017)&colon. :ehp2.

:p. CHKDSK&colon. Update CHKDSK messages&per.
.br

:p.:hp2.Revision r170 (valerius, Sat, 24 Dec 2016)&colon. :ehp2.

:p. build system&colon. Fixed bldlevel string&per.
.br

:p.:hp2.Revision r169 (valerius, Sun, 04 Dec 2016)&colon. :ehp2.

:p. build system&colon. Another WarpIN packaging bug fixed&per.
.br

:p.:hp2.Revision r168 (valerius, Sun, 04 Dec 2016)&colon. :ehp2.

:p. build system&colon. Minor WarpIN packaging bug fixed&per.
.br

:p.:hp2.Revision r167 (valerius, Sun, 04 Dec 2016)&colon. :ehp2.

:p. build system&colon. Separate &per.sym files to additional WarpIN package&per.
.br

:p.:hp2.Revision r166 (komh, Sat, 26 Nov 2016)&colon. :ehp2.

:p. Move branches/fat32-0&per.10 to trunk

:p. valerius merged all changes of trunk to 0&per.10 branch, and many
improvements are applied to 0&per.10 branch&per.

:p. So move trunk to branches/trunk-old for a backup, and 0&per.10 branch to
trunk&per.

:p. See ticket #26 for details&per.
.br

:p.:hp2.Revision r165 (komh, Sat, 26 Nov 2016)&colon. :ehp2.

:p. Move trunk to branches/trunk-old

:p. valerius merged all changes of trunk to 0&per.10 branch, and many
improvements are applied to 0&per.10 branch&per.

So move trunk to branches/trunk-old for a backup, and 0&per.10 branch to
trunk&per.

:p. See ticket #26 for details&per.
.br

:p.:hp2.Revision r164 (valerius, Mon, 21 Nov 2016)&colon. :ehp2.

:p. docs&colon. Documentation update&per.
.br

:p.:hp2.Revision r163 (valerius, Mon, 21 Nov 2016)&colon. :ehp2.

:p. fat32&per.ifs&colon. Update FS status (clean/dirty) on shutdown, continue with next volumes if something 
went wrong with the current one&per.
.br

:p.:hp2.Revision r162 (valerius, Wed, 16 Nov 2016)&colon. :ehp2.

:p. build system&colon. Small enhancements&per.
.br

:p.:hp2.Revision r161 (valerius, Tue, 15 Nov 2016)&colon. :ehp2.

:p. build system&colon. Fix broken ufat32&per.dll build&per.
.br

:p.:hp2.Revision r160 (valerius, Tue, 15 Nov 2016)&colon. :ehp2.

:p. build system&colon. Support for separate build and source dirs, and also, autopackaging&per.
.br

:p.:hp2.Revision r159 (valerius, Mon, 14 Nov 2016)&colon. :ehp2.

:p. bldsystem&colon. Minor changes to bldlevel creation&per.
.br

:p.:hp2.Revision r158 (valerius, Mon, 14 Nov 2016)&colon. :ehp2.

:p. build system&colon. Automatically add extra info to bldlevel of each binary&per.
.br

:p.:hp2.Revision r157 (valerius, Sun, 13 Nov 2016)&colon. :ehp2.

:p. fat32&per.ifs&colon. Avoid to enable large files support to prevent a trap when accessing sfi_sizel/sfi_positionl 
if kernel does not support DosOpenL &amp. Co&per.
.br

:p.:hp2.Revision r156 (valerius, Thu, 10 Nov 2016)&colon. :ehp2.

:p. fat32&per.ifs&colon. Fix for trap 000d in ReadSector when trying to allocate a buffer for a 64k cluster&per.
.br

:p.:hp2.Revision r155 (valerius, Thu, 10 Nov 2016)&colon. :ehp2.

:p. fat32&per.ifs&colon. Fix for division by zero on a JFS partition on mount, when booting from a fat32 medium&per.
.br

:p.:hp2.Revision r154 (valerius, Thu, 10 Nov 2016)&colon. :ehp2.

:p. fat32&per.ifs&colon. Code cleanup and formatting in FS_MOUNT&per.
.br

:p.:hp2.Revision r153 (valerius, Mon, 07 Nov 2016)&colon. :ehp2.

:p. fat32&per.ifs&colon. Return ERROR_INVALID_PARAMETER, like hpfs&per.ifs does&per.
.br

:p.:hp2.Revision r152 (valerius, Mon, 07 Nov 2016)&colon. :ehp2.

:p. fat32&per.ifs&colon. Files up to 4 GB support&per.

:p.:hp2.Revision r151 (valerius, Mon, 07 Nov 2016)&colon. :ehp2.

:p. fat32&per.ifs&colon. Check if size/file pos&per. does not exceed 2 or 4 Gb&per.
.br

:p.:hp2.Revision r150 (valerius, Fri, 04 Nov 2016)&colon. :ehp2.

:p. Support for 64 KB clusters&per.
.br

:p.:hp2.Revision r149 (valerius, Wed, 02 Nov 2016)&colon. :ehp2.

:p. fat32&per.ifs&colon. Prevent blocking in FSH_SETVOLUME when remounting after format&per.
.br

:p.:hp2.Revision r148 (valerius, Thu, 27 Oct 2016)&colon. :ehp2.

:p. fat32&per.ifs&colon. Fix for ERROR_SEM_TIMEOUT error in f32mon&per.exe&per.
.br

:p.:hp2.Revision r147 (valerius, Thu, 27 Oct 2016)&colon. :ehp2.

:p. fat32&per.ifs&colon. Code style fix&per.
.br

:p.:hp2.Revision r146 (valerius, Wed, 26 Oct 2016)&colon. :ehp2.

:p. fat32&per.ifs&colon. Some previously uncommitted changes flush (FS_IOCTL code didn&apos.t went through, for some reason)&per.
.br

:p.:hp2.Revision r145 (valerius, Wed, 26 Oct 2016)&colon. :ehp2.

:p. fat32&per.ifs&per. Some previously uncommitted changes flush&per.
.br

:p.:hp2.Revision r144 (valerius, Wed, 26 Oct 2016)&colon. :ehp2.

:p. fat32&per.ifs&colon. Some previously uncommitted changes flush&per.
.br

:p.:hp2.Revision r143 (valerius, Wed, 26 Oct 2016)&colon. :ehp2.

:p. fat32&per.ifs&colon. Use QSINIT/OS4LDR log, if it is detected, otherwise send a string directly 
to a com port&per.
.br

:p.:hp2.Revision r142 (valerius, Wed, 26 Oct 2016)&colon. :ehp2.

:p. fat32&per.ifs&colon. Add support for QSINIT/OS4LDR log buffer&per.
.br

:p.:hp2.Revision r141 (valerius, Sun, 23 Oct 2016)&colon. :ehp2.

:p.fat32.ifs&colon. Temporarily disable lazy write start in FAT32_STARTLW ioctl worker&per.
.br

:p.:hp2.Revision r140 (valerius, Sun, 23 Oct 2016)&colon. :ehp2.

:p. fat32&per.ifs&colon. Always do flush volume etc&per. on FS unmount&per.
.br

:p.:hp2.Revision r139 (valerius, Sun, 23 Oct 2016)&colon. :ehp2.

:p. format&colon. Add start/stop lazy write ioctls&per.
.br

:p.:hp2.Revision r138 (valerius, Sun, 23 Oct 2016)&colon. :ehp2.

:p. format&colon. Some error checking&per.
.br

:p.:hp2.Revision r137 (valerius, Sun, 23 Oct 2016)&colon. :ehp2.

:p. format&colon. Add reserved sectors command line option&per.
.br

:p.:hp2.Revision r136 (valerius, Fri, 14 Oct 2016)&colon. :ehp2.

:p. Merge changes with trunk&per.
.br

:p.:hp2.Revision r135 (valerius, Fri, 14 Oct 2016)&colon. :ehp2.

:p. fat32&per.ifs&colon. Preliminary swap support (not working atm)&per.
.br

:p.:hp2.Revision r134 (valerius, Fri, 14 Oct 2016)&colon. :ehp2.

:p. makefiles&colon. Use version string in makefile&per.mk, and other makefiles including it&per.
.br

:p.:hp2.Revision r133 (valerius, Fri, 14 Oct 2016)&colon. :ehp2.

:p. format&colon. Use sector mode for formatting&per..
.br

:p.:hp2.Revision r132 (valerius, Sat, 08 Oct 2016)&colon. :ehp2.

:p. fat32&per.ifs&colon. Add checks for removables to be unmounted&per.
.br

:p.:hp2.Revision r131 (valerius, Mon, 07 Oct 2016)&colon. :ehp2.

:p. scripts&colon. Add @echo off&per.
.br

:p.:hp2.Revision r130 (valerius, Mon, 07 Oct 2016)&colon. :ehp2.

:p. format&colon. Added FAT32_SECTORIO ioctl for better speed&per.
.br

:p.:hp2.Revision r129 (valerius, Mon, 07 Oct 2016)&colon. :ehp2.

:p. format&colon. Fix format failing on already FAT32 formatted drives&per.
.br

:p.:hp2.Revision r128 (valerius, Mon, 07 Oct 2016)&colon. :ehp2.

:p. scripts&colon. Make a single environment-setting script in the root dir, and others calling it&per.
.br

:p.:hp2.Revision r127 (valerius, Mon, 05 Sep 2016)&colon. :ehp2.

:p. ufat32&per.dll&colon. Added missing recover&per.c&per.
.br

:p.:hp2.Revision r126 (valerius, Sun, 04 Sep 2016)&colon. :ehp2.

:p. Additional autocheck on boot changes&per.
.br

:p.:hp2.Revision r125 (valerius, Sun, 04 Sep 2016)&colon. :ehp2.

:p. ufat32&per.dll&colon. Fix for reading/writing sectors&colon. Correct the amount of bytes to 
write on the current iteration&per.
.br

:p.:hp2.Revision r124 (valerius, Wed, 24 Aug 2016)&colon. :ehp2.

:p. ufat32&per.dll&colon. Add partition type modifying to FAT32&per.
.br

:p.:hp2.Revision r123 (valerius, Wed, 24 Aug 2016)&colon. :ehp2.

:p. ufat32&per.dll&colon. Add preliminary SYS command&per.
.br

:p.:hp2.Revision r122 (valerius, Wed, 24 Aug 2016)&colon. :ehp2.

:p. Makefiles&colon. Add root makefile&per. Add clean target&per.
.br

:p.:hp2.Revision r121 (valerius, Wed, 24 Aug 2016)&colon. :ehp2.

:p. Makefiles&colon. Add root makefile&per. Add clean target&per.
.br

:p.:hp2.Revision r120 (valerius, Sun, 21 Aug 2016)&colon. :ehp2.

:p. ufat32&per.dll&colon. Make CHKDSK to set volume clean&per.
.br

:p.:hp2.Revision r119 (valerius, Sun, 21 Aug 2016)&colon. :ehp2.

:p. fat32&per.ifs&colon. Delete forgotten debug messages from FS_DELETE&per.
.br

:p.:hp2.Revision r118 (valerius, Sun, 21 Aug 2016)&colon. :ehp2.

:p. Fix trap in FS_DELETE when rewriting files and disk is dirty&per.
.br

:p.:hp2.Revision r117 (valerius, Sun, 21 Aug 2016)&colon. :ehp2.

:p. FAT32 autocheck on init&per. Removed ioctl and fsctl calls to fat32&per.ifs from CHKDSK and ported 
the corresponding code from fat32.ifs to CHKDSK&per.
.br

:p.:hp2.Revision r116 (valerius, Wed, 17 Aug 2016)&colon. :ehp2.

:p. fat32&per.ifs&colon. Fixed bldlevel strings&per.
.br

:p.:hp2.Revision r115 (valerius, Wed, 17 Aug 2016)&colon. :ehp2.

:p. fat32&per.ifs&colon. Fix a trap after FS_MOUNT on REDETERMINEMEDIA ioctl&per. Added push es/pop es 
on enter/exit of each top-level IFS routine&per.
.br

:p.:hp2.Revision r114 (valerius, Sat, 02 Jul 2016)&colon. :ehp2.

:p. fat32-0&per.10&colon. Ticket #16 fixed&per. added floating point support inlining and initinstance/terminstance&per.
.br

:p.:hp2.Revision r113 (valerius, Mon, 04 Jan 2016)&colon. :ehp2.

:p. Additional __loadds to _loadds
.br

:p.:hp2.Revision r112 (valerius, Mon, 04 Jan 2016)&colon. :ehp2.

:p. AWK script OW->VAC for map files update
.br

:p.:hp2.Revision r111 (valerius, Mon, 04 Jan 2016)&colon. :ehp2.

:p. Build &per.cmd files for convenience
.br

:p.:hp2.Revision r110 (valerius, Mon, 04 Jan 2016)&colon. :ehp2.

:p. Misc non-important files
.br

:p.:hp2.Revision r109 (valerius, Mon, 04 Jan 2016)&colon. :ehp2.

:p. Change __loadds to _loadds
.br

:p.:hp2.Revision r108 (valerius, Mon, 04 Jan 2016)&colon. :ehp2.

:p. Makefile changes for ufat32&per.dll
.br

:p.:hp2.Revision r107 (valerius, Mon, 04 Jan 2016)&colon. :ehp2.

:p. Makefile changes for ufat32&per.dll
.br

:p.:hp2.Revision r106 (valerius, Mon, 04 Jan 2016)&colon. :ehp2.

:p. Makefile changes for FS utilities
.br

:p.:hp2.Revision r105 (valerius, Mon, 04 Jan 2016)&colon. :ehp2.

:p. newer fat32&per.txt
.br

:p.:hp2.Revision r104 (valerius, Mon, 04 Jan 2016)&colon. :ehp2.

:p. Added &per.ipf files of ifs and fat32 &per.inf&apos.s
.br

:p.:hp2.Revision r103 (valerius, Mon, 04 Jan 2016)&colon. :ehp2.

:p. Minor headers changes
.br

:p.:hp2.Revision r102 (valerius, Mon, 04 Jan 2016)&colon. :ehp2.

:p. Make partfilt.flt build with OW and MSC
.br

:p.:hp2.Revision r101 (valerius, Sun, 22 Mar 2015)&colon. :ehp2.

:p. Made format progress percent indicator working&per.
.br

:p.:hp2.Revision r100 (valerius, Sun, 22 Mar 2015)&colon. :ehp2.

:p. Made ufat32&per.dll (FORMAT) reformat-friendly&per. Now fat, fat32, hpfs, jfs could be reformatted 
from each one to each other&per.
.br

:p.:hp2.Revision r99 (valerius, Fri, 27 Feb 2015)&colon. :ehp2.

:p. A minor fix of Fat32Format for Win32 case&per.
.br

:p.:hp2.Revision r98 (valerius, Fri, 27 Apr 2015)&colon. :ehp2.

:p. Updated ufat32&per.dll fo Fat32Format 1&per.07&per. Eliminated some minor bugs&per. The bug with 
kernel trap when formatting the fat32 partition when booted from another fat32 partition&per.
.br

:p.:hp2.Revision r97 (valerius, Wed, 30 Apr 2014)&colon. :ehp2.

:p. Initial branch 0&per.10 commit. Now added the FORMAT routine (adopted Fat32format from Win32)&per. 
It is 32-bit&per. Also, CHKDSK routine made 32-bit, plus small 16-bit wrappers for calling from usual frontends, 
like chkdsk&per.exe/format&per.exe&per. Added fixes to the IFS needed to support (re-)formatting to/from FAT32&per.
Also, from now, all parts can be compiled by OpenWatcom wcc[386] and wasm&per.
.br

:p.:hp2.Revision r96 (komh, Sun, 14 Dec 2008)&colon. :ehp2.

:p.Fix a non-sector IO problem on a smaller volume than 2GB&per.
.br

:p.:hp2.Revision r95 (komh, Sun, 14 Dec 2008)&colon. :ehp2.

:p. Fix a non-sector IO problem on a smaller volume than 2GB&per.
.br

:p.:hp2.Revision r94 (komh, Sun, 14 Dec 2008)&colon. :ehp2.

:p. Convert a size from in bytes to in sectors when user request sector io on a less volume than 2GB&per.
.br

:p.:hp2.Revision r93 (komh, Sun, 14 Dec 2008)&colon. :ehp2.

:p. Convert a size from in bytes to in sectors when user request sector io on a less volume than 2GB&per.
.br

:p.:hp2.Revision r92 (komh, Sun, 14 Dec 2008)&colon. :ehp2.

:p. Support OPEN_FLAGS_DASD correctly on the devices larger than 2GB using sector IO like HPFS&per.
.br

:p.:hp2.Revision r91 (komh, Sat, 13 Dec 2008)&colon. :ehp2.

:p. Support OPEN_FLAGS_DASD correctly on the devices larger than 2GB using sector IO like HPFS&per.
.br 

:p.:hp2.Version 0&per.9&per.13&colon. :ehp2.

:p.Fixed the problem that a program trying to READ/WRITE from/to memory object  
with OBJ_ANY attribute is crashed&per. With fix, READ/WRITE performance is decreased
&per. 
.br 
Fixed the problem that CACHEF32&per.EXE prints &apos.Unicode translate table  
for xxx loaded&apos. even though &apos./S&apos. is specified&per. 
.br 
Clarified that messages of CACHEF32&per.EXE are related to FAT32&per. 
.br 
Improved lazy-write performance&per. 
.br 
  
:p.:hp2.Version 0&per.9&per.12&colon. :ehp2.

:p.Fixed the problem that CACHEF32 is crashed after calling CHKDSK for dirty  
volume&per. 
.br 
Fixed the problem the regonition of USB removable media takes very long time  
when inserting it since 0&per.9&per.11&per.  This is because FAT32 always calculates 
free space on mount&per. Now, FAT32  calculates free space only if /CALCFREE 
specified or free space info was not  stored on disk&per. 
.br 
Fixed the trap when removing USB removable media without eject&per. 
.br 
Improved Read/Write performance&per. By Lars&per.   

:p.*** Known Problems ***   

:p.When using USB removable media with other OS such as Windows, free space can 
be incorrect&per. At this time, you should use &apos.chkdsk&apos. to correct it or 
&apos./CALCFREE&apos. to avoid it&per. 
.br 
  
:p.:hp2.Version 0&per.9&per.11&colon. :ehp2.

:p.Fixed non-recognition problem of USB removable drive change&per. 
.br 
Fixed &osq.free space&osq. problem&per. 
.br 
Fixed some minor warnings such as &osq.non used variable&osq. on compilation
&per. 

:p.Known Problem&colon. Somtimes the pending flush state of a few sectors is not 
cleared&per. You can see this by report of CACHEF32&per.  Even though any dirty sector 
is not there, pending sector exist&per. However, this has no effect on the 
functionality of IFS&per. 
.br 
  
:p.:hp2.Version 0&per.9&per.10&colon. :ehp2.

:p.Ejection of removable media caused a dirty flag on removable drive&per. 
Fixed&per. 
.br 
Problem of driver trying to read from marked bad sectors on drives&per. Fixed 
.br 
Trap with RM&per. Fixed 
.br 
Changed version numbering style to major&per.minor&per.patch level style&per. 
.br 
Removed beta symbol&per. 
.br 
Filename including NLS characters not converted to unicode by OS/2, Caused 
&apos.lost cluster&apos. problem&per. fixed&per. 
.br 
Added IOCTL function to write in sector mode&per. 
.br 
  
:p.:hp2.Version 0&per.99&colon. :ehp2.

:p.CACHEF32&per.EXE caused a continuously beep if a copy of CACHEF32&per.EXE 
was already loaded and the current CP was different from a copy of FAT32 IFS&per. 
Fixed&per. 
.br 
Case conversion of national characters worked only for ASCII below 128&per. 
Introduced in version 0&per.98b&per. Fixed&per. 
.br 
Sometimes copies of the FATs did not match each other or &apos.lost cluster
&apos. occurred after copy operation with cache enabled&per. Fixed (maybe workaround ?
)&per. Unfortunately, cache performance is decreased as a result&per. 
.br 
Replaced legacy DBCS APIs with ULS APIs&per. 
.br 
Added /H option to FAT32&per.IFS to allocate cache memory in high memory (>16MB
)&per. This should take care of the &apos.linalloc&apos. problem&per. 
.br 
Added /F option to CACHEF32&per.EXE to force CACHEF32&per.EXE to be loaded 
without any FAT32 partition&per. Hopefully, this option can help people who use 
removable media formatted FAT32&per. 
.br 
Added a feature to clear fForceLoad flag to &apos./N&apos. option of CACHEF32
&per.EXE&per. 
.br 
FAT32&per.IFS process parameters are no longer case sensitive&per. 
.br 
  
:p.:hp2.Version 0&per.98&colon. :ehp2.

:p.Fixed TranslateName() to support FAT32 variation of WinNT family&per. 
.br 
Fixed FS_PATHINFO to support level 7 correctly&per. 
.br 
Both /FS and /FL option are not supported any more&per. 
.br 
Added &apos.Directory Circularity Check&apos. for FS_COPY and FS_MOVE in case 
of mixed path of short and long name&per. 
.br 
Added /CP option to CACHEF32&per.EXE to specify codepage in paramter&per. 
.br 
  
:p.:hp2.Version 0&per.97&colon. :ehp2.

:p.The partition support packages, PARTFILT&per.FLT and OS2DASD&per.F32, for 
non-LVM systems have been removed from the FAT32&per.IFS driver package&per. 
.br 
Renamed the following files to&colon. 
.br 
-F32MON&per.EXE   (formerly MONITOR&per.EXE) 
.br 
-F32PARTS&per.EXE (formerly DISKINFO&per.EXE) 
.br 
Why? By renaming these files, you will know they are all part of the FAT32 
distribution, AND you see them all grouped no matter what you use to browse your system&per. 
.br 
The FAT32&per.INF file has been revised with a different format and more 
information&per. 
.br 
There was error in the shutdown routine of FAT32 main source which was 
unintentionally added when DBCS support was added&per. Fixed&per. 
.br 
The possibility existed that files created by any system of the WinNT family 
such as Win2X and WinX could be recognized as having Extended Attributes by the 
FAT32 driver&per. Fixed&per. 
.br 
Changed EA mark byte for compatibility with WinNT family&per. 
.br 
Driver now supports 8&per.3 names whose name and ext part are all lower case 
and created by WinNT family&per. 

:p.:hp2.IMPORTANT&colon.  :ehp2.Starting with version 0&per.97, CHKDSK /F must first be run 
on each FAT32 partition before adding EA support to the FAT32 IFS driver&per. This 
step is required starting with this version due to the EA mark byte being changed 
for compatibility with the WinNT family&per. 
.br 
  
:p.:hp2.Version 0&per.96&colon. :ehp2.

:p.Changed the maximum value of RASECTORS from sectors per cluster times 4 to 
128&per. 
.br 
Fixed TRAP on accessing FAT32 partition having 32k cluster&per. 
.br 
Now FAT32&per.IFS does not mount FAT32 partition having >32k cluster&per. 
.br 
The DBCS [0xHH5C] code and [1 byte-katakana] was not changed correctly&per. The 
1st byte code of DBCS contained in SBCS caused the conversion which is not right
&per. Fixed&per. (Previously undocumented)&per. 
.br 
  
:p.:hp2.Version 0&per.95&colon. :ehp2.

:p.FAT32&per.IFS now supports DBCS, especially Korean, correctly&per. 
.br 
Fixed &apos.unsigned&apos. overflow problems&per. 
.br 
When both /FS and /EAS are enabled, MOVE and REN command don&apos.t process EA 
files correctly&per. Fixed&per. 
.br 
When /FL(default) is enabled, FillDirEntry doesn&apos.t recognize 8&per.3 name 
pointing to the same file as long name&per. 
.br 
When /EAS is disabled, FAT32&per.IFS now pass empty EA&per. 
.br 
Changed UFAT32 to find volume label FAT32 in ROOT directory not in bootsector
&per. 
.br 
Changed UFAT32 to report &apos.file allocation error&apos. as long name&per. 
.br 
Added FSCTL function to get short name from long name&per. 
.br 
When /FS is enabled and a target file already exists, COPY and MOVE command 
names a target file to the short name of the exisintg file&per. Fixed&per. 
.br 
Fixed bug of &apos.Dest check routine&apos. in FS_COPY&per. 
.br 
Fixed a potential problem in FS_MOVE&per. 
.br 
Fixed a problem of CACHEF32&per.EXE with restarting lazy write deamon after 
quiting it on CMD&per.EXE&per. 
.br 
Added /Y to assume &apos.Yes&apos. for CACHEF32&per.EXE 
.br 
Added /S to prevent CACHEF32&per.EXE from displaying normal messages&per. 
.br 
  
:p.:hp2.Version 0&per.94&colon. :ehp2.

:p.Fixed a problem CHKDSK had when finding bad sectors&per. 
.br 
Fixed a problem when running in &apos.Internal Shortname mode&apos. that long 
files were not found&per. 
.br 
  
:p.:hp2.Version 0&per.93&colon. :ehp2.

:p.Modified the way CHKDSK detected a bad cluster&per. 
.br 
When the disk was dirty, CHKDSK was unable to rename a lost EA file to a proper 
file&per. This has been fixed&per. 
.br 
FAT32&per.IFS now sets the archive attribute&per. It didn&apos.t before&per. 
.br 
Messed about a bit with the cache and lazy write handling&per. 
.br 
Rewrote UFAT32&per.DLL to utilize a IOCtl call to read sectors 
.br 
  
:p.:hp2.Version 0&per.92&colon. :ehp2.

:p.When a FAT32 volume was not shut down properly the IFS did not allow files 
to be opened&per. This caused device drivers etc during shutdown not being loaded 
from a FAT32 volume&per. Now FAT32&per.IFS allows files to be read from a dirty 
volume&per. 
.br 
When a directory was moved the &per.&per. entry in that directory was not 
updated to point to the proper parent directory&per. This has been corrected&per. 
.br 
CHKDSK checked for extended attributes even when /EAS was not specified after 
the IFS line in the config&per.sys&per. 
.br 
CHKDSK contained an error that when checking subdirectories sometimes (rare) 
the incorrect name (the longname of the previous directory entry) was used&per. 
This lead to claimed missing extended attributes for non-existing files&per. 
.br 
Modified the output of CHKDSK somewhat&per. Also, when the output is redirected 
to a file, the progress percentages are suppressed&per. 
.br 
  
:p.:hp2.Version 0&per.91&colon. :ehp2.

:p.Got a report that FAT32&per.IFS ignored the fourth character of the 
extention&per. Looking for the cause I found that FAT32&per.IFS created incorrect short 
name equivalents for files with a four character long extention&per. In that 
specific case the fourth character was ignored when creating the shortname&per. The 
effect was that e&per.g&per. a file called file&per.1234 and file&per.1235 ended up 
having the same shortname&per. FAT32&per.IFS now no longer exhibits this behaviour, 
but you still might have double shortnames in the same directory on your disk&per. 
I don&apos.t think SCANDISK detects this problem&per. Please note this could only 
have happened with files having an extention of four chars for which all chars 
except the fourth extention char were equal&per. 

:p.To check this, set FAT32&per.IFS to use short names internally (CACHEF32 /FS)
, and in a DOS session do a DIR command&per. If you detect any duplicate names, 
switch back to an OS/2 session and rename one of these files to another name and then 
rename it back to the original name&per. 
.br 
Modified the calling of the strategy2 calls&per. Now FAT32&per.IFS no longer 
request confirmation per individual sector&per. This fixes the problems with AURORA
&per. The problem with AURORA (Warp Server for eBusiness) FAT32&per.IFS was 
experiencing was caused by a bug in the Logical Volume Manager that did not properly confirm 
individual requests&per. Since FAT32&per.IFS now no longer depends on this individual 
confirmation the bug is no longer relevant&per. BM did confirm this bug to me (October 1999) 
and promissed it would be solved in the next fixpack&per. 
.br 
Introduced a separate thread (from CACHEF32&per.EXE) that runs at time critical 
priority&per. The thread is awakened whenever the cache runs full with dirty sectors and 
takes care of flushing these dirty sectors&per. 
.br 
Corrected a problem with Extended Attributes when a file had very large 
extended attributes (almost 65536 bytes) the system would trap&per. 
.br 
Corrected a problem when a sector towards the end of the disk was read&per. Due 
to the read-ahead function 1 sector too far was read and the read failed&per. 
.br 
Noticed that the diskspace under DOS was not correctly returned&per. The fake 
clustersize was too small to actually report the correct disksize for disks smaller then 2
&per.1 Gb&per. 
.br 
Until this version, the strategy2 routines did not set the flag to request the 
disk controller to cache the request on the outboard controller&per. I did not set 
this flag since it didn&apos.t seem to matter on my own machine&per. However, this 
version has the flag set since some of you might profit from this setting and 
experience better performance&per. 
.br 
Enlarged maximum amount of data that is flushed (when lazy writing is used) to 
64 Kb&per. Hopefully, this will increase performance for those disk controllers 
that do not have the on-board cache enabled&per. 
.br 
Did an effort to solve the problem when a cluster chain becomes improperly 
terminated&per. FAT32&per.IFS simply ignores the problem now and assumes end of file&per. 
CHKDSK (UFAT32&per.DLL) detects and corrects this problem&per. 
.br 
  
:p.:hp2.Version 0&per.90&colon. :ehp2.

:p.Found some problems with FS_COPY&per. If FS_COPY was copying a file the 
target file was not protected from being deleted or renamed in another session&per. 
.br 
Noticed that I had the default cache size incorrectly set to 128 Kb&per. 
.br 
Modified it to 1024 kb and added a warning message if no /CACHE argument is 
present&per. 
.br 
FS_COPY didn&apos.t check if source and target where on the same partition&per. 
(I incorrectly assumed OS/2 did this check&per.)&per. Now FS_COPY doesn&apos.t 
try to handle the copy when source and target are on different drives&per. This way 
the copy is done by OS/2 itself&per. 
.br 
Modified the way data is written from the cache to disk&per. Instead of 
instructing the Device Driver to write data directly from cache to disk, the data is now 
first copied to another memory area thereby releasing the data in the cache a lot 
sooner&per. This way the IFS does not have to wait for the device driver to finish 
before a specific sector in cache is  available again&per. The drawback is that this 
makes FAT32&per.IFS to use more (physical) memory&per. With a cache size of 2048 Kb, 
an additional 480 Kb is allocated&per. With smaller cache sizes the additional 
data reduces relatively&per. (a cache size of 1024 Kb leads to an additional memory 
allocation of 240 Kb) 
.br 
Never had any error handling (message) in the routine that is called by the 
device driver to notify completion of the strategy2 request list&per. This version has 
it&per. This is mainly to get more light on FAT32&per.IFS and aurora&per. 
.br 
  
:p.:hp2.Version 0&per.89&colon. :ehp2.

:p.FS_SHUTDOWN is called twice by the kernel&per. The first one is to signal 
begin of shutdown and the second to signal the end&per. FAT32&per.IFS flushed its 
internal buffers on the 2nd call&per. This has been modified so FAT32&per.IFS now 
flushes on the first call&per. 
.br 
I&apos.ve had a report from someone with a problem where the drives where 
mounted after CACHEF32 had queried CACHE settings&per. Since the cache is allocated on 
first mount this didn&apos.t work&per. I have added a call to force the drives to be 
mounted before CACHEF32 queries the cache settings&per. 
.br 
Fixed a small division by zero in CACHEF32 when the cache was zero sectors 
large&per. 
.br 
Fixed the &apos.cannot find message file&apos. problem in CACHEF32&per.EXE&per. 
.br 
Some users keep having troubles with &apos.error cannot find SH&osq.&per. Just 
for myself added the filename there&per. 
.br 
Replaced an internal call (FSH_PROBEBUF) by a DevHelp call&per. 
.br 
Fixed a trap I got due to a reentrancy problem in my memory allocation routines
&per. 
.br 
Changed the way data is kept per open file instance because I have received 
some reports from users that they got a &apos.ERROR&colon. Cannot find the IO!&osq. 
message&per. 
.br 
FAT32&per.IFS had its code segments marked as EXECUTEONLY&per. Now AURORA doesn
&apos.t seem to like that and traps&per. The code segments are no longer marked this 
way but as EXECUTEREAD&per. 
.br 
  
:p.:hp2.Version 0&per.88&colon. :ehp2.

:p.DISKINFO still didn&apos.t show partitions inside an extended partition type 
F&per. 
.br 
Fixed a potential problem in the lazy write routines&per. This could lead to 
sectors not being written when they should have&per. (have never seen it though) 
.br 
Made it work with Fixpack 10&per. (FP10 made FAT32&per.IFS trap) 
.br 
  
:p.:hp2.Version 0&per.87&colon. :ehp2.

:p.Modified the internal memory usage somewhat&per. Some allocated segments are 
marked unswappable now&per. 
.br 
Modified FS_DELETE to return ERROR_ACCESS_DENIED whenever this call was used 
for a directory&per. Previously, FAT32&per.IFS return ERROR_FILE_NOT_FOUND&per. 
.br 
Modified CACHEF32&per.EXE so it will also run if UCONV&per.DLL cannot be loaded
&per. If that is the case, no UNICODE translate table will be loaded&per. 
.br 
Solved a problem when a file was rename when on the case mapping was changed
&per. If the file had extended attributes the rename failed and the EA was lost&per. 
.br 
  
:p.:hp2.Version 0&per.86&colon. :ehp2.

:p.Always thought that it was not allowed to the directory bit set in 
DosSetPathInfo and therefor I rejected calls with this bit set&per. I was wrong&per. Now 
DosSetPathInfo accepts the directory bit set (for directories only)&per. 
.br 
AModified partfilt so it will also scan partitions inside a extended type F 
partition (partfilt is now on version 1&per.08)&per. Modified DISKINFO&per.EXE to do the 
same&per. 
.br 
A Noticed some problems when a file was opened multiple times&per. The 
directory information was not always updated properly&per. Also, the attribute of a open 
file was kept per instance instead of only one time for all open instances of a file
&per. So using DosSetFileInfo on one instance was not noticed by other instances of a 
file&per. 
.br 
  
:p.:hp2.Version 0&per.85&colon. :ehp2.

:p.A user pointed me at the problem that ReadOnly files cannot be renamed&per. 
This problem has been corrected&per. 
.br 
Corrected a problem whenever a non-existing file was opened&per. If later the 
file was created this could lead to a trap (at least in theory&colon. I haven&apos.t 
seen it nor heard it actually occured)&per. 
.br 
Changed PARTFILT again&per. Whenever a hidden partition type was specified 
partfilt also virtualized the unhidden type&per. So if /P 16 was specified, both types 
16 and 6 were virtualized&per. This could lead to unwanted results&per. 

:p.So I modified the mechanism again&per. Now - if /A is not used - only the 
types specified after /P are virtualized&per. (The /A switch controls whether or not 
to virtualize all partitions&per.) 

:p.PARTFILT always unhides the partitions it virtualizes&per. 

:p.For the partition types PARTFILT virtualizes the following rules apply&colon. 

:p.Normal partition types (types 1, 4, 6, 7, but also 11, 14, 16 and 17) are 
reported to OS/2 with their actual -unhidden- partition type&per. 

:p.Any other partition types specified after /P are reported as un-hidden IFS 
partitions&per. Any other partition types NOT specified after /P are reported as their 
actual -unhidden- type&per. 

:p.Keep in mind that if you specify /A you must also use the / M argument to 
tell PARTFILT which partitions you want to mount&per. 
.br 
Also, whenever /A is specified with PARTFILT /W is automatically set&per. 
Otherwise, OS/2 will not boot at all&per. 
.br 
CACHEF32&per.EXE did not properly handle the /P&colon.x argument when specified 
in CONFIG&per.SYS&per. The argument was lost&per. This has been fixed&per. 
.br 
  
:p.:hp2.Version 0&per.84&colon. :ehp2.

:p.Got a message from someone complaining that the change in PARTFILT made him 
loose the ability to mount hidden &apos.normal&apos. partitions since PARTFILT 
presented all partition types as IFS&per. Now PARTFILT show normal partitions by their 
actual types and only not-by-OS/2 supported types as IFS partitions&per. 
.br 
Increased performance for accessing large files by keeping track of first and 
last cluster for each open file, and even the current cluster for each open instance
&per. 
.br 
Removed the limit for the maximum number of open files&per. 
.br 
Fixed a problem in my internal memory (sub)allocation routines that lead to 
various problems&per. This became more clear since FS_OPEN now also uses malloc to 
allocated memory per open file&per. Problems I encountered were trap D&apos.s and 
internal fatal messages&per. 
.br 
Changed the way MONITOR works&per. Introduced a &apos.trace mask&apos.&per. See 
MONITOR above&per. 
.br 
  
:p.:hp2.Version 0&colon.83&colon. :ehp2.

:p.Replaced the translate mechanisme that was introduced in version 0&per.75 by 
a mechanisme where CACHEF32&per.EXE on first load calls the native OS/2 NLS 
support to create a default translate table between the current codepage brand unicode
&per. This should work better for some NLS versions of OS/2&per. 
.br 
Added a switch to CACHEF32 to dynamically change the priority of the lazy write 
thread&per. The default priority is still idle-time, but this can be changed to a 
higher priority&per. 
.br 
Did experiment a bit with lazy writing in general&per. In my own system it 
improved performance somewhat&per. 
.br 
Corrected a problem in PARTFILT where whenever a HIDDEN partition type was 
specified after /P this didn&apos.t always work properly&per. Now when a hidden partition 
type is specified, the unhidden type is also handled by PARTFILT&per. 
.br 
  
:p.:hp2.Version 0&per.82:ehp2.&colon. 

:p.Changed a bit in the algoritm for making a short name for a file with a 
longname (again)&per. Now filenames starting with a dot are handled better&per. 
.br 
Changed the (internal) memory handling routines to use more then one selector 
so I could use this for FindFirst/Next handling instead of allocation one selector 
per FindFirst/Next call&per. This was neccessary because a DOS session can fire a 
lot of FindFirst calls only for checking existence of files&per. Before the 
modification FAT32&per.IFS could run out of memory after say a couple of hundred FindFirst 
calls&per. (I found that OS/2 itself doesn&apos.t allow a DOS session to fire more 
than around 500 FindFirsts&per. After that OS/2 starts reusing find handles&per.) I 
encountered this problem when trying to unarj the Novell Netware client 2&per.12 on my 
FAT32 disk&per. 
.br 
  
:p.:hp2.Version 0&per.81&colon. :ehp2.

:p.Noticed that when FS_FILEINFO was used to set the date/time on a file as 
only action the date/time was not updated&per. Corrected&per. 
.br 
In version 0&per.66 I removed the setting of the attribute using DosSetFileInfo
&per. As I found out, I was wrong&per. Now DosSetFileInfo from 32 bits programs sets 
the attribute&per. From 16 bits programs however, this doesn&apos.t seem to work on 
FAT, HPFS and FAT32&per. Don&apos.t know why&per. 
.br 
  
:p.:hp2.Version 0&per.80&colon. :ehp2.

:p.Rewrote the strategy2 routines so more than one request list can be fired 
per volume&per. Hope this improves performance&per. 
.br 
Corrected a problem with FS_COPY that failed if the target file was zero bytes 
large&per. 
.br 
Corrected a problem with DosOpen with the Truncate flag set and when the target 
file already existed and had EAs&per. The open failed, but the file was truncated 
and a lost cluster was created&per. 
.br 
Created PARTFILT&per.TXT file about PARTFILT and DISKINFO&per. 
.br 
Implemented an version check between the IFS and UFAT32&per.DLL (for CHKDSK) 
and CACHEF32&per.EXE&per. 
.br 
  
:p.:hp2.Version 0&per.79&colon. :ehp2.

:p.Rewrote the emergency flush routine so it will handle multiple FAT32 
partitions better&per. 
.br 
Made some minor modifications in UFAT32&per.DLL (For CHKDSK)&per. 
.br 
A trap was solved when an EA file could not be repaired 
.br 
An error in the longname retrieval routine was fixed 
.br 
Output is matched more closely to HPFS and plain FAT&per. 
.br 
  
:p.:hp2.Version 0&per.78&colon. :ehp2.

:p.Corrected a TRAP that could occur when the cache is full with dirty sectors 
and the oldest dirty single sector was flushed using a single flush routine&per. 
The trap message was &apos.WriteCacheSector&colon. VOLINFO not found!&apos.&per. 
This problem only occured when more than 1 FAT32 partition is present&per. 
.br 
Corrected a problem with UFAT32&per.DLL when checking directories containing 
more than 2048 files&per. (CHKDSK reported lost clusters, but was wrong) 
.br 
Corrected another problem with UFAT32&per.DLL that occured when there is not 
enough memory available to check large directories&per. UFAT32 would abend with an 
access violation&per. 
.br 
Corrected a problem with DosOpen when on Opening an existing file with only the 
FILE_CREATE (OPEN_ACTION_CREATE_IF_NEW) bit set (and not the FILE_OPEN flag) the 
file was created while the open should fail&per. 
.br 
Corrected a problem that the characterset translate tables would be overwritten 
internally leading to all kinds of funny results&per. ( Duplicate directories etc) 
.br 
Modified the default behaviour of FAT32&per.IFS and for the /T option for 
CACHEF32&colon. No character translation takes place&per. This works for situations 
where Windows and OS/2 use the same codepage&per. 
.br 
  
:p.:hp2.Version 0&per.77&colon. :ehp2.

:p.Corrected a problem when more than 512 lost chains were found&per. (CHKDSK 
can only recover 512 lost chains at one run) 
.br 
Corrected a problem in OS/2 sessions (introduced in version 0&per.75) where 
valid 8&per.3 filenames in lowercase where always stored in uppercase&per. 
.br 
  
:p.:hp2.Version 0&per.76&colon. :ehp2.

:p.Corrected a NASTY BUG that lead to loss of data when multiple files were 
opened&per. 
.br 
  
:p.:hp2.Version 0&per.75&colon. :ehp2.

:p.Added a check for valid EA names&per. 
.br 
Added a translation mechanism for long filenames between the Windows Character 
set and OS/2 character set&per. See&colon. WINDOWS &caret. OS/2 CHARACTER SETS for 
more information&per. 
.br 
  
:p.:hp2.Version 0&per.74&colon. :ehp2.

:p.Corrected a problem with DosFindFirst/next when the buffer wasn&apos.t large 
enough for the extended attributes and FAT32&per.IFS returned ERROR_EAS_DIDNT_FIT when 
more than one entry was placed in the resultbuffer&per. Now FAT32&per.IFS returns 
this error only if the EA&apos.s of the first matching entry don&apos.t fit in the 
buffer&per. (This error lead to the WPS giving an error that no matching entries were 
found on opening of a directory) 
.br 
  
:p.:hp2.Version 0&per.73&colon. :ehp2.

:p.Using DosSetPathInfo, it was possible to create an EA file for a non 
existing file&per. This lead f&per.i&per. to a &apos. EA&per. SF&apos. file in the root 
directory&per. This problem has been corrected&per. 
.br 
Changed a bit in the algoritm for making a short name for a file with a 
longname&per. 
.br 
  
:p.:hp2.Version 0&per.72&colon. :ehp2.

:p.Forgot build in the EA logic for creating and removing directories&per. Has 
been added&per. 
.br 
  
:p.:hp2.Version 0&per.71&colon. :ehp2.

:p.EA&apos.s were not found from DOS sessions&per. Now this is hardly a problem 
since DOS programs never access EA&apos.s, but EAUTIL can be used in DOS sessions, 
and didn&apos.t work&per. Now it does&per. 
.br 
There was another problems with finding EAs when FAT32&per.IFS was set to the 
mode in which internally short names were used (CACHEF32 /FS)&per. Now this seems to 
work properly&per. 
.br 
  
:p.:hp2.Version 0&per.70&colon. :ehp2.

:p.Most significant change is the implementation of EXTENDED ATTRIBUTES&per. 
Currently they will only be supported if /EAS is specified after FAT32&per.IFS in the 
config&per.sys&per. Please read the chapter about extended attributes&per. 
.br 
Corrected a small problem where a whenever a short name had to be created for a 
longer name containing any embedded blanks in the name FAT32&per.IFS left the blanks 
in, while Win95 skips them while creating the shortname&per. This lead to SCANDISK 
reporting incorrect long file names&per. (When correcting this problem the short name was 
modified by SCANDISK&per.) Now FAT32&per.IFS does the same as Windows 95&per. 
.br 
Received a report that FAT32&per.IFS failed allocating the cache space&per. 
Modified FAT32&per.IFS so it will no longer trap on such a situation, but will continue 
to run (without a cache - slow!) 
.br 
Received a report about a possible memory leakage problem in FAT32&per.IFS&per. 
Changed CACHEF32&per.EXE so when run, it will show the number of GDT selectors 
currently allocated for FAT32&per.IFS&per. 
.br 
  
:p.:hp2.Version 0&per.66&colon. :ehp2.

:p.DosSetFileInfo returned an error (ERROR_INVALID_LEVEL) when trying to write 
Extended attributes&per. Now FAT32&per.IFS reports NO_ERROR (But still doesn&apos.t 
write the EA!)&per. This makes f&per.i&per. that the installation of the OS/2 
Netscape pluginpack now works properly&per. 
.br 
Corrected a problem with FindFirst/Next where the check on required buffer 
space was incorrect&per. Some programs (Slick Edit) failed doing a directory scan
&per. This has been fixed&per. 
.br 
 Modified the behaviour of DosSetFileInfo so that it will only set date/time 
values in the directory&per. Before DosSetFileInfo also set the attribute, but I found 
that this also doesn&apos.t work on HPFS, so I modified the behaviour&per. 
.br 
Modified the default MONITOR logging so that (almost) all FS_XXXX calls are 
shown with the return values given&per. 
.br 
  
:p.:hp2.Version 0&per.65&colon. :ehp2.

:p.Files with valid 8&per.3 lowercase filenames where returned by findfirst/
next in DOS sessions in lowercase as well&per. Some programs don&apos.t like that
&per. Now findfirst/next in DOS sessions always returns an uppercase name&per. (This 
problem only occured when LFN&apos.s were hidden to DOS&per.) 
.br 
Corrected a problem where while filling the buffer for FindFirst/Next too much 
data was initialized (due to using strncpy) and data was overwritten&per. This was 
most appearant with OS/2 Commander that trapped on a FAT32 directory with many files
&per. 
.br 
  
:p.:hp2.Version 0&per.64&colon. :ehp2.

:p.Again a problem with CHKDSK, this time the file allocation check failed if 
there were more than 65535 clusters assigned to a file&per. 
.br 
FAT32&per.IFS now reports fake cluster sizes and total and free cluster counts 
whenever a DOS session queries free space&per. The maximum cluster size returned has 
been set to 32 Kb and the maximum for total and free clusters is 65526 clusters so 
the maximum disk size in dos is reported as almost 2Gb&per. 
.br 
Encountered (and fixed) a trap that occured whenever a volume was flushed via a 
explicit call and there were still dirty sectors in call&per. It occured in code I 
changed in version 0&per.60 and this was the first time I trapped on it, so the 
combination of factors appears unlikely&per. 
.br 
Modified FS_CHGFILEPTR so negative seeks will be handled properly and build in 
logic to not allow files to grow bigger then 2Gb&per. 
.br 
Uptil now I ignored the MUST_HAVE_XXX settings for directory scans since I 
assumed they were not used&per. Some users reported files beeing show twice in some 
application so&colon. I stand corrected and so is FAT32&per.IFS&per. 
.br 
Corrected a potential problem where (theoretically) files could be given a 
directory attribute&per. 
.br 
Changed the algorythm used when no large enough contiguous FAT chain is 
available and the FAT chain has to be constructed from various chains&per. Before the 
change an algoritme searching for individual free clusters was used&per. Now FAT32
&per.IFS searches the largest free chain assigns it and then searches for the next 
largest free chain until a chain long enough is created&per. This is still not very 
fast, but will only really occur if the disk is rather full and very fragmented&per. 
.br 
  
:p.:hp2.Version 0&per.63&colon. :ehp2.

:p.Finally understood why CHKDSK failed on very large disks&per. UFAT32&per.DLL 
accesses the disk using DosOpen with OPEN_FLAGS_DASD&per. In that mode default behaviour 
is that the disk is accessed using physical byte offsets from the beginning of the 
(logical) disk&per. Now since the maximum value in a 32 bit integer is 2&caret.32 
this value divided by 512 was the maximum sector that could be read (= sector 
8388608 = 4Gb disk size maximum)&per. Now UFAT32 uses the same trick as HPFS uses, via 
a call to DosFSCtl disk access is switched to sector mode so 2&caret.32 sectors 
can be accessed&per. This means CHKDSK can (theoretically) check disks upto 2048 
gigabytes&per. 
.br 
  
:p.:hp2.Version 0&per.62&colon. :ehp2.

:p.Corrected a bug that lead to a disk full message when a file was rewritten (
for instance with E&per.EXE) and the new size was just a couple of bytes more then 
to old size&per. Problem was result of the logic change in FS_NEWSIZE&per. 
.br 
Oops&colon. Forgot to update the version number in version 0&per.61&per. 
.br 
  
:p.:hp2.Version 0&per.61&colon. :ehp2.

:p.Changed some logic in the dirty buffer flush mechanism&per. 
.br 
Changed some logic in the FS_WRITE and FS_NEWSIZE functions&per. 
.br 
  
:p.:hp2.Version 0&per.52 :ehp2.

:p.Didn&apos.t handle closes from child processes that inherited open files 
properly so the final close would fail&per. 
.br 

:p.:hp2.Version 0&per.60&colon. :ehp2.
.br 
- Changed the algoritm to detect EOF in the FATs since MS appearantly uses 
.br 
  other values than 0x0FFFFFF8 as EOF token&per. 
.br 
- Changed the flush dirty buffer mechanism to use strategy2 device calls&per. 
.br 
  This has resulted in an increase of performance during write to disk&per. 
.br 
- Changed CHKDSK to accept an /V&colon.1 argument to only show fragmented files, 
.br 
  while /V[&colon.2] also lists all files&per. 
.br 
- Made it possible that renaming a file to a new name where only the 
.br 
  case was changed worked&per.   

:p.:hp2.Version 0&per.51 :ehp2.

:p.Did a lot of work on Lazy write performance&per. Cache access is no longer 
protected with a semaphore but with a per sector inuse flag&per. 
.br 
Fixed a problem that caused INSTALL and MINSTALL to abort when FAT32&per.IFS 
was loaded&per. The problem had to do with argument checking with FS_IOCTL calls
&per. 
.br 
Fixed a problem that BRIEF, a populair editor under OS/2, trapped or hung tself
&per. The problem had to do with returning improperly formatted nformation when 
querying EAs (FAT32&per.IFS does not support EAs!) 
.br 
Corrected a serious problem when a single file was opened more than once and 
the file was modified using one of the instances&per. The other instance(s) didn
&apos.t pick up the changes and FAT32&per.IFS might trap&per. 
.br 
  
:p.:hp2.Version 0&per.50 :ehp2.

:p.Fixed a problem where files with longnames could sometimes not be found 
which lead to duplicate filenames&per. 
.br 
Fixed a problem where SCANDISK would claim that a directory created by FAT32
&per.IFS contained invalid blocks&per. 
.br 
Fixed a (BIG) problem with files or directories with long names where if such a 
file was opened in a DOS session and in an OS/2 session simultaniously OS/2 was 
unable to see that the same file was opened&per. 
.br 
Fixed a problem where read-only executables could not be run&per. 
.br 
 Fixed a problem where the algoritm used to determine the highest available 
cluster number was incorrect&per. 
.br 
Fixed a problem were CHKDSK was unable to fix cross-linked files&per. 
.br 
Since some people complained that FAT32 would sometimes hang, I have modified 
the internal semaphore mechanism so an error message will appear if a semaphore 
remains blocked for more than a minute&per. 
.br 
  
:p.:hp2.Version 0&per.41 :ehp2.

:p.Fixed a problem with numeric tails shortnames&per. Files always got ~1 
instead if an incrementing number&per. Has been fixed&per. 
.br 
  
:p.:hp2.Version 0&per.40 :ehp2.

:p.The volume label was retrieved from the boot sector&per. However Win95 
actually stores the Volume label in the root directory&per. The Volume label now is 
taken from the root directory&per. Also, the label can be set now&per. (The boot 
sector is however still updated) 
.br 
/Q switch of PARTFILT didn&apos.t work&per. Now it does&per. 
.br 
A problem was in CHGFILEPOINTER that could (theoretically) lead to an trap (
FAT32&colon. FS_WRITE&colon. No next cluster available&osq.)&per. 
.br 
Corrected a logical error where renaming a file or directory to an existing 
directory caused the file or directory to be moved into the target directory&per. Now 
FAT32&per.IFS returns an error&per. 
.br 
Changed CHKDSK so that if an error is found in one of the FATs CHKDSK continues
, but ignores the /F switch&per. Previously, CHKDSK would not do any additional 
checks&per. 
.br 
Renaming a file or directory from the workplace shell didn&apos.t work because 
of two problems&colon. 
.br 
 The WPS uses a strange algoritm to determine if the IFS supported long file 
names which appearantly failed with FAT32&per. This has been corrected&per. 
.br 
FAT32&per.IFS does not support EA&apos.s (yet), the WPS renames a file, tries 
to write EAs and since that fails renames the file back again&per. Now FAT32 
returns NO_ERROR on the call used to write EAs&per. 
.br 
CHKDSK now is able to fix cross-linked clusters on the disk&per. 
.br 
  
:p.:hp2.Version 0&per.30 :ehp2.

:p.Added PARTFILT&per.FLT to the archive&per. 
.br 
Added support for ReadOnly partitions&per. This is needed for PARTFILT&per. 
.br 
IOCTL Calls (category 8) are now passed throught to OS2DASD&per. 
.br 
  
:p.:hp2.Version 0&per.20 :ehp2.

:p.Cache routines have been improved for performance&per. Removing &apos.old
&apos. sectors from the cache is no longer needed&per. /T option for CACHEF32 has been 
removed&per. 
.br 
CHKDSK&colon. Is now able to fix incorrect free space count&per. 
.br 
CHKDSK&colon. Lost cluster fix algoritm has been improved for performance&per. 
.br 
CHKDSK&colon. Didn&apos.t recoqnize bad-sectors, has been fixed&per. 
.br 
CHKDSK&colon. Had problems with recoqnition of some type of free clusters, has 
been fixed&per. 
.br 
OS2DASD&per.DMD&colon. Is now based on the latest version&per. (December &apos.
97) 
.br 
  
:p.:hp2.Version 0&per.10 :ehp2. - Initial Version   

:p.
:h1 id=72 res=30066.Latest Files

:p.:hp2.LATEST FILES&colon. :ehp2.

:p.:hp2.Latest Compiled Version&colon. :ehp2.  

:p.FAT32 IFS&colon. :hp9.ftp&colon.//ftp&per.netlabs&per.org/pub/FAT32 :ehp9.

:p.FAT32&per.IFS Netlabs project page&colon. http&colon.//trac&per.netlabs&per.org/fat32/

:p.Builds archive&colon. ftp&colon.//osfree&per.org/upload/fat32/

:p.:hp2.Contact&colon. :ehp2.  

:p.Adrian Gschwend at :hp9.ktk&atsign.netlabs&per.org :ehp9.if you have some FAT32 patches, 
or you can join the #netlabs channel at irc&per.freenode&per.net (FreeNode IRC)&per. My nick 
is ktk, Brian Smith is nuke&per. 

:p.KO Myung-Hun at :hp9.komh&atsign.chollian&per.net :ehp9.concerning issues dealing the 
FAT32 code 

:p.Alfredo Fern�ndez D�az at :hp9.alfredo&atsign.netropolis-si&per.com :ehp9.concerning 
issues dealing with the WarpIN script and FAT32 code 

:p.David Graser at :hp9.dwgras&atsign.swbell&per.net :ehp9.concerning issues with the FAT32
&per.INF file 

:p.Valery V. Sedletski at :hp9._valerius&atsign.mail&per.ru :ehp9.concerning issues with the FAT32
&per.IFS version 0&per.10&per.

:p.
:h1 id=73 res=30068.Building FAT32 from Sources (Developers Only)

:p.:hp2.BUILDING FAT32 FROM SOURCES (DEVELOPERS ONLY)&colon. :ehp2.

:p.:link reftype=hd refid=74.REQUIREMENTS :elink.

:p.:link reftype=hd refid=75.SOURCE CODE :elink.

:p.:link reftype=hd refid=76.COMPILING :elink.
.br 

:h2 id=74 res=30070.Requirements

:p.:hp2.REQUIREMENTS&colon. :ehp2.

:p.-the IFS DDK Build Environment 

:p.-IBM VAC 3 

:p.-OS/2 Toolkit 4&per.5 (included with OS/2 ACP/MCP and eCS) 

:p.-IBM DDK sources and tools (freely available at IBM&apos.s DDK site) 

:p.-IBM C 

:p.-the System utilities (including link&per.exe) 
.br 

:p.:hp2.REQUIREMENTS for version 0&per.10&colon. :ehp2.

:p.-OpenWatcom (version 1.9 is ok)

:p.-OS/2 Toolkit 4.5 (included with OS/2 ACP/MCP and eCS).

:p.-Awk interpreter for converting Watcom map files to VAC map files

:p.-FAT32 source code. 
.br

:p.:hp2.Note&colon.  :ehp2.Although IBM DDK souces and tools are free, you will have to 
register at the IBM DDK site if you have not already done so&per. 

:p.:link reftype=hd refid=75.SOURCE CODE :elink.

:p.:link reftype=hd refid=76.COMPILING :elink.
.br 

:h2 id=75 res=30071.Source Code

:p.:hp2.SOURCE CODE&colon. :ehp2.  

:p.:hp9.http&colon.//svn&per.netlabs&per.org/fat32/ :ehp9.  

:p.The sourcecode is released under the :link reftype=hd refid=77.GNU LGPL&per. :elink.

:p.:link reftype=hd refid=74.REQUIREMENTS :elink.

:p.:link reftype=hd refid=76.COMPILING :elink.
.br 

:h2 id=76 res=30072.Compiling

:p.:hp2.COMPILING&colon. :ehp2.

:p.To get a build going, you&apos.ll need several things&colon. 

:p.You should have VisualAge installed and it&apos.s environment variables set
&per.  Then set these 3 variables&colon. 

:p.SET DDK=d&colon.&bsl.ddk 
.br 
SET DDKTOOLS=d&colon.&bsl.ddktools 
.br 
SET IBMC=d&colon.&bsl.ibmc 
.br 

:p.Correct the paths as neccessary&per. 

:p.Then run the make&per.cmd script, and if all goes well you should have a 
fresh build of FAT32&per.IFS&per. 

:p.:hp2.Note&colon.  :ehp2.There are many warnings when compiling FAT32&per. Most of them are 
&apos.optimizing&apos. warnings or &apos.unreference variable&apos. warnings&per. 
Essentially, these warnings mean that certain functions cannot be optimized and that some 
vars/const are never referenced&per. The resulting binaries work however&per. 

:p.:hp2.COMPILING version 0&per.10&colon. :ehp2.

:p.To get a build going, you'll need several things;

:p.You should have Watcom C installed and it&apos.s environment variables set&per. You&apos.ll need to take the buildw&per.cmd
file in the sources root directory and modify the following environment variables&per.

:p.set WATCOM=f&colon.\dev\watcom
.br
set TOOLKIT=f&colon.\os2tk45
.br

:p.Correct the paths as necessary&per. Buildw&per.cmd also calls %watcom%\owsetenv&per.cmd&per. This file must exist
and contain the usual Watcom environment, like this&colon.

:p.SET PATH=f&colon.\dev\watcom\BINW;%PATH%
.br
SET PATH=f&colon.\dev\watcom\BINP;%PATH%
.br
SET BEGINLIBPATH=f&colon.\dev\watcom\binp\dll
.br
SET INCLUDE=f&colon.\dev\watcom\H\OS2;%INCLUDE%
.br
SET INCLUDE=f&colon.\dev\watcom\H;%INCLUDE%
.br
SET WATCOM=f&colon.\dev\watcom
.br
SET EDPATH=f&colon.\dev\watcom\EDDAT
.br
SET WIPFC=f&colon.\dev\watcom\WIPFC
.br

:p.Then run the _wcc&per.cmd script, and if all goes well you should have a fresh build of FAT32&per.IFS&per.
There are also buildi&per.cmd for ICC and buildm&per.cmd for MSC in the root directory&per. They are called by
_icc&per.cmd or _msc&per.cmd in each subdir&per. So, to build sources in each subdir, you need just tap Enter
on _wcc&per.cmd/_msc&per.cmd/_icc&per.cmd&per.

:p.Also note that for creating *&per.sym files for fat32 binaries, you&apos.ll need the awk interpreter (awk&per.exe)&per.
You can use GNU awk (GAWK) for that&per. To install it, use

:p.:hp2.yum install gawk :ehp2.

:p.to install it from yum/rpm repository&per. The binary is called gawk&per.exe, so you may need to create a symlink
gawk&per.exe -> awk&per.exe&per. For creating the *&per.sym file, the Watcom map file (*&per.wmp) is taken&per.
Then it gets converted to a VAC map file (*&per.map) by mapsym&per.awk AWK script&per. And finally, mapsym&per.exe
from OS/2 Toolkit is used to convert *&per.map to *&per.sym&per.

:p.:hp2.Note:ehp2. that all you need for compiling the sources is Watcom installation&per. DDK is not needed as all needed
headers are present in the sources&per. The only external things needed are awk&per.exe from yum repo and mapsym&per.exe
from OS/2 Toolkit&per. 

:p.:link reftype=hd refid=74.REQUIREMENTS :elink.

:p.:link reftype=hd refid=75.SOURCE CODE :elink.
.br 

:h1 id=77 res=30073.GNU Lesser General Public License

:lines align=center.
:hp2.:link reftype=fn refid=fn82.GNU LESSER GENERAL PUBLIC LICENSE :elink.:ehp2.
.br 

.br 

.br 
Version 2&per.1, February 1999 
.br 

Copyright (C) 1991, 1999 Free Software Foundation,
.br 
Inc&per.
.br 

.br 
59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
.br 

.br 

:elines.
Everyone is permitted to copy and distribute verbatim copies of this license 
document, but changing it is not allowed&per. 

:p.[This is the first released version of the Lesser GPL&per.  It also counts as 
the successor of the GNU Library Public License, version 2, hence the version 
number 2&per.1&per.] 

:p.
:lines align=center.
:hp2.Preamble:ehp2.
.br 

:elines.

:p.The licenses for most software are designed to take away your freedom to 
share and change it&per.  By contrast, the GNU General Public Licenses are intended to 
guarantee your freedom to share and change free software--to make sure the software is 
free for all its users&per. 

:p.This license, the Lesser General Public License, applies to some specially 
designated software packages--typically libraries--of the Free Software Foundation and 
other authors who decide to use it&per.  You can use it too, but we suggest you first 
think carefully about whether this license or the ordinary General Public License is 
the better strategy to use in any particular case, based on the explanations below
&per. 

:p.When we speak of free software, we are referring to freedom of use, not price
&per.  Our General Public Licenses are designed to make sure that you have the freedom 
to distribute copies of free software (and charge for this service if you wish); 
that you receive source code or can get it if you want it; that you  can change the 
software and use pieces of it in new free programs; and that you are informed that you 
can do these things&per. 

:p.To protect your rights, we need to make restrictions that forbid distributors 
to deny you these rights or to ask you to surrender these rights&per.  These 
restrictions translate to certain responsibilities for you if you distribute copies of the 
library or if you modify it&per. 

:p.For example, if you distribute copies of the library, whether gratis or for a 
fee, you must give the recipients all the rights that we gave you&per.  You must 
make sure that they, too, receive or can get the source code&per.  If you link other 
code with the library, you must provide complete object files to the recipients, so 
that they can relink them with the library after making changes to the library and 
recompiling it&per.  And you must show them these terms so they know their rights&per. 

:p.We protect your rights with a two-step method&colon. (1) we copyright the 
library, and (2) we offer you this license, which gives you legal permission to copy, 
distribute and/or modify the library&per. 

:p.To protect each distributor, we want to make it very clear that there is no 
warranty for the free library&per.  Also, if the library is modified by someone else and 
passed on, the recipients should know that what they have is not the original version, 
so that the original author&apos.s reputation will not be affected by problems 
that might be introduced by others&per. 
.br 
  
.br 
Finally, software patents pose a constant threat to the existence of any free 
program&per.  We wish to make sure that a company cannot effectively restrict the users 
of a free program by obtaining a restrictive license from a patent holder&per.  
Therefore, we insist that any patent license obtained for a version of the library must 
be consistent with the full freedom of use specified in this license&per. 

:p.Most GNU software, including some libraries, is covered by the ordinary GNU 
General Public License&per.  This license, the GNU Lesser General Public License, 
applies to certain designated libraries, and is quite different from the ordinary 
General Public License&per.  We use this license for certain libraries in order to 
permit linking those libraries into non-free programs&per. 

:p.When a program is linked with a library, whether statically or using a shared 
library, the combination of the two is legally speaking a combined work, a derivative 
of the original library&per.  The ordinary General Public License therefore permits 
such linking only if the entire combination fits its criteria of freedom&per.  The 
Lesser General Public License permits more lax criteria for linking other code with 
the library&per. 

:p.We call this license the &osq.Lesser&osq. General Public License because it 
does Less to protect the user&apos.s freedom than the ordinary General Public 
License&per.  It also provides other free software developers Less of an advantage over 
competing non-free programs&per.  These disadvantages are the reason we use the ordinary 
General Public License for many libraries&per. However, the Lesser license provides 
advantages in certain special circumstances&per. 

:p.For example, on rare occasions, there may be a special need to encourage the 
widest possible use of a certain library, so that it becomes a de-facto standard&per.  
To achieve this, non-free programs must be allowed to use the library&per.  A more 
frequent case is that a free library does the same job as widely used non-free libraries
&per.  In this case, there is little to gain by limiting the free library to free 
software only, so we use the Lesser General Public License&per. 

:p.In other cases, permission to use a particular library in non-free programs 
enables a greater number of people to use a large body of free software&per.  For 
example, permission to use the GNU C Library in non-free programs enables many more 
people to use the whole GNU operating system, as well as its variant, the GNU/Linux 
operating system&per. 

:p.Although the Lesser General Public License is Less protective of the users
&apos. freedom, it does ensure that the user of a program that is linked with the 
Library has the freedom and the wherewithal to run that program using a modified 
version of the Library&per. 

:p.The precise terms and conditions for copying, distribution and modification 
follow&per.  Pay close attention to the difference between a &osq.work based on the 
library&osq. and a &osq.work that uses the library&osq.&per.  The former contains code 
derived from the library, whereas the latter must be combined with the library in order 
to run&per. 

:p.
:lines align=center.
:hp2.GNU LESSER GENERAL PUBLIC LICENSE :ehp2.
.br 

.br 
:hp2.TERMS AND CONDITIONS FOR COPYING, DISTRIBUTION AND MODIFICATION :ehp2.
.br 

:elines.

:p.    0&per.  This License Agreement applies to any software library or other 
program which contains a notice placed by the copyright holder or other authorized 
party saying it may be distributed under the terms of this Lesser General Public 
License (also called &osq.this License&osq.)&per. Each licensee is addressed as &osq.
you&osq.&per. 

:p.A &osq.library&osq. means a collection of software functions and/or data 
prepared so as to be conveniently linked with application programs (which use some of 
those functions and data) to form executables&per. 

:p.The &osq.Library&osq., below, refers to any such software library or work 
which has been distributed under these terms&per.  A &osq.work based on the Library
&osq. means either the Library or any derivative work under copyright law&colon. that 
is to say, a work containing the Library or a portion of it, either verbatim or 
with modifications and/or translated straightforwardly into another language&per.  (
Hereinafter, translation is included without limitation in the term &osq.modification&osq.
&per.) 

:p.&osq.Source code&osq. for a work means the preferred form of the work for 
making modifications to it&per.  For a library, complete source code means all the 
source code for all modules it contains, plus any associated interface definition 
files, plus the scripts used to control compilation and installation of the library
&per.   

:p.Activities other than copying, distribution and modification are not covered 
by this License; they are outside its scope&per.  The act of running a program 
using the Library is not restricted, and output from such a program is covered only 
if its contents constitute a work based on the Library (independent of the use of 
the Library in a tool for writing it)&per.  Whether that is true depends on what the 
Library does and what the program that uses the Library does&per. 
.br 

:p. 1&per.You may copy and distribute verbatim copies of the Library&apos.s 
complete source code as you receive it, in any medium, provided that you conspicuously 
and appropriately publish on each copy an appropriate copyright notice and 
disclaimer of warranty; keep intact all the notices that refer to this License and to the 
absence of any warranty; and distribute a copy of this License along with the Library
&per. 

:p.You may charge a fee for the physical act of transferring a copy, and you may 
at your option offer warranty protection in exchange for a fee&per. 
:p. 2&per.You may modify your copy or copies of the Library or any portion of it, 
thus forming a work based on the Library, and copy and distribute such modifications 
or work under the terms of Section 1 above, provided that you also meet all of 
these conditions&colon.   

:p. a&per.The modified work must itself be a software library&per. 

:p. b&per.You must cause the files modified to carry prominent notices stating 
that you changed the files and the date of any change&per. 

:p. c&per.You must cause the whole of the work to be licensed at no charge to all 
third parties under the terms of this License&per. 

:p. d&per.If a facility in the modified Library refers to a function or a table 
of data to be supplied by an application program that uses the facility, other 
than as an argument passed when the facility is invoked, then you must make a good 
faith effort to ensure that, in the event an application does not supply such 
function or table, the facility still operates, and performs whatever part of its 
purpose remains meaningful&per. 

:p.(For example, a function in a library to compute square roots has a purpose 
that is entirely well-defined independent of the application&per.  Therefore, 
Subsection 2d requires that any application-supplied function or table used by this 
function must be optional&colon. if the application does not supply it, the square root 
function must still compute square roots&per.) 

:p.These requirements apply to the modified work as a whole&per.  If identifiable 
sections of that work are not derived from the Library, and can be reasonably considered 
independent and separate works in themselves, then this License, and its terms, do not 
apply to those sections when you distribute them as separate works&per.  But when you 
distribute the same sections as part of a whole which is a work based on the Library, the 
distribution of the whole must be on the terms of this License, whose permissions for other 
licensees extend to the entire whole, and thus to each and every part regardless of who 
wrote it&per. 

:p.Thus, it is not the intent of this section to claim rights or contest your 
rights to work written entirely by you; rather, the intent is to exercise the right to 
control the distribution of derivative or collective works based on the Library&per. 

:p.In addition, mere aggregation of another work not based on the Library with 
the Library (or with a work based on the Library) on a volume of a storage or 
distribution medium does not bring the other work under the scope of this License&per. 

:p. 3&per.You may opt to apply the terms of the ordinary GNU General Public 
License instead of this License to a given copy of the Library&per.  To do this, you 
must alter all the notices that refer to this License, so that they refer to the 
ordinary GNU General Public License, version 2, instead of to this License&per.  (If a 
newer version than version 2 of the ordinary GNU General Public License has appeared, 
then you can specify that version instead if you wish&per.)  Do not make any other 
change in these notices&per. 

:p.Once this change is made in a given copy, it is irreversible for that copy, 
so the ordinary GNU General Public License applies to all subsequent copies and 
derivative works made from that copy&per. 

:p.This option is useful when you wish to copy part of the code of the Library 
into a program that is not a library&per. 

:p. 4&per.You may copy and distribute the Library (or a portion or derivative of 
it, under Section 2) in object code or executable form under the terms of Sections 
1 and 2 above provided that you accompany it with the complete corresponding 
machine-readable source code, which must be distributed under the terms of Sections 1 
and 2 above on a medium customarily used for software interchange&per. 

:p.If distribution of object code is made by offering access to copy from a 
designated place, then offering equivalent access to copy the source code from the same 
place satisfies the requirement to distribute the source code, even though third 
parties are not compelled to copy the source along with the object code&per. 

:p. 5&per.A program that contains no derivative of any portion of the Library, 
but is designed to work with the Library by being compiled or linked with it, is 
called a &osq.work that uses the Library&osq.&per.  Such a work, in isolation, is not a 
derivative work of the Library, and therefore falls outside the scope of this License&per. 

:p.However, linking a &osq.work that uses the Library&osq. with the Library 
creates an executable that is a derivative of the Library (because it contains portions 
of the Library), rather than a &osq.work that uses the library&osq.&per.  The 
executable is therefore covered by this License&per. Section 6 states terms for 
distribution of such executables&per. 

:p.When a &osq.work that uses the Library&osq. uses material from a header file 
that is part of the Library, the object code for the work may be a derivative work 
of the Library even though the source code is not&per. Whether this is true is 
especially significant if the work can be linked without the Library, or if the work is 
itself a library&per.  The threshold for this to be true is not precisely defined by 
law&per. 

:p.If such an object file uses only numerical parameters, data structure layouts 
and accessors, and small macros and small inline functions (ten lines or less in 
length), then the use of the object file is unrestricted, regardless of whether it is 
legally a derivative work&per.  (Executables containing this object code plus portions 
of the Library will still fall under Section 6&per.) 

:p.Otherwise, if the work is a derivative of the Library, you may distribute the 
object code for the work under the terms of Section 6&per. Any executables containing 
that work also fall under Section 6, whether or not they are linked directly with 
the Library itself&per.   

:p. 6&per.As an exception to the Sections above, you may also combine or link a 
&osq.work that uses the Library&osq. with the Library to produce a work containing 
portions of the Library, and distribute that work under terms of your choice, provided 
that the terms permit modification of the work for the customer&apos.s own use and 
reverse engineering for debugging such modifications&per. 

:p.You must give prominent notice with each copy of the work that the Library is 
used in it and that the Library and its use are covered by this License&per.  You 
must supply a copy of this License&per.  If the work during execution displays 
copyright notices, you must include the copyright notice for the Library among them, as 
well as a reference directing the user to the copy of this License&per.  Also, you 
must do one of these things&colon. 

:p. a&per.Accompany the work with the complete corresponding machine-readable 
source code for the Library including whatever changes were used in the work (which 
must be distributed under Sections 1 and 2 above); and, if the work is an executable 
linked with the Library, with the complete machine-readable &osq.work that uses the 
Library&osq., as object code and/or source code, so that the user can modify the 
Library and then relink to produce a modified  executable containing the modified 
Library&per.  (It is understood that the user who changes the contents of definitions 
files in the Library will not necessarily be able to recompile the application to use 
the modified definitions&per.) 

:p. b&per.Use a suitable shared library mechanism for linking with the Library
&per.  A suitable mechanism is one that (1) uses at run time a copy of the library 
already present on the user&apos.s computer system, rather than copying library 
functions into the executable, and (2) will operate properly with a modified version of 
the library, if the user installs one, as long as the modified version is interface
-compatible with the version that the work was made with&per. 

:p. c&per.Accompany the work with a written offer, valid for at least three years
, to give the same user the materials specified in Subsection 6a, above, for a 
charge no more than the cost of performing this distribution&per. 

:p. d&per.If distribution of the work is made by offering access to copy from a 
designated place, offer equivalent access to copy the above specified materials from the 
same place&per. 

:p. e&per.Verify that the user has already received a copy of these materials or 
that you have already sent this user a copy&per. 

:p.For an executable, the required form of the &osq.work that uses the Library
&osq. must include any data and utility programs needed for reproducing the 
executable from it&per.  However, as a special exception, the materials to be distributed 
need not include anything that is normally distributed (in either source or binary 
form) with the major 
.br 
components (compiler, kernel, and so on) of the operating system on which the 
executable runs, unless that component itself accompanies the executable&per. 

:p.It may happen that this requirement contradicts the license restrictions of 
other proprietary libraries that do not normally accompany the operating system&per.  
Such a contradiction means you cannot use both them and the Library together in an 
executable that you distribute&per. 

:p. 7&per.You may place library facilities that are a work based on the Library 
side-by-side in a single library together with other library facilities not covered 
by this License, and distribute such a combined library, provided that the 
separate distribution of the work based on the Library and of the other library 
facilities is otherwise permitted, and provided that you do these two things&colon. 

:p. a&per.Accompany the combined library with a copy of the same work based on 
the Library, uncombined with any other library facilities&per.  This must be 
distributed under the terms of the Sections above&per. 

:p. b&per.Give prominent notice with the combined library of the fact that part 
of it is a work based on the Library, and explaining where to find the 
accompanying uncombined form of the same work&per. 

:p. 8&per.You may not copy, modify, sublicense, link with, or distribute the 
Library except as expressly provided under this License&per.  Any attempt otherwise to 
copy, modify, sublicense, link with, or distribute the Library is void, and will 
automatically terminate your rights under this License&per.  However, parties who have 
received copies, or rights, from you under this License will not have their licenses 
terminated so long as such parties remain in full compliance&per. 

:p. 9&per.You are not required to accept this License, since you have not signed 
it&per.  However, nothing else grants you permission to modify or distribute the 
Library or its derivative works&per.  These actions are prohibited by law if you do not 
accept this License&per.  Therefore, by modifying or distributing the Library (or any 
work based on the Library), you indicate your acceptance of this License to do so, 
and all its terms and conditions for copying, distributing or modifying the Library 
or works based on it&per. 

:p.10&per.Each time you redistribute the Library (or any work based on the 
Library), the recipient automatically receives a license from the original licensor to 
copy, distribute, link with or modify the Library subject to these terms and 
conditions&per.  You may not impose any further restrictions on the recipients&apos. 
exercise of the rights granted herein&per. You are not responsible for enforcing 
compliance by third parties with this License&per. 

:p.11&per.If, as a consequence of a court judgment or allegation of patent 
infringement or for any other reason (not limited to patent issues), conditions are imposed 
on you (whether by court order, agreement or otherwise) that contradict the 
conditions of this License, they do not excuse you from the conditions of this License
&per.  If you cannot distribute so as to satisfy simultaneously your obligations under 
this License and any other pertinent obligations, then as a consequence you may not 
distribute the Library at all&per.  For example, if a patent license would not permit 
royalty-free redistribution of the Library by all those who receive copies directly or 
indirectly through you, then the only way you could satisfy both it and this License would 
be to refrain entirely from distribution of the Library&per. 

:p.If any portion of this section is held invalid or unenforceable under any 
particular circumstance, the balance of the section is intended to apply, and the section 
as a whole is intended to apply in other circumstances&per. 

:p.It is not the purpose of this section to induce you to infringe any patents 
or other property right claims or to contest validity of any such claims; this 
section has the sole purpose of protecting the integrity of the free software 
distribution system which is implemented by public license practices&per.  Many people have 
made generous contributions to the wide range of software distributed through that 
system in reliance on consistent application of that system; it is up to the author/
donor to decide if he or she is willing to distribute software through any other 
system and a licensee cannot impose that choice&per. 

:p.This section is intended  to make thoroughly clear what is believed to be a 
consequence of the rest of this License&per. 

:p.12&per.If the distribution and/or use of the Library is restricted in certain 
countries either by patents or by copyrighted interfaces, the original copyright holder 
who places the Library under this License may add an explicit geographical 
distribution limitation excluding those countries, so that distribution is permitted only in 
or among countries not thus excluded&per.  In such case, this License incorporates 
the limitation as if written in the body of this License&per. 

:p.13&per.The Free Software Foundation may publish revised and/or new versions 
of the Lesser General Public License from time to time&per. Such new versions will 
be similar in spirit to the present version, but may differ in detail to address 
new problems or concerns&per. 

:p.Each version is given a distinguishing version number&per.  If the Library 
specifies a version number of this License which applies to it and &osq.any later version
&osq., you have the option of following the terms and conditions either of that 
version or of any later version published by the Free Software Foundation&per.  If the 
Library does not specify a license version number, you may choose any version ever 
published by the Free Software Foundation&per. 

:p.14&per.If you wish to incorporate parts of the Library into other free 
programs whose distribution conditions are incompatible with these, write to the author 
to ask for permission&per.  For software which is copyrighted by the Free Software 
Foundation, write to the Free Software Foundation; we sometimes make exceptions for this
&per.  Our decision  will be guided by the two goals of preserving the free status of 
all derivatives of our free software and of promoting the sharing and reuse of 
software generally&per. 

:p.
:lines align=center.
:hp2.NO WARRANTY :ehp2.
:elines.

:p.15&per.BECAUSE THE LIBRARY IS LICENSED FREE OF CHARGE, THERE IS NO WARRANTY 
FOR THE LIBRARY, TO THE EXTENT PERMITTED BY APPLICABLE LAW&per. EXCEPT WHEN 
OTHERWISE STATED IN WRITING THE COPYRIGHT HOLDERS AND/OR OTHER PARTIES PROVIDE THE 
LIBRARY &osq.AS IS&osq. WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, 
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
A PARTICULAR PURPOSE&per.  THE ENTIRE RISK AS TO THE QUALITY AND PERFORMANCE OF 
THE LIBRARY IS WITH YOU&per.  SHOULD THE LIBRARY PROVE DEFECTIVE, YOU ASSUME THE 
COST OF ALL NECESSARY SERVICING, REPAIR OR CORRECTION&per. 

:p.16&per.IN NO EVENT UNLESS REQUIRED BY APPLICABLE LAW OR AGREED TO IN WRITING 
WILL ANY COPYRIGHT HOLDER, OR ANY OTHER PARTY WHO MAY MODIFY AND/OR REDISTRIBUTE THE 
LIBRARY AS PERMITTED ABOVE, BE LIABLE TO YOU FOR DAMAGES, INCLUDING ANY GENERAL, 
SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OR INABILITY TO USE 
THE LIBRARY (INCLUDING BUT NOT LIMITED TO LOSS OF DATA OR DATA BEING RENDERED 
INACCURATE OR LOSSES SUSTAINED BY YOU OR THIRD PARTIES OR A FAILURE OF THE LIBRARY TO 
OPERATE WITH ANY OTHER SOFTWARE), EVEN IF SUCH HOLDER OR OTHER PARTY HAS BEEN ADVISED 
OF THE POSSIBILITY OF SUCH DAMAGES&per. 

:p.
:lines align=center.
:hp2.END OF TERMS AND CONDITIONS 
:elines.
:ehp2.
.br 

:p.
:lines align=center.
:hp2.How to Apply These Terms to Your New Libraries :ehp2.
:elines.

:p.If you develop a new library, and you want it to be of the greatest possible 
use to the public, we recommend making it free software that everyone can 
redistribute and change&per.  You can do so by permitting redistribution under these terms (
or, alternatively, under the terms of the ordinary General Public License)&per. 

:p.To apply these terms, attach the following notices to the library&per.  It is 
safest to attach them to the start of each source file to most effectively convey the 
exclusion of warranty; and each file should have at least the &osq.copyright&osq. line 
and a pointer to where the full notice is found&per. 

:p. <one line to give the library&apos.s name and a brief idea of what it does
&per.> 
.br 

:p.Copyright (C) <year>  <name of author> 

:p.This library is free software; you can redistribute it and/or modify it under 
the terms of the GNU Lesser General Public License as published by the Free 
Software Foundation; either version 2&per.1 of the License, or (at your option) any 
later version&per. 

:p.This library is distributed in the hope that it will be useful, but WITHOUT 
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A 
PARTICULAR PURPOSE&per.  See the GNU Lesser General Public License for more details&per. 

:p.You should have received a copy of the GNU Lesser General Public License 
along with this library; if not, write to the Free Software Foundation, Inc&per., 59 
Temple Place, Suite 330, Boston, MA  02111-1307  USA 
.br 

:p.Also add information on how to contact you by electronic and paper mail&per. 

:p.You should also get your employer (if you work as a programmer) or your 
school, if any, to sign a &osq.copyright disclaimer&osq. for the library, if necessary
&per.  Here is a sample; alter the names&colon. 

:p.Yoyodyne, Inc&per., hereby disclaims all copyright interest in the library `
Frob&apos. (a library for tweaking knobs) written by James Random Hacker&per. 

:p.<signature of Ty Coon>, 1 April 1990 
.br 
Ty Coon, President of Vice 

:p.That&apos.s all there is to it! 
.br 

:h1 id=78 res=30079.Third party code licenses

:p.:hp2. Third party code licenses&colon. :ehp2.

:p. There are some code we borrowed from other projects,
which have their respective licenses (GPL-compatible)&colon.

:p.
:ul.

:li. :link reftype=hd refid=30048.FORMAT :elink. is based on Fat32Format, (c) Tom Thornhill,
licensed as GPL (no strict version),
http&colon.//www&per.ridgecrop&per.demon&per.co&per.uk/index&per.htm?fat32format&per.htm;

:li. :link reftype=hd refid=20055.QEMUIMG&per.DLL :elink. is based on QEMU code, (c) Fabrice Bellard and 
contributors, licensed as GPL, version 2 (not higher),
http&colon.//www&per.qemu&per.org

:li. :link reftype=hd refid=18.PARTFILT&per.FLT:elink. is (c) Deon van der Westhuysen, 1995; 
licensed as GPL, version 2, or any later version (at your option)

:li. zlib is (c) Mark Adler, Jean-loup Gailly, licensed as a 3-clause BSD license (see zlib.h)

:eul.

:p.FAT32.IFS itself is licensed as LGPL&per.

:fn id=fn78.

:p.:hp2.CACHEF32&per.EXE Command&colon.  /D&colon. Parameter :ehp2.

:p.Sets the amount of time (in milliseconds) that a disk must be idle before it 
can accept data from cache memory&per. The minimum amount of disk idle time must be 
greater than the value specified in the BUFFERIDLE parameter&per. 

:p.To set the amount of disk idle time to 2000 milliseconds, type the following 
in the CONFIG&per.SYS file&colon. 

:p.CALL=X&colon.&bsl.TOOLS&bsl.SYSTEM&bsl.BIN&bsl.CACHEF32&per.EXE /D&colon.2000 
:efn.

:fn id=fn79.

:p.:hp2.CACHEF32&per.EXE Command&colon.  /B&colon. Parameter :ehp2.

:p.Sets the amount of time (in milliseconds) that the cache buffer can be idle 
before the data it contains must be written to a disk&per. 

:p.To set the amount of cache buffer idle time to 1000 milliseconds, type the 
following in the CONFIG&per.SYS file&colon. 

:p.CALL=X&colon.&bsl.TOOLS&bsl.SYSTEM&bsl.BIN&bsl.CACHEF32&per.EXE /B&colon.1000 
:efn.

:fn id=fn80.

:p.:hp2.CACHEF32&per.EXE Command&colon.   /M&colon. Parameter :ehp2.

:p.Specifies the maximum amount of time (in milliseconds) before frequently 
written data is transferred to the disk&per. 

:p.To have frequently written data that has been in cache memory longer than 
4000 milliseconds written to disk, type the following in the CONFIG&per.SYS file
&colon. 

:p.CALL=X&colon.&bsl.TOOLS&bsl.SYSTEM&bsl.BIN&bsl.CACHEF32&per.EXE /M&colon.4000 
:efn.

:fn id=fn81.

:p.:hp2.CACHEF32&per.EXE Command&colon.  /L&colon. Parameter :ehp2.

:p.Specifies whether the contents of cache memory are written immediately to 
disk or only during disk idle time&per. This command pertains only to disks 
formatted for the FAT32 File System&per. ON enables writing of cache memory during disk 
idle time&per. OFF specifies immediate writing to disk&per. The default state is for 
lazy-writing to be ON&per.  If you want lazy-writing to be OFF as the default, you 
must add the following statement to your CONFIG&per.SYS file&colon. 

:p.CALL=X&colon.&bsl.TOOLS&bsl.SYSTEM&bsl.BIN&bsl.CACHEF32&per.EXE /L&colon.OFF 
:efn.

:fn id=fn82.:hp2.
.ce License&per.txt
.br 
:ehp2.
.ce This sourcecode was/is originaly written by Henk Kelder <henk&per.kelder
&atsign.cgey&per.nl>
.br 

.ce Henk Kelder gave the copyright of the sourcecode to netlabs&per.org in 
.br 

.ce January 2002&per. Because of the amount of work he spent on this sourcecode,
.br 

.ce he preferred to have a license that makes sure that no company or individual 
.br 

.ce can make profit from it&per. Because of this, we release the sourcecode 
under the 
.br 

.ce GNU LESSER GENERAL PUBLIC LICENSE (LGPL)&per.
.br 

:p.
.ce The original license is online at http&colon.//www&per.gnu&per.org/copyleft/
lesser&per.txt
.br 

.ce More information about GNU Software can be found at http&colon.//www&per.gnu
&per.org
.br 

.ce 6&per. January 2002 
.br 

.ce Adrian Gschwend <ktk&atsign.netlabs&per.org>
.br 

.ce http&colon.//www&per.netlabs&per.org
.br 

:efn.
:euserdoc.
