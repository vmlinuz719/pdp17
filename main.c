#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <poll.h>
#include <termios.h>

#include "bus.h"
#include "cpu.h"
#include "tty.h"

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

int cpu_running = 0;

void ctrl_c(int dummy) {
    cpu_running = 0;
}

unsigned int run_cpu(void) {
    unsigned int cycles = 0;
    
    run_tty = 1;
    cpu_running = 1;
    
    signal(SIGINT, ctrl_c);
    
    struct termios oldt, newt;
    tcgetattr(0, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON);
    tcsetattr(0, TCSANOW, &newt);
    
    pthread_t tty_tid;
    size_t tty_id = 2;
    pthread_create(&tty_tid, NULL, tty, (void *) &tty_id);
    
    pthread_t ttyin_tid;
    size_t ttyin_id = 3;
    pthread_create(&ttyin_tid, NULL, ttyin, (void *) &ttyin_id);
    
    while ((zpage[FLAG] & 0x1E0) >> 5 != 0xF && cpu_running) {
        step();
        cycles++;
    }
    
    zpage[FLAG] &= ~(0x1E0);
    
    run_tty = 0;
    pthread_join(tty_tid, NULL);
    pthread_join(ttyin_tid, NULL);
    
    tcsetattr(0, TCSANOW, &oldt);
    
    signal(SIGINT, NULL);
    printf("\n");
    
    return cycles;
}

void regs(void) {
    printf("%04hX %04hX %04hX %04hX %04hX %04hX %04hX %04hX\n",
        zpage[0], zpage[1], zpage[2], zpage[3],
        zpage[4], zpage[5], zpage[6], zpage[7]);
    
    printf("%04hX %04hX %04hX %04hX %04hX %04hX %04hX %04hX\n%04hX\n",
        zpage[8], zpage[9], zpage[10], zpage[11],
        zpage[12], zpage[13], zpage[14], zpage[15], zpage[FLAG]);
    
    return;
}

addr_width_t dump(addr_width_t address, data_width_t lines) {
    address &= 0xFFF8;
    for (int i = 0; i < lines; i++) {
        printf("%04hX|", (unsigned short) address);
        for (int j = 0; j < 8; j++) {
            data_width_t data;
            bus_read(address++, &data);
            address &= 0xFFFF;
            printf("%04hX ", data);
        }
        printf("\n");
    }
    return address;
}

int main(int argc, char *argv) {
    init_bus();
    switches = 0;
    
    install_unit(0, cpu_read, cpu_write);
    
    for (int i = 1; i <= 16; i++) // 4 KW core
        install_unit(i, mem_read, mem_write);
    
    install_attn(2, tty_attn);
    install_attn(3, tty_attn);
    
    int run = 1;
    
    printf("\"PDP-17\" - for evaluation use only\n");
    
    char command = '\0';
    data_width_t addr = 0;
    data_width_t data = 0;
    data_width_t dump_len = 0;
    
    while (run) {
        data_width_t value = 0;
        char garbage = '\0';
        
        printf("%04hX ", addr);
        
        char *line = NULL;
        size_t len = 0;
        getline(&line, &len, stdin);
        int valid = sscanf(line, " %c%4hX %c", &command, &value, &garbage);
        
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
            case 'u': // dump
                if (valid > 2) printf("?\n");
                else {
                    if (valid == 2) dump_len = value;
                    addr = dump(addr, dump_len);
                }
                break;
            case 'g': // go
                if (valid > 2) printf("?\n");
                else {
                    if (valid == 2) addr = value;
                    zpage[15] = addr;
                    unsigned int cycles = run_cpu();
                    addr = zpage[15];
                    // printf("%ud\n", cycles);
                }
                break;
            case 'c': // continue
                if (valid > 1) printf("?\n");
                else {
                    unsigned int cycles = run_cpu();
                    addr = zpage[15];
                    // printf("%ud\n", cycles);
                }
                break;
            case 's': // single step
                if (valid > 1) printf("?\n");
                else {
                    if ((zpage[FLAG] & 0x1E0) >> 5 == 0xF)
                        zpage[FLAG] &= ~(0x1E0);
                    step();
                }
                break;
            case 't': // step and show regs
                if (valid == 1) {
                    if ((zpage[FLAG] & 0x1E0) >> 5 == 0xF)
                        zpage[FLAG] &= ~(0x1E0);
                    step(); regs();
                }
                else printf("?\n");
                break;
            case 'r': // view regs
                if (valid == 1) regs();
                else if (valid == 2 && value <= 15) printf("%04hX\n", zpage[value]);
                else printf("?\n");
                break;
            case 'z': // zap registers
                if (valid == 1) {
                	for (int i = 0; i < 16; zpage[i++] = 0);
                	zpage[FLAG] = 0;
                }
                else printf("?\n");
                break;
            case 'w': // set switches
                if (valid == 2) switches = value;
                else if (valid == 1) printf("%04hX\n", switches);
                else printf("?\n");
                break;
            case 'q': // quit
                if (valid > 1) printf("?\n");
                else run = 0;
                break;
            default:
                printf("%s ?\n", line);
        }
        
        free(line);
    }
    
    return 0;
}
