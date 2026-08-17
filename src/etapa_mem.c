/* etapa_mem.c -- Etapa 4: Memory Access */
#include "mips.h"

bool etapa_mem(const senales_control_t *ctrl, int32_t resultado_alu,
               int32_t valor_rt, int32_t dmem[DMEM_WORDS], mem_out_t *out) {
    if (ctrl == NULL || dmem == NULL || out == NULL) return false;

    out->dato_leido = 0;

    if (ctrl->mem_read || ctrl->mem_write) {
        /* direccion en bytes -> palabra */
        if (resultado_alu < 0) return false;
        uint32_t indice = (uint32_t)resultado_alu / 4;
        if (indice >= DMEM_WORDS) return false;

        if (ctrl->mem_read)  out->dato_leido = dmem[indice];
        if (ctrl->mem_write) dmem[indice] = valor_rt;
    }
    return true;
}
