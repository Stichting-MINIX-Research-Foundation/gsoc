#ifndef _SPI_H_
#define _SPI_H_

#define SPI0_BASE		0x3f204000
#define SPI1_BASE		0x3f215080
#define SPI2_BASE		0x3f2150C0
#define SPI_REG_SIZE	0x18

#define SPI_IRQ			48
#define SPI_DEF_SPEED	250000000

#define SPI_CS			0x0
#define SPI_FIFO		0x4
#define SPI_CLK			0x8
#define SPI_DLEN		0xc
#define SPI_LTOH		0x10
#define SPI_DC			0x14

#define SPI_CS_CS 		0x3
#define SPI_CS_MODE		0xc
#define SPI_CS_MODE0	0x0
#define SPI_CS_MODE1	0x4
#define SPI_CS_MODE2	0x8
#define SPI_CS_MODE3	0xc
#define SPI_CS_TX_CLEAR	0x10
#define SPI_CS_RX_CLEAR 0x20
#define SPI_CS_TA		0x80
#define SPI_CS_DMAEN	0x100
#define SPI_CS_INTD		0x200
#define SPI_CS_INTR		0x400
#define SPI_CS_REN		0x1000
#define SPI_CS_DONE		0x10000
#define SPI_CS_RXD		0x20000
#define SPI_CS_TXD		0x40000
#define SPI_CS_CSPOL0	0x200000

#define WRITE_MODE		1
#define READ_MODE		0

uint32_t spi_read32(vir_bytes port);
void spi_write32(vir_bytes port, uint32_t value);
void spi_set32(vir_bytes port, uint32_t mask, uint32_t value);
#endif /* _SPI_H_ */