#include "kernel/system.h"
#include <minix/endpoint.h>

#if USE_PADCONF

/* get arch specific arch_padconf_set() */
#include "bsp_padconf.h"
#include "bsp_table.h"

extern bsp_table *bsp_tb;

/*===========================================================================*
 *                                do_padconf                                 *
 *===========================================================================*/
int do_padconf(struct proc *caller_ptr, message *m_ptr)
{
	return bsp_tb->bsp_padconf_set(m_ptr->PADCONF_PADCONF, m_ptr->PADCONF_MASK,
	    m_ptr->PADCONF_VALUE);
}

#endif /* USE_PADCONF */
