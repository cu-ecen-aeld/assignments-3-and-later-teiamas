# Define variables
CC = $(CROSS_COMPILE)gcc
CFLAGS = -Wall -Wextra -O2
TARGET = writer

# Default target
all: $(TARGET)

# Build the writer application
$(TARGET): writer.o
	$(CC) $(CFLAGS) -o $(TARGET) writer.o

# Compile the writer.o object file
writer.o: writer.c
	$(CC) $(CFLAGS) -c writer.c

# Clean target to remove the writer application and all .o files
clean:
	rm -f $(TARGET) *.o

# Cross-compilation support
.PHONY: all clean
