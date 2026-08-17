/* ===========================================================================
 * mips.h -- Simulador MIPS de 5 etapas (implementacion propia)
 * ---------------------------------------------------------------------------
 * Diseño: cada etapa es una funcion pura (entrada -> salida por struct),
 * lo que permite testearlas de forma aislada sin levantar el CPU completo.
 *
 * ISA soportada (13 instrucciones):
 *   Tipo R : add, sub, and, or, slt, sll
 *   Tipo I : addi, lw, sw, beq, bne
 *   Tipo J : j, jal
 *
 * Formato de instruccion (32 bits):
 *   Tipo R : [31:26] op=0 | [25:21] rs | [20:16] rt | [15:11] rd |
 *            [10:6] shamt | [5:0] funct
 *   Tipo I : [31:26] op    | [25:21] rs | [20:16] rt | [15:0] inmediato
 *   Tipo J : [31:26] op    | [25:0] direccion
 * ===========================================================================
 */
#ifndef MIPS_H
#define MIPS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------------
 * Parametros del simulador
 * ------------------------------------------------------------------- */
#define NUM_REGS      32
#define IMEM_WORDS    256   /* 1KB de memoria de instrucciones */
#define DMEM_WORDS    256   /* 1KB de memoria de datos         */
#define NOP_INSTR     0x00000000u

enum { R_ZERO=0, R_AT=1, R_V0=2, R_V1=3, R_A0=4, R_A1=5, R_A2=6, R_A3=7,
       R_T0=8, R_T1=9, R_T2=10, R_T3=11, R_T4=12, R_T5=13, R_T6=14, R_T7=15,
       R_S0=16, R_S1=17, R_S2=18, R_S3=19, R_S4=20, R_S5=21, R_S6=22, R_S7=23,
       R_T8=24, R_T9=25, R_SP=29, R_FP=30, R_RA=31 };

/* ---------------------------------------------------------------------
 * Opcodes / funct
 * ------------------------------------------------------------------- */
typedef enum {
    OP_RTYPE = 0x00,
    OP_J     = 0x02,
    OP_JAL   = 0x03,
    OP_BEQ   = 0x04,
    OP_BNE   = 0x05,
    OP_ADDI  = 0x08,
    OP_LW    = 0x23,
    OP_SW    = 0x2B
} opcode_t;

typedef enum {
    FUNCT_SLL = 0x00,
    FUNCT_ADD = 0x20,
    FUNCT_SUB = 0x22,
    FUNCT_AND = 0x24,
    FUNCT_OR  = 0x25,
    FUNCT_SLT = 0x2A
} funct_t;

typedef enum {
    ALU_ADD, ALU_SUB, ALU_AND, ALU_OR, ALU_SLT, ALU_SLL, ALU_NOP
} alu_op_t;

/* ---------------------------------------------------------------------
 * ETAPA 1: IF (Instruction Fetch)
 * ------------------------------------------------------------------- */
typedef struct {
    uint32_t pc_actual;
    uint32_t instr;
    uint32_t pc_siguiente; /* pc_actual + 4 */
} if_out_t;

bool etapa_if(uint32_t pc_actual, const uint32_t imem[IMEM_WORDS], if_out_t *out);

/* ---------------------------------------------------------------------
 * ETAPA 2: ID (Instruction Decode)
 * ------------------------------------------------------------------- */
typedef struct {
    uint8_t  opcode;
    uint8_t  rs, rt, rd;
    uint8_t  shamt, funct;
    int16_t  inmediato;
    uint32_t direccion26;
} instr_decodificada_t;

typedef struct {
    bool reg_write;      /* escribe en banco de registros */
    bool mem_read;       /* lw */
    bool mem_write;      /* sw */
    bool mem_to_reg;      /* WB toma dato de memoria en vez de ALU */
    bool alu_usa_inm;     /* EX usa inmediato en vez de rt */
    bool es_branch_eq;    /* beq */
    bool es_branch_ne;    /* bne */
    bool es_jump;         /* j / jal */
    bool link;            /* jal: guarda pc+4 en $ra */
    uint8_t reg_destino;  /* rd (tipo R) o rt (tipo I) */
    alu_op_t alu_op;
} senales_control_t;

/* unidad de control: deriva senales_control_t a partir de opcode/funct */
senales_control_t unidad_control(uint8_t opcode, uint8_t funct);

/* ALU control: deriva la operacion real de la ALU */
alu_op_t alu_control(uint8_t opcode, uint8_t funct);

typedef struct {
    instr_decodificada_t di;
    senales_control_t    ctrl;
    int32_t valor_rs;
    int32_t valor_rt;
} id_out_t;

bool etapa_id(uint32_t instr, const int32_t regfile[NUM_REGS], id_out_t *out);

/* ---------------------------------------------------------------------
 * ETAPA 3: EX (Execute / ALU)
 * ------------------------------------------------------------------- */
typedef struct {
    int32_t resultado_alu;
    bool    zero;              /* resultado_alu == 0 -> usado por beq/bne */
    uint32_t pc_branch;        /* pc destino si el salto se toma */
    uint32_t pc_jump;          /* pc destino para j/jal */
} ex_out_t;

bool etapa_ex(const instr_decodificada_t *di, const senales_control_t *ctrl,
              int32_t valor_rs, int32_t valor_rt, uint32_t pc_siguiente,
              ex_out_t *out);

/* ---------------------------------------------------------------------
 * ETAPA 4: MEM (Memory Access)
 * ------------------------------------------------------------------- */
typedef struct {
    int32_t dato_leido;
} mem_out_t;

bool etapa_mem(const senales_control_t *ctrl, int32_t resultado_alu,
               int32_t valor_rt, int32_t dmem[DMEM_WORDS], mem_out_t *out);

/* ---------------------------------------------------------------------
 * ETAPA 5: WB (Write Back)
 * ------------------------------------------------------------------- */
bool etapa_wb(const senales_control_t *ctrl, uint8_t reg_destino,
              int32_t resultado_alu, int32_t dato_memoria,
              uint32_t pc_siguiente, int32_t regfile[NUM_REGS]);

/* ---------------------------------------------------------------------
 * CPU completo (encadena las 5 etapas instruccion por instruccion)
 * ------------------------------------------------------------------- */
typedef struct {
    int32_t  regfile[NUM_REGS];
    uint32_t imem[IMEM_WORDS];
    int32_t  dmem[DMEM_WORDS];
    uint32_t pc;
    uint64_t instrucciones_ejecutadas;
} cpu_t;

void cpu_init(cpu_t *cpu);
/* ejecuta UNA instruccion completa (IF->ID->EX->MEM->WB). Devuelve false si
 * la instruccion es NOP en una zona sin codigo (fin de programa). */
bool cpu_step(cpu_t *cpu);
/* corre hasta max_pasos instrucciones o hasta encontrar NOP */
void cpu_run(cpu_t *cpu, uint64_t max_pasos);

#endif /* MIPS_H */
