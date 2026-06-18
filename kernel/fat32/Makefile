CC = gcc
CFLAG = -Wall
LDFLAG = 
BUILD_DIR = build
OBJECT_DIR = obj

SRC = $(wildcard *.c)
OBJ = $(patsubst %.c, $(OBJECT_DIR)/%.obj, $(SRC))

all: $(BUILD_DIR)/fat32

$(OBJECT_DIR)/%.obj: %.c
	$(CC) $(CFLAG) -c $< -o $@

$(BUILD_DIR)/fat32: $(OBJ)
	$(CC) $(LDFLAG) $^ -o $@

clean:
	rm -rf $(BUILD_DIR)/
	rm -rf $(OBJECT_DIR)/
	mkdir -p $(BUILD_DIR)/
	mkdir -p $(OBJECT_DIR)/