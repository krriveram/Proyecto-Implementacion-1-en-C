/* ===========================================================================
 * test_integracion.c -- Test del sistema con vectores de prueba
 * ---------------------------------------------------------------------------
 * Mientras los unit tests prueban cada etapa por separado, aqui se ejecuta el
 * PIPELINE COMPLETO sobre programas reales y se comparan los estados finales
 * (registros, memoria y codigo de error) contra los valores esperados.
 *
 * Cada vector de prueba declara:
 *   - el programa a ejecutar,
 *   - el contenido inicial de la memoria de datos,
 *   - los registros y posiciones de memoria esperados al terminar,
 *   - el codigo de error esperado.
 *
 * Plan de verificacion (un vector por riesgo/funcionalidad):
 *   V1 aritmetica y logica          V6 riesgos de datos encadenados
 *   V2 memoria (sw / lw)            V7 proteccion de $0
 *   V3 beq tomado y no tomado       V8 direccion de memoria invalida
 *   V4 bne en un bucle              V9 instruccion fuera de la ISA
 *   V5 saltos j y jr
 * ===========================================================================
 */
#include <stdio.h>
#include <string.h>

#include "mips.h"
#include "test_util.h"

/* --- Descripcion de un vector de prueba --------------------------------- */
typedef struct { uint8_t  reg; int32_t valor; } esp_reg_t;
typedef struct { uint32_t dir; int32_t valor; } esp_mem_t;

typedef struct {
    const char      *nombre;
    const uint32_t  *prog;
    uint32_t         n_prog;
    const esp_mem_t *mem_ini;   uint32_t n_mem_ini;
    const esp_reg_t *regs_esp;  uint32_t n_regs_esp;
    const esp_mem_t *mem_esp;   uint32_t n_mem_esp;
    mips_err_t       err_esp;
    uint64_t         max_ciclos;
} vector_t;

/* --- Ejecutor de un vector ---------------------------------------------- */
static void ejecutar_vector(const vector_t *v)
{
    mips_t   cpu;
    uint32_t i;
    char     desc[96];
    int      fallos_antes = g_fallos;

    CASO(v->nombre);

    mips_reset(&cpu);
    if (!mips_cargar_programa(&cpu, v->prog, v->n_prog)) {
        VERIFICAR(0, "el programa no cabe en la memoria de instrucciones");
        return;
    }
    for (i = 0; i < v->n_mem_ini; i++) {
        mips_escribir_dmem(&cpu, v->mem_ini[i].dir, v->mem_ini[i].valor);
    }

    mips_ejecutar(&cpu, v->max_ciclos);

    /* El codigo de error debe ser exactamente el previsto */
    snprintf(desc, sizeof desc, "codigo de error = %s", mips_err_str(v->err_esp));
    VERIFICAR_EQ(cpu.err, v->err_esp, desc);

    /* Ningun programa correcto debe agotar el limite de ciclos */
    if (v->err_esp == MIPS_OK) {
        VERIFICAR(cpu.ciclos < v->max_ciclos, "el programa termino sin agotar el limite de ciclos");
    }

    for (i = 0; i < v->n_regs_esp; i++) {
        snprintf(desc, sizeof desc, "$%u debe valer %d",
                 (unsigned)v->regs_esp[i].reg, (int)v->regs_esp[i].valor);
        VERIFICAR_EQ(cpu.regs[v->regs_esp[i].reg], v->regs_esp[i].valor, desc);
    }
    for (i = 0; i < v->n_mem_esp; i++) {
        snprintf(desc, sizeof desc, "M[%u] debe valer %d",
                 (unsigned)v->mem_esp[i].dir, (int)v->mem_esp[i].valor);
        VERIFICAR_EQ(mips_leer_dmem(&cpu, v->mem_esp[i].dir), v->mem_esp[i].valor, desc);
    }

    printf("  %-42s %s  (%u ciclos, %u instrucciones, %u burbujas, %u saltos)\n",
           v->nombre, (g_fallos == fallos_antes) ? "OK   " : "FALLO",
           (unsigned)cpu.ciclos, (unsigned)cpu.instr_retiradas,
           (unsigned)cpu.stalls, (unsigned)cpu.flushes);
}

/* ===========================================================================
 * Programas de prueba
 * ---------------------------------------------------------------------------
 * Se construyen con las funciones de codificacion (cod_r / cod_i / cod_j), que
 * no son constantes de compilacion, de modo que los arreglos se rellenan en
 * armar_programas() antes de ejecutar los vectores.
 * =========================================================================*/
static uint32_t p_alu[8];
static uint32_t p_mem[5];
static uint32_t p_beq[8];
static uint32_t p_bucle[5];
static uint32_t p_saltos[7];
static uint32_t p_riesgos[5];
static uint32_t p_cero[2];
static uint32_t p_dir_mala[2];
static uint32_t p_op_malo[2];

static void armar_programas(void)
{
    /* V1: aritmetica y logica.  $t0 = 12 (0b1100), $t1 = 10 (0b1010) */
    p_alu[0] = cod_i(OP_ADDI, R_ZERO, R_T0, 12);
    p_alu[1] = cod_i(OP_ADDI, R_ZERO, R_T1, 10);
    p_alu[2] = cod_r(FUNCT_ADD, R_T0, R_T1, R_T2);   /* 22  */
    p_alu[3] = cod_r(FUNCT_SUB, R_T0, R_T1, R_T3);   /* 2   */
    p_alu[4] = cod_r(FUNCT_AND, R_T0, R_T1, R_T4);   /* 8   */
    p_alu[5] = cod_r(FUNCT_OR,  R_T0, R_T1, R_T5);   /* 14  */
    p_alu[6] = cod_r(FUNCT_XOR, R_T0, R_T1, R_T6);   /* 6   */
    p_alu[7] = cod_r(FUNCT_NOR, R_T0, R_T1, R_T7);   /* -15 */

    /* V2: memoria.  Incluye el riesgo lw-uso y el cortocircuito del dato de sw */
    p_mem[0] = cod_i(OP_ADDI, R_ZERO, R_T0, 42);
    p_mem[1] = cod_i(OP_SW,   R_ZERO, R_T0, 16);     /* M[16] = 42        */
    p_mem[2] = cod_i(OP_LW,   R_ZERO, R_T1, 16);     /* $t1  = M[16] = 42 */
    p_mem[3] = cod_r(FUNCT_ADD, R_T1, R_T1, R_T2);   /* $t2  = 84 (burbuja lw-uso) */
    p_mem[4] = cod_i(OP_SW,   R_ZERO, R_T2, 20);     /* M[20] = 84        */

    /* V3: beq tomado (salta 2 instrucciones) y beq no tomado */
    p_beq[0] = cod_i(OP_ADDI, R_ZERO, R_T0, 5);
    p_beq[1] = cod_i(OP_ADDI, R_ZERO, R_T1, 5);
    p_beq[2] = cod_i(OP_BEQ,  R_T0, R_T1, 2);        /* 0x08 -> 0x14 */
    p_beq[3] = cod_i(OP_ADDI, R_ZERO, R_T2, 99);     /* anulada */
    p_beq[4] = cod_i(OP_ADDI, R_ZERO, R_T3, 99);     /* anulada */
    p_beq[5] = cod_i(OP_ADDI, R_ZERO, R_T4, 7);      /* 0x14: destino */
    p_beq[6] = cod_i(OP_BEQ,  R_T0, R_ZERO, 2);      /* no se toma (5 != 0) */
    p_beq[7] = cod_i(OP_ADDI, R_ZERO, R_T5, 8);      /* si se ejecuta */

    /* V4: bucle con bne -- cuenta de 1 a 5 y guarda el resultado */
    p_bucle[0] = cod_i(OP_ADDI, R_ZERO, R_T0, 0);    /* i = 0            */
    p_bucle[1] = cod_i(OP_ADDI, R_ZERO, R_T1, 5);    /* limite = 5       */
    p_bucle[2] = cod_i(OP_ADDI, R_T0, R_T0, 1);      /* 0x08: i = i + 1  */
    p_bucle[3] = cod_i(OP_BNE,  R_T0, R_T1, -2);     /* 0x0C -> 0x08     */
    p_bucle[4] = cod_i(OP_SW,   R_ZERO, R_T0, 0);    /* M[0] = i         */

    /* V5: j hacia una subrutina y jr para regresar */
    p_saltos[0] = cod_i(OP_ADDI, R_ZERO, R_RA, 24);  /* $ra = 0x18       */
    p_saltos[1] = cod_j(OP_J, 16);                   /* salta a 0x10     */
    p_saltos[2] = cod_i(OP_ADDI, R_ZERO, R_T0, 99);  /* anulada          */
    p_saltos[3] = cod_i(OP_ADDI, R_ZERO, R_T1, 99);  /* anulada          */
    p_saltos[4] = cod_i(OP_ADDI, R_ZERO, R_T2, 7);   /* 0x10: subrutina  */
    p_saltos[5] = cod_jr(R_RA);                      /* 0x14: retorno    */
    p_saltos[6] = cod_i(OP_ADDI, R_ZERO, R_T3, 5);   /* 0x18             */

    /* V6: dependencias encadenadas -> exige cortocircuito EX/MEM y MEM/WB */
    p_riesgos[0] = cod_i(OP_ADDI, R_ZERO, R_T0, 1);       /* 1 */
    p_riesgos[1] = cod_r(FUNCT_ADD, R_T0, R_T0, R_T1);    /* 2 */
    p_riesgos[2] = cod_r(FUNCT_ADD, R_T1, R_T0, R_T2);    /* 3 */
    p_riesgos[3] = cod_r(FUNCT_ADD, R_T2, R_T1, R_T3);    /* 5 */
    p_riesgos[4] = cod_r(FUNCT_ADD, R_T3, R_T2, R_T4);    /* 8 */

    /* V7: $0 es de solo lectura */
    p_cero[0] = cod_i(OP_ADDI, R_ZERO, R_ZERO, 99);  /* intenta escribir $0 */
    p_cero[1] = cod_i(OP_ADDI, R_ZERO, R_T0, 1);     /* debe dar 1, no 100  */

    /* V8: direccion de memoria fuera de rango */
    p_dir_mala[0] = cod_i(OP_ADDI, R_ZERO, R_T0, 1);
    p_dir_mala[1] = cod_i(OP_LW, R_ZERO, R_T1, 4000);

    /* V9: instruccion que no pertenece a la ISA (opcode 0x0E) */
    p_op_malo[0] = cod_i(OP_ADDI, R_ZERO, R_T0, 1);
    p_op_malo[1] = 0x38000000u;
}

/* --- Valores esperados --------------------------------------------------- */
static const esp_reg_t esp_alu[]     = { {R_T0,12}, {R_T1,10}, {R_T2,22}, {R_T3,2},
                                         {R_T4,8},  {R_T5,14}, {R_T6,6},  {R_T7,-15} };
static const esp_reg_t esp_mem_r[]   = { {R_T0,42}, {R_T1,42}, {R_T2,84} };
static const esp_mem_t esp_mem_m[]   = { {16,42}, {20,84} };
static const esp_reg_t esp_beq[]     = { {R_T0,5}, {R_T1,5}, {R_T2,0}, {R_T3,0},
                                         {R_T4,7}, {R_T5,8} };
static const esp_reg_t esp_bucle_r[] = { {R_T0,5}, {R_T1,5} };
static const esp_mem_t esp_bucle_m[] = { {0,5} };
static const esp_reg_t esp_saltos[]  = { {R_RA,24}, {R_T0,0}, {R_T1,0}, {R_T2,7}, {R_T3,5} };
static const esp_reg_t esp_riesgos[] = { {R_T0,1}, {R_T1,2}, {R_T2,3}, {R_T3,5}, {R_T4,8} };
static const esp_reg_t esp_cero[]    = { {R_ZERO,0}, {R_T0,1} };
/* V8: el error se detecta en MEM, en el mismo ciclo en que el addi anterior
 * llega a WB. Como WB se ejecuta al inicio del ciclo, $t0 alcanza a escribirse. */
static const esp_reg_t esp_dir[]     = { {R_T0,1} };
/* V9: el error se detecta en ID, dos ciclos antes de que el addi anterior
 * llegue a WB. El simulador detiene el reloj de inmediato, asi que esa
 * instruccion en vuelo ya no se retira y $t0 se queda en 0. Es la consecuencia
 * directa de no implementar excepciones precisas (ver README). */
static const esp_reg_t esp_op[]      = { {R_T0,0} };

#define N(x) ((uint32_t)(sizeof (x) / sizeof (x)[0]))

/* ===========================================================================
 * Pruebas directas de las unidades de deteccion de riesgos
 * =========================================================================*/
static void probar_unidad_riesgos(void)
{
    id_ex_t  en_ex;
    if_id_t  en_id;

    CASO("unidad de riesgos (lw-uso)");

    memset(&en_ex, 0, sizeof en_ex);
    memset(&en_id, 0, sizeof en_id);
    en_ex.valido = true;
    en_ex.rt     = R_T1;
    id_unidad_control(OP_LW, 0, &en_ex.ctrl);   /* lw $t1, ... esta en EX */
    en_id.valido = true;

    en_id.instr = cod_r(FUNCT_ADD, R_T1, R_T0, R_T2);   /* usa $t1 como rs */
    VERIFICAR(unidad_riesgos(&en_ex, &en_id), "add que lee el destino del lw exige burbuja");

    en_id.instr = cod_r(FUNCT_ADD, R_T0, R_T1, R_T2);   /* usa $t1 como rt */
    VERIFICAR(unidad_riesgos(&en_ex, &en_id), "el riesgo tambien se detecta en rt");

    en_id.instr = cod_i(OP_ADDI, R_T0, R_T1, 4);        /* addi no lee rt  */
    VERIFICAR(!unidad_riesgos(&en_ex, &en_id), "addi no lee rt: no hay riesgo");

    en_id.instr = cod_r(FUNCT_ADD, R_T3, R_T4, R_T5);   /* sin dependencia */
    VERIFICAR(!unidad_riesgos(&en_ex, &en_id), "sin dependencia no hay burbuja");

    id_unidad_control(OP_RTIPO, FUNCT_ADD, &en_ex.ctrl); /* ahora hay un add en EX */
    en_ex.rd    = R_T1;
    en_id.instr = cod_r(FUNCT_ADD, R_T1, R_T0, R_T2);
    VERIFICAR(!unidad_riesgos(&en_ex, &en_id),
              "un add en EX se resuelve con cortocircuito, sin burbuja");
}

static void probar_unidad_cortocircuito(void)
{
    id_ex_t   en_ex;
    ex_mem_t  en_mem;
    mem_wb_t  en_wb;
    fwd_sel_t a, b;

    CASO("unidad de cortocircuito");

    memset(&en_ex,  0, sizeof en_ex);
    memset(&en_mem, 0, sizeof en_mem);
    memset(&en_wb,  0, sizeof en_wb);
    en_ex.valido = true;  en_ex.rs = R_T0;  en_ex.rt = R_T1;

    en_mem.valido  = true; en_mem.reg_dst = R_T0;
    id_unidad_control(OP_RTIPO, FUNCT_ADD, &en_mem.ctrl);
    unidad_cortocircuito(&en_ex, &en_mem, &en_wb, &a, &b);
    VERIFICAR_EQ(a, FWD_EX_MEM,  "rs se toma de EX/MEM");
    VERIFICAR_EQ(b, FWD_NINGUNO, "rt no tiene productor");

    en_wb.valido  = true; en_wb.reg_dst = R_T1;
    id_unidad_control(OP_RTIPO, FUNCT_ADD, &en_wb.ctrl);
    unidad_cortocircuito(&en_ex, &en_mem, &en_wb, &a, &b);
    VERIFICAR_EQ(b, FWD_MEM_WB, "rt se toma de MEM/WB");

    en_wb.reg_dst = R_T0;   /* los dos productores escriben $t0 */
    unidad_cortocircuito(&en_ex, &en_mem, &en_wb, &a, &b);
    VERIFICAR_EQ(a, FWD_EX_MEM, "gana el productor mas reciente (EX/MEM)");

    en_mem.reg_dst = R_ZERO;
    en_wb.reg_dst  = R_ZERO;
    unidad_cortocircuito(&en_ex, &en_mem, &en_wb, &a, &b);
    VERIFICAR_EQ(a, FWD_NINGUNO, "escribir en $0 nunca genera cortocircuito");

    /* Un lw en MEM no puede ser fuente: su dato aun no existe */
    en_mem.reg_dst = R_T0;
    id_unidad_control(OP_LW, 0, &en_mem.ctrl);
    en_wb.valido = false;
    unidad_cortocircuito(&en_ex, &en_mem, &en_wb, &a, &b);
    VERIFICAR_EQ(a, FWD_NINGUNO, "un lw en MEM no se cortocircuita (lo cubre la burbuja)");
}

int main(void)
{
    uint32_t i;
    vector_t vectores[9];

    printf("== Test del sistema: vectores de prueba sobre el pipeline completo ==\n\n");

    armar_programas();

    memset(vectores, 0, sizeof vectores);

    vectores[0].nombre     = "V1 aritmetica y logica";
    vectores[0].prog       = p_alu;      vectores[0].n_prog     = N(p_alu);
    vectores[0].regs_esp   = esp_alu;    vectores[0].n_regs_esp = N(esp_alu);
    vectores[0].max_ciclos = 100;

    vectores[1].nombre     = "V2 memoria (sw / lw + riesgo lw-uso)";
    vectores[1].prog       = p_mem;      vectores[1].n_prog     = N(p_mem);
    vectores[1].regs_esp   = esp_mem_r;  vectores[1].n_regs_esp = N(esp_mem_r);
    vectores[1].mem_esp    = esp_mem_m;  vectores[1].n_mem_esp  = N(esp_mem_m);
    vectores[1].max_ciclos = 100;

    vectores[2].nombre     = "V3 beq tomado y no tomado";
    vectores[2].prog       = p_beq;      vectores[2].n_prog     = N(p_beq);
    vectores[2].regs_esp   = esp_beq;    vectores[2].n_regs_esp = N(esp_beq);
    vectores[2].max_ciclos = 100;

    vectores[3].nombre     = "V4 bucle con bne";
    vectores[3].prog       = p_bucle;    vectores[3].n_prog     = N(p_bucle);
    vectores[3].regs_esp   = esp_bucle_r;vectores[3].n_regs_esp = N(esp_bucle_r);
    vectores[3].mem_esp    = esp_bucle_m;vectores[3].n_mem_esp  = N(esp_bucle_m);
    vectores[3].max_ciclos = 200;

    vectores[4].nombre     = "V5 saltos j y jr";
    vectores[4].prog       = p_saltos;   vectores[4].n_prog     = N(p_saltos);
    vectores[4].regs_esp   = esp_saltos; vectores[4].n_regs_esp = N(esp_saltos);
    vectores[4].max_ciclos = 100;

    vectores[5].nombre     = "V6 riesgos de datos encadenados";
    vectores[5].prog       = p_riesgos;  vectores[5].n_prog     = N(p_riesgos);
    vectores[5].regs_esp   = esp_riesgos;vectores[5].n_regs_esp = N(esp_riesgos);
    vectores[5].max_ciclos = 100;

    vectores[6].nombre     = "V7 proteccion de $0";
    vectores[6].prog       = p_cero;     vectores[6].n_prog     = N(p_cero);
    vectores[6].regs_esp   = esp_cero;   vectores[6].n_regs_esp = N(esp_cero);
    vectores[6].max_ciclos = 100;

    vectores[7].nombre     = "V8 direccion de memoria invalida";
    vectores[7].prog       = p_dir_mala; vectores[7].n_prog     = N(p_dir_mala);
    vectores[7].regs_esp   = esp_dir;    vectores[7].n_regs_esp = N(esp_dir);
    vectores[7].err_esp    = MIPS_ERR_DIR_RANGO;
    vectores[7].max_ciclos = 100;

    vectores[8].nombre     = "V9 instruccion fuera de la ISA";
    vectores[8].prog       = p_op_malo;  vectores[8].n_prog     = N(p_op_malo);
    vectores[8].regs_esp   = esp_op;     vectores[8].n_regs_esp = N(esp_op);
    vectores[8].err_esp    = MIPS_ERR_OPCODE;
    vectores[8].max_ciclos = 100;

    for (i = 0; i < N(vectores); i++) {
        ejecutar_vector(&vectores[i]);
    }

    printf("\n");
    probar_unidad_riesgos();
    probar_unidad_cortocircuito();
    printf("\n");

    return RESUMEN("TEST DEL SISTEMA");
}
