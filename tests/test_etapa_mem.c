/* ===========================================================================
 * test_etapa_mem.c -- Unit tests de la ETAPA 4 (Memory)
 * ---------------------------------------------------------------------------
 * Que se verifica:
 *   - validacion y traduccion de direcciones (byte -> palabra);
 *   - escritura de sw y lectura de lw;
 *   - paso limpio del resultado de la ALU para las instrucciones que no
 *     acceden a memoria;
 *   - que una direccion invalida reporte error y NO modifique la memoria;
 *   - que una burbuja deje la memoria intacta.
 * ===========================================================================
 */
#include <string.h>

#include "mips.h"
#include "test_util.h"

#define PALABRAS 16

static int32_t dmem[PALABRAS];

static void preparar_dmem(void)
{
    int i;

    for (i = 0; i < PALABRAS; i++) {
        dmem[i] = 1000 + i;
    }
}

/* Construye la entrada de la etapa a partir de una instruccion decodificada */
static mem_in_t entrada(uint32_t instr, int32_t alu_res, int32_t rt_val,
                        uint8_t reg_dst)
{
    mem_in_t in;

    memset(&in, 0, sizeof in);
    in.ex_mem.valido  = true;
    in.ex_mem.pc      = 0x40;
    in.ex_mem.alu_res = alu_res;
    in.ex_mem.rt_val  = rt_val;
    in.ex_mem.reg_dst = reg_dst;
    id_unidad_control(id_campo_opcode(instr), id_campo_funct(instr), &in.ex_mem.ctrl);
    in.dmem          = dmem;
    in.dmem_palabras = PALABRAS;
    return in;
}

static void probar_validacion_direcciones(void)
{
    uint32_t   indice = 0xFFFFFFFFu;
    mips_err_t err    = MIPS_OK;

    CASO("validacion de direcciones");

    VERIFICAR   (mem_dir_valida(12, PALABRAS, &indice, &err), "12 es una direccion valida");
    VERIFICAR_EQ(indice, 3,        "direccion de byte 12 -> palabra 3");
    VERIFICAR_EQ(err, MIPS_OK,     "sin error");

    VERIFICAR   (!mem_dir_valida(13, PALABRAS, &indice, &err), "13 no esta alineada");
    VERIFICAR_EQ(err, MIPS_ERR_DIR_ALINEACION, "reporta desalineacion");

    VERIFICAR   (!mem_dir_valida(PALABRAS * 4u, PALABRAS, &indice, &err),
                 "la primera direccion fuera del rango se rechaza");
    VERIFICAR_EQ(err, MIPS_ERR_DIR_RANGO, "reporta fuera de rango");

    VERIFICAR   (mem_dir_valida((PALABRAS - 1) * 4u, PALABRAS, &indice, &err),
                 "la ultima direccion valida se acepta");
}

static void probar_lectura_escritura(void)
{
    mem_in_t  in;
    mem_out_t out;

    CASO("sw escribe en memoria");
    preparar_dmem();
    in = entrada(cod_i(OP_SW, R_T1, R_T3, 0), 20, 555, 0);   /* M[20] = 555 */
    etapa_mem(&in, &out);
    VERIFICAR_EQ(dmem[5], 555,        "la palabra 5 recibe el dato");
    VERIFICAR_EQ(dmem[4], 1004,       "las palabras vecinas no se tocan");
    VERIFICAR_EQ(out.err, MIPS_OK,    "sin errores");
    VERIFICAR   (out.mem_wb.valido,   "propaga la instruccion a WB");
    VERIFICAR   (!out.mem_wb.ctrl.reg_write, "sw no escribe registros");

    CASO("lw lee de memoria");
    preparar_dmem();
    in = entrada(cod_i(OP_LW, R_T1, R_T3, 0), 8, 0, R_T3);   /* $t3 = M[8] */
    etapa_mem(&in, &out);
    VERIFICAR_EQ(out.mem_wb.mem_dato, 1002, "entrega el dato de la palabra 2");
    VERIFICAR_EQ(out.mem_wb.reg_dst,  R_T3, "propaga el registro destino");
    VERIFICAR   (out.mem_wb.ctrl.mem_to_reg, "lw marca el dato como proveniente de memoria");

    CASO("instruccion que no accede a memoria");
    preparar_dmem();
    in = entrada(cod_r(FUNCT_ADD, R_T0, R_T1, R_T2), 42, 0, R_T2);
    etapa_mem(&in, &out);
    VERIFICAR_EQ(out.mem_wb.alu_res, 42,   "el resultado de la ALU pasa intacto");
    VERIFICAR_EQ(dmem[0], 1000,            "la memoria no se modifica");
    VERIFICAR_EQ(out.mem_wb.mem_dato, 0,   "no hay dato de memoria");
}

static void probar_errores(void)
{
    mem_in_t  in;
    mem_out_t out;

    CASO("sw fuera de rango");
    preparar_dmem();
    in = entrada(cod_i(OP_SW, R_T1, R_T3, 0), PALABRAS * 4, 999, 0);
    etapa_mem(&in, &out);
    VERIFICAR_EQ(out.err, MIPS_ERR_DIR_RANGO, "reporta fuera de rango");
    VERIFICAR_EQ(dmem[0], 1000,               "no se corrompe la memoria");

    CASO("lw desalineado");
    preparar_dmem();
    in = entrada(cod_i(OP_LW, R_T1, R_T3, 0), 10, 0, R_T3);
    etapa_mem(&in, &out);
    VERIFICAR_EQ(out.err, MIPS_ERR_DIR_ALINEACION, "reporta desalineacion");
    VERIFICAR_EQ(out.mem_wb.mem_dato, 0,           "no entrega un dato basura");

    CASO("direccion negativa");
    preparar_dmem();
    in = entrada(cod_i(OP_LW, R_T1, R_T3, 0), -4, 0, R_T3);
    etapa_mem(&in, &out);
    VERIFICAR_EQ(out.err, MIPS_ERR_DIR_RANGO,
                 "una direccion negativa se interpreta como muy grande y se rechaza");

    CASO("memoria no conectada");
    in = entrada(cod_i(OP_SW, R_T1, R_T3, 0), 0, 1, 0);
    in.dmem = NULL;
    etapa_mem(&in, &out);
    VERIFICAR_EQ(out.err, MIPS_ERR_DIR_RANGO, "puntero nulo se reporta como error");
}

static void probar_burbuja(void)
{
    mem_in_t  in;
    mem_out_t out;

    CASO("burbuja en MEM");
    preparar_dmem();
    in = entrada(cod_i(OP_SW, R_T1, R_T3, 0), 0, 777, 0);
    in.ex_mem.valido = false;
    etapa_mem(&in, &out);

    VERIFICAR   (!out.mem_wb.valido, "no propaga nada a WB");
    VERIFICAR_EQ(dmem[0], 1000,      "no escribe en memoria");
}

int main(void)
{
    printf("== Unit tests: ETAPA 4 -- Memory ==\n");

    probar_validacion_direcciones();
    probar_lectura_escritura();
    probar_errores();
    probar_burbuja();

    return RESUMEN("ETAPA 4 (MEM)");
}
