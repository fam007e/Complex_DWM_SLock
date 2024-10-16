# Compiler and flags
CC := gcc
OPTIMIZATIONS := -march=native -mtune=native -flto=auto -O3
CFLAGS := -std=c99 -pedantic -Wall -Wno-deprecated-declarations $(OPTIMIZATIONS) -Os
CPPFLAGS := -I/usr/include/freetype2 -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_XOPEN_SOURCE=700L
LDFLAGS := -L/usr/X11R6/lib
LDLIBS := -lX11 -lXext -lXrandr -lcrypt -lm -lXft -lfontconfig -lImlib2 -lfreetype -lpam

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
CPPFLAGS += -DBUILD_VERSION=\"$(BUILD_VERSION)\"

# Installation paths
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin

# Default target
.PHONY: all
all: $(TARGET)

# Linking
$(TARGET): $(OBJ) | $(OBJ_DIR)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

# Compilation
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(SRC_DIR)/config.h | $(OBJ_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

# Create object directory
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# Install
.PHONY: install
install: $(TARGET)
	install -d $(DESTDIR)$(BINDIR)
	install -m 4755 $(TARGET) $(DESTDIR)$(BINDIR)

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

.PHONY: print-version
print-version:
	@echo $(BUILD_VERSION)