#include <minix/drivers.h>
#include <minix/chardriver.h>
#include <minix/type.h>
#include <minix/vm.h>
#include <sys/mman.h>
#include <minix/log.h>

#include "mbox.h"

#define NR_DEVS     	1	/* number of minor devices */
#define MAILBOX_DEV  	0	/* minor device for /dev/mailbox */

#define SANE_TIMEOUT 500000	/* 500 ms */

#define MBOX_IRQ 33

/* SEF functions and variables. */
static void sef_local_startup(void);
static int sef_cb_init_fresh(int type, sef_init_info_t *info);

static ssize_t m_read(devminor_t minor, u64_t position, endpoint_t endpt,
	cp_grant_id_t grant, size_t size, int flags, cdev_id_t id);
static ssize_t m_write(devminor_t minor, u64_t position, endpoint_t endpt,
	cp_grant_id_t grant, size_t size, int flags, cdev_id_t id);
static int m_open(devminor_t minor, int access, endpoint_t user_endpt);
static int m_close(devminor_t UNUSED(minor));

static u8_t* mboxbuffer_vir;	/* Address of dss phys memory map */
static phys_bytes mboxbuffer_phys;

/* Entry points to this driver. */
static struct chardriver m_dtab = {
  .cdr_open		= m_open,	/* open device */
  .cdr_close	= m_close,
  .cdr_read		= m_read,	/* read from device */
  .cdr_write	= m_write,	/* write to device (seeding it) */
};

static struct log log = {
	.name = "mailbox",
	.log_level = LEVEL_INFO,
	.log_func = default_log
};

static int hook_id = 1;
/*===========================================================================*
 *				   main 				     *
 *===========================================================================*/
int main(void)
{
	/* SEF local startup. */
	sef_local_startup();
	
	/* Call the generic receive loop. */
	chardriver_task(&m_dtab);
	
	return(OK);
}

/*===========================================================================*
 *			       sef_local_startup			     *
 *===========================================================================*/
static void sef_local_startup()
{
	/* Register init callbacks. */
	sef_setcb_init_fresh(sef_cb_init_fresh);
	sef_setcb_init_restart(sef_cb_init_fresh);
	
	/* Let SEF perform startup. */
	sef_startup();
}

/*===========================================================================*
 *		            sef_cb_init_fresh                                *
 *===========================================================================*/
static int sef_cb_init_fresh(int UNUSED(type), sef_init_info_t *UNUSED(info))
{
	/* Initialize the random driver. */
	static struct k_randomness krandom;
	int i, s;

	mailbox_init();

	if (sys_irqsetpolicy(MBOX_IRQ, 0, &hook_id) != OK) {
		log_warn(&log, "mailbox: couldn't set IRQ policy %d\n",
		    MBOX_IRQ);
		return(EPERM);
	}

	mbox_flush();

	/* Configure mailbox buffer */
	mboxbuffer_vir = (u8_t*) alloc_contig(0x1000, 0, &mboxbuffer_phys);
	if (mboxbuffer_vir == (u8_t*) MAP_FAILED) {
		panic("Unable to allocate contiguous memory for mailbox buffer\n");
	}

	/* Announce we are up! */
	chardriver_announce();

	return(OK);
}

static ssize_t m_read(devminor_t minor, u64_t position, endpoint_t endpt,
	cp_grant_id_t grant, size_t size, int flags, cdev_id_t id)
{
	/* Read from one of the driver's minor devices. */
	size_t offset, chunk;
	int r;

	if (minor != MAILBOX_DEV) 
		return(EIO);
	
	mbox_read(MBOX_PROP);
	r = sys_safecopyto(endpt, grant, 0, (vir_bytes)mboxbuffer_vir, size);
	if (r != OK) {
		log_warn(&log, "mailbox: sys_safecopyto failed for proc %d, grant %d\n",
			endpt, grant);
		return r;
	}

	mbox_flush();

	return(OK);
}


static int wait_irq()
{
	if (sys_irqenable(&hook_id) != OK)
		log_warn(&log, "Failed to enable irq\n");

	/* Wait for a task completion interrupt. */
	message m;
	int ipc_status;
	int ticks = SANE_TIMEOUT * sys_hz() / 1000000;

	if (ticks <= 0)
		ticks = 1;
	while (1) {
		int rr;
		sys_setalarm(ticks, 0);
		if ((rr = driver_receive(ANY, &m, &ipc_status)) != OK) {
			panic("driver_receive failed: %d", rr);
		};
		if (is_ipc_notify(ipc_status)) {
			switch (_ENDPOINT_P(m.m_source)) {
			case CLOCK:
				/* Timeout. */
				log_warn(&log, "TIMEOUT\n");
				return -1;
				break;
			case HARDWARE:
				log_debug(&log, "HARDWARE\n");
				sys_setalarm(0, 0);
				return 0;
			}
		}
	}
	sys_setalarm(0, 0);	/* cancel the alarm */

}

static ssize_t m_write(devminor_t minor, u64_t position, endpoint_t endpt,
	cp_grant_id_t grant, size_t size, int flags, cdev_id_t id)
{
	/* Write to one of the driver's minor devices. */
	int r;
	uint32_t msg;
	

	if (minor != MAILBOX_DEV) 
		return(EIO);

	r = sys_safecopyfrom(endpt, grant, 0, (vir_bytes)mboxbuffer_vir, size);
	if (r != OK) {
		log_warn(&log, "mailbox: sys_safecopyfrom failed for proc %d,"
			" grant %d\n", endpt, grant);
		return r;
	}
	mbox_write(MBOX_PROP, (uint32_t)mboxbuffer_phys + 0x40000000);

	if (wait_irq() < 0) {
		log_warn(&log, "can't wait interrupt from mbox\n");
		return(ETIME);
	}

	return(OK);
}

static int m_open(devminor_t minor, int access, endpoint_t user_endpt)
{
	if (minor < 0 || minor >= NR_DEVS) 
		return(ENXIO);
}

static int m_close(devminor_t UNUSED(minor))
{
    return (OK);
}
