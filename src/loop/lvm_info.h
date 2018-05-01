#ifndef LVM_DATA_H_INCLUDED
#define LVM_DATA_H_INCLUDED

#define DLA_TABLE_SIGNATURE1  0x424D5202L
#define DLA_TABLE_SIGNATURE2  0x44464D50L

#define PARTITION_NAME_SIZE  20
#define VOLUME_NAME_SIZE     20
#define DISK_NAME_SIZE       20

typedef struct _DLA_Entry
{
    ULONG Volume_Serial_Number;
    ULONG Partition_Serial_Number;
    ULONG Partition_Size;
    ULONG Partition_Start;
    UCHAR On_Boot_Manager_Menu;
    UCHAR Installable;
    CHAR  Drive_Letter;
    UCHAR Reserved;
    CHAR  Volume_Name[VOLUME_NAME_SIZE];
    CHAR  Partition_Name[PARTITION_NAME_SIZE];
} DLA_Entry;

typedef struct _DLA_Table_Sector 
{
    ULONG DLA_Signature1;
    ULONG DLA_Signature2;
    ULONG DLA_CRC;
    ULONG Disk_Serial_Number;
    ULONG Boot_Disk_Serial_Number;
    ULONG Install_Flags;
    ULONG Cylinders;
    ULONG Heads_Per_Cylinder;
    ULONG Sectors_Per_Track;
    CHAR  Disk_Name[DISK_NAME_SIZE];
    UCHAR Reboot;
    UCHAR Reserved[3];
    DLA_Entry DLA_Array[4];
} DLA_Table_Sector;

#endif
