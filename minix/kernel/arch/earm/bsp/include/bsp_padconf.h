#ifndef _BSP_PADCONF_H_
#define _BSP_PADCONF_H_

#ifndef __ASSEMBLY__

#define PADCONF_GENERATE(name) \
	void name##_padconf_init (void); \
	int  name##_padconf_set (u32_t padconf, u32_t mask, u32_t value);

PADCONF_GENERATE(rpi);
PADCONF_GENERATE(omap);

#endif /* __ASSEMBLY__ */

#endif /* _BSP_PADCONF_H_ */
