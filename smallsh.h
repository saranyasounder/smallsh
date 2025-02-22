#include <fcntl.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

pid_t bgProcess[1000];        // stores background pIDs
pid_t fgProcess = -1;         // stores foreground pIDs if there are any
int numBg = 0;                // initialized count for background processes
int endExitStatus = 0;        // exit status of last process
int endSigStatus = 0;         // singal number that terminated the last process
char *home;                   // home directory oath
char *cwDir;                  // current working directory
bool lastSignalCheck = false; // boolean to check if last process was terminated by a signal
bool fgMode = false;          // boolean to indiacte if shell is in foreground mode only.

// Struct to represent a command and its attributes
struct Command
{
    char *name;
    char **args;
    char *fileInput;
    char *fileOutput;
    char *cdPath;
    char *commandPath;
    char *echoFile;
    int numArg;
    // bools used as flags
    bool useBg;
    bool isCd;
    bool isComment;
    bool isEcho;
};