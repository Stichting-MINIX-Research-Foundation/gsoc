#include <minix/drivers.h>
#include <minix/chardriver.h>
#include <minix/type.h>
#include <minix/log.h>
#include <minix/vm.h>
#include <minix/spin.h>
#include <sys/mman.h>

#include <assert.h>

#include "mbox.h"

struct mailbox_t mailbox = {
	.read = 0x00,
	.res1 = 0x04,
	.res2 = 0x08,
	.res3 = 0x0c,
	.peek = 0x10,
	.sender = 0x14,
	.status = 0x18,
	.config = 0x1c,
	.write = 0x20
};

struct mailbox_t *gmailbox;

uint32_t mbox_base;
/*
 * Define a structure to be used for logging
 */
static struct log log = {
	.name = "mailbox",
	.log_level = LEVEL_INFO,
	.log_func = default_log
};

static void write32(vir_bytes reg, uint32_t data)
{
	assert(reg >= 0 && reg <= sizeof (struct mailbox_t));
	uint32_t addr = mbox_base + reg;
	addr = data;
}

static uint32_t read32(vir_bytes reg)
{
	assert(reg >= 0 && reg <= sizeof (struct mailbox_t));
	return mbox_base + reg;
}

void mailbox_init()
{
	uint32_t value;
	value = 0;
	struct minix_mem_range mr;

	mr.mr_base = MBOX_BASE;
	mr.mr_limit = MBOX_BASE + sizeof (struct mailbox_t);

	/* grant ourself rights to map the register memory */
	if (sys_privctl(SELF, SYS_PRIV_ADD_MEM, &mr) != 0) {
		panic("Unable to request permission to map memory");
	}

	/* Set the base address to use */
	mbox_base =
	    (uint32_t) vm_map_phys(SELF, (void *) MBOX_BASE,
	    sizeof (struct mailbox_t));

	if (mbox_base == (uint32_t) MAP_FAILED)
		panic("Unable to map MMC memory");

	gmailbox = &mailbox;
}

uint32_t mbox_read(uint8_t chan)
{
	uint32_t data = 0;
	while (1) {
		while (read32(gmailbox->status) & MAILBOX_EMPTY) {
			log_debug(&log, "status: 0x%x\n", read32(gmailbox->status));
			//mem barrier
		}
		data = read32(gmailbox->read);

		log_debug(&log, "received data %d\n", data);

		if (((uint8_t) data & 0xf) == chan) {
			return (data & ~0xf);
		}
	}
}

void mbox_write(uint8_t chan, uint32_t data)
{
	while (read32(gmailbox->status) & MAILBOX_FULL) ;

	write32(gmailbox->write, (data & ~0xf) | (uint32_t) (chan & 0xf));
}

void mbox_flush(void) 
{
	spin_t spin;
	while (!(read32(gmailbox->status) & MAILBOX_EMPTY)) {
		spin_init(&spin, 20000);
		while (1) {
			if(spin_check(&spin) == FALSE)
				break;
		}
	}
}