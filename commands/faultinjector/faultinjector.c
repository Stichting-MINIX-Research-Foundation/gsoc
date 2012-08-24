#include <sys/types.h>
#include <lib.h>
#include <stdio.h>
#include <string.h>
#include <minix/fault.h>
#include <minix/ds.h>

#define CMD_OFF_STR     "off"
#define CMD_ON_STR      "on"
#define CMD_TEST_STR    "test"
#define CMD_PRINT_STATS_STR    "stats"

char *prog_name;

void print_usage(){
        fprintf(stderr, "Usage: %s <label> (%s|%s|%s|%s)\n", prog_name, CMD_ON_STR, CMD_OFF_STR, CMD_TEST_STR, CMD_PRINT_STATS_STR);
}

int main(int argc, char *argv[]){
    int cmd;
    char *cmd_str, *label;
	message msg;

    /* a printf fixes some memory issue when copying the label to pm, for some reason. */
    printf("%s", "");

    prog_name = argv[0];

    if(argc < 3){
        print_usage();
        return 1;
    }
   
    label = argv[1]; 
    cmd_str = argv[2];

    if(strlen(label) <= 0 || strlen(label) > DS_MAX_KEYLEN) {
        print_usage();
        return 1;
    }


    if(!strcmp(cmd_str, CMD_ON_STR)){
        cmd = FAULT_INJECTOR_CMD_ON;
    }else if(!strcmp(cmd_str, CMD_OFF_STR)){
        cmd = FAULT_INJECTOR_CMD_OFF;
    }else if(!strcmp(cmd_str, CMD_TEST_STR)){
        cmd = FAULT_INJECTOR_CMD_TEST;
    }else if(!strcmp(cmd_str, CMD_PRINT_STATS_STR)){
        cmd = FAULT_INJECTOR_CMD_PRINT_STATS;
    }else{
        print_usage();
        return 1;
    }

	msg.FAULT_INJECTOR_CMD  = cmd;
	msg.FAULT_INJECTOR_TARGET_LABEL = label;
  	_syscall(PM_PROC_NR, COMMON_REQ_FAULT_INJECTOR, &msg);
    return 0;
}
