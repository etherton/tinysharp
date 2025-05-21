CC = /usr/bin/clang++
CFLAGS = -std=c++17

tinysharp: compiler.cpp compiler.h machine.cpp machine.h table.cpp table.h opcodes.h node.cpp node.h
	$(CC) $(CFLAGS) -o tinysharp compiler.cpp machine.cpp node.cpp table.cpp
