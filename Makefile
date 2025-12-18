TARGET = bin/server
TEST_TARGET = bin/angry_threads_test

CC = gcc

CFLAGS = -Wall -Wextra -Iinclude -pthread

SRCS = src/server.c src/http.c src/main.c src/parser.c src/logger.c src/stats.c
TEST_SRC = test/angry_threads_test.c

OBJS = $(patsubst src/%.c, obj/%.o, $(SRCS))


all: $(TARGET) $(TEST_TARGET)

$(TARGET): $(OBJS)
	@mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $^
	@echo "Construcción exitosa: $(TARGET)"

$(TEST_TARGET): $(TEST_SRC)
	@mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $<
	@echo "Construcción exitosa: $(TEST_TARGET)"

obj/%.o: src/%.c
	@mkdir -p obj
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf bin obj
	@echo "Limpieza completada"

run: $(TARGET)
	./$(TARGET)
