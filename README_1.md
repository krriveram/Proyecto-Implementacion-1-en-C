# Proyecto-Implementación-1-en-C

Implementación en C de un simulador del procesador MIPS segmentado (pipeline de
5 etapas: IF, ID, EX, MEM, WB) con sus subfunciones, unit tests y un test
integral con vectores de prueba.

## Manejo de riesgos (hazards)

El simulador resuelve los tres tipos de riesgos del pipeline para que los
programas con instrucciones dependientes se ejecuten correctamente:

- **Riesgos de datos (forwarding):** los operandos de la ALU se adelantan desde
  EX/MEM (instrucción 1 adelante) y desde el banco de registros ya actualizado
  (que cubre las instrucciones 2 y 3 adelante, porque `write_back()` se ejecuta
  antes que la etapa EX dentro de `cpu_step()`). Esto también aplica al dato de
  escritura de `sw`.
- **Riesgo carga-uso (load-use):** cuando un `lw` es seguido inmediatamente por
  una instrucción que usa el registro cargado, se detecta en ID y se inserta una
  burbuja de 1 ciclo (se congela el PC e IF/ID).
- **Riesgos de control (flush):** los `beq`/`bne` se resuelven en EX; si el salto
  se toma, se descarta (flush) la instrucción que está en IF/ID y el PC se
  redirige al destino. Los saltos incondicionales `j`/`jr` se resuelven en ID.

## Compilar y ejecutar

```bash
make        # compila el simulador (mips_sim)
make test   # compila y ejecuta los vectores de prueba
make clean  # elimina el binario
```

Salida esperada: las tres suites (ALU, Memoria, Control de Flujo) reportan `OK`.
