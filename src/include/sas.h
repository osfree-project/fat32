#ifndef __SAS_H__
#define __SAS_H__

#define SAS_SIG      "SAS "
#define SAS_CBSIG    4

struct SAS
{
   unsigned char   SAS_signature[SAS_CBSIG];
   unsigned short  SAS_tables_data;
   unsigned short  SAS_flat_sel;
   unsigned short  SAS_config_data;
   unsigned short  SAS_dd_data;
   unsigned short  SAS_vm_data;
   unsigned short  SAS_task_data;
   unsigned short  SAS_RAS_data;
   unsigned short  SAS_file_data;
   unsigned short  SAS_info_data;
};

struct SAS_tables_section
{
   unsigned short  SAS_tbl_GDT;
   unsigned short  SAS_tbl_LDT;
   unsigned short  SAS_tbl_IDT;
   unsigned short  SAS_tbl_GDTPOOL;
};

struct SAS_config_section
{
   unsigned short  SAS_config_table;
};

struct SAS_vm_section
{
   unsigned long   SAS_vm_arena;
   unsigned long   SAS_vm_object;
   unsigned long   SAS_vm_context;
   unsigned long   SAS_vm_krnl_mte;
   unsigned long   SAS_vm_glbl_mte;
   unsigned long   SAS_vm_pft;
   unsigned long   SAS_vm_prt;
   unsigned long   SAS_vm_swap;
   unsigned long   SAS_vm_idle_head;
   unsigned long   SAS_vm_free_head;
   unsigned long   SAS_vm_heap_info;
   unsigned long   SAS_vm_all_mte;
};

struct SAS_dd_section
{
   unsigned short  SAS_dd_bimodal_chain;
   unsigned short  SAS_dd_real_chain;
   unsigned short  SAS_dd_DPB_segment;
   unsigned short  SAS_dd_CDA_anchor_p;
   unsigned short  SAS_dd_CDA_anchor_r;
   unsigned short  SAS_dd_FSC;
};

struct SAS_RAS_section
{
   unsigned short  SAS_RAS_STDA_p;
   unsigned short  SAS_RAS_STDA_r;
   unsigned short  SAS_RAS_event_mask;
};

struct SAS_info_section
{
   unsigned short  SAS_info_global;
   unsigned long   SAS_info_local;
   unsigned long   SAS_info_localRM;
   unsigned short  SAS_info_CDIB;
};

struct SAS_task_section
{
   unsigned short  SAS_task_PTDA;
   unsigned long   SAS_task_ptdaptrs;
   unsigned long   SAS_task_threadptrs;
   unsigned long   SAS_task_tasknumber;
   unsigned long   SAS_task_threadcount;
};

struct SAS_file_section
{
   unsigned long   SAS_file_MFT;
   unsigned short  SAS_file_SFT;
   unsigned short  SAS_file_VPB;
   unsigned short  SAS_file_CDS;
   unsigned short  SAS_file_buffers;
};


#endif /* __SAS_H__ */
