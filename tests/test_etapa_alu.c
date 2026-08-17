#include "../include/mips.h"
#include "test_common.h"

int main(void) {
    instr_decodificada_t di = {0};
    senales_control_t ctrl = {0};
    ex_out_t out;

    /* ADD: 7 + 3 = 10 */
    ctrl.alu_op = ALU_ADD;
    etapa_ex(&di, &ctrl, 7, 3, 100, &out);
    ASSERT_EQ_INT("7+3=10", 10, out.resultado_alu);
    ASSERT_TRUE("no es zero", !out.zero);

    /* SUB que da cero -> zero=true (usado por beq) */
    ctrl.alu_op = ALU_SUB;
    etapa_ex(&di, &ctrl, 5, 5, 100, &out);
    ASSERT_EQ_INT("5-5=0", 0, out.resultado_alu);
    ASSERT_TRUE("zero flag activo", out.zero);

    /* SLT: 3 < 5 -> 1 */
    ctrl.alu_op = ALU_SLT;
    etapa_ex(&di, &ctrl, 3, 5, 100, &out);
    ASSERT_EQ_INT("3<5 -> 1", 1, out.resultado_alu);

    /* SLL: 1 << 4 = 16 */
    di.shamt = 4;
    ctrl.alu_op = ALU_SLL;
    etapa_ex(&di, &ctrl, 0, 1, 100, &out);
    ASSERT_EQ_INT("1<<4=16", 16, out.resultado_alu);

    /* ALU con inmediato (addi -3) */
    di.inmediato = -3;
    ctrl.alu_op = ALU_ADD;
    ctrl.alu_usa_inm = true;
    etapa_ex(&di, &ctrl, 10, 999, 100, &out); /* valor_rt no se usa */
    ASSERT_EQ_INT("10+(-3)=7", 7, out.resultado_alu);

    /* calculo de pc_branch: pc_siguiente + (inmediato<<2) */
    di.inmediato = 2;
    etapa_ex(&di, &ctrl, 0, 0, 100, &out);
    ASSERT_EQ_INT("pc_branch = 100 + 8", 108, out.pc_branch);

    /* calculo de pc_jump */
    di.direccion26 = 0x10;
    etapa_ex(&di, &ctrl, 0, 0, 0x00000004, &out);
    ASSERT_EQ_INT("pc_jump = direccion26<<2", 0x40, out.pc_jump);

    /* alu_control: verificar mapeo opcode/funct -> operacion */
    ASSERT_EQ_INT("alu_control add", ALU_ADD, alu_control(OP_RTYPE, FUNCT_ADD));
    ASSERT_EQ_INT("alu_control sub", ALU_SUB, alu_control(OP_RTYPE, FUNCT_SUB));
    ASSERT_EQ_INT("alu_control lw usa ADD", ALU_ADD, alu_control(OP_LW, 0));
    ASSERT_EQ_INT("alu_control beq usa SUB", ALU_SUB, alu_control(OP_BEQ, 0));

    RESUMEN_TESTS("test_etapa_alu");
}
