#include "fs.h"
#include <assert.h>
#include <minix/vfsif.h>
#include <minix/bdev.h>
#include "inode.h"
#include "clean.h"

/*===========================================================================*
 *				fs_sync					     *
 *===========================================================================*/
int fs_sync()
{
/* Perform the sync() system call.  Flush all the tables. 
 * The order in which the various tables are flushed is critical.  The
 * blocks must be flushed last, since rw_inode() leaves its results in
 * the block cache.
 */
  struct inode *rip;
  struct buf *bp;

  assert(nr_bufs > 0);
  assert(buf);

  /* Write all the dirty inodes to the disk. */
  for(rip = &inode[0]; rip < &inode[NR_INODES]; rip++)
	  if(rip->i_count > 0 && IN_ISDIRTY(rip)) rw_inode(rip, WRITING);

  /* Write all the dirty blocks to the disk, one drive at a time. */
  for(bp = &buf[0]; bp < &buf[nr_bufs]; bp++)
	  if(bp->b_dev != NO_DEV && ISDIRTY(bp)) 
		  flushall(bp->b_dev);

  return(OK);		/* sync() can't fail */
}


/*===========================================================================*
 *				fs_flush				     *
 *===========================================================================*/
int fs_flush()
{
/* Flush the blocks of a device from the cache after writing any dirty blocks
 * to disk.
 */
  dev_t dev = (dev_t) fs_m_in.REQ_DEV;
  if(dev == fs_dev) return(EBUSY);
 
  flushall(dev);
  invalidate(dev);

  return(OK);
}


/*===========================================================================*
 *				fs_new_driver				     *
 *===========================================================================*/
int fs_new_driver(void)
{
/* Set a new driver endpoint for this device. */
  dev_t dev;
  cp_grant_id_t label_gid;
  size_t label_len;
  char label[sizeof(fs_dev_label)];
  int r;

  dev = (dev_t) fs_m_in.REQ_DEV;
  label_gid = (cp_grant_id_t) fs_m_in.REQ_GRANT;
  label_len = (size_t) fs_m_in.REQ_PATH_LEN;

  if (label_len > sizeof(label))
	return(EINVAL);

  r = sys_safecopyfrom(fs_m_in.m_source, label_gid, (vir_bytes) 0,
	(vir_bytes) label, label_len);

  if (r != OK) {
	printf("MFS: fs_new_driver safecopyfrom failed (%d)\n", r);
	return(EINVAL);
  }

  bdev_driver(dev, label);

  return(OK);
}
