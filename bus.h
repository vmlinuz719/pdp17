#ifndef __BUS_H__
#define __BUS_H__

#include <stdint.h>

/*
 * Definitions of data width, address width and the number of units to attach to
 * the bus. We use word addressing at the lowest level because modern buses
 * don't support byte addressing anyway, in fact it's all cache lines down there
 * If that's a problem, whatever calls read and write should handle byte
 * selection itself
 */

typedef uint16_t data_width_t;
typedef uint32_t addr_width_t;

#define PAGE_SIZE 256 // THIS MUST BE A POWER OF TWO
#define MAX_PAGES 256 // THIS DOESN'T MATTER

extern int init_bus();
extern data_width_t offset_width;
extern addr_width_t offset_mask;

extern int install_unit(
	size_t pgn,
	int (*unit_read) (addr_width_t, data_width_t *),
	int (*unit_write) (addr_width_t, data_width_t)
);

extern int install_attn(
	size_t pgn,
	int (*unit_attn) (size_t, data_width_t)
);

extern int addr_split(addr_width_t addr, size_t *pgn, size_t *offset);

extern int bus_read(addr_width_t src, data_width_t *dst);
extern int bus_write(addr_width_t dst, data_width_t src);
extern int bus_attn(size_t unit, data_width_t cmd);

#endif
