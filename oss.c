#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/shm.h>
#include <signal.h>
#include <time.h>

#define MAX_PROCESSES 20

// Structure for Process Control Block (PCB)
struct PCB {
    int occupied; // 1 if occupied, 0 if free
    pid_t pid;    // Process ID of the child
    int startSeconds; // Time when it was forked (simulated seconds)
    int startNano;    // Time when it was forked (simulated nanoseconds)
};

// Shared memory for simulated clock
struct SharedClock {
    int seconds;
    int nanoseconds;
};

int shmid_clock;
int shmid_pcb;
struct SharedClock *clock_ptr;
struct PCB *processTable;

void incrementClock() {
    // Increment simulated clock (seconds and nanoseconds)
    clock_ptr->nanoseconds += 500000000; // Increment 0.5 seconds
    if (clock_ptr->nanoseconds >= 1000000000) {
        clock_ptr->nanoseconds -= 1000000000;
        clock_ptr->seconds += 1;
    }
}

void updatePCB(pid_t pid) {
    // Update process table to reflect termination of a child process
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processTable[i].pid == pid) {
            processTable[i].occupied = 0;
            break;
        }
    }
}

int main(int argc, char *argv[]) {
    // Set up shared memory for the clock
    shmid_clock = shmget(IPC_PRIVATE, sizeof(struct SharedClock), IPC_CREAT | 0666);
    if (shmid_clock < 0) {
        perror("shmget for clock");
        exit(1);
    }

    clock_ptr = (struct SharedClock *)shmat(shmid_clock, NULL, 0);
    if (clock_ptr == (void *)-1) {
        perror("shmat for clock");
        exit(1);
    }

    // Initialize the simulated clock
    clock_ptr->seconds = 0;
    clock_ptr->nanoseconds = 0;

    // Set up shared memory for the process table
    shmid_pcb = shmget(IPC_PRIVATE, sizeof(struct PCB) * MAX_PROCESSES, IPC_CREAT | 0666);
    if (shmid_pcb < 0) {
        perror("shmget for process table");
        exit(1);
    }

    processTable = (struct PCB *)shmat(shmid_pcb, NULL, 0);
    if (processTable == (void *)-1) {
        perror("shmat for process table");
        exit(1);
    }

    // Initialize process table
    for (int i = 0; i < MAX_PROCESSES; i++) {
        processTable[i].occupied = 0;
    }

    // Simulate launching children and incrementing clock
    while (1) {
        incrementClock();
        sleep(1); // To simulate time passing

        // Periodically output the process table every 0.5 second
        if (clock_ptr->nanoseconds % 500000000 == 0) {
            printf("OSS PID: %d SysClockS: %d SysClockNano: %d\n", getpid(), clock_ptr->seconds, clock_ptr->nanoseconds);
            printf("Process Table:\n");
            printf("Entry Occupied PID StartS StartN\n");
            for (int i = 0; i < MAX_PROCESSES; i++) {
                printf("%d %d %d %d %d\n", i, processTable[i].occupied, processTable[i].pid, processTable[i].startSeconds, processTable[i].startNano);
            }
        }

        // Check for terminated processes
        int status;
        pid_t pid = waitpid(-1, &status, WNOHANG);
        if (pid > 0) {
            updatePCB(pid);
        }

        // Simulate launching new child processes (for example)
        // In a real program, you would fork here and assign child process info to the PCB
    }

    // Cleanup shared memory
    shmdt(clock_ptr);
    shmdt(processTable);
    shmctl(shmid_clock, IPC_RMID, NULL);
    shmctl(shmid_pcb, IPC_RMID, NULL);

    return 0;
}
