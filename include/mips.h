#ifndef MIPS_H
#define MIPS_H

#include <stdint.h>

typedef struct {
    int32_t  regs[32];
    uint32_t pc;
    uint32_t instr_mem[1024];
    uint8_t  data_mem[4096];
    int      flag;
} cpu_state_t;

#endif
