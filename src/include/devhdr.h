#ifndef __DEVHDR_H__
#define __DEVHDR_H__

#define DEV_CBNAME      8

struct SysDev
{
   unsigned long    SDevNext;
   unsigned short   SDevAtt;
   unsigned short   SDevStrat;
   unsigned short   SDevInt;
   unsigned char    SDevName[DEV_CBNAME];
   unsigned short   SDevProtCS;
   unsigned short   SDevProtDS;
   unsigned short   SDevRealCS;
   unsigned short   SDevRealDS;
};

struct  SysDev3
{
   struct SysDev    SysDevBef3;
   unsigned long    SDevCaps;
};

#define DEV_IOCTL2         0x0001
#define DEV_16MB           0x0002
#define DEV_PARALLEL       0x0004
#define DEV_ADAPTER_DD     0x0008
#define DEV_INITCOMPLETE   0x0010

#define DEV_SAVERESTORE    0x0020

#define DEV_CIN            0x0001
#define DEV_COUT           0x0002
#define DEV_NULL           0x0004
#define DEV_CLOCK          0x0008
#define DEV_SPEC           0x0010
#define DEV_ADD_ON         0x0020
#define DEV_GIOCTL         0x0040
#define DEV_FCNLEV         0x0380

#define DEV_30             0x0800
#define DEV_SHARE          0x1000
#define DEV_NON_IBM        0x2000
#define DEV_IOCTL          0x4000
#define DEV_CHAR_DEV       0x8000

#define DEVLEV_0           0x0000
#define DEVLEV_1           0x0080
#define DEVLEV_2           0x0100
#define DEVLEV_3           0x0180


#endif /* __DEVHDR_H__ */
