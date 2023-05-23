##
# Sebbitor
#
# @file
# @version 0.2

CFLAGS = -Wall -Wextra -pedantic -std=c99 -g3

SRC_DIR = ./src
BUILD_DIR = ./build

SRC = $(wildcard $(SRC_DIR)/*.c)
OBJ = $(SRC:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

all: $(OBJ)
	mkdir -p bin
	$(CC) $(OBJ) $(CFLAGS) -o bin/sebbitor

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) -c $< -o $@

.PHONY: clean

clean:
	rm -rf build bin

# end
