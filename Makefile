CC       = gcc
CFLAGS   = -Wall -Wextra -std=c11 -Iinclude -g
SRC_DIR  = src
TEST_DIR = tests
BUILD_DIR = build

# Fuentes del simulador (sin main.c, se linkea aparte para los tests)
CORE_SRCS = $(SRC_DIR)/etapa_if.c $(SRC_DIR)/etapa_id.c $(SRC_DIR)/etapa_alu.c \
            $(SRC_DIR)/etapa_mem.c $(SRC_DIR)/etapa_wb.c $(SRC_DIR)/mips_core.c

TEST_BINS = $(BUILD_DIR)/test_etapa_if $(BUILD_DIR)/test_etapa_id \
            $(BUILD_DIR)/test_etapa_alu $(BUILD_DIR)/test_etapa_mem \
            $(BUILD_DIR)/test_etapa_wb $(BUILD_DIR)/test_integracion

.PHONY: all sim test clean

all: sim

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# --- Simulador principal ---
sim: $(BUILD_DIR) $(CORE_SRCS) $(SRC_DIR)/main.c
	$(CC) $(CFLAGS) $(CORE_SRCS) $(SRC_DIR)/main.c -o $(BUILD_DIR)/mips_sim
	@echo "Ejecutable listo en $(BUILD_DIR)/mips_sim"

# --- Unit tests (uno por etapa) + integracion ---
$(BUILD_DIR)/test_etapa_if: $(BUILD_DIR) $(CORE_SRCS) $(TEST_DIR)/test_etapa_if.c
	$(CC) $(CFLAGS) $(CORE_SRCS) $(TEST_DIR)/test_etapa_if.c -o $@

$(BUILD_DIR)/test_etapa_id: $(BUILD_DIR) $(CORE_SRCS) $(TEST_DIR)/test_etapa_id.c
	$(CC) $(CFLAGS) $(CORE_SRCS) $(TEST_DIR)/test_etapa_id.c -o $@

$(BUILD_DIR)/test_etapa_alu: $(BUILD_DIR) $(CORE_SRCS) $(TEST_DIR)/test_etapa_alu.c
	$(CC) $(CFLAGS) $(CORE_SRCS) $(TEST_DIR)/test_etapa_alu.c -o $@

$(BUILD_DIR)/test_etapa_mem: $(BUILD_DIR) $(CORE_SRCS) $(TEST_DIR)/test_etapa_mem.c
	$(CC) $(CFLAGS) $(CORE_SRCS) $(TEST_DIR)/test_etapa_mem.c -o $@

$(BUILD_DIR)/test_etapa_wb: $(BUILD_DIR) $(CORE_SRCS) $(TEST_DIR)/test_etapa_wb.c
	$(CC) $(CFLAGS) $(CORE_SRCS) $(TEST_DIR)/test_etapa_wb.c -o $@

$(BUILD_DIR)/test_integracion: $(BUILD_DIR) $(CORE_SRCS) $(TEST_DIR)/test_integracion.c
	$(CC) $(CFLAGS) $(CORE_SRCS) $(TEST_DIR)/test_integracion.c -o $@

test: $(TEST_BINS)
	@echo "===== Ejecutando unit tests ====="
	@for t in $(TEST_BINS); do ./$$t || exit 1; done
	@echo "===== Todos los tests pasaron ====="

clean:
	rm -rf $(BUILD_DIR)
