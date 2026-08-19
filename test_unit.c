/*
 * Unit tests del simulador MIPS.
 *
 * Cada una de las 5 funciones del pipeline se prueba de forma AISLADA
 * (a diferencia de mips_sim.c, que solo contiene pruebas integrales que
 * ejecutan un programa completo por varios ciclos):
 *
 *   1) instruction_fetch  (etapa IF)
 *   2) instruction_decode (etapa ID)
 *   3) alu_execute        (etapa EX)
 *   4) memory_access      (etapa MEM)
 *   5) write_back         (etapa WB)
 *
 * Se define UNIT_TESTING antes de incluir mips_sim.c para reutilizar toda su
 * lógica (tipos, funciones auxiliares y las 5 etapas) SIN arrastrar su main().
 *
 * Compilar y ejecutar:  make unit-run
 */
#ifndef UNIT_TESTING
#define UNIT_TESTING
#endif
#include "mips_sim.c"

// ==========================================
// MINI-FRAMEWORK DE PRUEBAS
// ==========================================
static int g_checks = 0;   // aserciones evaluadas
static int g_fails  = 0;   // aserciones fallidas

#define CHECK(cond, msg) do {                                            \
    g_checks++;                                                          \
    if (!(cond)) {                                                       \
        g_fails++;                                                       \
        printf("   [FALLO] %s (linea %d)\n", (msg), __LINE__);           \
    }                                                                    \
} while (0)

#define CHECK_EQ(actual, expected, msg) do {                             \
    g_checks++;                                                          \
    long _a = (long)(actual), _e = (long)(expected);                     \
    if (_a != _e) {                                                      \
        g_fails++;                                                       \
        printf("   [FALLO] %s: esperado %ld, obtenido %ld (linea %d)\n", \
               (msg), _e, _a, __LINE__);                                 \
    }                                                                    \
} while (0)

// ------------------------------------------------------------------
// Helpers para construir instrucciones MIPS de 32 bits
// ------------------------------------------------------------------
static uint32_t enc_R(uint8_t rs, uint8_t rt, uint8_t rd, uint8_t funct) {
    return ((uint32_t)0 << 26) | ((uint32_t)rs << 21) | ((uint32_t)rt << 16) |
           ((uint32_t)rd << 11) | funct;
}
static uint32_t enc_I(uint8_t op, uint8_t rs, uint8_t rt, uint16_t imm) {
    return ((uint32_t)op << 26) | ((uint32_t)rs << 21) | ((uint32_t)rt << 16) | imm;
}
static uint32_t enc_J(uint8_t op, uint32_t addr) {
    return ((uint32_t)op << 26) | (addr & 0x3FFFFFF);
}

// ==========================================
// 1) UNIT TEST: instruction_fetch (IF)
// ==========================================
static void test_instruction_fetch(void) {
    printf(" Unit test: instruction_fetch (IF)\n");
    MIPSPipeline cpu;

    // Caso normal: se lee la instruccion en PC y el PC avanza +4.
    cpu_reset(&cpu);
    imem_write(&cpu, 0, enc_I(8, 0, 8, 10)); // addi $t0,$0,10 en 0x00
    instruction_fetch(&cpu);
    CHECK_EQ(cpu.if_id.instr, enc_I(8, 0, 8, 10), "IF carga la instruccion correcta");
    CHECK_EQ(cpu.if_id.pc_plus_4, 4, "IF calcula PC+4");
    CHECK(cpu.if_id.valid, "IF marca valido cuando la instruccion no es NOP");
    CHECK_EQ(cpu.pc, 4, "IF avanza el PC a 4");

    // NOP (instruccion 0) debe marcar el latch como invalido.
    cpu_reset(&cpu);
    instruction_fetch(&cpu); // imem vacia -> lee 0
    CHECK(!cpu.if_id.valid, "IF marca invalido ante un NOP (instr=0)");

    // Stall (load-use): el PC NO avanza y el latch IF/ID se preserva.
    cpu_reset(&cpu);
    cpu.pc = 8;
    cpu.stall = true;
    cpu.if_id.instr = 0xAAAA;
    instruction_fetch(&cpu);
    CHECK_EQ(cpu.pc, 8, "IF congela el PC durante un stall");
    CHECK_EQ(cpu.if_id.instr, 0xAAAA, "IF preserva IF/ID durante un stall");
    CHECK(!cpu.stall, "IF limpia la senal de stall tras aplicarla");

    // Redireccion por salto tomado: el PC salta a next_pc y luego avanza +4.
    cpu_reset(&cpu);
    imem_write(&cpu, 12, enc_I(8, 0, 9, 7)); // instruccion destino en 0x0C
    cpu.next_pc = 12;
    cpu.branch_taken = true;
    instruction_fetch(&cpu);
    CHECK_EQ(cpu.if_id.instr, enc_I(8, 0, 9, 7), "IF busca en el destino del salto");
    CHECK_EQ(cpu.pc, 16, "IF avanza el PC a destino+4");
    CHECK(!cpu.branch_taken, "IF limpia branch_taken tras redirigir");
}

// ==========================================
// 2) UNIT TEST: instruction_decode (ID)
// ==========================================
static void test_instruction_decode(void) {
    printf(" Unit test: instruction_decode (ID)\n");
    MIPSPipeline cpu;

    // R-type (add): reg_dst=1, reg_write=1, alu_op=R, campos rs/rt/rd.
    cpu_reset(&cpu);
    cpu.if_id.valid = true;
    cpu.if_id.instr = enc_R(8, 9, 10, 32); // add $t2,$t0,$t1
    instruction_decode(&cpu);
    CHECK(cpu.id_ex.valid, "ID valida una instruccion R");
    CHECK_EQ(cpu.id_ex.ctrl.reg_dst, 1, "add: reg_dst=1");
    CHECK_EQ(cpu.id_ex.ctrl.reg_write, 1, "add: reg_write=1");
    CHECK_EQ(cpu.id_ex.ctrl.alu_op, ALU_OP_R, "add: alu_op=ALU_OP_R");
    CHECK_EQ(cpu.id_ex.rs, 8, "add: rs decodificado");
    CHECK_EQ(cpu.id_ex.rt, 9, "add: rt decodificado");
    CHECK_EQ(cpu.id_ex.rd, 10, "add: rd decodificado");

    // lw: mem_read, mem_to_reg, reg_write, alu_src, alu_op=ADD, imm con signo.
    cpu_reset(&cpu);
    cpu.if_id.valid = true;
    cpu.if_id.instr = enc_I(35, 0, 9, (uint16_t)-4); // lw $t1,-4($0)
    instruction_decode(&cpu);
    CHECK_EQ(cpu.id_ex.ctrl.mem_read, 1, "lw: mem_read=1");
    CHECK_EQ(cpu.id_ex.ctrl.mem_to_reg, 1, "lw: mem_to_reg=1");
    CHECK_EQ(cpu.id_ex.ctrl.reg_write, 1, "lw: reg_write=1");
    CHECK_EQ(cpu.id_ex.ctrl.alu_src, 1, "lw: alu_src=1");
    CHECK_EQ(cpu.id_ex.ctrl.alu_op, ALU_OP_ADD, "lw: alu_op=ADD");
    CHECK_EQ(cpu.id_ex.imm, -4, "lw: inmediato con extension de signo");

    // sw: mem_write=1 y NO escribe registro.
    cpu_reset(&cpu);
    cpu.if_id.valid = true;
    cpu.if_id.instr = enc_I(43, 0, 8, 4); // sw $t0,4($0)
    instruction_decode(&cpu);
    CHECK_EQ(cpu.id_ex.ctrl.mem_write, 1, "sw: mem_write=1");
    CHECK_EQ(cpu.id_ex.ctrl.reg_write, 0, "sw: reg_write=0");

    // beq: branch=1 y alu_op=SUB.
    cpu_reset(&cpu);
    cpu.if_id.valid = true;
    cpu.if_id.instr = enc_I(4, 8, 9, 3); // beq $t0,$t1,3
    instruction_decode(&cpu);
    CHECK_EQ(cpu.id_ex.ctrl.branch, 1, "beq: branch=1");
    CHECK_EQ(cpu.id_ex.ctrl.alu_op, ALU_OP_SUB, "beq: alu_op=SUB");

    // j: se resuelve en ID -> branch_taken y next_pc = (PC+4)[31:28]:(addr<<2).
    cpu_reset(&cpu);
    cpu.if_id.valid = true;
    cpu.if_id.pc_plus_4 = 4;
    cpu.if_id.instr = enc_J(2, 5); // j 5 -> destino 5*4 = 20
    instruction_decode(&cpu);
    CHECK(cpu.branch_taken, "j: activa branch_taken en ID");
    CHECK_EQ(cpu.next_pc, 20, "j: next_pc = addr<<2");

    // Riesgo carga-uso: lw en EX cuyo destino ($t0) usa la instruccion en ID.
    // Debe insertarse burbuja (id_ex invalido) y activar stall.
    cpu_reset(&cpu);
    cpu.id_ex.valid = true;
    cpu.id_ex.ctrl.mem_read = 1;  // la de adelante es un lw...
    cpu.id_ex.rt = 8;             // ...que carga $t0
    cpu.if_id.valid = true;
    cpu.if_id.instr = enc_R(8, 0, 10, 32); // add $t2,$t0,$0  (usa $t0 como rs)
    instruction_decode(&cpu);
    CHECK(cpu.stall, "load-use: activa stall");
    CHECK(!cpu.id_ex.valid, "load-use: inserta burbuja (id_ex invalido)");
}

// ==========================================
// 3) UNIT TEST: alu_execute (EX)
// ==========================================
static void test_alu_execute(void) {
    printf(" Unit test: alu_execute (EX)\n");
    MIPSPipeline cpu;

    // Prepara una operacion R con dos operandos de registro ya cargados.
    // funct: 32=add, 34=sub, 36=and, 37=or, 38=xor, 39=nor.
    struct { uint8_t funct; uint32_t a, b; int32_t exp; const char* name; } cases[] = {
        {32, 20, 22, 42,          "add"},
        {34, 50, 8,  42,          "sub"},
        {36, 0xF0, 0x3C, 0x30,    "and"},
        {37, 0xF0, 0x0F, 0xFF,    "or"},
        {38, 0xFF, 0x0F, 0xF0,    "xor"},
        {39, 0x00, 0x00, -1,      "nor"}, // ~(0|0) = 0xFFFFFFFF = -1
    };
    for (size_t i = 0; i < sizeof(cases)/sizeof(cases[0]); i++) {
        cpu_reset(&cpu);
        cpu.regs[8] = cases[i].a;
        cpu.regs[9] = cases[i].b;
        cpu.id_ex.valid = true;
        cpu.id_ex.rs = 8;
        cpu.id_ex.rt = 9;
        cpu.id_ex.rd = 10;
        cpu.id_ex.funct = cases[i].funct;
        cpu.id_ex.ctrl.reg_dst = 1;
        cpu.id_ex.ctrl.reg_write = 1;
        cpu.id_ex.ctrl.alu_op = ALU_OP_R;
        alu_execute(&cpu);
        CHECK_EQ(cpu.ex_mem.alu_out, cases[i].exp, cases[i].name);
        CHECK_EQ(cpu.ex_mem.dest_reg, 10, "EX: dest_reg=rd en R-type");
    }

    // addi (alu_src=1): usa el inmediato como segundo operando.
    cpu_reset(&cpu);
    cpu.regs[8] = 100;
    cpu.id_ex.valid = true;
    cpu.id_ex.rs = 8;
    cpu.id_ex.rt = 9;
    cpu.id_ex.imm = -30;
    cpu.id_ex.ctrl.alu_src = 1;
    cpu.id_ex.ctrl.reg_write = 1;
    cpu.id_ex.ctrl.alu_op = ALU_OP_ADD;
    alu_execute(&cpu);
    CHECK_EQ(cpu.ex_mem.alu_out, 70, "addi: 100 + (-30) = 70");
    CHECK_EQ(cpu.ex_mem.dest_reg, 9, "EX: dest_reg=rt cuando reg_dst=0");

    // Forwarding EX/MEM: el registro del banco esta obsoleto, pero la
    // instruccion de adelante (en EX/MEM) tiene el valor correcto en alu_out.
    cpu_reset(&cpu);
    cpu.regs[10] = 1;            // valor OBSOLETO en el banco
    cpu.id_ex.valid = true;
    cpu.id_ex.rs = 10;          // usamos $10 como operando...
    cpu.id_ex.rt = 0;
    cpu.id_ex.funct = 32;       // add
    cpu.id_ex.ctrl.reg_dst = 1;
    cpu.id_ex.ctrl.reg_write = 1;
    cpu.id_ex.ctrl.alu_op = ALU_OP_R;
    cpu.ex_mem.valid = true;    // instruccion 1 adelante...
    cpu.ex_mem.ctrl.reg_write = 1;
    cpu.ex_mem.dest_reg = 10;   // ...que escribe $10
    cpu.ex_mem.alu_out = 41;    // con el valor REAL
    alu_execute(&cpu);
    CHECK_EQ(cpu.ex_mem.alu_out, 41, "forwarding: usa 41 (adelantado) en vez de 1");

    // Branch tomado (beq con operandos iguales -> resta 0 -> zero).
    cpu_reset(&cpu);
    cpu.regs[8] = 5;
    cpu.regs[9] = 5;
    cpu.id_ex.valid = true;
    cpu.id_ex.rs = 8;
    cpu.id_ex.rt = 9;
    cpu.id_ex.imm = 3;
    cpu.id_ex.pc_plus_4 = 8;
    cpu.id_ex.ctrl.branch = 1;
    cpu.id_ex.ctrl.alu_op = ALU_OP_SUB;
    alu_execute(&cpu);
    CHECK(cpu.branch_taken, "beq igual: branch tomado");
    CHECK_EQ(cpu.next_pc, 8 + (3 << 2), "beq: next_pc = PC+4 + imm*4");

    // Branch NO tomado (beq con operandos distintos).
    cpu_reset(&cpu);
    cpu.regs[8] = 5;
    cpu.regs[9] = 6;
    cpu.id_ex.valid = true;
    cpu.id_ex.rs = 8;
    cpu.id_ex.rt = 9;
    cpu.id_ex.ctrl.branch = 1;
    cpu.id_ex.ctrl.alu_op = ALU_OP_SUB;
    alu_execute(&cpu);
    CHECK(!cpu.branch_taken, "beq distinto: branch NO tomado");
}

// ==========================================
// 4) UNIT TEST: memory_access (MEM)
// ==========================================
static void test_memory_access(void) {
    printf(" Unit test: memory_access (MEM)\n");
    MIPSPipeline cpu;

    // sw: escribe rt_val en la direccion calculada por la ALU.
    cpu_reset(&cpu);
    cpu.ex_mem.valid = true;
    cpu.ex_mem.ctrl.mem_write = 1;
    cpu.ex_mem.alu_out = 8;     // direccion
    cpu.ex_mem.rt_val = 1234;   // dato a guardar
    memory_access(&cpu);
    CHECK_EQ(dmem_read(&cpu, 8), 1234, "sw: escribe el dato en memoria");

    // lw: lee de memoria y lo deja en mem_wb.mem_out.
    cpu_reset(&cpu);
    dmem_write(&cpu, 12, 777);
    cpu.ex_mem.valid = true;
    cpu.ex_mem.ctrl.mem_read = 1;
    cpu.ex_mem.alu_out = 12;    // direccion
    cpu.ex_mem.dest_reg = 9;
    memory_access(&cpu);
    CHECK_EQ(cpu.mem_wb.mem_out, 777, "lw: carga el dato desde memoria");
    CHECK_EQ(cpu.mem_wb.dest_reg, 9, "MEM: propaga dest_reg");
    CHECK(cpu.mem_wb.valid, "MEM: marca valido el latch MEM/WB");

    // Instruccion sin acceso a memoria (add): solo propaga alu_out.
    cpu_reset(&cpu);
    cpu.ex_mem.valid = true;
    cpu.ex_mem.alu_out = 99;
    memory_access(&cpu);
    CHECK_EQ(cpu.mem_wb.alu_out, 99, "MEM: propaga alu_out cuando no hay acceso");
}

// ==========================================
// 5) UNIT TEST: write_back (WB)
// ==========================================
static void test_write_back(void) {
    printf(" Unit test: write_back (WB)\n");
    MIPSPipeline cpu;

    // Resultado de ALU (mem_to_reg=0) -> se escribe alu_out en el registro.
    cpu_reset(&cpu);
    cpu.mem_wb.valid = true;
    cpu.mem_wb.ctrl.reg_write = 1;
    cpu.mem_wb.ctrl.mem_to_reg = 0;
    cpu.mem_wb.dest_reg = 5;
    cpu.mem_wb.alu_out = 123;
    write_back(&cpu);
    CHECK_EQ(cpu.regs[5], 123, "WB: escribe alu_out cuando mem_to_reg=0");

    // Dato de memoria (mem_to_reg=1) -> se escribe mem_out.
    cpu_reset(&cpu);
    cpu.mem_wb.valid = true;
    cpu.mem_wb.ctrl.reg_write = 1;
    cpu.mem_wb.ctrl.mem_to_reg = 1;
    cpu.mem_wb.dest_reg = 6;
    cpu.mem_wb.mem_out = 456;
    cpu.mem_wb.alu_out = 999; // no debe usarse
    write_back(&cpu);
    CHECK_EQ(cpu.regs[6], 456, "WB: escribe mem_out cuando mem_to_reg=1");

    // Proteccion de $zero: nunca se escribe el registro 0.
    cpu_reset(&cpu);
    cpu.mem_wb.valid = true;
    cpu.mem_wb.ctrl.reg_write = 1;
    cpu.mem_wb.dest_reg = 0;
    cpu.mem_wb.alu_out = 777;
    write_back(&cpu);
    CHECK_EQ(cpu.regs[0], 0, "WB: $0 permanece en 0 (no se sobrescribe)");

    // reg_write=0 -> no se escribe nada.
    cpu_reset(&cpu);
    cpu.regs[7] = 42;
    cpu.mem_wb.valid = true;
    cpu.mem_wb.ctrl.reg_write = 0;
    cpu.mem_wb.dest_reg = 7;
    cpu.mem_wb.alu_out = 100;
    write_back(&cpu);
    CHECK_EQ(cpu.regs[7], 42, "WB: no escribe cuando reg_write=0");
}

// ==========================================
// MAIN DE LOS UNIT TESTS
// ==========================================
int main(void) {
    printf("=====================================================\n");
    printf("  UNIT TESTS del simulador MIPS (5 funciones)\n");
    printf("=====================================================\n");

    test_instruction_fetch();
    test_instruction_decode();
    test_alu_execute();
    test_memory_access();
    test_write_back();

    printf("=====================================================\n");
    printf("  Resultado: %d/%d aserciones OK", g_checks - g_fails, g_checks);
    if (g_fails == 0) {
        printf("  -> TODOS LOS UNIT TESTS PASARON\n");
    } else {
        printf("  -> %d FALLARON\n", g_fails);
    }
    printf("=====================================================\n");
    return g_fails == 0 ? 0 : 1;
}
