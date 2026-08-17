/* ===========================================================================
 * etapa_mem.c -- ETAPA 4: MEMORY
 * ---------------------------------------------------------------------------
 * Responsabilidades:
 *   1. lw : leer la palabra de la memoria de datos en la direccion calculada
 *           por la ALU.
 *   2. sw : escribir el valor de rt en esa direccion.
 *   3. Para el resto de instrucciones, dejar pasar el resultado de la ALU
 *      hacia el registro MEM/WB sin tocar la memoria.
 *   4. Validar la direccion (alineacion y rango) ANTES de tocar la memoria.
 *
 * Entradas (mem_in_t) : registro EX/MEM, puntero a la memoria de datos y su
 *                       tamano en palabras
 * Salidas  (mem_out_t): registro MEM/WB, err
 *                       (la escritura de sw es un efecto lateral sobre dmem)
 * ===========================================================================
 */
#include <string.h>

#include "mips.h"

/* --- Subfuncion: validacion y traduccion de direccion --------------------
 * La memoria se indexa por palabras; la direccion que produce la ALU es de
 * byte. Se rechaza toda direccion desalineada o fuera del rango fisico, que
 * son las dos entradas "no esperadas" que puede recibir esta etapa.
 */
bool mem_dir_valida(uint32_t dir, uint32_t palabras, uint32_t *indice,
                    mips_err_t *err)
{
    if (err != NULL) {
        *err = MIPS_OK;
    }
    if ((dir & 0x3u) != 0u) {
        if (err != NULL) *err = MIPS_ERR_DIR_ALINEACION;
        return false;
    }
    if ((dir >> 2) >= palabras) {
        if (err != NULL) *err = MIPS_ERR_DIR_RANGO;
        return false;
    }
    if (indice != NULL) {
        *indice = dir >> 2;
    }
    return true;
}

/* --- Funcion principal de la etapa -------------------------------------- */
void etapa_mem(const mem_in_t *in, mem_out_t *out)
{
    const ex_mem_t *e = &in->ex_mem;
    uint32_t        dir;
    uint32_t        indice = 0;
    mips_err_t      err    = MIPS_OK;

    memset(out, 0, sizeof *out);

    if (!e->valido) {
        return;   /* burbuja: la memoria no se toca */
    }

    /* El resultado de la ALU siempre viaja a WB (lo necesitan add, addi, ...) */
    out->mem_wb.valido  = true;
    out->mem_wb.pc      = e->pc;
    out->mem_wb.alu_res = e->alu_res;
    out->mem_wb.reg_dst = e->reg_dst;
    out->mem_wb.ctrl    = e->ctrl;

    if (!e->ctrl.mem_read && !e->ctrl.mem_write) {
        return;   /* instruccion que no accede a memoria */
    }

    if (in->dmem == NULL) {
        out->err = MIPS_ERR_DIR_RANGO;
        return;
    }

    dir = (uint32_t)e->alu_res;
    if (!mem_dir_valida(dir, in->dmem_palabras, &indice, &err)) {
        /* Direccion invalida: se reporta el error y NO se escribe nada. */
        out->err = err;
        return;
    }

    if (e->ctrl.mem_write) {
        in->dmem[indice] = e->rt_val;
    }
    if (e->ctrl.mem_read) {
        out->mem_wb.mem_dato = in->dmem[indice];
    }
}
