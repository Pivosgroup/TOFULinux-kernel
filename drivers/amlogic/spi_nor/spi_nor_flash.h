#ifndef __SPI_NOR_FLASH__H__
#define __SPI_NOR_FLASH__H__
#include <mach/spi_nor.h>

struct spi_nor {
	struct spi_device	*spi;
	struct mutex		lock;
	struct mtd_info		mtd;
	unsigned		partitioned:1;
	u32				flash_pp_size;
	u8			erase_opcode;
	u8			command[CMD_SIZE + FAST_READ_DUMMY_BYTE];
#ifdef CONFIG_SPI_NOR_SECURE_STORAGE
	u8 		secure_protect;
	struct aml_spisecurestorage_info_t  *securestorage_info;
#endif
};


#endif
