/* main.c -- Punto de entrada: carga un programa de ejemplo y muestra el
 * estado final de registros y memoria de datos. */
#include <stdio.h>
#include "mips.h"

/* Ensamblador -> hex manual, comentado instruccion por instruccion.
 *
 *   addi $t0, $zero, 5      ; t0 = 5
 *   addi $t1, $zero, 10     ; t1 = 10
 *   add  $t2, $t0, $t1      ; t2 = 15
 *   sw   $t2, 0($zero)      ; mem[0] = 15
 *   lw   $t3, 0($zero)      ; t3 = mem[0] = 15
 *   beq  $t2, $t3, +2       ; salta si t2 == t3 (se cumple)
 *   addi $t4, $zero, 99     ; (se salta, no se ejecuta)
 *   addi $t5, $zero, 1      ; (se salta, no se ejecuta)
 *   addi $t6, $zero, 7      ; t6 = 7   <- destino del beq
 */
int main(void) {
    cpu_t cpu;
    cpu_init(&cpu);

    /* Construimos el programa a mano con macros de ensamblado simple
     * (mas legible y menos propenso a error que hex crudo). */
    #define R_TYPE(op, rs, rt, rd, sh, fn) \
        (((op)<<26)|((rs)<<21)|((rt)<<16)|((rd)<<11)|((sh)<<6)|(fn))
    #define I_TYPE(op, rs, rt, imm) \
        (((op)<<26)|((rs)<<21)|((rt)<<16)|((imm)&0xFFFF))

    uint32_t prog[] = {
        I_TYPE(OP_ADDI, R_ZERO, R_T0, 5),                 /* addi $t0,$zero,5   */
        I_TYPE(OP_ADDI, R_ZERO, R_T1, 10),                /* addi $t1,$zero,10  */
        R_TYPE(OP_RTYPE, R_T0, R_T1, R_T2, 0, FUNCT_ADD), /* add  $t2,$t0,$t1   */
        I_TYPE(OP_SW,   R_ZERO, R_T2, 0),                 /* sw   $t2,0($zero)  */
        I_TYPE(OP_LW,   R_ZERO, R_T3, 0),                 /* lw   $t3,0($zero)  */
        I_TYPE(OP_BEQ,  R_T2, R_T3, 2),                   /* beq  $t2,$t3,+2    */
        I_TYPE(OP_ADDI, R_ZERO, R_T4, 99),                /* (saltada)          */
        I_TYPE(OP_ADDI, R_ZERO, R_T5, 1),                 /* (saltada)          */
        I_TYPE(OP_ADDI, R_ZERO, R_T6, 7),                 /* addi $t6,$zero,7   */
        NOP_INSTR
    };

    for (size_t i = 0; i < sizeof(prog)/sizeof(prog[0]); i++) cpu.imem[i] = prog[i];

    cpu_run(&cpu, 1000);

    printf("=== Simulador MIPS - resultado programa demo ===\n");
    printf("Instrucciones ejecutadas: %llu\n", (unsigned long long)cpu.instrucciones_ejecutadas);
    printf("PC final: 0x%08X\n", cpu.pc);
    printf("$t0=%d $t1=%d $t2=%d $t3=%d $t4=%d $t5=%d $t6=%d\n",
           cpu.regfile[R_T0], cpu.regfile[R_T1], cpu.regfile[R_T2],
           cpu.regfile[R_T3], cpu.regfile[R_T4], cpu.regfile[R_T5], cpu.regfile[R_T6]);
    printf("mem[0]=%d\n", cpu.dmem[0]);

    return 0;
}
