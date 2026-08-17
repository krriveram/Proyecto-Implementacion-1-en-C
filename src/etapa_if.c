/* ===========================================================================
 * etapa_if.c -- ETAPA 1: INSTRUCTION FETCH + PROGRAM COUNTER
 * ---------------------------------------------------------------------------
 * Responsabilidades:
 *   1. Leer de la memoria de instrucciones la palabra apuntada por el PC.
 *   2. Calcular PC+4 (sumador dedicado).
 *   3. Elegir el PC siguiente entre PC+4 y el destino de un salto (mux_pc),
 *      segun la senal pc_src que produce la etapa 3 (ALU).
 *   4. Atender las senales de riesgo:
 *        - stall: congela el PC y mantiene el registro IF/ID (la instruccion
 *                 se vuelve a leer en el ciclo siguiente).
 *        - flush: la instruccion leida pertenece al camino equivocado de un
 *                 salto tomado, asi que se convierte en burbuja.
 *
 * Entradas  (if_in_t) : pc[31:0], imem, pc_src, pc_objetivo[31:0], stall, flush
 * Salidas   (if_out_t): pc_siguiente[31:0], registro IF/ID, cargar_if_id, err
 *
 * Prioridad de senales: flush > stall. Un salto tomado SIEMPRE debe redirigir
 * el PC; ademas las dos condiciones son mutuamente excluyentes en la practica
 * (el stall solo lo genera un lw, que nunca es una instruccion de salto).
 * ===========================================================================
 */
#include <string.h>

#include "mips.h"

/* --- Subfuncion: sumador dedicado PC+4 ---------------------------------- */
uint32_t if_sumador_pc4(uint32_t pc)
{
    return pc + 4u;
}

/* --- Subfuncion: mux del PC --------------------------------------------- */
uint32_t if_mux_pc(uint32_t pc_mas_4, uint32_t pc_objetivo, bool pc_src)
{
    return pc_src ? pc_objetivo : pc_mas_4;
}

/* --- Subfuncion: lectura de la memoria de instrucciones -----------------
 * La memoria esta organizada en palabras de 32 bits, pero el PC es una
 * direccion de BYTE: hay que dividir para 4. Se validan las dos entradas
 * "no esperadas" tipicas: direccion desalineada y direccion fuera de rango.
 * En cualquier error se devuelve NOP para que el pipeline no propague basura.
 */
uint32_t if_imem_leer(const uint32_t *imem, uint32_t palabras, uint32_t dir,
                      mips_err_t *err)
{
    if (err != NULL) {
        *err = MIPS_OK;
    }
    if (imem == NULL) {
        if (err != NULL) *err = MIPS_ERR_DIR_RANGO;
        return MIPS_NOP;
    }
    if ((dir & 0x3u) != 0u) {
        if (err != NULL) *err = MIPS_ERR_DIR_ALINEACION;
        return MIPS_NOP;
    }
    if ((dir >> 2) >= palabras) {
        if (err != NULL) *err = MIPS_ERR_DIR_RANGO;
        return MIPS_NOP;
    }
    return imem[dir >> 2];
}

/* --- Funcion principal de la etapa -------------------------------------- */
void etapa_if(const if_in_t *in, if_out_t *out)
{
    uint32_t   instr;
    uint32_t   pc_mas_4;
    mips_err_t err = MIPS_OK;

    memset(out, 0, sizeof *out);
    out->cargar_if_id = true;

    /* 1. Busqueda de la instruccion y sumador PC+4 (trabajan en paralelo) */
    instr    = if_imem_leer(in->imem, in->imem_palabras, in->pc, &err);
    pc_mas_4 = if_sumador_pc4(in->pc);
    out->err = err;

    /* 2. Congelamiento por riesgo lw-uso: el PC no avanza y IF/ID se mantiene */
    if (in->stall && !in->flush) {
        out->pc_siguiente = in->pc;
        out->cargar_if_id = false;
        return;
    }

    /* 3. Mux del PC: PC+4 o destino del salto resuelto en la etapa ALU */
    out->pc_siguiente = if_mux_pc(pc_mas_4, in->pc_objetivo, in->pc_src);

    /* 4. Anulacion: la instruccion leida esta en el camino equivocado */
    if (in->flush) {
        return;   /* out->if_id quedo en ceros => burbuja */
    }

    /* 5. Carga del registro de pipeline IF/ID.
     *    Una palabra en 0x00000000 es el NOP canonico de MIPS y tambien lo que
     *    devuelve la memoria mas alla del programa cargado: se trata como
     *    burbuja para que no consuma recursos ni cuente como instruccion. */
    out->if_id.valido   = (instr != MIPS_NOP) && (err == MIPS_OK);
    out->if_id.pc       = in->pc;
    out->if_id.pc_mas_4 = pc_mas_4;
    out->if_id.instr    = instr;
}
