# Diagrama de subfunciones por etapa

## ID — decodificación y control

```mermaid
flowchart TD
    A[instr de 32 bits] --> B["decodificar_campos()\n(interna, static)"]
    B --> C[opcode, rs, rt, rd, shamt, funct, inmediato, direccion26]
    C --> D["unidad_control(opcode, funct)"]
    D --> E["alu_control(opcode, funct)"]
    D --> F[senales_control_t]
    C --> G["lectura regfile[rs], regfile[rt]"]
```

## EX — ALU y cálculo de destinos de salto

```mermaid
flowchart TD
    A[valor_rs, valor_rt, inmediato, shamt] --> B{"ctrl.alu_usa_inm?"}
    B -- si --> C[operando_b = inmediato]
    B -- no --> D[operando_b = valor_rt]
    C --> E["calcular_alu(alu_op, rs, operando_b, shamt)\n(interna, static)"]
    D --> E
    E --> F[resultado_alu]
    F --> G["zero = (resultado_alu == 0)"]
    A --> H["pc_branch = pc_siguiente + (inmediato << 2)"]
    A --> I["pc_jump = bits_altos(pc_siguiente) | (direccion26 << 2)"]
```

## Unidades de control (tabla de verdad resumida)

| opcode  | reg_write | mem_read | mem_write | mem_to_reg | alu_usa_inm | branch | jump | link |
|---------|:---------:|:--------:|:---------:|:----------:|:-----------:|:------:|:----:|:----:|
| R-type  | 1         | 0        | 0         | 0          | 0           | 0      | 0    | 0    |
| addi    | 1         | 0        | 0         | 0          | 1           | 0      | 0    | 0    |
| lw      | 1         | 1        | 0         | 1          | 1           | 0      | 0    | 0    |
| sw      | 0         | 0        | 1         | -          | 1           | 0      | 0    | 0    |
| beq     | 0         | 0        | 0         | -          | 0           | eq     | 0    | 0    |
| bne     | 0         | 0        | 0         | -          | 0           | ne     | 0    | 0    |
| j       | 0         | 0        | 0         | -          | -           | 0      | 1    | 0    |
| jal     | 1         | 0        | 0         | 0          | -           | 0      | 1    | 1    |

## ALU control (`alu_control`)

| opcode/funct        | operación ALU |
|----------------------|----------------|
| addi, lw, sw          | ADD            |
| beq, bne               | SUB (para comparar) |
| R-type + funct=ADD     | ADD            |
| R-type + funct=SUB     | SUB            |
| R-type + funct=AND     | AND            |
| R-type + funct=OR      | OR             |
| R-type + funct=SLT     | SLT            |
| R-type + funct=SLL     | SLL            |
