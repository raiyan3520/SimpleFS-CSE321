CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -std=c11

.PHONY: all clean
all: simplefs_builder simplefs_adder

simplefs_builder: simplefs_builder.c simplefs.h
	$(CC) $(CFLAGS) simplefs_builder.c -o $@

simplefs_adder: simplefs_adder.c simplefs.h
	$(CC) $(CFLAGS) simplefs_adder.c -o $@

clean:
	rm -f simplefs_builder simplefs_adder disk.img
