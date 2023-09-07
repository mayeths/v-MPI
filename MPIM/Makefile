
CFLAGS=-Wall -Wextra -pthread -L-lmpi_monitor -I.

default: compile

compile: mpi_monitor.c mpi_monitor.h
	mpicc -o mpi_monitor.o -c mpi_monitor.c -Wall -pthread

clean:
	rm -f mpi_monitor.o
