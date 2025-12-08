# Nombre del ejecutable final
TARGET = bin/server

# Compilador
CC = gcc

# Flags (Opciones de compilación)
# -Iinclude: Se pone una sola vez y aplica para todos los archivos
CFLAGS = -Wall -Wextra -Iinclude

# Archivos fuente ESPECÍFICOS (Tal como en tu comando)
SRCS = src/server.c src/http.c src/main.c src/parser.c

# Transformamos la lista de .c (src/...) a .o (obj/...)
OBJS = $(patsubst src/%.c, obj/%.o, $(SRCS))

# --- Reglas ---

# Regla por defecto (la primera que se ejecuta al escribir "make")
all: $(TARGET)

# Paso de Linkeo: Une todos los objetos (.o) para crear el ejecutable
$(TARGET): $(OBJS)
	@mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $^
	@echo "Construcción exitosa: $(TARGET)"

# Paso de Compilación: Crea un .o por cada .c
# Esto es más eficiente que tu comando manual porque si modificas solo
# server.c, make no recompilará main.c ni http.c innecesariamente.
obj/%.o: src/%.c
	@mkdir -p obj
	$(CC) $(CFLAGS) -c $< -o $@

# Limpiar archivos generados
clean:
	rm -rf bin obj
	@echo "Limpieza completada"

# Ejecutar el servidor
run: $(TARGET)
	./$(TARGET)
