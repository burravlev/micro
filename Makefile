# Название исполняемого файла
TARGET = bin/micro

# Директории
SRC_DIR = src
OBJ_DIR = obj
INC_DIR = include

# Список исходных файлов
SRCS = $(wildcard $(SRC_DIR)/*.c)

# Список объектных файлов
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

# Компилятор и флаги
CC = gcc
CFLAGS = -I$(INC_DIR) -Wall -Wextra -g

# Правило по умолчанию
all: $(TARGET)

# Правило для сборки исполняемого файла
$(TARGET): $(OBJS)
	$(CC) -o $@ $^

# Правило для компиляции .c файлов в .o
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)  # Создает директорию obj, если она не существует
	$(CC) $(CFLAGS) -c $< -o $@

# Правило для очистки
clean:
	rm -f $(OBJ_DIR)/*.o $(TARGET)

# Правило для очистки и сборки
rebuild: clean all

.PHONY: all clean rebuild
