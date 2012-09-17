#define _SYSTEM 1
#include <stdio.h>
#include <stdlib.h>
#include <minix/com.h>
#include <minix/fault.h>

#include <sys/time.h>

int main(int argc, char **argv){
    message m;

    /* seed rand() */
    int seed;
    if(argc == 2){
        seed = atoi(argv[1]);
    }else{
        struct timeval tp;
        gettimeofday(&tp, NULL);
        seed = (int) tp.tv_usec;
    }
    srand(seed);

    /* turn on fault execution */
    m.FAULT_INJECTOR_CMD = FAULT_INJECTOR_CMD_ON;
    do_fault_injector_request_impl(&m);

    /* run fault test suite */
    m.FAULT_INJECTOR_CMD = FAULT_INJECTOR_CMD_TEST;
    do_fault_injector_request_impl(&m);

    /* print stats */
    m.FAULT_INJECTOR_CMD = FAULT_INJECTOR_CMD_PRINT_STATS;
    do_fault_injector_request_impl(&m);

    exit(0);
}
