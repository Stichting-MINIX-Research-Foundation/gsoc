/* The file system maintains a buffer cache to reduce the number of disk
 * accesses needed.  Whenever a read or write to the disk is done, a check is
 * first made to see if the block is in the cache.  This file manages the
 * cache.
 *
 * The entry points into this file are:
 *   get_block:	  request to fetch a block for reading or writing from cache
 *   put_block:	  return a block previously requested with get_block
 *   invalidate:  remove all the cache blocks on some device
 *
 * Private functions:
 *   rw_block:    read or write a block from the disk itself
 *
 * Created (MFS based):
 *   February 2010 (Evgeniy Ivanov)
 */

#include "fs.h"
#include <minix/u64.h>
#include <minix/bdev.h>
#include <minix/libminixfs.h>
#include <stdlib.h>
#include <assert.h>
#include "buf.h"
#include "super.h"
#include "inode.h"

static void rm_lru(struct buf *bp);
static void rw_block(struct buf *, int);

int vmcache = 0; /* are we using vm's secondary cache? (initially not) */

static block_t super_start = 0, super_end = 0;

/*===========================================================================*
 *				get_block				     *
 *===========================================================================*/
struct buf *get_block(
  register dev_t dev,		/* on which device is the block? */
  register block_t block,	/* which block is wanted? */
  int only_search		/* if NO_READ, don't read, else act normal */
)
{
/* Check to see if the requested block is in the block cache.  If so, return
 * a pointer to it.  If not, evict some other block and fetch it (unless
 * 'only_search' is 1).  All the blocks in the cache that are not in use
 * are linked together in a chain, with 'front' pointing to the least recently
 * used block and 'rear' to the most recently used block.  If 'only_search' is
 * 1, the block being requested will be overwritten in its entirety, so it is
 * only necessary to see if it is in the cache; if it is not, any free buffer
 * will do.  It is not necessary to actually read the block in from disk.
 * If 'only_search' is PREFETCH, the block need not be read from the disk,
 * and the device is not to be marked on the block, so callers can tell if
 * the block returned is valid.
 * In addition to the LRU chain, there is also a hash chain to link together
 * blocks whose block numbers end with the same bit strings, for fast lookup.
 */

  int b;
  static struct buf *bp, *prev_ptr;
  u64_t yieldid = VM_BLOCKID_NONE, getid = make64(dev, block);

  assert(buf_hash);
  assert(buf);
  assert(nr_bufs > 0);

  ASSERT(fs_block_size > 0);

  /* Search the hash chain for (dev, block). Do_read() can use
   * get_block(NO_DEV ...) to get an unnamed block to fill with zeros when
   * someone wants to read from a hole in a file, in which case this search
   * is skipped
   */
  if (dev != NO_DEV) {
	b = BUFHASH(block);
	bp = buf_hash[b];
	while (bp != NULL) {
		if (bp->b_blocknr == block && bp->b_dev == dev) {
			/* Block needed has been found. */
			if (bp->b_count == 0) rm_lru(bp);
			bp->b_count++;	/* record that block is in use */
			ASSERT(bp->b_bytes == fs_block_size);
			ASSERT(bp->b_dev == dev);
			ASSERT(bp->b_dev != NO_DEV);
			ASSERT(bp->bp);
			return(bp);
		} else {
			/* This block is not the one sought. */
			bp = bp->b_hash; /* move to next block on hash chain */
		}
	}
  }

  /* Desired block is not on available chain.  Take oldest block ('front'). */
  if ((bp = front) == NULL) panic("all buffers in use", nr_bufs);

  if(bp->b_bytes < fs_block_size) {
	ASSERT(!bp->bp);
	ASSERT(bp->b_bytes == 0);
	if(!(bp->bp = alloc_contig( (size_t) fs_block_size, 0, NULL))) {
		ext2_debug("ext2: couldn't allocate a new block.\n");
		for(bp = front;
			bp && bp->b_bytes < fs_block_size; bp = bp->b_next)
			;
		if(!bp) {
			panic("no buffer available");
		}
	} else {
		bp->b_bytes = fs_block_size;
	}
  }

  ASSERT(bp);
  ASSERT(bp->bp);
  ASSERT(bp->b_bytes == fs_block_size);
  ASSERT(bp->b_count == 0);

  rm_lru(bp);

  /* Remove the block that was just taken from its hash chain. */
  b = BUFHASH(bp->b_blocknr);
  prev_ptr = buf_hash[b];
  if (prev_ptr == bp) {
	buf_hash[b] = bp->b_hash;
  } else {
	/* The block just taken is not on the front of its hash chain. */
	while (prev_ptr->b_hash != NULL)
		if (prev_ptr->b_hash == bp) {
			prev_ptr->b_hash = bp->b_hash;	/* found it */
			break;
		} else {
			prev_ptr = prev_ptr->b_hash;	/* keep looking */
		}
  }

  /* If the block taken is dirty, make it clean by writing it to the disk.
   * Avoid hysteresis by flushing all other dirty blocks for the same device.
   */
  if (bp->b_dev != NO_DEV) {
	if (bp->b_dirt == DIRTY) flushall(bp->b_dev);

	/* Are we throwing out a block that contained something?
	 * Give it to VM for the second-layer cache.
	 */
	yieldid = make64(bp->b_dev, bp->b_blocknr);
	assert(bp->b_bytes == fs_block_size);
	bp->b_dev = NO_DEV;
  }

  /* Fill in block's parameters and add it to the hash chain where it goes. */
  bp->b_dev = dev;		/* fill in device number */
  bp->b_blocknr = block;	/* fill in block number */
  bp->b_count++;		/* record that block is being used */
  b = BUFHASH(bp->b_blocknr);
  bp->b_hash = buf_hash[b];

  buf_hash[b] = bp;		/* add to hash list */

  if(dev == NO_DEV) {
	if(vmcache && cmp64(yieldid, VM_BLOCKID_NONE) != 0) {
		vm_yield_block_get_block(yieldid, VM_BLOCKID_NONE,
			bp->bp, fs_block_size);
	}
	return(bp);	/* If the caller wanted a NO_DEV block, work is done. */
  }

  /* Go get the requested block unless searching or prefetching. */
  if(only_search == PREFETCH || only_search == NORMAL) {
	/* Block is not found in our cache, but we do want it
	 * if it's in the vm cache.
	 */
	if(vmcache) {
		/* If we can satisfy the PREFETCH or NORMAL request
		 * from the vm cache, work is done.
		 */
		if(vm_yield_block_get_block(yieldid, getid,
			bp->bp, fs_block_size) == OK) {
			return bp;
		}
	}
  }

  if(only_search == PREFETCH) {
	/* PREFETCH: don't do i/o. */
	bp->b_dev = NO_DEV;
  } else if (only_search == NORMAL) {
	rw_block(bp, READING);
  } else if(only_search == NO_READ) {
	/* we want this block, but its contents
	 * will be overwritten. VM has to forget
	 * about it.
	 */
	if(vmcache) {
		vm_forgetblock(getid);
	}
  } else
	panic("unexpected only_search value: %d", only_search);

  assert(bp->bp);

  return(bp);			/* return the newly acquired block */
}

/*===========================================================================*
 *				put_block				     *
 *===========================================================================*/
void put_block(
  register struct buf *bp,	/* pointer to the buffer to be released */
  int block_type 		/* INODE_BLOCK, DIRECTORY_BLOCK, or whatever */
)
{
/* Return a block to the list of available blocks.   Depending on 'block_type'
 * it may be put on the front or rear of the LRU chain.  Blocks that are
 * expected to be needed again shortly (e.g., partially full data blocks)
 * go on the rear; blocks that are unlikely to be needed again shortly
 * (e.g., full data blocks) go on the front.  Blocks whose loss can hurt
 * the integrity of the file system (e.g., inode blocks) are written to
 * disk immediately if they are dirty.
 */
  if (bp == NULL) return;	/* it is easier to check here than in caller */

  bp->b_count--;		/* there is one use fewer now */
  if (bp->b_count != 0) return;	/* block is still in use */

  bufs_in_use--;		/* one fewer block buffers in use */

  /* Put this block back on the LRU chain.  If the ONE_SHOT bit is set in
   * 'block_type', the block is not likely to be needed again shortly, so put
   * it on the front of the LRU chain where it will be the first one to be
   * taken when a free buffer is needed later.
   */
  if (bp->b_dev == DEV_RAM || (block_type & ONE_SHOT)) {
	/* Block probably won't be needed quickly. Put it on front of chain.
	 * It will be the next block to be evicted from the cache.
	 */
	bp->b_prev = NULL;
	bp->b_next = front;
	if (front == NULL)
		rear = bp;	/* LRU chain was empty */
	else
		front->b_prev = bp;
	front = bp;
  }
  else {
	/* Block probably will be needed quickly.  Put it on rear of chain.
	 * It will not be evicted from the cache for a long time.
	 */
	bp->b_prev = rear;
	bp->b_next = NULL;
	if (rear == NULL)
		front = bp;
	else
		rear->b_next = bp;
	rear = bp;
  }

  /* Some blocks are so important (e.g., inodes, indirect blocks) that they
   * should be written to the disk immediately to avoid messing up the file
   * system in the event of a crash.
   */
  if ((block_type & WRITE_IMMED) && bp->b_dirt==DIRTY && bp->b_dev != NO_DEV) {
		rw_block(bp, WRITING);
  }
}


/*===========================================================================*
 *				rw_block				     *
 *===========================================================================*/
static void rw_block(
  register struct buf *bp,	/* buffer pointer */
  int rw_flag 			/* READING or WRITING */
)
{
/* Read or write a disk block. This is the only routine in which actual disk
 * I/O is invoked. If an error occurs, a message is printed here, but the error
 * is not reported to the caller.  If the error occurred while purging a block
 * from the cache, it is not clear what the caller could do about it anyway.
 */
  int r, op_failed = 0;
  u64_t pos;
  dev_t dev;

  if ( (dev = bp->b_dev) != NO_DEV) {
	pos = mul64u(bp->b_blocknr, fs_block_size);
	if (rw_flag == READING)
		r = bdev_read(dev, pos, bp->b_data, fs_block_size,
			BDEV_NOFLAGS);
	else
		r = bdev_write(dev, pos, bp->b_data, fs_block_size,
			BDEV_NOFLAGS);
	if (r < 0) {
		printf("Ext2(%d) I/O error on device %d/%d, block %u\n",
			SELF_E, major(dev), minor(dev), bp->b_blocknr);
		op_failed = 1;
	} else if (r != (ssize_t) fs_block_size) {
		r = END_OF_FILE;
		op_failed = 1;
	}

	if (op_failed) {
		bp->b_dev = NO_DEV;     /* invalidate block */

		/* Report read errors to interested parties. */
		if (rw_flag == READING) rdwt_err = r;

	}
  }

  bp->b_dirt = CLEAN;
}

/*===========================================================================*
 *				invalidate				     *
 *===========================================================================*/
void invalidate(
  dev_t device 		/* device whose blocks are to be purged */
)
{
/* Remove all the blocks belonging to some device from the cache. */

  register struct buf *bp;

  for (bp = &buf[0]; bp < &buf[nr_bufs]; bp++)
	if (bp->b_dev == device) bp->b_dev = NO_DEV;

  vm_forgetblocks();
}

/*===========================================================================*
 *				flushall				     *
 *===========================================================================*/
void flushall(
  dev_t dev 			/* device to flush */
)
{
/* Flush all dirty blocks for one device. */

  register struct buf *bp;
  static struct buf **dirty = NULL;	/* static so it isn't on stack */
  static int unsigned dirtylistsize = 0;
  int ndirty;

  if(dirtylistsize != nr_bufs) {
	if(dirtylistsize > 0) {
		assert(dirty != NULL);
		free(dirty);
	}
	if(!(dirty = malloc(sizeof(dirty[0])*nr_bufs)))
		panic("couldn't allocate dirty buf list");
	dirtylistsize = nr_bufs;
  }

  for (bp = &buf[0], ndirty = 0; bp < &buf[nr_bufs]; bp++)
	if (bp->b_dirt == DIRTY && bp->b_dev == dev) dirty[ndirty++] = bp;
  rw_scattered(dev, dirty, ndirty, WRITING);
}

/*===========================================================================*
 *				rw_scattered				     *
 *===========================================================================*/
void rw_scattered(
  dev_t dev,			/* major-minor device number */
  struct buf **bufq,		/* pointer to array of buffers */
  int bufqsize,		/* number of buffers */
  int rw_flag 			/* READING or WRITING */
)
{
/* Read or write scattered data from a device. */

  register struct buf *bp;
  int gap;
  register int i;
  register iovec_t *iop;
  static iovec_t *iovec = NULL;
  u64_t pos;
  int j, r;

  STATICINIT(iovec, NR_IOREQS);
  assert(bufq != NULL);

  /* (Shell) sort buffers on b_blocknr. */
  gap = 1;
  do
	gap = 3 * gap + 1;
  while (gap <= bufqsize);
  while (gap != 1) {
	gap /= 3;
	for (j = gap; j < bufqsize; j++) {
		for (i = j - gap;
		     i >= 0 && bufq[i]->b_blocknr > bufq[i + gap]->b_blocknr;
		     i -= gap) {
			bp = bufq[i];
			bufq[i] = bufq[i + gap];
			bufq[i + gap] = bp;
		}
	}
  }

  /* Set up I/O vector and do I/O.  The result of dev_io is OK if everything
   * went fine, otherwise the error code for the first failed transfer.
   */
  while (bufqsize > 0) {
	for (j = 0, iop = iovec; j < NR_IOREQS && j < bufqsize; j++, iop++) {
		bp = bufq[j];
		if (bp->b_blocknr != (block_t) bufq[0]->b_blocknr + j) break;
		iop->iov_addr = (vir_bytes) bp->b_data;
		iop->iov_size = (vir_bytes) fs_block_size;
	}
	pos = mul64u(bufq[0]->b_blocknr, fs_block_size);
	if (rw_flag == READING)
		r = bdev_gather(dev, pos, iovec, j, BDEV_NOFLAGS);
	else
		r = bdev_scatter(dev, pos, iovec, j, BDEV_NOFLAGS);

	/* Harvest the results.  The driver may have returned an error, or it
	 * may have done less than what we asked for.
	 */
	if (r < 0) {
		printf("ext2: I/O error %d on device %d/%d, block %u\n",
			r, major(dev), minor(dev), bufq[0]->b_blocknr);
	}
	for (i = 0; i < j; i++) {
		bp = bufq[i];
		if (r < (ssize_t) fs_block_size) {
			/* Transfer failed. */
			if (i == 0) {
				bp->b_dev = NO_DEV;	/* invalidate block */
				vm_forgetblocks();
			}
			break;
		}
		if (rw_flag == READING) {
			bp->b_dev = dev;	/* validate block */
			put_block(bp, PARTIAL_DATA_BLOCK);
		} else {
			bp->b_dirt = CLEAN;
		}
		r -= fs_block_size;
	}
	bufq += i;
	bufqsize -= i;
	if (rw_flag == READING) {
		/* Don't bother reading more than the device is willing to
		 * give at this time.  Don't forget to release those extras.
		 */
		while (bufqsize > 0) {
			put_block(*bufq++, PARTIAL_DATA_BLOCK);
			bufqsize--;
		}
	}
	if (rw_flag == WRITING && i == 0) {
		/* We're not making progress, this means we might keep
		 * looping. Buffers remain dirty if un-written. Buffers are
		 * lost if invalidate()d or LRU-removed while dirty. This
		 * is better than keeping unwritable blocks around forever..
		 */
		break;
	}
  }
}

/*===========================================================================*
 *				rm_lru					     *
 *===========================================================================*/
static void rm_lru(
  struct buf *bp
)
{
/* Remove a block from its LRU chain. */
  struct buf *next_ptr, *prev_ptr;

  bufs_in_use++;
  next_ptr = bp->b_next;	/* successor on LRU chain */
  prev_ptr = bp->b_prev;	/* predecessor on LRU chain */
  if (prev_ptr != NULL)
	prev_ptr->b_next = next_ptr;
  else
	front = next_ptr;	/* this block was at front of chain */

  if (next_ptr != NULL)
	next_ptr->b_prev = prev_ptr;
  else
	rear = prev_ptr;	/* this block was at rear of chain */
}

/*===========================================================================*
 *				cache_resize				     *
 *===========================================================================*/
static void cache_resize(unsigned int blocksize, unsigned int bufs)
{
  struct buf *bp;
  struct inode *rip;

#define MINBUFS 10
  assert(blocksize > 0);
  assert(bufs >= MINBUFS);

  for (bp = &buf[0]; bp < &buf[nr_bufs]; bp++)
	if(bp->b_count != 0) panic("change blocksize with buffer in use");

  for (rip = &inode[0]; rip < &inode[NR_INODES]; rip++)
	if (rip->i_count > 0) panic("change blocksize with inode in use");

  buf_pool(bufs);

  fs_block_size = blocksize;
  super_start = SUPER_BLOCK_BYTES / fs_block_size;
  super_end = (SUPER_BLOCK_BYTES + _MIN_BLOCK_SIZE - 1) / fs_block_size;
}

/*===========================================================================*
 *				bufs_heuristic				     *
 *===========================================================================*/
static int bufs_heuristic(struct super_block *sp)
{
  u32_t btotal, bfree;

  btotal = sp->s_blocks_count;
  bfree = sp->s_free_blocks_count;
  return fs_bufs_heuristic(MINBUFS, btotal, bfree,
  	sp->s_block_size, major(sp->s_dev));
}

/*===========================================================================*
 *				set_blocksize				     *
 *===========================================================================*/
void set_blocksize(struct super_block *sp)
{
  int bufs;

  cache_resize(sp->s_block_size, MINBUFS);
  bufs = bufs_heuristic(sp);
  cache_resize(sp->s_block_size, bufs);
  
  /* Decide whether to use seconday cache or not.
   * Only do this if
   *	- it's available, and
   *	- use of it hasn't been disabled for this fs, and
   *	- our main FS device isn't a memory device
   */

  vmcache = 0;
  if(vm_forgetblock(VM_BLOCKID_NONE) != ENOSYS &&
  	may_use_vmcache && major(sp->s_dev) != MEMORY_MAJOR) {
	vmcache = 1;
  }
}

/*===========================================================================*
 *                              buf_pool                                     *
 *===========================================================================*/
void buf_pool(int new_nr_bufs)
{
/* Initialize the buffer pool. */
  register struct buf *bp;

  assert(new_nr_bufs >= MINBUFS);

  if(nr_bufs > 0) {
	assert(buf);
	(void) fs_sync();
  	for (bp = &buf[0]; bp < &buf[nr_bufs]; bp++) {
		if(bp->bp) {
			assert(bp->b_bytes > 0);
			free_contig(bp->bp, bp->b_bytes);
		}
	}
  }

  if(buf)
	free(buf);

  if(!(buf = calloc(sizeof(buf[0]), new_nr_bufs)))
	panic("couldn't allocate buf list (%d)", new_nr_bufs);

  if(buf_hash)
	free(buf_hash);
  if(!(buf_hash = calloc(sizeof(buf_hash[0]), new_nr_bufs)))
	panic("couldn't allocate buf hash list (%d)", new_nr_bufs);

  nr_bufs = new_nr_bufs;

  bufs_in_use = 0;
  front = &buf[0];
  rear = &buf[nr_bufs - 1];

  for (bp = &buf[0]; bp < &buf[nr_bufs]; bp++) {
        bp->b_blocknr = NO_BLOCK;
        bp->b_dev = NO_DEV;
        bp->b_next = bp + 1;
        bp->b_prev = bp - 1;
        bp->bp = NULL;
        bp->b_bytes = 0;
  }
  front->b_prev = NULL;
  rear->b_next = NULL;

  for (bp = &buf[0]; bp < &buf[nr_bufs]; bp++) bp->b_hash = bp->b_next;
  buf_hash[0] = front;

  vm_forgetblocks();
}

