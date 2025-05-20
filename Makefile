CC = /usr/bin/clang++
CFLAGS = -std=c++17

tinysharp: compiler.cpp compiler.h machine.cpp machine.h table.h opcodes.h
	$(CC) $(CFLAGS) -o tinysharp compiler.cpp machine.cpp
