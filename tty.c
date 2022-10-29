#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <poll.h>

#include "bus.h"
#include "cpu.h"
#include "tty.h"

pthread_mutex_t reg_mutex;
size_t unit_reg = 0;
data_width_t cmd_reg = 0xFFFF;

int tty_attn(size_t unit, data_width_t cmd) {
	pthread_mutex_lock(&reg_mutex);
	
	unit_reg = unit;
	cmd_reg = cmd;
	
	pthread_mutex_unlock(&reg_mutex);

	return 0;
}

int run_tty = 0;

extern int get_flag_acc();

void *tty(void *vargp) {
	size_t *unit_no_ptr = (size_t *) vargp;
	size_t unit_no = *unit_no_ptr;

	int cycles = 0;

	while (run_tty) {
		data_width_t my_cmd = 0xFFFF;
		
		if (unit_reg == unit_no && cmd_reg != 0xFFFF) {
			pthread_mutex_lock(&reg_mutex);
			my_cmd = cmd_reg;
			unit_reg = 0;
			cmd_reg = 0xFFFF;
			pthread_mutex_unlock(&reg_mutex);
		}
		
		int did_something = my_cmd != 0xFFFF;
		
		if (did_something) {
			data_width_t acc_val = zpage[get_flag_acc()];
			
			switch (my_cmd) {
				case 0x4:
					zpage[FLAG] &= ~(1 << IO);
					printf("%c", (char) (acc_val & 0xFF));
					fflush(stdout);
					break;
				case 0x1:
					zpage[PC]++;
					zpage[FLAG] &= ~(1 << IO);
					break;
				default:
					zpage[FLAG] &= ~(1 << IO);
			}
			
		}

	}
	
	return NULL;
}

void *ttyin(void *vargp) {
	size_t *unit_no_ptr = (size_t *) vargp;
	size_t unit_no = *unit_no_ptr;

	while (run_tty) {
		data_width_t my_cmd = 0xFFFF;
		
		if (unit_reg == unit_no && cmd_reg != 0xFFFF) {
			pthread_mutex_lock(&reg_mutex);
			my_cmd = cmd_reg;
			unit_reg = 0;
			cmd_reg = 0xFFFF;
			pthread_mutex_unlock(&reg_mutex);
		}
		
		int did_something = my_cmd != 0xFFFF;
		
		if (did_something) {
			data_width_t acc = get_flag_acc();
			
			struct pollfd pfd;
			pfd.fd = 0;
			pfd.events = POLLIN;
			
			switch (my_cmd) {
				case 0x1:
					poll(&pfd, 1, 0);
					if (pfd.revents & POLLIN)
						zpage[PC]++;
					
					zpage[FLAG] &= ~(1 << IO);
					break;
				case 0x6:
					poll(&pfd, 1, 0);
					if (pfd.revents & POLLIN)
						zpage[acc] = getchar();
					else
						zpage[acc] = 0;
					
					zpage[FLAG] &= ~(1 << IO);
					break;
				default:
					zpage[FLAG] &= ~(1 << IO);
			}
			
		}

	}
	
	return NULL;
}
