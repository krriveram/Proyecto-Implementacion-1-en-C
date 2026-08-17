/* etapa_alu.c -- Etapa 3: Execute (ALU + calculo de destinos de salto) */
#include "mips.h"

static int32_t calcular_alu(alu_op_t op, int32_t a, int32_t b, uint8_t shamt) {
    switch (op) {
        case ALU_ADD: return a + b;
        case ALU_SUB: return a - b;
        case ALU_AND: return a & b;
        case ALU_OR:  return a | b;
        case ALU_SLT: return (a < b) ? 1 : 0;
        case ALU_SLL: return (int32_t)((uint32_t)b << shamt);
        default:      return 0;
    }
}

bool etapa_ex(const instr_decodificada_t *di, const senales_control_t *ctrl,
              int32_t valor_rs, int32_t valor_rt, uint32_t pc_siguiente,
              ex_out_t *out) {
    if (di == NULL || ctrl == NULL || out == NULL) return false;

    int32_t operando_b = ctrl->alu_usa_inm ? (int32_t)di->inmediato : valor_rt;

    out->resultado_alu = calcular_alu(ctrl->alu_op, valor_rs, operando_b, di->shamt);
    out->zero = (out->resultado_alu == 0);

    /* branch: pc_siguiente + (inmediato << 2) */
    out->pc_branch = pc_siguiente + ((uint32_t)((int32_t)di->inmediato) << 2);

    /* jump: bits altos de pc_siguiente + direccion26 << 2 */
    out->pc_jump = (pc_siguiente & 0xF0000000u) | (di->direccion26 << 2);

    return true;
}
