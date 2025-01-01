SRC_DIR = src
OBJ_DIR = obj
LIB_DIR = lib
BIN_DIR = bin

TARGET = $(BIN_DIR)/micro

SRCS = $(wildcard $(SRC_DIR)/*.c)

OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

CC = gcc
CFLAGS = -I$(LIB_DIR) -Wall -Wextra -g

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(BIN_DIR)
	$(CC) -o $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ_DIR)/*.o $(TARGET)

rebuild: clean all

.PHONY: all clean rebuild
