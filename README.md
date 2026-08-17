# Simulador MIPS de 5 etapas en C

Implementación en C de un simulador funcional de un procesador MIPS con
sus 5 etapas clásicas (IF, ID, EX, MEM, WB), cada una expuesta como una
función independiente y testeable de forma aislada.

## ISA soportada (13 instrucciones)

- **Tipo R:** `add`, `sub`, `and`, `or`, `slt`, `sll`
- **Tipo I:** `addi`, `lw`, `sw`, `beq`, `bne`
- **Tipo J:** `j`, `jal`

## Estructura del repositorio

```
include/mips.h          Contrato completo: tipos, structs, prototipos
src/etapa_if.c           Etapa 1: Instruction Fetch
src/etapa_id.c            Etapa 2: Instruction Decode + unidad de control
src/etapa_alu.c            Etapa 3: Execute / ALU
src/etapa_mem.c             Etapa 4: Memory Access
src/etapa_wb.c                Etapa 5: Write Back
src/mips_core.c                Encadena las 5 etapas (cpu_step / cpu_run)
src/main.c                      Programa demo ejecutable
tests/test_etapa_*.c              Unit tests (uno por etapa)
tests/test_integracion.c            Test integral con 4 vectores de prueba
tests/test_common.h                  Arnés de testing propio (sin dependencias)
docs/diagrama_sistema.md              Datapath completo (Mermaid)
docs/diagrama_subfunciones.md          Subfunciones de cada etapa
```

## Compilar y correr

```bash
make sim        # compila el simulador -> build/mips_sim
./build/mips_sim

make test        # compila y corre todos los unit tests + integración
make clean         # limpia build/
```

## Diseño

Cada etapa recibe una struct de entrada y devuelve una struct de salida
explícita (`if_out_t`, `id_out_t`, `ex_out_t`, `mem_out_t`), lo que
permite probarlas sin necesidad de correr el CPU completo. `mips_core.c`
las encadena instrucción por instrucción (modelo funcional multi-ciclo,
no pipeline solapado con hazards).

Ver `docs/diagrama_sistema.md` y `docs/diagrama_subfunciones.md` para el
detalle del datapath y las tablas de señales de control.
