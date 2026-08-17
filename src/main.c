/* ===========================================================================
 * main.c -- Demostracion del simulador
 * ---------------------------------------------------------------------------
 * Ejecuta dos programas de ejemplo sobre el pipeline completo:
 *   1. Suma de un arreglo de 5 palabras con un bucle (lw, add, addi, bne, sw).
 *      Ejercita el riesgo lw-uso y el cortocircuito.
 *   2. Llamada y retorno con j y jr (saltos incondicionales).
 *
 * Uso:  mips_sim [--traza]
 *       --traza imprime el estado del pipeline ciclo por ciclo.
 * ===========================================================================
 */
#include <stdio.h>
#include <string.h>

#include "mips.h"

static void imprimir_programa(const uint32_t *prog, uint32_t n)
{
    uint32_t i;
    char     texto[48];

    printf("  Programa cargado:\n");
    for (i = 0; i < n; i++) {
        mips_desensamblar(prog[i], texto, sizeof texto);
        printf("    0x%04X:  0x%08X   %s\n", (unsigned)(i * 4),
               (unsigned)prog[i], texto);
    }
}

/* --- Demo 1: suma de un arreglo de 5 palabras --------------------------- */
static void demo_suma_arreglo(bool traza)
{
    mips_t   cpu;
    uint32_t i;
    const int32_t datos[5] = { 10, 20, 30, 40, 50 };

    const uint32_t prog[] = {
        /* 0x00 */ cod_i(OP_ADDI, R_ZERO, R_T0, 0),    /* $t0 = 0   (suma)     */
        /* 0x04 */ cod_i(OP_ADDI, R_ZERO, R_T1, 0),    /* $t1 = 0   (puntero)  */
        /* 0x08 */ cod_i(OP_ADDI, R_ZERO, R_T2, 20),   /* $t2 = 20  (fin)      */
        /* 0x0C */ cod_i(OP_LW,   R_T1,   R_T3, 0),    /* bucle: $t3 = M[$t1]  */
        /* 0x10 */ cod_r(FUNCT_ADD, R_T0, R_T3, R_T0), /* $t0 = $t0 + $t3      */
        /* 0x14 */ cod_i(OP_ADDI, R_T1,   R_T1, 4),    /* $t1 += 4             */
        /* 0x18 */ cod_i(OP_BNE,  R_T1,   R_T2, -4),   /* si $t1 != 20 -> 0x0C */
        /* 0x1C */ cod_i(OP_SW,   R_ZERO, R_T0, 32)    /* M[32] = $t0          */
    };

    printf("\n===============================================================\n");
    printf(" DEMO 1: suma de un arreglo de 5 palabras (bucle con lw/add/bne)\n");
    printf("===============================================================\n");

    mips_reset(&cpu);
    mips_cargar_programa(&cpu, prog, (uint32_t)(sizeof prog / sizeof prog[0]));
    for (i = 0; i < 5; i++) {
        mips_escribir_dmem(&cpu, i * 4u, datos[i]);
    }
    cpu.traza = traza;

    imprimir_programa(prog, (uint32_t)(sizeof prog / sizeof prog[0]));
    printf("  Datos en memoria: M[0..16] = 10, 20, 30, 40, 50\n\n");

    mips_ejecutar(&cpu, 200);

    mips_volcado(&cpu);
    printf("  Resultado en memoria: M[32] = %d  (esperado 150)\n",
           (int)mips_leer_dmem(&cpu, 32));
}

/* --- Demo 2: llamada y retorno con j / jr ------------------------------- */
static void demo_saltos(bool traza)
{
    mips_t cpu;

    const uint32_t prog[] = {
        /* 0x00 */ cod_i(OP_ADDI, R_ZERO, R_RA, 24),  /* $ra = 0x18 (retorno) */
        /* 0x04 */ cod_j(OP_J, 16),                   /* salta a 0x10         */
        /* 0x08 */ cod_i(OP_ADDI, R_ZERO, R_T0, 99),  /* anulada por el salto */
        /* 0x0C */ cod_i(OP_ADDI, R_ZERO, R_T1, 99),  /* anulada por el salto */
        /* 0x10 */ cod_i(OP_ADDI, R_ZERO, R_T2, 7),   /* subrutina: $t2 = 7   */
        /* 0x14 */ cod_jr(R_RA),                      /* regresa a 0x18       */
        /* 0x18 */ cod_i(OP_ADDI, R_ZERO, R_T3, 5)    /* $t3 = 5              */
    };

    printf("\n===============================================================\n");
    printf(" DEMO 2: salto incondicional (j) y retorno por registro (jr)\n");
    printf("===============================================================\n");

    mips_reset(&cpu);
    mips_cargar_programa(&cpu, prog, (uint32_t)(sizeof prog / sizeof prog[0]));
    cpu.traza = traza;

    imprimir_programa(prog, (uint32_t)(sizeof prog / sizeof prog[0]));
    printf("\n");

    mips_ejecutar(&cpu, 200);

    mips_volcado(&cpu);
    printf("  $t0 y $t1 deben seguir en 0 (instrucciones anuladas por el salto)\n");
}

int main(int argc, char **argv)
{
    bool traza = false;
    int  i;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--traza") == 0) {
            traza = true;
        }
    }

    printf("===============================================================\n");
    printf(" Simulador MIPS con pipeline de 5 etapas -- demostracion\n");
    printf(" (ejecutar con --traza para ver el pipeline ciclo por ciclo)\n");
    printf("===============================================================\n");

    demo_suma_arreglo(traza);
    demo_saltos(traza);

    printf("\n");
    return 0;
}
