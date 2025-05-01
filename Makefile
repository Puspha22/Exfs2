# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -Wno-sign-compare -g
TARGET = exfs2

# Source and object files
SRCS = main.c init.c add.c extract.c remove.c debug.c helpers.c path.c
OBJS = $(SRCS:.c=.o)

.PHONY: all clean

# Default target to build everything
all: $(TARGET)

# Link object files into final executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# Compile each source file into an object file
%.o: %.c exfs2.h
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up build and segment artifacts
clean:
	rm -f $(TARGET) *.o
	rm -f inode_segment_*.seg data_segment_*.seg
	rm -f recovered_*.bin *.bin *.hex *.txt
