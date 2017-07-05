#include <minix/drivers.h>
#include <minix/chardriver.h>
#include <minix/type.h>

#include "mbox.h"

#define NR_DEVS     	1	/* number of minor devices */
#define MAILBOX_DEV  	0	/* minor device for /dev/mailbox */

/* SEF functions and variables. */
static void sef_local_startup(void);
static int sef_cb_init_fresh(int type, sef_init_info_t *info);

static ssize_t m_read(devminor_t minor, u64_t position, endpoint_t endpt,
	cp_grant_id_t grant, size_t size, int flags, cdev_id_t id);
static ssize_t m_write(devminor_t minor, u64_t position, endpoint_t endpt,
	cp_grant_id_t grant, size_t size, int flags, cdev_id_t id);
static int m_open(devminor_t minor, int access, endpoint_t user_endpt);
static int m_select(devminor_t, unsigned int, endpoint_t);

/* Entry points to this driver. */
static struct chardriver m_dtab = {
  .cdr_open		= m_open,	/* open device */
  .cdr_read		= m_read,	/* read from device */
  .cdr_write	= m_write,	/* write to device (seeding it) */
  .cdr_select	= m_select,	/* select hook */
};

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
}

static ssize_t m_write(devminor_t minor, u64_t position, endpoint_t endpt,
	cp_grant_id_t grant, size_t size, int flags, cdev_id_t id)
{
	/* Write to one of the driver's minor devices. */
	size_t offset, chunk;
	int r;

	if (minor != MAILBOX_DEV) 
		return(EIO);
}

static int m_open(devminor_t minor, int access, endpoint_t user_endpt)
{
	if (minor < 0 || minor >= NR_DEVS) 
		return(ENXIO);
}

static int m_select(devminor_t minor, unsigned int ops, endpoint_t endpt)
{
	/* mailbox device is always writable; it's infinitely readable
	 * once seeded, and doesn't block when it's not, so all operations
	 * are instantly possible. we ignore CDEV_OP_ERR.
	 */
	int ready_ops = 0;
	if (minor != MAILBOX_DEV) 
		return(EIO);
	return ops & (CDEV_OP_RD | CDEV_OP_WR);
}