/* ===========================================================================
 * etapa_wb.c -- ETAPA 5: WRITE BACK
 * ---------------------------------------------------------------------------
 * Responsabilidades:
 *   1. Elegir el dato a escribir (mux MemToReg): el leido de memoria para lw,
 *      o el resultado de la ALU para el resto.
 *   2. Producir la orden de escritura en el banco de registros, respetando
 *      dos reglas:
 *        - solo escriben las instrucciones con reg_write (no lo hacen sw,
 *          beq, bne, j ni jr);
 *        - $0 esta cableado a cero y nunca se escribe.
 *
 * Entradas (wb_in_t) : registro MEM/WB
 * Salidas  (wb_out_t): reg_write, reg_dst[4:0], dato[31:0]
 *
 * La etapa NO escribe directamente en el banco: devuelve la orden y el nucleo
 * la aplica. Asi la funcion es pura y facil de probar, y ademas el mismo dato
 * se puede reutilizar como fuente de cortocircuito hacia la etapa ALU.
 * ===========================================================================
 */
#include <string.h>

#include "mips.h"

/* --- Subfuncion: mux MemToReg ------------------------------------------- */
int32_t wb_mux_mem_a_reg(int32_t alu_res, int32_t mem_dato, bool mem_to_reg)
{
    return mem_to_reg ? mem_dato : alu_res;
}

/* --- Funcion principal de la etapa -------------------------------------- */
void etapa_wb(const wb_in_t *in, wb_out_t *out)
{
    const mem_wb_t *m = &in->mem_wb;

    memset(out, 0, sizeof *out);

    if (!m->valido) {
        return;   /* burbuja: no se escribe nada */
    }

    out->dato      = wb_mux_mem_a_reg(m->alu_res, m->mem_dato, m->ctrl.mem_to_reg);
    out->reg_dst   = m->reg_dst;
    out->reg_write = m->ctrl.reg_write && (m->reg_dst != 0);
}
