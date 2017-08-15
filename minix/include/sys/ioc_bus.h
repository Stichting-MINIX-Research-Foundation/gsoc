/*	sys/ioc_block.h - Block ioctl() command codes.
 *
 */

#ifndef _S_I_BUS_H
#define _S_I_BUS_H

#include <minix/ioctl.h>
#include <minix/btrace.h>

#define SPIIORATE	_IOW('U', 1, unsigned int)
#define SPIIOMODE	_IOW('U', 2, unsigned int)

#endif /* _S_I_BUS_H */