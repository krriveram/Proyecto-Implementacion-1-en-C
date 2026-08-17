/* ===========================================================================
 * mips_isa.c -- Ensamblador minimo y desensamblador
 * ---------------------------------------------------------------------------
 * No forma parte de las 5 etapas del pipeline: son utilidades para construir
 * programas de prueba de forma legible (en vez de escribir constantes hex a
 * mano) y para imprimir trazas entendibles.
 * ===========================================================================
 */
#include <stdio.h>

#include "mips.h"

const char *mips_err_str(mips_err_t e)
{
    switch (e) {
    case MIPS_OK:                   return "OK";
    case MIPS_ERR_OPCODE:           return "instruccion no soportada por la ISA";
    case MIPS_ERR_DIR_RANGO:        return "direccion fuera de rango";
    case MIPS_ERR_DIR_ALINEACION:   return "direccion no alineada a 4 bytes";
    default:                        return "error desconocido";
    }
}

/* Tipo R: opcode=0 | rs | rt | rd | shamt=0 | funct */
uint32_t cod_r(uint8_t funct, uint8_t rs, uint8_t rt, uint8_t rd)
{
    return ((uint32_t)(rs    & 0x1Fu) << 21)
         | ((uint32_t)(rt    & 0x1Fu) << 16)
         | ((uint32_t)(rd    & 0x1Fu) << 11)
         |  (uint32_t)(funct & 0x3Fu);
}

/* Tipo I: opcode | rs | rt | inmediato de 16 bits */
uint32_t cod_i(uint8_t opcode, uint8_t rs, uint8_t rt, int16_t imm)
{
    return ((uint32_t)(opcode & 0x3Fu) << 26)
         | ((uint32_t)(rs     & 0x1Fu) << 21)
         | ((uint32_t)(rt     & 0x1Fu) << 16)
         |  ((uint32_t)(uint16_t)imm);
}

/* Tipo J: opcode | direccion de 26 bits (la direccion de byte se divide en 4) */
uint32_t cod_j(uint8_t opcode, uint32_t destino_byte)
{
    return ((uint32_t)(opcode & 0x3Fu) << 26)
         | ((destino_byte >> 2) & 0x03FFFFFFu);
}

uint32_t cod_jr(uint8_t rs)
{
    return cod_r(FUNCT_JR, rs, 0, 0);
}

uint32_t cod_nop(void)
{
    return MIPS_NOP;
}

/* Desensamblado en texto; deja siempre la cadena terminada en '\0'. */
void mips_desensamblar(uint32_t instr, char *buf, size_t n)
{
    uint8_t  op    = id_campo_opcode(instr);
    uint8_t  rs    = id_campo_rs(instr);
    uint8_t  rt    = id_campo_rt(instr);
    uint8_t  rd    = id_campo_rd(instr);
    uint8_t  funct = id_campo_funct(instr);
    int32_t  imm   = id_extension_signo(id_campo_imm16(instr));

    if (buf == NULL || n == 0) {
        return;
    }
    if (instr == MIPS_NOP) {
        snprintf(buf, n, "nop");
        return;
    }

    switch (op) {
    case OP_RTIPO: {
        const char *nombre = NULL;
        switch (funct) {
        case FUNCT_ADD: nombre = "add"; break;
        case FUNCT_SUB: nombre = "sub"; break;
        case FUNCT_AND: nombre = "and"; break;
        case FUNCT_OR:  nombre = "or";  break;
        case FUNCT_XOR: nombre = "xor"; break;
        case FUNCT_NOR: nombre = "nor"; break;
        case FUNCT_JR:  snprintf(buf, n, "jr   $%u", (unsigned)rs); return;
        default:        snprintf(buf, n, "??? (funct=0x%02X)", (unsigned)funct); return;
        }
        snprintf(buf, n, "%-4s $%u, $%u, $%u", nombre, (unsigned)rd,
                 (unsigned)rs, (unsigned)rt);
        return;
    }
    case OP_ADDI: snprintf(buf, n, "addi $%u, $%u, %d", (unsigned)rt, (unsigned)rs, (int)imm); return;
    case OP_LW:   snprintf(buf, n, "lw   $%u, %d($%u)", (unsigned)rt, (int)imm, (unsigned)rs); return;
    case OP_SW:   snprintf(buf, n, "sw   $%u, %d($%u)", (unsigned)rt, (int)imm, (unsigned)rs); return;
    case OP_BEQ:  snprintf(buf, n, "beq  $%u, $%u, %d", (unsigned)rs, (unsigned)rt, (int)imm); return;
    case OP_BNE:  snprintf(buf, n, "bne  $%u, $%u, %d", (unsigned)rs, (unsigned)rt, (int)imm); return;
    case OP_J:    snprintf(buf, n, "j    0x%08X", (unsigned)(id_campo_addr26(instr) << 2)); return;
    default:      snprintf(buf, n, "??? (op=0x%02X)", (unsigned)op); return;
    }
}
