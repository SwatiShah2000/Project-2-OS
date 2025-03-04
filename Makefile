# Makefile for Project 2 (OSS and Worker)

CC = gcc
CFLAGS = -Wall -g
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

# Compile oss object file
oss.o: oss.c
	$(CC) $(CFLAGS) -c oss.c

# Compile worker object file
worker.o: worker.c
	$(CC) $(CFLAGS) -c worker.c

# Clean up generated files
clean:
	rm -f *.o oss worker

# Run make clean, then build
rebuild: clean all
