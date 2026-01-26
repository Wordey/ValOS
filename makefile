.POSIX: 
.PHONY: all clean

TARGET = make_disk
CC = clang
#CC = gcc
CFLAGS = -std=c17 -Wextra -Wpedantic -O2

all: $(TARGET)

clean:
	rm -rf $(TARGET) *.img
