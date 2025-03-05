#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/shm.h>
#include <signal.h>
#include <time.h>

#define NANOSECONDS_IN_SECOND 1000000000

// Structure for the simulated clock
typedef struct {
    int seconds;
    int nanoseconds;
} SimulatedClock;

// Function to convert time to nanoseconds
long long toNanoseconds(int sec, int nano) {
    return (long long)sec * NANOSECONDS_IN_SECOND + nano;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <sec> <nano>\n", argv[0]);
        exit(1);
    }

    int waitSeconds = atoi(argv[1]);
    int waitNanoseconds = atoi(argv[2]);

    // Get shared memory ID for simulated clock
    int shm_id = shmget(IPC_PRIVATE, sizeof(SimulatedClock), 0666);
    if (shm_id < 0) {
        perror("shmget failed");
        exit(1);
    }

    SimulatedClock* clock_ptr = (SimulatedClock*)shmat(shm_id, NULL, 0);
    if (clock_ptr == (void*)-1) {
        perror("shmat failed");
        exit(1);
    }

    // Calculate the target termination time
    long long targetTime = toNanoseconds(clock_ptr->seconds, clock_ptr->nanoseconds) +
                           toNanoseconds(waitSeconds, waitNanoseconds);

    printf("WORKER PID: %d PPID: %d SysClockS: %d SysClockNano: %d TermTimeS: %d TermTimeNano: %d\n",
           getpid(), getppid(), clock_ptr->seconds, clock_ptr->nanoseconds, waitSeconds, waitNanoseconds);
    printf("--Just Starting\n");

    // Poll the clock until the target time is reached
    while (1) {
        long long currentTime = toNanoseconds(clock_ptr->seconds, clock_ptr->nanoseconds);

        if (currentTime >= targetTime) {
            // Terminate once the target time is reached
            printf("WORKER PID: %d PPID: %d SysClockS: %d SysClockNano: %d TermTimeS: %d TermTimeNano: %d\n",
                   getpid(), getppid(), clock_ptr->seconds, clock_ptr->nanoseconds, waitSeconds, waitNanoseconds);
            printf("--Terminating\n");
            break;
        }

        // Output periodic messages every second
        if (clock_ptr->nanoseconds % NANOSECONDS_IN_SECOND == 0) {
            printf("WORKER PID: %d PPID: %d SysClockS: %d SysClockNano: %d TermTimeS: %d TermTimeNano: %d\n",
                   getpid(), getppid(), clock_ptr->seconds, clock_ptr->nanoseconds, waitSeconds, waitNanoseconds);
            printf("--%d seconds have passed since starting\n", clock_ptr->seconds);
        }

        // Polling delay (no sleep here)
        usleep(100000);  // You can adjust this based on desired granularity
    }

    return 0;
}

