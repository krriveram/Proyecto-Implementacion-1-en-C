# Diagrama 2 — Subfunciones de cada etapa

Qué hay dentro de cada una de las cinco funciones: los muxes, sumadores y demás
bloques que la componen, con el número de bits e índice de cada señal que entra
y sale. Cada subfunción del diagrama existe como función en el código, así que
se pueden probar por separado.

---

## Etapa 1 — `etapa_if` (Instruction Fetch + Program Counter)

```mermaid
flowchart LR
    PC[/"pc[31:0]"/] --> LEER["if_imem_leer<br/>(imem, palabras, dir)"]
    IMEM[("imem[256]<br/>32 bits/palabra")] --> LEER
    PC --> SUM["if_sumador_pc4<br/>pc + 4"]

    LEER -- "instr[31:0]" --> IFID(["IF/ID"])
    LEER -- "err" --> ERR[/"err"/]
    SUM  -- "pc_mas_4[31:0]" --> MUX["if_mux_pc<br/>(pc_mas_4, pc_objetivo, pc_src)"]
    SUM  -- "pc_mas_4[31:0]" --> IFID

    OBJ[/"pc_objetivo[31:0]"/] --> MUX
    SRC[/"pc_src (1 bit)"/] --> MUX
    MUX -- "pc_siguiente[31:0]" --> OUT[/"pc_siguiente"/]

    STALL[/"stall (1 bit)"/] -.->|"congela PC<br/>cargar_if_id = 0"| MUX
    FLUSH[/"flush (1 bit)"/] -.->|"valido = 0"| IFID
```

| Subfunción | Entradas | Salidas |
|---|---|---|
| `if_sumador_pc4` | `pc[31:0]` | `pc+4[31:0]` |
| `if_mux_pc` | `pc_mas_4[31:0]`, `pc_objetivo[31:0]`, `pc_src` | `pc_siguiente[31:0]` |
| `if_imem_leer` | `imem[]`, `palabras`, `dir[31:0]` | `instr[31:0]`, `err` |

Validaciones: `dir[1:0] != 00` → `DIR_ALINEACION`; `dir >> 2 >= palabras` → `DIR_RANGO`.
En ambos casos devuelve `NOP` para no propagar basura.

---

## Etapa 2 — `etapa_id` (Instruction Decode)

```mermaid
flowchart LR
    INSTR[/"instr[31:0]"/] --> C1["id_campo_opcode<br/>instr[31:26]"]
    INSTR --> C2["id_campo_rs<br/>instr[25:21]"]
    INSTR --> C3["id_campo_rt<br/>instr[20:16]"]
    INSTR --> C4["id_campo_rd<br/>instr[15:11]"]
    INSTR --> C5["id_campo_funct<br/>instr[5:0]"]
    INSTR --> C6["id_campo_imm16<br/>instr[15:0]"]
    INSTR --> C7["id_campo_addr26<br/>instr[25:0]"]

    C1 -- "opcode[5:0]" --> UC["id_unidad_control<br/>(opcode, funct)"]
    C5 -- "funct[5:0]" --> UC
    UC -- "ctrl (11 señales)<br/>+ alu_op" --> IDEX(["ID/EX"])
    UC -- "false -> err = OPCODE" --> ERR[/"err"/]

    C2 -- "rs[4:0]" --> BANCO["id_banco_leer<br/>(regs, rs, rt)"]
    C3 -- "rt[4:0]" --> BANCO
    REGS[("regs[32]<br/>32 bits")] --> BANCO
    BANCO -- "rs_val[31:0]<br/>rt_val[31:0]" --> IDEX

    C6 -- "imm16[15:0]" --> EXT["id_extension_signo<br/>replica instr[15]"]
    EXT -- "imm[31:0]" --> IDEX

    C7 -- "addr26[25:0]" --> DJ["id_destino_j<br/>pc_mas_4[31:28] : addr26 : 00"]
    PC4[/"pc_mas_4[31:0]"/] --> DJ
    DJ -- "pc_salto_j[31:0]" --> IDEX

    C4 -- "rd[4:0]" --> IDEX

    SF[/"stall / flush /<br/>IF-ID no válido"/] -.->|"burbuja: valido = 0"| IDEX
```

| Subfunción | Entradas | Salidas |
|---|---|---|
| `id_campo_*` | `instr[31:0]` | campo correspondiente |
| `id_extension_signo` | `imm16[15:0]` | `imm[31:0]` |
| `id_unidad_control` | `opcode[5:0]`, `funct[5:0]` | `ctrl`, `alu_op`, `bool` (soportada) |
| `id_banco_leer` | `regs[32]`, `rs[4:0]`, `rt[4:0]` | `rs_val[31:0]`, `rt_val[31:0]` |
| `id_destino_j` | `pc_mas_4[31:0]`, `addr26[25:0]` | `pc_salto_j[31:0]` |
| `id_registros_usados` | `instr[31:0]` | `usa_rs`, `usa_rt` (para la unidad de riesgos) |

### Tabla de la unidad de control

| Instr | opcode | funct | RegDst | ALUSrc | MemToReg | RegWrite | MemRead | MemWrite | Branch | Jump | ALUOp |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `add` | 0x00 | 0x20 | 1 | 0 | 0 | 1 | 0 | 0 | – | – | ADD |
| `sub` | 0x00 | 0x22 | 1 | 0 | 0 | 1 | 0 | 0 | – | – | SUB |
| `and` | 0x00 | 0x24 | 1 | 0 | 0 | 1 | 0 | 0 | – | – | AND |
| `or`  | 0x00 | 0x25 | 1 | 0 | 0 | 1 | 0 | 0 | – | – | OR |
| `xor` | 0x00 | 0x26 | 1 | 0 | 0 | 1 | 0 | 0 | – | – | XOR |
| `nor` | 0x00 | 0x27 | 1 | 0 | 0 | 1 | 0 | 0 | – | – | NOR |
| `jr`  | 0x00 | 0x08 | – | – | – | 0 | 0 | 0 | – | jump_reg | PASA_A |
| `addi`| 0x08 | – | 0 | 1 | 0 | 1 | 0 | 0 | – | – | ADD |
| `lw`  | 0x23 | – | 0 | 1 | 1 | 1 | 1 | 0 | – | – | ADD |
| `sw`  | 0x2B | – | – | 1 | – | 0 | 0 | 1 | – | – | ADD |
| `beq` | 0x04 | – | – | 0 | – | 0 | 0 | 0 | eq | – | SUB |
| `bne` | 0x05 | – | – | 0 | – | 0 | 0 | 0 | ne | – | SUB |
| `j`   | 0x02 | – | – | – | – | 0 | 0 | 0 | – | jump | NOP |

---

## Etapa 3 — `etapa_alu` (Execute)

```mermaid
flowchart LR
    RSV[/"rs_val[31:0]"/] --> FA["alu_mux_cortocircuito<br/>(fwd_a)"]
    RTV[/"rt_val[31:0]"/] --> FB["alu_mux_cortocircuito<br/>(fwd_b)"]
    EXMEM[/"dato_ex_mem[31:0]"/] --> FA
    EXMEM --> FB
    MEMWB[/"dato_mem_wb[31:0]"/] --> FA
    MEMWB --> FB

    FA -- "operando A[31:0]" --> ALU["alu_ejecutar<br/>(a, b, alu_op)"]
    FB -- "B de registro[31:0]" --> MSRC["alu_mux_src<br/>(rt_val, imm, alu_src)"]
    IMM[/"imm[31:0]"/] --> MSRC
    MSRC -- "operando B[31:0]" --> ALU

    ALU -- "alu_res[31:0]" --> EXM(["EX/MEM"])
    ALU -- "cero (1 bit)" --> DEC["alu_decision_salto<br/>(ctrl, cero)"]
    DEC -- "pc_src (1 bit)" --> OUT1[/"pc_src"/]

    PC4[/"pc_mas_4[31:0]"/] --> SUMS["alu_sumador_salto<br/>pc_mas_4 + (imm << 2)"]
    IMM --> SUMS
    SUMS -- "destino beq/bne" --> MUXD{"mux destino"}
    PCJ[/"pc_salto_j[31:0]"/] --> MUXD
    ALU -- "rs_val (jr)" --> MUXD
    MUXD -- "pc_objetivo[31:0]" --> OUT2[/"pc_objetivo"/]

    RT[/"rt[4:0]"/] --> MRD["alu_mux_reg_dst<br/>(rt, rd, reg_dst)"]
    RD[/"rd[4:0]"/] --> MRD
    MRD -- "reg_dst[4:0]" --> EXM
    FB -- "dato para sw[31:0]" --> EXM
```

| Subfunción | Entradas | Salidas |
|---|---|---|
| `alu_mux_cortocircuito` | `valor_banco[31:0]`, `dato_ex_mem[31:0]`, `dato_mem_wb[31:0]`, `sel` | operando `[31:0]` |
| `alu_mux_src` | `rt_val[31:0]`, `imm[31:0]`, `alu_src` | operando B `[31:0]` |
| `alu_ejecutar` | `a[31:0]`, `b[31:0]`, `alu_op` | `resultado[31:0]`, `cero` |
| `alu_sumador_salto` | `pc_mas_4[31:0]`, `imm[31:0]` | `destino[31:0]` |
| `alu_mux_reg_dst` | `rt[4:0]`, `rd[4:0]`, `reg_dst` | `reg_dst[4:0]` |
| `alu_decision_salto` | `ctrl`, `cero` | `pc_src` |

Todos los saltos (`beq`, `bne`, `j`, `jr`) se resuelven aquí: una sola regla,
con penalidad fija de 2 instrucciones anuladas por salto tomado.

---

## Etapa 4 — `etapa_mem` (Memory)

```mermaid
flowchart LR
    ALURES[/"alu_res[31:0]<br/>= dirección"/] --> VAL["mem_dir_valida<br/>(dir, palabras)"]
    VAL -- "indice[7:0] = dir >> 2" --> DMEM[("dmem[256]<br/>32 bits/palabra")]
    VAL -- "dir[1:0] != 00 -> DIR_ALINEACION<br/>dir>>2 >= 256 -> DIR_RANGO" --> ERR[/"err"/]

    RTV[/"rt_val[31:0]"/] --> WR{"mem_write = 1?"}
    WR -- "sí" --> DMEM
    DMEM --> RD{"mem_read = 1?"}
    RD -- "sí: mem_dato[31:0]" --> MW(["MEM/WB"])
    ALURES -- "paso directo" --> MW
    REGDST[/"reg_dst[4:0]"/] --> MW
```

| Subfunción | Entradas | Salidas |
|---|---|---|
| `mem_dir_valida` | `dir[31:0]`, `palabras` | `indice`, `err`, `bool` |

Una dirección inválida **aborta el acceso**: no se escribe ni se lee nada.

---

## Etapa 5 — `etapa_wb` (Write Back)

```mermaid
flowchart LR
    ALURES[/"alu_res[31:0]"/] --> MUX["wb_mux_mem_a_reg<br/>(alu_res, mem_dato, mem_to_reg)"]
    MEMDATO[/"mem_dato[31:0]"/] --> MUX
    MUX -- "dato[31:0]" --> OUT1[/"dato"/]

    REGW[/"ctrl.reg_write"/] --> AND{"AND"}
    RD[/"reg_dst[4:0] != 0"/] --> AND
    VAL[/"mem_wb.valido"/] --> AND
    AND -- "reg_write (1 bit)" --> OUT2[/"reg_write"/]
    RD -- "reg_dst[4:0]" --> OUT3[/"reg_dst"/]

    OUT1 --> BANCO[("banco de registros<br/>32 x 32 bits")]
    OUT2 --> BANCO
    OUT3 --> BANCO
```

| Subfunción | Entradas | Salidas |
|---|---|---|
| `wb_mux_mem_a_reg` | `alu_res[31:0]`, `mem_dato[31:0]`, `mem_to_reg` | `dato[31:0]` |

La etapa **no escribe** directamente: devuelve la orden y el núcleo la aplica.
Así la función es pura (fácil de probar) y el mismo dato alimenta el
cortocircuito MEM/WB hacia la etapa 3.

---

## Unidades de manejo de riesgos

```mermaid
flowchart TB
    subgraph RIESGOS["unidad_riesgos -- riesgo lw-uso"]
        A1[/"ID/EX: mem_read, rt[4:0]"/] --> A3{"lw en EX y<br/>su destino lo lee<br/>la instrucción en ID?"}
        A2[/"IF/ID: instr[31:0]"/] --> A3
        A3 -- "sí" --> A4[/"stall = 1<br/>congela IF, burbuja en ID"/]
    end

    subgraph FWD["unidad_cortocircuito -- riesgos de datos"]
        B1[/"EX/MEM: reg_write,<br/>reg_dst[4:0], mem_to_reg"/] --> B3{"coincide con<br/>rs o rt de ID/EX?"}
        B2[/"MEM/WB: reg_write,<br/>reg_dst[4:0]"/] --> B3
        B3 -- "EX/MEM primero<br/>(productor más reciente)" --> B4[/"fwd_a, fwd_b"/]
    end
```

| Riesgo | Distancia | Solución |
|---|---|---|
| Datos (ALU → ALU) | 1 | cortocircuito EX/MEM |
| Datos (ALU → ALU) | 2 | cortocircuito MEM/WB |
| Datos (ALU → ALU) | 3 | banco que escribe en la 1ª mitad del ciclo |
| Datos `lw` → uso | 1 | burbuja de 1 ciclo (`unidad_riesgos`) + cortocircuito MEM/WB |
| Control (`beq`, `bne`, `j`, `jr`) | — | anulación de las 2 instrucciones más jóvenes |
