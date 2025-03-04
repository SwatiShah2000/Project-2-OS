#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <time.h>

#define SHM_KEY 0x1234

typedef struct {
    int seconds;
    int nanoseconds;
} SimulatedClock;

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <seconds> <nanoseconds>\n", argv[0]);
        exit(1);
    }

    // Parse max execution time from command-line arguments
    int max_seconds = atoi(argv[1]);
    int max_nanoseconds = atoi(argv[2]);

    // Attach to shared memory
    int shm_id = shmget(SHM_KEY, sizeof(SimulatedClock), 0666);
    if (shm_id < 0) {
        perror("Worker: Shared memory access failed");
        exit(1);
    }

    SimulatedClock *simClock = (SimulatedClock *)shmat(shm_id, NULL, 0);
    if (simClock == (void *)-1) {
        perror("Worker: Shared memory attach failed");
        exit(1);
    }

    // Calculate termination time
    int target_seconds = simClock->seconds + max_seconds;
    int target_nanoseconds = simClock->nanoseconds + max_nanoseconds;
    if (target_nanoseconds >= 1000000000) {
        target_seconds++;
        target_nanoseconds -= 1000000000;
    }

    printf("WORKER PID:%d PPID:%d SysClockS:%d SysClockNano:%d TermTimeS:%d TermTimeNano:%d -- Just Starting\n",
           getpid(), getppid(), simClock->seconds, simClock->nanoseconds, target_seconds, target_nanoseconds);

    // Track last reported second for "Still Running" output
    int last_reported_sec = simClock->seconds;
    
    // Backup exit condition (real-time timeout)
    time_t start_time = time(NULL);  

    // Loop until termination time is reached
    while (1) {
        int current_seconds = simClock->seconds;
        int current_nanoseconds = simClock->nanoseconds;

        // Check if worker should terminate
        if (current_seconds > target_seconds ||
           (current_seconds == target_seconds && current_nanoseconds >= target_nanoseconds)) {
            printf("WORKER PID:%d PPID:%d SysClockS:%d SysClockNano:%d -- Terminating\n",
                  getpid(), getppid(), current_seconds, current_nanoseconds);
            break;
        }

        // Print "Still Running" once per second
        if (current_seconds > last_reported_sec) {
            last_reported_sec = current_seconds;
            printf("WORKER PID:%d PPID:%d SysClockS:%d SysClockNano:%d -- Still Running\n",
                   getpid(), getppid(), current_seconds, current_nanoseconds);
        }

        // **Backup Exit Condition: Exit after 10 seconds real-time**
        if (time(NULL) - start_time > 10) {
            printf("WORKER PID:%d PPID:%d -- Backup Exit Triggered (Real-time timeout)\n", getpid(), getppid());
            break;
        }

        usleep(50000);  // Prevent CPU overuse
    }

    // Print confirmation before exiting
    printf("WORKER PID:%d PPID:%d SysClockS:%d SysClockNano:%d -- Exiting Normally\n",
           getpid(), getppid(), simClock->seconds, simClock->nanoseconds);

    // Detach from shared memory and exit
    shmdt(simClock);
    return 0;
}

