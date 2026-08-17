# Diagrama 1 — Sistema completo

El simulador entero: las cinco etapas, los registros que van entre ellas, las
memorias, las unidades que manejan los riesgos y los módulos de prueba que
atacan cada parte. Las flechas punteadas son señales de control; las gruesas,
datos que entran o salen del sistema.

## 1.1 Vista general

```mermaid
flowchart LR
    %% ---------------- ENTRADAS ----------------
    subgraph ENT["ENTRADAS DEL SISTEMA"]
        direction TB
        PROG[/"programa<br/>uint32_t prog[n]<br/>32 bits por instrucción"/]
        DATOS[/"datos iniciales<br/>int32_t, 32 bits"/]
        CLK(["mips_ciclo()<br/>1 llamada = 1 flanco de reloj"]):::ctrl
        RST(["mips_reset()<br/>estado en cero"]):::ctrl
    end

    %% ---------------- PROCESADOR ----------------
    subgraph CPU["SIMULADOR MIPS -- PIPELINE DE 5 ETAPAS"]
        direction LR

        IF["1. etapa_if<br/>Fetch + PC"]:::etapa
        R1(["IF/ID<br/>valido, pc[31:0]<br/>pc_mas_4[31:0]<br/>instr[31:0]"]):::reg
        ID["2. etapa_id<br/>Decode"]:::etapa
        R2(["ID/EX<br/>rs_val/rt_val[31:0]<br/>imm[31:0], rs/rt/rd[4:0]<br/>pc_salto_j[31:0], ctrl"]):::reg
        EX["3. etapa_alu<br/>Execute"]:::etapa
        R3(["EX/MEM<br/>alu_res[31:0]<br/>rt_val[31:0]<br/>reg_dst[4:0], ctrl"]):::reg
        MEM["4. etapa_mem<br/>Memory"]:::etapa
        R4(["MEM/WB<br/>alu_res[31:0]<br/>mem_dato[31:0]<br/>reg_dst[4:0], ctrl"]):::reg
        WB["5. etapa_wb<br/>Write Back"]:::etapa

        IMEM[("imem<br/>256 x 32 bits")]:::mem
        DMEM[("dmem<br/>256 x 32 bits")]:::mem
        BANCO[("banco de registros<br/>32 x 32 bits<br/>$0 cableado a 0")]:::mem

        RIESGO{{"unidad_riesgos<br/>detecta lw-uso"}}:::haz
        FWD{{"unidad_cortocircuito<br/>fwd_a / fwd_b"}}:::haz

        IF --> R1 --> ID --> R2 --> EX --> R3 --> MEM --> R4 --> WB

        IMEM -- "instr[31:0]" --> IF
        BANCO -- "rs_val, rt_val[31:0]" --> ID
        WB -- "reg_write, reg_dst[4:0], dato[31:0]" --> BANCO
        MEM <-- "dir[31:0] / dato[31:0]" --> DMEM

        EX -- "pc_src, pc_objetivo[31:0]" --> IF
        EX -. "flush (anula ID e IF)" .-> ID

        R2 -. "lw en EX" .-> RIESGO
        R1 -. "instr en ID" .-> RIESGO
        RIESGO -- "stall" --> IF
        RIESGO -- "stall" --> ID

        R3 -. "reg_dst, alu_res" .-> FWD
        R4 -. "reg_dst, dato" .-> FWD
        FWD -- "fwd_a, fwd_b" --> EX
    end

    %% ---------------- PRUEBAS ----------------
    subgraph PRUEBAS["MÓDULOS DE VERIFICACIÓN"]
        direction TB
        U1["test_etapa_if"]:::test
        U2["test_etapa_id"]:::test
        U3["test_etapa_alu"]:::test
        U4["test_etapa_mem"]:::test
        U5["test_etapa_wb"]:::test
        SYS["test_integracion<br/>9 vectores de prueba"]:::test
    end

    %% ---------------- SALIDAS ----------------
    subgraph SAL["SALIDAS OBSERVABLES"]
        direction TB
        OUT1[/"regs[32]"/]
        OUT2[/"dmem[256]"/]
        OUT3[/"pc, ciclos, instr_retiradas<br/>stalls, flushes"/]
        OUT4[/"err: OK / OPCODE /<br/>DIR_RANGO / DIR_ALINEACION"/]
    end

    PROG ==> IMEM
    DATOS ==> DMEM
    CLK ==> CPU
    RST ==> CPU

    BANCO ==> OUT1
    DMEM  ==> OUT2
    CPU   ==> OUT3
    CPU   ==> OUT4

    U1 -. "inyecta if_in_t / evalúa if_out_t" .-> IF
    U2 -. "inyecta id_in_t / evalúa id_out_t" .-> ID
    U3 -. "inyecta alu_in_t / evalúa alu_out_t" .-> EX
    U4 -. "inyecta mem_in_t / evalúa mem_out_t" .-> MEM
    U5 -. "inyecta wb_in_t / evalúa wb_out_t" .-> WB
    SYS -. "carga programa" .-> IMEM
    SYS -. "compara estado final" .-> SAL

    classDef etapa fill:#1f6feb,stroke:#0b3d91,color:#fff
    classDef reg   fill:#8957e5,stroke:#4c2889,color:#fff
    classDef mem   fill:#1a7f37,stroke:#0b4a20,color:#fff
    classDef haz   fill:#bf8700,stroke:#7a5600,color:#fff
    classDef test  fill:#cf222e,stroke:#82071e,color:#fff
    classDef ctrl  fill:#57606a,stroke:#24292f,color:#fff
```

## 1.2 Entradas y salidas de cada módulo

| Módulo | Entradas | Salidas |
|---|---|---|
| `etapa_if` | `pc[31:0]`, `imem[]`, `imem_palabras`, `pc_src`, `pc_objetivo[31:0]`, `stall`, `flush` | `pc_siguiente[31:0]`, `IF/ID`, `cargar_if_id`, `err` |
| `etapa_id` | `IF/ID`, `regs[32][31:0]`, `stall`, `flush` | `ID/EX` (valores, índices `[4:0]`, `imm[31:0]`, `ctrl`), `err` |
| `etapa_alu` | `ID/EX`, `fwd_a`, `fwd_b`, `dato_ex_mem[31:0]`, `dato_mem_wb[31:0]` | `EX/MEM`, `pc_src`, `pc_objetivo[31:0]`, `cero` |
| `etapa_mem` | `EX/MEM`, `dmem[]`, `dmem_palabras` | `MEM/WB`, `err`, escritura en `dmem` |
| `etapa_wb` | `MEM/WB` | `reg_write`, `reg_dst[4:0]`, `dato[31:0]` |
| `unidad_riesgos` | `ID/EX`, `IF/ID` | `stall` |
| `unidad_cortocircuito` | `ID/EX`, `EX/MEM`, `MEM/WB` | `fwd_a`, `fwd_b` |

## 1.3 Cronograma de un ciclo (`mips_ciclo`)

```mermaid
sequenceDiagram
    participant N as mips_ciclo
    participant WB as 5. etapa_wb
    participant MEM as 4. etapa_mem
    participant EX as 3. etapa_alu
    participant ID as 2. etapa_id
    participant IF as 1. etapa_if

    Note over N: FASE COMBINACIONAL<br/>todas leen el estado ACTUAL de los registros de pipeline
    N->>WB: MEM/WB
    WB-->>N: reg_write, reg_dst, dato
    Note over N: se aplica la escritura en el banco<br/>(1a mitad del ciclo, antes de que ID lea)
    N->>MEM: EX/MEM + dmem
    MEM-->>N: MEM/WB nuevo
    N->>EX: ID/EX + selectores de cortocircuito
    EX-->>N: EX/MEM nuevo, pc_src, pc_objetivo
    Note over N: unidad_riesgos -> stall<br/>pc_src -> flush (flush tiene prioridad)
    N->>ID: IF/ID + regs + stall + flush
    ID-->>N: ID/EX nuevo
    N->>IF: pc + imem + pc_src + stall + flush
    IF-->>N: pc_siguiente, IF/ID nuevo
    Note over N: FLANCO DE RELOJ<br/>se actualizan PC, IF/ID, ID/EX, EX/MEM y MEM/WB de golpe
```
