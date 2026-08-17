/* etapa_if.c -- Etapa 1: Instruction Fetch */
#include "mips.h"

bool etapa_if(uint32_t pc_actual, const uint32_t imem[IMEM_WORDS], if_out_t *out) {
    if (out == NULL || imem == NULL) return false;

    uint32_t indice = pc_actual / 4;
    if (indice >= IMEM_WORDS) {
        /* PC fuera de rango: tratamos como fin de programa */
        out->pc_actual = pc_actual;
        out->instr = NOP_INSTR;
        out->pc_siguiente = pc_actual;
        return false;
    }

    out->pc_actual = pc_actual;
    out->instr = imem[indice];
    out->pc_siguiente = pc_actual + 4;
    return true;
}
