#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/shm.h>
#include <time.h>

struct SharedClock {
    int seconds;
    int nanoseconds;
};

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <seconds> <nanoseconds>\n", argv[0]);
        exit(1);
    }

    // Parse arguments for time limit
    int limit_seconds = atoi(argv[1]);
    int limit_nanoseconds = atoi(argv[2]);

    // Attach to shared memory (simulated clock)
    int shmid_clock = shmget(IPC_PRIVATE, sizeof(struct SharedClock), 0666);
    if (shmid_clock < 0) {
        perror("shmget for clock");
        exit(1);
    }

    struct SharedClock *clock_ptr = (struct SharedClock *)shmat(shmid_clock, NULL, 0);
    if (clock_ptr == (void *)-1) {
        perror("shmat for clock");
        exit(1);
    }

    // Compute termination time
    int term_seconds = clock_ptr->seconds + limit_seconds;
    int term_nanoseconds = clock_ptr->nanoseconds + limit_nanoseconds;
    if (term_nanoseconds >= 1000000000) {
        term_nanoseconds -= 1000000000;
        term_seconds += 1;
    }

    // Print starting information
    printf("WORKER PID:%d PPID:%d SysClockS:%d SysClockNano:%d TermTimeS:%d TermTimeNano:%d --Just Starting\n",
           getpid(), getppid(), clock_ptr->seconds, clock_ptr->nanoseconds, term_seconds, term_nanoseconds);

    // Loop until termination time
    while (clock_ptr->seconds < term_seconds || (clock_ptr->seconds == term_seconds && clock_ptr->nanoseconds < term_nanoseconds)) {
        if (clock_ptr->seconds > term_seconds || (clock_ptr->seconds == term_seconds && clock_ptr->nanoseconds >= term_nanoseconds)) {
            break;
        }

        // Periodically output system clock and remaining time
        if (clock_ptr->nanoseconds % 500000000 == 0) {
            printf("WORKER PID:%d PPID:%d SysClockS:%d SysClockNano:%d TermTimeS:%d TermTimeNano:%d -- %d seconds have passed since starting\n",
                   getpid(), getppid(), clock_ptr->seconds, clock_ptr->nanoseconds, term_seconds, term_nanoseconds, clock_ptr->seconds - 5);
        }

        usleep(100000);  // Wait for a short time before checking again
    }

    // Output termination message
    printf("WORKER PID:%d PPID:%d SysClockS:%d SysClockNano:%d TermTimeS:%d TermTimeNano:%d --Terminating\n",
           getpid(), getppid(), clock_ptr->seconds, clock_ptr->nanoseconds, term_seconds, term_nanoseconds);

    // Detach shared memory
    shmdt(clock_ptr);

    return 0;
}
