#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// SUBFUNCIONES: ETAPA IF (Instruction Fetch)
// ============================================================================

// Subfunción: Mux_PC
uint32_t sub_mux_pc(uint32_t pc_plus_4, uint32_t branch_target, bool pc_src) {
    return pc_src ? branch_target : pc_plus_4;
}

// Subfunción: Adder_4
uint32_t sub_adder_4(uint32_t current_pc) {
    return current_pc + 4;
}

// ============================================================================
// SUBFUNCIONES: ETAPA ID (Instruction Decode)
// ============================================================================

// Subfunción: Sign_Extend
int32_t sub_sign_extend(uint16_t imm_16) {
    return (int32_t)((int16_t)imm_16); // Extensión de signo de 16 a 32 bits
}

// Subfunción: Register_File (Lectura)
void sub_register_file_read(const uint32_t regs[32], uint8_t rs, uint8_t rt, 
                           uint32_t* read_data1, uint32_t* read_data2) {
    *read_data1 = regs[rs];
    *read_data2 = regs[rt];
}

// ============================================================================
// SUBFUNCIONES: ETAPA EX (Execute / ALU)
// ============================================================================

// Subfunción: Mux_ALUSrc
int32_t sub_mux_alu_src(uint32_t rt_val, int32_t sign_imm, uint8_t alu_src) {
    return alu_src ? sign_imm : (int32_t)rt_val;
}

// Subfunción: Mux_RegDst
uint8_t sub_mux_reg_dst(uint8_t rt, uint8_t rd, uint8_t reg_dst) {
    return reg_dst ? rd : rt;
}

// Subfunción: Branch_Adder
uint32_t sub_branch_adder(uint32_t pc_plus_4, int32_t sign_imm) {
    return pc_plus_4 + (sign_imm << 2);
}

// Subfunción: ALU Principal
void sub_alu(uint32_t operand_a, int32_t operand_b, uint8_t alu_selection, 
             int32_t* alu_result, bool* zero_flag) {
    switch (alu_selection) {
        case 0: *alu_result = (int32_t)operand_a + operand_b; break; // ADD / ADDI / LW / SW
        case 1: *alu_result = (int32_t)operand_a - operand_b; break; // SUB / BEQ / BNE
        case 2: *alu_result = (int32_t)operand_a & operand_b; break; // AND
        case 3: *alu_result = (int32_t)operand_a | operand_b; break; // OR
        case 4: *alu_result = ~(operand_a | operand_b);       break; // NOR
        case 5: *alu_result = (int32_t)operand_a ^ operand_b; break; // XOR
        default: *alu_result = 0;
    }
    *zero_flag = (*alu_result == 0);
}

// ============================================================================
// SUBFUNCIONES: ETAPA MEM (Memory) & WB (Write Back)
// ============================================================================

// Subfunción: Branch_AND_Gate (MEM)
bool sub_branch_gate(uint8_t branch_ctrl, bool zero_flag) {
    if (branch_ctrl == 1 && zero_flag) return true;      // BEQ
    if (branch_ctrl == 2 && !zero_flag) return true;     // BNE
    return false;
}

// Subfunción: Mux_MemToReg (WB)
int32_t sub_mux_mem_to_reg(int32_t alu_result, int32_t mem_data, uint8_t mem_to_reg) {
    return mem_to_reg ? mem_data : alu_result;
}
