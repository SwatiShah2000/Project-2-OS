#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <signal.h>
#include <time.h>
#include <getopt.h>
#include <string.h>
#include "shared.h"

#define MAX_PROCESSES 20
#define DEFAULT_PROC 5
#define DEFAULT_SIMUL 5
#define DEFAULT_TIME_LIMIT 5
#define DEFAULT_INTERVAL 100

// PCB structure
struct PCB {
    int occupied;       // Either true (1) or false (0)
    pid_t pid;          // Process ID
    int startSeconds;   // Time when it was forked
    int startNano;      // Time when it was forked
};

// Global variables
SimulatedClock *simulatedClock;
struct PCB processTable[MAX_PROCESSES];
int shmID;
int activeChildren = 0;
int totalChildren = 0;
int maxProc = DEFAULT_PROC;
int maxSimul = DEFAULT_SIMUL;
int timeLimit = DEFAULT_TIME_LIMIT;
int interval = DEFAULT_INTERVAL;
int running = 1;

// Function to handle signals
void signal_handler(int signum) {
    printf("Signal %d received. Terminating...\n", signum);
    running = 0;
}

// Function to clean up shared memory and kill child processes
void cleanup() {
    // Kill all active child processes
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processTable[i].occupied) {
            kill(processTable[i].pid, SIGTERM);
        }
    }

    // Detach and remove shared memory
    shmdt(simulatedClock);
    shmctl(shmID, IPC_RMID, NULL);

    printf("Cleanup complete. Exiting...\n");
}

// Function to increment the simulated clock
void incrementClock() {
    // Increment by a small amount (10ms = 10,000,000 nanoseconds)
    simulatedClock->nanoseconds += 10000000;
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
            if (processTable[i].occupied && processTable[i].pid == pid) {
                processTable[i].occupied = 0;
                activeChildren--;
                break;
            }
        }
    }
}

// Function to print the process table
void printProcessTable() {
    printf("Process Table:\n");
    printf("Entry\tOccupied\tPID\tStartS\tStartN\n");
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processTable[i].occupied) {
            printf("%d\t%d\t\t%d\t%d\t%d\n", i, processTable[i].occupied, processTable[i].pid,
                   processTable[i].startSeconds, processTable[i].startNano);
        } else {
            printf("%d\t%d\t\t%d\t%d\t%d\n", i, 0, 0, 0, 0);
        }
    }
}

// Function to print the system clock
void printClock() {
    printf("OSS PID: %d SysClockS: %d SysClockNano: %d\n", getpid(),
           simulatedClock->seconds, simulatedClock->nanoseconds);
}

// Function to launch a new child process
void launchNewChild() {
    // Check if we've reached the total limit of processes
    if (totalChildren >= maxProc) {
        return;
    }

    // Check if we've reached the simultaneous limit
    if (activeChildren >= maxSimul) {
        return;
    }

    // Check if enough simulated time has passed since the last launch
    static int lastLaunchSec = 0;
    static int lastLaunchNano = 0;

    long long currentTime = (long long)simulatedClock->seconds * 1000000000LL + simulatedClock->nanoseconds;
    long long lastLaunchTime = (long long)lastLaunchSec * 1000000000LL + lastLaunchNano;
    // Convert interval from milliseconds to nanoseconds
    long long intervalNano = (long long)interval * 1000000LL;

    if (currentTime - lastLaunchTime < intervalNano) {
        return;
    }

    // Find an empty slot in the process table
    int emptySlot = -1;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (!processTable[i].occupied) {
            emptySlot = i;
            break;
        }
    }

    if (emptySlot == -1) {
        return;  // No empty slots in the process table
    }

    // Generate random time for the child to run (between 1 and timeLimit seconds)
    // Use different seeds for each child process
    srand(time(NULL) ^ (getpid() << 16) ^ totalChildren);
    int childSec = (rand() % timeLimit) + 1;
    int childNano = rand() % 1000000000;

    // Fork a new process
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        return;
    } else if (pid == 0) {
        // Child process
        char secStr[20], nanoStr[20];
        sprintf(secStr, "%d", childSec);
        sprintf(nanoStr, "%d", childNano);

        execl("./worker", "worker", secStr, nanoStr, NULL);
        // If execl fails
        perror("execl");
        exit(EXIT_FAILURE);
    } else {
        // Parent process
        printf("Launched child PID %d with run time %d seconds %d nanoseconds\n", pid, childSec, childNano);

        // Update the process table
        processTable[emptySlot].occupied = 1;
        processTable[emptySlot].pid = pid;
        processTable[emptySlot].startSeconds = simulatedClock->seconds;
        processTable[emptySlot].startNano = simulatedClock->nanoseconds;

        // Update counts and last launch time
        activeChildren++;
        totalChildren++;
        lastLaunchSec = simulatedClock->seconds;
        lastLaunchNano = simulatedClock->nanoseconds;
    }
}

// Function to show help
void show_help() {
    printf("Usage: oss [-h] [-n proc] [-s simul] [-t timelimitForChildren] [-i intervalInMsToLaunchChildren]\n");
    printf("  -h                   : Show this help message\n");
    printf("  -n proc              : Total number of child processes to launch (default: %d)\n", DEFAULT_PROC);
    printf("  -s simul             : Maximum number of child processes to run simultaneously (default: %d)\n", DEFAULT_SIMUL);
    printf("  -t timelimit         : Upper bound (in seconds) for child process time (default: %d)\n", DEFAULT_TIME_LIMIT);
    printf("  -i interval          : Minimum interval (in ms) between child launches (default: %d)\n", DEFAULT_INTERVAL);
}

// Function to parse command line arguments
void parse_args(int argc, char *argv[]) {
    int opt;
    while ((opt = getopt(argc, argv, "hn:s:t:i:")) != -1) {
        switch (opt) {
            case 'h':
                show_help();
                exit(EXIT_SUCCESS);
            case 'n':
                maxProc = atoi(optarg);
                break;
            case 's':
                maxSimul = atoi(optarg);
                break;
            case 't':
                timeLimit = atoi(optarg);
                break;
            case 'i':
                interval = atoi(optarg);
                break;
            default:
                fprintf(stderr, "Invalid option. Use -h for help.\n");
                exit(EXIT_FAILURE);
        }
    }

    // Validate arguments
    if (maxProc <= 0 || maxSimul <= 0 || timeLimit <= 0 || interval <= 0) {
        fprintf(stderr, "All numeric arguments must be positive.\n");
        exit(EXIT_FAILURE);
    }

    printf("Configuration: maxProc=%d, maxSimul=%d, timeLimit=%d, interval=%d\n",
           maxProc, maxSimul, timeLimit, interval);
}

// Function to setup timer for 60 second timeout
void setup_timer() {
    // Set up the timer signal handler
    signal(SIGALRM, signal_handler);

    // Set up the timer for 60 seconds
    alarm(60);
}

int main(int argc, char *argv[]) {
    // Parse command line arguments
    parse_args(argc, argv);

    // Set up signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Set up timer for 60 second timeout
    setup_timer();

    // Initialize shared memory for the simulated clock
    shmID = shmget(SHM_KEY, sizeof(SimulatedClock), IPC_CREAT | 0666);
    if (shmID == -1) {
        perror("shmget");
        exit(EXIT_FAILURE);
    }

    simulatedClock = (SimulatedClock *)shmat(shmID, NULL, 0);
    if (simulatedClock == (void *)-1) {
        perror("shmat");
        exit(EXIT_FAILURE);
    }

    // Initialize the clock
    simulatedClock->seconds = 0;
    simulatedClock->nanoseconds = 0;

    // Initialize process table
    for (int i = 0; i < MAX_PROCESSES; i++) {
        processTable[i].occupied = 0;
    }

    // Variables for tracking when to print the process table
    int lastPrintSecond = -1;

    // Main loop
    while (running && totalChildren < maxProc) {
        // Increment clock
        incrementClock();

        // Check for terminated children
        checkForTerminatedChild();

        // Print process table every half second of simulated time
        if (simulatedClock->seconds > lastPrintSecond &&
            (simulatedClock->seconds % 1 == 0 && simulatedClock->nanoseconds >= 500000000)) {
            printClock();
            printProcessTable();
            lastPrintSecond = simulatedClock->seconds;
        }

        // Launch new children when appropriate
        launchNewChild();

        // Short delay to prevent busy waiting
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 1000;  // 1 millisecond
        select(0, NULL, NULL, NULL, &tv);
    }

    // Wait for any remaining children to finish
    printf("Waiting for remaining child processes to terminate...\n");
    while (activeChildren > 0) {
        checkForTerminatedChild();
        usleep(10000);  // 10ms delay
    }

    // Give children a short time to output their final messages
    sleep(1);

    // Clean up
    cleanup();

    return EXIT_SUCCESS;
}
