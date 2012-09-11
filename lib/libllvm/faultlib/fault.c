#define _SYSTEM 1
#include <stdio.h>
#include <stdlib.h>
#include <minix/com.h>
#include <minix/fault.h>
#include <signal.h>
#include <sys/errno.h>

#include "../../libc/stdlib/rand.c"

volatile int magic_ensure_linkage = ((int) &rand);

int faultinjection_enabled = 0;
int fault_count_swap = 0, fault_count_no_load = 0, fault_count_no_store = 0, fault_count_flip_bool = 0, fault_count_flip_branch = 0, fault_count_corrupt_pointer = 0;

int lh=4, rh=3, condition=~0;

void fault_test_no_load(){
    int my_lh=lh, my_rh=rh;
    printf("fault_test: %d\n", my_lh-my_rh);
    lh++;
}

void fault_test_no_store(){
    int my_lh=lh, my_rh=rh;
    printf("fault_test: %d\n", my_lh-my_rh);
    lh++;
}

void fault_test_flip_bool(){
    volatile int local_condition = 1;
    volatile int new_condition = local_condition && condition;
    if(new_condition){
        printf("fault_test: do\n");
    }
    condition = ~condition;
}

void fault_test_corrupt_pointer(){
    volatile int i;
    volatile int *p = &i;
    if(i){
        p++;
    }
    printf("pointer: %p\n", p);
}

void fault_test(){
    printf("fault_test_no_load_start\n");
    fault_test_no_load();
    printf("fault_test_no_load_end\n");
    printf("fault_test_no_store_start\n");
    fault_test_no_store();
    printf("fault_test_no_store_end\n");
    printf("fault_test_flip_bool_start\n");
    fault_test_flip_bool();
    fault_test_flip_bool();
    fault_test_flip_bool();
    printf("fault_test_flip_bool_end\n");
    printf("fault_test_corrupt_pointer_start\n");
    fault_test_corrupt_pointer();
    printf("fault_test_corrupt_pointer_end\n");
}

void fault_switch(int enable){
    faultinjection_enabled = enable;
    printf("faultinjection_enabled: %d\n", faultinjection_enabled);
}

void fault_print_stats(){
    printf("faultinjector stats:\n");
    printf("   swap:     %d\n", fault_count_swap);
    printf("   no load:  %d\n", fault_count_no_load);
    printf("   no store: %d\n", fault_count_no_store);
    printf("   flip bool: %d\n", fault_count_flip_bool);
    printf("   flip branch: %d\n", fault_count_flip_branch);
    printf("   corrupt pointer: %d\n", fault_count_corrupt_pointer);
}

int do_fault_injector_request_impl(message *m){
    int i;
    if(m->FAULT_INJECTOR_CMD == FAULT_INJECTOR_CMD_OFF){
        fault_switch(0);
    }else if(m->FAULT_INJECTOR_CMD == FAULT_INJECTOR_CMD_ON){
        fault_switch(1);
    }else if(m->FAULT_INJECTOR_CMD == FAULT_INJECTOR_CMD_TEST){
        for(i=0;i<1;i++){
            fault_test();
        }
    }else if(m->FAULT_INJECTOR_CMD == FAULT_INJECTOR_CMD_PRINT_STATS){
        fault_print_stats();
    }else{
        return EGENERIC;
    }
    return OK;
}

