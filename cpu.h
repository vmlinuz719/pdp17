#ifndef __CPU_H__
#define __CPU_H__

#include "bus.h"

extern int cpu_read(addr_width_t src, data_width_t *dst);
extern int cpu_write(addr_width_t dst, data_width_t src);
extern int cpu_attn(size_t unit, data_width_t cmd);

extern data_width_t zpage[PAGE_SIZE];
extern data_width_t switches;

extern void step(void);

#endif
