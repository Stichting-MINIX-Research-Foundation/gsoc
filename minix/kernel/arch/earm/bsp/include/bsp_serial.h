#ifndef _BSP_SERIAL_H_
#define _BSP_SERIAL_H_

#define SERIAL_GENERATE(name) \
	void name##_ser_init (void); \
	void name##_ser_putc (char c);

SERIAL_GENERATE(rpi);
SERIAL_GENERATE(omap);

#endif /* _BSP_SERIAL_H_ */
