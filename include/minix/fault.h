#include <minix/ipc.h>

#define FAULT_INJECTOR_CMD_SWITCH   0
#define FAULT_INJECTOR_CMD_TEST     1

void fault_switch();

void fault_test();

int do_fault_injector_request(message *m);

