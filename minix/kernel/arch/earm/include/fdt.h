#ifndef __FDT_H__
#define __FDT_H__

int fdt_set_machine_type (void *fdt, int offset, void *machine);
int fdt_set_memory (void *fdt, int offset, void *dst);
int fdt_set_cpu_num (void *fdt, int offset, void *dst);
int fdt_step_node (void *fdt, int (*cb)(void *fdt, int offset, void *dst), void *dst);

#endif