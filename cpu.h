#ifndef __CPU_H__
#define __CPU_H__

#include "bus.h"
#include <pthread.h>

#define ID 4
#define EX 3
#define IO 2
#define OP 1
#define LK 0

#define FLAG 07
#define PC 017

extern int cpu_read(addr_width_t src, data_width_t *dst);
extern int cpu_write(addr_width_t dst, data_width_t src);
extern int cpu_attn(size_t unit, data_width_t cmd);

extern data_width_t zpage[PAGE_SIZE];
extern data_width_t switches;

extern pthread_mutex_t io_lock;

extern void step(void);

#endif
