#include <minix/drivers.h>
#include <minix/chardriver.h>
#include <stdio.h>
#include <stdlib.h>
#include <minix/ds.h>
#include <minix/log.h>
#include <minix/com.h>
#include <minix/vm.h>
#include <minix/mmio.h>
#include <minix/type.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <minix/padconf.h>

#include <inttypes.h>

#include "spi.h"

/*
 * Function prototypes for the spi driver.
 */
static int spi_open(devminor_t minor, int access, endpoint_t user_endpt);
static int spi_close(devminor_t minor);
static ssize_t spi_read(devminor_t minor, u64_t position, endpoint_t endpt,
	cp_grant_id_t grant, size_t size, int flags, cdev_id_t id);
static ssize_t spi_write(devminor_t minor, u64_t position, endpoint_t endpt,
	cp_grant_id_t grant, size_t size, int flags, cdev_id_t id);
static int spi_ioctl(devminor_t UNUSED(minor), unsigned long request, endpoint_t endpt,
	cp_grant_id_t grant, int UNUSED(flags), endpoint_t UNUSED(user_endpt),
	cdev_id_t UNUSED(id));

#define COPYBUF_SIZE 0x1000	/* 4k buff */
static unsigned char copybuf[COPYBUF_SIZE];

/* SEF functions and variables. */
static void sef_local_startup(void);
static int sef_cb_init(int type, sef_init_info_t *info);
static int sef_cb_lu_state_save(int, int);
static int lu_state_restore(void);

static char io_ctl_buf[IOCPARM_MASK];

static int spi_set_mode(unsigned int mode);
static int spi_set_speed(unsigned int speed);

/* Entry points to the spi driver. */
static struct chardriver spi_tab =
{
	.cdr_open	= spi_open,
	.cdr_close	= spi_close,
	.cdr_read	= spi_read,
	.cdr_write  = spi_write,
	.cdr_ioctl	= spi_ioctl,
};

typedef struct
{
	vir_bytes CS;
	vir_bytes FIFO;
	vir_bytes CLK;
	vir_bytes DLEN;
	vir_bytes LTOH;
	vir_bytes DC;
} spi_regs_t;

static spi_regs_t spi_reg = {
	.CS = SPI_CS,
	.FIFO = SPI_FIFO,
	.CLK = SPI_CLK,
	.DLEN = SPI_DLEN,
	.LTOH = SPI_LTOH,
	.DC = SPI_DC,
};

vir_bytes io_base;
spi_regs_t *rpi_spi_bus;
int spi_bus_id;

/*
 * Define a structure to be used for logging
 */
static struct log log = {
	.name = "spi",
	.log_level = LEVEL_INFO,
	.log_func = default_log
};

#define	div_roundup(x, y) (((x)+((y)-1))/(y))

#define roundup2pow(v) \
	do { \
    	v--; \
    	v |= v >> 1; \
    	v |= v >> 2; \
    	v |= v >> 4; \
    	v |= v >> 8; \
    	v |= v >> 16; \
    	v++; \
	} while(0)

static int spi_reset()
{
	/* Disable interrupts */
	spi_set32(rpi_spi_bus->CS, SPI_CS_INTR | SPI_CS_INTD | SPI_CS_TA, 0);
	/* Clear fifo */
	spi_set32(rpi_spi_bus->CS, SPI_CS_RX_CLEAR | SPI_CS_TX_CLEAR, 
		SPI_CS_RX_CLEAR | SPI_CS_TX_CLEAR);

	spi_write32(rpi_spi_bus->DLEN, 0);
	return OK;
}

static void spi_padconf()
{
	int r;
	uint32_t pinopts;
	switch(spi_bus_id) {
		case 0:
			pinopts = CONTROL_BCM_CONF_SPI0_MISO |
				CONTROL_BCM_CONF_SPI0_CE1 | CONTROL_BCM_CONF_SPI0_CE0;
	
			r = sys_padconf(GPFSEL09, pinopts,
				pinopts);
			if (r != OK) {
				log_warn(&log, "sys_padconfnf failed (r=%d)\n", r);
			}

			log_debug(&log, "pinopts=0x%x\n", pinopts);

			pinopts = CONTROL_BCM_CONF_SPI0_MOSI | CONTROL_BCM_CONF_SPI0_SCLK;
			r = sys_padconf(GPFSEL1019, pinopts,
				pinopts);
			if (r != OK) {
				log_warn(&log, "padconf failed (r=%d)\n", r);
			}
	
			log_debug(&log, "pinopts=0x%x\n", pinopts);
			break;
		case 1:
			pinopts = CONTROL_BCM_CONF_SPI1_MISO |
				CONTROL_BCM_CONF_SPI1_CE1 | CONTROL_BCM_CONF_SPI1_CE0 |
				CONTROL_BCM_CONF_SPI1_CE2;
	
			r = sys_padconf(GPFSEL1019, pinopts,
				pinopts);
			if (r != OK) {
				log_warn(&log, "sys_padconfnf failed (r=%d)\n", r);
			}

			log_debug(&log, "pinopts=0x%x\n", pinopts);

			pinopts = CONTROL_BCM_CONF_SPI1_MOSI | CONTROL_BCM_CONF_SPI1_SCLK;
			r = sys_padconf(GPFSEL2029, pinopts,
		    	pinopts);
			if (r != OK) {
				log_warn(&log, "padconf failed (r=%d)\n", r);
			}
	
			log_debug(&log, "pinopts=0x%x\n", pinopts);
			break;
		case 2:
			pinopts = CONTROL_BCM_CONF_SPI2_MISO | CONTROL_BCM_CONF_SPI2_CE2 |
				CONTROL_BCM_CONF_SPI2_CE1 | CONTROL_BCM_CONF_SPI2_CE0 |
				CONTROL_BCM_CONF_SPI2_MOSI | CONTROL_BCM_CONF_SPI2_SCLK;
	
			r = sys_padconf(GPFSEL4049, pinopts,
				pinopts);
			if (r != OK) {
				log_warn(&log, "sys_padconfnf failed (r=%d)\n", r);
			}

			log_debug(&log, "pinopts=0x%x\n", pinopts);

			break;
	}
}

static int spi_open(devminor_t UNUSED(minor), int UNUSED(access),
	endpoint_t UNUSED(user_endpt))
{
	log_debug(&log, "spi_open\n");
		/* Clear fifo */
	spi_set32(rpi_spi_bus->CS, SPI_CS_RX_CLEAR | SPI_CS_TX_CLEAR, 
		SPI_CS_RX_CLEAR | SPI_CS_TX_CLEAR);
	return OK;
}

static int spi_close(devminor_t UNUSED(minor))
{
	log_debug(&log, "spi_close\n");
	spi_reset();
	return OK;
}

static uint32_t
spi_poll(uint32_t mask)
{
	spin_t spin;
	uint32_t status;

	/* poll for up to 1 s */
	spin_init(&spin, 1000000);
	do {
		status = spi_read32(rpi_spi_bus->CS);
		if ((status & mask) != 0) {	/* any bits in mask set */
			return status;
		}
	} while (spin_check(&spin));

	return status;		/* timeout reached, abort */
}

static int spi_read_bytes(uint32_t size)
{
	int i;
	uint32_t status;
	uint32_t pollmask;

	for(i = 0; i < size; i++) {
		pollmask = SPI_CS_RXD;
		status = spi_poll(pollmask);
		if ((pollmask & status) == 0) {
			log_warn(&log, "Can't wait RXD interrupt. Status = 0x%08x\n", status);
			return (-EIO);
		}
		copybuf[i] = spi_read32(rpi_spi_bus->FIFO);
		
		pollmask = SPI_CS_DONE;
		status = spi_poll(pollmask);
		if ((pollmask & status) == 0) {
			log_warn(&log, "Can't wait DONE interrupt. Status = 0x%08x\n", status);
			return (-EIO);
		}
	}
	return i;
}

static int spi_write_bytes(uint32_t size)
{
	int i;
	uint32_t status;
	uint32_t pollmask;

	for(i = 0; i < size; i++) {
		pollmask = SPI_CS_TXD;
		status = spi_poll(pollmask);
		if ((pollmask & status) == 0) {
			log_warn(&log, "Can't wait TXD interrupt. Status = 0x%08x\n", status);
			return (-EIO);
		}
		spi_write32(rpi_spi_bus->FIFO, copybuf[i]);
		
		pollmask = SPI_CS_DONE;
		status = spi_poll(pollmask);
		if ((pollmask & status) == 0) {
			log_warn(&log, "Can't wait DONE interrupt. Status = 0x%08x\n", status);
			return (-EIO);
		}
	}
	return i;
}

static int spi_transfer(uint32_t size, int mode)
{
	int retv;

	spi_set32(rpi_spi_bus->CS, SPI_CS_CS, 0);
	spi_set32(rpi_spi_bus->CS, SPI_CS_MODE, SPI_CS_MODE0);

	/* Start transfer */
	spi_set32(rpi_spi_bus->CS, SPI_CS_TA, SPI_CS_TA);

	log_debug(&log, "size %d\n", size);

	if (mode == WRITE_MODE) {
		spi_set32(rpi_spi_bus->CS, SPI_CS_REN, 0);
		retv = spi_write_bytes(size);
	} else {
		spi_set32(rpi_spi_bus->CS, SPI_CS_REN, SPI_CS_REN);
		retv = spi_read_bytes(size);
	}

	if (retv < 0) {
		log_warn(&log, "Transfer failed\n");
		return (-EIO);
	}

	/* Stop transfer */
	spi_set32(rpi_spi_bus->CS, SPI_CS_TA, 0);

	return retv;
}

static ssize_t spi_read(devminor_t UNUSED(minor), u64_t position,
	endpoint_t endpt, cp_grant_id_t grant, size_t size, int UNUSED(flags),
	cdev_id_t UNUSED(id))
{
	int retv;
	log_debug(&log, "spi_read\n");

	if(size < 0 || size > COPYBUF_SIZE) {
		log_warn(&log, "illegal size\n");
		return EINVAL;
	}

	retv = spi_transfer(size, READ_MODE);

	ssize_t ret;
	if ((ret = sys_safecopyto(endpt, grant, 0, (vir_bytes) copybuf, size)) != OK)
		return ret;

	return retv;
}


static ssize_t spi_write(devminor_t UNUSED(minor), u64_t position,
	endpoint_t endpt, cp_grant_id_t grant, size_t size, int UNUSED(flags),
	cdev_id_t UNUSED(id))
{
	int retv;
	log_debug(&log, "spi_write\n");

	if(size < 0 || size > COPYBUF_SIZE) {
		log_warn(&log, "illegal size\n");
		return EINVAL;
	}
	
	ssize_t ret;
	if ((ret = sys_safecopyfrom(endpt, grant, 0, (vir_bytes) copybuf, size)) != OK)
		return ret;

	retv = spi_transfer(size, WRITE_MODE);

	return retv;
}

static int spi_set_mode(unsigned int mode)
{
	switch(mode) {
		case SPI_CS_MODE0:
		case SPI_CS_MODE1:
		case SPI_CS_MODE2:
		case SPI_CS_MODE3:
			spi_set32(rpi_spi_bus->CS, SPI_CS_MODE, mode);
			break;
		default:
			return EIO;
	}
	return OK;
}

static int spi_set_speed(unsigned int speed)
{
	if (speed > SPI_DEF_SPEED || speed < 0)
		return EIO;

	unsigned int div = div_roundup(SPI_DEF_SPEED, speed);

	roundup2pow(div);
	log_debug(&log, "div %d\n", div);
	
	spi_write32(rpi_spi_bus->CLK, div);

	return OK;
}

static int spi_ioctl(devminor_t UNUSED(minor), unsigned long request, endpoint_t endpt,
	cp_grant_id_t grant, int UNUSED(flags), endpoint_t UNUSED(user_endpt),
	cdev_id_t UNUSED(id))
{
	if (request & IOC_IN) { /* if there is data for us, copy it */
		
		if (sys_safecopyfrom(endpt, grant, 0, (vir_bytes)io_ctl_buf,
		    4) != OK) {
			printf("%s:%d: safecopyfrom failed\n", __FILE__, __LINE__);
		}
	}

	int status;
	
	switch(request) {
		case SPIIORATE:		status = spi_set_speed(*(u32_t *)io_ctl_buf); break;
		case SPIIOMODE:		status = spi_set_mode(*(u32_t *)io_ctl_buf); break;
		default:            status = ENOTTY; break;
	}

	return status;

}

static void sef_local_startup()
{
	/*
	 * Register init callbacks. Use the same function for all event types
	 */
	sef_setcb_init_fresh(sef_cb_init);
	sef_setcb_init_lu(sef_cb_init);
	sef_setcb_init_restart(sef_cb_init);

	/* Let SEF perform startup. */
	sef_startup();
}

static int sef_cb_init(int type, sef_init_info_t *UNUSED(info))
{
	spi_reset();

	/* Enable interrupts */
	spi_set32(rpi_spi_bus->CS, SPI_CS_INTR | SPI_CS_INTD | SPI_CS_TA,
		SPI_CS_INTR | SPI_CS_INTD | SPI_CS_TA);

	chardriver_announce();

	/* Initialization completed successfully. */
	return OK;
}

static int
env_parse_instance(void)
{
	int r;
	long instance;

	/* Parse the instance number passed to service */
	instance = 0;
	r = env_parse("instance", "d", 0, &instance, 1, 3);
	if (r == -1) {
		log_warn(&log,
		    "Expecting '-arg instance=N' argument (N=1..3)\n");
		return EXIT_FAILURE;
	}

	/* Device files count from 1, hardware starts counting from 0 */
	spi_bus_id = instance - 1;

	return OK;
}

int main(int argc, char *argv[])
{
	struct minix_mem_range mr;
	int r;
	env_setargs(argc, argv);

	r = env_parse_instance();
	if (r != OK) {
		return r;
	}
	uint32_t bus_base;
	switch(spi_bus_id) {
		case 0:
			bus_base = SPI0_BASE;	/* start addr */
			break;
		case 1:
			bus_base = SPI1_BASE;	/* start addr */
			break;
		case 2:
			bus_base = SPI2_BASE;	/* start addr */
			break;
	}

	/* Configure memory access */
	mr.mr_base = bus_base;
	mr.mr_limit = mr.mr_base + SPI_REG_SIZE;	/* end addr */

	/* ask for privileges to access the SPI memory range */
	if (sys_privctl(SELF, SYS_PRIV_ADD_MEM, &mr) != OK) {
		panic("Unable to obtain spi memory range privileges");
	}

	/* map the memory into this process */
	io_base = (vir_bytes) vm_map_phys(SELF,
	    (void *) bus_base, SPI_REG_SIZE);

	if (io_base == (vir_bytes) MAP_FAILED) {
		panic("Unable to map spi registers");
	}

	uint32_t hook_id = 1;
	if (sys_irqsetpolicy(SPI_IRQ, 0, &hook_id) != OK) {
		log_warn(&log, "couldn't set IRQ policy %d\n",
		    SPI_IRQ);
		return 1;
	}

	rpi_spi_bus = &spi_reg;

	spi_padconf();
	/*
	 * Perform initialization.
	 */
	sef_local_startup();

	/*
	 * Run the main loop.
	 */
	chardriver_task(&spi_tab);
	log_debug(&log, "Started\n");
	return OK;
}

void
spi_set32(vir_bytes reg, uint32_t mask, uint32_t value)
{
	if(reg < 0 || reg > SPI_REG_SIZE) {
		log_warn(&log, "Non-existent register");
		return;
	}
	set32(io_base + reg, mask, value);
}

uint32_t
spi_read32(vir_bytes reg)
{
	if(reg < 0 || reg > SPI_REG_SIZE) {
		log_warn(&log, "Non-existent register");
		return 0; 
	}
	return read32(io_base + reg);
}

void
spi_write32(vir_bytes reg, uint32_t value)
{
	if(reg < 0 || reg > SPI_REG_SIZE) {
		log_warn(&log, "Non-existent register");
		return;
	}
	write32(io_base + reg, value);
}

