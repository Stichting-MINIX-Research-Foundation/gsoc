#include <sys/types.h>
#include <lib.h>
#include <stdio.h>
#include <string.h>
#include <minix/fault.h>

#define CMD_OFF_STR     "off"
#define CMD_ON_STR      "on"
#define CMD_TEST_STR    "test"

char *prog_name;

void print_usage(){
        fprintf(stderr, "Usage: %s <endpoint> (%s|%s|%s)\n", prog_name, CMD_ON_STR, CMD_OFF_STR, CMD_TEST_STR);
}

int main(int argc, char *argv[]){
    int ep, cmd;
    char *cmd_str;
	message msg;

    prog_name = argv[0];

    if(argc < 3){
        print_usage();
        return 1;
    }
    
    if(sscanf(argv[1], "%d", &ep)!=1) {
        print_usage();
        return 1;
    }

    cmd_str = argv[2];

    if(!strcmp(cmd_str, CMD_ON_STR)){
        cmd = FAULT_INJECTOR_CMD_ON;
    }else if(!strcmp(cmd_str, CMD_OFF_STR)){
        cmd = FAULT_INJECTOR_CMD_OFF;
    }else if(!strcmp(cmd_str, CMD_TEST_STR)){
        cmd = FAULT_INJECTOR_CMD_TEST;
    }else{
        print_usage();
        return 1;
    }

	msg.FAULT_INJECTOR_CMD  = cmd;
  	_syscall(ep, COMMON_REQ_FAULT_INJECTOR, &msg);
    return 0;
}
