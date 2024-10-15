# Compiler and flags
CC := gcc
CFLAGS := -std=c99 -pedantic -Wall -Wno-deprecated-declarations -Os
CPPFLAGS := -I/usr/include/freetype2
LDFLAGS := -lX11 -lXext -lXrandr -lcrypt -lm -lXft -lfontconfig -lImlib2 -lfreetype

# Directories
SRC_DIR := src
ASSET_DIR := assets
OBJ_DIR := obj

# Source and object files
SRC := $(wildcard $(SRC_DIR)/*.c)
OBJ := $(SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

# Output binary
TARGET := slock

# Version information
CURRENT_DATE := $(shell date +"%Y.%m.%d")
GIT_HASH := $(shell git rev-parse --short HEAD 2>/dev/null || echo "unknown")
BUILD_VERSION := $(CURRENT_DATE).$(GIT_HASH)
CFLAGS += -DBUILD_VERSION=\"$(BUILD_VERSION)\"

# Installation paths
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin

# Default target
.PHONY: all
all: $(TARGET)

# Linking
$(TARGET): $(OBJ) | $(OBJ_DIR)
	$(CC) -o $@ $^ $(LDFLAGS)

# Compilation
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(SRC_DIR)/config.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

# Create object directory
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# Install
.PHONY: install
install: $(TARGET)
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(TARGET) $(DESTDIR)$(BINDIR)
	chmod u+s $(DESTDIR)$(BINDIR)/$(TARGET)

# Uninstall
.PHONY: uninstall
uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)

# Clean
.PHONY: clean
clean:
	rm -rf $(TARGET) $(OBJ_DIR)

# Debug information
.PHONY: debug
debug:
	@echo "Source files: $(SRC)"
	@echo "Object files: $(OBJ)"
	@echo "Build version: $(BUILD_VERSION)"