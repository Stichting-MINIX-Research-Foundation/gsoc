#ifndef _RPI_I2C_REGISTERS_H
#define _RPI_I2C_REGISTERS_H

/* I2C Addresses for bcm2835 */

/* IRQ Numbers */
#define BCM283X_I2C0_IRQ 47

/* Base Addresses */
#define BCM283X_I2C0_BASE 0x3f205000
#define BCM283X_I2C1_BASE 0x3f804000

/* Size of I2C Register Address Range */
#define BCM283X_I2C0_SIZE 0x1000
#define BCM283X_I2C1_SIZE 0x1000

/* Register Offsets */
#define BCM283X_CTRL	0x00
#define BCM283X_STATUS	0x04
#define BCM283X_DLEN	0x08
#define BCM283X_SL_ADDR	0x0C
#define BCM283X_FIFO	0x10
#define BCM283X_DIV 	0x14
#define BCM283X_DEL 	0x18
#define BCM283X_CLKT 	0x1c

/* Constants */
#define BCM283X_FUNCTIONAL_CLOCK 	96000000 	/* 96 MHz */
#define BCM283X_MODULE_CLOCK 		12000000	/* 12 MHz */

#define BCM283X_I2C_CDIV_MIN	0x0002
#define BCM283X_I2C_CDIV_MAX	0xFFFE

/* Shared Values */

#define BUS_SPEED_100KHz 	100000	/* 100 KHz */
#define BUS_SPEED_400KHz 	400000	/* 400 KHz */
#define I2C_OWN_ADDRESS 	0x01

/* Masks */

#define MAX_I2C_SA_MASK (0x3ff)	/* Highest 10 bit address -- 9..0 */
#define SL_ADDR_MASK (0x78) /* Mask for slave address (the last two bits 
								are the most significant of the addr)*/

/* Bit Offsets within Registers (only those used are listed) */

#define I2C_EN 15
#define MST    10
#define TRX		9
#define XSA     8
#define STP     1
#define STT     0

#define BCM283X_STATUS_TA	0x01
#define BCM283X_STATUS_DONE	0x02
#define BCM283X_STATUS_TXW 	0x04
#define BCM283X_STATUS_RXR 	0x08
#define BCM283X_STATUS_TXD 	0x10
#define BCM283X_STATUS_RXD 	0x20
#define BCM283X_STATUS_TXE 	0x40
#define BCM283X_STATUS_RXF 	0x80
#define BCM283X_STATUS_ERR 	0x100
#define BCM283X_STATUS_CLKT	0x200

#define BCM283X_CTRL_READ	0x01
#define BCM283X_CTRL_ST		0x80
#define BCM283X_CTRL_CFIFO	0x30
#define BCM283X_CTRL_INTD	0x100
#define BCM283X_CTRL_INTT	0x200
#define BCM283X_CTRL_INTR	0x400
#define BCM283X_CTRL_I2C_EN	0x8000

#endif /* _RPI_I2C_REGISTERS_H */
