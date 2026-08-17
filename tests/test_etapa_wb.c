#include "../include/mips.h"
#include "test_common.h"

int main(void) {
    int32_t regfile[NUM_REGS] = {0};
    senales_control_t ctrl = {0};

    /* resultado de ALU (tipo R / addi) */
    ctrl.reg_write = true;
    etapa_wb(&ctrl, R_T2, 15, 999, 0, regfile);
    ASSERT_EQ_INT("WB escribe resultado_alu", 15, regfile[R_T2]);

    /* resultado de memoria (lw) */
    ctrl.mem_to_reg = true;
    etapa_wb(&ctrl, R_T3, 15, 77, 0, regfile);
    ASSERT_EQ_INT("WB escribe dato_memoria en lw", 77, regfile[R_T3]);

    /* jal: escribe pc_siguiente */
    ctrl = (senales_control_t){0};
    ctrl.reg_write = true;
    ctrl.link = true;
    etapa_wb(&ctrl, R_RA, 0, 0, 0x40, regfile);
    ASSERT_EQ_INT("WB jal escribe pc_siguiente en $ra", 0x40, regfile[R_RA]);

    /* $zero nunca se escribe */
    ctrl = (senales_control_t){0};
    ctrl.reg_write = true;
    regfile[R_ZERO] = 0;
    etapa_wb(&ctrl, R_ZERO, 12345, 0, 0, regfile);
    ASSERT_EQ_INT("$zero permanece en 0", 0, regfile[R_ZERO]);

    /* reg_write=false no debe modificar nada */
    ctrl = (senales_control_t){0};
    regfile[R_T5] = 111;
    etapa_wb(&ctrl, R_T5, 999, 0, 0, regfile);
    ASSERT_EQ_INT("sin reg_write no se modifica", 111, regfile[R_T5]);

    RESUMEN_TESTS("test_etapa_wb");
}
