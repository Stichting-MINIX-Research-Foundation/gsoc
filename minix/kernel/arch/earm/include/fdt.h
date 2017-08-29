#ifndef __FDT_H__
#define __FDT_H__

/* Callbacks */
#define FDT_PROP_CB(name) int name (void *fdt, int offset, void *dst)

FDT_PROP_CB(fdt_set_machine_type);
FDT_PROP_CB(fdt_set_memory);
FDT_PROP_CB(fdt_set_cpu_num);
FDT_PROP_CB(fdt_set_bootargs);
FDT_PROP_CB(fdt_check_compat);

int fdt_is_compatible (void *fdt);
int fdt_step_node (void *fdt, int (*cb)(void *fdt, int offset, void *dst), void *dst);

#endif
