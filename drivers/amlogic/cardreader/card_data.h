#ifndef __CARD__DATA__H_
#define __CARD__DATA__H_

#include <linux/cardreader/card_block.h>

struct card_blk_data {
	spinlock_t lock;
	struct gendisk *disk;
	struct card_queue queue;

	unsigned int usage;
	unsigned int block_bits;
	unsigned int read_only;
};


#endif

