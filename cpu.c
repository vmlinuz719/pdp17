#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <pthread.h>

#include "bus.h"
#include "cpu.h"

data_width_t zpage[PAGE_SIZE];
data_width_t switches;

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

int cpu_attn(size_t unit, data_width_t cmd) {
    return ENOSYS;
}

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
 * IOT instruction format
 * 0 1 2 3 4 5 6 7 8 9 A B C D E F
 * 1 1 0       Device      0
 *       AccSel              Func
 */

/*
 * OPR instruction format
 * 0 1 2 3 4 5 6 7 8 9 A B C D E F
 * 1 1 1       0 Gr
 *       AccSel    Function
 */

/*
 * AOP instruction format
 * 0 1 2 3 4 5 6 7 8 9 A B C D E F
 * 1 1 1       1 1 Function
 *       DstSel            Src/Imm4
 */

int get_mbr_opr_gr() {
    return (mbr & 0x0100) >> 8;
}

int get_mbr_opr_regop() {
    return (mbr & 0x0200) >> 9;
}

int get_mbr_opr_gr2_and() {
    return (mbr & 0x0008) >> 3;
}

/*
 * Flag register layout
 * 0 1 2 3 4 5 6 7 8 9 A B C D E F
 * AccSel        Cycle   Id  Io  Lk
 *       Tmp               Ex  Op
 */

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
 * Rotating through a carry bit is slightly complicated in C -
 * here are some helper functions to do it through the "link"
 * bit in the PSW
 */

data_width_t ral(data_width_t value) {
    data_width_t old_link = zpage[FLAG] & 1;
    data_width_t new_link = (value & 0x8000) >> 15;
    zpage[FLAG] = (zpage[FLAG] & 0xFFFE) | new_link;
    return (value << 1) | old_link;
}
    
data_width_t rar(data_width_t value) {
    data_width_t old_link = (zpage[FLAG] & 1) << 15;
    data_width_t new_link = value & 1;
    zpage[FLAG] = (zpage[FLAG] & 0xFFFE) | new_link;
    return (value >> 1) | old_link;
}

/*
 * OPR helpers
 */

void opr1(int ucode) {
    int acc = get_flag_acc();
    
    if (!ucode) {}

    if (ucode & 0x80) // CLA
        zpage[acc] = 0;
    if (ucode & 0x40) // CLL
        zpage[FLAG] &= ~(1 << LK);
    
    if (ucode & 0x20) // CMA
        zpage[acc] ^= 0xFFFF;
    if (ucode & 0x10) // CML
        zpage[FLAG] ^= 1 << LK;
    
    if (ucode & 0x1) { // IAC
        int result = (int) (zpage[acc] + 1);
        zpage[acc] = result & 0xFFFF;
        if (result & ~(0xFFFF)) zpage[FLAG] ^= 1 << LK;
    }
    
    switch ((ucode & 0xE) >> 1) {
        case 1: // 6-bit BSW
            zpage[acc] = (zpage[acc] & 0xF000)
                       | ((zpage[acc] & 07700) >> 6)
                       | ((zpage[acc] & 077) << 6);
            break;
        
        case 2: // RAL once
            zpage[acc] = ral(zpage[acc]);
            break;
        
        case 3: // RAL twice
            zpage[acc] = ral(ral(zpage[acc]));
            break;
        
        case 4: // RAR once
            zpage[acc] = rar(zpage[acc]);
            break;
        
        case 5: // RAR twice
            zpage[acc] = rar(rar(zpage[acc]));
            break;
        
        case 7: // 8-bit BSW
            zpage[acc] = (zpage[acc] & 0xFF00) >> 8
                       | (zpage[acc] & 0xFF) << 8;
            break;
    }

    return;
}

void opr2(int ucode) {
    int acc = get_flag_acc();
    
    if (!ucode) {}

    else if (ucode & 1); // opr3(ucode);
    
    else if (!((ucode & 0x0008) >> 3)) { // OR group
        int skip = 0;
        
        if ((ucode & 0x40) && zpage[acc] & 0x8000) skip = 1; // SMA
        if ((ucode & 0x20) && zpage[acc] == 0) skip = 1; // SZA
        if ((ucode & 0x10) && zpage[FLAG] & 1) skip = 1; // SNL
        
        if (skip) zpage[PC]++;
    }
    
    else { // AND group
        int skip = 1;
        
        if (ucode & 0x40) skip &= (zpage[acc] & 0x8000 == 0); // SPA
        if (ucode & 0x20) skip &= (zpage[acc] != 0); // SNA
        if (ucode & 0x10) skip &= (zpage[FLAG] & 1 == 0); // SZL
        
        if (skip) zpage[PC]++;
    }
    
    if (ucode & 0x80) zpage[acc] = 0; // CLA
    
    if (ucode & 0x04) zpage[acc] |= switches; // OSR
    if (ucode & 0x02) set_flag_cycle(0xF); // HLT
    
    return;
}

/*
 * New OPR group with register-register operations
 */

void reg_op(void) {
    uint32_t dst = get_flag_acc();
    uint32_t src = mbr & 07;
    uint32_t imm4 = mbr & 017;
    uint32_t result = 0;
    int s_result = 0;
    
    switch ((mbr & 0x00F0) >> 4) {
        case 0x00: // MOV
            zpage[dst] = zpage[src];
            break;
            
        case 0x01: // SWP
            result = zpage[src];
            zpage[src] = zpage[dst];
            zpage[dst] = result;
            break;
        
        case 0x02: // OR
            zpage[dst] |= zpage[src];
            break;
        
        case 0x03: // XOR
            zpage[dst] ^= zpage[src];
            break;
        
        case 0x04: // SHL
            result = zpage[dst];
            result <<= (zpage[src] & 0xF);
            zpage[dst] = result;
            if (result & 0x10000) zpage[FLAG] |= 1 << LK;
            else zpage[FLAG] &= ~(1 << LK);
            break;
            
        case 0x05: // SLI
            result = zpage[dst];
            result <<= imm4 + 1;
            zpage[dst] = result;
            if (result & 0x10000) zpage[FLAG] |= 1 << LK;
            else zpage[FLAG] &= ~(1 << LK);
            break;
            
        case 0x06: // SHR
            result = zpage[dst];
            
            if ((zpage[src] & 0xF) && result & (1 << ((zpage[src] & 0xF) - 1)))
                zpage[FLAG] |= 1 << LK;
            else if ((zpage[src] & 0xF)) zpage[FLAG] &= ~(1 << LK);
            
            result >>= (zpage[src] & 0xF);
            zpage[dst] = result;
            break;
            
        case 0x07: // SRI
            result = zpage[dst];
            
            if (result & (1 << imm4))
                zpage[FLAG] |= 1 << LK;
            else zpage[FLAG] &= ~(1 << LK);
            
            result >>= imm4 + 1;
            zpage[dst] = result;
            break;
            
        case 0x08: // ASR
            s_result = (int16_t) zpage[dst];
            
            if ((zpage[src] & 0xF) && s_result & (1 << ((zpage[src] & 0xF) - 1)))
                zpage[FLAG] |= 1 << LK;
            else if ((zpage[src] & 0xF)) zpage[FLAG] &= ~(1 << LK);
            
            s_result >>= (zpage[src] & 0xF);
            zpage[dst] = s_result;
            break;
            
        case 0x09: // ASI
            s_result = (int16_t) zpage[dst];
            
            if (s_result & (1 << imm4))
                zpage[FLAG] |= 1 << LK;
            else zpage[FLAG] &= ~(1 << LK);
            
            s_result >>= imm4 + 1;
            zpage[dst] = s_result;
            break;
    }
    return;
}

/*
 * Cycle 0: IFETCH
 */

void cycle_IFETCH(void) {
    zpage[FLAG] &= 1;

    mar = zpage[PC]++;
    bus_read(mar, &mbr);
    // printf("%04X %04hX\n", mar, mbr);
    
    int opcode = get_mbr_opcode();
    switch (opcode) {
        case 6:
            // IOT
            mar = (mbr & 0x3F0) >> 4;
            set_flag_tmp(mbr & 0x7);
            set_flag_acc(get_mbr_acc());
            
            zpage[FLAG] |= 1 << IO;
            set_flag_cycle(4);
            
            if (bus_attn(mar, get_flag_tmp()))
                zpage[FLAG] &= ~(1 << IO);
            break;
        
        case 7:
            // OPR
            if (get_mbr_opr_regop() && get_mbr_opr_gr()) { // Reg-reg operation
                set_flag_acc(get_mbr_acc());
                reg_op();
            }
            else if (!get_mbr_opr_gr()) { // OPR1
                set_flag_acc(get_mbr_acc());
                opr1(mbr & offset_mask);
            }
            else if (get_mbr_opr_gr()) { // OPR2
                set_flag_acc(get_mbr_acc());
                opr2(mbr & offset_mask);
            }
            
            break;
        
        default:
            // Basic instruction
            set_flag_tmp(opcode);
            set_flag_acc(get_mbr_acc());
            
            int zero = get_mbr_z();
            int indirect = get_mbr_i() << ID;
            
            data_width_t cmp_val = 0;
            
            mar = address(zero, mbr & offset_mask);
            if (opcode <= 3 && zero && !indirect && mar <= PC) {
                int result;
                switch (opcode) {
                    case 0: // ANDR
                        zpage[get_flag_acc()] &= zpage[mar];
                        break;
                    case 1: // TADR
                        result = (int) zpage[get_flag_acc()] + (int) zpage[mar];                        
                        if (result & ~(0xFFFF)) zpage[FLAG] ^= 1 << LK; // carry complement
                        zpage[get_flag_acc()] = (data_width_t) (result & 0xFFFF);
                        break;
                    case 2: // ISZR
                        result = ++zpage[mar];
                        
                        if (get_flag_acc()) cmp_val = zpage[get_flag_acc() - 1]; // ISE
                        
                        if (result == cmp_val) zpage[PC]++;
                        break;
                    case 3: // DCAR
                        zpage[mar] = zpage[get_flag_acc()];
                        zpage[get_flag_acc()] = 0;
                        break;
                }
            }
            
            else if (opcode >= 4 && zero && indirect
                && mar <= PC && (opcode == 4 || !get_flag_acc())) { // JMSR, JMPR
                
                addr_width_t jmp_addr = (010 <= mar && PC >= mar)
                    ? zpage[mar]++ 
                    : zpage[mar];
                if (opcode == 4) zpage[get_flag_acc()] = zpage[PC];
                zpage[PC] = jmp_addr;
            }
            
            else if ((opcode == 4 && !indirect)
                || (opcode == 5 && !indirect && !get_flag_acc())) { // JMS, JMP
                
                if (opcode == 4) zpage[get_flag_acc()] = zpage[PC];
                zpage[PC] = (data_width_t) mar;
            }
            
            else {
                zpage[FLAG] |= 1 << EX;
                
                if (indirect) {
                    if (mar <= PC)
                        mar = (010 <= mar && PC >= mar)
                            ? zpage[mar]++ 
                            : zpage[mar];
                    else zpage[FLAG] |= 1 << ID;
                }
            }
    }
    
    if ((zpage[FLAG] >> ID) & 1) set_flag_cycle(2);
    else if ((zpage[FLAG] >> EX) & 1) set_flag_cycle(3);
    else if ((zpage[FLAG] >> OP) & 1) set_flag_cycle(5);
    
    return;
}

/*
 * Cycle 2: INADDR
 */

void cycle_INADDR(void) {
    if (mar <= PC) {
        mar = (010 <= mar && PC >= mar)
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

int local_read(addr_width_t src, data_width_t *dst) {
    if (src <= PC) {
        *dst = zpage[src];
        return 0;
    } else {
        return bus_read(src, dst);
    }
}

int local_write(addr_width_t dst, data_width_t src) {
    if (dst <= PC) {
        zpage[dst] = src;
        return 0;
    } else {
        return bus_write(dst, src);
    }
}

void cycle_EXEC(void) {
    int acc = get_flag_acc();
    
    switch (get_flag_tmp()) {
        case 0:
            // AND
            local_read(mar, &mbr);
            
            mbr = zpage[acc] & mbr;
            zpage[FLAG] &= ~(1 << ID); // writeback to accumulator
            zpage[get_flag_acc()] = mbr;
            break;
        
        case 1:
            // TAD
            local_read(mar, &mbr);
            
            int result = (int) zpage[acc] + (int) mbr;
            mbr = (data_width_t) (result & 0xFFFF);
            
            if (result & ~(0xFFFF)) zpage[FLAG] ^= 1 << LK; // carry complement
            
            zpage[FLAG] &= ~(1 << ID); // writeback to accumulator
            zpage[get_flag_acc()] = mbr;
            break;

        case 2:
            // ISZ/STA
            
            if (acc) { // STA
                mbr = zpage[acc - 1];
                local_write(mar, mbr);
                zpage[FLAG] &= ~(1 << ID);
            }
            
            else { // ISZ
                local_read(mar, &mbr);
                mbr++;
                
                if (mbr == 0) zpage[PC]++;
                
                if (mar <= PC) { // contents in register, no deferral needed
                    zpage[mar]++;
                    zpage[FLAG] &= ~(1 << ID);
                }
                else zpage[FLAG] |= 1 << ID; // deferred writeback to memory
            }
            
            break;
        
        case 3:
            // DCA
            mbr = zpage[acc];
            local_write(mar, mbr);
            
            zpage[FLAG] &= ~(1 << ID); // writeback to accumulator
            zpage[get_flag_acc()] = 0;
            break;
        
        case 4:
            // JMS
            mbr = zpage[PC];
            zpage[PC] = (data_width_t) mar;

            zpage[FLAG] &= ~(1 << ID); // writeback to accumulator
            zpage[get_flag_acc()] = mbr;
            break;
        
        case 5:
            // JMP/LDA
            
            if (acc) { // LDA
                local_read(mar, &mbr);
                zpage[acc - 1] = mbr;
                zpage[FLAG] &= ~(1 << ID);
            }
            else { // JMP
                zpage[PC] = (data_width_t) mar;
                zpage[FLAG] &= ~(1 << ID); // writeback to accumulator
            }
            break;
        
        default:
            // TBD
            printf("Illegal opcode - how?!?\n");
    }
    
    if ((zpage[FLAG] >> ID) & 1) set_flag_cycle(9);
    else set_flag_cycle(0);
    
    return;
}

/*
 * Cycle 4: IOWAIT
 */

void cycle_IOWAIT(void) {
    if (!(zpage[FLAG] & (1 << IO))) set_flag_cycle(0);
}

/*
 * Cycle 9: WTBACK
 */

void cycle_WTBACK(void) {
    set_flag_cycle(0);

    if (zpage[FLAG] & 1 << ID) local_write(mar, mbr);
    else zpage[get_flag_acc()] = mbr;
    
    return;
}

void step(void) {
    switch (get_flag_cycle()) {
        case 0:
            cycle_IFETCH();
            break;
        case 1:
            cycle_IFETCH();
            break;
        case 2:
            cycle_INADDR();
            break;
        case 3:
            cycle_EXEC();
            break;
        case 4:
            cycle_IOWAIT();
            break;
        case 9:
            cycle_WTBACK();
            break;
        default:
            printf("Invalid cycle - how?!?\n");
    }
    
    // printf("%04hX %04hX\n", zpage[7], zpage[15]);
}


