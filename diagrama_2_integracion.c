// Ejemplo de cómo queda la etapa EX usando las subfunciones del Diagrama 2
void alu_execute_modular(MIPSPipeline* cpu) {
    if (!cpu->id_ex.valid) return;

    // 1. Mux ALUSrc
    int32_t operand_b = sub_mux_alu_src(cpu->id_ex.rt_val, cpu->id_ex.imm, cpu->id_ex.ctrl.alu_src);

    // 2. ALU Principal
    int32_t alu_result;
    bool zero_flag;
    sub_alu(cpu->id_ex.rs_val, operand_b, cpu->id_ex.ctrl.alu_op, &alu_result, &zero_flag);

    // 3. Branch Adder
    uint32_t branch_target = sub_branch_adder(cpu->id_ex.pc_plus_4, cpu->id_ex.imm);

    // 4. Mux RegDst
    uint8_t dest_reg = sub_mux_reg_dst(cpu->id_ex.rt, cpu->id_ex.rd, cpu->id_ex.ctrl.reg_dst);

    // 5. Guardar en Buffer EX/MEM
    cpu->ex_mem.alu_out = alu_result;
    cpu->ex_mem.dest_reg = dest_reg;
    cpu->ex_mem.valid = true;
}
