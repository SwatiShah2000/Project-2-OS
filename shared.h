#ifndef SHARED_H
#define SHARED_H

// Shared memory key
#define SHM_KEY 0x1234

// Simulated clock structure
typedef struct {
    int seconds;
    int nanoseconds;
} SimulatedClock;

#endif
