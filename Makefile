# Compiler
CC = clang

# Detect OS
UNAME_S := $(shell uname -s)

# --- Configuration ---

ifeq ($(UNAME_S), FreeBSD)
	export PKG_CONFIG_PATH=/usr/local/libdata/pkgconfig
endif

# --- Auto-Detect Vendor Includes ---
# Finds any folder matching "vendor/*/include" (e.g. vendor/glad/include)
VENDOR_INCLUDES = $(wildcard vendor/*/include)

# Include Paths (includes src, app, and all vendor include folders)
INCLUDES = -I. -Isrc -Iapp $(addprefix -I, $(VENDOR_INCLUDES)) -Ivendor

ifeq ($(UNAME_S), FreeBSD)
	INCLUDES += -I/usr/local/include
endif

# Compiler Flags
CFLAGS = -Wall -Wextra -g -std=c99 -O2 $(INCLUDES) $(shell pkg-config --cflags glfw3) -MMD -MP #-DDEBUG_FRUSTUM

# define DEBUG_FRUSTUM to enable frustum debug

# Linker Flags
LDFLAGS = $(shell pkg-config --libs glfw3)

ifeq ($(UNAME_S), FreeBSD)
	LDFLAGS += -L/usr/local/lib -lGL -lm -lpthread
endif

ifeq ($(UNAME_S), Darwin)
	LDFLAGS += -framework OpenGL
endif

# --- Directory Structure ---

OBJ_DIR = obj
BIN_DIR = bin

# --- Files ---

# Source Files
# NOTE: You still need to manually add the .c files for new libraries here.
# (e.g. vendor/physac/src/physac.c) because relying on wildcards for source
# files is dangerous (it might compile test files or examples you don't want).
SRCS = src/cubit.c \
       src/backend.c \
       src/frontend.c \
       src/object.c \
	   src/camera.c\
       src/input.c \
       src/batch.c \
       src/material.c \
	   src/shader.c \
	   src/light.c \
	   src/mesh.c \
	   src/texture.c \
	   src/shadow.c \
	   src/collision.c \
       vendor/glad/src/glad.c \
       app/game.c

# Generate Object paths
OBJS = $(SRCS:%.c=$(OBJ_DIR)/%.o)

# Generate Dependency paths
DEPS = $(OBJS:.o=.d)

# The final executable name
TARGET = $(BIN_DIR)/game

# --- Rules ---

.PHONY: all clean

all: $(TARGET)

# Link step
$(TARGET): $(OBJS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

# Compile step
$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Include dependencies
-include $(DEPS)

# Clean
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

# Fix clock skew / infinite rebuild (The Magic Step)
fix-time:
	find . -type f \( -name "*.c" -o -name "*.h" \) -exec touch {} +
	@echo "Timestamps reset."

# Assets copy
copy:
	@echo "Copying assets to bin/..."
	@mkdir -p bin/
	@cp -R app/assets bin/
