#include <libfdt.h>
#include <minix/board.h>
#include <minix/type.h>
#include "fdt.h"
#include "bsp_compatibles.h"

#include <string.h>
#include <stdio.h>

void *dev_tree;

int fdt_set_machine_type (void *fdt, int offset, void *dst)
{
	struct machine *machine = (struct machine *)dst;
	const char *model = fdt_getprop(fdt, offset, "model", NULL);
	if (model == NULL)
		return -1;
	machine->board_id = get_board_id_by_long_name(model);

	if (machine->board_id == 0) {
		/* same thing as above there is no safe escape */
		return -1;
	}
	return 0;
}

int fdt_set_bootargs (void *fdt, int offset, void *dst)
{
	char *bootargs = (char *)dst;
	bootargs = (char *)fdt_getprop(fdt, offset, "bootargs", NULL);
	if (bootargs == NULL)
		return -1;
	return 0;
}

int fdt_set_memory (void *fdt, int offset, void *dst)
{
	return 0;
}

int fdt_set_cpu_num (void *fdt, int offset, void *dst)
{
	return 0;
}

int fdt_check_compat (void *fdt, int offset, void *dst)
{
	char *compat = (char *)dst;
	char *get_compat;
	get_compat = (char *)fdt_getprop(fdt, offset, "compatible", NULL);
	if (get_compat == NULL)
		return -1;
	if (!strcmp (compat, get_compat)) {
		return 0;
	}
	return -1;
}

int fdt_is_compatible (void *fdt)
{
	char ***table_compat = compatible_table;

	while (*table_compat) {
		while (**table_compat) {
			if (!fdt_step_node(fdt, fdt_check_compat, **table_compat))
				return 0;
			(*table_compat)++;
		}
		(table_compat)++;
	}
	return -1;
}

int fdt_step_node (void *fdt, int (*cb)(void *fdt, int offset, void *dst), void *dst)
{
	int offset = -1;
	int len = -1;
	int retv = 0;
	char* pathp;
	for (offset = fdt_next_node(fdt, -1, &len);
		 offset >= 0 && len >= 0;
		 offset = fdt_next_node(fdt, offset, &len)) {
		pathp = (char *)fdt_get_name(fdt, offset, NULL);
		retv = cb(fdt, offset, dst);
		if (retv == 0)
			return 0;
	}
	return -1;
}
