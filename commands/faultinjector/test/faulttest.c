#define _SYSTEM 1
#include <stdio.h>
#include <stdlib.h>
#include <minix/com.h>
#include <minix/fault.h>

#include <sys/time.h>

int main(){
    message m1, m2;

    /* seed rand() */
    struct timeval tp;
    gettimeofday(&tp, NULL);
    srand((int) tp.tv_usec);

    /* turn on fault execution */
    m1.FAULT_INJECTOR_CMD = FAULT_INJECTOR_CMD_ON;
    do_fault_injector_request_impl(&m1);

    /* run fault test suite */
    m2.FAULT_INJECTOR_CMD = FAULT_INJECTOR_CMD_TEST;
    do_fault_injector_request_impl(&m2);

    exit(0);
}
