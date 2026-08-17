/* mips_core.c -- Encadena IF->ID->EX->MEM->WB para simular el CPU completo */
#include <string.h>
#include "mips.h"

void cpu_init(cpu_t *cpu) {
    if (cpu == NULL) return;
    memset(cpu, 0, sizeof(*cpu));
}

bool cpu_step(cpu_t *cpu) {
    if (cpu == NULL) return false;

    if_out_t ifo;
    if (!etapa_if(cpu->pc, cpu->imem, &ifo)) return false;
    if (ifo.instr == NOP_INSTR) return false; /* fin de programa */

    id_out_t ido;
    etapa_id(ifo.instr, cpu->regfile, &ido);

    ex_out_t exo;
    etapa_ex(&ido.di, &ido.ctrl, ido.valor_rs, ido.valor_rt, ifo.pc_siguiente, &exo);

    mem_out_t memo;
    if (!etapa_mem(&ido.ctrl, exo.resultado_alu, ido.valor_rt, cpu->dmem, &memo))
        return false; /* direccion invalida */

    etapa_wb(&ido.ctrl, ido.ctrl.reg_destino, exo.resultado_alu, memo.dato_leido,
             ifo.pc_siguiente, cpu->regfile);

    /* actualizacion de PC */
    if (ido.ctrl.es_jump) {
        cpu->pc = exo.pc_jump;
    } else if ((ido.ctrl.es_branch_eq && exo.zero) ||
               (ido.ctrl.es_branch_ne && !exo.zero)) {
        cpu->pc = exo.pc_branch;
    } else {
        cpu->pc = ifo.pc_siguiente;
    }

    cpu->instrucciones_ejecutadas++;
    return true;
}

void cpu_run(cpu_t *cpu, uint64_t max_pasos) {
    if (cpu == NULL) return;
    for (uint64_t i = 0; i < max_pasos; i++) {
        if (!cpu_step(cpu)) break;
    }
}
