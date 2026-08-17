#include "../include/mips.h"
#include "test_common.h"

int main(void) {
    uint32_t imem[IMEM_WORDS] = {0};
    imem[0] = 0xAABBCCDD;
    imem[1] = 0x11223344;

    if_out_t out;

    ASSERT_TRUE("fetch pc=0 ok", etapa_if(0, imem, &out));
    ASSERT_EQ_INT("instr en pc=0", 0xAABBCCDD, out.instr);
    ASSERT_EQ_INT("pc_siguiente = pc+4", 4, out.pc_siguiente);

    ASSERT_TRUE("fetch pc=4 ok", etapa_if(4, imem, &out));
    ASSERT_EQ_INT("instr en pc=4", 0x11223344, out.instr);

    /* pc fuera de rango */
    bool ok = etapa_if(IMEM_WORDS * 4, imem, &out);
    ASSERT_TRUE("pc fuera de rango devuelve false", !ok);

    /* punteros nulos no deben crashear */
    ASSERT_TRUE("out NULL devuelve false", !etapa_if(0, imem, NULL));

    RESUMEN_TESTS("test_etapa_if");
}
