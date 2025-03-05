#ifndef SHARED_H
#define SHARED_H

#define SHM_KEY 0x1234  // Define the shared memory key

typedef struct {
    int seconds;
    int nanoseconds;
} SimulatedClock;

#endif
