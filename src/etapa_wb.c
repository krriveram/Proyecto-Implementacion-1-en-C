/* etapa_wb.c -- Etapa 5: Write Back */
#include "mips.h"

bool etapa_wb(const senales_control_t *ctrl, uint8_t reg_destino,
              int32_t resultado_alu, int32_t dato_memoria,
              uint32_t pc_siguiente, int32_t regfile[NUM_REGS]) {
    if (ctrl == NULL || regfile == NULL) return false;
    if (!ctrl->reg_write) return true;      /* nada que escribir */
    if (reg_destino == R_ZERO) return true; /* $zero nunca se escribe */

    if (ctrl->link)          regfile[reg_destino] = (int32_t)pc_siguiente; /* jal */
    else if (ctrl->mem_to_reg) regfile[reg_destino] = dato_memoria;         /* lw  */
    else                        regfile[reg_destino] = resultado_alu;      /* R/addi */

    return true;
}
