#include <minix/com.h>
#include <minix/vm.h>
#include <minix/mmio.h>
#include <minix/type.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <minix/padconf.h>

#include <inttypes.h>

#include "pcm.h"

#include <minix/log.h>

static void dsp_dma_setup(int sub_dev);

static int dsp_ioctl(unsigned long request, void *val, int *len);
static int dsp_set_size(unsigned int size);
static int dsp_set_speed(unsigned int speed);
static int dsp_set_stereo(unsigned int stereo);
static int dsp_set_bits(unsigned int bits);
static int dsp_set_sign(unsigned int sign);
static int dsp_get_max_frag_size(u32_t *val, int *len);

static void clock_stop();
static void clock_start();

static unsigned int DspStereo = DEFAULT_STEREO;
static unsigned int DspSpeed = DEFAULT_SPEED; 
static unsigned int DspBits = DEFAULT_BITS;
static unsigned int DspSign = DEFAULT_SIGN;
static unsigned int DspFragmentSize;

static phys_bytes DmaPhys;
static int running = FALSE;

/*
 * Define a structure to be used for logging
 */
static struct log log = {
	.name = "pcm",
	.log_level = LEVEL_INFO,
	.log_func = default_log
};

#define	div_roundup(x, y) (((x)+((y)-1))/(y))

typedef struct
{
	uint32_t TI;
	uint32_t SRC_AD;
	uint32_t DST_AD;
	uint32_t TXR_LEN;
	uint32_t STRIDE;
	uint32_t NEXTCONBK;
	uint32_t unused1;
	uint32_t unused2;
} ctrl_blk_t;

sub_dev_t sub_dev[1];
special_file_t special_file[2];
drv_t drv;
uint32_t io_base;
uint32_t clk_base;
uint32_t dma_base;
static phys_bytes ctrl_blk;
static phys_bytes ph;
char *blk;

int drv_init(void)
{
	drv.DriverName = "pcm";
	drv.NrOfSubDevices = 1;
	drv.NrOfSpecialFiles = 2;
	
	sub_dev[AUDIO].readable = 1;
	sub_dev[AUDIO].writable = 1;
	sub_dev[AUDIO].DmaSize = 16 * 1024;
	sub_dev[AUDIO].NrOfDmaFragments = 1;
	sub_dev[AUDIO].MinFragmentSize = 1024;
	sub_dev[AUDIO].NrOfExtraBuffers = 4;
	
	special_file[0].minor_dev_nr = 0;
	special_file[0].write_chan = AUDIO;
	special_file[0].read_chan = NO_CHANNEL;
	special_file[0].io_ctl = AUDIO;

	special_file[1].minor_dev_nr = 1;
	special_file[1].write_chan = NO_CHANNEL;
	special_file[1].read_chan = AUDIO;
	special_file[1].io_ctl = AUDIO;

	return OK;
}

static void config_clk()
{
	log_debug(&log, "config clk starts\n");

	struct minix_mem_range mr;
	char *base;

	mr.mr_base = CLK_BASE;
	mr.mr_limit = CLK_BASE + CLK_REG_SIZE;

	/* grant ourself rights to map the register memory */
	if (sys_privctl(SELF, SYS_PRIV_ADD_MEM, &mr) != 0) {
		panic("Unable to request permission to map memory");
	}

	/* Set the base address to use */
	clk_base =
	    (uint32_t) vm_map_phys(SELF, (void *) CLK_BASE,
	    CLK_REG_SIZE);

	if (clk_base == (uint32_t) MAP_FAILED)
		panic("Unable to map clock memory");
}

static void config_dma()
{
	log_debug(&log, "config dma starts\n");

	struct minix_mem_range mr;

	mr.mr_base = DMA_BASE + DMA_REG_SIZE * DMA_CHAN;
	mr.mr_limit = mr.mr_base + DMA_REG_SIZE;

	/* grant ourself rights to map the register memory */
	if (sys_privctl(SELF, SYS_PRIV_ADD_MEM, &mr) != 0) {
		panic("Unable to request permission to map memory");
	}

	/* Set the base address to use */
	dma_base =
	    (uint32_t) vm_map_phys(SELF, (void *) DMA_BASE + DMA_REG_SIZE * DMA_CHAN,
	    DMA_REG_SIZE);

	if (dma_base == (uint32_t) MAP_FAILED)
		panic("Unable to map dma memory");

	blk = alloc_contig(sizeof(ctrl_blk_t), AC_ALIGN4K, &ph);
	if ((u8_t *)blk == (u8_t *) MAP_FAILED) {
		panic("Unable to allocate contiguous memory for control block\n");
	}

	int i = sys_umap(SELF, VM_D, (vir_bytes) blk, (phys_bytes) sizeof(ctrl_blk_t),
			&(ctrl_blk));

	if (i != OK) {
		panic("Unable to map phys control block\n");
	}
}

static void
pcm_padconf()
{
	log_debug(&log, "padconf starts\n");

	int r;
	uint32_t pinopts = CONTROL_BCM_CONF_PCM_CLK
		| CONTROL_BCM_CONF_PCM_FS;

	r = sys_padconf(GPFSEL1019, pinopts,
	    pinopts);
	if (r != OK) {
		log_warn(&log, "padconf failed (r=%d)\n", r);
	}
	
	log_debug(&log, "pinopts=0x%x\n", pinopts);

	pinopts = CONTROL_BCM_CONF_PCM_DIN
		| CONTROL_BCM_CONF_PCM_DOUT;

	r = sys_padconf(GPFSEL2029, pinopts,
	    pinopts);
	if (r != OK) {
		log_warn(&log, "padconf failed (r=%d)\n", r);
	}
	
	log_debug(&log, "pinopts=0x%x\n", pinopts);
}

static void clear_fifo()
{
	int timeout = 1000;
	uint32_t clkreg;

	/* Stop I2S module */
	pcm_set(PCM_CS_A, PCM_CS_A_TXON, 0);

	pcm_set(PCM_CS_A, PCM_CS_A_TXCLR, PCM_CS_A_TXCLR);

	while (--timeout) {
		clkreg = read32(clk_base + CLK_PCMCTL);
		if (!(clkreg & PCM_CLK_BUSY))
			break;
	}

	if (!timeout) {
		/* KILL the clock */
		log_warn(&log, "PCM clock didn't stop. Kill the clock!\n");
		set32(clk_base + CLK_PCMCTL,
			PCM_CLK_KILL | PCM_CLK_PASSWD_MASK,
			PCM_CLK_KILL | PCM_CLK_PASSWD);
	}
	pcm_set(PCM_CS_A, PCM_CS_A_TXON, PCM_CS_A_TXON);

}

static void set_speed()
{
	uint32_t divider;
	uint32_t bclk_rate;
	uint32_t cur_freq = PCM_CLK_FREQ << CLK_SHIFT;

	switch(DspBits)
	{
		case 8:
		case 16:
			bclk_rate = 40;
			break;
		default:
			return;
	}

	divider = div_roundup(cur_freq, DspSpeed * bclk_rate);

	write32(clk_base + CLK_PCMDIV, PCM_CLK_PASSWD
			| CLK_DIVI(divider >> CLK_SHIFT)
			| CLK_DIVF(divider & CLK_DIVF_MASK));

	write32(clk_base + CLK_PCMCTL, PCM_CLK_PASSWD
			| CLK_MASH(1)
			| CLK_SRC(6));
}

int drv_init_hw(void) 
{
	int i;
	log_debug(&log, "drv_init_hw starts\n");
	struct minix_mem_range mr;

	mr.mr_base = I2S_BASE;
	mr.mr_limit = I2S_BASE + I2S_REG_SIZE;

	/* grant ourself rights to map the register memory */
	if (sys_privctl(SELF, SYS_PRIV_ADD_MEM, &mr) != 0) {
		panic("Unable to request permission to map memory");
	}

	/* Set the base address to use */
	io_base =
	    (uint32_t) vm_map_phys(SELF, (void *) I2S_BASE,
	    I2S_REG_SIZE);

	if (io_base == (uint32_t) MAP_FAILED)
		panic("Unable to map I2S memory");

	if(drv_reset () != OK) { 
		log_debug(&log, "No audio card detected\n");
		return -1;
	}

	config_clk();

	config_dma();

	pcm_padconf();

	if (DspBits == 16)
		pcm_write(PCM_TXC_A, PCM_TXC_A_CH1WID16 | PCM_TXC_A_CH2WID16 |
			PCM_TXC_A_CH1EN | PCM_TXC_A_CH2EN);
	else
		pcm_write(PCM_TXC_A, PCM_TXC_A_CH1WID8 | PCM_TXC_A_CH2WID8 |
			PCM_TXC_A_CH1EN | PCM_TXC_A_CH2EN);

	/* Set frame start position */
	pcm_set(PCM_TXC_A, (1 << 20) | (33 << 4), 0xffffffff);
	pcm_write(PCM_MODE_A, PCM_MODE_A_FTXP | (63 << 10) | 32);

	pcm_set(PCM_CS_A, PCM_CS_A_STBY, 0);
	for(i = 0; i < 1000; i++); /* wait a while */

	pcm_set(PCM_CS_A, PCM_CS_A_TXCLR, PCM_CS_A_TXCLR);
	pcm_set(PCM_CS_A, PCM_CS_A_RXCLR, PCM_CS_A_RXCLR);

	pcm_set(PCM_CS_A, PCM_CS_A_TXTHR, 0);
	pcm_set(PCM_CS_A, PCM_CS_A_RXTHR, 0);

	pcm_set(PCM_CS_A, PCM_CS_A_SYNC, 0);

	set_speed();

	pcm_write(PCM_INTSTC_A, PCM_INTSTC_A_TXW |
		PCM_INTSTC_A_RXR | PCM_INTSTC_A_TXERR | PCM_INTSTC_A_RXERR);
	pcm_write(PCM_INTEN_A, PCM_INTEN_A_TXW | PCM_INTEN_A_TXERR);

	pcm_set(PCM_CS_A, PCM_CS_A_EN, PCM_CS_A_EN);

	DspFragmentSize = sub_dev[AUDIO].DmaSize / sub_dev[AUDIO].NrOfDmaFragments;

	clear_fifo();

	return OK;
}

int drv_reset(void) 
{
	int i;
	log_debug(&log, "drv_reset starts\n");

	uint32_t ctrl = pcm_read(PCM_CS_A);
	ctrl &= (~PCM_CS_A_EN);
	pcm_write(PCM_CS_A, ctrl);
	for(i = 0; i < 1000; i++); /* wait a while */

	ctrl = pcm_read(PCM_CS_A);
	ctrl |= PCM_CS_A_EN;
	pcm_write(PCM_CS_A, ctrl);

	return OK;
}

int drv_start(int channel, int DmaMode) 
{
	int i;

	log_debug(&log, "drv_start starts\n");

	ctrl_blk_t ctrl;
	ctrl.SRC_AD = DmaPhys;
	ctrl.DST_AD = I2S_BASE + PCM_FIFO_A;
	ctrl.TXR_LEN = DspFragmentSize * sub_dev[channel].NrOfDmaFragments;
	ctrl.TI = PERMAP_SET(DREQ_TX) | DMA_TI_DST_DREQ 
		| DMA_TI_INTEN | DMA_TI_WAIT_RESP | DMA_TI_SRC_INC;
	ctrl.NEXTCONBK = ctrl_blk;

	memcpy(blk, &ctrl, sizeof(ctrl_blk_t));

	dsp_set_speed(DspSpeed);

	if (!DspStereo)
		pcm_set(PCM_TXC_A, PCM_TXC_A_CH2EN, 0);

	clear_fifo();

	pcm_set(PCM_CS_A, PCM_CS_A_TXON, PCM_CS_A_TXON);

	running = TRUE;
	
	dsp_dma_setup(DmaMode);

	return OK;
}


static void clock_start()
{
	set32(clk_base + CLK_PCMCTL, 
		PCM_CLK_PASSWD_MASK | PCM_CLK_ENAB,
		PCM_CLK_PASSWD | PCM_CLK_ENAB);
}

static void clock_stop()
{
	int timeout = 1000;
	uint32_t clkreg;

	set32(clk_base + CLK_PCMCTL,
		PCM_CLK_PASSWD_MASK | PCM_CLK_ENAB,
		PCM_CLK_PASSWD);

	while (--timeout) {
		clkreg = read32(clk_base + CLK_PCMCTL);
		if (!(clkreg & PCM_CLK_BUSY))
			break;
	}

	if (!timeout) {
		/* KILL the clock */
		log_warn(&log, "PCM clock didn't stop. Kill the clock!\n");
		set32(clk_base + CLK_PCMCTL,
			PCM_CLK_KILL | PCM_CLK_PASSWD_MASK,
			PCM_CLK_KILL | PCM_CLK_PASSWD);
	}
}

int drv_stop(int sub_dev)
{
	if(running) {
		log_debug(&log, "drv_stop start\n");
		pcm_set(PCM_CS_A, PCM_CS_A_TXON, 0);
		running = FALSE;
		drv_reenable_int(sub_dev);

		clock_stop();
	}
	return OK;
}

int drv_set_dma(u32_t dma, u32_t UNUSED(length), int UNUSED(chan))
{
	log_debug(&log, "drv_set_dma starts\n");
	DmaPhys = dma;
	return OK;
}

int drv_reenable_int(int UNUSED(chan))
{
	log_debug(&log, "drv_reenable_int starts\n");
	pcm_write(PCM_INTEN_A, PCM_INTEN_A_TXW | PCM_INTEN_A_TXERR);
	return OK;
}

int drv_pause(int chan)
{
	drv_stop(chan);
	return OK;
}

int drv_resume(int UNUSED(chan))
{
	clock_start();
	pcm_set(PCM_CS_A, PCM_CS_A_TXON, PCM_CS_A_TXON);
	return OK;
}

int drv_io_ctl(unsigned long request, void *val, int *len, int sub_dev)
{
	log_debug(&log, "got ioctl %lu, argument: %d sub_dev: %d\n",
		request, val, sub_dev);

	if(sub_dev == AUDIO)
		return dsp_ioctl(request, val, len);

	return EIO;
}

int drv_get_irq(char *irq)
{
	log_debug(&log, "drv_get_irq starts\n");
	*irq = PCM_IRQ;
	return OK;
}

int drv_get_frag_size(u32_t *frag_size, int UNUSED(sub_dev))
{
	log_debug(&log, "drv_get_frag_size starts\n");
	*frag_size = DspFragmentSize;
	return OK;
}

int drv_int(int sub_dev) {
	return sub_dev == AUDIO;
}

static int dsp_ioctl(unsigned long request, void *val, int *len)
{
	int status;
	
	switch(request) {
		case DSPIORATE:		status = dsp_set_speed(*((u32_t*) val)); break;
		case DSPIOSTEREO:	status = dsp_set_stereo(*((u32_t*) val)); break;
		case DSPIOSIZE:		status = dsp_set_size(*((u32_t*) val)); break;
		case DSPIOBITS:		status = dsp_set_bits(*((u32_t*) val)); break;
		case DSPIOSIGN:		status = dsp_set_sign(*((u32_t*) val)); break;
		case DSPIOMAX:		status = dsp_get_max_frag_size(val, len); break;
		case DSPIORESET:    status = drv_reset(); break;
		default:            status = ENOTTY; break;
	}

	return status;
}

static void dsp_dma_setup(int DmaMode)
{
	pvb_pair_t pvb[9];

	log_debug(&log, "Setting up %d bit DMA\n", DspBits);
	if(DspBits == 16 || DspBits == 8) {   /* 16 bit sound */
		pcm_set(PCM_CS_A, PCM_CS_A_DMAEN, 0); 
		write32(dma_base + DMA_CS, DMA_CS_RESET);
		set32(dma_base + DMA_CS, DMA_CS_END | DMA_CS_INT, 0xffffffff);
		write32(dma_base + DMA_CONBLK, (uint32_t)ctrl_blk);
		pcm_set(PCM_DREQ_A, PCM_TX_PANIC(0x10) |
			PCM_RX_PANIC(0x30) | PCM_TX(0x30) | PCM_RX(0x20), 0xffffffff);

		pcm_set(PCM_CS_A, PCM_CS_A_DMAEN, PCM_CS_A_DMAEN);

		set32(dma_base + DMA_CS, DMA_CS_ACTIVE, DMA_CS_ACTIVE);
	}
}

static int dsp_set_size(unsigned int size)
{
	log_debug(&log, "set fragment size to %u\n", size);

	/* Sanity checks */
	if(size < sub_dev[AUDIO].MinFragmentSize 
		|| size > sub_dev[AUDIO].DmaSize / sub_dev[AUDIO].NrOfDmaFragments 
		|| size % 2 != 0) {
		return EINVAL;
	}

	DspFragmentSize = size; 

	return OK;
}

static int dsp_set_speed(unsigned int speed)
{
	log_debug(&log, "setting speed to %u, stereo = %d\n", speed, DspStereo);

	if(speed < DSP_MIN_SPEED || speed > DSP_MAX_SPEED) {
		return EPERM;
	}

	DspSpeed = speed;

	return OK;
}

static int dsp_set_stereo(unsigned int stereo)
{
	if(stereo) { 
		DspStereo = 1;
	} else { 
		DspStereo = 0;
	}

	return OK;
}

static int dsp_set_bits(unsigned int bits)
{
	/* Sanity checks */
	if(bits != 16 && bits != 8) {
		return EINVAL;
	}

	DspBits = bits; 

	return OK;
}

static int dsp_set_sign(unsigned int sign)
{
	log_debug(&log, "set sign to %u\n", sign);

	DspSign = (sign > 0 ? 1 : 0); 

	return OK;
}

static int dsp_get_max_frag_size(u32_t *val, int *len)
{
	*len = sizeof(*val);
	*val = sub_dev[AUDIO].DmaSize / sub_dev[AUDIO].NrOfDmaFragments;
	return OK;
}

void
pcm_set(vir_bytes reg, uint32_t mask, uint32_t value)
{
	if(reg < 0 || reg > I2S_REG_SIZE) {
		log_warn(&log, "Non-existent register");
		return;
	}
	set32(io_base + reg, mask, value);
}

uint32_t
pcm_read(vir_bytes reg)
{
	if(reg < 0 || reg > I2S_REG_SIZE) {
		log_warn(&log, "Non-existent register");
		return 0; 
	}
	return read32(io_base + reg);
}

void
pcm_write(vir_bytes reg, uint32_t value)
{
	if(reg < 0 || reg > I2S_REG_SIZE) {
		log_warn(&log, "Non-existent register");
		return;
	}
	write32(io_base + reg, value);
}