#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>

#include "bus.h"
#include "cpu.h"

#define MEM_SIZE 65536

data_width_t mem[MEM_SIZE];

int mem_read(addr_width_t src, data_width_t *dst) {
	*dst = mem[src];
	return 0;
}

int mem_write(addr_width_t dst, data_width_t src) {
	mem[dst] = src;
	return 0;
}

void run_cpu(void) {
	while ((zpage[7] & 0x1E0) >> 5 != 0xF) {
		step();
	}
	
	zpage[7] ^= 0x1E0;
	
	return;
}

void regs(void) {
	printf("A0   A1   A2   A3   A4   A5   A6   PSW\n");   
	printf("%04hX %04hX %04hX %04hX %04hX %04hX %04hX %04hX\n",
		zpage[0], zpage[1], zpage[2], zpage[3],
		zpage[4], zpage[5], zpage[6], zpage[7]);
	
	printf("I0   I1   I2   I3   I4   I5   I6   PC\n");
	printf("%04hX %04hX %04hX %04hX %04hX %04hX %04hX %04hX\n",
		zpage[8], zpage[9], zpage[10], zpage[11],
		zpage[12], zpage[13], zpage[14], zpage[15]);
	
	return;
}

int main(int argc, char *argv) {
	init_bus();
	switches = 0;
	
	install_unit(0, cpu_read, cpu_write);
	install_unit(1, mem_read, mem_write);
	
	int run = 1;
	
	printf("\"PDP-17\" - for evaluation use only\n");
	
	char command = '\0';
	data_width_t addr = 0;
	data_width_t data = 0;
	
	while (run) {
		data_width_t value = 0;
		char garbage = '\0';
		
		printf("%04hX %04hX> ", addr, switches);
		
		char *line = NULL;
		size_t len = 0;
		getline(&line, &len, stdin);
		int valid = sscanf(line, " %c%4hX %c", &command, &value, &garbage);
		free(line);
		
		command = tolower(command);
		
		switch (command) {
			case 'a': // set address
				if (valid == 2) addr = value;
				else printf("?\n");
				break;
			case 'd': // deposit
				if (valid > 2) printf("?\n");
				else {
					if (valid == 2) data = value;
					bus_write(addr++, data);
				}
				break;
			case 'i': // inspect
				if (valid > 2) printf("?\n");
				else {
					if (valid == 2) addr = value;
					bus_read(addr, &data);
					printf("%04hX\n", data);
					addr++;
				}
				break;
			case 'g': // go
				if (valid > 1) printf("?\n");
				else {
					zpage[15] = addr;
					run_cpu();
					addr = zpage[15];
				}
				break;
			case 'c': // continue
				if (valid > 1) printf("?\n");
				else {
					run_cpu();
					addr = zpage[15];
				}
				break;
			case 's': // single step
				if (valid > 1) printf("?\n");
				else { step(); regs(); }
				break;
			case 'r': // view regs
				if (valid > 1) printf("?\n");
				else regs();
				break;
			case 'w': // set switches
				if (valid == 2) switches = value;
				else printf("?\n");
				break;
			case 'q': // quit
				if (valid > 1) printf("?\n");
				else run = 0;
				break;
			default:
				printf("?\n");
		}
	}
	
	return 0;
}
