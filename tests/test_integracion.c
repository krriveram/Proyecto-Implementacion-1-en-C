/* test_integracion.c -- corre programas MIPS completos por el pipeline de
 * 5 etapas y verifica el estado final contra vectores de prueba calculados
 * a mano. */
#include "../include/mips.h"
#include "test_common.h"

#define R_TYPE(op, rs, rt, rd, sh, fn) \
    (((op)<<26)|((rs)<<21)|((rt)<<16)|((rd)<<11)|((sh)<<6)|(fn))
#define I_TYPE(op, rs, rt, imm) \
    (((op)<<26)|((rs)<<21)|((rt)<<16)|((imm)&0xFFFF))
#define J_TYPE(op, addr) \
    (((op)<<26)|((addr)&0x03FFFFFF))

/* Vector 1: aritmetica basica encadenada
 *   t0=5; t1=10; t2=t0+t1; t3=t2-t0; t4=t2 AND t3; t5=t2 OR t3
 * Esperado: t0=5 t1=10 t2=15 t3=10 t4=10 t5=15 */
static void vector_aritmetica(void) {
    cpu_t cpu; cpu_init(&cpu);
    uint32_t prog[] = {
        I_TYPE(OP_ADDI, R_ZERO, R_T0, 5),
        I_TYPE(OP_ADDI, R_ZERO, R_T1, 10),
        R_TYPE(OP_RTYPE, R_T0, R_T1, R_T2, 0, FUNCT_ADD),
        R_TYPE(OP_RTYPE, R_T2, R_T0, R_T3, 0, FUNCT_SUB),
        R_TYPE(OP_RTYPE, R_T2, R_T3, R_T4, 0, FUNCT_AND),
        R_TYPE(OP_RTYPE, R_T2, R_T3, R_T5, 0, FUNCT_OR),
        NOP_INSTR
    };
    for (size_t i = 0; i < sizeof(prog)/sizeof(prog[0]); i++) cpu.imem[i] = prog[i];
    cpu_run(&cpu, 100);

    ASSERT_EQ_INT("[aritmetica] t0", 5,  cpu.regfile[R_T0]);
    ASSERT_EQ_INT("[aritmetica] t1", 10, cpu.regfile[R_T1]);
    ASSERT_EQ_INT("[aritmetica] t2", 15, cpu.regfile[R_T2]);
    ASSERT_EQ_INT("[aritmetica] t3", 10, cpu.regfile[R_T3]);
    ASSERT_EQ_INT("[aritmetica] t4", 10, cpu.regfile[R_T4]);
    ASSERT_EQ_INT("[aritmetica] t5", 15, cpu.regfile[R_T5]);
    ASSERT_EQ_INT("[aritmetica] instrucciones", 6, cpu.instrucciones_ejecutadas);
}

/* Vector 2: memoria (sw/lw) sobre 3 direcciones distintas
 * Esperado: mem[0]=100 mem[1]=200 mem[2]=300 ; t5=100 t6=200 t7=300 */
static void vector_memoria(void) {
    cpu_t cpu; cpu_init(&cpu);
    uint32_t prog[] = {
        I_TYPE(OP_ADDI, R_ZERO, R_T0, 100),
        I_TYPE(OP_ADDI, R_ZERO, R_T1, 200),
        I_TYPE(OP_ADDI, R_ZERO, R_T2, 300),
        I_TYPE(OP_SW, R_ZERO, R_T0, 0),
        I_TYPE(OP_SW, R_ZERO, R_T1, 4),
        I_TYPE(OP_SW, R_ZERO, R_T2, 8),
        I_TYPE(OP_LW, R_ZERO, R_T5, 0),
        I_TYPE(OP_LW, R_ZERO, R_T6, 4),
        I_TYPE(OP_LW, R_ZERO, R_T7, 8),
        NOP_INSTR
    };
    for (size_t i = 0; i < sizeof(prog)/sizeof(prog[0]); i++) cpu.imem[i] = prog[i];
    cpu_run(&cpu, 100);

    ASSERT_EQ_INT("[memoria] mem[0]", 100, cpu.dmem[0]);
    ASSERT_EQ_INT("[memoria] mem[1]", 200, cpu.dmem[1]);
    ASSERT_EQ_INT("[memoria] mem[2]", 300, cpu.dmem[2]);
    ASSERT_EQ_INT("[memoria] t5", 100, cpu.regfile[R_T5]);
    ASSERT_EQ_INT("[memoria] t6", 200, cpu.regfile[R_T6]);
    ASSERT_EQ_INT("[memoria] t7", 300, cpu.regfile[R_T7]);
}

/* Vector 3: control de flujo (beq tomado + bne no tomado)
 * Programa: t0=5 t1=5 (iguales) -> beq salta 2 instrucciones -> t2=7
 * luego t3=1 t4=2 (distintos) -> bne salta 2 instrucciones -> t5=9  */
static void vector_saltos(void) {
    cpu_t cpu; cpu_init(&cpu);
    uint32_t prog[] = {
        I_TYPE(OP_ADDI, R_ZERO, R_T0, 5),
        I_TYPE(OP_ADDI, R_ZERO, R_T1, 5),
        I_TYPE(OP_BEQ, R_T0, R_T1, 2),      /* pc=8 -> salta a pc=8+4+2*4=20 */
        I_TYPE(OP_ADDI, R_ZERO, R_T2, 111), /* saltada */
        I_TYPE(OP_ADDI, R_ZERO, R_T2, 222), /* saltada */
        I_TYPE(OP_ADDI, R_ZERO, R_T2, 7),   /* pc=20: destino del beq */
        I_TYPE(OP_ADDI, R_ZERO, R_T3, 1),
        I_TYPE(OP_ADDI, R_ZERO, R_T4, 2),
        I_TYPE(OP_BNE, R_T3, R_T4, 2),      /* distintos -> salta */
        I_TYPE(OP_ADDI, R_ZERO, R_T5, 111), /* saltada */
        I_TYPE(OP_ADDI, R_ZERO, R_T5, 222), /* saltada */
        I_TYPE(OP_ADDI, R_ZERO, R_T5, 9),   /* destino del bne */
        NOP_INSTR
    };
    for (size_t i = 0; i < sizeof(prog)/sizeof(prog[0]); i++) cpu.imem[i] = prog[i];
    cpu_run(&cpu, 100);

    ASSERT_EQ_INT("[saltos] t2 (beq tomado)", 7, cpu.regfile[R_T2]);
    ASSERT_EQ_INT("[saltos] t5 (bne tomado)", 9, cpu.regfile[R_T5]);
}

/* Vector 4: jal/jump + $ra
 * main: jal subrutina ; addi t0,zero,999 (se ejecuta al volver, PERO no hay
 * salto de vuelta automatico -- este simulador no implementa jr, asi que
 * solo validamos que $ra guarda la direccion de retorno correcta). */
static void vector_jal(void) {
    cpu_t cpu; cpu_init(&cpu);
    uint32_t prog[] = {
        J_TYPE(OP_JAL, 2),                  /* pc=0: jal -> pc=8, $ra=4 */
        I_TYPE(OP_ADDI, R_ZERO, R_T0, 999), /* pc=4 (no deberia ejecutarse) */
        I_TYPE(OP_ADDI, R_ZERO, R_T1, 42),  /* pc=8: destino del jal */
        NOP_INSTR
    };
    for (size_t i = 0; i < sizeof(prog)/sizeof(prog[0]); i++) cpu.imem[i] = prog[i];
    cpu_run(&cpu, 100);

    ASSERT_EQ_INT("[jal] $ra = pc+4 del jal", 4, cpu.regfile[R_RA]);
    ASSERT_EQ_INT("[jal] t1 se ejecuta tras el salto", 42, cpu.regfile[R_T1]);
    ASSERT_EQ_INT("[jal] t0 nunca se ejecuta", 0, cpu.regfile[R_T0]);
}

int main(void) {
    vector_aritmetica();
    vector_memoria();
    vector_saltos();
    vector_jal();
    RESUMEN_TESTS("test_integracion");
}
