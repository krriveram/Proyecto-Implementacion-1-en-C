# Variables del compilador
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -O2

# Nombre del ejecutable y archivos fuente
TARGET = mips_sim
SRC = mips_sim.c

# Regla por defecto (compilar todo)
all: $(TARGET)

# Regla para compilar el simulador
$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

# Regla para ejecutar automáticamente las pruebas
test: $(TARGET)
	./$(TARGET)

# Regla para limpiar archivos binarios generados
clean:
	rm -f $(TARGET)
