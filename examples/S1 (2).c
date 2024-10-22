/*
* Solution S1 Operating Systems - Signals
* Curs 2023-24
*
* @author: Cristina Martí
*
*/

#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <string.h>

#include <sys/types.h>
#include <sys/signal.h>
#include <signal.h>
#include <fcntl.h>

// Text colors for terminal outputs
#define C_RESET   "\033[0m"
#define C_RED     "\033[31m"
#define C_GREEN   "\033[32m"
#define C_YELLOW  "\033[33m"
#define C_BLUE    "\033[34m"
#define C_MAGENTA "\033[35m"

#define OXYGEN_CRITICAL_INTERVAL 5   // Critical interval for oxygen emergency in seconds
#define SOLAR_STORM_DELAY 10         // Delay for simulating solar storm interference in seconds

#define printF(x) write(1, x, strlen(x))  // Macro to simplify writing strings to stdout

// Global volatile variables to track the state of the drone
int critical_oxygen_state = 0;
int energy_failure_state = 0;
int in_solar_storm = 0;
int sigusr2_count = 0;
int sigusr2_oxygen = 0;
time_t first_sigusr2_time;
time_t last_sigusr2_time;

// Prototypes for signal handling functions
void handleSIGUSR1();   // Handle critical oxygen situation
void handleSIGUSR2();   // Handle energy failure situation
void handleSIGALRM();   // Handle solar storm interference
void handleSIGINT();    // Handle successful rescue operation
void handleSIGHUP();    // Generate a status report
void logState(char* state);



/**
 * Checks if the SIGUSR2 signal has been received and resets the counter.
*/
void checkSigurs2Oxygen(){
    if(sigusr2_oxygen > 0 ){
        sigusr2_oxygen = 0;
    }
}

/**
 * Handles the SIGUSR1 signal for oxygen critical state initiation.
 */
void handleSIGUSR1() {
    time_t current_time = time(NULL);
    if (in_solar_storm == 1){
        printF(C_YELLOW "Solar storm detected.\n" C_RESET);
    } else {
        if (difftime(current_time, first_sigusr2_time) <= OXYGEN_CRITICAL_INTERVAL && sigusr2_oxygen == 1) {
            critical_oxygen_state = 1;
            energy_failure_state = 0;
            printF(C_RED "Critical oxygen state detected.\n" C_RESET);
        } else if (difftime(current_time, first_sigusr2_time) > OXYGEN_CRITICAL_INTERVAL && sigusr2_oxygen == 1) {
            printF(C_GREEN "Oxygen state stabilized.\n" C_RESET);
        }
        checkSigurs2Oxygen();
    }

    signal(SIGUSR1, handleSIGUSR1);
   
}

/**
 * Handles the SIGUSR2 signal for detecting energy failures.
 */
void handleSIGUSR2() {
    time_t current_time = time(NULL);

    checkSigurs2Oxygen();
    if (in_solar_storm == 1){
        printF(C_YELLOW "Solar storm detected.\n" C_RESET);
    } else {
        sigusr2_count++;
        sigusr2_oxygen = 1;

        if (sigusr2_count >= 2 && difftime(current_time, last_sigusr2_time) < OXYGEN_CRITICAL_INTERVAL) {
            energy_failure_state = 1;
            critical_oxygen_state = 0;
            printF(C_RED"Energy failure detected.\n"C_RESET);
        }
        last_sigusr2_time = current_time;
        first_sigusr2_time = current_time;
    }

    signal(SIGUSR2, handleSIGUSR2);
}

/**
 * Handles the SIGALRM signal, simulating solar storm interference.
 */
void handleSIGALRM() {
    checkSigurs2Oxygen();
    if(in_solar_storm == 1){
        in_solar_storm = 0;
        printF("End of solar storm. All systems operational.\n");
        printF(C_YELLOW "Signals unblocked\n" C_RESET);
    }else{
        alarm(SOLAR_STORM_DELAY);
        in_solar_storm = 1;
        printF(C_YELLOW "Signals blocked\n" C_RESET);
        printF("Solar storm detected. All systems paused.\n");
    }    

    signal(SIGALRM, handleSIGALRM);
}

/**
 * Generates a detailed status report upon receiving SIGHUP.
 */
void handleSIGHUP() {
    checkSigurs2Oxygen();
    if (in_solar_storm == 1){
        printF(C_YELLOW "Solar storm detected.\n" C_RESET);
    } else {
        if (critical_oxygen_state) {
            logState("Report: Critical oxygen level.");
            printF(C_BLUE "Report: Critical oxygen level.\n" C_RESET);
        } else if (energy_failure_state) {
            logState("Report: Energy failure.");
            printF(C_BLUE "Report: Energy failure.\n" C_RESET);
        } else {
            logState("Report: Normal state.");
            printF(C_BLUE "Report: Normal state.\n" C_RESET);
        }
    }
    signal(SIGHUP, handleSIGHUP);
}

/**
 * Writes the current state to a file.
 * @param state The current state description to write to the file.
 */
void logState(char* state) {
     if (in_solar_storm == 1){
        printF(C_YELLOW "Solar storm detected.\n" C_RESET);
    } else {
        int fd = open("drone_state.txt", O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (fd == -1) {
            printF("Error opening file");
            return;
        }
        tzset();
        char *output;        
        char* time_string = ctime(&(time_t){time(NULL)});
        asprintf(&output, "Time: %sState: %s\n", time_string, state);
        write(fd, output, strlen(output));
        free(output);
        close(fd);
    }
}

/**
 * Handles the SIGINT signal, indicating a successful rescue operation.
 */
void handleSIGINT() {
    checkSigurs2Oxygen();
    if (in_solar_storm == 1){
        printF(C_YELLOW "Solar storm detected.\n" C_RESET);
        signal(SIGINT, handleSIGINT);
    } else {
        printF(C_MAGENTA "Rescue mission successful\n" C_RESET);
        signal(SIGINT, SIG_DFL);
        raise(SIGINT);
    }
}

int main() {
    char *pid;
    asprintf(&pid, "Process PID: %d\n", getpid());
    printF(pid);
    free(pid);

    // Configure signal handlers
    signal(SIGUSR1, handleSIGUSR1);
    signal(SIGUSR2, handleSIGUSR2);
    signal(SIGALRM, handleSIGALRM);
    signal(SIGINT, handleSIGINT);
    signal(SIGHUP, handleSIGHUP);

    // Initialize timing variables
    first_sigusr2_time = time(NULL);
    last_sigusr2_time = time(NULL);

    printF(C_MAGENTA "Rescue Drone AION initialized, waiting for signals...\n" C_RESET);

    while (1) {
        pause();  // Active waiting for signals
    }

    return 0;
}
