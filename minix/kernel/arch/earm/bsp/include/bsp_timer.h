#ifndef _BSP_TIMER_H_
#define _BSP_TIMER_H_

#ifndef __ASSEMBLY__

#define TIMER_GENERATE(name) \
	void name##_timer_init (unsigned freq); \
	void name##_timer_stop (void); \
	int  name##_register_timer_handler (const irq_handler_t handler); \
	void name##_timer_int_handler (void); \
	void name##_read_tsc_64 (u64_t * t)

TIMER_GENERATE(rpi);
TIMER_GENERATE(omap);

#endif /* __ASSEMBLY__ */

#endif /* _BSP_TIMER_H_ */
