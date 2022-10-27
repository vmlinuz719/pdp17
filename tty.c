#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>

#include "bus.h"
#include "cpu.h"
#include "tty.h"

pthread_mutex_t reg_mutex;
size_t unit_reg = 0;
data_width_t cmd_reg = 0xFFFF;

int tty_attn(size_t unit, data_width_t cmd) {
	pthread_mutex_lock(&reg_mutex);
	
	// printf("Sent command\n");
	
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
			// printf("Got command #%d\n", cycles++);
			my_cmd = cmd_reg;
			unit_reg = 0;
			cmd_reg = 0xFFFF;
			pthread_mutex_unlock(&reg_mutex);
		}
		
		int did_something = my_cmd != 0xFFFF;
		
		if (did_something) {
			zpage[FLAG] &= ~(1 << IO);
			switch (my_cmd) {
				case 0x4:
					printf("%c", (char) (zpage[get_flag_acc()] & 0xFF));
					fflush(stdout);
					break;
				case 0x1:
					zpage[PC]++;
					break;
			}
			
		}

	}
	
	return NULL;
}
