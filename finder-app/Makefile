# Define variables
CC = $(CROSS_COMPILE)gcc
CFLAGS = -Wall -Wextra -O2
TARGET = writer

# Output directory
OUTDIR ?= $(CURDIR)

# Default target
all: $(OUTDIR)/$(TARGET)

# Build the writer application
$(OUTDIR)/$(TARGET): $(OUTDIR)/writer.o
	@mkdir -p $(OUTDIR)
	$(CC) $(CFLAGS) -o $(OUTDIR)/$(TARGET) $(OUTDIR)/writer.o

# Compile the writer.o object file
$(OUTDIR)/writer.o: writer.c
	@mkdir -p $(OUTDIR)
	$(CC) $(CFLAGS) -c writer.c -o $(OUTDIR)/writer.o

# Clean target to remove the writer application and all .o files
clean:
	rm -rf $(OUTDIR)/$(TARGET) $(OUTDIR)/*.o

#the all and clean targets are declared as phony to ensure that make always 
# executes their recipes, regardless of whether files named all or clean 
# exist in the directory.
.PHONY: all clean