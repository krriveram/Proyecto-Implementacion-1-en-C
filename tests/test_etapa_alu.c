/* ===========================================================================
 * test_etapa_alu.c -- Unit tests de la ETAPA 3 (ALU / Execute)
 * ---------------------------------------------------------------------------
 * Que se verifica:
 *   - las 7 operaciones de la ALU y la bandera Z;
 *   - aritmetica modulo 2^32 en el desbordamiento (sin comportamiento
 *     indefinido);
 *   - muxes ALUSrc, RegDst y de cortocircuito, y el sumador de destino de
 *     salto con desplazamientos negativos;
 *   - decision de salto para beq, bne, j y jr;
 *   - calculo de la direccion efectiva de lw y sw;
 *   - que el dato de sw tambien pase por el cortocircuito;
 *   - que una burbuja no produzca ningun efecto.
 * ===========================================================================
 */
#include <string.h>

#include "mips.h"
#include "test_util.h"

/* Construye un registro ID/EX ya decodificado, como el que entregaria ID */
static id_ex_t decodificar(uint32_t instr, int32_t rs_val, int32_t rt_val,
                           uint32_t pc)
{
    id_ex_t e;

    memset(&e, 0, sizeof e);
    e.valido     = true;
    e.pc         = pc;
    e.pc_mas_4   = pc + 4u;
    e.rs         = id_campo_rs(instr);
    e.rt         = id_campo_rt(instr);
    e.rd         = id_campo_rd(instr);
    e.rs_val     = rs_val;
    e.rt_val     = rt_val;
    e.imm        = id_extension_signo(id_campo_imm16(instr));
    e.pc_salto_j = id_destino_j(pc + 4u, id_campo_addr26(instr));
    id_unidad_control(id_campo_opcode(instr), id_campo_funct(instr), &e.ctrl);
    return e;
}

static alu_in_t entrada(id_ex_t e)
{
    alu_in_t in;

    memset(&in, 0, sizeof in);
    in.id_ex = e;
    return in;
}

static void probar_alu(void)
{
    bool cero = false;

    CASO("operaciones de la ALU");

    VERIFICAR_EQ(alu_ejecutar(10,  5, ALU_ADD, &cero),  15, "add");
    VERIFICAR_EQ(alu_ejecutar(10,  5, ALU_SUB, &cero),   5, "sub");
    VERIFICAR_EQ(alu_ejecutar(-4,  9, ALU_ADD, &cero),   5, "add con negativo");
    VERIFICAR_EQ(alu_ejecutar(0x0F, 0x35, ALU_AND, &cero), 0x05, "and");
    VERIFICAR_EQ(alu_ejecutar(0x0F, 0x35, ALU_OR,  &cero), 0x3F, "or");
    VERIFICAR_EQ(alu_ejecutar(0x0F, 0x35, ALU_XOR, &cero), 0x3A, "xor");
    VERIFICAR_EQ(alu_ejecutar(0, 0, ALU_NOR, &cero), -1, "nor de ceros da todos unos");
    VERIFICAR_EQ(alu_ejecutar(0x0F, 0x35, ALU_NOR, &cero), ~(0x0F | 0x35), "nor");
    VERIFICAR_EQ(alu_ejecutar(0x1234, 0x9999, ALU_PASA_A, &cero), 0x1234, "pasa A (jr)");
    VERIFICAR_EQ(alu_ejecutar(7, 3, ALU_NOP, &cero), 0, "ALU_NOP no calcula nada");

    CASO("bandera Z");
    (void)alu_ejecutar(5, 5, ALU_SUB, &cero);
    VERIFICAR(cero,  "5 - 5 activa la bandera Z");
    (void)alu_ejecutar(5, 4, ALU_SUB, &cero);
    VERIFICAR(!cero, "5 - 4 no activa la bandera Z");

    CASO("desbordamiento");
    VERIFICAR_EQ(alu_ejecutar(2147483647, 1, ALU_ADD, &cero), (int32_t)-2147483647 - 1,
                 "INT_MAX + 1 envuelve a INT_MIN (modulo 2^32)");
}

static void probar_subfunciones(void)
{
    CASO("muxes y sumador de salto");

    VERIFICAR_EQ(alu_mux_src(7, 99, false), 7,  "ALUSrc=0 elige el registro rt");
    VERIFICAR_EQ(alu_mux_src(7, 99, true),  99, "ALUSrc=1 elige el inmediato");

    VERIFICAR_EQ(alu_mux_reg_dst(R_T1, R_T2, false), R_T1, "RegDst=0 -> rt (tipo I)");
    VERIFICAR_EQ(alu_mux_reg_dst(R_T1, R_T2, true),  R_T2, "RegDst=1 -> rd (tipo R)");

    VERIFICAR_EQ(alu_sumador_salto(0x20,  4), 0x30, "destino hacia adelante");
    VERIFICAR_EQ(alu_sumador_salto(0x1C, -4), 0x0C, "destino hacia atras (bucle)");

    VERIFICAR_EQ(alu_mux_cortocircuito(1, 2, 3, FWD_NINGUNO), 1, "sin cortocircuito");
    VERIFICAR_EQ(alu_mux_cortocircuito(1, 2, 3, FWD_EX_MEM),  2, "cortocircuito EX/MEM");
    VERIFICAR_EQ(alu_mux_cortocircuito(1, 2, 3, FWD_MEM_WB),  3, "cortocircuito MEM/WB");
}

static void probar_operaciones_completas(void)
{
    alu_in_t  in;
    alu_out_t out;

    CASO("add en la etapa completa");
    in = entrada(decodificar(cod_r(FUNCT_ADD, R_T0, R_T1, R_T2), 10, 5, 0));
    etapa_alu(&in, &out);
    VERIFICAR_EQ(out.ex_mem.alu_res, 15,   "10 + 5");
    VERIFICAR_EQ(out.ex_mem.reg_dst, R_T2, "destino rd por el mux RegDst");
    VERIFICAR   (out.ex_mem.ctrl.reg_write, "add escribe en el banco");
    VERIFICAR   (!out.pc_src,               "add no altera el PC");

    CASO("sub, and, or, xor, nor en la etapa completa");
    in = entrada(decodificar(cod_r(FUNCT_SUB, R_T0, R_T1, R_T2), 10, 5, 0));
    etapa_alu(&in, &out); VERIFICAR_EQ(out.ex_mem.alu_res, 5, "10 - 5");
    in = entrada(decodificar(cod_r(FUNCT_AND, R_T0, R_T1, R_T2), 0xF0, 0x3C, 0));
    etapa_alu(&in, &out); VERIFICAR_EQ(out.ex_mem.alu_res, 0x30, "0xF0 and 0x3C");
    in = entrada(decodificar(cod_r(FUNCT_OR,  R_T0, R_T1, R_T2), 0xF0, 0x3C, 0));
    etapa_alu(&in, &out); VERIFICAR_EQ(out.ex_mem.alu_res, 0xFC, "0xF0 or 0x3C");
    in = entrada(decodificar(cod_r(FUNCT_XOR, R_T0, R_T1, R_T2), 0xF0, 0x3C, 0));
    etapa_alu(&in, &out); VERIFICAR_EQ(out.ex_mem.alu_res, 0xCC, "0xF0 xor 0x3C");
    in = entrada(decodificar(cod_r(FUNCT_NOR, R_T0, R_T1, R_T2), 0xF0, 0x3C, 0));
    etapa_alu(&in, &out); VERIFICAR_EQ(out.ex_mem.alu_res, ~(0xF0 | 0x3C), "0xF0 nor 0x3C");

    CASO("addi con inmediato negativo");
    in = entrada(decodificar(cod_i(OP_ADDI, R_T0, R_T1, -6), 10, 0, 0));
    etapa_alu(&in, &out);
    VERIFICAR_EQ(out.ex_mem.alu_res, 4,    "10 + (-6)");
    VERIFICAR_EQ(out.ex_mem.reg_dst, R_T1, "destino rt por el mux RegDst");

    CASO("direccion efectiva de lw y sw");
    in = entrada(decodificar(cod_i(OP_LW, R_T1, R_T3, 8), 16, 0, 0));
    etapa_alu(&in, &out);
    VERIFICAR_EQ(out.ex_mem.alu_res, 24,   "lw: base 16 + offset 8");
    VERIFICAR_EQ(out.ex_mem.reg_dst, R_T3, "lw escribe en rt");

    in = entrada(decodificar(cod_i(OP_SW, R_T1, R_T3, -4), 16, 77, 0));
    etapa_alu(&in, &out);
    VERIFICAR_EQ(out.ex_mem.alu_res, 12, "sw: base 16 + offset -4");
    VERIFICAR_EQ(out.ex_mem.rt_val,  77, "sw lleva el dato a escribir");
    VERIFICAR   (out.ex_mem.ctrl.mem_write, "sw activa la escritura de memoria");
}

static void probar_saltos(void)
{
    alu_in_t  in;
    alu_out_t out;

    CASO("beq tomado");
    /* beq $t0, $t1, +2  en PC=0x10 -> destino 0x14 + 8 = 0x1C */
    in = entrada(decodificar(cod_i(OP_BEQ, R_T0, R_T1, 2), 7, 7, 0x10));
    etapa_alu(&in, &out);
    VERIFICAR   (out.cero,                "rs - rt == 0");
    VERIFICAR   (out.pc_src,              "beq con operandos iguales salta");
    VERIFICAR_EQ(out.pc_objetivo, 0x1C,   "destino = PC+4 + imm*4");

    CASO("beq no tomado");
    in = entrada(decodificar(cod_i(OP_BEQ, R_T0, R_T1, 2), 7, 8, 0x10));
    etapa_alu(&in, &out);
    VERIFICAR(!out.pc_src, "beq con operandos distintos no salta");

    CASO("bne tomado y no tomado");
    in = entrada(decodificar(cod_i(OP_BNE, R_T0, R_T1, -4), 7, 8, 0x18));
    etapa_alu(&in, &out);
    VERIFICAR   (out.pc_src,            "bne con operandos distintos salta");
    VERIFICAR_EQ(out.pc_objetivo, 0x0C, "destino hacia atras (bucle)");

    in = entrada(decodificar(cod_i(OP_BNE, R_T0, R_T1, -4), 7, 7, 0x18));
    etapa_alu(&in, &out);
    VERIFICAR(!out.pc_src, "bne con operandos iguales no salta");

    CASO("j");
    in = entrada(decodificar(cod_j(OP_J, 0x40), 0, 0, 0x04));
    etapa_alu(&in, &out);
    VERIFICAR   (out.pc_src,            "j siempre salta");
    VERIFICAR_EQ(out.pc_objetivo, 0x40, "destino calculado en ID");
    VERIFICAR   (!out.ex_mem.ctrl.reg_write, "j no escribe registros");

    CASO("jr");
    in = entrada(decodificar(cod_jr(R_RA), 0x30, 0, 0x14));
    etapa_alu(&in, &out);
    VERIFICAR   (out.pc_src,            "jr siempre salta");
    VERIFICAR_EQ(out.pc_objetivo, 0x30, "destino = contenido de rs");
    VERIFICAR   (!out.ex_mem.ctrl.reg_write, "jr no escribe registros");
}

static void probar_cortocircuito(void)
{
    alu_in_t  in;
    alu_out_t out;

    CASO("cortocircuito del operando A");
    /* El valor leido del banco es obsoleto (0); el correcto llega desde EX/MEM */
    in = entrada(decodificar(cod_r(FUNCT_ADD, R_T0, R_T1, R_T2), 0, 5, 0));
    in.fwd_a       = FWD_EX_MEM;
    in.dato_ex_mem = 40;
    etapa_alu(&in, &out);
    VERIFICAR_EQ(out.ex_mem.alu_res, 45, "usa el dato cortocircuitado desde EX/MEM");

    CASO("cortocircuito del operando B");
    in = entrada(decodificar(cod_r(FUNCT_ADD, R_T0, R_T1, R_T2), 5, 0, 0));
    in.fwd_b       = FWD_MEM_WB;
    in.dato_mem_wb = 60;
    etapa_alu(&in, &out);
    VERIFICAR_EQ(out.ex_mem.alu_res, 65, "usa el dato cortocircuitado desde MEM/WB");

    CASO("cortocircuito del dato de sw");
    /* sw $t3, 0($t1): el dato a guardar tambien debe cortocircuitarse */
    in = entrada(decodificar(cod_i(OP_SW, R_T1, R_T3, 0), 8, 0, 0));
    in.fwd_b       = FWD_EX_MEM;
    in.dato_ex_mem = 123;
    etapa_alu(&in, &out);
    VERIFICAR_EQ(out.ex_mem.rt_val,  123, "el dato de sw viene del cortocircuito");
    VERIFICAR_EQ(out.ex_mem.alu_res, 8,   "la direccion no se ve afectada");
}

static void probar_burbuja(void)
{
    alu_in_t  in;
    alu_out_t out;
    id_ex_t   e = decodificar(cod_i(OP_BEQ, R_T0, R_T0, 4), 3, 3, 0);

    CASO("burbuja en EX");
    e.valido = false;
    in = entrada(e);
    etapa_alu(&in, &out);

    VERIFICAR(!out.ex_mem.valido, "no produce resultado");
    VERIFICAR(!out.pc_src,        "una burbuja nunca desvia el PC");
}

int main(void)
{
    printf("== Unit tests: ETAPA 3 -- ALU (Execute) ==\n");

    probar_alu();
    probar_subfunciones();
    probar_operaciones_completas();
    probar_saltos();
    probar_cortocircuito();
    probar_burbuja();

    return RESUMEN("ETAPA 3 (ALU)");
}
