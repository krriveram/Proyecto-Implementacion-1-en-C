#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>

#define MEM_SIZE 256 // 256 palabras de 32 bits = 1024 bytes

// Enumeración para operaciones de la ALU
typedef enum {
    ALU_OP_NONE = 0,
    ALU_OP_ADD,
    ALU_OP_SUB,
    ALU_OP_R
} ALUOp;

// Señales de Control
typedef struct {
    uint8_t reg_dst;
    uint8_t alu_src;
    uint8_t mem_to_reg;
    uint8_t reg_write;
    uint8_t mem_read;
    uint8_t mem_write;
    uint8_t branch;
    uint8_t jump;
    ALUOp alu_op;
} ControlSignals;

// Registros del Pipeline (Buffers intermedios)
typedef struct {
    uint32_t pc_plus_4;
    uint32_t instr;
    bool valid;
} IF_ID;

typedef struct {
    uint32_t pc_plus_4;
    uint32_t rs_val;
    uint32_t rt_val;
    int32_t imm;
    uint8_t rs;
    uint8_t rt;
    uint8_t rd;
    uint8_t funct;
    ControlSignals ctrl;
    bool valid;
} ID_EX;

typedef struct {
    int32_t alu_out;
    uint32_t rt_val;
    uint8_t dest_reg;
    ControlSignals ctrl;
    bool valid;
} EX_MEM;

typedef struct {
    int32_t mem_out;
    int32_t alu_out;
    uint8_t dest_reg;
    ControlSignals ctrl;
    bool valid;
} MEM_WB;

// Estado del Procesador MIPS
typedef struct {
    uint32_t pc;
    uint32_t regs[32];
    uint32_t imem[MEM_SIZE];
    uint32_t dmem[MEM_SIZE];

    IF_ID if_id;
    ID_EX id_ex;
    EX_MEM ex_mem;
    MEM_WB mem_wb;

    uint32_t next_pc;
    bool branch_taken;
} MIPSPipeline;

// ==========================================
// FUNCIONES AUXILIARES
// ==========================================
void cpu_reset(MIPSPipeline* cpu) {
    memset(cpu, 0, sizeof(MIPSPipeline));
}

void imem_write(MIPSPipeline* cpu, uint32_t addr, uint32_t data) {
    if ((addr >> 2) < MEM_SIZE) cpu->imem[addr >> 2] = data;
}

uint32_t imem_read(MIPSPipeline* cpu, uint32_t addr) {
    if ((addr >> 2) < MEM_SIZE) return cpu->imem[addr >> 2];
    return 0;
}

void dmem_write(MIPSPipeline* cpu, uint32_t addr, uint32_t data) {
    if ((addr >> 2) < MEM_SIZE) cpu->dmem[addr >> 2] = data;
}

uint32_t dmem_read(MIPSPipeline* cpu, uint32_t addr) {
    if ((addr >> 2) < MEM_SIZE) return cpu->dmem[addr >> 2];
    return 0;
}

// ==========================================
// ETAPAS DEL PIPELINE
// ==========================================
void instruction_fetch(MIPSPipeline* cpu) {
    if (cpu->branch_taken) {
        cpu->pc = cpu->next_pc;
        cpu->branch_taken = false;
    }

    uint32_t instr = imem_read(cpu, cpu->pc); // 0 = NOP
    uint32_t pc_plus_4 = cpu->pc + 4;
    
    cpu->if_id.pc_plus_4 = pc_plus_4;
    cpu->if_id.instr = instr;
    cpu->if_id.valid = (instr != 0);
    
    cpu->pc = pc_plus_4;
}

void instruction_decode(MIPSPipeline* cpu) {
    if (!cpu->if_id.valid) {
        cpu->id_ex.valid = false;
        return;
    }

    uint32_t instr = cpu->if_id.instr;
    uint32_t pc_plus_4 = cpu->if_id.pc_plus_4;

    uint8_t opcode = (instr >> 26) & 0x3F;
    uint8_t rs = (instr >> 21) & 0x1F;
    uint8_t rt = (instr >> 16) & 0x1F;
    uint8_t rd = (instr >> 11) & 0x1F;
    uint8_t funct = instr & 0x3F;
    int32_t imm = (int32_t)((int16_t)(instr & 0xFFFF));
    uint32_t address = instr & 0x3FFFFFF;

    ControlSignals ctrl = {0};

    if (opcode == 0) { // R-Type
        ctrl.reg_dst = 1;
        ctrl.reg_write = 1;
        ctrl.alu_op = ALU_OP_R;
        if (funct == 8) { // jr
            ctrl.jump = 2;
            ctrl.reg_write = 0;
        }
    } else if (opcode == 8) { // addi
        ctrl.alu_src = 1;
        ctrl.reg_write = 1;
        ctrl.alu_op = ALU_OP_ADD;
    } else if (opcode == 35) { // lw
        ctrl.alu_src = 1;
        ctrl.mem_to_reg = 1;
        ctrl.reg_write = 1;
        ctrl.mem_read = 1;
        ctrl.alu_op = ALU_OP_ADD;
    } else if (opcode == 43) { // sw
        ctrl.alu_src = 1;
        ctrl.mem_write = 1;
        ctrl.alu_op = ALU_OP_ADD;
    } else if (opcode == 4) { // beq
        ctrl.branch = 1;
        ctrl.alu_op = ALU_OP_SUB;
    } else if (opcode == 5) { // bne
        ctrl.branch = 2;
        ctrl.alu_op = ALU_OP_SUB;
    } else if (opcode == 2) { // j
        ctrl.jump = 1;
    }

    uint32_t rs_val = cpu->regs[rs];
    uint32_t rt_val = cpu->regs[rt];

    // Resolución de saltos absolutos en ID (J, JR)
    if (ctrl.jump == 1) {
        cpu->next_pc = (pc_plus_4 & 0xF0000000) | (address << 2);
        cpu->branch_taken = true;
    } else if (ctrl.jump == 2) {
        cpu->next_pc = rs_val;
        cpu->branch_taken = true;
    }

    cpu->id_ex.pc_plus_4 = pc_plus_4;
    cpu->id_ex.rs_val = rs_val;
    cpu->id_ex.rt_val = rt_val;
    cpu->id_ex.imm = imm;
    cpu->id_ex.rs = rs;
    cpu->id_ex.rt = rt;
    cpu->id_ex.rd = rd;
    cpu->id_ex.funct = funct;
    cpu->id_ex.ctrl = ctrl;
    cpu->id_ex.valid = true;
}

void alu_execute(MIPSPipeline* cpu) {
    if (!cpu->id_ex.valid) {
        cpu->ex_mem.valid = false;
        return;
    }

    ControlSignals ctrl = cpu->id_ex.ctrl;
    uint32_t rs_val = cpu->id_ex.rs_val;
    uint32_t rt_val = cpu->id_ex.rt_val;
    int32_t imm = cpu->id_ex.imm;
    uint8_t funct = cpu->id_ex.funct;

    int32_t operand2 = ctrl.alu_src ? imm : (int32_t)rt_val;
    int32_t alu_out = 0;

    if (ctrl.alu_op == ALU_OP_ADD || (ctrl.alu_op == ALU_OP_R && funct == 32)) {
        alu_out = (int32_t)rs_val + operand2;
    } else if (ctrl.alu_op == ALU_OP_SUB || (ctrl.alu_op == ALU_OP_R && funct == 34)) {
        alu_out = (int32_t)rs_val - operand2;
    } else if (ctrl.alu_op == ALU_OP_R && funct == 36) { // and
        alu_out = rs_val & operand2;
    } else if (ctrl.alu_op == ALU_OP_R && funct == 37) { // or
        alu_out = rs_val | operand2;
    } else if (ctrl.alu_op == ALU_OP_R && funct == 39) { // nor
        alu_out = ~(rs_val | operand2);
    } else if (ctrl.alu_op == ALU_OP_R && funct == 38) { // xor
        alu_out = rs_val ^ operand2;
    }

    bool zero = (alu_out == 0);
    if ((ctrl.branch == 1 && zero) || (ctrl.branch == 2 && !zero)) {
        cpu->next_pc = cpu->id_ex.pc_plus_4 + (imm << 2);
        cpu->branch_taken = true;
    }

    uint8_t dest_reg = ctrl.reg_dst ? cpu->id_ex.rd : cpu->id_ex.rt;

    cpu->ex_mem.alu_out = alu_out;
    cpu->ex_mem.rt_val = rt_val;
    cpu->ex_mem.dest_reg = dest_reg;
    cpu->ex_mem.ctrl = ctrl;
    cpu->ex_mem.valid = true;
}

void memory_access(MIPSPipeline* cpu) {
    if (!cpu->ex_mem.valid) {
        cpu->mem_wb.valid = false;
        return;
    }

    ControlSignals ctrl = cpu->ex_mem.ctrl;
    int32_t alu_out = cpu->ex_mem.alu_out;
    uint32_t rt_val = cpu->ex_mem.rt_val;
    int32_t mem_out = 0;

    if (ctrl.mem_write) {
        dmem_write(cpu, (uint32_t)alu_out, rt_val);
    }
    if (ctrl.mem_read) {
        mem_out = (int32_t)dmem_read(cpu, (uint32_t)alu_out);
    }

    cpu->mem_wb.mem_out = mem_out;
    cpu->mem_wb.alu_out = alu_out;
    cpu->mem_wb.dest_reg = cpu->ex_mem.dest_reg;
    cpu->mem_wb.ctrl = ctrl;
    cpu->mem_wb.valid = true;
}

void write_back(MIPSPipeline* cpu) {
    if (!cpu->mem_wb.valid) {
        return;
    }

    ControlSignals ctrl = cpu->mem_wb.ctrl;
    uint8_t dest_reg = cpu->mem_wb.dest_reg;

    if (ctrl.reg_write && dest_reg != 0) { 
        int32_t write_data = ctrl.mem_to_reg ? cpu->mem_wb.mem_out : cpu->mem_wb.alu_out;
        cpu->regs[dest_reg] = (uint32_t)write_data;
    }
}

// Ejecución de 1 ciclo
void cpu_step(MIPSPipeline* cpu) {
    write_back(cpu);
    memory_access(cpu);
    alu_execute(cpu);
    instruction_decode(cpu);
    instruction_fetch(cpu);
}

// ==========================================
// ENTORNO DE VECTORES DE PRUEBA (INTEGRALES)
// ==========================================
typedef struct {
    uint8_t reg_idx;
    uint32_t expected_val;
} RegExpectation;

typedef struct {
    uint32_t mem_addr;
    uint32_t expected_val;
} MemExpectation;

typedef struct {
    const char* test_name;
    uint32_t instructions[32]; // Instrucciones del programa (vector)
    size_t inst_count;         // Cantidad de instrucciones
    int cycles;                // Ciclos de reloj a simular
    
    RegExpectation reg_exps[10];
    size_t reg_exp_count;
    
    MemExpectation mem_exps[10];
    size_t mem_exp_count;
} TestVector;

void run_test_vector(TestVector* vec) {
    MIPSPipeline cpu;
    cpu_reset(&cpu);

    // Cargar instrucciones en memoria
    for (size_t i = 0; i < vec->inst_count; i++) {
        imem_write(&cpu, (uint32_t)(i * 4), vec->instructions[i]);
    }

    // Ejecutar pipeline
    for (int i = 0; i < vec->cycles; i++) {
        cpu_step(&cpu);
    }

    // Validar Registros
    for (size_t i = 0; i < vec->reg_exp_count; i++) {
        uint8_t r = vec->reg_exps[i].reg_idx;
        uint32_t expected = vec->reg_exps[i].expected_val;
        uint32_t actual = cpu.regs[r];
        if (actual != expected) {
            printf("[FALLO] %s -> Registro $%d: Esperado %u, Actual %u\n", vec->test_name, r, expected, actual);
            assert(actual == expected);
        }
    }

    // Validar Memoria
    for (size_t i = 0; i < vec->mem_exp_count; i++) {
        uint32_t addr = vec->mem_exps[i].mem_addr;
        uint32_t expected = vec->mem_exps[i].expected_val;
        uint32_t actual = dmem_read(&cpu, addr);
        if (actual != expected) {
            printf("[FALLO] %s -> Memoria[%u]: Esperado %u, Actual %u\n", vec->test_name, addr, expected, actual);
            assert(actual == expected);
        }
    }
    
    printf("Test Integral: %s ... OK\n", vec->test_name);
}

void test_integral_vectors() {
    // Vector 1: Prueba de Aritmética Lógica (ALU)
    TestVector vec_alu = {
        .test_name = "Suite ALU (add, sub, addi, and, or, xor, nor)",
        .cycles = 15, // 7 inst + 5 etapas
        .instructions = {
            (8 << 26) | (0 << 21) | (8 << 16) | 10,                 // addi $t0, $0, 10
            (8 << 26) | (0 << 21) | (9 << 16) | 5,                  // addi $t1, $0, 5
            (0 << 26) | (8 << 21) | (9 << 16) | (10 << 11) | 32,    // add $t2, $t0, $t1  (15)
            (0 << 26) | (8 << 21) | (9 << 16) | (11 << 11) | 34,    // sub $t3, $t0, $t1  (5)
            (0 << 26) | (8 << 21) | (9 << 16) | (12 << 11) | 36,    // and $t4, $t0, $t1  (0)
            (0 << 26) | (8 << 21) | (9 << 16) | (13 << 11) | 37,    // or $t5, $t0, $t1   (15)
            (0 << 26) | (8 << 21) | (9 << 16) | (14 << 11) | 38,    // xor $t6, $t0, $t1  (15)
        },
        .inst_count = 7,
        .reg_exps = { {8, 10}, {9, 5}, {10, 15}, {11, 5}, {12, 0}, {13, 15}, {14, 15} },
        .reg_exp_count = 7,
        .mem_exp_count = 0
    };

    // Vector 2: Prueba de Acceso a Memoria (sw, lw)
    TestVector vec_mem = {
        .test_name = "Suite Memoria (sw, lw)",
        .cycles = 10,
        .instructions = {
            (8 << 26) | (0 << 21) | (8 << 16) | 42,                 // addi $t0, $0, 42
            (43 << 26) | (0 << 21) | (8 << 16) | 4,                 // sw $t0, 4($0)
            (35 << 26) | (0 << 21) | (9 << 16) | 4,                 // lw $t1, 4($0)
        },
        .inst_count = 3,
        .reg_exps = { {8, 42}, {9, 42} },
        .reg_exp_count = 2,
        .mem_exps = { {4, 42} },
        .mem_exp_count = 1
    };

    // Vector 3: Prueba de Saltos y Control de Flujo (beq, bne)
    TestVector vec_ctrl = {
        .test_name = "Suite Control de Flujo (beq tomado y bne ignorado)",
        .cycles = 15,
        .instructions = {
            (8 << 26) | (0 << 21) | (8 << 16) | 1,                  // 0: addi $t0, $0, 1
            (4 << 26) | (8 << 21) | (8 << 16) | 2,                  // 4: beq $t0, $t0, 2 (Salta 2 intrucciones)
            (8 << 26) | (0 << 21) | (9 << 16) | 99,                 // 8: addi $t1, $0, 99 (Ignorada por branch)
            (8 << 26) | (0 << 21) | (10 << 16) | 99,                // 12: addi $t2, $0, 99 (Ignorada por branch)
            (8 << 26) | (0 << 21) | (11 << 16) | 7,                 // 16: addi $t3, $0, 7 (Destino del branch)
            (5 << 26) | (8 << 21) | (11 << 16) | 3,                 // 20: bne $t0, $t3, 3 (Salta porque 1 != 7)
            (8 << 26) | (0 << 21) | (12 << 16) | 99,                // 24: addi $t4, $0, 99 (Ignorada)
        },
        .inst_count = 7,
        .reg_exps = { {8, 1}, {9, 0}, {10, 0}, {11, 7}, {12, 0} }, // Los registros ignorados quedan en 0
        .reg_exp_count = 5,
        .mem_exp_count = 0
    };

    run_test_vector(&vec_alu);
    run_test_vector(&vec_mem);
    run_test_vector(&vec_ctrl);
}

int main() {
    printf("Iniciando Verificación Integral del Simulador MIPS...\n");
    printf("=====================================================\n");
    
    test_integral_vectors();
    
    printf("=====================================================\n");
    printf("Todos los vectores de prueba han pasado sin errores.\n");
    return 0;
}
