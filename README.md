# Proyecto-Implementacion-1-en-C

Implementación en C de un simulador del procesador **MIPS segmentado** (pipeline
de 5 etapas: IF, ID, EX, MEM, WB) con sus subfunciones, **unit tests** por
función y un **test integral con vectores de prueba**.

Cada branch contiene el código individual realizado por cada miembro del grupo,
etiquetado con el apellido de quien lo realizó. El código en `main` es el
consenso al que llegó el grupo para la implementación.

## Estructura del repositorio

| Archivo | Descripción |
|---------|-------------|
| `mips_sim.c` | Simulador MIPS: las 5 etapas del pipeline + test integral (vectores de prueba). |
| `test_unit.c` | Unit tests que prueban cada una de las 5 funciones por separado. |
| `Makefile` | Reglas de compilación y ejecución de pruebas. |
| `docs/DIAGRAMAS.md` | Diagramas del pipeline y del manejo de riesgos (hazards). |
| `docs/pipeline.svg`, `docs/hazards.svg` | Versiones exportadas de los diagramas. |

## Las 5 funciones (etapas del pipeline)

1. `instruction_fetch` — IF: lee la instrucción de memoria y avanza el PC.
2. `instruction_decode` — ID: decodifica campos, genera señales de control y detecta load-use.
3. `alu_execute` — EX: operación de la ALU, forwarding y resolución de `beq`/`bne`.
4. `memory_access` — MEM: acceso a memoria de datos (`lw`/`sw`).
5. `write_back` — WB: escribe el resultado en el registro destino.

## Manejo de riesgos (hazards)

- **Riesgos de datos (forwarding):** los operandos de la ALU se adelantan desde
  `EX/MEM` (1 instrucción adelante) y desde el banco de registros ya actualizado
  (instrucciones 2 y 3 adelante, porque `write_back()` corre antes que la etapa
  EX dentro de `cpu_step()`).
- **Riesgo carga-uso (load-use):** un `lw` seguido de una instrucción que usa el
  registro cargado se detecta en ID y se inserta una burbuja de 1 ciclo.
- **Riesgos de control (flush):** los `beq`/`bne` se resuelven en EX; si el salto
  se toma, se hace flush de IF/ID y se redirige el PC. Los saltos `j`/`jr` se
  resuelven en ID.

Ver los diagramas en [`docs/DIAGRAMAS.md`](docs/DIAGRAMAS.md).

## Compilar y ejecutar

```bash
make            # compila el simulador (mips_sim)
make test       # test integral: ejecuta los vectores de prueba
make unit-run   # unit tests: prueba las 5 funciones por separado
make check      # ejecuta TODO (unit tests + test integral)
make clean      # elimina los binarios generados
```

Salida esperada:

- `make unit-run` → `TODOS LOS UNIT TESTS PASARON`.
- `make test` → las tres suites (ALU, Memoria, Control de Flujo) reportan `OK`.
