/* etapa_id.c -- Etapa 2: Instruction Decode + Unidad de Control */
#include "mips.h"

static instr_decodificada_t decodificar_campos(uint32_t instr) {
    instr_decodificada_t di;
    di.opcode      = (instr >> 26) & 0x3F;
    di.rs          = (instr >> 21) & 0x1F;
    di.rt          = (instr >> 16) & 0x1F;
    di.rd          = (instr >> 11) & 0x1F;
    di.shamt       = (instr >> 6)  & 0x1F;
    di.funct       = instr & 0x3F;
    di.inmediato   = (int16_t)(instr & 0xFFFF);
    di.direccion26 = instr & 0x03FFFFFF;
    return di;
}

alu_op_t alu_control(uint8_t opcode, uint8_t funct) {
    if (opcode == OP_ADDI || opcode == OP_LW || opcode == OP_SW) return ALU_ADD;
    if (opcode == OP_BEQ  || opcode == OP_BNE) return ALU_SUB;
    if (opcode == OP_RTYPE) {
        switch (funct) {
            case FUNCT_ADD: return ALU_ADD;
            case FUNCT_SUB: return ALU_SUB;
            case FUNCT_AND: return ALU_AND;
            case FUNCT_OR:  return ALU_OR;
            case FUNCT_SLT: return ALU_SLT;
            case FUNCT_SLL: return ALU_SLL;
            default: return ALU_NOP;
        }
    }
    return ALU_NOP;
}

senales_control_t unidad_control(uint8_t opcode, uint8_t funct) {
    senales_control_t c = {0};
    c.alu_op = alu_control(opcode, funct);

    switch (opcode) {
        case OP_RTYPE:
            c.reg_write = true;
            break;
        case OP_ADDI:
            c.reg_write  = true;
            c.alu_usa_inm = true;
            break;
        case OP_LW:
            c.reg_write   = true;
            c.alu_usa_inm = true;
            c.mem_read    = true;
            c.mem_to_reg  = true;
            break;
        case OP_SW:
            c.alu_usa_inm = true;
            c.mem_write   = true;
            break;
        case OP_BEQ:
            c.es_branch_eq = true;
            break;
        case OP_BNE:
            c.es_branch_ne = true;
            break;
        case OP_J:
            c.es_jump = true;
            break;
        case OP_JAL:
            c.es_jump   = true;
            c.link      = true;
            c.reg_write = true;
            break;
        default:
            break;
    }
    return c;
}

bool etapa_id(uint32_t instr, const int32_t regfile[NUM_REGS], id_out_t *out) {
    if (out == NULL || regfile == NULL) return false;

    out->di   = decodificar_campos(instr);
    out->ctrl = unidad_control(out->di.opcode, out->di.funct);

    out->valor_rs = regfile[out->di.rs];
    out->valor_rt = regfile[out->di.rt];

    /* registro destino: rd para tipo R, rt para tipo I, $ra para jal */
    if (out->di.opcode == OP_RTYPE) out->ctrl.reg_destino = out->di.rd;
    else if (out->ctrl.link)        out->ctrl.reg_destino = R_RA;
    else                            out->ctrl.reg_destino = out->di.rt;

    return true;
}
