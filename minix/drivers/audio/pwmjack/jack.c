#include <minix/com.h>
#include <minix/vm.h>
#include <minix/mmio.h>
#include <minix/type.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <minix/padconf.h>

#include <inttypes.h>

#include "pwm.h"

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
	.name = "jack",
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
phys_bytes ctrl_blk;
phys_bytes ph;
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

	mr.mr_base = CLK_BASE + CLK_PWMCTL;
	mr.mr_limit = CLK_BASE + CLK_PWMCTL + 8;

	/* grant ourself rights to map the register memory */
	if (sys_privctl(SELF, SYS_PRIV_ADD_MEM, &mr) != 0) {
		panic("Unable to request permission to map memory");
	}

	/* Set the base address to use */
	clk_base =
	    (uint32_t) vm_map_phys(SELF, (void *) (CLK_BASE + CLK_PWMCTL),
	    8);

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
	    (uint32_t) vm_map_phys(SELF, (void *) (DMA_BASE + DMA_REG_SIZE * DMA_CHAN),
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
pwm_padconf()
{
	log_debug(&log, "padconf starts\n");

	int r;
	uint32_t pinopts = CONTROL_BCM_CONF_PWM0_JACK
		| CONTROL_BCM_CONF_PWM1_JACK;

	r = sys_padconf(GPFSEL4049, pinopts, pinopts);
	if (r != OK) {
		log_warn(&log, "padconf failed (r=%d)\n", r);
	}
	
	log_debug(&log, "pinopts=0x%x\n", pinopts);
}	

static void clear_fifo()
{
	pwm_set(PWM_CTL, PWM_CTL_CLRF1, PWM_CTL_CLRF1);
}

static void set_speed()
{
	uint32_t divider;
	uint32_t bclk_rate;
	uint32_t cur_freq = CLK_FREQ << CLK_SHIFT;

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

	write32(clk_base + 4, CLK_PASSWD
			| CLK_DIVI(divider >> CLK_SHIFT)
			| CLK_DIVF(divider & CLK_DIVF_MASK));

	write32(clk_base, CLK_PASSWD
			| CLK_SRC(5) | CLK_SRC(1));
}

int drv_init_hw(void) 
{
	int i;
	log_debug(&log, "drv_init_hw starts\n");
	struct minix_mem_range mr;

	mr.mr_base = PWM_BASE;
	mr.mr_limit = PWM_BASE + PWM_REG_SIZE;

	/* grant ourself rights to map the register memory */
	if (sys_privctl(SELF, SYS_PRIV_ADD_MEM, &mr) != 0) {
		panic("Unable to request permission to map memory");
	}

	/* Set the base address to use */
	io_base =
	    (uint32_t) vm_map_phys(SELF, (void *) PWM_BASE,
	    PWM_REG_SIZE);

	if (io_base == (uint32_t) MAP_FAILED)
		panic("Unable to map PWM memory");

	config_clk();

	config_dma();

	pwm_padconf();

	pwm_write(PWM_RNG1, 0x1b4);
	pwm_write(PWM_RNG2, 0x1b4);

	DspFragmentSize = sub_dev[AUDIO].DmaSize / sub_dev[AUDIO].NrOfDmaFragments;

	return OK;
}

int drv_reset(void) 
{
	int i;
	log_debug(&log, "drv_reset starts\n");
	clock_stop();
	clear_fifo();
	pwm_write(PWM_STA, ERRMASK);

	return OK;
}

int drv_start(int channel, int DmaMode) 
{
	int i;

	log_debug(&log, "drv_start starts\n");

	ctrl_blk_t ctrl;
	ctrl.SRC_AD = DmaPhys;
	ctrl.DST_AD = PWM_BASE + PWM_FIF1;
	ctrl.TXR_LEN = DspFragmentSize * sub_dev[channel].NrOfDmaFragments;
	ctrl.TI = PERMAP_SET(DREQ_PWM) | DMA_TI_DST_DREQ 
		| DMA_TI_INTEN | DMA_TI_WAIT_RESP | DMA_TI_SRC_INC;
	ctrl.NEXTCONBK = ctrl_blk;
	memcpy(blk, &ctrl, sizeof(ctrl_blk_t));

	dsp_set_speed(DspSpeed);

	pwm_write(PWM_CTL, PWM_CTL_USEF2
		| PWM_CTL_PWEN2
		| PWM_CTL_USEF1
		| PWM_CTL_PWEN1
		| PWM_CTL_CLRF1);

	clock_start();

	running = TRUE;

	dsp_dma_setup(DmaMode);

	return OK;
}


static void clock_start()
{
	set32(clk_base, 
		CLK_PASSWD_MASK | CLK_ENAB,
		CLK_PASSWD | CLK_ENAB);
}

static void clock_stop()
{
	int timeout = 1000;
	uint32_t clkreg;

	set32(clk_base,
		CLK_PASSWD_MASK | CLK_ENAB,
		CLK_PASSWD);

	while (--timeout) {
		clkreg = read32(clk_base);
		if (!(clkreg & CLK_BUSY))
			break;
	}

	if (!timeout) {
		/* KILL the clock */
		log_warn(&log, "PWM clock didn't stop. Kill the clock!\n");
		set32(clk_base,
			CLK_KILL | CLK_PASSWD_MASK,
			CLK_KILL | CLK_PASSWD);
	}
}

int drv_stop(int sub_dev)
{
	if(running) {
		log_debug(&log, "drv_stop start\n");
		pwm_set(PWM_DMAC, PWM_DMAC_ENAB, 0);
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
	running = TRUE;
	pwm_set(PWM_DMAC, PWM_DMAC_ENAB, PWM_DMAC_ENAB);
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
	*irq = PWM_IRQ;
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
	log_debug(&log, "Setting up %d bit DMA\n", DspBits);
	write32(dma_base + DMA_CS, DMA_CS_RESET);
	set32(dma_base + DMA_CS, DMA_CS_END | DMA_CS_INT, 0xffffffff);
	write32(dma_base + DMA_CONBLK, (uint32_t)ctrl_blk);
	
	pwm_write(PWM_DMAC, PWM_DMAC_DREQ(0xf));
	pwm_set(PWM_DMAC, PWM_DMAC_ENAB, PWM_DMAC_ENAB);
	
	set32(dma_base + DMA_CS, DMA_CS_ACTIVE, DMA_CS_ACTIVE);
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
pwm_set(vir_bytes reg, uint32_t mask, uint32_t value)
{
	if(reg < 0 || reg > PWM_REG_SIZE) {
		log_warn(&log, "Non-existent register");
		return;
	}
	set32(io_base + reg, mask, value);
}

uint32_t
pwm_read(vir_bytes reg)
{
	if(reg < 0 || reg > PWM_REG_SIZE) {
		log_warn(&log, "Non-existent register");
		return 0; 
	}
	return read32(io_base + reg);
}

void
pwm_write(vir_bytes reg, uint32_t value)
{
	if(reg < 0 || reg > PWM_REG_SIZE) {
		log_warn(&log, "Non-existent register");
		return;
	}
	write32(io_base + reg, value);
}