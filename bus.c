#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>

#include "bus.h"

/*
 * Width of page offset, to be calculated at init time
 */

data_width_t offset_width = 0;
addr_width_t offset_mask = 0;

/*
 * Per-unit read functions. Populate at startup
 *
 * addr_width_t src: address to read
 * data_width_t *dst: where to store fetched memory line
 * return int: 0 on success, nonzero on error (see errno.h for possible values)
 */

int (*read[MAX_PAGES])(addr_width_t src, data_width_t *dst);

/*
 * Per-unit write functions. Populate at startup
 *
 * addr_width_t dst: address to write
 * data_width_t src: contents to write to memory line
 * return int: 0 on success, nonzero on error (see errno.h for possible values)
 */

int (*write[MAX_PAGES])(addr_width_t dst, data_width_t src);

/*
 * I/O control functions, commands defined per device. Calls may block.
 *
 * *data_width_t cmd: command to be parsed and handled by device
 * return int: 0 on success, nonzero on error (see errno.h for possible values)
 */

int (*attn[MAX_PAGES])(data_width_t cmd);

/*
 * Initialize bus arrays, very important so we can reliably say what addresses
 * are valid; also determine page size for selection.
 */

int init_bus() {
	for (size_t x = 0; x < MAX_PAGES; x++) {
		read[x] = NULL;
		write[x] = NULL;
		attn[x] = NULL;
	}
	
	int sz = PAGE_SIZE;
	while (sz > 1) {
		sz >>= 1;
		offset_width++;
	}
	
	offset_mask = ((1 << offset_width) - 1);
	
	if (sz == 1) return 0;
	else return ENOTRECOVERABLE;
}

/*
 * Install unit handlers for a page. Returns EINVAL if page number is too high,
 * 0 otherwise.
 */

int install_unit(
	size_t pgn,
	int (*unit_read)(addr_width_t, data_width_t *),
	int (*unit_write)(addr_width_t, data_width_t)
) {
	if (pgn >= MAX_PAGES) return EINVAL;
	
	read[pgn] = unit_read;
	write[pgn] = unit_write;
	
	return 0;
}

extern int install_attn(
	size_t pgn,
	int (*unit_attn) (data_width_t)
) {
	if (pgn >= MAX_PAGES) return EINVAL;
	
	attn[pgn] = unit_attn;
	
	return 0;
}

/*
 * Split address into page:offset. Returned page number may not be valid if it
 * is too high; returns EINVAL in that case but still sets output.
 */

int addr_split(addr_width_t addr, size_t *pgn, size_t *offset) {
	*pgn = addr >> offset_width;
	*offset = addr & offset_mask;
	
	if (*pgn >= MAX_PAGES) return EINVAL;
	else return 0;
}

/*
 * Dispatch read, write and attn calls to appropriate bus units.
 */

int bus_read(addr_width_t src, data_width_t *dst) {
	size_t pgn = 0;
	size_t offset = 0;
	
	int invalid_addr = addr_split(src, &pgn, &offset);
	
	if (invalid_addr || read[pgn] == NULL) return EINVAL;
	else return (*read[pgn])(src, dst);
}

int bus_write(addr_width_t dst, data_width_t src) {
	size_t pgn = 0;
	size_t offset = 0;
	
	int invalid_addr = addr_split(dst, &pgn, &offset);
	
	if (invalid_addr || write[pgn] == NULL) return EINVAL;
	else return (*write[pgn])(dst, src);
}

int bus_attn(size_t unit, data_width_t cmd) {
	if (unit >= MAX_PAGES || attn[unit] == NULL) return EINVAL;
	else return (*attn[unit])(cmd);
}
