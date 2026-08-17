#include "../include/mips.h"
#include "test_common.h"

int main(void) {
    int32_t regfile[NUM_REGS] = {0};
    regfile[R_T0] = 3;
    regfile[R_T1] = 4;

    /* add $t2, $t0, $t1 */
    uint32_t instr_add = (OP_RTYPE<<26)|(R_T0<<21)|(R_T1<<16)|(R_T2<<11)|(0<<6)|FUNCT_ADD;
    id_out_t out;
    etapa_id(instr_add, regfile, &out);

    ASSERT_EQ_INT("opcode add", OP_RTYPE, out.di.opcode);
    ASSERT_EQ_INT("funct add", FUNCT_ADD, out.di.funct);
    ASSERT_EQ_INT("rd = t2", R_T2, out.di.rd);
    ASSERT_EQ_INT("valor_rs", 3, out.valor_rs);
    ASSERT_EQ_INT("valor_rt", 4, out.valor_rt);
    ASSERT_TRUE("reg_write activo en add", out.ctrl.reg_write);
    ASSERT_EQ_INT("reg_destino = rd", R_T2, out.ctrl.reg_destino);
    ASSERT_EQ_INT("alu_op = ADD", ALU_ADD, out.ctrl.alu_op);

    /* addi $t3, $zero, -5  -> inmediato con signo */
    uint32_t instr_addi = (OP_ADDI<<26)|(R_ZERO<<21)|(R_T3<<16)|((uint16_t)(-5));
    etapa_id(instr_addi, regfile, &out);
    ASSERT_EQ_INT("inmediato con signo = -5", -5, out.di.inmediato);
    ASSERT_TRUE("addi usa inmediato en ALU", out.ctrl.alu_usa_inm);
    ASSERT_EQ_INT("reg_destino addi = rt", R_T3, out.ctrl.reg_destino);

    /* lw activa mem_read y mem_to_reg */
    uint32_t instr_lw = (OP_LW<<26)|(R_ZERO<<21)|(R_T4<<16)|(0);
    etapa_id(instr_lw, regfile, &out);
    ASSERT_TRUE("lw activa mem_read", out.ctrl.mem_read);
    ASSERT_TRUE("lw activa mem_to_reg", out.ctrl.mem_to_reg);

    /* sw NO escribe registro */
    uint32_t instr_sw = (OP_SW<<26)|(R_ZERO<<21)|(R_T4<<16)|(0);
    etapa_id(instr_sw, regfile, &out);
    ASSERT_TRUE("sw no activa reg_write", !out.ctrl.reg_write);
    ASSERT_TRUE("sw activa mem_write", out.ctrl.mem_write);

    /* jal: reg_destino debe ser $ra y activa link */
    uint32_t instr_jal = (OP_JAL<<26)|(0x100);
    etapa_id(instr_jal, regfile, &out);
    ASSERT_TRUE("jal activa link", out.ctrl.link);
    ASSERT_EQ_INT("jal escribe en $ra", R_RA, out.ctrl.reg_destino);

    RESUMEN_TESTS("test_etapa_id");
}
