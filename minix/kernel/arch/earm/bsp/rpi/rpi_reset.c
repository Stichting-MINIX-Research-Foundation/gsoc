#include <assert.h>
#include <sys/types.h>
#include <machine/cpu.h>
#include <minix/type.h>
#include <minix/board.h>
#include <io.h>

#include "kernel/kernel.h"
#include "kernel/proc.h"
#include "kernel/vm.h"
#include "kernel/proto.h"
#include "arch_proto.h"
#include "bsp_reset.h"

void
rpi_reset_init(void)
{
}

void
rpi_reset(void)
{
}

void
rpi_poweroff(void)
{
}

void rpi_disable_watchdog(void)
{
}

