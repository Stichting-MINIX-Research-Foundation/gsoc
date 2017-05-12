#ifndef _BSP_TABLE_H
#define _BSP_TABLE_H

#include "bsp_init.h"
#include "bsp_intr.h"
#include "bsp_serial.h"
#include "bsp_timer.h"
#include "bsp_padconf.h"
#include "bsp_reset.h"

typedef struct {
	void (*bsp_init)(void);
	void (*bsp_irq_unmask)(int irq);
	void (*bsp_irq_mask)(int irq);
	void (*bsp_irq_handle)(void);
	void (*bsp_padconf_init)(void);
	int  (*bsp_padconf_set)(u32_t padconf, u32_t mask, u32_t value);
	void (*bsp_reset_init)(void);
	void (*bsp_reset)(void);
	void (*bsp_poweroff)(void);
	void (*bsp_disable_watchdog)(void);
	void (*bsp_ser_init)(void);
	void (*bsp_ser_putc)(char c);
	void (*bsp_timer_init)(unsigned freq);
	void (*bsp_timer_stop)(void);
	int  (*bsp_register_timer_handler)(const irq_handler_t handler);
	void (*bsp_timer_int_handler)(void);
	void (*read_tsc_64)(u64_t * t);
	int  (*intr_init)(const int auto_eoi);
} bsp_table;

#endif