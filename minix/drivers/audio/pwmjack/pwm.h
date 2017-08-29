#ifndef _PWM_H_
#define _PWM_H_

#include <minix/sound.h>
#include <minix/audio_fw.h>

#define AUDIO 0

#define PWM_IRQ			50

#define DEFAULT_SPEED		44100   /* Sample rate */
#define DEFAULT_BITS		16	   /* Nr. of bits */
#define DEFAULT_SIGN		0	   /* 0 = unsigned, 1 = signed */
#define DEFAULT_STEREO		0	   /* 0 = mono, 1 = stereo */

#define PWM_CTL 		0x00
#define PWM_STA 		0x04
#define PWM_DMAC 		0x08
#define PWM_RNG1 		0x10
#define PWM_DAT1 		0x14
#define PWM_FIF1 		0x18
#define PWM_RNG2 		0x20
#define PWM_DAT2	 	0x24

#define PWM_BASE		0x3f20C000
#define PWM_REG_SIZE	0x28
#define CLK_BASE		0x3f101000
#define DMA_BASE		0x3f007000
#define DMA_REG_SIZE	0x100

#define DMA_CHAN		0

#define DMA_CS			0x0
#define DMA_CONBLK		0x4
#define DMA_TI			0x8
#define DMA_SOURCE_AD	0xc
#define DMA_DEST_AD		0x10
#define DMA_TXFR_LEN	0x14
#define DMA_STRIDE		0x18
#define DMA_NEXTCONBK	0x1c

#define DMA_EN 			0xff0

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

#define DREQ_PWM		5
#define PERMAP_SET(v) ((v) << 16)

#define PWM_CTL_PWEN1	0x1
#define PWM_CTL_MODE1	0x2
#define PWM_CTL_RPTL1	0x4
#define PWM_CTL_SBIT1	0x8
#define PWM_CTL_POLA1	0x10
#define PWM_CTL_USEF1	0x20
#define PWM_CTL_CLRF1	0x40
#define PWM_CTL_MSEN1	0x80
#define PWM_CTL_PWEN2	0x100
#define PWM_CTL_MODE2	0x200
#define PWM_CTL_RPTL2	0x400
#define PWM_CTL_SBIT2	0x800
#define PWM_CTL_POLA2	0x1000
#define PWM_CTL_USEF2	0x2000
#define PWM_CTL_MSEN2	0x8000

#define PWM_STA_FULL1	0x1
#define PWM_STA_EMPT1	0x2
#define PWM_STA_WERR1	0x4
#define PWM_STA_RERR1	0x8
#define PWM_STA_GAPO1	0x10
#define PWM_STA_GAPO2	0x20
#define PWM_STA_GAPO3	0x40
#define PWM_STA_GAPO4	0x80
#define PWM_STA_BERR	0x100
#define PWM_STA_STA1	0x200
#define PWM_STA_STA2	0x400
#define PWM_STA_STA3	0x800
#define PWM_STA_STA4	0x1000
#define ERRMASK 		(PWM_STA_WERR1 | PWM_STA_RERR1 | PWM_STA_BERR)

#define PWM_DMAC_ENAB	0x80000000
#define PWM_DMAC_PANIC(v)	((v) << 8)
#define PWM_DMAC_DREQ(v) 	(v)

#define CLK_PWMCTL		0xa0
#define CLK_PWMDIV		0xa4

/* Clock register settings */
#define CLK_FREQ		19200000

#define CLK_PASSWD		(0x5a000000)
#define CLK_PASSWD_MASK	(0xff000000)
#define CLK_FLIP		0x100
#define CLK_BUSY		0x80
#define CLK_KILL		0x20
#define CLK_ENAB		0x10

#define CLK_SRC(v) (v)
#define CLK_MASH(v) ((v) << 9)

#define CLK_SHIFT		(12)
#define CLK_DIVI(v)		((v) << CLK_SHIFT)
#define CLK_DIVF(v)		(v)
#define CLK_DIVF_MASK	(0xFFF)

#define DSP_MAX_SPEED			44100      /* Max sample speed in KHz */
#define DSP_MIN_SPEED			4000       /* Min sample speed in KHz */

/* Number of bytes you can DMA before hitting a 64K boundary: */
#define dma_bytes_left(phys)    \
   ((unsigned) (sizeof(int) == 2 ? 0 : 0x10000) - (unsigned) ((phys) & 0xFFFF))

uint32_t pwm_read(vir_bytes port);
void pwm_write(vir_bytes port, uint32_t value);
void pwm_set(vir_bytes port, uint32_t mask, uint32_t value);


#endif // _PWM_H_