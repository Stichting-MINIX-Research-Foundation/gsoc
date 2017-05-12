#ifndef _BSP_INTR_H_
#define _BSP_INTR_H_

#ifndef __ASSEMBLY__

#define INTR_GENERATE(name) \
	void name##_irq_unmask (int irq); \
	void name##_irq_mask (int irq); \
	void name##_irq_handle (void); \
	int  name##_intr_init(const int auto_eoi);

INTR_GENERATE(rpi);
INTR_GENERATE(omap);

#endif /* __ASSEMBLY__ */

#endif /* _BSP_INTR_H_ */
