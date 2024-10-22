/*
* Solution S2 Operating Systems - Forks
* Curs 2024-25
*
* @author: Cristina Martí
*
*/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <string.h>

#define printF(X) write(1, X, strlen(X))

// TEXT COLORS
#define C_RESET   "\033[0m"
#define C_RED     "\033[31m"
#define C_GREEN   "\033[32m"
#define C_YELLOW  "\033[33m"
#define C_BLUE    "\033[34m"
#define C_MAGENTA "\033[35m"

int ready_to_start = 0;

/**
 * Signal handler for SIGUSR1.
 * Sets the flag to start performance for sections.
 */
void signal_handler() {
    ready_to_start = 1;
    printF( "Section is ready to start.\n" );
}

void nothing(){
    signal(SIGINT, nothing);
}

/**
 * Sets up signal handling for SIGUSR1.
 */
void setup_signals() {
    signal(SIGUSR1, signal_handler);
    signal(SIGINT, nothing);
}

/**
 * Performs the instrument's specific piece, waiting for the signal to start.
 * 
 * @param section_name The name of the section (e.g., "Strings").
 * @param instrument_id The specific instrument ID within the section.
 */
void perform_instrument(char* section_name, int instrument_id) {
   
    char *message;
    if (strcmp(section_name, "Strings") == 0) {
        if (instrument_id == 1 || instrument_id == 2) {  // Violins
            asprintf(&message, C_GREEN "%s Violin %d is playing: Re\n" C_RESET, section_name, instrument_id);
        } else {  // Viola
            asprintf(&message, C_GREEN "%s Viola is playing: Do\n" C_RESET, section_name);
        }
    } else if (strcmp(section_name, "Wind") == 0) {
        if (instrument_id == 1) {  // Flute
            asprintf(&message, C_BLUE"%s Flute is playing: Do Do\n" C_RESET, section_name);
        } else {  // Clarinets
            asprintf(&message, C_BLUE "%s Clarinet %d is playing: Re Re\n" C_RESET, section_name, instrument_id - 1);
        }
    } else if (strcmp(section_name, "Percussion") == 0) {
        if (instrument_id == 1) {  // Vibraphone
            asprintf(&message, C_MAGENTA "%s Vibraphone are playing: Do Re Mi\n" C_RESET, section_name);
        } else {  // Triangle
            asprintf(&message, C_MAGENTA "%s Triangle are playing: Do Re Re Mi\n" C_RESET, section_name);
        }
    }
    printF(message);
    free(message);
    sleep(2); 
    exit(0);  
}

/**
 * Main function to set up the concert.
 */
int main(int argc, char*argv) {

    if(argc != 1){
        printF("Error: No arguments needed.\n");
        return 1;
    }


    setup_signals(); 
    pid_t pid_director = getpid();

    char *message;
    asprintf(&message, C_YELLOW "Director (PID %d) starting the concert. Use 'kill -SIGUSR1 PID' to start sections.\n", pid_director);
    printF(message);
    free(message);
    
    while (!ready_to_start) {     
        pause();
    } 
    ready_to_start = 0; 
    int totalCreated=0;
    pid_t pid_section = fork();
    if(pid_section <0){
        printF("Error creating fork.\n ");
    }else if (pid_section == 0) {  
        for (int i = 0; i < 3; i++) {
            pid_t pid_instrument = fork();
            if(pid_instrument <0){
                printF("Error creating fork.\n ");
                totalCreated--;
            }else if (pid_instrument == 0) {  
                perform_instrument("Strings", i + 1);
            }
            totalCreated++;
        }
        for (int i = 0; i < totalCreated; i++) {
            wait(NULL);
        }
        exit(0);
    }

    wait(NULL);  
    printF( "Director: Strings section completed.\n ");
    printF(C_RED"Waiting to start Wind section.\n" C_RESET);
    
  
    while (!ready_to_start) {     
        pause();
    } 
    ready_to_start = 0;
    
    totalCreated = 0;
    pid_section = fork();
    if(pid_section <0){
        printF("Error creating fork.\n ");
    }else if (pid_section == 0) {  
        for (int i = 0; i < 2; i++) {  
            pid_t pid_instrument = fork();
            if(pid_instrument <0){
                printF("Error creating fork.\n ");
                totalCreated--;
            }else if (pid_instrument == 0) {
                perform_instrument("Wind", i + 1);
            }
            totalCreated++;
        }
        for (int i = 0; i < totalCreated; i++) {
            wait(NULL);
        }
        exit(0);
    }

    wait(NULL);  

    printF( "Director: Wind section completed.\n ");
    printF(C_RED"Waiting to start Percussion section.\n" C_RESET);
    
     while (!ready_to_start) {     
        pause();
    } 
    ready_to_start = 0;
    
    totalCreated = 0;
    pid_section = fork();
    if(pid_section <0){
        printF("Error creating fork.\n ");
    }else if (pid_section == 0) {  
        for (int i = 0; i < 2; i++) {  
            pid_t pid_instrument = fork();
            if(pid_instrument <0){
                printF("Error creating fork.\n ");
                totalCreated--;
            }else if (pid_instrument == 0) {
                perform_instrument("Percussion", i + 1);
            }
            totalCreated++;
        }
        for (int i = 0; i < totalCreated; i++) {
            wait(NULL);
        }
        exit(0);
    }

    wait(NULL); 

    printF("Director: Percussion section completed.\n ");
    printF("\nConcert finished successfully.\n" );

    return 0;
}
