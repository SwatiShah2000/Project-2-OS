#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <signal.h>

#define MAX_PROCESSES 20
#define MAX_WORK_TIME 10  // Seconds after which the child process will terminate

// Simulated clock structure
typedef struct {
    int seconds;
    int nanoseconds;
} SysClock;

struct PCB {
    int occupied; // Either true or false
    pid_t pid;    // Process ID
    int startSeconds;
    int startNano;
};

SysClock *simulatedClock;  // Shared memory for the simulated clock
struct PCB processTable[MAX_PROCESSES];

// Function to increment the simulated clock by 10 milliseconds
void incrementClock() {
    simulatedClock->nanoseconds += 10000000;  // Increment 10 milliseconds for example
    if (simulatedClock->nanoseconds >= 1000000000) {
        simulatedClock->nanoseconds -= 1000000000;
        simulatedClock->seconds++;
    }
}

// Function to handle child process termination without blocking
void checkForTerminatedChild() {
    int status;
    pid_t pid = waitpid(-1, &status, WNOHANG);  // Non-blocking check for terminated child
    if (pid > 0) {
        printf("Child process %d terminated\n", pid);
        for (int i = 0; i < MAX_PROCESSES; i++) {
            if (processTable[i].pid == pid) {
                processTable[i].occupied = 0;
                break;
            }
        }
    }
}

// Function to print the process table
void printProcessTable() {
    printf("Process Table:\n");
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processTable[i].occupied) {
            printf("%d\t%d\t%d\t%d\t%d\n", i, processTable[i].occupied, processTable[i].pid,
                   processTable[i].startSeconds, processTable[i].startNano);
        }
    }
}

// Function to print the system clock
void printClock() {
    printf("OSS PID: %d SysClockS: %d SysClockNano: %d\n", getpid(),
           simulatedClock->seconds, simulatedClock->nanoseconds);
}

int main(int argc, char *argv[]) {
    // Initialize shared memory for the simulated clock
    int shmID = shmget(IPC_PRIVATE, sizeof(SysClock), IPC_CREAT | 0666);
    if (shmID == -1) {
        perror("shmget");
        exit(1);
    }
    simulatedClock = (SysClock *)shmat(shmID, NULL, 0);
    if (simulatedClock == (void *)-1) {
        perror("shmat");
        exit(1);
    }

    // Initialize the clock
    simulatedClock->seconds = 0;
    simulatedClock->nanoseconds = 0;

    // Initialize process table
    for (int i = 0; i < MAX_PROCESSES; i++) {
        processTable[i].occupied = 0;  // All entries are initially unoccupied
    }

    // Main loop for OSS simulation
    int numChildren = 0;
    int maxChildren = 5;  // Set the limit for child processes (example)
    while (numChildren < maxChildren) {
        // Simulate time increment by busy-waiting
        incrementClock();

        // Check if any child has terminated
        checkForTerminatedChild();

        // Print the process table and clock
        if (simulatedClock->seconds % 1 == 0) {
            printClock();
            printProcessTable();
        }

        // Simulate launching a new child process if conditions are met
        if (numChildren < maxChildren) {
            printf("Forking a new child process...\n");
            pid_t pid = fork();
            if (pid == 0) {
                // Worker code (Child Process)
                int workTime = 0;
                while (workTime < MAX_WORK_TIME) {
                    printf("Child process working...\n");
                    // Simulate work by busy-wait loop instead of sleep
                    for (volatile int i = 0; i < 100000000; i++);
                    workTime++;
                }
                printf("Child process %d terminating after %d seconds\n", getpid(), workTime);
                exit(0);  // Terminate the child process
            } else if (pid > 0) {
                // Parent code (OSS)
                printf("Parent created child with PID: %d\n", pid);
                for (int i = 0; i < MAX_PROCESSES; i++) {
                    if (processTable[i].occupied == 0) {
                        processTable[i].occupied = 1;
                        processTable[i].pid = pid;
                        processTable[i].startSeconds = simulatedClock->seconds;
                        processTable[i].startNano = simulatedClock->nanoseconds;
                        numChildren++;
                        break;
                    }
                }
            }
        }

        // Use select to allow for non-blocking wait and check every 1 millisecond
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 1000;  // 1 millisecond
        select(0, NULL, NULL, NULL, &tv);
    }

    // Cleanup
    shmdt(simulatedClock);
    shmctl(shmID, IPC_RMID, NULL);

    return 0;
}

