# Compiler & Flags
CC = gcc
CFLAGS = -O2 -Wall -Wextra -pthread -g -fno-omit-frame-pointer -I./include

# Directories
SRC_DIR = src
BIN_DIR = bin

# List of benchmarks to build
# ADDED: bench_server_sim
TARGETS = $(BIN_DIR)/bench_churn \
          $(BIN_DIR)/bench_churn_mt \
          $(BIN_DIR)/bench_fixed \
          $(BIN_DIR)/bench_mt \
          $(BIN_DIR)/bench_threads \
          $(BIN_DIR)/bench_var \
          $(BIN_DIR)/test_multilistener \
          $(BIN_DIR)/bench_server_sim \

# Default Target
all: $(BIN_DIR) $(TARGETS)

# Create bin directory
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Generic Compilation Rule
# This works for bench_server_sim too because it matches bin/% from src/%.c
$(BIN_DIR)/%: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -o $@ $<

# Clean Rule
clean:
	rm -rf $(BIN_DIR)/*
	rm -rf artifacts/*

.PHONY: all clean