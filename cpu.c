#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>

#include "bus.h"

data_width_t zpage[PAGE_SIZE];

/*
 * Bus interface functions
 */

int cpu_read(addr_width_t src, data_width_t *dst) {
	addr_width_t offset = src & offset_mask;
	*dst = zpage[offset];
	return 0;
}

int cpu_write(addr_width_t dst, data_width_t src) {
	addr_width_t offset = dst & offset_mask;
	zpage[offset] = src;
	return 0;
}

int cpu_attn(data_width_t cmd) {
	return ENOSYS;
}

#define FLAG 07
#define PC 017

addr_width_t mar = 0;
data_width_t mbr = 0;

/*
 * Basic instruction format
 * 0 1 2 3 4 5 6 7 8 9 A B C D E F
 * Opcode      I Z Offset
 *       AccSel
 */

int get_mbr_opcode() {
	return (mbr & 0xE000) >> 13;
}

int get_mbr_acc() {
	return (mbr & 0x1C00) >> 10;
}

int get_mbr_i() {
	return (mbr & 0x0200) >> 9;
}

int get_mbr_z() {
	return (mbr & 0x0100) >> 8;
}

/*
 * Offset: trivial
 */

/*
 * TODO: add IOT, OPR
 */

/*
 * Flag register layout
 * 0 1 2 3 4 5 6 7 8 9 A B C D E F
 * AccSel        Cycle   Id  Io  Lk
 *       Tmp               Ex  Op
 */

#define ID 4
#define EX 3
#define IO 2
#define OP 1
#define LK 0

/*
 * Getters and setters for the accumulator select flags
 */

void set_flag_acc(int value) {
	zpage[FLAG] = (zpage[FLAG] & 0x1FFF) | ((value & 0x7) << 13);
	return;
}

int get_flag_acc() {
	return (zpage[FLAG] & 0xE000) >> 13;
}

/*
 * Getters and setters for the temporary flags
 * Stores opcode in EXEC cycle, function in IOTXRX, OPR group in OPERTx
 */

void set_flag_tmp(int value) {
	zpage[FLAG] = (zpage[FLAG] & 0xE1FF) | ((value & 0xF) << 9);
	return;
}

int get_flag_tmp() {
	return (zpage[FLAG] & 0x1E00) >> 9;
}

/*
 * Getters and setters for the cycle counter flags
 */

void set_flag_cycle(int value) {
	zpage[FLAG] = (zpage[FLAG] & 0xFE1F) | ((value & 0xF) << 5);
	return;
}

int get_flag_cycle() {
	return (zpage[FLAG] & 0x1E0) >> 5;
}

/*
 * The rest are trivial;
 * no getters or setters are provided nor should be needed.
 */

/*
 * Calculate an address using an offset and zero-page bit.
 */

addr_width_t address(int z, addr_width_t offset) {
	offset &= offset_mask;
	
	if (!z) offset |= zpage[PC] & ~offset_mask;
	
	return offset;
}

/*
 * Cycle 0: IFETCH
 */

void cycle_IFETCH(void) {
	zpage[FLAG] &= 1;

	mar = zpage[PC]++;
	bus_read(mar, &mbr);
	
	set_flag_cycle(1);
	
	return;
}

/*
 * Cycle 1: DECODE
 */

void cycle_DECODE(void) {
	set_flag_acc(get_mbr_acc());
	
	int opcode = get_mbr_opcode();
	switch (opcode) {
		case 6:
			// IOT
			break;
		
		case 7:
			// OPR
			break;
		
		default:
			// Basic instruction
			set_flag_tmp(opcode);
			zpage[FLAG] |= get_mbr_i() << ID;
			zpage[FLAG] |= 1 << EX;
			mar = address(get_mbr_z(), mbr & offset_mask);
	}
	
	if ((zpage[FLAG] >> ID) & 1) set_flag_cycle(2);
	else if ((zpage[FLAG] >> EX) & 1) set_flag_cycle(3);
	else if ((zpage[FLAG] >> IO) & 1) set_flag_cycle(4);
	else if ((zpage[FLAG] >> OP) & 1) set_flag_cycle(5);
	else set_flag_cycle(10);
	
	return;
}

/*
 * Cycle 2: INADDR
 */

void cycle_INADDR(void) {
	if (mar < 020) {
		mar = (010 <= mar && 017 >= mar)
			? zpage[mar]++ 
			: zpage[mar];
	} else {
		bus_read(mar, &mbr);
		mar = mbr;
	}
	
	set_flag_cycle(3);
	
	return;
}

/*
 * Cycle 3: EXEC
 */

void cycle_EXEC(void) {
	int acc = get_flag_acc();
	
	switch (get_flag_tmp()) {
		case 0:
			// AND
			bus_read(mar, &mbr);
			
			mbr = zpage[acc] & mbr;
			zpage[FLAG] &= ~(1 << ID); // writeback to accumulator
			break;
		
		case 1:
			// TAD
			bus_read(mar, &mbr);
			
			int result = (int) zpage[acc] + (int) mbr;
			mbr = (data_width_t) (result & 0xFFFF);
			
			if (result & ~(0xFFFF)) zpage[FLAG] |= 1 << LK; // carry set
			else zpage[FLAG] &= ~(1 << LK); // carry clear
			
			zpage[FLAG] &= ~(1 << ID); // writeback to accumulator
			break;

		case 2:
			// ISZ
			bus_read(mar, &mbr);
			mbr++;
			if (mbr == 0) zpage[PC]++;
			
			zpage[FLAG] |= 1 << ID; // deferred writeback to memory
			break;
		
		case 3:
			// DCA
			mbr = zpage[acc];
			bus_write(mar, mbr);
			mbr = 0;
			
			zpage[FLAG] &= ~(1 << ID); // writeback to accumulator
			break;
		
		case 4:
			// JMS
			mbr = zpage[PC];
			zpage[PC] = (data_width_t) mar;

			zpage[FLAG] &= ~(1 << ID); // writeback to accumulator
			break;
		
		case 5:
			// JMP
			mbr = zpage[acc];
			zpage[PC] = (data_width_t) mar;
			
			zpage[FLAG] &= ~(1 << ID); // writeback to accumulator
			break;
		
		default:
			// TBD
			printf("Illegal opcode - how?!?\n");
	}
	
	set_flag_cycle(9);
	
	return;
}

/*
 * Cycle 9: WTBACK
 */

void cycle_WTBACK(void) {
	set_flag_cycle(0);

	if (zpage[FLAG] & 1 << ID) bus_write(mar, mbr);
	else zpage[get_flag_acc()] = mbr;
	
	return;
}

void step(void) {
	switch (get_flag_cycle()) {
		case 0:
			cycle_IFETCH();
			break;
		case 1:
			cycle_DECODE();
			break;
		case 2:
			cycle_INADDR();
			break;
		case 3:
			cycle_EXEC();
			break;
		case 9:
			cycle_WTBACK();
			break;
		default:
			printf("Invalid cycle - how?!?\n");
	}
	
	printf("%04hX %04hX\n", zpage[7], zpage[15]);
}


