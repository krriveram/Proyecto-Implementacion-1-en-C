# Variables del compilador
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -O2

# Nombre del ejecutable y archivos fuente
TARGET = mips_sim
SRC = mips_sim.c

# Ejecutable y fuente de los unit tests
UNIT_TARGET = test_unit
UNIT_SRC = test_unit.c

# Regla por defecto (compilar el simulador)
all: $(TARGET)

# Regla para compilar el simulador
$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

# Test integral (vectores de prueba): compila y ejecuta el simulador
test: $(TARGET)
	./$(TARGET)

# Compila los unit tests. Se define UNIT_TESTING para que test_unit.c
# pueda incluir mips_sim.c sin arrastrar su main().
$(UNIT_TARGET): $(UNIT_SRC) $(SRC)
	$(CC) $(CFLAGS) -DUNIT_TESTING -o $(UNIT_TARGET) $(UNIT_SRC)

# Alias corto para compilar los unit tests
unit: $(UNIT_TARGET)

# Compila y ejecuta los unit tests de las 5 funciones
unit-run: $(UNIT_TARGET)
	./$(UNIT_TARGET)

# Ejecuta TODAS las pruebas: unit tests + test integral
check: unit-run test

# Limpia los binarios generados
clean:
	rm -f $(TARGET) $(UNIT_TARGET)

.PHONY: all test unit unit-run check clean
