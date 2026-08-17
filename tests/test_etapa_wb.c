/* ===========================================================================
 * test_etapa_wb.c -- Unit tests de la ETAPA 5 (Write Back)
 * ---------------------------------------------------------------------------
 * Que se verifica:
 *   - el mux MemToReg (dato de memoria para lw, resultado de la ALU para el
 *     resto);
 *   - que solo escriban las instrucciones con reg_write;
 *   - que $0 nunca se escriba;
 *   - que una burbuja no genere ninguna escritura.
 * ===========================================================================
 */
#include <string.h>

#include "mips.h"
#include "test_util.h"

/* Construye el registro MEM/WB que llega a la etapa */
static wb_in_t entrada(uint32_t instr, int32_t alu_res, int32_t mem_dato,
                       uint8_t reg_dst)
{
    wb_in_t in;

    memset(&in, 0, sizeof in);
    in.mem_wb.valido   = true;
    in.mem_wb.pc       = 0x30;
    in.mem_wb.alu_res  = alu_res;
    in.mem_wb.mem_dato = mem_dato;
    in.mem_wb.reg_dst  = reg_dst;
    id_unidad_control(id_campo_opcode(instr), id_campo_funct(instr), &in.mem_wb.ctrl);
    return in;
}

static void probar_mux(void)
{
    CASO("mux MemToReg");

    VERIFICAR_EQ(wb_mux_mem_a_reg(11, 22, false), 11, "MemToReg=0 -> resultado de la ALU");
    VERIFICAR_EQ(wb_mux_mem_a_reg(11, 22, true),  22, "MemToReg=1 -> dato de memoria");
}

static void probar_escrituras(void)
{
    wb_in_t  in;
    wb_out_t out;

    CASO("escritura del resultado de la ALU");
    in = entrada(cod_r(FUNCT_ADD, R_T0, R_T1, R_T2), 15, 999, R_T2);
    etapa_wb(&in, &out);
    VERIFICAR   (out.reg_write,        "add escribe en el banco");
    VERIFICAR_EQ(out.reg_dst, R_T2,    "registro destino rd");
    VERIFICAR_EQ(out.dato,    15,      "escribe el resultado de la ALU, no el de memoria");

    CASO("escritura del dato de memoria (lw)");
    in = entrada(cod_i(OP_LW, R_T1, R_T3, 0), 24, 1234, R_T3);
    etapa_wb(&in, &out);
    VERIFICAR   (out.reg_write,        "lw escribe en el banco");
    VERIFICAR_EQ(out.reg_dst, R_T3,    "registro destino rt");
    VERIFICAR_EQ(out.dato,    1234,    "escribe el dato leido de memoria");
}

static void probar_no_escrituras(void)
{
    wb_in_t  in;
    wb_out_t out;

    CASO("instrucciones que no escriben registros");

    in = entrada(cod_i(OP_SW, R_T1, R_T3, 0), 24, 0, 0);
    etapa_wb(&in, &out);
    VERIFICAR(!out.reg_write, "sw no escribe registros");

    in = entrada(cod_i(OP_BEQ, R_T0, R_T1, 4), 0, 0, 0);
    etapa_wb(&in, &out);
    VERIFICAR(!out.reg_write, "beq no escribe registros");

    in = entrada(cod_jr(R_RA), 0x30, 0, 0);
    etapa_wb(&in, &out);
    VERIFICAR(!out.reg_write, "jr no escribe registros");

    CASO("proteccion de $0");
    in = entrada(cod_i(OP_ADDI, R_T0, R_ZERO, 5), 5, 0, R_ZERO);
    etapa_wb(&in, &out);
    VERIFICAR(!out.reg_write, "$0 es de solo lectura aunque reg_write este activo");

    CASO("burbuja en WB");
    in = entrada(cod_r(FUNCT_ADD, R_T0, R_T1, R_T2), 15, 0, R_T2);
    in.mem_wb.valido = false;
    etapa_wb(&in, &out);
    VERIFICAR(!out.reg_write, "una burbuja no escribe nada");
}

int main(void)
{
    printf("== Unit tests: ETAPA 5 -- Write Back ==\n");

    probar_mux();
    probar_escrituras();
    probar_no_escrituras();

    return RESUMEN("ETAPA 5 (WB)");
}
