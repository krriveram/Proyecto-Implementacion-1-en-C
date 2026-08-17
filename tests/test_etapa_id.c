/* ===========================================================================
 * test_etapa_id.c -- Unit tests de la ETAPA 2 (Instruction Decode)
 * ---------------------------------------------------------------------------
 * Que se verifica:
 *   - extraccion de cada campo de la instruccion (opcode, rs, rt, rd, funct,
 *     inmediato de 16 bits, direccion de 26 bits);
 *   - extension de signo, incluyendo los extremos del rango;
 *   - la unidad de control para las 13 instrucciones de la ISA;
 *   - lectura del banco de registros y el caracter de solo lectura de $0;
 *   - calculo del destino de j;
 *   - deteccion de instrucciones fuera de la ISA (opcode y funct);
 *   - inyeccion de burbujas por stall, flush o entrada invalida.
 * ===========================================================================
 */
#include <string.h>

#include "mips.h"
#include "test_util.h"

static int32_t regs[MIPS_NUM_REGS];

static void preparar_regs(void)
{
    memset(regs, 0, sizeof regs);
    regs[R_T0] = 10;
    regs[R_T1] = -3;
    regs[R_T2] = 0x7FFFFFFF;
}

/* Construye la entrada de la etapa a partir de una instruccion y su PC */
static id_in_t entrada(uint32_t instr, uint32_t pc)
{
    id_in_t in;

    memset(&in, 0, sizeof in);
    in.if_id.valido   = true;
    in.if_id.instr    = instr;
    in.if_id.pc       = pc;
    in.if_id.pc_mas_4 = pc + 4u;
    in.regs           = regs;
    return in;
}

static void probar_campos(void)
{
    /* add $t2, $t0, $t1  ->  op=0 rs=8 rt=9 rd=10 funct=0x20 */
    uint32_t r = cod_r(FUNCT_ADD, R_T0, R_T1, R_T2);
    /* lw $t3, -8($t1)    ->  op=0x23 rs=9 rt=11 imm=0xFFF8 */
    uint32_t i = cod_i(OP_LW, R_T1, R_T3, -8);
    uint32_t j = cod_j(OP_J, 0x40);

    CASO("extraccion de campos");

    VERIFICAR_EQ(id_campo_opcode(r), OP_RTIPO,  "opcode de tipo R es 0");
    VERIFICAR_EQ(id_campo_rs(r),     R_T0,      "campo rs [25:21]");
    VERIFICAR_EQ(id_campo_rt(r),     R_T1,      "campo rt [20:16]");
    VERIFICAR_EQ(id_campo_rd(r),     R_T2,      "campo rd [15:11]");
    VERIFICAR_EQ(id_campo_funct(r),  FUNCT_ADD, "campo funct [5:0]");

    VERIFICAR_EQ(id_campo_opcode(i), OP_LW,     "opcode de lw");
    VERIFICAR_EQ(id_campo_rs(i),     R_T1,      "rs de lw (registro base)");
    VERIFICAR_EQ(id_campo_rt(i),     R_T3,      "rt de lw (registro destino)");
    VERIFICAR_EQ(id_campo_imm16(i),  0xFFF8u,   "inmediato de 16 bits sin extender");

    VERIFICAR_EQ(id_campo_opcode(j),  OP_J,     "opcode de j");
    VERIFICAR_EQ(id_campo_addr26(j),  0x40 >> 2,"direccion de 26 bits (palabras)");
}

static void probar_extension_signo(void)
{
    CASO("extension de signo");

    VERIFICAR_EQ(id_extension_signo(0x0000), 0,       "cero");
    VERIFICAR_EQ(id_extension_signo(0x0005), 5,       "positivo pequeno");
    VERIFICAR_EQ(id_extension_signo(0x7FFF), 32767,   "maximo positivo");
    VERIFICAR_EQ(id_extension_signo(0xFFFF), -1,      "menos uno");
    VERIFICAR_EQ(id_extension_signo(0x8000), -32768,  "minimo negativo");
    VERIFICAR_EQ(id_extension_signo(0xFFFC), -4,      "desplazamiento negativo de bne");
}

static void probar_unidad_control(void)
{
    ctrl_t c;

    CASO("unidad de control: tipo R");
    VERIFICAR(id_unidad_control(OP_RTIPO, FUNCT_ADD, &c), "add reconocida");
    VERIFICAR(c.reg_write && c.reg_dst,                   "add escribe en rd");
    VERIFICAR(!c.alu_src,                                 "add usa el registro rt");
    VERIFICAR_EQ(c.alu_op, ALU_ADD,                       "add -> ALU_ADD");
    VERIFICAR(!c.mem_read && !c.mem_write,                "add no toca memoria");

    VERIFICAR(id_unidad_control(OP_RTIPO, FUNCT_SUB, &c) && c.alu_op == ALU_SUB, "sub -> ALU_SUB");
    VERIFICAR(id_unidad_control(OP_RTIPO, FUNCT_AND, &c) && c.alu_op == ALU_AND, "and -> ALU_AND");
    VERIFICAR(id_unidad_control(OP_RTIPO, FUNCT_OR,  &c) && c.alu_op == ALU_OR,  "or  -> ALU_OR");
    VERIFICAR(id_unidad_control(OP_RTIPO, FUNCT_XOR, &c) && c.alu_op == ALU_XOR, "xor -> ALU_XOR");
    VERIFICAR(id_unidad_control(OP_RTIPO, FUNCT_NOR, &c) && c.alu_op == ALU_NOR, "nor -> ALU_NOR");

    CASO("unidad de control: jr");
    VERIFICAR(id_unidad_control(OP_RTIPO, FUNCT_JR, &c), "jr reconocida");
    VERIFICAR(c.jump_reg,                                "jr activa jump_reg");
    VERIFICAR(!c.reg_write,                              "jr no escribe registros");
    VERIFICAR_EQ(c.alu_op, ALU_PASA_A,                   "jr deja pasar rs por la ALU");

    CASO("unidad de control: tipo I");
    VERIFICAR(id_unidad_control(OP_ADDI, 0, &c), "addi reconocida");
    VERIFICAR(c.alu_src && c.reg_write && !c.reg_dst, "addi usa inmediato y escribe en rt");

    VERIFICAR(id_unidad_control(OP_LW, 0, &c), "lw reconocida");
    VERIFICAR(c.alu_src && c.mem_read && c.mem_to_reg && c.reg_write, "senales de lw");
    VERIFICAR_EQ(c.alu_op, ALU_ADD, "lw calcula base + offset con la ALU");

    VERIFICAR(id_unidad_control(OP_SW, 0, &c), "sw reconocida");
    VERIFICAR(c.alu_src && c.mem_write && !c.reg_write, "sw escribe memoria y no registros");

    VERIFICAR(id_unidad_control(OP_BEQ, 0, &c), "beq reconocida");
    VERIFICAR(c.branch_eq && !c.reg_write, "beq no escribe registros");
    VERIFICAR_EQ(c.alu_op, ALU_SUB, "beq compara restando");

    VERIFICAR(id_unidad_control(OP_BNE, 0, &c), "bne reconocida");
    VERIFICAR(c.branch_ne && !c.reg_write, "bne no escribe registros");

    CASO("unidad de control: tipo J");
    VERIFICAR(id_unidad_control(OP_J, 0, &c), "j reconocida");
    VERIFICAR(c.jump && !c.reg_write && !c.mem_write, "j solo cambia el PC");

    CASO("unidad de control: entradas no soportadas");
    VERIFICAR(!id_unidad_control(0x3F, 0, &c),          "opcode inexistente se rechaza");
    VERIFICAR(!id_unidad_control(OP_RTIPO, 0x2A, &c),   "funct fuera de la ISA (slt) se rechaza");
}

static void probar_banco_y_destino_j(void)
{
    int32_t d1 = 123, d2 = 456;

    CASO("banco de registros y destino de j");

    id_banco_leer(regs, R_T0, R_T1, &d1, &d2);
    VERIFICAR_EQ(d1, 10, "lectura de $t0");
    VERIFICAR_EQ(d2, -3, "lectura de $t1");

    id_banco_leer(regs, R_ZERO, R_ZERO, &d1, &d2);
    VERIFICAR_EQ(d1, 0, "$0 siempre vale cero");

    VERIFICAR_EQ(id_destino_j(0x00000008u, 0x40 >> 2), 0x40,
                 "destino de j = PC+4[31:28] : addr26 << 2");
    VERIFICAR_EQ(id_destino_j(0x10000008u, 0x40 >> 2), 0x10000040u,
                 "los 4 bits altos se heredan de PC+4");
}

static void probar_registros_usados(void)
{
    bool rs = false, rt = false;

    CASO("registros leidos por cada instruccion");

    id_registros_usados(cod_r(FUNCT_ADD, R_T0, R_T1, R_T2), &rs, &rt);
    VERIFICAR(rs && rt, "add lee rs y rt");

    id_registros_usados(cod_i(OP_ADDI, R_T0, R_T1, 4), &rs, &rt);
    VERIFICAR(rs && !rt, "addi solo lee rs");

    id_registros_usados(cod_i(OP_LW, R_T0, R_T1, 4), &rs, &rt);
    VERIFICAR(rs && !rt, "lw solo lee rs (rt es el destino)");

    id_registros_usados(cod_i(OP_SW, R_T0, R_T1, 4), &rs, &rt);
    VERIFICAR(rs && rt, "sw lee la base y el dato");

    id_registros_usados(cod_i(OP_BEQ, R_T0, R_T1, 4), &rs, &rt);
    VERIFICAR(rs && rt, "beq lee ambos operandos");

    id_registros_usados(cod_jr(R_RA), &rs, &rt);
    VERIFICAR(rs && !rt, "jr solo lee rs");

    id_registros_usados(cod_j(OP_J, 0x20), &rs, &rt);
    VERIFICAR(!rs && !rt, "j no lee registros");
}

static void probar_decodificacion_completa(void)
{
    id_in_t  in;
    id_out_t out;

    CASO("decodificacion de add");
    in = entrada(cod_r(FUNCT_ADD, R_T0, R_T1, R_T2), 0x10);
    etapa_id(&in, &out);
    VERIFICAR   (out.id_ex.valido,          "produce una instruccion valida");
    VERIFICAR_EQ(out.id_ex.rs,     R_T0,    "indice rs");
    VERIFICAR_EQ(out.id_ex.rt,     R_T1,    "indice rt");
    VERIFICAR_EQ(out.id_ex.rd,     R_T2,    "indice rd");
    VERIFICAR_EQ(out.id_ex.rs_val, 10,      "valor leido de $t0");
    VERIFICAR_EQ(out.id_ex.rt_val, -3,      "valor leido de $t1");
    VERIFICAR_EQ(out.id_ex.pc,     0x10,    "propaga el PC");
    VERIFICAR_EQ(out.id_ex.pc_mas_4, 0x14,  "propaga PC+4");
    VERIFICAR_EQ(out.err, MIPS_OK,          "sin errores");

    CASO("decodificacion de addi con inmediato negativo");
    in = entrada(cod_i(OP_ADDI, R_T0, R_T3, -20), 0);
    etapa_id(&in, &out);
    VERIFICAR_EQ(out.id_ex.imm, -20,          "inmediato con signo extendido");
    VERIFICAR   (out.id_ex.ctrl.alu_src,      "usa el inmediato como operando B");
    VERIFICAR   (out.id_ex.ctrl.reg_write,    "addi escribe en el banco");

    CASO("decodificacion de j");
    in = entrada(cod_j(OP_J, 0x2C), 0x04);
    etapa_id(&in, &out);
    VERIFICAR   (out.id_ex.ctrl.jump,     "activa la senal jump");
    VERIFICAR_EQ(out.id_ex.pc_salto_j, 0x2C, "destino de j calculado en ID");
}

static void probar_burbujas_y_errores(void)
{
    id_in_t  in;
    id_out_t out;

    CASO("burbuja por flush");
    in = entrada(cod_r(FUNCT_ADD, R_T0, R_T1, R_T2), 0);
    in.flush = true;
    etapa_id(&in, &out);
    VERIFICAR(!out.id_ex.valido, "flush anula la instruccion");

    CASO("burbuja por stall");
    in = entrada(cod_r(FUNCT_ADD, R_T0, R_T1, R_T2), 0);
    in.stall = true;
    etapa_id(&in, &out);
    VERIFICAR(!out.id_ex.valido, "stall inyecta burbuja hacia EX");

    CASO("entrada invalida desde IF");
    in = entrada(cod_r(FUNCT_ADD, R_T0, R_T1, R_T2), 0);
    in.if_id.valido = false;
    etapa_id(&in, &out);
    VERIFICAR(!out.id_ex.valido, "una burbuja de IF se propaga");

    CASO("instruccion fuera de la ISA");
    in = entrada(0xFC000000u, 0x08);   /* opcode 0x3F, inexistente */
    etapa_id(&in, &out);
    VERIFICAR_EQ(out.err, MIPS_ERR_OPCODE, "se reporta el opcode invalido");
    VERIFICAR   (!out.id_ex.valido,        "no se ejecuta nada desconocido");
}

int main(void)
{
    printf("== Unit tests: ETAPA 2 -- Instruction Decode ==\n");

    preparar_regs();
    probar_campos();
    probar_extension_signo();
    probar_unidad_control();
    probar_banco_y_destino_j();
    probar_registros_usados();
    probar_decodificacion_completa();
    probar_burbujas_y_errores();

    return RESUMEN("ETAPA 2 (ID)");
}
