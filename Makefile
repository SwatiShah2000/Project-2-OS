# Makefile for Project 2 (OSS and Worker)

CC = gcc
CFLAGS = -Wall -g
DEPS = shared.h
OBJS_OSS = oss.o
OBJS_WORKER = worker.o

# Target to build both executables
all: oss worker

# Build oss executable
oss: $(OBJS_OSS)
	$(CC) $(CFLAGS) -o oss $(OBJS_OSS)

# Build worker executable
worker: $(OBJS_WORKER)
	$(CC) $(CFLAGS) -o worker $(OBJS_WORKER)

# Compile object files
oss.o: oss.c $(DEPS)
	$(CC) $(CFLAGS) -c oss.c

worker.o: worker.c $(DEPS)
	$(CC) $(CFLAGS) -c worker.c

# Clean up generated files
clean:
	rm -f *.o oss worker

# Run make clean, then build
rebuild: clean all
