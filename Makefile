# =============================================================================
# Makefile -- Simulador MIPS con pipeline de 5 etapas
# rama: implementacion_kenny
# -----------------------------------------------------------------------------
# Objetivos:
#   make            compila el simulador y todos los ejecutables de prueba
#   make test       compila y ejecuta los 5 unit tests + el test del sistema
#   make unit       solo los unit tests de las 5 etapas
#   make integracion  solo el test del sistema (vectores de prueba)
#   make run        ejecuta la demostracion del simulador
#   make traza      ejecuta la demostracion mostrando el pipeline por ciclo
#   make clean      borra los ejecutables generados
#
# Funciona en Linux/macOS (make) y en Windows (mingw32-make).
# =============================================================================

CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -Wpedantic -O2 -Iinclude -Itests

# --- Ajustes por sistema operativo -------------------------------------------
ifeq ($(OS),Windows_NT)
    SHELL       := cmd.exe
    .SHELLFLAGS := /C
    EXE  := .exe
    RM   := del /Q /F
    # cmd.exe exige el prefijo ".\" para ejecutar un binario del directorio
    # actual. Se arma con una variable vacia a los lados para que la barra
    # invertida no se lea como continuacion de linea del Makefile.
    VACIO :=
    EJEC  := .$(VACIO)\$(VACIO)
else
    EXE  :=
    RM   := rm -f
    EJEC := ./
endif

# --- Fuentes ------------------------------------------------------------------
# Las 5 etapas del pipeline + el nucleo que las conecta + utilidades de la ISA.
NUCLEO = src/etapa_if.c  \
         src/etapa_id.c  \
         src/etapa_alu.c \
         src/etapa_mem.c \
         src/etapa_wb.c  \
         src/mips_core.c \
         src/mips_isa.c

CABECERAS = include/mips.h

SIM         = mips_sim$(EXE)
UNIT_TESTS  = test_etapa_if$(EXE)  \
              test_etapa_id$(EXE)  \
              test_etapa_alu$(EXE) \
              test_etapa_mem$(EXE) \
              test_etapa_wb$(EXE)
INTEGRACION = test_integracion$(EXE)

BINARIOS = $(SIM) $(UNIT_TESTS) $(INTEGRACION)

# --- Reglas -------------------------------------------------------------------
.PHONY: all test unit integracion run traza clean

all: $(BINARIOS)

$(SIM): src/main.c $(NUCLEO) $(CABECERAS)
	$(CC) $(CFLAGS) -o $@ src/main.c $(NUCLEO)

test_etapa_if$(EXE): tests/test_etapa_if.c $(NUCLEO) $(CABECERAS) tests/test_util.h
	$(CC) $(CFLAGS) -o $@ tests/test_etapa_if.c $(NUCLEO)

test_etapa_id$(EXE): tests/test_etapa_id.c $(NUCLEO) $(CABECERAS) tests/test_util.h
	$(CC) $(CFLAGS) -o $@ tests/test_etapa_id.c $(NUCLEO)

test_etapa_alu$(EXE): tests/test_etapa_alu.c $(NUCLEO) $(CABECERAS) tests/test_util.h
	$(CC) $(CFLAGS) -o $@ tests/test_etapa_alu.c $(NUCLEO)

test_etapa_mem$(EXE): tests/test_etapa_mem.c $(NUCLEO) $(CABECERAS) tests/test_util.h
	$(CC) $(CFLAGS) -o $@ tests/test_etapa_mem.c $(NUCLEO)

test_etapa_wb$(EXE): tests/test_etapa_wb.c $(NUCLEO) $(CABECERAS) tests/test_util.h
	$(CC) $(CFLAGS) -o $@ tests/test_etapa_wb.c $(NUCLEO)

$(INTEGRACION): tests/test_integracion.c $(NUCLEO) $(CABECERAS) tests/test_util.h
	$(CC) $(CFLAGS) -o $@ tests/test_integracion.c $(NUCLEO)

# Cada prueba devuelve 0 solo si no hubo fallos: si una falla, make se detiene.
unit: $(UNIT_TESTS)
	@echo === UNIT TESTS DE LAS 5 ETAPAS ===
	$(EJEC)test_etapa_if$(EXE)
	$(EJEC)test_etapa_id$(EXE)
	$(EJEC)test_etapa_alu$(EXE)
	$(EJEC)test_etapa_mem$(EXE)
	$(EJEC)test_etapa_wb$(EXE)

integracion: $(INTEGRACION)
	@echo === TEST DEL SISTEMA ===
	$(EJEC)$(INTEGRACION)

test: unit integracion
	@echo === TODAS LAS PRUEBAS PASARON ===

run: $(SIM)
	$(EJEC)$(SIM)

traza: $(SIM)
	$(EJEC)$(SIM) --traza

clean:
	-$(RM) $(BINARIOS)
