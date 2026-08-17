#include "../include/mips.h"
#include "test_common.h"

int main(void) {
    int32_t dmem[DMEM_WORDS] = {0};
    senales_control_t ctrl = {0};
    mem_out_t out;

    /* sw: escribe valor_rt en la direccion alu_result */
    ctrl.mem_write = true;
    etapa_mem(&ctrl, 8 /* direccion byte 8 -> palabra 2 */, 42, dmem, &out);
    ASSERT_EQ_INT("sw escribe en palabra 2", 42, dmem[2]);

    /* lw: lee de esa misma direccion */
    ctrl = (senales_control_t){0};
    ctrl.mem_read = true;
    etapa_mem(&ctrl, 8, 0, dmem, &out);
    ASSERT_EQ_INT("lw lee 42", 42, out.dato_leido);

    /* instruccion que no toca memoria (add) -> dato_leido queda en 0, no toca dmem */
    ctrl = (senales_control_t){0};
    dmem[3] = 999;
    etapa_mem(&ctrl, 12, 0, dmem, &out);
    ASSERT_EQ_INT("sin mem_read/write, dato_leido=0", 0, out.dato_leido);
    ASSERT_EQ_INT("dmem no se modifica", 999, dmem[3]);

    /* direccion negativa -> error */
    ctrl.mem_read = true;
    bool ok = etapa_mem(&ctrl, -4, 0, dmem, &out);
    ASSERT_TRUE("direccion negativa falla", !ok);

    /* direccion fuera de rango -> error */
    ok = etapa_mem(&ctrl, DMEM_WORDS * 4, 0, dmem, &out);
    ASSERT_TRUE("direccion fuera de rango falla", !ok);

    RESUMEN_TESTS("test_etapa_mem");
}
