# Makefile for ExFS2

CC = gcc
CFLAGS = -Wall -Wextra -g
TARGET = exfs2
SRC = exfs2.c

.PHONY: all clean

# Default rule to build
all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

# Clean build artifacts and segment files
clean:
	rm -f $(TARGET) *.o inode_segment_*.seg data_segment_*.seg recovered.txt hello.txt *.hex test.txt
