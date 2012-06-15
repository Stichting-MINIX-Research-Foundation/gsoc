
#include "kernel/kernel.h"
#include "kernel/proc.h"
#include "kernel/vm.h"

#include <machine/vm.h>

#include <minix/type.h>
#include <minix/syslib.h>
#include <minix/cpufeature.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <stdlib.h>

#include <machine/vm.h>

#include "oxpcie.h"
#include "arch_proto.h"
#include "kernel/proto.h"
#include "kernel/debug.h"

#ifdef USE_APIC
#include "apic.h"
#ifdef USE_WATCHDOG
#include "kernel/watchdog.h"
#endif
#endif

int i386_paging_enabled = 0;

static int psok = 0;

#define MAX_FREEPDES	2
static int nfreepdes = 0, freepdes[MAX_FREEPDES];

#define HASPT(procptr) ((procptr)->p_seg.p_cr3 != 0)

static u32_t phys_get32(phys_bytes v);
static void vm_enable_paging(void);

	
void segmentation2paging(struct proc * current)
{
	/* switch to the current process page tables before turning paging on */
	switch_address_space(current);
	vm_enable_paging();
}

/* This function sets up a mapping from within the kernel's address
 * space to any other area of memory, either straight physical
 * memory (pr == NULL) or a process view of memory, in 4MB windows.
 * I.e., it maps in 4MB chunks of virtual (or physical) address space
 * to 4MB chunks of kernel virtual address space.
 *
 * It recognizes pr already being in memory as a special case (no
 * mapping required).
 *
 * The target (i.e. in-kernel) mapping area is one of the freepdes[]
 * VM has earlier already told the kernel about that is available. It is
 * identified as the 'pde' parameter. This value can be chosen freely
 * by the caller, as long as it is in range (i.e. 0 or higher and corresonds
 * to a known freepde slot). It is up to the caller to keep track of which
 * freepde's are in use, and to determine which ones are free to use.
 *
 * The logical number supplied by the caller is translated into an actual
 * pde number to be used, and a pointer to it (linear address) is returned
 * for actual use by phys_copy or phys_memset.
 */
static phys_bytes createpde(
	const struct proc *pr,	/* Requested process, NULL for physical. */
	const phys_bytes linaddr,/* Address after segment translation. */
	phys_bytes *bytes,	/* Size of chunk, function may truncate it. */
	int free_pde_idx,	/* index of the free slot to use */
	int *changed		/* If mapping is made, this is set to 1. */
	)
{
	u32_t pdeval;
	phys_bytes offset;
	int pde;

	assert(free_pde_idx >= 0 && free_pde_idx < nfreepdes);
	pde = freepdes[free_pde_idx];
	assert(pde >= 0 && pde < 1024);

	if(pr && ((pr == get_cpulocal_var(ptproc)) || !HASPT(pr))) {
		/* Process memory is requested, and
		 * it's a process that is already in current page table, or
		 * a process that is in every page table.
		 * Therefore linaddr is valid directly, with the requested
		 * size.
		 */
		return linaddr;
	}

	if(pr) {
		/* Requested address is in a process that is not currently
		 * accessible directly. Grab the PDE entry of that process'
		 * page table that corresponds to the requested address.
		 */
		assert(pr->p_seg.p_cr3_v);
		pdeval = pr->p_seg.p_cr3_v[I386_VM_PDE(linaddr)];
	} else {
		/* Requested address is physical. Make up the PDE entry. */
		pdeval = (linaddr & I386_VM_ADDR_MASK_4MB) | 
			I386_VM_BIGPAGE | I386_VM_PRESENT | 
			I386_VM_WRITE | I386_VM_USER;
	}

	/* Write the pde value that we need into a pde that the kernel
	 * can access, into the currently loaded page table so it becomes
	 * visible.
	 */
	assert(get_cpulocal_var(ptproc)->p_seg.p_cr3_v);
	if(get_cpulocal_var(ptproc)->p_seg.p_cr3_v[pde] != pdeval) {
		get_cpulocal_var(ptproc)->p_seg.p_cr3_v[pde] = pdeval;
		*changed = 1;
	}

	/* Memory is now available, but only the 4MB window of virtual
	 * address space that we have mapped; calculate how much of
	 * the requested range is visible and return that in *bytes,
	 * if that is less than the requested range.
	 */
	offset = linaddr & I386_VM_OFFSET_MASK_4MB; /* Offset in 4MB window. */
	*bytes = MIN(*bytes, I386_BIG_PAGE_SIZE - offset); 

	/* Return the linear address of the start of the new mapping. */
	return I386_BIG_PAGE_SIZE*pde + offset;
}
  
/*===========================================================================*
 *				lin_lin_copy				     *
 *===========================================================================*/
static int lin_lin_copy(struct proc *srcproc, vir_bytes srclinaddr, 
	struct proc *dstproc, vir_bytes dstlinaddr, vir_bytes bytes)
{
	u32_t addr;
	proc_nr_t procslot;

	assert(vm_running);
	assert(nfreepdes >= MAX_FREEPDES);

	assert(get_cpulocal_var(ptproc));
	assert(get_cpulocal_var(proc_ptr));
	assert(read_cr3() == get_cpulocal_var(ptproc)->p_seg.p_cr3);

	procslot = get_cpulocal_var(ptproc)->p_nr;

	assert(procslot >= 0 && procslot < I386_VM_DIR_ENTRIES);

	if(srcproc) assert(!RTS_ISSET(srcproc, RTS_SLOT_FREE));
	if(dstproc) assert(!RTS_ISSET(dstproc, RTS_SLOT_FREE));
	assert(!RTS_ISSET(get_cpulocal_var(ptproc), RTS_SLOT_FREE));
	assert(get_cpulocal_var(ptproc)->p_seg.p_cr3_v);
	if(srcproc) assert(!RTS_ISSET(srcproc, RTS_VMINHIBIT));
	if(dstproc) assert(!RTS_ISSET(dstproc, RTS_VMINHIBIT));

	while(bytes > 0) {
		phys_bytes srcptr, dstptr;
		vir_bytes chunk = bytes;
		int changed = 0;

#ifdef CONFIG_SMP
		unsigned cpu = cpuid;

		if (GET_BIT(srcproc->p_stale_tlb, cpu)) {
			changed = 1;
			UNSET_BIT(srcproc->p_stale_tlb, cpu);
		}
		if (GET_BIT(dstproc->p_stale_tlb, cpu)) {
			changed = 1;
			UNSET_BIT(dstproc->p_stale_tlb, cpu);
		}
#endif

		/* Set up 4MB ranges. */
		srcptr = createpde(srcproc, srclinaddr, &chunk, 0, &changed);
		dstptr = createpde(dstproc, dstlinaddr, &chunk, 1, &changed);
		if(changed)
			reload_cr3(); 

		/* Copy pages. */
		PHYS_COPY_CATCH(srcptr, dstptr, chunk, addr);

		if(addr) {
			/* If addr is nonzero, a page fault was caught. */

			if(addr >= srcptr && addr < (srcptr + chunk)) {
				return EFAULT_SRC;
			}
			if(addr >= dstptr && addr < (dstptr + chunk)) {
				return EFAULT_DST;
			}

			panic("lin_lin_copy fault out of range");

			/* Not reached. */
			return EFAULT;
		}

		/* Update counter and addresses for next iteration, if any. */
		bytes -= chunk;
		srclinaddr += chunk;
		dstlinaddr += chunk;
	}

	if(srcproc) assert(!RTS_ISSET(srcproc, RTS_SLOT_FREE));
	if(dstproc) assert(!RTS_ISSET(dstproc, RTS_SLOT_FREE));
	assert(!RTS_ISSET(get_cpulocal_var(ptproc), RTS_SLOT_FREE));
	assert(get_cpulocal_var(ptproc)->p_seg.p_cr3_v);

	return OK;
}


static u32_t phys_get32(phys_bytes addr)
{
	const u32_t v;
	int r;

	if(!vm_running) {
		phys_copy(addr, vir2phys(&v), sizeof(v));
		return v;
	}

	if((r=lin_lin_copy(NULL, addr, 
		proc_addr(SYSTEM), vir2phys(&v), sizeof(v))) != OK) {
		panic("lin_lin_copy for phys_get32 failed: %d",  r);
	}

	return v;
}

#if 0
static char *cr0_str(u32_t e)
{
	static char str[80];
	strcpy(str, "");
#define FLAG(v) do { if(e & (v)) { strcat(str, #v " "); e &= ~v; } } while(0)
	FLAG(I386_CR0_PE);
	FLAG(I386_CR0_MP);
	FLAG(I386_CR0_EM);
	FLAG(I386_CR0_TS);
	FLAG(I386_CR0_ET);
	FLAG(I386_CR0_PG);
	FLAG(I386_CR0_WP);
	if(e) { strcat(str, " (++)"); }
	return str;
}

static char *cr4_str(u32_t e)
{
	static char str[80];
	strcpy(str, "");
	FLAG(I386_CR4_VME);
	FLAG(I386_CR4_PVI);
	FLAG(I386_CR4_TSD);
	FLAG(I386_CR4_DE);
	FLAG(I386_CR4_PSE);
	FLAG(I386_CR4_PAE);
	FLAG(I386_CR4_MCE);
	FLAG(I386_CR4_PGE);
	if(e) { strcat(str, " (++)"); }
	return str;
}
#endif

void vm_stop(void)
{
	write_cr0(read_cr0() & ~I386_CR0_PG);
}

static void vm_enable_paging(void)
{
	u32_t cr0, cr4;
	int pgeok;

	psok = _cpufeature(_CPUF_I386_PSE);
	pgeok = _cpufeature(_CPUF_I386_PGE);

	cr0= read_cr0();
	cr4= read_cr4();

	/* First clear PG and PGE flag, as PGE must be enabled after PG. */
	write_cr0(cr0 & ~I386_CR0_PG);
	write_cr4(cr4 & ~(I386_CR4_PGE | I386_CR4_PSE));

	cr0= read_cr0();
	cr4= read_cr4();

	/* Our first page table contains 4MB entries. */
	if(psok)
		cr4 |= I386_CR4_PSE;

	write_cr4(cr4);

	/* First enable paging, then enable global page flag. */
	cr0 |= I386_CR0_PG;
	write_cr0(cr0 );
	cr0 |= I386_CR0_WP;
	write_cr0(cr0);

	/* May we enable these features? */
	if(pgeok)
		cr4 |= I386_CR4_PGE;

	write_cr4(cr4);
}

/*===========================================================================*
 *                              umap_local                                   *
 *===========================================================================*/
phys_bytes umap_local(rp, seg, vir_addr, bytes)
register struct proc *rp;       /* pointer to proc table entry for process */
int seg;                        /* T, D, or S segment */
vir_bytes vir_addr;             /* virtual address in bytes within the seg */
vir_bytes bytes;                /* # of bytes to be copied */
{
/* Calculate the physical memory address for a given virtual address. */
  vir_clicks vc;                /* the virtual address in clicks */
  phys_bytes pa;                /* intermediate variables as phys_bytes */
  phys_bytes seg_base;

  if(seg != T && seg != D && seg != S)
	panic("umap_local: wrong seg: %d",  seg);

  if (bytes <= 0) return( (phys_bytes) 0);
  if (vir_addr + bytes <= vir_addr) return 0;   /* overflow */
  vc = (vir_addr + bytes - 1) >> CLICK_SHIFT;   /* last click of data */
 
  if (seg != T)
        seg = (vc < rp->p_memmap[D].mem_vir + rp->p_memmap[D].mem_len ? D : S);
  else if (rp->p_memmap[T].mem_len == 0)	/* common I&D? */
        seg = D;				/* ptrace needs this */
 
  if ((vir_addr>>CLICK_SHIFT) >= rp->p_memmap[seg].mem_vir +
        rp->p_memmap[seg].mem_len) return( (phys_bytes) 0 );
 
  if (vc >= rp->p_memmap[seg].mem_vir +
        rp->p_memmap[seg].mem_len) return( (phys_bytes) 0 );
  
  seg_base = (phys_bytes) rp->p_memmap[seg].mem_phys;
  seg_base = seg_base << CLICK_SHIFT;   /* segment origin in bytes */
  pa = (phys_bytes) vir_addr;
  pa -= rp->p_memmap[seg].mem_vir << CLICK_SHIFT;
  return(seg_base + pa);
}

/*===========================================================================*
 *                              umap_virtual                                 *
 *===========================================================================*/
phys_bytes umap_virtual(rp, seg, vir_addr, bytes)
register struct proc *rp;       /* pointer to proc table entry for process */
int seg;                        /* T, D, or S segment */
vir_bytes vir_addr;             /* virtual address in bytes within the seg */
vir_bytes bytes;                /* # of bytes to be copied */
{
	vir_bytes linear;
	phys_bytes phys = 0;

	if(!(linear = umap_local(rp, seg, vir_addr, bytes))) {
			printf("SYSTEM:umap_virtual: umap_local failed\n");
			phys = 0;
		} else {
			if(vm_lookup(rp, linear, &phys, NULL) != OK) {
				printf("SYSTEM:umap_virtual: vm_lookup of %s: seg 0x%x: 0x%lx failed\n", rp->p_name, seg, vir_addr);
				phys = 0;
			} else {
				if(phys == 0)
					panic("vm_lookup returned phys: %d",  phys);
			}
		}
	

	if(phys == 0) {
		printf("SYSTEM:umap_virtual: lookup failed\n");
		return 0;
	}

	/* Now make sure addresses are contiguous in physical memory
	 * so that the umap makes sense.
	 */
	if(bytes > 0 && vm_lookup_range(rp, linear, NULL, bytes) != bytes) {
		printf("umap_virtual: %s: %lu at 0x%lx (vir 0x%lx) not contiguous\n",
			rp->p_name, bytes, linear, vir_addr);
		return 0;
	}

	/* phys must be larger than 0 (or the caller will think the call
	 * failed), and address must not cross a page boundary.
	 */
	assert(phys);

	return phys;
}


/*===========================================================================*
 *                              vm_lookup                                    *
 *===========================================================================*/
int vm_lookup(const struct proc *proc, const vir_bytes virtual,
 phys_bytes *physical, u32_t *ptent)
{
	u32_t *root, *pt;
	int pde, pte;
	u32_t pde_v, pte_v;

	assert(proc);
	assert(physical);
	assert(!isemptyp(proc));

	if(!HASPT(proc)) {
		*physical = virtual;
		return OK;
	}

	/* Retrieve page directory entry. */
	root = (u32_t *) proc->p_seg.p_cr3;
	assert(!((u32_t) root % I386_PAGE_SIZE));
	pde = I386_VM_PDE(virtual);
	assert(pde >= 0 && pde < I386_VM_DIR_ENTRIES);
	pde_v = phys_get32((u32_t) (root + pde));

	if(!(pde_v & I386_VM_PRESENT)) {
		return EFAULT;
	}

	/* We don't expect to ever see this. */
	if(pde_v & I386_VM_BIGPAGE) {
		*physical = pde_v & I386_VM_ADDR_MASK_4MB;
		if(ptent) *ptent = pde_v;
		*physical += virtual & I386_VM_OFFSET_MASK_4MB;
	} else {
		/* Retrieve page table entry. */
		pt = (u32_t *) I386_VM_PFA(pde_v);
		assert(!((u32_t) pt % I386_PAGE_SIZE));
		pte = I386_VM_PTE(virtual);
		assert(pte >= 0 && pte < I386_VM_PT_ENTRIES);
		pte_v = phys_get32((u32_t) (pt + pte));
		if(!(pte_v & I386_VM_PRESENT)) {
			return EFAULT;
		}

		if(ptent) *ptent = pte_v;

		/* Actual address now known; retrieve it and add page offset. */
		*physical = I386_VM_PFA(pte_v);
		*physical += virtual % I386_PAGE_SIZE;
	}

	return OK;
}

/*===========================================================================*
 *				vm_lookup_range				     *
 *===========================================================================*/
size_t vm_lookup_range(const struct proc *proc, vir_bytes vir_addr,
	phys_bytes *phys_addr, size_t bytes)
{
	/* Look up the physical address corresponding to linear virtual address
	 * 'vir_addr' for process 'proc'. Return the size of the range covered
	 * by contiguous physical memory starting from that address; this may
	 * be anywhere between 0 and 'bytes' inclusive. If the return value is
	 * nonzero, and 'phys_addr' is non-NULL, 'phys_addr' will be set to the
	 * base physical address of the range. 'vir_addr' and 'bytes' need not
	 * be page-aligned, but the caller must have verified that the given
	 * linear range is valid for the given process at all.
	 */
	phys_bytes phys, next_phys;
	size_t len;

	assert(proc);
	assert(bytes > 0);

	if (!HASPT(proc))
		return bytes;

	/* Look up the first page. */
	if (vm_lookup(proc, vir_addr, &phys, NULL) != OK)
		return 0;

	if (phys_addr != NULL)
		*phys_addr = phys;

	len = I386_PAGE_SIZE - (vir_addr % I386_PAGE_SIZE);
	vir_addr += len;
	next_phys = phys + len;

	/* Look up any next pages and test physical contiguity. */
	while (len < bytes) {
		if (vm_lookup(proc, vir_addr, &phys, NULL) != OK)
			break;

		if (next_phys != phys)
			break;

		len += I386_PAGE_SIZE;
		vir_addr += I386_PAGE_SIZE;
		next_phys += I386_PAGE_SIZE;
	}

	/* We might now have overshot the requested length somewhat. */
	return MIN(bytes, len);
}

/*===========================================================================*
 *                              vm_suspend                                *
 *===========================================================================*/
static void vm_suspend(struct proc *caller, const struct proc *target,
	const vir_bytes linaddr, const vir_bytes len, const int type)
{
	/* This range is not OK for this process. Set parameters  
	 * of the request and notify VM about the pending request. 
	 */								
	assert(!RTS_ISSET(caller, RTS_VMREQUEST));
	assert(!RTS_ISSET(target, RTS_VMREQUEST));

	RTS_SET(caller, RTS_VMREQUEST);

	caller->p_vmrequest.req_type = VMPTYPE_CHECK;
	caller->p_vmrequest.target = target->p_endpoint;
	caller->p_vmrequest.params.check.start = linaddr;
	caller->p_vmrequest.params.check.length = len;
	caller->p_vmrequest.params.check.writeflag = 1;
	caller->p_vmrequest.type = type;
							
	/* Connect caller on vmrequest wait queue. */	
	if(!(caller->p_vmrequest.nextrequestor = vmrequest))
		if(OK != send_sig(VM_PROC_NR, SIGKMEM))
			panic("send_sig failed");
	vmrequest = caller;
}

/*===========================================================================*
 *				vm_check_range				     *
 *===========================================================================*/
int vm_check_range(struct proc *caller, struct proc *target,
	vir_bytes vir_addr, size_t bytes)
{
	/* Public interface to vm_suspend(), for use by kernel calls. On behalf
	 * of 'caller', call into VM to check linear virtual address range of
	 * process 'target', starting at 'vir_addr', for 'bytes' bytes. This
	 * function assumes that it will called twice if VM returned an error
	 * the first time (since nothing has changed in that case), and will
	 * then return the error code resulting from the first call. Upon the
	 * first call, a non-success error code is returned as well.
	 */
	int r;

	if (!vm_running)
		return EFAULT;

	if ((caller->p_misc_flags & MF_KCALL_RESUME) &&
			(r = caller->p_vmrequest.vmresult) != OK)
		return r;

	vm_suspend(caller, target, vir_addr, bytes, VMSTYPE_KERNELCALL);

	return VMSUSPEND;
}

/*===========================================================================*
 *                              delivermsg                                *
 *===========================================================================*/
void delivermsg(struct proc *rp)
{
	int r = OK;

	assert(rp->p_misc_flags & MF_DELIVERMSG);
	assert(rp->p_delivermsg.m_source != NONE);

	if (copy_msg_to_user(rp, &rp->p_delivermsg,
				(message *) rp->p_delivermsg_vir)) {
		printf("WARNING wrong user pointer 0x%08lx from "
				"process %s / %d\n",
				rp->p_delivermsg_vir,
				rp->p_name,
				rp->p_endpoint);
		r = EFAULT;
	}

	/* Indicate message has been delivered; address is 'used'. */
	rp->p_delivermsg.m_source = NONE;
	rp->p_misc_flags &= ~MF_DELIVERMSG;

	if(!(rp->p_misc_flags & MF_CONTEXT_SET)) {
		rp->p_reg.retreg = r;
	}
}

#if 0
static char *flagstr(u32_t e, const int dir)
{
	static char str[80];
	strcpy(str, "");
	FLAG(I386_VM_PRESENT);
	FLAG(I386_VM_WRITE);
	FLAG(I386_VM_USER);
	FLAG(I386_VM_PWT);
	FLAG(I386_VM_PCD);
	FLAG(I386_VM_GLOBAL);
	if(dir)
		FLAG(I386_VM_BIGPAGE);	/* Page directory entry only */
	else
		FLAG(I386_VM_DIRTY);	/* Page table entry only */
	return str;
}

static void vm_pt_print(u32_t *pagetable, const u32_t v)
{
	int pte;
	int col = 0;

	assert(!((u32_t) pagetable % I386_PAGE_SIZE));

	for(pte = 0; pte < I386_VM_PT_ENTRIES; pte++) {
		u32_t pte_v, pfa;
		pte_v = phys_get32((u32_t) (pagetable + pte));
		if(!(pte_v & I386_VM_PRESENT))
			continue;
		pfa = I386_VM_PFA(pte_v);
		printf("%4d:%08lx:%08lx %2s ",
			pte, v + I386_PAGE_SIZE*pte, pfa,
			(pte_v & I386_VM_WRITE) ? "rw":"RO");
		col++;
		if(col == 3) { printf("\n"); col = 0; }
	}
	if(col > 0) printf("\n");

	return;
}

static void vm_print(u32_t *root)
{
	int pde;

	assert(!((u32_t) root % I386_PAGE_SIZE));

	printf("page table 0x%lx:\n", root);

	for(pde = 0; pde < I386_VM_DIR_ENTRIES; pde++) {
		u32_t pde_v;
		u32_t *pte_a;
		pde_v = phys_get32((u32_t) (root + pde));
		if(!(pde_v & I386_VM_PRESENT))
			continue;
		if(pde_v & I386_VM_BIGPAGE) {
			printf("%4d: 0x%lx, flags %s\n",
				pde, I386_VM_PFA(pde_v), flagstr(pde_v, 1));
		} else {
			pte_a = (u32_t *) I386_VM_PFA(pde_v);
			printf("%4d: pt %08lx %s\n",
				pde, pte_a, flagstr(pde_v, 1));
			vm_pt_print(pte_a, pde * I386_VM_PT_ENTRIES * I386_PAGE_SIZE);
			printf("\n");
		}
	}


	return;
}
#endif

int vm_memset(endpoint_t who, phys_bytes ph, const u8_t c, phys_bytes bytes)
{
	u32_t p;
	int r = OK;
	struct proc *whoptr = NULL;
	
	/* NONE for physical, otherwise virtual */
	if(who != NONE) {
		int n;
		vir_bytes lin;
		assert(vm_running);
		if(!isokendpt(who, &n)) return ESRCH;
		whoptr = proc_addr(n);
	        if(!(lin = umap_local(whoptr, D, ph, bytes))) return EFAULT;
		ph = lin;
	} 
	
	p = c | (c << 8) | (c << 16) | (c << 24);

	if(!vm_running) {
		if(who != NONE) panic("can't vm_memset without vm running");
		phys_memset(ph, p, bytes);
		return OK;
	}

	assert(nfreepdes >= MAX_FREEPDES);

	assert(get_cpulocal_var(ptproc)->p_seg.p_cr3_v);

	assert(!catch_pagefaults);
	catch_pagefaults=1;

	/* With VM, we have to map in the memory (virtual or physical). 
	 * We can do this 4MB at a time.
	 */
	while(bytes > 0) {
		int changed = 0;
		phys_bytes chunk = bytes, ptr, pfa;
		ptr = createpde(whoptr, ph, &chunk, 0, &changed);
		if(changed)
			reload_cr3(); 

		/* We can memset as many bytes as we have remaining,
		 * or as many as remain in the 4MB chunk we mapped in.
		 */
		if((pfa=phys_memset(ptr, p, chunk))) {
			printf("kernel memset pagefault\n");
			r = EFAULT;
			break;
		}
		bytes -= chunk;
		ph += chunk;
	}

	assert(catch_pagefaults);
	catch_pagefaults=0;

	assert(get_cpulocal_var(ptproc)->p_seg.p_cr3_v);

	return OK;
}

/*===========================================================================*
 *				virtual_copy_f				     *
 *===========================================================================*/
int virtual_copy_f(caller, src_addr, dst_addr, bytes, vmcheck)
struct proc * caller;
struct vir_addr *src_addr;	/* source virtual address */
struct vir_addr *dst_addr;	/* destination virtual address */
vir_bytes bytes;		/* # of bytes to copy  */
int vmcheck;			/* if nonzero, can return VMSUSPEND */
{
/* Copy bytes from virtual address src_addr to virtual address dst_addr. */
  struct vir_addr *vir_addr[2];	/* virtual source and destination address */
  phys_bytes phys_addr[2];	/* absolute source and destination */ 
  int seg_index;
  int i;
  struct proc *procs[2];

  assert((vmcheck && caller) || (!vmcheck && !caller));

  /* Check copy count. */
  if (bytes <= 0) return(EDOM);

  /* Do some more checks and map virtual addresses to physical addresses. */
  vir_addr[_SRC_] = src_addr;
  vir_addr[_DST_] = dst_addr;

  for (i=_SRC_; i<=_DST_; i++) {
	int proc_nr, type;
	struct proc *p;

 	type = vir_addr[i]->segment & SEGMENT_TYPE;
	if((type != PHYS_SEG) && isokendpt(vir_addr[i]->proc_nr_e, &proc_nr))
		p = proc_addr(proc_nr);
	else 
		p = NULL;

	procs[i] = p;

      /* Get physical address. */
      switch(type) {
      case LOCAL_SEG:
      case LOCAL_VM_SEG:
	  if(!p) {
		return EDEADSRCDST;
	  }
          seg_index = vir_addr[i]->segment & SEGMENT_INDEX;
	  if(type == LOCAL_SEG)
	          phys_addr[i] = umap_local(p, seg_index, vir_addr[i]->offset,
			bytes);
	  else
	  	phys_addr[i] = umap_virtual(p, seg_index,
				vir_addr[i]->offset, bytes);
	  if(phys_addr[i] == 0) {
		printf("virtual_copy: map 0x%x failed for %s seg %d, "
			"offset %lx, len %lu, i %d\n",
			type, p->p_name, seg_index, vir_addr[i]->offset,
			bytes, i);
	  }
          break;
      case PHYS_SEG:
          phys_addr[i] = vir_addr[i]->offset;
          break;
      default:
	  printf("virtual_copy: strange type 0x%x\n", type);
	  return EINVAL;
      }

      /* Check if mapping succeeded. */
      if (phys_addr[i] <= 0 && vir_addr[i]->segment != PHYS_SEG)  {
      printf("virtual_copy EFAULT\n");
	  return EFAULT;
      }
  }

  if(vm_running) {
	int r;

	if(caller && (caller->p_misc_flags & MF_KCALL_RESUME)) {
		assert(caller->p_vmrequest.vmresult != VMSUSPEND);
		if(caller->p_vmrequest.vmresult != OK) {
	  		return caller->p_vmrequest.vmresult;
		}
	}

	if((r=lin_lin_copy(procs[_SRC_], phys_addr[_SRC_],
		procs[_DST_], phys_addr[_DST_], bytes)) != OK) {
		struct proc *target = NULL;
		phys_bytes lin;
		if(r != EFAULT_SRC && r != EFAULT_DST)
			panic("lin_lin_copy failed: %d",  r);
		if(!vmcheck || !caller) {
	  		return r;
		}

		if(r == EFAULT_SRC) {
			lin = phys_addr[_SRC_];
			target = procs[_SRC_];
		} else if(r == EFAULT_DST) {
			lin = phys_addr[_DST_];
			target = procs[_DST_];
		} else {
			panic("r strange: %d",  r);
		}

		assert(caller);
		assert(target);

		vm_suspend(caller, target, lin, bytes, VMSTYPE_KERNELCALL);
		return VMSUSPEND;
	}

  	return OK;
  }

  assert(!vm_running);

  /* can't copy to/from process with PT without VM */
#define NOPT(p) (!(p) || !HASPT(p))
  if(!NOPT(procs[_SRC_])) {
	printf("ignoring page table src: %s / %d at 0x%x\n",
		procs[_SRC_]->p_name, procs[_SRC_]->p_endpoint, procs[_SRC_]->p_seg.p_cr3);
}
  if(!NOPT(procs[_DST_])) {
	printf("ignoring page table dst: %s / %d at 0x%x\n",
		procs[_DST_]->p_name, procs[_DST_]->p_endpoint,
		procs[_DST_]->p_seg.p_cr3);
  }

  /* Now copy bytes between physical addresseses. */
  if(phys_copy(phys_addr[_SRC_], phys_addr[_DST_], (phys_bytes) bytes))
  	return EFAULT;
 
  return OK;
}

/*===========================================================================*
 *				data_copy				     *
 *===========================================================================*/
int data_copy(const endpoint_t from_proc, const vir_bytes from_addr,
	const endpoint_t to_proc, const vir_bytes to_addr,
	size_t bytes)
{
  struct vir_addr src, dst;

  src.segment = dst.segment = D;
  src.offset = from_addr;
  dst.offset = to_addr;
  src.proc_nr_e = from_proc;
  dst.proc_nr_e = to_proc;

  return virtual_copy(&src, &dst, bytes);
}

/*===========================================================================*
 *				data_copy_vmcheck			     *
 *===========================================================================*/
int data_copy_vmcheck(struct proc * caller,
	const endpoint_t from_proc, const vir_bytes from_addr,
	const endpoint_t to_proc, const vir_bytes to_addr,
	size_t bytes)
{
  struct vir_addr src, dst;

  src.segment = dst.segment = D;
  src.offset = from_addr;
  dst.offset = to_addr;
  src.proc_nr_e = from_proc;
  dst.proc_nr_e = to_proc;

  return virtual_copy_vmcheck(caller, &src, &dst, bytes);
}

/*===========================================================================*
 *				arch_pre_exec				     *
 *===========================================================================*/
void arch_pre_exec(struct proc *pr, const u32_t ip, const u32_t sp)
{
/* set program counter and stack pointer. */
	pr->p_reg.pc = ip;
	pr->p_reg.sp = sp;
}

/* VM reports page directory slot we're allowed to use freely. */
void i386_freepde(const int pde)
{
	if(nfreepdes >= MAX_FREEPDES)
		return;
	freepdes[nfreepdes++] = pde;
}

static int oxpcie_mapping_index = -1,
	lapic_mapping_index = -1,
	ioapic_first_index = -1,
	ioapic_last_index = -1;

int arch_phys_map(const int index,
			phys_bytes *addr,
			phys_bytes *len,
			int *flags)
{
	static int first = 1;
	int freeidx = 0;
	static char *ser_var = NULL;

	if(first) {
#ifdef USE_APIC
		if(lapic_addr)
			lapic_mapping_index = freeidx++;
		if (ioapic_enabled) {
			ioapic_first_index = freeidx;
			assert(nioapics > 0);
			freeidx += nioapics;
			ioapic_last_index = freeidx-1;
		}
#endif

#ifdef CONFIG_OXPCIE
		if((ser_var = env_get("oxpcie"))) {
			if(ser_var[0] != '0' || ser_var[1] != 'x') {
				printf("oxpcie address in hex please\n");
			} else {
				printf("oxpcie address is %s\n", ser_var);
				oxpcie_mapping_index = freeidx++;
			}
		}
#endif
		first = 0;
	}

#ifdef USE_APIC
	/* map the local APIC if enabled */
	if (index == lapic_mapping_index) {
		if (!lapic_addr)
			return EINVAL;
		*addr = vir2phys(lapic_addr);
		*len = 4 << 10 /* 4kB */;
		*flags = VMMF_UNCACHED;
		return OK;
	}
	else if (ioapic_enabled && index <= nioapics) {
		*addr = io_apic[index - 1].paddr;
		*len = 4 << 10 /* 4kB */;
		*flags = VMMF_UNCACHED;
		return OK;
	}
#endif

#if CONFIG_OXPCIE
	if(index == oxpcie_mapping_index) {
		*addr = strtoul(ser_var+2, NULL, 16);
		*len = 0x4000;
		*flags = VMMF_UNCACHED;
		return OK;
	}
#endif

	return EINVAL;
}

int arch_phys_map_reply(const int index, const vir_bytes addr)
{
#ifdef USE_APIC
	/* if local APIC is enabled */
	if (index == lapic_mapping_index && lapic_addr) {
		lapic_addr_vaddr = addr;
		return OK;
	}
	else if (ioapic_enabled && index >= ioapic_first_index &&
		index <= ioapic_last_index) {
		io_apic[index - ioapic_first_index].vaddr = addr;
		return OK;
	}
#endif

#if CONFIG_OXPCIE
	if (index == oxpcie_mapping_index) {
		oxpcie_set_vaddr((unsigned char *) addr);
		return OK;
	}
#endif

	return EINVAL;
}

int arch_enable_paging(struct proc * caller, const message * m_ptr)
{
	struct vm_ep_data ep_data;
	int r;

	/* switch_address_space() checks what is in cr3, and do nothing if it's
	 * the same as the cr3 of its argument, newptproc.  If MINIX was
	 * previously booted, this could very well be the case.
	 *
	 * The first time switch_address_space() is called, we want to
	 * force it to do something (load cr3 and set newptproc), so we
	 * zero cr3, and force paging off to make that a safe thing to do.
	 *
	 * After that, segmentation2paging() enables paging with the page table
	 * of caller loaded.
	 */

	vm_stop();
	write_cr3(0);

	/* switch from segmentation only to paging */
	segmentation2paging(caller);

	vm_running = 1;

	/*
	 * copy the extra data associated with the call from userspace
	 */
	if((r=data_copy(caller->p_endpoint, (vir_bytes)m_ptr->SVMCTL_VALUE,
		KERNEL, (vir_bytes) &ep_data, sizeof(ep_data))) != OK) {
		printf("vmctl_enable_paging: data_copy failed! (%d)\n", r);
		return r;
	}

	/*
	 * when turning paging on i386 we also change the segment limits to make
	 * the special mappings requested by the kernel reachable
	 */
	if ((r = prot_set_kern_seg_limit(ep_data.data_seg_limit)) != OK)
		return r;

	/*
	 * install the new map provided by the call
	 */
	if (newmap(caller, caller, ep_data.mem_map) != OK)
		panic("arch_enable_paging: newmap failed");

#ifdef USE_APIC
	/* start using the virtual addresses */

	/* if local APIC is enabled */
	if (lapic_addr) {
		lapic_addr = lapic_addr_vaddr;
		lapic_eoi_addr = LAPIC_EOI;
	}
	/* if IO apics are enabled */
	if (ioapic_enabled) {
		int i;

		for (i = 0; i < nioapics; i++) {
			io_apic[i].addr = io_apic[i].vaddr;
		}
	}
#if CONFIG_SMP
	barrier();

	i386_paging_enabled = 1;

	wait_for_APs_to_finish_booting();
#endif
#endif

#ifdef USE_WATCHDOG
	/*
	 * We make sure that we don't enable the watchdog until paging is turned
	 * on as we might get an NMI while switching and we might still use wrong
	 * lapic address. Bad things would happen. It is unfortunate but such is
	 * life
	 */
	if (watchdog_enabled)
		i386_watchdog_start();
#endif

	return OK;
}

void release_address_space(struct proc *pr)
{
	pr->p_seg.p_cr3_v = NULL;
}

/* computes a checksum of a buffer of a given length. The byte sum must be zero */
int platform_tbl_checksum_ok(void *ptr, unsigned int length)
{
	u8_t total = 0;
	unsigned int i;
	for (i = 0; i < length; i++)
		total += ((unsigned char *)ptr)[i];
	return !total;
}

int platform_tbl_ptr(phys_bytes start,
					phys_bytes end,
					unsigned increment,
					void * buff,
					unsigned size,
					phys_bytes * phys_addr,
					int ((* cmp_f)(void *)))
{
	phys_bytes addr;

	for (addr = start; addr < end; addr += increment) {
		phys_copy (addr, vir2phys(buff), size);
		if (cmp_f(buff)) {
			if (phys_addr)
				*phys_addr = addr;
			return 1;
		}
	}
	return 0;
}
