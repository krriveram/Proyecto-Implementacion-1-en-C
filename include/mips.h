/* ===========================================================================
 * mips.h -- Simulador MIPS con pipeline de 5 etapas
 * Proyecto Implementacion 1 en C  --  rama: implementacion_kenny
 * ---------------------------------------------------------------------------
 * Este encabezado declara TODO el contrato del simulador:
 *
 *   - Tipos de la ISA (opcodes, funct, operaciones de la ALU).
 *   - Senales de control y registros de pipeline (IF/ID, ID/EX, EX/MEM, MEM/WB).
 *   - Las 5 funciones principales, una por etapa, con sus structs de
 *     entrada/salida explicitos (esto permite probar cada etapa AISLADA, sin
 *     necesidad de levantar el procesador completo).
 *   - Las subfunciones internas de cada etapa (muxes, sumadores, extension de
 *     signo, unidad de control, ALU, ...) tambien se exponen para poder
 *     escribir unit tests de grano fino.
 *
 * ISA implementada (13 instrucciones):
 *   Tipo R : add, sub, and, or, nor, xor, jr
 *   Tipo I : addi, lw, sw, beq, bne
 *   Tipo J : j
 *
 * Convenciones de bits (instruccion de 32 bits):
 *   Tipo R : [31:26] opcode=0 | [25:21] rs | [20:16] rt | [15:11] rd |
 *            [10:6] shamt | [5:0] funct
 *   Tipo I : [31:26] opcode   | [25:21] rs | [20:16] rt | [15:0] inmediato
 *   Tipo J : [31:26] opcode   | [25:0] direccion
 * ===========================================================================
 */
#ifndef MIPS_H
#define MIPS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ===========================================================================
 * 1. PARAMETROS DEL SIMULADOR
 * =========================================================================*/
#define MIPS_NUM_REGS       32      /* banco de 32 registros de 32 bits      */
#define MIPS_IMEM_PALABRAS  256     /* 256 palabras = 1024 bytes de codigo   */
#define MIPS_DMEM_PALABRAS  256     /* 256 palabras = 1024 bytes de datos    */
#define MIPS_NOP            0x00000000u  /* codificacion canonica del NOP    */

/* Indices de registros con nombre (facilita leer los tests) */
enum {
    R_ZERO = 0,
    R_AT   = 1,
    R_V0   = 2,  R_V1 = 3,
    R_A0   = 4,  R_A1 = 5,  R_A2 = 6,  R_A3 = 7,
    R_T0   = 8,  R_T1 = 9,  R_T2 = 10, R_T3 = 11,
    R_T4   = 12, R_T5 = 13, R_T6 = 14, R_T7 = 15,
    R_S0   = 16, R_S1 = 17, R_S2 = 18, R_S3 = 19,
    R_S4   = 20, R_S5 = 21, R_S6 = 22, R_S7 = 23,
    R_T8   = 24, R_T9 = 25,
    R_SP   = 29, R_FP = 30, R_RA = 31
};

/* ===========================================================================
 * 2. ISA: opcodes y codigos de funcion
 * =========================================================================*/
typedef enum {
    OP_RTIPO = 0x00,   /* add, sub, and, or, nor, xor, jr */
    OP_J     = 0x02,
    OP_BEQ   = 0x04,
    OP_BNE   = 0x05,
    OP_ADDI  = 0x08,
    OP_LW    = 0x23,
    OP_SW    = 0x2B
} mips_opcode_t;

typedef enum {
    FUNCT_JR  = 0x08,
    FUNCT_ADD = 0x20,
    FUNCT_SUB = 0x22,
    FUNCT_AND = 0x24,
    FUNCT_OR  = 0x25,
    FUNCT_XOR = 0x26,
    FUNCT_NOR = 0x27
} mips_funct_t;

/* Operacion que la ALU debe ejecutar. La unidad de control la resuelve por
 * completo en la etapa ID (opcode + funct -> alu_op), de modo que la etapa 3
 * no necesita volver a mirar el campo funct: recibe una orden ya decodificada.
 */
typedef enum {
    ALU_NOP = 0,   /* no hace nada util (j, burbujas)                       */
    ALU_ADD,
    ALU_SUB,
    ALU_AND,
    ALU_OR,
    ALU_XOR,
    ALU_NOR,
    ALU_PASA_A     /* deja pasar el operando A sin modificar (usado por jr) */
} alu_op_t;

/* ===========================================================================
 * 3. CODIGOS DE ERROR (entradas no esperadas)
 * =========================================================================*/
typedef enum {
    MIPS_OK = 0,
    MIPS_ERR_OPCODE,        /* instruccion fuera de la ISA soportada        */
    MIPS_ERR_DIR_RANGO,     /* direccion fuera del tamano de la memoria     */
    MIPS_ERR_DIR_ALINEACION /* direccion no multiplo de 4                   */
} mips_err_t;

const char *mips_err_str(mips_err_t e);

/* ===========================================================================
 * 4. SENALES DE CONTROL
 * =========================================================================*/
typedef struct {
    bool     reg_write;   /* 1 = WB escribe en el banco de registros        */
    bool     mem_read;    /* 1 = lw                                         */
    bool     mem_write;   /* 1 = sw                                         */
    bool     mem_to_reg;  /* 1 = el dato escrito viene de memoria (lw)      */
    bool     alu_src;     /* 1 = operando B es el inmediato, 0 = registro rt*/
    bool     reg_dst;     /* 1 = destino rd (tipo R), 0 = destino rt        */
    bool     branch_eq;   /* beq                                            */
    bool     branch_ne;   /* bne                                            */
    bool     jump;        /* j   (destino calculado en ID)                  */
    bool     jump_reg;    /* jr  (destino = valor de rs, resuelto en EX)    */
    alu_op_t alu_op;
} ctrl_t;

/* ===========================================================================
 * 5. REGISTROS DE PIPELINE
 *    El campo "valido" en 0 representa una BURBUJA (NOP inyectado): la etapa
 *    siguiente no debe producir ningun efecto observable.
 * =========================================================================*/
typedef struct {
    bool     valido;
    uint32_t pc;        /* [31:0] PC de la instruccion (para trazas/errores)*/
    uint32_t pc_mas_4;  /* [31:0] PC + 4                                    */
    uint32_t instr;     /* [31:0] instruccion capturada                     */
} if_id_t;

typedef struct {
    bool     valido;
    uint32_t pc;
    uint32_t pc_mas_4;
    int32_t  rs_val;     /* [31:0] contenido del registro rs                */
    int32_t  rt_val;     /* [31:0] contenido del registro rt                */
    int32_t  imm;        /* [31:0] inmediato con signo extendido            */
    uint32_t pc_salto_j; /* [31:0] destino de la instruccion j              */
    uint8_t  rs;         /* [4:0]                                           */
    uint8_t  rt;         /* [4:0]                                           */
    uint8_t  rd;         /* [4:0]                                           */
    ctrl_t   ctrl;
} id_ex_t;

typedef struct {
    bool     valido;
    uint32_t pc;
    int32_t  alu_res;   /* [31:0] resultado de la ALU (o direccion lw/sw)   */
    int32_t  rt_val;    /* [31:0] dato a escribir en memoria (sw)           */
    uint8_t  reg_dst;   /* [4:0]  registro destino ya multiplexado          */
    ctrl_t   ctrl;
} ex_mem_t;

typedef struct {
    bool     valido;
    uint32_t pc;
    int32_t  alu_res;   /* [31:0]                                           */
    int32_t  mem_dato;  /* [31:0] dato leido de memoria (lw)                */
    uint8_t  reg_dst;   /* [4:0]                                            */
    ctrl_t   ctrl;
} mem_wb_t;

/* Selector de la unidad de cortocircuito (forwarding) */
typedef enum {
    FWD_NINGUNO = 0,  /* usar el valor leido del banco de registros */
    FWD_EX_MEM,       /* usar el resultado de la instruccion que esta en MEM */
    FWD_MEM_WB        /* usar el dato que WB escribe en este mismo ciclo     */
} fwd_sel_t;

/* ===========================================================================
 * 6. ETAPA 1: INSTRUCTION FETCH + PROGRAM COUNTER
 * =========================================================================*/
typedef struct {
    uint32_t        pc;            /* [31:0] PC actual                       */
    const uint32_t *imem;          /* memoria de instrucciones (palabras)    */
    uint32_t        imem_palabras; /* tamano de imem                         */
    bool            pc_src;        /* 1 = tomar pc_objetivo (salto en EX)    */
    uint32_t        pc_objetivo;   /* [31:0] destino del salto               */
    bool            stall;         /* 1 = congelar PC y mantener IF/ID       */
    bool            flush;         /* 1 = anular la instruccion capturada    */
} if_in_t;

typedef struct {
    uint32_t   pc_siguiente;  /* [31:0] valor que tomara el PC en el flanco  */
    if_id_t    if_id;         /* contenido a cargar en el registro IF/ID     */
    bool       cargar_if_id;  /* 0 = mantener el valor previo (stall)        */
    mips_err_t err;
} if_out_t;

void     etapa_if(const if_in_t *in, if_out_t *out);
/* subfunciones */
uint32_t if_sumador_pc4(uint32_t pc);
uint32_t if_mux_pc(uint32_t pc_mas_4, uint32_t pc_objetivo, bool pc_src);
uint32_t if_imem_leer(const uint32_t *imem, uint32_t palabras, uint32_t dir,
                      mips_err_t *err);

/* ===========================================================================
 * 7. ETAPA 2: INSTRUCTION DECODE
 * =========================================================================*/
typedef struct {
    if_id_t        if_id;
    const int32_t *regs;   /* banco de registros (ya escrito por WB)         */
    bool           stall;  /* 1 = inyectar burbuja (riesgo lw-uso)           */
    bool           flush;  /* 1 = anular (salto tomado)                      */
} id_in_t;

typedef struct {
    id_ex_t    id_ex;
    mips_err_t err;
} id_out_t;

void     etapa_id(const id_in_t *in, id_out_t *out);
/* subfunciones: extraccion de campos */
uint8_t  id_campo_opcode(uint32_t instr);
uint8_t  id_campo_rs(uint32_t instr);
uint8_t  id_campo_rt(uint32_t instr);
uint8_t  id_campo_rd(uint32_t instr);
uint8_t  id_campo_funct(uint32_t instr);
uint16_t id_campo_imm16(uint32_t instr);
uint32_t id_campo_addr26(uint32_t instr);
/* subfunciones: logica */
int32_t  id_extension_signo(uint16_t imm16);
bool     id_unidad_control(uint8_t opcode, uint8_t funct, ctrl_t *ctrl);
void     id_banco_leer(const int32_t *regs, uint8_t rs, uint8_t rt,
                       int32_t *dato1, int32_t *dato2);
uint32_t id_destino_j(uint32_t pc_mas_4, uint32_t addr26);
void     id_registros_usados(uint32_t instr, bool *usa_rs, bool *usa_rt);

/* ===========================================================================
 * 8. ETAPA 3: ALU (EXECUTE)
 * =========================================================================*/
typedef struct {
    id_ex_t   id_ex;
    fwd_sel_t fwd_a;       /* cortocircuito del operando A (rs)             */
    fwd_sel_t fwd_b;       /* cortocircuito del operando B (rt)             */
    int32_t   dato_ex_mem; /* [31:0] resultado de la instruccion en MEM     */
    int32_t   dato_mem_wb; /* [31:0] dato que WB escribe este ciclo         */
} alu_in_t;

typedef struct {
    ex_mem_t ex_mem;
    bool     pc_src;      /* 1 = hay que redirigir el PC                    */
    uint32_t pc_objetivo; /* [31:0] destino calculado                       */
    bool     cero;        /* bandera Z de la ALU (expuesta para los tests)  */
} alu_out_t;

void     etapa_alu(const alu_in_t *in, alu_out_t *out);
/* subfunciones */
int32_t  alu_ejecutar(int32_t a, int32_t b, alu_op_t op, bool *cero);
int32_t  alu_mux_src(int32_t rt_val, int32_t imm, bool alu_src);
uint8_t  alu_mux_reg_dst(uint8_t rt, uint8_t rd, bool reg_dst);
uint32_t alu_sumador_salto(uint32_t pc_mas_4, int32_t imm);
int32_t  alu_mux_cortocircuito(int32_t valor_banco, int32_t dato_ex_mem,
                               int32_t dato_mem_wb, fwd_sel_t sel);
bool     alu_decision_salto(const ctrl_t *ctrl, bool cero);

/* ===========================================================================
 * 9. ETAPA 4: MEMORY
 * =========================================================================*/
typedef struct {
    ex_mem_t  ex_mem;
    int32_t  *dmem;          /* memoria de datos (se modifica en sw)        */
    uint32_t  dmem_palabras;
} mem_in_t;

typedef struct {
    mem_wb_t   mem_wb;
    mips_err_t err;
} mem_out_t;

void etapa_mem(const mem_in_t *in, mem_out_t *out);
/* subfuncion: traduce direccion de byte a indice de palabra y valida */
bool mem_dir_valida(uint32_t dir, uint32_t palabras, uint32_t *indice,
                    mips_err_t *err);

/* ===========================================================================
 * 10. ETAPA 5: WRITE BACK
 * =========================================================================*/
typedef struct {
    mem_wb_t mem_wb;
} wb_in_t;

typedef struct {
    bool    reg_write;  /* 1 = escribir en el banco                         */
    uint8_t reg_dst;    /* [4:0] registro destino                           */
    int32_t dato;       /* [31:0] dato a escribir                           */
} wb_out_t;

void    etapa_wb(const wb_in_t *in, wb_out_t *out);
/* subfuncion */
int32_t wb_mux_mem_a_reg(int32_t alu_res, int32_t mem_dato, bool mem_to_reg);

/* ===========================================================================
 * 11. NUCLEO: estado completo y control del pipeline
 * =========================================================================*/
typedef struct {
    uint32_t pc;
    int32_t  regs[MIPS_NUM_REGS];
    uint32_t imem[MIPS_IMEM_PALABRAS];
    int32_t  dmem[MIPS_DMEM_PALABRAS];
    uint32_t n_instr;          /* instrucciones cargadas en imem            */

    if_id_t  if_id;            /* registros de pipeline                     */
    id_ex_t  id_ex;
    ex_mem_t ex_mem;
    mem_wb_t mem_wb;

    uint64_t ciclos;           /* estadisticas                              */
    uint64_t instr_retiradas;
    uint64_t stalls;
    uint64_t flushes;

    mips_err_t err;            /* primer error detectado                    */
    uint32_t   err_pc;
    bool       traza;          /* 1 = imprimir el estado de cada ciclo      */
} mips_t;

void     mips_reset(mips_t *cpu);
bool     mips_cargar_programa(mips_t *cpu, const uint32_t *prog, uint32_t n);
void     mips_ciclo(mips_t *cpu);
uint64_t mips_ejecutar(mips_t *cpu, uint64_t max_ciclos);
bool     mips_pipeline_vacio(const mips_t *cpu);
int32_t  mips_leer_dmem(const mips_t *cpu, uint32_t dir_byte);
bool     mips_escribir_dmem(mips_t *cpu, uint32_t dir_byte, int32_t valor);
void     mips_volcado(const mips_t *cpu);

/* Unidades de deteccion de riesgos (se exponen para poder probarlas) */
bool unidad_riesgos(const id_ex_t *en_ex, const if_id_t *en_id);
void unidad_cortocircuito(const id_ex_t *en_ex, const ex_mem_t *en_mem,
                          const mem_wb_t *en_wb, fwd_sel_t *fwd_a,
                          fwd_sel_t *fwd_b);

/* ===========================================================================
 * 12. ENSAMBLADOR / DESENSAMBLADOR (utilidades para armar los tests)
 * =========================================================================*/
uint32_t cod_r(uint8_t funct, uint8_t rs, uint8_t rt, uint8_t rd);
uint32_t cod_i(uint8_t opcode, uint8_t rs, uint8_t rt, int16_t imm);
uint32_t cod_j(uint8_t opcode, uint32_t destino_byte);
uint32_t cod_jr(uint8_t rs);
uint32_t cod_nop(void);
void     mips_desensamblar(uint32_t instr, char *buf, size_t n);

#endif /* MIPS_H */
