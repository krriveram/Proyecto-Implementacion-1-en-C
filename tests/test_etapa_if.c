/* ===========================================================================
 * test_etapa_if.c -- Unit tests de la ETAPA 1 (Instruction Fetch + PC)
 * ---------------------------------------------------------------------------
 * Que se verifica:
 *   - subfunciones: sumador PC+4, mux del PC, lectura de la memoria de
 *     instrucciones (incluidas direcciones invalidas);
 *   - busqueda secuencial normal;
 *   - redireccion del PC cuando la etapa ALU reporta un salto tomado;
 *   - congelamiento del PC y del registro IF/ID ante un riesgo lw-uso;
 *   - anulacion de la instruccion capturada (flush) y prioridad flush > stall;
 *   - tratamiento de entradas no esperadas: PC desalineado y fuera de rango.
 * ===========================================================================
 */
#include <string.h>

#include "mips.h"
#include "test_util.h"

/* Memoria de instrucciones de juguete para las pruebas */
static uint32_t imem[8];

static void preparar_imem(void)
{
    memset(imem, 0, sizeof imem);
    imem[0] = cod_i(OP_ADDI, R_ZERO, R_T0, 5);   /* 0x00 */
    imem[1] = cod_i(OP_ADDI, R_ZERO, R_T1, 7);   /* 0x04 */
    imem[2] = cod_r(FUNCT_ADD, R_T0, R_T1, R_T2);/* 0x08 */
    imem[3] = MIPS_NOP;                          /* 0x0C */
}

/* Entrada base: sin saltos, sin riesgos */
static if_in_t entrada_base(uint32_t pc)
{
    if_in_t in;

    memset(&in, 0, sizeof in);
    in.pc            = pc;
    in.imem          = imem;
    in.imem_palabras = (uint32_t)(sizeof imem / sizeof imem[0]);
    return in;
}

static void probar_subfunciones(void)
{
    mips_err_t err = MIPS_OK;

    CASO("subfunciones de IF");

    VERIFICAR_EQ(if_sumador_pc4(0),      4,  "PC+4 desde 0");
    VERIFICAR_EQ(if_sumador_pc4(0x1000), 0x1004, "PC+4 desde 0x1000");

    VERIFICAR_EQ(if_mux_pc(0x10, 0x40, false), 0x10, "mux_pc elige PC+4");
    VERIFICAR_EQ(if_mux_pc(0x10, 0x40, true),  0x40, "mux_pc elige el destino");

    VERIFICAR_EQ(if_imem_leer(imem, 8, 8, &err), imem[2], "lectura de imem[2]");
    VERIFICAR_EQ(err, MIPS_OK, "lectura valida sin error");

    (void)if_imem_leer(imem, 8, 6, &err);
    VERIFICAR_EQ(err, MIPS_ERR_DIR_ALINEACION, "direccion 6 no esta alineada");

    (void)if_imem_leer(imem, 8, 64, &err);
    VERIFICAR_EQ(err, MIPS_ERR_DIR_RANGO, "direccion 64 fuera de una imem de 8 palabras");

    VERIFICAR_EQ(if_imem_leer(NULL, 8, 0, &err), MIPS_NOP, "imem nula devuelve NOP");
}

static void probar_busqueda_secuencial(void)
{
    if_in_t  in  = entrada_base(4);
    if_out_t out;

    CASO("busqueda secuencial");
    etapa_if(&in, &out);

    VERIFICAR_EQ(out.if_id.instr,    imem[1], "captura la instruccion apuntada por el PC");
    VERIFICAR_EQ(out.if_id.pc,       4,       "IF/ID guarda el PC de la instruccion");
    VERIFICAR_EQ(out.if_id.pc_mas_4, 8,       "IF/ID guarda PC+4");
    VERIFICAR   (out.if_id.valido,            "la instruccion es valida");
    VERIFICAR_EQ(out.pc_siguiente,   8,       "el PC avanza a PC+4");
    VERIFICAR   (out.cargar_if_id,            "IF/ID se carga");
    VERIFICAR_EQ(out.err, MIPS_OK,            "sin errores");
}

static void probar_salto_tomado(void)
{
    if_in_t  in  = entrada_base(4);
    if_out_t out;

    CASO("salto tomado (pc_src + flush)");
    in.pc_src      = true;
    in.pc_objetivo = 0x20;
    in.flush       = true;
    etapa_if(&in, &out);

    VERIFICAR_EQ(out.pc_siguiente, 0x20, "el PC toma el destino del salto");
    VERIFICAR   (!out.if_id.valido,      "la instruccion del camino equivocado se anula");
    VERIFICAR   (out.cargar_if_id,       "IF/ID se carga con la burbuja");
}

static void probar_stall(void)
{
    if_in_t  in  = entrada_base(8);
    if_out_t out;

    CASO("congelamiento por riesgo lw-uso");
    in.stall = true;
    etapa_if(&in, &out);

    VERIFICAR_EQ(out.pc_siguiente, 8, "el PC se mantiene para releer la instruccion");
    VERIFICAR   (!out.cargar_if_id,   "IF/ID conserva su valor anterior");
}

static void probar_prioridad_flush_sobre_stall(void)
{
    if_in_t  in  = entrada_base(8);
    if_out_t out;

    CASO("prioridad flush > stall");
    in.stall       = true;
    in.flush       = true;
    in.pc_src      = true;
    in.pc_objetivo = 0x30;
    etapa_if(&in, &out);

    VERIFICAR_EQ(out.pc_siguiente, 0x30, "el salto redirige el PC aunque haya stall");
    VERIFICAR   (!out.if_id.valido,      "la instruccion se anula");
    VERIFICAR   (out.cargar_if_id,       "IF/ID se actualiza con la burbuja");
}

static void probar_nop_y_fin_de_programa(void)
{
    if_in_t  in  = entrada_base(0x0C);   /* imem[3] == NOP */
    if_out_t out;

    CASO("NOP y memoria vacia");
    etapa_if(&in, &out);

    VERIFICAR   (!out.if_id.valido,   "una palabra en 0 se trata como burbuja");
    VERIFICAR_EQ(out.pc_siguiente, 0x10, "aun asi el PC avanza");
    VERIFICAR_EQ(out.err, MIPS_OK,       "leer un NOP no es un error");
}

static void probar_entradas_invalidas(void)
{
    if_in_t  in;
    if_out_t out;

    CASO("PC desalineado");
    in = entrada_base(5);
    etapa_if(&in, &out);
    VERIFICAR_EQ(out.err, MIPS_ERR_DIR_ALINEACION, "PC=5 reporta desalineacion");
    VERIFICAR   (!out.if_id.valido,                "no se propaga una instruccion basura");

    CASO("PC fuera de rango");
    in = entrada_base(0x100);   /* 256 bytes > imem de 8 palabras */
    etapa_if(&in, &out);
    VERIFICAR_EQ(out.err, MIPS_ERR_DIR_RANGO, "PC fuera de la memoria reporta error");
    VERIFICAR   (!out.if_id.valido,           "no se propaga una instruccion basura");
}

int main(void)
{
    printf("== Unit tests: ETAPA 1 -- Instruction Fetch + Program Counter ==\n");

    preparar_imem();
    probar_subfunciones();
    probar_busqueda_secuencial();
    probar_salto_tomado();
    probar_stall();
    probar_prioridad_flush_sobre_stall();
    probar_nop_y_fin_de_programa();
    probar_entradas_invalidas();

    return RESUMEN("ETAPA 1 (IF + PC)");
}
