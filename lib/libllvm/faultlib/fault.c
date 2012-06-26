#define _SYSTEM 1
#include <stdio.h>
#include <stdlib.h>
#include <minix/com.h>
#include <minix/fault.h>
#include <signal.h>
#include <sys/errno.h>

int faultinjection_enabled = 0, lh=4, rh=3;

void fault_test(){
    printf("fault_test: %d\n", lh-rh);
}

void fault_switch(int enable){
    faultinjection_enabled = enable;
    printf("faultinjection_enabled: %d\n", faultinjection_enabled);
}

int do_fault_injector_request(message *m){
    message replymsg;
    if(m->FAULT_INJECTOR_CMD == FAULT_INJECTOR_CMD_OFF){
        fault_switch(0);
    }else if(m->FAULT_INJECTOR_CMD == FAULT_INJECTOR_CMD_ON){
        fault_switch(1);
    }else if(m->FAULT_INJECTOR_CMD == FAULT_INJECTOR_CMD_TEST){
        fault_test();
    }
    return send(m->m_source, &replymsg);
}

