#ifndef _BSP_INIT_H_
#define _BSP_INIT_H_

/* BSP init */
#define INIT_GENERATE(name) \
	void name##_init (void);

INIT_GENERATE(rpi);
INIT_GENERATE(omap);

#endif /* __BSP_INIT_H__ */
