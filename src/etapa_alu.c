/* ===========================================================================
 * etapa_alu.c -- ETAPA 3: ALU (EXECUTE)
 * ---------------------------------------------------------------------------
 * Responsabilidades:
 *   1. Seleccionar los operandos reales: muxes de cortocircuito (forwarding)
 *      y mux ALUSrc (registro rt o inmediato).
 *   2. Ejecutar la operacion aritmetico-logica y producir la bandera Z.
 *   3. Calcular el destino de los saltos y decidir si se toman:
 *        beq/bne -> PC+4 + (inmediato << 2)   (bandera Z de la resta)
 *        j       -> destino calculado en ID
 *        jr      -> valor de rs (la ALU lo deja pasar, con cortocircuito)
 *   4. Elegir el registro destino (mux RegDst: rd para tipo R, rt para tipo I).
 *
 * Entradas (alu_in_t) : registro ID/EX, selectores de cortocircuito y los dos
 *                       datos candidatos (desde EX/MEM y desde MEM/WB)
 * Salidas  (alu_out_t): registro EX/MEM, pc_src, pc_objetivo[31:0], bandera Z
 *
 * Decision de diseno: TODOS los saltos (beq, bne, j, jr) se resuelven en esta
 * etapa. Es una regla unica y facil de verificar; el costo es una penalidad
 * fija de 2 instrucciones anuladas por salto tomado.
 * ===========================================================================
 */
#include <string.h>

#include "mips.h"

/* --- Subfuncion: ALU -----------------------------------------------------
 * La aritmetica se hace sobre uint32_t a proposito: en C el desbordamiento con
 * signo es comportamiento indefinido, mientras que el sin signo esta definido
 * como aritmetica modulo 2^32, que es exactamente lo que hace el hardware.
 */
int32_t alu_ejecutar(int32_t a, int32_t b, alu_op_t op, bool *cero)
{
    uint32_t ua = (uint32_t)a;
    uint32_t ub = (uint32_t)b;
    uint32_t r;

    switch (op) {
    case ALU_ADD:    r = ua + ub;      break;
    case ALU_SUB:    r = ua - ub;      break;
    case ALU_AND:    r = ua & ub;      break;
    case ALU_OR:     r = ua | ub;      break;
    case ALU_XOR:    r = ua ^ ub;      break;
    case ALU_NOR:    r = ~(ua | ub);   break;
    case ALU_PASA_A: r = ua;           break;
    case ALU_NOP:
    default:         r = 0u;           break;
    }

    if (cero != NULL) {
        *cero = (r == 0u);
    }
    return (int32_t)r;
}

/* --- Subfuncion: mux ALUSrc --------------------------------------------- */
int32_t alu_mux_src(int32_t rt_val, int32_t imm, bool alu_src)
{
    return alu_src ? imm : rt_val;
}

/* --- Subfuncion: mux RegDst --------------------------------------------- */
uint8_t alu_mux_reg_dst(uint8_t rt, uint8_t rd, bool reg_dst)
{
    return reg_dst ? rd : rt;
}

/* --- Subfuncion: sumador de destino de salto ----------------------------
 * destino = PC+4 + (inmediato con signo << 2). El desplazamiento se hace sobre
 * uint32_t porque desplazar a la izquierda un valor negativo es UB en C.
 */
uint32_t alu_sumador_salto(uint32_t pc_mas_4, int32_t imm)
{
    return pc_mas_4 + ((uint32_t)imm << 2);
}

/* --- Subfuncion: mux de cortocircuito (forwarding) ----------------------- */
int32_t alu_mux_cortocircuito(int32_t valor_banco, int32_t dato_ex_mem,
                              int32_t dato_mem_wb, fwd_sel_t sel)
{
    switch (sel) {
    case FWD_EX_MEM: return dato_ex_mem;
    case FWD_MEM_WB: return dato_mem_wb;
    case FWD_NINGUNO:
    default:         return valor_banco;
    }
}

/* --- Subfuncion: decision de salto -------------------------------------- */
bool alu_decision_salto(const ctrl_t *ctrl, bool cero)
{
    if (ctrl == NULL) {
        return false;
    }
    if (ctrl->jump || ctrl->jump_reg) {
        return true;                       /* saltos incondicionales */
    }
    if (ctrl->branch_eq &&  cero) {
        return true;                       /* beq: rs - rt == 0      */
    }
    if (ctrl->branch_ne && !cero) {
        return true;                       /* bne: rs - rt != 0      */
    }
    return false;
}

/* --- Funcion principal de la etapa -------------------------------------- */
void etapa_alu(const alu_in_t *in, alu_out_t *out)
{
    ctrl_t  ctrl;
    int32_t op_a, op_b_reg, op_b, resultado;
    bool    cero = false;

    memset(out, 0, sizeof *out);

    /* Una burbuja no debe alterar el PC ni producir un resultado utilizable */
    if (!in->id_ex.valido) {
        return;
    }

    ctrl = in->id_ex.ctrl;

    /* 1. Muxes de cortocircuito sobre los dos valores leidos en ID */
    op_a     = alu_mux_cortocircuito(in->id_ex.rs_val, in->dato_ex_mem,
                                     in->dato_mem_wb, in->fwd_a);
    op_b_reg = alu_mux_cortocircuito(in->id_ex.rt_val, in->dato_ex_mem,
                                     in->dato_mem_wb, in->fwd_b);

    /* 2. Mux ALUSrc y operacion */
    op_b      = alu_mux_src(op_b_reg, in->id_ex.imm, ctrl.alu_src);
    resultado = alu_ejecutar(op_a, op_b, ctrl.alu_op, &cero);
    out->cero = cero;

    /* 3. Destino y decision del salto */
    out->pc_src = alu_decision_salto(&ctrl, cero);
    if (ctrl.jump) {
        out->pc_objetivo = in->id_ex.pc_salto_j;
    } else if (ctrl.jump_reg) {
        out->pc_objetivo = (uint32_t)resultado;   /* jr: la ALU dejo pasar rs */
    } else {
        out->pc_objetivo = alu_sumador_salto(in->id_ex.pc_mas_4, in->id_ex.imm);
    }

    /* 4. Carga del registro de pipeline EX/MEM.
     *    rt_val guarda el valor YA cortocircuitado porque es el dato que sw
     *    escribira en memoria. */
    out->ex_mem.valido  = true;
    out->ex_mem.pc      = in->id_ex.pc;
    out->ex_mem.alu_res = resultado;
    out->ex_mem.rt_val  = op_b_reg;
    out->ex_mem.reg_dst = alu_mux_reg_dst(in->id_ex.rt, in->id_ex.rd, ctrl.reg_dst);
    out->ex_mem.ctrl    = ctrl;
}
