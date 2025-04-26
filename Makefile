# Makefile for ExFS2

CC = gcc
CFLAGS = -Wall -Wextra -g
TARGET = exfs2
SRC = exfs2.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET) inode_segment_*.seg data_segment_*.seg recovered.txt hello.txt *.o
