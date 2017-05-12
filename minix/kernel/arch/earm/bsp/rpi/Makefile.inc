#
# BSP for RPI hardware
#

.PATH: ${.CURDIR}/arch/${MACHINE_ARCH}/bsp/rpi

SRCS+=	rpi_init.c rpi_serial.c rpi_timer.c rpi_padconf.c rpi_intr.c \
	rpi_reset.c
