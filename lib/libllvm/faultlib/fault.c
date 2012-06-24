#include <stdio.h>
#include <minix/fault.h>
#include <signal.h>

int faultinjection_enabled = 0;

void fault_test(){
    printf("faultinjection_enabled: %d\n", faultinjection_enabled);
}

int do_fault_injector_request(message *m){
    fault_test();
}

