#ifndef __TTY_H__
#define __TTY_H__

extern int tty_attn(size_t unit, data_width_t cmd);
extern int run_tty;
extern void *tty(void *vargp);

#endif
