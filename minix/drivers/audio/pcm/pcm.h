#ifndef _PCM_H_
#define _PCM_H_

#include <minix/sound.h>
#include <minix/audio_fw.h>

#define AUDIO 0

#define PCM_IRQ			49

#define DEFAULT_SPEED		44100   /* Sample rate */
#define DEFAULT_BITS		16	   /* Nr. of bits */
#define DEFAULT_SIGN		0	   /* 0 = unsigned, 1 = signed */
#define DEFAULT_STEREO		0	   /* 0 = mono, 1 = stereo */

#define PCM_CS_A 		0x00
#define PCM_FIFO_A 		0x04
#define PCM_MODE_A 		0x08
#define PCM_RXC_A 		0x0C
#define PCM_TXC_A 		0x10
#define PCM_DREQ_A 		0x14
#define PCM_INTEN_A 	0x18
#define PCM_INTSTC_A 	0x1C
#define PCM_GRAY 		0x20

#define I2S_BASE		0x3f203000
#define I2S_REG_SIZE	0x200
#define CLK_BASE		0x3f101000
#define CLK_REG_SIZE	0x8
#define DMA_BASE		0x3f007000
#define DMA_REG_SIZE	0x100

#define DMA_CHAN		5

#define DMA_CS			0x0
#define DMA_CONBLK		0x4
#define DMA_TI			0x8
#define DMA_SOURCE_AD	0xc
#define DMA_DEST_AD		0x10
#define DMA_TXFR_LEN	0x14
#define DMA_STRIDE		0x18
#define DMA_NEXTCONBK	0x1c

#define DMA_CS_ACTIVE	0x1
#define DMA_CS_END		0x2
#define DMA_CS_INT		0x4
#define DMA_CS_RESET	0x80000000

#define DMA_TI_INTEN	0x1
#define DMA_TI_WAIT_RESP	0x8
#define DMA_TI_DST_INC	0x10
#define DMA_TI_DST_DREQ 0x40
#define DMA_TI_SRC_INC	0x100
#define DMA_TI_TDMODE	0x200
#define DMA_TI_SRC_DREQ 0x400

#define DREQ_TX		2
#define DREQ_RX		3
#define PERMAP_SET(v) ((v) << 16)

#define PCM_CS_A_EN			0x1
#define PCM_CS_A_RXON		0x2
#define PCM_CS_A_TXON		0x4
#define PCM_CS_A_TXCLR		0x8
#define PCM_CS_A_RXCLR		0x10
#define PCM_CS_A_TXTHR		0x60
#define PCM_CS_A_RXTHR		0x180
#define PCM_CS_A_DMAEN		0x200
#define PCM_CS_A_TXSYNC		0x2000
#define PCM_CS_A_RXSYNC		0x4000
#define PCM_CS_A_TXERR		0x8000
#define PCM_CS_A_RXERR		0x10000
#define PCM_CS_A_TXW		0x20000
#define PCM_CS_A_RXR		0x40000
#define PCM_CS_A_TXD		0x80000
#define PCM_CS_A_RXD		0x100000
#define PCM_CS_A_TXE		0x200000
#define PCM_CS_A_RXF		0x400000
#define PCM_CS_A_RXSEX		0x800000
#define PCM_CS_A_SYNC		0x1000000
#define PCM_CS_A_STBY		0x2000000

#define PCM_TXC_A_CH2WID8	0x0
#define PCM_TXC_A_CH2WID16	0x8
#define PCM_TXC_A_CH2WID24	0x8000
#define PCM_TXC_A_CH2WID32	0x8008
#define PCM_TXC_A_CH2EN		0x4000
#define PCM_TXC_A_CH1WID8	0x0
#define PCM_TXC_A_CH1WID16	0x80000
#define PCM_TXC_A_CH1WID24	0x80000000
#define PCM_TXC_A_CH1WID32	0x80080000
#define PCM_TXC_A_CH1EN		0x40000000

#define PCM_RXC_A_CH2WID8	0x0
#define PCM_RXC_A_CH2WID16	0x8
#define PCM_RXC_A_CH2WID24	0x8000
#define PCM_RXC_A_CH2WID32	0x8008
#define PCM_RXC_A_CH2EN		0x4000
#define PCM_RXC_A_CH1WID8	0x0
#define PCM_RXC_A_CH1WID16	0x80000
#define PCM_RXC_A_CH1WID24	0x80000000
#define PCM_RXC_A_CH1WID32	0x80080000
#define PCM_RXC_A_CH1EN		0x40000000

#define PCM_MODE_A_FSI		0x100000
#define PCM_MODE_A_FSM		0x200000
#define PCM_MODE_A_CLKI		0x400000
#define PCM_MODE_A_CLKM		0x800000
#define PCM_MODE_A_FTXP		0x1000000
#define PCM_MODE_A_FRXP		0x2000000
#define PCM_MODE_A_PDME		0x4000000
#define PCM_MODE_A_PDMN		0x8000000
#define PCM_MODE_A_CLK_DIS	0x10000000

#define PCM_INTEN_A_TXW		0x1
#define PCM_INTEN_A_RXR		0x2
#define PCM_INTEN_A_TXERR	0x4
#define PCM_INTEN_A_RXERR	0x8

#define PCM_INTSTC_A_TXW	0x1
#define PCM_INTSTC_A_RXR	0x2
#define PCM_INTSTC_A_TXERR	0x4
#define PCM_INTSTC_A_RXERR	0x8

#define CLK_PCMCTL		0x00
#define CLK_PCMDIV		0x04

/* Clock register settings */
#define PCM_CLK_FREQ		19200000

#define PCM_CLK_PASSWD		(0x5a000000)
#define PCM_CLK_PASSWD_MASK	(0xff000000)
#define PCM_CLK_FLIP		0x100
#define PCM_CLK_BUSY		0x80
#define PCM_CLK_KILL		0x20
#define PCM_CLK_ENAB		0x10

#define CLK_SRC(v) (v)
#define CLK_MASH(v) ((v) << 9)

#define CLK_SHIFT		(12)
#define CLK_DIVI(v)		((v) << CLK_SHIFT)
#define CLK_DIVF(v)		(v)
#define CLK_DIVF_MASK	(0xFFF)

#define DSP_MAX_SPEED			44100      /* Max sample speed in KHz */
#define DSP_MIN_SPEED			4000       /* Min sample speed in KHz */

#define PCM_TX_PANIC(v)	((v) << 24)
#define PCM_RX_PANIC(v) ((v) << 16)
#define PCM_TX(v)		((v) << 8)
#define PCM_RX(v) (v)

/* Number of bytes you can DMA before hitting a 64K boundary: */
#define dma_bytes_left(phys)    \
   ((unsigned) (sizeof(int) == 2 ? 0 : 0x10000) - (unsigned) ((phys) & 0xFFFF))

uint32_t pcm_read(vir_bytes port);
void pcm_write(vir_bytes port, uint32_t value);
void pcm_set(vir_bytes port, uint32_t mask, uint32_t value);


#endif // _PCM_H_