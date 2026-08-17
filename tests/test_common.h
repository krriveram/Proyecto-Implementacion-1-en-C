/* test_common.h -- arnes de pruebas minimalista (sin dependencias externas) */
#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#include <stdio.h>

static int _tests_ok = 0;
static int _tests_fail = 0;

#define ASSERT_EQ_INT(desc, esperado, obtenido) do { \
    long long _e = (long long)(esperado), _o = (long long)(obtenido); \
    if (_e == _o) { _tests_ok++; } \
    else { _tests_fail++; \
        printf("  [FALLO] %s: esperado=%lld obtenido=%lld (linea %d)\n", \
               desc, _e, _o, __LINE__); } \
} while (0)

#define ASSERT_TRUE(desc, cond) do { \
    if (cond) { _tests_ok++; } \
    else { _tests_fail++; \
        printf("  [FALLO] %s: se esperaba TRUE (linea %d)\n", desc, __LINE__); } \
} while (0)

#define RESUMEN_TESTS(nombre_suite) do { \
    printf("--- %s: %d OK, %d FALLOS ---\n", nombre_suite, _tests_ok, _tests_fail); \
    return _tests_fail == 0 ? 0 : 1; \
} while (0)

#endif
