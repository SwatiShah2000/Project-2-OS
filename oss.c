#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>

#define SHM_KEY 0x1234
#define MAX_PROCESSES 20

// Structure for the simulated clock
typedef struct {
    int seconds;
    int nanoseconds;
} SimulatedClock;

// Structure for the process table
typedef struct {
    int occupied;
    pid_t pid;
    int startSeconds;
    int startNano;
} ProcessControlBlock;

// Global variables
SimulatedClock *simClock;
ProcessControlBlock processTable[MAX_PROCESSES];
int shm_id;
int max_runtime_seconds = 10;  // Max runtime before oss terminates

// Function to increment simulated clock
void incrementClock() {
    simClock->nanoseconds += 10000000;  // Increment by 10ms
    if (simClock->nanoseconds >= 1000000000) {
        simClock->seconds++;
        simClock->nanoseconds -= 1000000000;
    }
}

// Function to clean up shared memory on termination
void cleanup(int signum) {
    shmdt(simClock);
    shmctl(shm_id, IPC_RMID, NULL);
    printf("\nOSS: Cleanup complete. Exiting.\n");
    exit(0);
}

int main(int argc, char *argv[]) {
    // Handle Ctrl+C to clean up shared memory
    signal(SIGINT, cleanup);

    // Create shared memory for the clock
    shm_id = shmget(SHM_KEY, sizeof(SimulatedClock), IPC_CREAT | 0666);
    simClock = (SimulatedClock *)shmat(shm_id, NULL, 0);
    simClock->seconds = 0;
    simClock->nanoseconds = 0;

    // Initialize process table
    for (int i = 0; i < MAX_PROCESSES; i++) {
        processTable[i].occupied = 0;
    }

    int activeProcesses = 0;

    // Main loop: Fork worker processes and track time
    while (simClock->seconds < max_runtime_seconds) {
        incrementClock();
        usleep(500000);  // Simulate time passing (500ms)

        // Print process table every 0.5 simulated second
        if (simClock->nanoseconds % 500000000 == 0) {
            printf("OSS PID: %d SysClockS: %d SysClockNano: %d\n", getpid(), simClock->seconds, simClock->nanoseconds);
            printf("Process Table:\n");
            printf("Entry | Occupied | PID  | StartS | StartNano\n");
            for (int i = 0; i < MAX_PROCESSES; i++) {
                if (processTable[i].occupied) {
                    printf("%d     | %d        | %d  | %d      | %d\n",
                           i, processTable[i].occupied, processTable[i].pid,
                           processTable[i].startSeconds, processTable[i].startNano);
                }
            }
        }

        // Fork new workers if needed
        if (activeProcesses < 3) {  // Change 3 to the value you want
            pid_t child_pid = fork();

            if (child_pid < 0) {
                perror("Fork failed");
                exit(1);
            } else if (child_pid == 0) {  // Child process
                printf("OSS: Forked worker with PID %d\n", getpid());

                // Generate random time for the worker
                char sec_arg[10], nano_arg[10];
                sprintf(sec_arg, "%d", (rand() % 7) + 1);   // Random 1-7 seconds
                sprintf(nano_arg, "%d", rand() % 1000000000); // Random nanoseconds

                execl("./worker", "worker", sec_arg, nano_arg, NULL);
                perror("Exec failed");
                exit(1);
            } else {  // Parent process (oss)
                // Store worker in process table
                for (int i = 0; i < MAX_PROCESSES; i++) {
                    if (processTable[i].occupied == 0) {
                        processTable[i].occupied = 1;
                        processTable[i].pid = child_pid;
                        processTable[i].startSeconds = simClock->seconds;
                        processTable[i].startNano = simClock->nanoseconds;
                        activeProcesses++;
                        break;
                    }
                }
            }
        }

        // Check for terminated processes
        int status;
        pid_t pid;
        while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
            printf("OSS: Worker process %d terminated.\n", pid);
            for (int i = 0; i < MAX_PROCESSES; i++) {
                if (processTable[i].pid == pid) {
                    processTable[i].occupied = 0;
                    activeProcesses--;
                    break;
                }
            }
        }
    }

    // Cleanup and exit
    cleanup(0);
    return 0;
}

