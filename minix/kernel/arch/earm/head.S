
#include "kernel/kernel.h" /* configures the kernel */

/* sections */

#include <machine/vm.h>
#include "kernel/kernel.h"
#include <minix/config.h>
#include <minix/const.h>
#include <minix/com.h>
#include <machine/asm.h>
#include <machine/interrupt.h>
#include "archconst.h"
#include "kernel/const.h"
#include "kernel/proc.h"
#include "sconst.h"
#include <machine/multiboot.h>
#include <machine/cpu.h>

#include "arch_proto.h" /* K_STACK_SIZE */

.section .startup
/*==================================================*/
/*						MINIX						*/
/*==================================================*/
.global MINIX
MINIX:
/* this is the entry point for the MINIX kernel */
	b multiboot_init

.text
multiboot_init:

/* 
 * We enable paging on early stage of booting in this file.
 * Only the head.o contains __k_unpaged code. 
 *
 * To make platform-independent boot system we should not keep _kern_phys_base
 * as a constant. We should know it at run-time from program counter
 * and create page table relatively it.
 *
 * NOTE! 
 * We should not use r0 and r1 here, because they contain external parametrs.
 *
 * in r0 - argc
 * in r1 - argv
 *
 * r4 - pg_table
 * r9 - _kern_phys_base
 *
 * out r0 - argc
 * out r1 - argv
 */ 

	ldr r6, =0xFFF00000
	and r9, pc, r6

	mov r6, #0
	/* Clean cache */
	mcr p15, 0, r6, c7, c14, 2

	ldr r4, =pg_table
	mov r6, #0

/****************************************************/
/*				Clear kernel bss section 			*/
/****************************************************/

	ldr	r2, =_edata
	ldr r3, =_end
	sub r3, r3, r2
	mov	r6, #0
	mov r5, #0
	b check1
1:	
	str	r6, [r2], #4
	add r5, r5, #1
check1:
	cmp r5, r3
	bne	1b

/****************************************************/
/*			Clear kernel unpaged bss section 		*/
/****************************************************/

	ldr	r2, =_kern_unpaged_edata
	ldr r3, =_kern_unpaged_end
	sub r3, r3, r2
	mov	r6, #0
	mov r5, #0
	b check2
1:	
	str	r6, [r2], #4
	add r5, r5, #1
check2:
	cmp r5, r3
	bne	1b

/****************************************************/
/*				Clear the page table 				*/
/****************************************************/
/* The page table is initialized by zeroes			*/
/* (the page table are not in the bss section).		*/
/****************************************************/

	mov	r2, r4
	mov	r3, #0
	add	r6, r2, #0x4000
1:	
	str	r3, [r2], #4
	str	r3, [r2], #4
	str	r3, [r2], #4
	str	r3, [r2], #4
	teq	r2, r6
	bne	1b

/****************************************************/
/*				Page identify						*/
/****************************************************/
/* Page table is filled by cahced or device memory	*/
/* It requires, because it will be better, if all	*/
/* entries are initialized.							*/
/****************************************************/
	mov r6, #0
	mov r7, #0
	mov r3, #0
	orr r3, r3, #ARM_VM_SECTION
	orr r3, r3, #ARM_VM_SECTION_USER
	orr r3, r3, #ARM_VM_SECTION_DOMAIN
1:
	ldr r5, =0x3effffff //FIXME
	cmp r7, r5
	blo cached
	ldr r2, =#ARM_VM_SECTION_DEVICE
	b setup
cached:
	ldr r2, =#ARM_VM_SECTION_CACHED
setup:
	orr r3, r3, r2
	orr r2, r3, r7
	add r7, r7, #ARM_SECTION_SIZE
	str r2, [r4, r6, lsl #2]
	add r6, r6, #1
	cmp r6, #ARM_VM_DIR_ENTRIES
	blo 1b

/****************************************************/
/*					Map kernel						*/
/****************************************************/
/* Kernel maps from _kern_phys_base, which locates	*/
/* in r9, to _kern_vir_base (0xf0400000).			*/
/* The first free entry in the page table is placed	*/
/* in freepde.										*/
/****************************************************/
	ldr	r6, =0xf0400000 //FIXME
	mov r7, r9
	add	r5, r4, r6, lsr #18

	mov r8, #0
	orr r8, r8, #ARM_VM_SECTION
	orr r8, r8, #ARM_VM_SECTION_SUPER
	orr r8, r8, #ARM_VM_SECTION_DOMAIN
	ldr r2, =#ARM_VM_SECTION_CACHED
	orr r8, r8, r2
	ldr r3, =#ARM_VM_SECTION_MASK
1:	
	and r2, r7, r3
	orr r2, r2, r8
	str	r2, [r5], #4
	add	r7, r7, #ARM_SECTION_SIZE
	cmp	r7, r6
	blo	1b

	ldr r3, =freepde
	sub r5, r5, r4
	lsr r5, #3
	str r5, [r3]

/****************************************************/
/*				Load the page table 				*/
/****************************************************/
	dsb
	isb
	ldr r2, =#ARM_TTBR_ADDR_MASK
	and r2, r4, r2
	orr r2, r2, #ARM_TTBR_FLAGS_CACHED
	/* Write page table base */
	mcr p15, 0, r2, c2, c0, 0
	dsb
	mov r2, #0
	/* Invalidate unified TLB register */
	mcr p15, 0, r2, c8, c7, 0
	/* Invalidate entire instruction cache register */
	mcr p15, 0, r2, c7, c5, 0
	/* Flush entire branch target cache register */
	mcr p15, 0, r2, c7, c5, 6
	dsb
	isb

/****************************************************/
/*				Enable paging  						*/
/****************************************************/
	/* Read translation table base control register */
	mcr p15, 0, r2, c2, c0, 2
	ldr r2, =0x55555555
	/* Read domain access control register */
	mcr p15, 0, r2, c3, c0, 0
	isb
	/* Read control register */
	mrc p15, 0, r3, c1, c0, 0
	isb
	orr r3, r3, #CPU_CONTROL_32BD_ENABLE
	orr r3, r3, #CPU_CONTROL_MMU_ENABLE
	orr r3, r3, #CPU_CONTROL_DC_ENABLE
	orr r3, r3, #CPU_CONTROL_IC_ENABLE
	orr r3, r3, #CPU_CONTROL_BPRD_ENABLE
	/* Read auxiliary control register */
	mrc p15, 0, r2, c1, c0, 1
	orr r2, r2, #2
	/* Write auxiliary control register */
	mcr p15, 0, r2, c1, c0, 1
	isb
	/* Write control register */
	mcr p15, 0, r3, c1, c0, 0
	isb

	ldr	sp, =k_initial_stktop	/* make usable stack */
	/* Terminate stack */
	mov	fp, #0
	mov	r2, #0
	push	{r2}	
	bl _C_LABEL(pre_init)

	ldr r4, =1384 // FIXME
	add r3, r0, r4
	ldr r5, =freepde
	ldr r5, [r5]
	str r5, [r3]

	/* Kernel is mapped high now and ready to go, with
	 * the boot info pointer returned by pre_init in r0.
	 * Set the highly mapped stack and initialize it.
	 *
	 * Afther that call kmain with r0 still pointing to boot info
	 */
	ldr	r2, =_C_LABEL(kmain)	/* r0 holds kinfo_t ptr */
	bx	r2

	/* not reached */
hang:
	b hang

.data
	.balign 16384
pg_table:
	.space 16384
freepde:
	.word 0

