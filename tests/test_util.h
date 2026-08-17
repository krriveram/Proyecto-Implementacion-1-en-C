/* ===========================================================================
 * test_util.h -- Micro framework de pruebas
 * ---------------------------------------------------------------------------
 * Deliberadamente NO usa assert(): un assert aborta en el primer fallo y
 * esconde el resto de los problemas. Aqui cada verificacion que falla se
 * imprime con archivo, linea, valor esperado y valor obtenido, y el programa
 * continua. El codigo de salida es 0 solo si no hubo ningun fallo, de modo que
 * "make test" se detiene automaticamente cuando algo se rompe.
 * ===========================================================================
 */
#ifndef TEST_UTIL_H
#define TEST_UTIL_H

#include <stdio.h>

static int         g_pruebas = 0;
static int         g_fallos  = 0;
static const char *g_caso    = "(sin nombre)";

/* Etiqueta el caso de prueba en curso (aparece en los mensajes de fallo). */
#define CASO(nombre)  do { g_caso = (nombre); } while (0)

/* Verificacion de una condicion booleana. */
#define VERIFICAR(cond, desc)                                                \
    do {                                                                     \
        g_pruebas++;                                                         \
        if (!(cond)) {                                                       \
            g_fallos++;                                                      \
            printf("  [FALLO] %s :: %s\n", g_caso, (desc));                  \
            printf("          condicion falsa: %s   (%s:%d)\n",              \
                   #cond, __FILE__, __LINE__);                               \
        }                                                                    \
    } while (0)

/* Verificacion de igualdad numerica con reporte de ambos valores. */
#define VERIFICAR_EQ(obtenido, esperado, desc)                               \
    do {                                                                     \
        long _obt = (long)(obtenido);                                        \
        long _esp = (long)(esperado);                                        \
        g_pruebas++;                                                         \
        if (_obt != _esp) {                                                  \
            g_fallos++;                                                      \
            printf("  [FALLO] %s :: %s\n", g_caso, (desc));                  \
            printf("          esperado = %ld, obtenido = %ld   (%s:%d)\n",   \
                   _esp, _obt, __FILE__, __LINE__);                          \
        }                                                                    \
    } while (0)

/* Resumen final; se usa como valor de retorno de main(). */
#define RESUMEN(modulo)                                                      \
    (printf("%-34s %3d verificaciones, %d fallos  -> %s\n",                  \
            (modulo), g_pruebas, g_fallos, g_fallos ? "FALLO" : "OK"),       \
     g_fallos ? 1 : 0)

#endif /* TEST_UTIL_H */
