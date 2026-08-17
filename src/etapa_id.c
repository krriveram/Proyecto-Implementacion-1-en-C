/* ===========================================================================
 * etapa_id.c -- ETAPA 2: INSTRUCTION DECODE
 * ---------------------------------------------------------------------------
 * Responsabilidades:
 *   1. Partir la instruccion de 32 bits en sus campos (opcode, rs, rt, rd,
 *      funct, inmediato de 16 bits, direccion de 26 bits).
 *   2. Unidad de control: opcode + funct -> senales de control + operacion
 *      de la ALU ya resuelta.
 *   3. Leer el banco de registros ($0 es siempre 0).
 *   4. Extension de signo del inmediato de 16 a 32 bits.
 *   5. Calcular el destino de la instruccion j.
 *   6. Inyectar una burbuja cuando llega stall (riesgo lw-uso) o flush
 *      (salto tomado), o cuando la instruccion no pertenece a la ISA.
 *
 * Entradas (id_in_t) : registro IF/ID, banco de registros, stall, flush
 * Salidas  (id_out_t): registro ID/EX (valores, campos y control), err
 *
 * Nota de diseno: el banco de registros se escribe en la primera mitad del
 * ciclo y se lee en la segunda (el nucleo llama a WB antes que a ID). Eso
 * elimina el riesgo de datos a distancia 3 sin necesidad de cortocircuito.
 * ===========================================================================
 */
#include <string.h>

#include "mips.h"

/* --- Subfunciones: extraccion de campos --------------------------------- */
uint8_t  id_campo_opcode(uint32_t instr) { return (uint8_t)((instr >> 26) & 0x3Fu); }
uint8_t  id_campo_rs    (uint32_t instr) { return (uint8_t)((instr >> 21) & 0x1Fu); }
uint8_t  id_campo_rt    (uint32_t instr) { return (uint8_t)((instr >> 16) & 0x1Fu); }
uint8_t  id_campo_rd    (uint32_t instr) { return (uint8_t)((instr >> 11) & 0x1Fu); }
uint8_t  id_campo_funct (uint32_t instr) { return (uint8_t) (instr        & 0x3Fu); }
uint16_t id_campo_imm16 (uint32_t instr) { return (uint16_t)(instr        & 0xFFFFu); }
uint32_t id_campo_addr26(uint32_t instr) { return          (instr        & 0x03FFFFFFu); }

/* --- Subfuncion: extension de signo de 16 a 32 bits ---------------------- */
int32_t id_extension_signo(uint16_t imm16)
{
    /* El doble casting es la forma portable de replicar el bit 15: primero se
     * reinterpreta como entero de 16 bits con signo y luego se promueve. */
    return (int32_t)(int16_t)imm16;
}

/* --- Subfuncion: lectura del banco de registros --------------------------
 * $0 esta cableado a cero por hardware; el nucleo garantiza regs[0] == 0.
 */
void id_banco_leer(const int32_t *regs, uint8_t rs, uint8_t rt,
                   int32_t *dato1, int32_t *dato2)
{
    if (dato1 != NULL) {
        *dato1 = (regs != NULL && rs < MIPS_NUM_REGS) ? regs[rs] : 0;
    }
    if (dato2 != NULL) {
        *dato2 = (regs != NULL && rt < MIPS_NUM_REGS) ? regs[rt] : 0;
    }
}

/* --- Subfuncion: destino de la instruccion j -----------------------------
 * Los 26 bits de la instruccion se desplazan 2 (direcciones de palabra) y los
 * 4 bits altos se heredan de PC+4: destino = PC+4[31:28] : addr26 : 00
 */
uint32_t id_destino_j(uint32_t pc_mas_4, uint32_t addr26)
{
    return (pc_mas_4 & 0xF0000000u) | ((addr26 & 0x03FFFFFFu) << 2);
}

/* --- Subfuncion: unidad de control ---------------------------------------
 * Devuelve false si el opcode/funct no pertenece a la ISA soportada, para que
 * la etapa reporte MIPS_ERR_OPCODE en lugar de ejecutar algo indefinido.
 */
bool id_unidad_control(uint8_t opcode, uint8_t funct, ctrl_t *ctrl)
{
    ctrl_t c;

    memset(&c, 0, sizeof c);
    c.alu_op = ALU_NOP;

    switch (opcode) {
    case OP_RTIPO:
        switch (funct) {
        case FUNCT_ADD: c.alu_op = ALU_ADD; break;
        case FUNCT_SUB: c.alu_op = ALU_SUB; break;
        case FUNCT_AND: c.alu_op = ALU_AND; break;
        case FUNCT_OR:  c.alu_op = ALU_OR;  break;
        case FUNCT_XOR: c.alu_op = ALU_XOR; break;
        case FUNCT_NOR: c.alu_op = ALU_NOR; break;
        case FUNCT_JR:
            /* jr no escribe registros: la ALU solo deja pasar rs para que la
             * etapa 3 lo use como nuevo PC (y asi aprovecha el cortocircuito). */
            c.alu_op   = ALU_PASA_A;
            c.jump_reg = true;
            *ctrl = c;
            return true;
        default:
            return false;   /* funct fuera de la ISA */
        }
        c.reg_dst   = true;   /* destino rd */
        c.reg_write = true;
        break;

    case OP_ADDI:
        c.alu_src   = true;
        c.reg_write = true;   /* destino rt */
        c.alu_op    = ALU_ADD;
        break;

    case OP_LW:
        c.alu_src    = true;  /* direccion = rs + offset */
        c.reg_write  = true;
        c.mem_read   = true;
        c.mem_to_reg = true;
        c.alu_op     = ALU_ADD;
        break;

    case OP_SW:
        c.alu_src   = true;
        c.mem_write = true;
        c.alu_op    = ALU_ADD;
        break;

    case OP_BEQ:
        c.branch_eq = true;
        c.alu_op    = ALU_SUB;  /* la bandera Z indica rs == rt */
        break;

    case OP_BNE:
        c.branch_ne = true;
        c.alu_op    = ALU_SUB;
        break;

    case OP_J:
        c.jump   = true;
        c.alu_op = ALU_NOP;
        break;

    default:
        return false;   /* opcode fuera de la ISA */
    }

    *ctrl = c;
    return true;
}

/* --- Subfuncion: que operandos lee realmente una instruccion -------------
 * La usa la unidad de riesgos para no congelar el pipeline por un registro
 * que la instruccion ni siquiera va a leer (p.ej. addi no lee rt).
 */
void id_registros_usados(uint32_t instr, bool *usa_rs, bool *usa_rt)
{
    uint8_t op    = id_campo_opcode(instr);
    uint8_t funct = id_campo_funct(instr);
    bool    rs    = false;
    bool    rt    = false;

    switch (op) {
    case OP_RTIPO:
        if (funct == FUNCT_JR) { rs = true;  rt = false; }
        else                   { rs = true;  rt = true;  }
        break;
    case OP_ADDI: case OP_LW:            rs = true;  rt = false; break;
    case OP_SW:   case OP_BEQ: case OP_BNE: rs = true; rt = true; break;
    case OP_J:                           rs = false; rt = false; break;
    default:                             rs = false; rt = false; break;
    }

    if (usa_rs != NULL) *usa_rs = rs;
    if (usa_rt != NULL) *usa_rt = rt;
}

/* --- Funcion principal de la etapa -------------------------------------- */
void etapa_id(const id_in_t *in, id_out_t *out)
{
    uint32_t instr;
    ctrl_t   ctrl;

    memset(out, 0, sizeof *out);

    /* Burbuja: por salto tomado (flush), por riesgo lw-uso (stall) o porque
     * IF no entrego una instruccion valida. */
    if (in->flush || in->stall || !in->if_id.valido) {
        return;
    }

    instr = in->if_id.instr;

    /* Unidad de control. Si la instruccion no pertenece a la ISA se reporta el
     * error y se propaga una burbuja: nunca se ejecuta algo desconocido. */
    if (!id_unidad_control(id_campo_opcode(instr), id_campo_funct(instr), &ctrl)) {
        out->err = MIPS_ERR_OPCODE;
        return;
    }

    out->id_ex.valido     = true;
    out->id_ex.pc         = in->if_id.pc;
    out->id_ex.pc_mas_4   = in->if_id.pc_mas_4;
    out->id_ex.rs         = id_campo_rs(instr);
    out->id_ex.rt         = id_campo_rt(instr);
    out->id_ex.rd         = id_campo_rd(instr);
    out->id_ex.imm        = id_extension_signo(id_campo_imm16(instr));
    out->id_ex.pc_salto_j = id_destino_j(in->if_id.pc_mas_4, id_campo_addr26(instr));
    out->id_ex.ctrl       = ctrl;

    id_banco_leer(in->regs, out->id_ex.rs, out->id_ex.rt,
                  &out->id_ex.rs_val, &out->id_ex.rt_val);
}
