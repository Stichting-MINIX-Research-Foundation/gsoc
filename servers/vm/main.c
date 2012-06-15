
#define _POSIX_SOURCE      1
#define _MINIX             1
#define _SYSTEM            1

#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/config.h>
#include <minix/const.h>
#include <minix/ds.h>
#include <minix/endpoint.h>
#include <minix/keymap.h>
#include <minix/minlib.h>
#include <minix/type.h>
#include <minix/ipc.h>
#include <minix/sysutil.h>
#include <minix/syslib.h>
#include <minix/const.h>
#include <minix/bitmap.h>
#include <minix/crtso.h>
#include <minix/rs.h>

#include <errno.h>
#include <string.h>
#include <env.h>
#include <stdio.h>
#include <assert.h>

#include <memory.h>

#define _MAIN 1
#include "glo.h"
#include "proto.h"
#include "util.h"
#include "vm.h"
#include "sanitycheck.h"

extern int missing_spares;

#include <machine/archtypes.h>
#include "kernel/const.h"
#include "kernel/config.h"
#include "kernel/proc.h"

#include <signal.h>

/* Table of calls and a macro to test for being in range. */
struct {
	int (*vmc_func)(message *);	/* Call handles message. */
	char *vmc_name;			/* Human-readable string. */
} vm_calls[NR_VM_CALLS];

/* Macro to verify call range and map 'high' range to 'base' range
 * (starting at 0) in one. Evaluates to zero-based call number if call
 * number is valid, returns -1 otherwise.
 */
#define CALLNUMBER(c) (((c) >= VM_RQ_BASE && 				\
			(c) < VM_RQ_BASE + ELEMENTS(vm_calls)) ?	\
			((c) - VM_RQ_BASE) : -1)

static int map_service(struct rprocpub *rpub);
static int vm_acl_ok(endpoint_t caller, int call);

/* SEF functions and variables. */
static void sef_local_startup(void);
static int sef_cb_init_fresh(int type, sef_init_info_t *info);
static void sef_cb_signal_handler(int signo);

/*===========================================================================*
 *				main					     *
 *===========================================================================*/
int main(void)
{
  message msg;
  int result, who_e, rcv_sts;
  int caller_slot;
  struct vmproc *vmp_caller;

  /* SEF local startup. */
  sef_local_startup();

  SANITYCHECK(SCL_TOP);

  /* This is VM's main loop. */
  while (TRUE) {
	int r, c;

	SANITYCHECK(SCL_TOP);
	if(missing_spares > 0) {
		pt_cycle();	/* pagetable code wants to be called */
	}

  	if ((r=sef_receive_status(ANY, &msg, &rcv_sts)) != OK)
		panic("sef_receive_status() error: %d", r);

	if (is_ipc_notify(rcv_sts)) {
		/* Unexpected notify(). */
		printf("VM: ignoring notify() from %d\n", msg.m_source);
		continue;
	}
	who_e = msg.m_source;
	if(vm_isokendpt(who_e, &caller_slot) != OK)
		panic("invalid caller", who_e);
	vmp_caller = &vmproc[caller_slot];
	c = CALLNUMBER(msg.m_type);
	result = ENOSYS; /* Out of range or restricted calls return this. */
	if (msg.m_type == VM_PAGEFAULT) {
		if (!IPC_STATUS_FLAGS_TEST(rcv_sts, IPC_FLG_MSG_FROM_KERNEL)) {
			printf("VM: process %d faked VM_PAGEFAULT "
					"message!\n", msg.m_source);
		}
		do_pagefaults(&msg);
		pt_clearmapcache();
		/*
		 * do not reply to this call, the caller is unblocked by
		 * a sys_vmctl() call in do_pagefaults if success. VM panics
		 * otherwise
		 */
		continue;
	} else if(c < 0 || !vm_calls[c].vmc_func) {
		/* out of range or missing callnr */
	} else {
		if (vm_acl_ok(who_e, c) != OK) {
			printf("VM: unauthorized %s by %d\n",
					vm_calls[c].vmc_name, who_e);
		} else {
			SANITYCHECK(SCL_FUNCTIONS);
			result = vm_calls[c].vmc_func(&msg);
			SANITYCHECK(SCL_FUNCTIONS);
		}
	}

	/* Send reply message, unless the return code is SUSPEND,
	 * which is a pseudo-result suppressing the reply message.
	 */
	if(result != SUSPEND) {
		msg.m_type = result;
		if((r=send(who_e, &msg)) != OK) {
			printf("VM: couldn't send %d to %d (err %d)\n",
				msg.m_type, who_e, r);
			panic("send() error");
		}
	}
  }
  return(OK);
}

/*===========================================================================*
 *			       sef_local_startup			     *
 *===========================================================================*/
static void sef_local_startup()
{
  /* Register init callbacks. */
  sef_setcb_init_fresh(sef_cb_init_fresh);
  sef_setcb_init_restart(sef_cb_init_fail);

  /* No live update support for now. */

  /* Register signal callbacks. */
  sef_setcb_signal_handler(sef_cb_signal_handler);

  /* Let SEF perform startup. */
  sef_startup();
}

/*===========================================================================*
 *				sef_cb_init_fresh			     *
 *===========================================================================*/
static int sef_cb_init_fresh(int type, sef_init_info_t *info)
{
/* Initialize the vm server. */
	int s, i;
	struct memory mem_chunks[NR_MEMS];
	struct boot_image image[NR_BOOT_PROCS];
	struct boot_image *ip;
	struct rprocpub rprocpub[NR_BOOT_PROCS];
	phys_bytes limit = 0;
	int is_elf = 0;

#if SANITYCHECKS
	incheck = nocheck = 0;
#endif

#if SANITYCHECKS
	env_parse("vm_sanitychecklevel", "d", 0, &vm_sanitychecklevel, 0, SCL_MAX);
#endif

	/* Get chunks of available memory. */
	get_mem_chunks(mem_chunks);

	/* Initialize VM's process table. Request a copy of the system
	 * image table that is defined at the kernel level to see which
	 * slots to fill in.
	 */
	if (OK != (s=sys_getimage(image)))
		panic("couldn't get image table: %d", s);

	/* Set table to 0. This invalidates all slots (clear VMF_INUSE). */
	memset(vmproc, 0, sizeof(vmproc));

	for(i = 0; i < ELEMENTS(vmproc); i++) {
		vmproc[i].vm_slot = i;
	}

	/* Walk through boot-time system processes that are alive
	 * now and make valid slot entries for them.
	 */
	for (ip = &image[0]; ip < &image[NR_BOOT_PROCS]; ip++) {
		phys_bytes proclimit;
		struct vmproc *vmp;

		if(ip->proc_nr >= _NR_PROCS) { panic("proc: %d", ip->proc_nr); }
		if(ip->proc_nr < 0 && ip->proc_nr != SYSTEM) continue;

#define GETVMP(v, nr)						\
		if(nr >= 0) {					\
			vmp = &vmproc[ip->proc_nr];		\
		} else if(nr == SYSTEM) {			\
			vmp = &vmproc[VMP_SYSTEM];		\
		} else {					\
			panic("init: crazy proc_nr: %d", nr);	\
		}

		/* Initialize normal process table slot or special SYSTEM
		 * table slot. Kernel memory is already reserved.
		 */
		GETVMP(vmp, ip->proc_nr);

		/* reset fields as if exited */
		clear_proc(vmp);

		/* Get memory map for this process from the kernel. */
		if ((s=get_mem_map(ip->proc_nr, vmp->vm_arch.vm_seg)) != OK)
			panic("couldn't get process mem_map: %d", s);

		/* Remove this memory from the free list. */
		reserve_proc_mem(mem_chunks, vmp->vm_arch.vm_seg);

		/* Set memory limit. */
		proclimit = CLICK2ABS(vmp->vm_arch.vm_seg[S].mem_phys +
			vmp->vm_arch.vm_seg[S].mem_len) - 1;

		if(proclimit > limit)
			limit = proclimit;

		vmp->vm_flags = VMF_INUSE;
		vmp->vm_endpoint = ip->endpoint;
		vmp->vm_stacktop =
			CLICK2ABS(vmp->vm_arch.vm_seg[S].mem_vir +
				vmp->vm_arch.vm_seg[S].mem_len);

		if (vmp->vm_arch.vm_seg[T].mem_len != 0)
			vmp->vm_flags |= VMF_SEPARATE;
	}

	/* region management initialization. */
	map_region_init();

	/* Architecture-dependent initialization. */
	pt_init(limit);

	/* Initialize tables to all physical memory. */
	mem_init(mem_chunks);
	meminit_done = 1;

	/* Architecture-dependent memory initialization. */
	pt_init_mem();

	/* Give these processes their own page table. */
	for (ip = &image[0]; ip < &image[NR_BOOT_PROCS]; ip++) {
		struct vmproc *vmp;
		vir_bytes old_stacktop, old_stacklen;

		if(ip->proc_nr < 0) continue;

		GETVMP(vmp, ip->proc_nr);

               if(!(ip->flags & PROC_FULLVM))
                       continue;

        	if(pt_new(&vmp->vm_pt) != OK)
			panic("VM: no new pagetable");
#define BASICSTACK VM_PAGE_SIZE
		old_stacktop = CLICK2ABS(vmp->vm_arch.vm_seg[S].mem_vir +
				vmp->vm_arch.vm_seg[S].mem_len);
		if(sys_vmctl(vmp->vm_endpoint, VMCTL_INCSP,
			VM_STACKTOP - old_stacktop) != OK) {
			panic("VM: vmctl for new stack failed");
		}

		old_stacklen =
			vmp->vm_arch.vm_seg[S].mem_vir +
			vmp->vm_arch.vm_seg[S].mem_len -
			vmp->vm_arch.vm_seg[D].mem_len -
			vmp->vm_arch.vm_seg[D].mem_vir;

		free_mem(vmp->vm_arch.vm_seg[D].mem_phys +
			vmp->vm_arch.vm_seg[D].mem_len,
			old_stacklen);

#if defined(__ELF__)
		is_elf = 1;
#endif

		if(proc_new(vmp,
			VM_PROCSTART,
			CLICK2ABS(vmp->vm_arch.vm_seg[T].mem_vir),
			CLICK2ABS(vmp->vm_arch.vm_seg[T].mem_len),
			CLICK2ABS(vmp->vm_arch.vm_seg[D].mem_vir),
			CLICK2ABS(vmp->vm_arch.vm_seg[D].mem_len),
			BASICSTACK,
			CLICK2ABS(vmp->vm_arch.vm_seg[S].mem_vir +
				vmp->vm_arch.vm_seg[S].mem_len -
				vmp->vm_arch.vm_seg[D].mem_len -
				vmp->vm_arch.vm_seg[D].mem_vir) - BASICSTACK,
			CLICK2ABS(vmp->vm_arch.vm_seg[T].mem_phys),
			CLICK2ABS(vmp->vm_arch.vm_seg[D].mem_phys),
			    VM_STACKTOP, 0, is_elf, 0) != OK) {
			panic("failed proc_new for boot process");
		}
	}

	/* Set up table of calls. */
#define CALLMAP(code, func) { int i;			      \
	if((i=CALLNUMBER(code)) < 0) { panic(#code " invalid: %d", (code)); } \
	if(i >= NR_VM_CALLS) { panic(#code " invalid: %d", (code)); } \
	vm_calls[i].vmc_func = (func); 				      \
	vm_calls[i].vmc_name = #code; 				      \
}

	/* Set call table to 0. This invalidates all calls (clear
	 * vmc_func).
	 */
	memset(vm_calls, 0, sizeof(vm_calls));

	/* Basic VM calls. */
	CALLMAP(VM_MMAP, do_mmap);
	CALLMAP(VM_MUNMAP, do_munmap);
	CALLMAP(VM_MUNMAP_TEXT, do_munmap);
	CALLMAP(VM_MAP_PHYS, do_map_phys);
	CALLMAP(VM_UNMAP_PHYS, do_unmap_phys);

	/* Calls from PM. */
	CALLMAP(VM_EXIT, do_exit);
	CALLMAP(VM_FORK, do_fork);
	CALLMAP(VM_BRK, do_brk);
	CALLMAP(VM_PUSH_SIG, do_push_sig);
	CALLMAP(VM_WILLEXIT, do_willexit);
	CALLMAP(VM_ADDDMA, do_adddma);
	CALLMAP(VM_DELDMA, do_deldma);
	CALLMAP(VM_GETDMA, do_getdma);
	CALLMAP(VM_NOTIFY_SIG, do_notify_sig);

	/* Calls from RS */
	CALLMAP(VM_RS_SET_PRIV, do_rs_set_priv);
	CALLMAP(VM_RS_UPDATE, do_rs_update);
	CALLMAP(VM_RS_MEMCTL, do_rs_memctl);

	/* Calls from RS/VFS */
	CALLMAP(VM_PROCCTL, do_procctl);

	/* Generic calls. */
	CALLMAP(VM_REMAP, do_remap);
	CALLMAP(VM_REMAP_RO, do_remap);
	CALLMAP(VM_GETPHYS, do_get_phys);
	CALLMAP(VM_SHM_UNMAP, do_shared_unmap);
	CALLMAP(VM_GETREF, do_get_refcount);
	CALLMAP(VM_INFO, do_info);
	CALLMAP(VM_QUERY_EXIT, do_query_exit);
	CALLMAP(VM_WATCH_EXIT, do_watch_exit);
	CALLMAP(VM_FORGETBLOCKS, do_forgetblocks);
	CALLMAP(VM_FORGETBLOCK, do_forgetblock);
	CALLMAP(VM_YIELDBLOCKGETBLOCK, do_yieldblockgetblock);

	/* Sanity checks */
	if(find_kernel_top() >= VM_PROCSTART)
		panic("kernel loaded too high");

	/* Initialize the structures for queryexit */
	init_query_exit();

	/* Map all the services in the boot image. */
	if((s = sys_safecopyfrom(RS_PROC_NR, info->rproctab_gid, 0,
		(vir_bytes) rprocpub, sizeof(rprocpub), S)) != OK) {
		panic("sys_safecopyfrom failed: %d", s);
	}
	for(i=0;i < NR_BOOT_PROCS;i++) {
		if(rprocpub[i].in_use) {
			if((s = map_service(&rprocpub[i])) != OK) {
				panic("unable to map service: %d", s);
			}
		}
	}

	return(OK);
}

/*===========================================================================*
 *		            sef_cb_signal_handler                            *
 *===========================================================================*/
static void sef_cb_signal_handler(int signo)
{
	/* Check for known kernel signals, ignore anything else. */
	switch(signo) {
		/* There is a pending memory request from the kernel. */
		case SIGKMEM:
			do_memory();
		break;
	}

	/* It can happen that we get stuck receiving signals
	 * without sef_receive() returning. We could need more memory
	 * though.
	 */
	if(missing_spares > 0) {
		pt_cycle();	/* pagetable code wants to be called */
	}

	pt_clearmapcache();
}

/*===========================================================================*
 *		               map_service                                   *
 *===========================================================================*/
static int map_service(rpub)
struct rprocpub *rpub;
{
/* Map a new service by initializing its call mask. */
	int r, proc_nr;

	if ((r = vm_isokendpt(rpub->endpoint, &proc_nr)) != OK) {
		return r;
	}

	/* Copy the call mask. */
	memcpy(&vmproc[proc_nr].vm_call_mask, &rpub->vm_call_mask,
		sizeof(vmproc[proc_nr].vm_call_mask));

	return(OK);
}

/*===========================================================================*
 *				vm_acl_ok				     *
 *===========================================================================*/
static int vm_acl_ok(endpoint_t caller, int call)
{
	int n, r;

	if ((r = vm_isokendpt(caller, &n)) != OK)
		panic("VM: from strange source: %d", caller);

	/* See if the call is allowed. */
	if (!GET_BIT(vmproc[n].vm_call_mask, call)) {
		return EPERM;
	}

	return OK;
}

