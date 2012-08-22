#define _SYSTEM 1
#include <stdio.h>
#include <stdlib.h>
#include <minix/com.h>
#include <minix/fault.h>
#include <signal.h>
#include <sys/errno.h>

#include "../../libc/stdlib/rand.c"

volatile int magic_ensure_linkage = ((int) &rand);

int faultinjection_enabled = 0, lh=4, rh=3;

void fault_test(){
    int my_lh=4, my_rh=3;
    printf("fault_test: %d\n", my_lh-my_rh);
}

void fault_switch(int enable){
    faultinjection_enabled = enable;
    printf("faultinjection_enabled: %d\n", faultinjection_enabled);
}

int do_fault_injector_request_impl(message *m){
    if(m->FAULT_INJECTOR_CMD == FAULT_INJECTOR_CMD_OFF){
        fault_switch(0);
    }else if(m->FAULT_INJECTOR_CMD == FAULT_INJECTOR_CMD_ON){
        fault_switch(1);
    }else if(m->FAULT_INJECTOR_CMD == FAULT_INJECTOR_CMD_TEST){
        fault_test();
    }else{
        return EGENERIC;
    }
    return OK;
}

