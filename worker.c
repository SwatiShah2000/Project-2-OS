#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/shm.h>
#include <signal.h>
#include "shared.h"

#define NANOSECONDS_IN_SECOND 1000000000

int main(int argc, char *argv[]) {
    // Check command line arguments
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <seconds> <nanoseconds>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Parse arguments for time limit
    int waitSeconds = atoi(argv[1]);
    int waitNanoseconds = atoi(argv[2]);

    // Attach to shared memory (simulated clock)
    int shmid = shmget(SHM_KEY, sizeof(SimulatedClock), 0666);
    if (shmid < 0) {
        perror("worker: shmget");
        exit(EXIT_FAILURE);
    }

    SimulatedClock* clock_ptr = (SimulatedClock*)shmat(shmid, NULL, 0);
    if (clock_ptr == (void*)-1) {
        perror("worker: shmat");
        exit(EXIT_FAILURE);
    }

    // Calculate termination time
    int termSeconds = clock_ptr->seconds + waitSeconds;
    int termNanoseconds = clock_ptr->nanoseconds + waitNanoseconds;

    // Normalize termination time
    if (termNanoseconds >= NANOSECONDS_IN_SECOND) {
        termNanoseconds -= NANOSECONDS_IN_SECOND;
        termSeconds++;
    }

    // Print starting information
    printf("WORKER PID:%d PPID:%d SysClockS: %d SysclockNano: %d TermTimeS: %d TermTimeNano: %d\n",
           getpid(), getppid(), clock_ptr->seconds, clock_ptr->nanoseconds,
           termSeconds, termNanoseconds);
    printf("--Just Starting\n");

    // Variables to track the last printed second
    int lastPrintedSecond = clock_ptr->seconds;
    int secondsPassed = 0;

    // Loop until termination time is reached
    while (1) {
        // Check if it's time to terminate
        if (clock_ptr->seconds > termSeconds ||
            (clock_ptr->seconds == termSeconds && clock_ptr->nanoseconds >= termNanoseconds)) {
            // Final message before terminating
            printf("WORKER PID:%d PPID:%d SysClockS: %d SysclockNano: %d TermTimeS: %d TermTimeNano: %d\n",
                   getpid(), getppid(), clock_ptr->seconds, clock_ptr->nanoseconds,
                   termSeconds, termNanoseconds);
            printf("--Terminating\n");
            break;
        }

        // Check if a second has passed in the simulated clock
        if (clock_ptr->seconds > lastPrintedSecond) {
            secondsPassed = clock_ptr->seconds - lastPrintedSecond;
            printf("WORKER PID:%d PPID:%d SysClockS: %d SysclockNano: %d TermTimeS: %d TermTimeNano: %d\n",
                   getpid(), getppid(), clock_ptr->seconds, clock_ptr->nanoseconds,
                   termSeconds, termNanoseconds);
            printf("--%d seconds have passed since starting\n", secondsPassed);
            lastPrintedSecond = clock_ptr->seconds;
        }

        // Smaller delay to reduce CPU usage but still check clock frequently
        usleep(500);  // 0.5ms delay
    }

    // Detach from shared memory
    shmdt(clock_ptr);

    return EXIT_SUCCESS;
}
