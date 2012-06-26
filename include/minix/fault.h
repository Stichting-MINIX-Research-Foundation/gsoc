#include <minix/ipc.h>

#define FAULT_INJECTOR_CMD_OFF  0
#define FAULT_INJECTOR_CMD_ON   1
#define FAULT_INJECTOR_CMD_TEST 2

void fault_switch(int enable);

void fault_test();

int do_fault_injector_request(message *m);

