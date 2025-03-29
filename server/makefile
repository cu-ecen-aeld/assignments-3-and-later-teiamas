# Compiler and flags
CC = $(CROSS_COMPILE)gcc
CFLAGS ?= -Wall -Werror -g
LDFLAGS ?=

# Source and target
SRC    = aesdsocket.c
OBJ    = $(patsubst %.c, $(OUTDIR)/%.o, $(SRC)) # Ensure object files are placed in OUTDIR
TARGET = aesdsocket

# Output directory
OUTDIR ?= $(CURDIR)

# Default target
all: $(OUTDIR)/$(TARGET)

# Ensure the output directory exists
$(OUTDIR):
	mkdir -p $(OUTDIR)

# Build the target binary
$(OUTDIR)/$(TARGET): $(OBJ) | $(OUTDIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Compile object files
$(OUTDIR)/%.o: %.c | $(OUTDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean target to remove the binary and object files
clean:
	rm -rf $(OUTDIR)/$(TARGET) $(OBJ)

# Declare phony targets
.PHONY: all clean