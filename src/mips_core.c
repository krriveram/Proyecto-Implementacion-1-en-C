/* ===========================================================================
 * mips_core.c -- Nucleo del simulador: conexionado de las 5 etapas
 * ---------------------------------------------------------------------------
 * Aqui vive el "reloj" del procesador. Cada llamada a mips_ciclo() equivale a
 * un ciclo de reloj y se ejecuta en dos fases, igual que el hardware real:
 *
 *   FASE COMBINACIONAL: se evaluan las 5 etapas leyendo SOLO el valor actual
 *   de los registros de pipeline. Los resultados se guardan en variables
 *   locales.
 *   FASE DE FLANCO: se actualizan de golpe PC, IF/ID, ID/EX, EX/MEM y MEM/WB.
 *
 * Con este esquema el orden en que se llamen las etapas no altera el
 * resultado, con una sola excepcion deliberada: WB se ejecuta primero y su
 * escritura se aplica antes de que ID lea el banco de registros. Eso modela el
 * banco que escribe en la primera mitad del ciclo y lee en la segunda, y es lo
 * que elimina el riesgo de datos a distancia 3.
 *
 * Riesgos cubiertos:
 *   - Datos (distancia 1 y 2): unidad de cortocircuito EX/MEM y MEM/WB.
 *   - Datos (distancia 3)    : banco con escritura en la primera mitad.
 *   - lw seguido de uso      : unidad de riesgos -> 1 ciclo de burbuja.
 *   - Control (saltos)       : anulacion de las 2 instrucciones mas jovenes.
 * ===========================================================================
 */
#include <stdio.h>
#include <string.h>

#include "mips.h"

/* ===========================================================================
 * Unidad de deteccion de riesgos (lw seguido de uso)
 * ---------------------------------------------------------------------------
 * Unico caso que el cortocircuito no puede resolver: el dato de un lw recien
 * esta disponible al final de la etapa MEM, un ciclo despues de que la
 * instruccion dependiente lo necesita en EX. Se congela un ciclo.
 * =========================================================================*/
bool unidad_riesgos(const id_ex_t *en_ex, const if_id_t *en_id)
{
    uint8_t destino;
    bool    usa_rs = false;
    bool    usa_rt = false;

    if (en_ex == NULL || en_id == NULL) {
        return false;
    }
    if (!en_ex->valido || !en_ex->ctrl.mem_read) {
        return false;   /* solo un lw en EX puede provocar la burbuja */
    }
    if (!en_id->valido) {
        return false;   /* no hay instruccion que congelar */
    }

    destino = en_ex->ctrl.reg_dst ? en_ex->rd : en_ex->rt;
    if (destino == 0) {
        return false;   /* escribir en $0 no genera dependencia real */
    }

    id_registros_usados(en_id->instr, &usa_rs, &usa_rt);
    return (usa_rs && id_campo_rs(en_id->instr) == destino)
        || (usa_rt && id_campo_rt(en_id->instr) == destino);
}

/* ===========================================================================
 * Unidad de cortocircuito (forwarding)
 * ---------------------------------------------------------------------------
 * EX/MEM tiene prioridad sobre MEM/WB porque es el productor mas reciente.
 * Un lw que este en MEM se excluye como fuente (su dato aun no existe); ese
 * caso ya lo cubre la unidad de riesgos con una burbuja.
 * =========================================================================*/
void unidad_cortocircuito(const id_ex_t *en_ex, const ex_mem_t *en_mem,
                          const mem_wb_t *en_wb, fwd_sel_t *fwd_a,
                          fwd_sel_t *fwd_b)
{
    bool mem_util;
    bool wb_util;

    if (fwd_a != NULL) *fwd_a = FWD_NINGUNO;
    if (fwd_b != NULL) *fwd_b = FWD_NINGUNO;

    if (en_ex == NULL || en_mem == NULL || en_wb == NULL || !en_ex->valido) {
        return;
    }

    mem_util = en_mem->valido && en_mem->ctrl.reg_write
            && en_mem->reg_dst != 0 && !en_mem->ctrl.mem_to_reg;
    wb_util  = en_wb->valido  && en_wb->ctrl.reg_write  && en_wb->reg_dst != 0;

    if (fwd_a != NULL) {
        if      (mem_util && en_mem->reg_dst == en_ex->rs) *fwd_a = FWD_EX_MEM;
        else if (wb_util  && en_wb->reg_dst  == en_ex->rs) *fwd_a = FWD_MEM_WB;
    }
    if (fwd_b != NULL) {
        if      (mem_util && en_mem->reg_dst == en_ex->rt) *fwd_b = FWD_EX_MEM;
        else if (wb_util  && en_wb->reg_dst  == en_ex->rt) *fwd_b = FWD_MEM_WB;
    }
}

/* ===========================================================================
 * Gestion del estado
 * =========================================================================*/
void mips_reset(mips_t *cpu)
{
    if (cpu == NULL) {
        return;
    }
    memset(cpu, 0, sizeof *cpu);
}

bool mips_cargar_programa(mips_t *cpu, const uint32_t *prog, uint32_t n)
{
    uint32_t i;

    if (cpu == NULL || prog == NULL || n > MIPS_IMEM_PALABRAS) {
        return false;
    }
    for (i = 0; i < n; i++) {
        cpu->imem[i] = prog[i];
    }
    cpu->n_instr = n;
    return true;
}

int32_t mips_leer_dmem(const mips_t *cpu, uint32_t dir_byte)
{
    uint32_t indice;

    if (cpu == NULL || !mem_dir_valida(dir_byte, MIPS_DMEM_PALABRAS, &indice, NULL)) {
        return 0;
    }
    return cpu->dmem[indice];
}

bool mips_escribir_dmem(mips_t *cpu, uint32_t dir_byte, int32_t valor)
{
    uint32_t indice;

    if (cpu == NULL || !mem_dir_valida(dir_byte, MIPS_DMEM_PALABRAS, &indice, NULL)) {
        return false;
    }
    cpu->dmem[indice] = valor;
    return true;
}

bool mips_pipeline_vacio(const mips_t *cpu)
{
    if (cpu == NULL) {
        return true;
    }
    return !cpu->if_id.valido && !cpu->id_ex.valido
        && !cpu->ex_mem.valido && !cpu->mem_wb.valido;
}

/* --- Traza de un ciclo (solo si cpu->traza esta activo) ------------------ */
static void imprimir_traza(const mips_t *cpu, const if_id_t *nuevo_if_id,
                           bool stall, bool flush)
{
    char texto[48];

    mips_desensamblar(nuevo_if_id->valido ? nuevo_if_id->instr : MIPS_NOP,
                      texto, sizeof texto);
    printf("  ciclo %3u | PC=0x%04X | IF:%-20s | ID:%c EX:%c MEM:%c WB:%c%s%s\n",
           (unsigned)cpu->ciclos, (unsigned)cpu->pc, texto,
           cpu->if_id.valido  ? 'x' : '.',
           cpu->id_ex.valido  ? 'x' : '.',
           cpu->ex_mem.valido ? 'x' : '.',
           cpu->mem_wb.valido ? 'x' : '.',
           stall ? "  [BURBUJA lw-uso]" : "",
           flush ? "  [SALTO TOMADO]"   : "");
}

/* ===========================================================================
 * Un ciclo de reloj
 * =========================================================================*/
void mips_ciclo(mips_t *cpu)
{
    wb_in_t   wb_in;   wb_out_t  wb_out;
    mem_in_t  mem_in;  mem_out_t mem_out;
    alu_in_t  alu_in;  alu_out_t alu_out;
    id_in_t   id_in;   id_out_t  id_out;
    if_in_t   if_in;   if_out_t  if_out;
    fwd_sel_t fwd_a, fwd_b;
    bool      stall, flush;

    if (cpu == NULL || cpu->err != MIPS_OK) {
        return;   /* tras un error el procesador se detiene */
    }

    /* --- ETAPA 5: WRITE BACK -------------------------------------------- */
    memset(&wb_in, 0, sizeof wb_in);
    wb_in.mem_wb = cpu->mem_wb;
    etapa_wb(&wb_in, &wb_out);
    if (wb_out.reg_write) {
        cpu->regs[wb_out.reg_dst] = wb_out.dato;   /* escritura: 1a mitad */
    }
    if (cpu->mem_wb.valido) {
        cpu->instr_retiradas++;
    }

    /* --- ETAPA 4: MEMORY ------------------------------------------------ */
    memset(&mem_in, 0, sizeof mem_in);
    mem_in.ex_mem        = cpu->ex_mem;
    mem_in.dmem          = cpu->dmem;
    mem_in.dmem_palabras = MIPS_DMEM_PALABRAS;
    etapa_mem(&mem_in, &mem_out);

    /* --- ETAPA 3: ALU (con cortocircuito) ------------------------------- */
    unidad_cortocircuito(&cpu->id_ex, &cpu->ex_mem, &cpu->mem_wb, &fwd_a, &fwd_b);
    memset(&alu_in, 0, sizeof alu_in);
    alu_in.id_ex       = cpu->id_ex;
    alu_in.fwd_a       = fwd_a;
    alu_in.fwd_b       = fwd_b;
    alu_in.dato_ex_mem = cpu->ex_mem.alu_res;
    alu_in.dato_mem_wb = wb_out.dato;   /* el dato que WB acaba de escribir */
    etapa_alu(&alu_in, &alu_out);

    /* --- Unidades de riesgo --------------------------------------------- */
    stall = unidad_riesgos(&cpu->id_ex, &cpu->if_id);
    flush = alu_out.pc_src;   /* salto tomado: se anulan ID e IF */
    if (flush) {
        stall = false;        /* el salto manda: no tiene sentido congelar */
    }

    /* --- ETAPA 2: INSTRUCTION DECODE ------------------------------------ */
    memset(&id_in, 0, sizeof id_in);
    id_in.if_id = cpu->if_id;
    id_in.regs  = cpu->regs;
    id_in.stall = stall;
    id_in.flush = flush;
    etapa_id(&id_in, &id_out);

    /* --- ETAPA 1: INSTRUCTION FETCH + PC -------------------------------- */
    memset(&if_in, 0, sizeof if_in);
    if_in.pc            = cpu->pc;
    if_in.imem          = cpu->imem;
    if_in.imem_palabras = MIPS_IMEM_PALABRAS;
    if_in.pc_src        = alu_out.pc_src;
    if_in.pc_objetivo   = alu_out.pc_objetivo;
    if_in.stall         = stall;
    if_in.flush         = flush;
    etapa_if(&if_in, &if_out);

    /* --- Registro del primer error detectado ---------------------------- */
    if (cpu->err == MIPS_OK && mem_out.err != MIPS_OK) {
        cpu->err = mem_out.err; cpu->err_pc = mem_in.ex_mem.pc;
    }
    if (cpu->err == MIPS_OK && id_out.err != MIPS_OK) {
        cpu->err = id_out.err;  cpu->err_pc = id_in.if_id.pc;
    }
    if (cpu->err == MIPS_OK && if_out.err != MIPS_OK) {
        cpu->err = if_out.err;  cpu->err_pc = if_in.pc;
    }

    if (cpu->traza) {
        imprimir_traza(cpu, &if_out.if_id, stall, flush);
    }

    /* --- FLANCO DE RELOJ: se actualizan todos los registros de pipeline -- */
    cpu->mem_wb = mem_out.mem_wb;
    cpu->ex_mem = alu_out.ex_mem;
    cpu->id_ex  = id_out.id_ex;
    if (if_out.cargar_if_id) {
        cpu->if_id = if_out.if_id;
    }
    cpu->pc      = if_out.pc_siguiente;
    cpu->regs[0] = 0;   /* $0 cableado a cero */

    cpu->ciclos++;
    if (stall) cpu->stalls++;
    if (flush) cpu->flushes++;
}

/* ===========================================================================
 * Ejecucion hasta que el programa termine
 * ---------------------------------------------------------------------------
 * Se detiene cuando el pipeline queda vacio y el PC ya salio del programa
 * cargado, cuando aparece un error, o al agotar max_ciclos (red de seguridad
 * frente a un bucle infinito en el programa de prueba).
 * =========================================================================*/
uint64_t mips_ejecutar(mips_t *cpu, uint64_t max_ciclos)
{
    if (cpu == NULL) {
        return 0;
    }
    while (cpu->ciclos < max_ciclos && cpu->err == MIPS_OK) {
        if (mips_pipeline_vacio(cpu) && cpu->pc >= (uint32_t)(cpu->n_instr * 4u)) {
            break;
        }
        mips_ciclo(cpu);
    }
    return cpu->ciclos;
}

/* ===========================================================================
 * Volcado de estado (depuracion / demo)
 * =========================================================================*/
void mips_volcado(const mips_t *cpu)
{
    int i;

    if (cpu == NULL) {
        return;
    }

    printf("  PC = 0x%08X | ciclos = %u | instrucciones = %u"
           " | burbujas = %u | saltos tomados = %u\n",
           (unsigned)cpu->pc, (unsigned)cpu->ciclos,
           (unsigned)cpu->instr_retiradas, (unsigned)cpu->stalls,
           (unsigned)cpu->flushes);
    if (cpu->err != MIPS_OK) {
        printf("  ERROR: %s (PC = 0x%08X)\n", mips_err_str(cpu->err),
               (unsigned)cpu->err_pc);
    }

    printf("  Registros distintos de cero:\n   ");
    for (i = 0; i < MIPS_NUM_REGS; i++) {
        if (cpu->regs[i] != 0) {
            printf(" $%-2d=%-8d", i, (int)cpu->regs[i]);
        }
    }
    printf("\n");
}
