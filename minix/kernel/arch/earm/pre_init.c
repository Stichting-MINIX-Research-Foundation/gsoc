#include "kernel/kernel.h"
#include <assert.h>
#include <stdlib.h>
#include <minix/minlib.h>
#include <minix/const.h>
#include <minix/type.h>
#include <minix/board.h>
#include <minix/com.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/reboot.h>
#include "string.h"
#include "arch_proto.h"
#include "direct_utils.h"
#include "bsp_serial.h"
#include "bsp_table.h"
#include "glo.h"
#include <machine/multiboot.h>

#if USE_SYSDEBUG
#define MULTIBOOT_VERBOSE 1
#endif

/* to-be-built kinfo struct, diagnostics buffer */
kinfo_t kinfo;
struct kmessages kmessages;

static void setup_mbi(multiboot_info_t *mbi, char *bootargs);

/* String length used for mb_itoa */
#define ITOA_BUFFER_SIZE 20

/* Kernel may use memory */
int kernel_may_alloc = 1;

/* kernel bss */
extern u32_t _edata;
extern u32_t _end;

#define BSP_TABLE_GENERATE(name) \
	bsp_table name##_bsp_table = { \
		.bsp_init = name##_init, \
		.bsp_irq_unmask = name##_irq_unmask, \
		.bsp_irq_mask = name##_irq_mask, \
		.bsp_irq_handle = name##_irq_handle, \
		.bsp_padconf_init = name##_padconf_init, \
		.bsp_padconf_set = name##_padconf_set, \
		.bsp_reset_init = name##_reset_init, \
		.bsp_reset = name##_reset, \
		.bsp_poweroff = name##_poweroff, \
		.bsp_disable_watchdog = name##_disable_watchdog, \
		.bsp_ser_init = name##_ser_init, \
		.bsp_ser_putc = name##_ser_putc, \
		.bsp_timer_init = name##_timer_init, \
		.bsp_timer_stop = name##_timer_stop, \
		.bsp_register_timer_handler = name##_register_timer_handler, \
		.bsp_timer_int_handler = name##_timer_int_handler, \
		.read_tsc_64 = name##_read_tsc_64, \
		.intr_init = name##_intr_init \
	}

BSP_TABLE_GENERATE(rpi);
BSP_TABLE_GENERATE(omap);

bsp_table *bsp_tb;

/*
 * During low level init many things are not supposed to work
 * serial being one of them. We therefore can't rely on the
 * serial to debug. POORMANS_FAILURE_NOTIFICATION can be used
 * before we setup our own vector table and will result in calling
 * the bootloader's debugging methods that will hopefully show some
 * information like the currnet PC at on the serial.
 */
#define POORMANS_FAILURE_NOTIFICATION  asm volatile("svc #00\n")

/**
 *
 * The following function combines a few things together
 * that can well be done using standard libc like strlen/strstr
 * and such but these are not available in pre_init stage. 
 *
 * The function expects content to be in the form of space separated
 * key value pairs.
 * param content the contents to search in
 * param key the key to find (this *should* include the key/value delimiter)
 * param value a pointer to an initialized char * of at least value_max_len length
 * param value_max_len the maximum length of the value to store in value including
 *       the end char
 *
**/
int find_value(char * content,char * key,char *value,int value_max_len){

	char *iter,*keyp;
	int key_len,content_len,match_len,value_len;

	/* return if the input is invalid */
	if  (key == NULL || content == NULL || value == NULL) {
		return 1;
	}

	/* find the key and content length */
	key_len = content_len =0;
	for(iter = key ; *iter != '\0'; iter++, key_len++);
	for(iter = content ; *iter != '\0'; iter++, content_len++);

	/* return if key or content length invalid */
	if (key_len == 0 || content_len == 0) {
		return 1;
	}

	/* now find the key in the contents */
	match_len =0;
	for (iter = content ,keyp=key; match_len < key_len && *iter != '\0' ; iter++) {
		if (*iter == *keyp) {
			match_len++;
			keyp++;
			continue;
		} 
		/* The current key does not match the value , reset */
		match_len =0;
		keyp=key;
	}

	if (match_len == key_len) {
		printf("key found at %d %s\n", match_len, &content[match_len]);
		value_len = 0;
		/* copy the content to the value char iter already points to the first 
		   char value */
		while(*iter != '\0' && *iter != ' ' && value_len  + 1< value_max_len) {
			*value++ = *iter++;
			value_len++;
		}
		*value='\0';
		return 0;
	}
	return 1; /* not found */
}

static int mb_set_param(char *bigbuf,char *name,char *value, kinfo_t *cbi)
{
	/* bigbuf contains a list of key=value pairs separated by \0 char.
	 * The list itself is ended by a second \0 terminator*/
	char *p = bigbuf;
	char *bufend = bigbuf + MULTIBOOT_PARAM_BUF_SIZE;
	char *q;
	int namelen = strlen(name);
	int valuelen = strlen(value);

	/* Some variables we recognize */
	if(!strcmp(name, SERVARNAME)) { cbi->do_serial_debug = 1; }
	if(!strcmp(name, SERBAUDVARNAME)) { cbi->serial_debug_baud = atoi(value); }

	/* Delete the item if already exists */
	while (*p) {
		if (strncmp(p, name, namelen) == 0 && p[namelen] == '=') {
			q = p;
			/* let q point to the end of the entry */
			while (*q) q++; 
			/* now copy the remained of the buffer */
			for (q++; q < bufend; q++, p++)
				*p = *q;
			break;
		}

		/* find the end of the buffer */
		while (*p++);
		p++;
	}
	

	/* find the first empty spot */
	for (p = bigbuf; p < bufend && (*p || *(p + 1)); p++);

	/* unless we are the first entry step over the delimiter */
	if (p > bigbuf) p++;
	
	/* Make sure there's enough space for the new parameter */
	if (p + namelen + valuelen + 3 > bufend) {
		return -1;
	}
	
	strcpy(p, name);
	p[namelen] = '=';
	strcpy(p + namelen + 1, value);
	p[namelen + valuelen + 1] = 0;
	p[namelen + valuelen + 2] = 0; /* end with a second delimiter */
	return 0;
}

int overlaps(multiboot_module_t *mod, int n, int cmp_mod)
{
	multiboot_module_t *cmp = &mod[cmp_mod];
	int m;

#define INRANGE(mod, v) ((v) >= mod->mod_start && (v) <= thismod->mod_end)
#define OVERLAP(mod1, mod2) (INRANGE(mod1, mod2->mod_start) || \
		INRANGE(mod1, mod2->mod_end))
	for(m = 0; m < n; m++) {
		multiboot_module_t *thismod = &mod[m];
		if(m == cmp_mod) continue;
		if(OVERLAP(thismod, cmp)) {
			return 1;
		}
	}
	return 0;
}

/* XXX: hard-coded stuff for modules */
#define MB_MODS_NR NR_BOOT_MODULES

multiboot_module_t mb_modlist[MB_MODS_NR];
multiboot_memory_map_t mb_memmap;

void setup_mbi(multiboot_info_t *mbi, char *bootargs)
{
	memset(mbi, 0, sizeof(*mbi));
	mbi->flags = MULTIBOOT_INFO_MODS | MULTIBOOT_INFO_MEM_MAP |
			MULTIBOOT_INFO_CMDLINE;
	mbi->mi_mods_count = MB_MODS_NR;
	mbi->mods_addr = (u32_t)&mb_modlist;

	phys_bytes mb_mods_base;
	phys_bytes mb_mods_align = 0x00800000;
	phys_bytes mb_mmap_start;
	phys_bytes mb_mmap_size;

	if (BOARD_IS_BB(machine.board_id) || BOARD_IS_BBXM(machine.board_id)) {
		mb_mods_base = 0x82000000;
		mb_mmap_start = 0x80000000;
		mb_mmap_size = 0x10000000; /* 256 MB */
	}
	else if (BOARD_IS_RPI_2_B(machine.board_id) || BOARD_IS_RPI_3_B(machine.board_id)) {
		mb_mods_base = 0x02000000;
		mb_mmap_start = 0x00008000; /* Don't overwrite bootcode for secondary CPUs */
		mb_mmap_size = 0x3C000000 - 0x00008000; /* 960 MB */
	}
	else
		POORMANS_FAILURE_NOTIFICATION;

	int i;
	for (i = 0; i < MB_MODS_NR; ++i) {
		mb_modlist[i].mod_start = mb_mods_base + i * mb_mods_align;
		mb_modlist[i].mod_end = mb_modlist[i].mod_start + mb_mods_align
			- ARM_PAGE_SIZE;
		mb_modlist[i].cmdline = 0;
	}

	/* morph the bootargs into multiboot */
	mbi->cmdline = (u32_t) bootargs;

	mbi->mmap_addr =(u32_t)&mb_memmap;
	mbi->mmap_length = sizeof(mb_memmap);

	mb_memmap.size = sizeof(multiboot_memory_map_t);
	mb_memmap.mm_base_addr = mb_mmap_start;
	mb_memmap.mm_length  = mb_mmap_size;
	mb_memmap.type = MULTIBOOT_MEMORY_AVAILABLE;
}

void get_parameters(kinfo_t *cbi, char *bootargs)
{
	multiboot_memory_map_t *mmap;
	multiboot_info_t *mbi = &cbi->mbi;
	int var_i,value_i, m, k;
	char *p;
	extern char _kern_phys_base, _kern_vir_base, _kern_size,
		_kern_unpaged_start, _kern_unpaged_end;
	phys_bytes kernbase = (phys_bytes) &_kern_phys_base,
		kernsize = (phys_bytes) &_kern_size;
#define BUF 1024
	static char cmdline[BUF];

	/* get our own copy of the multiboot info struct and module list */
	setup_mbi(mbi, bootargs);

	/* Set various bits of info for the higher-level kernel. */
	cbi->mem_high_phys = 0;
	cbi->user_sp = (vir_bytes) &_kern_vir_base;
	cbi->vir_kern_start = (vir_bytes) &_kern_vir_base;
	cbi->bootstrap_start = (vir_bytes) &_kern_unpaged_start;
	cbi->bootstrap_len = (vir_bytes) &_kern_unpaged_end -
		cbi->bootstrap_start;
	cbi->kmess = &kmess;

	/* set some configurable defaults */
	cbi->do_serial_debug = 1;
	cbi->serial_debug_baud = 115200;

	/* parse boot command line */
	if (mbi->flags&MULTIBOOT_INFO_CMDLINE) {
		static char var[BUF];
		static char value[BUF];

		/* Override values with cmdline argument */
		memcpy(cmdline, (void *) mbi->cmdline, BUF);
		p = cmdline;
		while (*p) {
			var_i = 0;
			value_i = 0;
			while (*p == ' ') p++; /* skip spaces */
			if (!*p) break; /* is this the end? */
			while (*p && *p != '=' && *p != ' ' && var_i < BUF - 1)
				var[var_i++] = *p++ ;
			var[var_i] = 0;
			if (*p++ != '=') continue; /* skip if not name=value */
			while (*p && *p != ' ' && value_i < BUF - 1) {
				value[value_i++] = *p++ ;
			}
			value[value_i] = 0;
			
			mb_set_param(cbi->param_buf, var, value, cbi);
		}
	}

	/* let higher levels know what we are booting on */
	mb_set_param(cbi->param_buf, ARCHVARNAME, (char *)get_board_arch_name(machine.board_id), cbi);
	mb_set_param(cbi->param_buf, BOARDVARNAME,(char *)get_board_name(machine.board_id) , cbi);
	

	/* move user stack/data down to leave a gap to catch kernel
	 * stack overflow; and to distinguish kernel and user addresses
	 * at a glance (0xf.. vs 0xe..) 
	 */
	cbi->user_sp = USR_STACKTOP;
	cbi->user_end = USR_DATATOP;

	/* kernel bytes without bootstrap code/data that is currently
	 * still needed but will be freed after bootstrapping.
	 */
	kinfo.kernel_allocated_bytes = (phys_bytes) &_kern_size;
	kinfo.kernel_allocated_bytes -= cbi->bootstrap_len;

	assert(!(cbi->bootstrap_start % ARM_PAGE_SIZE));
	cbi->bootstrap_len = rounddown(cbi->bootstrap_len, ARM_PAGE_SIZE);
	assert(mbi->flags & MULTIBOOT_INFO_MODS);
	assert(mbi->mi_mods_count < MULTIBOOT_MAX_MODS);
	assert(mbi->mi_mods_count > 0);
	memcpy(&cbi->module_list, (void *) mbi->mods_addr,
		mbi->mi_mods_count * sizeof(multiboot_module_t));
	
	memset(cbi->memmap, 0, sizeof(cbi->memmap));
	/* mem_map has a variable layout */
	if(mbi->flags & MULTIBOOT_INFO_MEM_MAP) {
		cbi->mmap_size = 0;
			for (mmap = (multiboot_memory_map_t *) mbi->mmap_addr;
			 (unsigned long) mmap < mbi->mmap_addr + mbi->mmap_length;
			   mmap = (multiboot_memory_map_t *) 
				((unsigned long) mmap + mmap->size + sizeof(mmap->size))) {
			if(mmap->type != MULTIBOOT_MEMORY_AVAILABLE) continue;
			add_memmap(cbi, mmap->mm_base_addr, mmap->mm_length);
		}
	} else {
		assert(mbi->flags & MULTIBOOT_INFO_MEMORY);
		add_memmap(cbi, 0, mbi->mem_lower_unused*1024);
		add_memmap(cbi, 0x100000, mbi->mem_upper_unused*1024);
	}

	/* Sanity check: the kernel nor any of the modules may overlap
	 * with each other. Pretend the kernel is an extra module for a
	 * second.
	 */
	k = mbi->mi_mods_count;
	assert(k < MULTIBOOT_MAX_MODS);
	cbi->module_list[k].mod_start = kernbase;
	cbi->module_list[k].mod_end = kernbase + kernsize;
	cbi->mods_with_kernel = mbi->mi_mods_count+1;
	cbi->kern_mod = k;

	for(m = 0; m < cbi->mods_with_kernel; m++) {
#if 0
		printf("checking overlap of module %08lx-%08lx\n",
		  cbi->module_list[m].mod_start, cbi->module_list[m].mod_end);
#endif
		if(overlaps(cbi->module_list, cbi->mods_with_kernel, m))
			panic("overlapping boot modules/kernel");
		/* We cut out the bits of memory that we know are
		 * occupied by the kernel and boot modules.
		 */
		cut_memmap(cbi,
			cbi->module_list[m].mod_start,
			cbi->module_list[m].mod_end);
	}
}

/* use the passed cmdline argument to determine the machine id */
void set_machine_id(char *cmdline)
{

	char boardname[20];
	memset(boardname,'\0',20);
	if (find_value(cmdline,"board_name=",boardname,20)){
		/* we expect the bootloader to pass a board_name as argument
		 * this however did not happen and given we still are in early
		 * boot we can't use the serial. We therefore generate an interrupt
		 * and hope the bootloader will do something nice with it */
		POORMANS_FAILURE_NOTIFICATION;
	}  
	machine.board_id = get_board_id_by_short_name(boardname);

	if (machine.board_id ==0){
		/* same thing as above there is no safe escape */
		POORMANS_FAILURE_NOTIFICATION;
	}
}

void set_bsp_table ()
{
	if (BOARD_IS_BB(machine.board_id) || BOARD_IS_BBXM(machine.board_id)) {
		bsp_tb = &omap_bsp_table;
	}
	else if (BOARD_IS_RPI_2_B(machine.board_id) || BOARD_IS_RPI_3_B(machine.board_id)) {
		bsp_tb = &rpi_bsp_table;
	}
}

void read_tsc_64(u64_t * t) 
{
	bsp_tb->read_tsc_64(t);
}

void bsp_irq_handle(void) 
{
	bsp_tb->bsp_irq_handle();
}

int intr_init(const int auto_eoi)
{
	return bsp_tb->intr_init (auto_eoi);
}

kinfo_t *pre_init(int argc, char **argv)
{
	char* bootargs;

	/* This is the main "c" entry point into the kernel. It gets called
	from head.S */
	   
	/* we get called in a c like fashion where the first arg
	 * is the program name (load address) and the rest are
	 * arguments. by convention the second argument is the
	 *  command line */
	if (argc != 2) {
		POORMANS_FAILURE_NOTIFICATION;
	}

	bootargs = argv[1];
	set_machine_id(bootargs);
	set_bsp_table();

	/* Get our own copy boot params pointed to by r1.
	 * Here we find out whether we should do serial output.
	 */
	get_parameters(&kinfo, bootargs);

	return &kinfo;
}

/* pre_init gets executed at the memory location where the kernel was loaded by the boot loader.
 * at that stage we only have a minimum set of functionality present (all symbols gets renamed to
 * ensure this). The following methods are used in that context. Once we jump to kmain they are no
 * longer used and the "real" implementations are visible
 */
struct machine machine; /* pre init stage machine */
