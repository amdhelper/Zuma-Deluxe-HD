#include "Application.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include "global/HQC.h"

#include <unistd.h>
#include <string.h>

void signal_handler(int signum) {
    const char* msg = "CRITICAL ERROR: Received signal ";
    write(STDERR_FILENO, msg, strlen(msg));
    
    char num[32];
    int len = snprintf(num, sizeof(num), "%d\n", signum);
    write(STDERR_FILENO, num, len);
    
    // Attempt to flush stdout just in case
    // fflush(stdout); // unsafe, but might help
    
    exit(signum);
}

void register_signal_handlers() {
    signal(SIGSEGV, signal_handler);
    signal(SIGABRT, signal_handler);
    signal(SIGILL,  signal_handler);
    signal(SIGFPE,  signal_handler);
}

int main(int argc, char ** args) {
    register_signal_handlers();
    HQC_Log("Starting ZumaHD...");
    return ApplicationZuma_Start();
}