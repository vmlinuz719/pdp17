#include <stdio.h>
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

int main(int argc, char *argv) {
	init_bus();
	
	install_unit(0, cpu_read, cpu_write);
	install_unit(1, mem_read, mem_write);
	
	bus_write(0, 0b1111111111111111); // initialize accumulators
	bus_write(1, 0b1100110011001100); // test pattern
	bus_write(2, 0b1111111111111111);
	bus_write(3, 0b1111111111111111);
	bus_write(4, 0b1111111111111111);
	bus_write(5, 0b1100110011001100);
	bus_write(6, 0b0011001100110011);
	bus_write(7, 0b0000000000000000);
	
	bus_write(017, 0x0100); // initialize PC
	
	bus_write(0x0100, 0b0000000100000001); // AND A0, A1
	bus_write(0x0101, 0b0000100011111111); // AND A2, *FF
	bus_write(0x0102, 0b0000111100001111); // AND A3, #CCCC
	bus_write(0x0103, 0b1100110011001100);
	bus_write(0x0104, 0b0001001011111110); // AND A4, **FE
	bus_write(0x0105, 0b0011010100000110); // TAD A5, A6
	bus_write(0x0106, 0b0011000100000100); // TAD A4, A4
	bus_write(0x0107, 0b0100000100000100); // ISZ A4
	bus_write(0x0108, 0b0100000100000101); // ISZ A5
	bus_write(0x0109, 0b0110000100001000); // DCA A0, I0
	bus_write(0x010A, 0b0111100100000100); // DCA A6, A4
	bus_write(0x010B, 0b0111110100000101); // DCA PS, A5
	bus_write(0x010C, 0b1001100010000001); // JMS A6, *81
	bus_write(0x010D, 0b0011101100001111); // TAD A6, #CCCC
	bus_write(0x010E, 0b1100110011001100);
	
	bus_write(0x0180, 0b0000000000000000);
	bus_write(0x0181, 0b0111100010000000); // DCA A6, *80
	bus_write(0x0182, 0b0011001100001111); // TAD A4, #9999
	bus_write(0x0183, 0b1001100110011001);
	bus_write(0x0184, 0b1010001010000000); // JMP **80
	
	bus_write(0x0185, 0b0001001100001111); // AND A4, #0000
	bus_write(0x0186, 0b0000000000000000);
	
	bus_write(0x01FE, 0b0000000100000011);
	bus_write(0x01FF, 0b1100110011001100);
	
	/*
	 * Expected output:
	 * CCCC CCCC CCCC CCCC CCCC E669 CCCC 0020
	 * 0000 0000 0000 0000 0000 0000 0000 0110
	 */
	
	while (zpage[15] != 0x110) step();
	
	printf("%04hX %04hX %04hX %04hX %04hX %04hX %04hX %04hX\n",
		zpage[0], zpage[1], zpage[2], zpage[3],
		zpage[4], zpage[5], zpage[6], zpage[7]);
	printf("%04hX %04hX %04hX %04hX %04hX %04hX %04hX %04hX\n",
		zpage[8], zpage[9], zpage[10], zpage[11],
		zpage[12], zpage[13], zpage[14], zpage[15]);
	
	return 0;
}
