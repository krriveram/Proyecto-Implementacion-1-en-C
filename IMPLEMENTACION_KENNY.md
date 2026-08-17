# Simulador MIPS con pipeline de 5 etapas

Rama `implementacion_kenny`. Esta es mi versión individual de lo que planificamos
en el Proyecto Parte 1: las cinco funciones que corresponden a las etapas del
pipeline, un unit test para cada una y un test del sistema con vectores de prueba.

## Cómo correrlo

```bash
make            # compila el simulador y los ejecutables de prueba
make test       # corre los 5 unit tests y después el test del sistema
make run        # demo: suma de un arreglo y una llamada con j/jr
make traza      # la misma demo pero mostrando el pipeline ciclo por ciclo
make clean
```

En Windows es `mingw32-make`; el Makefile detecta el sistema y ajusta el `.exe`,
el comando de borrado y la forma de invocar los binarios. También hay
`make unit` y `make integracion` si solo quieres una de las dos tandas.

Compila con `-std=c99 -Wall -Wextra -Wpedantic -O2` sin advertencias.

## Dónde está cada cosa

```
include/mips.h            todos los tipos y prototipos: es el contrato del simulador
src/etapa_if.c            1. Instruction Fetch + Program Counter
src/etapa_id.c            2. Instruction Decode
src/etapa_alu.c           3. ALU
src/etapa_mem.c           4. Memory
src/etapa_wb.c            5. Write Back
src/mips_core.c           el reloj: conecta las cinco etapas y maneja los riesgos
src/mips_isa.c            ensamblador mínimo y desensamblador, para armar pruebas
src/main.c                la demo
tests/                    un test por etapa, el test del sistema y el mini framework
docs/                     los dos diagramas
```

## Las cinco funciones

Cada etapa recibe un struct de entrada y llena uno de salida. No tocan variables
globales ni el estado de la CPU directamente (salvo `etapa_mem`, que sí escribe
en la memoria de datos porque ese es literalmente su trabajo).

Esa fue la decisión más útil de todo el proyecto: gracias a eso, cada unit test
arma la entrada a mano y revisa la salida sin necesidad de arrancar el
procesador completo. Probar `etapa_alu` con un cortocircuito activo, por ejemplo,
son tres líneas.

| Función | Recibe | Entrega |
|---|---|---|
| `etapa_if` | `pc`, `imem`, `pc_src`, `pc_objetivo`, `stall`, `flush` | `pc_siguiente`, IF/ID, `cargar_if_id`, `err` |
| `etapa_id` | IF/ID, banco de registros, `stall`, `flush` | ID/EX, `err` |
| `etapa_alu` | ID/EX, selectores de cortocircuito y sus datos | EX/MEM, `pc_src`, `pc_objetivo`, `cero` |
| `etapa_mem` | EX/MEM, memoria de datos | MEM/WB, `err` |
| `etapa_wb` | MEM/WB | `reg_write`, `reg_dst`, `dato` |

El desglose con el número de bits e índice de cada señal está en
[docs/diagrama_sistema.md](docs/diagrama_sistema.md), y las subfunciones internas
de cada etapa en [docs/diagrama_subfunciones.md](docs/diagrama_subfunciones.md).

Las 13 instrucciones son las que pedía la consigna: `add`, `sub`, `and`, `or`,
`nor`, `xor` y `jr` de tipo R; `addi`, `lw`, `sw`, `beq` y `bne` de tipo I; y `j`.

## Por qué está hecho así

**El ciclo tiene dos fases.** `mips_ciclo()` primero evalúa las cinco etapas
leyendo únicamente el valor *actual* de los registros de pipeline, y recién al
final los actualiza todos juntos. Es lo que hacen los flip-flops reales, y tiene
una ventaja práctica: el orden en que llame a las etapas deja de importar, así
que no hay forma de introducir un bug sutil por reordenarlas.

**El banco de registros escribe en la primera mitad del ciclo.** WB corre primero
y su escritura se aplica antes de que ID lea. Con eso, una instrucción que
depende de otra tres posiciones atrás ya no necesita nada especial: el dato
correcto está ahí cuando lo va a buscar.

**Todos los saltos se resuelven en la ALU.** `beq`, `bne`, `j` y `jr` salen por
la misma señal `pc_src`. Podría haber resuelto `j` antes, en ID, y ahorrarme un
ciclo, pero habría terminado con tres rutas de control distintas que verificar
en lugar de una. Prefiero pagar dos ciclos fijos por salto tomado y tener una
sola regla. De yapa, `jr` hereda el cortocircuito gratis porque su destino sale
de la ALU.

**Toda la decodificación vive en ID.** La etapa 3 recibe un `alu_op` ya resuelto
y nunca vuelve a mirar el campo `funct`. Si mañana agregamos una instrucción,
hay un solo lugar que tocar.

**La aritmética se hace sobre `uint32_t`.** En C el desbordamiento con signo es
comportamiento indefinido, mientras que el sin signo está definido como
aritmética módulo 2³², que es justo lo que hace el hardware. Lo mismo aplica al
`imm << 2` del cálculo de saltos.

**Las burbujas son explícitas.** Cada registro de pipeline tiene un campo
`valido`; si está en cero, la etapa siguiente no hace nada. No uso "la
instrucción vale 0" como convención implícita, porque eso confunde un NOP real
con un hueco del pipeline.

### Los riesgos

| Riesgo | Cómo se resuelve |
|---|---|
| Un resultado que la instrucción siguiente necesita | cortocircuito desde EX/MEM |
| Lo mismo, dos instrucciones después | cortocircuito desde MEM/WB |
| Tres instrucciones después | el banco que escribe en la primera mitad |
| `lw` seguido de quien usa ese registro | burbuja de un ciclo, y después el cortocircuito MEM/WB |
| Saltos | se anulan las dos instrucciones más jóvenes |

El caso que casi se me pasa: el dato que guarda `sw` también tiene que pasar por
el cortocircuito. Si haces `add $t0, ...` y justo después `sw $t0, 0($s0)`, sin
eso guardas el valor viejo. Tiene su propio unit test para que no se vuelva a
perder.

## Cómo lo verifiqué

Los unit tests suman 206 comprobaciones repartidas así: 31 en IF, 78 en ID, 57
en la ALU, 27 en MEM y 13 en WB. Además de la operación normal, cada uno prueba
qué pasa con entradas que no deberían llegar nunca.

El test del sistema son nueve programas completos, con 63 comprobaciones:

| | Qué ejercita |
|---|---|
| V1 | las siete operaciones aritméticas y lógicas |
| V2 | `sw`/`lw`, el riesgo `lw`-uso y el cortocircuito del dato de `sw` |
| V3 | `beq` tomado y no tomado, y que lo anulado no deje rastro |
| V4 | un bucle con `bne` |
| V5 | `j` a una subrutina y `jr` de vuelta |
| V6 | cuatro dependencias encadenadas seguidas |
| V7 | que `$0` no se pueda escribir |
| V8 | una dirección de memoria fuera de rango |
| V9 | una instrucción que no existe en la ISA |

Más pruebas directas a la unidad de riesgos y a la de cortocircuito. **En total
269 comprobaciones, todas pasando.**

No uso `assert()`: aborta en el primer fallo y te esconde los otros veinte. En
vez de eso cada comprobación fallida imprime archivo, línea, valor esperado y
valor obtenido, y el programa sigue hasta el final. El código de salida es 0
solo si no falló nada, así que `make test` se corta solo cuando algo se rompe.

## Qué pasa con las entradas raras

Fue el punto que más trabajé, porque era lo que pedía el rol de Crítico:

- PC desalineado o fuera de la memoria de instrucciones: se reporta el error y
  se devuelve un NOP, para no propagar basura por el pipeline.
- Opcode o `funct` que no existen: error y burbuja. Nunca se ejecuta algo que no
  se entendió.
- Dirección de datos desalineada o fuera de rango (incluidas las negativas, que
  vistas como sin signo quedan enormes): se reporta y **no se toca la memoria**.
- Escritura a `$0`: se descarta en WB.
- Punteros nulos: cada subfunción devuelve algo seguro en vez de reventar.
- Programa con bucle infinito: `mips_ejecutar` tiene un tope de ciclos.

## Lo que no hace

- **No hay excepciones precisas.** Cuando detecta un error, el simulador para el
  reloj en ese mismo instante, así que las instrucciones más viejas que seguían
  en vuelo no alcanzan a terminar. Se nota comparando V8 con V9: en V8 el error
  aparece en MEM y la instrucción anterior sí alcanza a escribir; en V9 aparece
  en ID, dos ciclos antes, y no.
- **No hay predicción de saltos.** Cada salto tomado cuesta dos ciclos.
- **`add`, `addi` y `sub` no lanzan trampa por desbordamiento**, se comportan
  como `addu`/`subu`. Es a propósito y está anotado en el código.
- **La memoria es solo por palabras** de 32 bits alineadas: no hay `lb` ni `lh`.
- Las memorias son de 256 palabras cada una. Alcanza de sobra para las pruebas y
  se cambia con `MIPS_IMEM_PALABRAS` y `MIPS_DMEM_PALABRAS`.
