#ifndef _BSP_RESET_H_
#define _BSP_RESET_H_

#define RESET_GENERATE(name) \
	void name##_reset_init (void); \
	void name##_reset (void); \
	void name##_poweroff (void); \
	void name##_disable_watchdog (void);

RESET_GENERATE(rpi);
RESET_GENERATE(omap);

#endif /* _BSP_RESET_H_ */
