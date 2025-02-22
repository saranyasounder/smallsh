#include <fcntl.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#define MAX_CHAR_LENGTH 2049
#define MAX_PATH_LENGTH 1024
#define MAX_NUM_ARGS 512
#define NOT_ALPHA " \t\n\0"
#define EXIT_NAME "exit"
#define COMMENT_NAME "Comment"

pid_t backgroundProcesses[1000]; 
pid_t foregroundProcess = -1; 
int numberOfBackgroundProcesses = 0; 
int lastExitStatus = 0; 
int lastSignalStatus = 0; 
char* home; 
char* currentWorkingDirectory; 
bool lastStatusWasSignal = false; 
bool foregroundModeOnly = false; 


struct Command {
    char* name; 
    char** args; 
    char* inputFile; 
    char* outputFile; // this holds output file if user desires to change the output
    char* cdFilePath; // if the user has a path along with their cd command, this variable holds that path
    char* commandFilePath; // this variable holds the system file path of the command
    char* echoContents; // this variable holds all the contents if the cmd is an echo; these will be outputted back to the user without 'echo' which will be stored in name in that case
    int numberOfArgs; // tracking the number of args
    bool runInBackground; // this boolean tracks if the command is to be run in the background or not
    bool isCd; // if we encounter a cd command, we are storing a boolean to say so
    bool isComment; // if we encounter a comment, we are marking that because it's a special case (we are to ignore it)
    bool isEcho;
}; // end of "Command" struct


void freeStructMemory(struct Command cmd) { // freeing all the memory associated with the struct if it was allocated
    if (cmd.name != NULL) { // if name has memory allocated to it
        free(cmd.name); // free the memory
    }
    if (cmd.args != NULL) { // if args have mem allocated to it
    int i;
        for ( i = 0; cmd.args[i] != NULL; i++) { // loop through each arg until there is no mem allocated to that location in the array
            free(cmd.args[i]); // if there is allocated mem at that location in the array, free it
        } // end of for loop
        free(cmd.args); // free the mem alloacted to args
    }
    if (cmd.inputFile != NULL) { // if input file has mem allocated to it
        free(cmd.inputFile); // free the input file
    }
    if (cmd.outputFile != NULL) { // if output file has mem allocated to it
        free(cmd.outputFile); // free the output file
    }
    if (cmd.cdFilePath != NULL) { // if cdFilePath has mem allocated to it
        free(cmd.cdFilePath); // free the cdFilePath
    }
    if (cmd.commandFilePath != NULL) { // if it is null, then memory was never allocated to it.
        free(cmd.commandFilePath); // free the commandFilePath
    }
} // end of "freeStructMemory" function


char* getCommandFilePath(char* command) {
    char* path = getenv("PATH"); // getting the path
    if (path == NULL) { // handling an error case before using path
        fprintf(stderr, "Path variable not available\n");
        fflush(stdout);
        return NULL;
    }
    char* delim = ":"; // separating each segment of the path by its delimiter (this is how it's stored in the system with ':' as the delim)
    char* token = strtok(path, delim); // getting the first output of path
    char* fullPath = malloc(sizeof (char) * 200); // allocating memory to the file path

    while (token != NULL) { // continue the loop until the token == NULL
        snprintf(fullPath, 200, "%s/%s", token, command); // constructing the full path to the command

        if (access(fullPath, X_OK) != -1) { // if we can access that file, it is valid
            return fullPath; // returning the constructed file path
        }
        token = strtok(NULL, delim); // iterating the token
    }
    fprintf(stderr, "Error: Command '%s' not found\n", command); // if file path is not available, output error message
    fflush(stdout);
    free(fullPath); // free allocated memory
    exit(EXIT_FAILURE); // send exit status of 1
} // end of "getCommandFilePath" function


void killBackgroundProcesses() {
    int i;
    for (i = 0; i < numberOfBackgroundProcesses; i++) { // looping through current backgroundProcesses to kill them
        kill(backgroundProcesses[i], SIGTERM); // terminating all background processes and storing the result
    }
    for (i = 0; i < numberOfBackgroundProcesses; i++) { // waiting for all processes to close.
        int status = 0; // intializing the status variable which we will use to check the exit/signal values
        waitpid(backgroundProcesses[i], &status, 0); // wait for the process to close
        if (WIFEXITED(status)) { // if the return statement was an exit value and not a singal
            lastStatusWasSignal = false; // set that the last value was not a signal
            lastExitStatus = WEXITSTATUS(status); // set the last Exit value so we can print it to the user
        }
        else if (WIFSIGNALED(status)) { // if the return statement was a signal
            lastStatusWasSignal = true; // indicate that the last return was a signal
            lastSignalStatus =  WTERMSIG(status); // set the signal value so we can output that to the user
        }
        printf("Background process with PID %d has exited\n", backgroundProcesses[i]); // inform the user that the backgroundProcess was closed
        fflush(stdout);
    }
    numberOfBackgroundProcesses = 0; // reset the numberOfBackgroundProcesses to 0
} // end of "killBackgroundProcesses" function


void killForegroundProcess(int signum) { // function for killing the foreground process
    if (foregroundProcess != -1) { // ensuring that the foreground process is running
        kill(foregroundProcess, SIGINT); // killing the foreground process with sigint
    }
    printf("terminated by signal %d\n", signum); // outputting to the user.
    fflush(stdout); // flushing to stdout to ensure that the user is made away of the termination
    lastStatusWasSignal = true; // because we are sending a signal to the user, we set the lastStatusWasSignal to true for printing
    lastSignalStatus = signum; // this is the signal value that we will output to the user
    foregroundProcess = -1; // setting the foreground process to -1 to indicate that there isn't one any longer (becuase we killed it)
} // end of "killForegroundProcess" function


void signalStop() {
    if (!foregroundModeOnly) { // if foreground mode is true
        printf("\nEntering foreground-only mode (& is now ignored)\n"); // print to the user
        foregroundModeOnly = true; // set the foreground mode to false
    }
    else { // if foreground mode is false
        printf("\nExiting foreground-only mode\n"); // print to the user
        foregroundModeOnly = false; // set foreground mode to true
    }
    fflush(stdout); // flushing the standard out to ensure print out occurs
} // end of "signalStop" function


void checkOnBackgroundProcesses() {
    int numOfBackgroundProcessesTerminated = 0; // starting this function by initializing a tracker value that tracks the amount of background processes that are terminated in this function
    int i;
    for (i = 0; i < numberOfBackgroundProcesses; i++) { // looping through the backgroundProcesses
        int status; // intializing a status. This will help us determine the output of the pid, whether it was a signal or an exit and what that value is
        pid_t result = waitpid(backgroundProcesses[i], &status, WNOHANG); // getting the result of the child process
        if (result == 0) { // the process is still running and not complete
            continue; // because the process is not complete, just continue through the loop and don't consider this one any longer
        }
        else if (result == -1) { // there was an error. We thus output the error to the user and seek to remove the pid from the backgroundProcesses that we are tracking
            perror("waitpid"); // output the error to the user
            backgroundProcesses[i] = -1; // doing this so that it can be removed from the array
            numOfBackgroundProcessesTerminated += 1; // if this value is greater than 0, we must reorder the array.
        }
        else {
            if (WIFEXITED(status)) { // checking if the background process was an exit and not a signal
                lastStatusWasSignal = false; // because it wasn't a signal, set this variable to false so that exit is outputted to the user and not signal
                lastExitStatus = WEXITSTATUS(status); // setting the last exit value
            }
            else if (WIFSIGNALED(status)) { // checking if the background process was a signal and not a exit
                lastStatusWasSignal = true; // because it was a signal, set this variable to true so that signal is outputted to the user and not exit
                lastSignalStatus = WTERMSIG(status); // setting the last signal value
            }
            if (lastStatusWasSignal) { // if the closing was a signal
                printf("Background pid %d is done: terminated by signal %d\n", backgroundProcesses[i], lastSignalStatus); // output the signal value to the user
            }
            else { // if the closing was an exit and not a signal
                printf("Background pid %d is done: exit value %d\n", backgroundProcesses[i], lastExitStatus); // output the exit value to the user
            }
            fflush(stdout); // flushing the stdout to ensure user is made aware of what background process closed
            backgroundProcesses[i] = -1; // doing this so that it can be removed from the array in the second loop of this function
            numOfBackgroundProcessesTerminated += 1; // if this value is greater than 0, we must reorder the array so we track the number of terminated values we have
        }
    } // end of for loop
    if (numOfBackgroundProcessesTerminated > 0) { // if numOfBackgroundProcessesTerminated above is greater than 0, it means we must reorder our backgroundProcesses array because some need to be removed
        int j = 0; // initalizing an index j
        int i;
        for (i = 0; i < numberOfBackgroundProcesses; i++) { // running a loop that reorders the array removing pid's that have been marked for termination.
            if (backgroundProcesses[i] != -1) { // if the pid was not terminated, move it up in the array
                backgroundProcesses[j] = backgroundProcesses[i]; // if the backgroundProcesses was not terminated, tack it onto the jth spot in the array; when backgroundProcesses[i] was terminated, i iterates but j does not, and backgroundProcesses[i] ends up being overridden with a value one
                j++;
            }
        } // end of for loop
        numberOfBackgroundProcesses = j; // updating the amount of background processes that are active in the array.
    }
} // end of "checkOnBackgroundProcesses" function


bool checkForComment(char firstLetterOfUserInput) {
    return firstLetterOfUserInput == '#'; // this function receives a letter and checks if that letter is '#'
} // end of "checkForComment" function


bool checkForEcho(char* userInput) {
    char* userInputCopy = strdup(userInput); // creating a copy of userInput as to not edit the userInput itself
    char* token = strtok(userInputCopy, " "); // getting the first word (first set of chars without a space)
    bool returnValue = strcmp(token, "echo") == 0; // if the first word is echo, setting the return value to true; if not, setting it to false.
    free(userInputCopy); // freeing the duplicated mem
    return returnValue;
} // end of "checkForEcho" function


bool checkForCD(const char* userInput) { // this function reads the command and checks if it is 'cd'
    if (userInput[0] == 'c' && userInput[1] == 'd') { // if the first two letters of the user input is cd then this is a cd command. We also check to ensure that the userInput length is 2 or (if it's not length of 2) that the third character is a space, indicating that the userInput is 'cd xxxx'
        return true; // this is reached if the userInput is a cd command, so return true
    }
    else {
        return false; // return false because the first two letters are not a cd command
    }
} // end of "checkForCD" function


bool checkForExit(char* userInput) { // this function reads the command and checks if it is 'exit'
    char* userInputCopy = strdup(userInput); // creating a copy to avoid editing the original value
    char* token = strtok(userInputCopy, NOT_ALPHA); // getting the first word in the userInput
    bool returnValue = strcmp(token, "exit") == 0;  // setting the return value (boolean) to true if token == exit and false otherwise
    free(userInputCopy); // freeing the duplicated memory
    return returnValue; // returning the return value; again, if the token == exit, the first word of the userInput is exit, thus, this is a exit command
} // end of "checkForExit" function


bool checkForStatus(char* userInput) { // this function reads the command and checks if it is 'status'
    char* userInputCopy = strdup(userInput); // creating a copy to avoid editing the original value
    char* token = strtok(userInputCopy, NOT_ALPHA); // getting the first word in the userInput
    bool returnValue = strcmp(token, "status") == 0; // setting the return value (boolean) to true if token == status and false otherwise
    free(userInputCopy); // freeing the duplicated memory
    return returnValue; // returning the return value; again, if the token == status, the first word of the userInput is status, thus, this is a status command
} // end of "checkForStatus" function


void changeDirectory(char* path) {
    char* pathCopy = strdup(path); // creating a copy to avoid editing the original value
    if (pathCopy != NULL) { // error checking to ensure that PathCopy is not null before use. if it is NULL, the changeDirectory function does nothing. The path should never be null however because if a user doens't enter the directory, this function will recieve "home" which is the home directory
        if (chdir(pathCopy) != 0) { // error handling when changing the directory. 0 means the directory was validly changed
            perror("chdir"); // outputting the error
            free(pathCopy); // freeing the duplicated memory
            fflush(stdout); // flushing the stdout so that the user receives the error
        } else {
            strcpy(currentWorkingDirectory, pathCopy); // setting the currentWorkingDirectory variable to the path because the current home directory was changed.
            free(pathCopy); // freeing the duplicated memory
        }
    }
} // end of "changeDirectory" function


void replace$$WithPid(char* userInput) {
    bool pidAdded = false;
    char* userInputCopy = strdup(userInput); // creating a copy as to not mess with userInput before desired
    char pidString[20]; // creating a string to store the pid in
    int pidValue = getpid(); // gettign the pid
    sprintf(pidString, "%d", pidValue); // converting pidValue to a string
    char* newString = malloc(sizeof (char) * (strlen(userInputCopy) + strlen(pidString) + 1)); // allocating mem to newString
    if (newString == NULL) { // handlding the error in case malloc of newString doesn't work
        perror("malloc");
        free(userInputCopy); // freeing the mem to ensure no seg faults.
        return;
    }
    int j = 0; // index for new string
    int i;
    for (i = 0; userInputCopy[i] != '\0'; i++) {
        if (userInputCopy[i] == '$' && userInputCopy[i+1] == '$') {
            pidAdded = true;
            strcat(newString, pidString); // adding pidString to newString
            j += strlen(pidString); // updating the index for new string
            j += 1; // skipping the second '$'
            if (userInputCopy[++i] == '\0') { // iterating i to skip the second $ but if it is the null terminator, break the loop.
                break;
            }
        }
        else {
            newString[j++] = userInputCopy[i]; // copying char from userInput copy
        }
    } // end of for loop
    newString[j] = '\0'; // add null term to end of new string
    if (pidAdded) { // only copy the newString into the userInput if pid is added
        strcpy(userInput, newString); // copying the userInput as the new string. This will directly change the buffer in the parent function to reflect the updated userInput
    }
    free(newString); // freeing mem to ensure no seg faults.
    free(userInputCopy); // freeing mem to ensure no seg faults.
} // end of "replace$$WithPid" function


struct Command getUserInput() {
    if (numberOfBackgroundProcesses > 0) { // if the number of background processes is greater than 0, we seek to check if any of them have finished before allowing the user to do anything.
        checkOnBackgroundProcesses(); // checking if there were any processes that finished since the last input. This function will output the processes that finished to the user
    }
    struct Command cmd = {0}; // initializing the struct to 0/NULL for all variables. This will be our return variable
    char buffer[MAX_CHAR_LENGTH]; // creating the buffer for the user input
    char* token; // creating the token
    printf(": "); // outputting to the user
    fflush(stdout); // flushing to ensure that the output is recieved by the user
    if (fgets(buffer, sizeof(buffer), stdin) == NULL) { // if there was an error with fgets, just return a new output to the user
        return cmd;
    }
    buffer[strcspn(buffer, "\n")] = '\0'; // scan buffer until it finds the newline char and replace it with null terminator.
    if (buffer[0] == '\0') { // if the user input nothing, return cmd empty and give the user a new output
        return cmd;
    }
    replace$$WithPid(buffer); // calling replace$$WithPid function on the buffer to replace all double dollar signs with the pid value
    cmd.name = malloc(sizeof(char) * 15); // allocating 15 characters to the name.
    if (checkForCD(buffer)) { // checking if the cd command was in the buffer
        strcpy(cmd.name, "cd"); // copying the command into the name
        token = strtok(buffer + 3, NOT_ALPHA); // getting the file path.
        cmd.isCd = true; // setting the isCd boolean to true
        if (token != NULL) { // if token != NULL, it means that an argument was supplied to the cd command.
            cmd.cdFilePath = malloc(sizeof(char) * (strlen(token) + 1)); // allocating mem to the cdFilePath (the cd arg)
            strcpy(cmd.cdFilePath, token); // copying the filePath into the cdFilePath
        }
    }
    else if (checkForComment(buffer[0])) { // checking if the user inputted a comment
        cmd.isComment = true; // if the first char is '#' then this is a comment. If this value is true, the cmd will not be passed to the handler (basically, the program does nothing)
        strcpy(cmd.name, COMMENT_NAME); // setting the name of the command struct to "Comment".
    }
    else if (checkForExit(buffer)) {
        strcpy(cmd.name, EXIT_NAME);
    }
    else if (checkForStatus(buffer)) {
        token = strtok(buffer, NOT_ALPHA);
        strcpy(cmd.name, token);
    }
    else if (checkForEcho(buffer)) { // handling the case of an echo. This is a special case
        cmd.isEcho = true;
        cmd.args = malloc(sizeof(char*) * (MAX_NUM_ARGS + 1)); // allocating the proper memory amount to args.
        cmd.name = malloc(sizeof(char) + (strlen("echo") + 1)); // allocating space to the name
        cmd.echoContents = malloc(sizeof(char) * (strlen(buffer) - 4)); // allocating mem to the echoContents
        strcpy(cmd.name, "echo"); // copying echo into the name
        strcpy(cmd.echoContents, buffer + 5); // copying the buffer contents, excluding echo
        cmd.args[0] = malloc(sizeof(char) * (strlen(cmd.name) + 1)); // allocating mem to location 0 in args
        cmd.args[1] = malloc(sizeof(char) * (strlen(cmd.echoContents) + 1)); // allocating mem to args to have the echo
        strcpy(cmd.args[0], cmd.name); // copying the name into arg[0]
        strcpy(cmd.args[1], cmd.echoContents); // putting the echo contents in the args
        cmd.args[2] = NULL; // tacking NULL onto the end of the args array
    }
    else { // commands other than standard (exit, cd, & status) and comment.
        token = strtok(buffer, NOT_ALPHA); // getting the command in the buffer
        if (token == NULL) { // some general error handling on token before using. Empty buffers are handled elsewhere but thought it wise to check that the token was not null before using
            fprintf(stderr, "No command found\n");
            fflush(stdout);
            exit(EXIT_FAILURE); // exiting the program; again, empty buffer is handled elsewhere so if the token makes it to this location as NULL, there is a strange error
        }
        strcpy(cmd.name, token); // getting the command
        cmd.numberOfArgs = 0; // setting the number of commands to 0 for use in loop.
        cmd.args = malloc(sizeof(char*) * (MAX_NUM_ARGS + 1)); // allocating the proper memory amount to args.
        while (true) {
            if (cmd.numberOfArgs == 0) { // if this is the first arg, we need to add the command itself to the args
                cmd.numberOfArgs += 1; // increase the number of args
                cmd.args[0] = malloc(sizeof(char) * (strlen(cmd.name) + 1)); // allocate space for the command itself
                strcpy(cmd.args[0], cmd.name); // copy the command to the args[0] location
                cmd.args[1] = NULL; // tack on NULL at the end of the args
                continue; // iterate the loop
            }
            token = strtok(NULL, NOT_ALPHA);
            if (token == NULL) {
                break; // no more arguments
            }
            if (token[0] != '<' && token[0] != '>' && token[0] != '&') { // if we encounter something that does not indicate a output file, input file, or background running, treat it like a normal arg.
                cmd.numberOfArgs += 1; // increase the number of args
                cmd.args[cmd.numberOfArgs - 1] = malloc(sizeof(char) * (strlen(token) + 1)); // allocate space for the arg
                strcpy(cmd.args[cmd.numberOfArgs - 1], token); // copy the arg into the args array
                cmd.args[cmd.numberOfArgs] = NULL; // tack on a null to the end of the args array
            }
            else if (token[0] == '<' && strlen(token) == 1) { // if we encounter a < that is surrounded by spaces, it means that we have an input file. the token in this case would be '<'
                token = strtok(NULL, NOT_ALPHA); // iterate the token to get the input file
                cmd.inputFile = malloc(sizeof (char) * (strlen(token) + 1)); // allocate memory to input file
                strcpy(cmd.inputFile, token); // copy the token into the cmd's input file location
            }
            else if (token[0] == '>' && strlen(token) == 1) { // if we encounter a > that is surrounded by spaces, it means that we have an output file. the token in this case would be '>'
                token = strtok(NULL, NOT_ALPHA); // iterate the token to get the output file
                cmd.outputFile = malloc(sizeof (char) * (strlen(token) + 1)); // allocate memory to the output file
                strcpy(cmd.outputFile, token); // copy the token into the cmd's output file location
            }
            else if (token[0] == '&') { // if the only thing in the token is '&' it means that we are going to run this command in the background
                token = strtok(NULL, NOT_ALPHA); // iterate the token to ensure that we are at the end of the buffer
                if (token == NULL) // ensure that we are at the end of the buffer
                    cmd.runInBackground = true; // set the runInBackground to true
            }
            else {
                break;
            }
        } // end of while loop
    }
    return cmd;
} // end of "getUserInput" function


void handleUserInput(struct Command cmd) { // once the cmd is populated correctly, handle the cmd
    if (cmd.isCd) { // if the cd command is called, change the directory
        if (cmd.cdFilePath == NULL) { // handling the case where the user didn't pass a filepath with cd command.
            changeDirectory(home); // calling change directory with home directory
        }
        else {
            changeDirectory(cmd.cdFilePath); // passing in the specified directory
        }
    }
    else if (strcmp(cmd.name, EXIT_NAME) == 0) { // if exit is called
        if (numberOfBackgroundProcesses > 0) { // if there are more background processes than 0
            killBackgroundProcesses(); // kill backgrond proccesses
        }
        free(currentWorkingDirectory); // free the currenting working directory memory
        exit(EXIT_SUCCESS); // exiting the program
    }
    else if (checkForStatus(cmd.name)) { // call checkForStatus function to see if the status command was called
        if (lastStatusWasSignal) { // if the last exit was a signal
            printf("terminated by signal %d\n", lastSignalStatus); // output the last signal status
            fflush(stdout);
        }
        else { // if the last status was not a signal
            printf("exit value %d\n", lastExitStatus); // output the last error status
            fflush(stdout);
        }
    }
    else if(!cmd.isComment) { // handle all other scenarios that are not comments.
        lastStatusWasSignal = false; // always resetting the lastStatusWasSignal variable
        if (strcmp(cmd.name, "pkill") == 0) { // setting the last status signal to true if the user tries to kill the program.
            lastStatusWasSignal = true;
        }
        if (foregroundModeOnly == true) { // resetting runInBackground to false for this command.
            cmd.runInBackground = false; // reset run in background mode to false
        }
        pid_t pid = fork(); // forking
        if (pid == -1) { // checking if the fork failed before using
            perror("fork"); // output error to the user and return
            return;
        }
        else if (pid == 0) { // child proccess
            char* filePathToCommand = getCommandFilePath(cmd.name); // calling getCommandFilePath to get the file path for the nonstandard command
            cmd.commandFilePath = filePathToCommand; // putting the file path to command into the command file path of the cmd
            if (cmd.commandFilePath == NULL) { // if the file path is null, then the command does not exist
                printf("Error: Command file path is NULL\n");
                fflush(stdout);
                exit(EXIT_FAILURE);
            }

            if (cmd.inputFile != NULL) { // redirecting the input to the specified file.
                int inputFile = open(cmd.inputFile, O_RDONLY); // opening file as read only
                if (inputFile == -1) { // error handling for input file
                    perror("open"); // outputting error message if open had an error
                    exit(EXIT_FAILURE);
                }
                if (dup2(inputFile, STDIN_FILENO) == -1) { // redirecting input as the file and error handling
                    perror("dup2"); // outputting error message if dup2 had an error
                    exit(EXIT_FAILURE);
                }
                if (close(inputFile) == -1) { // error handling close
                    perror("close"); // outputting error message if close had an error
                    exit(EXIT_FAILURE);
                }
            }

            if (cmd.outputFile != NULL) { // redirecting the output to the specified file
                int outputFile = open(cmd.outputFile, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR); // creating file if it doesn't exist and truncating if it does. also assigning read and write permissions to owner.
                if (outputFile == -1) { // checking that the open is successful before using it. Exiting it not.
                    perror("open"); // outputting error message if open had an error
                    exit(EXIT_FAILURE);
                }
                if (dup2(outputFile, STDOUT_FILENO) == -1) { // calling dup2 and error handling
                    perror("dup2"); // outputting error message if dup2 had an error
                    exit(EXIT_FAILURE);
                }
                if (close(outputFile) == -1) { // calling close and checking for error value
                    perror("close"); // outputting error message if close had an error
                    exit(EXIT_FAILURE);
                }
            }

            if (cmd.runInBackground) { // if the command was intended to run in the background
                if (cmd.inputFile == NULL) { // if the input file is null, use /dev/null for the input file
                    int devNull = open("/dev/null", O_RDONLY);
                    if (devNull == -1) { // throwing an error if unable to open the file.
                        perror("open"); // outputting error message if open had an error
                        exit(EXIT_FAILURE);
                    }
                    if (dup2(devNull, STDIN_FILENO) == -1) { // redirecting the standard out to /dev/null and checking that there is no erro thrown
                        perror("dup2"); // outputting error message if dup2 had an error
                        exit(EXIT_FAILURE);
                    }
                    if (close(devNull) == -1) { // closing the devNull and handling the error if there is one.
                        perror("close"); // outputting error message if close had an error
                        exit(EXIT_FAILURE);
                    }
                }
                if (cmd.outputFile == NULL) { // if the output file is null, use /dev/null for the output file
                    int devNull = open("/dev/null", O_WRONLY); // opening /dev/null in read only
                    if (devNull == -1) { // checking that open worked properly
                        perror("open"); // outputting error message if open had an error
                        exit(EXIT_FAILURE);
                    }
                    if (dup2(devNull, STDOUT_FILENO) == -1) { // redirecting the standard out to the /dev/null file and handling the error if there is one.
                        perror("dup2"); // outputting error message if dup2 had an error
                        exit(EXIT_FAILURE);
                    }
                    if (close(devNull) == -1) { // closing the devNull and handling the error if there is one.
                        perror("close"); // outputting error message if close had an error
                        exit(EXIT_FAILURE);
                    }
                }
            }
            execv(cmd.commandFilePath, cmd.args); // calling execv to run non standard command
            perror("execv"); // outputting errors if the function returns.
            fflush(stdout);
            exit(EXIT_FAILURE); // sending an error back
        }
        else { // parent process
            int status;
            if (!cmd.runInBackground) {
                foregroundProcess = pid;
                if (waitpid(foregroundProcess, &status, 0) == -1) { // outputting the error of waitpid before using it
                    perror("waitpid");
                } // wait for the child process to finish
                if (WIFEXITED(status)) { // if the return is an exit value
                    lastStatusWasSignal = false; // set the lastStatusWasSignal value to false because it wasn't a signal
                    lastExitStatus = WIFEXITED(status); // set the lastExitStatues to the child return exit value
                }
                else if (WIFSIGNALED(status)) { // if the return is a signal
                    lastStatusWasSignal = true; // set the lastStatusWasSignal value to true
                    lastSignalStatus = WIFSIGNALED(status); // set the lastSignalStatus to the child return sig value
                }
            }
            else {
                backgroundProcesses[numberOfBackgroundProcesses] = pid; // putting the pid in teh array
                numberOfBackgroundProcesses += 1; // increasing the number of background processes becasue pid was added
                printf("background pid is %d\n", pid); // outputting to the user
                fflush(stdout); // flushing the stdout
            }
        }
    }
} // end of "handleUserInput" function


int main() {
    home = getenv("HOME"); // setting the home variable for use
    currentWorkingDirectory = malloc(sizeof(char) * (MAX_PATH_LENGTH + 1)); // allocating memory for current working directory.
    getcwd(currentWorkingDirectory, sizeof(currentWorkingDirectory)); // setting the working directory to the initial directory tha the file is stored in.

    signal(SIGINT, killForegroundProcess); // Set up SIGINT signal handler
    signal(SIGTSTP, signalStop); // set up SIGTSTP signal handler

    while (true) {
        struct Command cmd = getUserInput();
        if (cmd.name != NULL && !cmd.isComment) { // only handle the command if it's not a comment and the cmd was not null indicating that nothing was entered by the user. If it is a comment, ignore it.
            handleUserInput(cmd); // passing the cmd, once it's been created, into the handler function
        }
        freeStructMemory(cmd); // freeing memory of cmd contents
    } // end of while loop
} 