main:
	mpicc -Wall -Wl,-stack_size,0xF0000000 main.c

