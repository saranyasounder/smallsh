/*  Name: Saranya Sounder Rajan
    Assignment: small shell
    Date: 2/22/25
 */

#include <fcntl.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include "smallsh.h"

// prints out messages to know when we have entered/exited foreground mode
void stop()
{
    if (!fgMode)
    {
        printf("\nEntering foreground-only mode (& is now ignored)\n");
        fgMode = true;
    }
    else
    {
        printf("\nExiting foreground-only mode\n");
        fgMode = false;
    }
    fflush(stdout);
}

// function to terminate all background processes
void killbgProcess()
{
    int i;
    for (i = 0; i < numBg; i++)
    {
        kill(bgProcess[i], SIGTERM); // Send SIGTERM signal to terminate background process
    }
    for (i = 0; i < numBg; i++)
    {
        int status = 0;
        waitpid(bgProcess[i], &status, 0); // Wait for background processes to terminate
        if (WIFEXITED(status))
        {
            lastSignalCheck = false;
            endExitStatus = WEXITSTATUS(status); // Capture the exit status of terminated process
        }
        else if (WIFSIGNALED(status))
        {
            lastSignalCheck = true;
            endSigStatus = WTERMSIG(status); // Capture signal status of terminated process
        }
        printf("background pid %d is done\n", bgProcess[i]);
        fflush(stdout); // show the termination message
    }
    numBg = 0; // initialize count back to 0
}

// Function to find the full path of a command by searching in directories listed in PATH
char *findcmdPath(char *command)
{
    char *path = getenv("PATH"); // Get the system's PATH environment variable
    char *colon = ":";
    char *token = strtok(path, colon);           // tokenize the path by colon
    char *fullPath = malloc(sizeof(char) * 200); // allocating memory for the full path

    while (token != NULL)
    {
        snprintf(fullPath, 200, "%s/%s", token, command); // Construct the full command path

        if (access(fullPath, X_OK) != -1)
        {                    // check if file is executable
            return fullPath; // return the full path
        }
        token = strtok(NULL, colon); // else move to the next directory in path
    }
    fprintf(stderr, "Error: Command '%s' not found\n", command); // print error message
    fflush(stdout);
    free(fullPath);
    exit(EXIT_FAILURE); // Exit the program if command is not found
}

// function to handle SIGINT signal(^C) and terminate the foreground process
void killFgProcess(int signum)
{
    if (fgProcess != -1)
    {
        kill(fgProcess, SIGINT); // Send SIGNT to foreground process
    }
    printf("terminated by signal %d\n", signum);
    fflush(stdout);
    lastSignalCheck = true;
    endSigStatus = signum; // update the signal number
    fgProcess = -1;        // initialize the foreground pID
}

// check the status of all background processes
void checkBg()
{
    int numBgTerm = 0;
    int i;
    for (i = 0; i < numBg; i++)
    {
        int status;
        pid_t result = waitpid(bgProcess[i], &status, WNOHANG); // Non-blocking check for background process
        if (result == 0)
        {
            continue; // process is still running
        }
        else if (result == -1)
        {
            perror("waitpid"); // handle errors in waitpid
            bgProcess[i] = -1;
            numBgTerm++;
        }
        else
        {
            if (WIFEXITED(status))
            {
                lastSignalCheck = false;
                endExitStatus = WEXITSTATUS(status); // update exit status of process terminated normally
            }
            else if (WIFSIGNALED(status))
            {
                lastSignalCheck = true;
                endSigStatus = WTERMSIG(status); // update signal status if process terminated by signal
            }
            if (lastSignalCheck)
            {
                printf("background pid %d is done: terminated by signal %d\n", bgProcess[i], endSigStatus); // print terminal message
            }
            else
            {
                printf("Background pid %d is done: exit value %d\n", bgProcess[i], endExitStatus); // print exit value messsage
            }
            fflush(stdout);
            bgProcess[i] = -1;
            numBgTerm++;
        }
    }
    if (numBgTerm > 0)
    {
        int j;
        int i;
        for (i = 0; i < numBg; i++)
        {
            if (bgProcess[i] != -1)
            {
                bgProcess[j] = bgProcess[i]; // shift remaining background processes in the array
                j++;
            }
        }
        numBg = j; // update the background processes count
    }
}

// Function to check if the input starts with '#'
bool checkComment(char firstLetterOfUserInput)
{
    return firstLetterOfUserInput == '#'; // only return if it is a comment
}

// check if input is 'echo'
bool checkEcho(char *userInput)
{
    char *userInputCopy = strdup(userInput);
    char *token = strtok(userInputCopy, " ");
    bool returnValue = strcmp(token, "echo") == 0;
    free(userInputCopy);
    return returnValue;
}

// check if input is CD
bool checkCD(const char *userInput)
{
    if (userInput[0] == 'c' && userInput[1] == 'd')
    {
        return true;
    }
    else
    {
        return false;
    }
}

// check if input is exit
bool checkExit(char *userInput)
{
    char *userInputCopy = strdup(userInput);
    char *token = strtok(userInputCopy, " \t\n\0");
    bool returnValue = strcmp(token, "exit") == 0;
    free(userInputCopy);
    return returnValue;
}

// check if input is status
bool checkStatus(char *userInput)
{
    char *userInputCopy = strdup(userInput);
    char *token = strtok(userInputCopy, " \t\n\0");
    bool returnValue = strcmp(token, "status") == 0;
    free(userInputCopy);
    return returnValue;
}

// function to change directory
void switchDir(char *path)
{
    char *pathCopy = strdup(path);
    if (pathCopy != NULL)
    {
        if (chdir(pathCopy) != 0)
        {
            perror("chdir");
            free(pathCopy);
            fflush(stdout);
        }
        else
        {
            strcpy(cwDir, pathCopy);
            free(pathCopy);
        }
    }
}

// replace '$$' with the current pID number
void changeDollarSign(char *userInput)
{
    bool pidAdded = false;
    char *userInputCopy = strdup(userInput);
    char pidString[20];
    int pidValue = getpid();            // get the current pID
    sprintf(pidString, "%d", pidValue); // convert pIF to a string
    char *newString = malloc(sizeof(char) * (strlen(userInputCopy) + strlen(pidString) + 1));
    if (newString == NULL)
    {
        perror("malloc");
        free(userInputCopy);
        return;
    }
    int j = 0;
    int i;
    for (i = 0; userInputCopy[i] != '\0'; i++)
    {
        if (userInputCopy[i] == '$' && userInputCopy[i + 1] == '$')
        {
            pidAdded = true;
            strcat(newString, pidString); // Replace '$$' with process ID
            j += strlen(pidString);
            j++;
            if (userInputCopy[++i] == '\0')
            {
                break;
            }
        }
        else
        {
            newString[j++] = userInputCopy[i]; // copy remianing characters
        }
    }
    newString[j] = '\0';
    if (pidAdded)
    {
        strcpy(userInput, newString);
    }
    free(newString);
    free(userInputCopy);
}

// function to read and parse user input to the command struct
struct Command userCommands()
{
    if (numBg > 0)
    {
        checkBg(); // check for any terminated bg processes
    }
    struct Command cmd = {0}; // initialize a command structure
    char line[2049];          // line to hold input
    char *token;
    printf(": "); // ask user for input
    fflush(stdout);
    if (fgets(line, sizeof(line), stdin) == NULL)
    {
        return cmd; // if input is NULL, then return an empty struct
    }
    line[strcspn(line, "\n")] = '\0';
    if (line[0] == '\0')
    {
        return cmd;
    }
    changeDollarSign(line); // replace '$$' with pID
    cmd.name = malloc(sizeof(char) * 15);
    if (checkCD(line))
    {
        strcpy(cmd.name, "cd");              // check if command is cd
        token = strtok(line + 3, " \t\n\0"); // extract directory path if provided
        cmd.isCd = true;
        if (token != NULL)
        {
            cmd.cdPath = malloc(sizeof(char) * (strlen(token) + 1));
            strcpy(cmd.cdPath, token);
        }
    }

    else if (checkEcho(line))
    { // check if input is 'echo'
        cmd.isEcho = true;
        cmd.args = malloc(sizeof(char *) * (513)); // Allocate memory for arguments (maximum 512 arguments + 1 for NULL termination)
        cmd.name = malloc(sizeof(char) + (strlen("echo") + 1));
        cmd.echoFile = malloc(sizeof(char) * (strlen(line) - 4));
        strcpy(cmd.name, "echo");
        strcpy(cmd.echoFile, line + 5); // Copy everything after "echo " into echoFile

        // Allocate and store command arguments
        cmd.args[0] = malloc(sizeof(char) * (strlen(cmd.name) + 1));
        cmd.args[1] = malloc(sizeof(char) * (strlen(cmd.echoFile) + 1));
        strcpy(cmd.args[0], cmd.name);
        strcpy(cmd.args[1], cmd.echoFile);
        cmd.args[2] = NULL; // Null-terminate the argument list
    }
    else if (checkComment(line[0]))
    { // check if input is comment
        cmd.isComment = true;
        strcpy(cmd.name, "Comment");
    }
    else if (checkExit(line))
    { // check if input is exit
        strcpy(cmd.name, "exit");
    }
    else if (checkStatus(line))
    {
        token = strtok(line, " \t\n\0");
        strcpy(cmd.name, token);
    }
    else
    { // process any other commands
        token = strtok(line, " \t\n\0");
        // If no command is found, print an error message and exit
        if (token == NULL)
        {
            fprintf(stderr, "No command found\n");
            fflush(stdout);
            exit(EXIT_FAILURE);
        }
        strcpy(cmd.name, token);
        cmd.numArg = 0;
        cmd.args = malloc(sizeof(char *) * (513));
        while (true)
        {
            if (cmd.numArg == 0)
            {// First argument is the command itself
                cmd.numArg++;
                cmd.args[0] = malloc(sizeof(char) * (strlen(cmd.name) + 1));
                strcpy(cmd.args[0], cmd.name);
                cmd.args[1] = NULL;
                continue;
            }
            // Get the next token
            token = strtok(NULL, " \t\n\0");
            // If no more tokens, exit loop
            if (token == NULL)
            {
                break;
            }
            if (token[0] != '<' && token[0] != '>' && token[0] != '&')
            {
                cmd.numArg++;
                cmd.args[cmd.numArg - 1] = malloc(sizeof(char) * (strlen(token) + 1));
                strcpy(cmd.args[cmd.numArg - 1], token);
                cmd.args[cmd.numArg] = NULL;
            }
            // Handle input redirection ('<' followed by a file name)
            else if (token[0] == '<' && strlen(token) == 1)
            {
                token = strtok(NULL, " \t\n\0"); // Get the file name
                cmd.fileInput = malloc(sizeof(char) * (strlen(token) + 1));
                strcpy(cmd.fileInput, token);
            }
            // Handle output redirection ('>' followed by a file name)
            else if (token[0] == '>' && strlen(token) == 1)
            {
                token = strtok(NULL, " \t\n\0");
                cmd.fileOutput = malloc(sizeof(char) * (strlen(token) + 1));
                strcpy(cmd.fileOutput, token);
            }
            // Handle background execution ('&' at the end of the command)
            else if (token[0] == '&')
            {
                token = strtok(NULL, " \t\n\0");
                if (token == NULL)
                    cmd.useBg = true; //indicate that it is a background process
            }
            else
            {
                break;
            }
        }
    }
    return cmd;
}

/**
 * Function to redirect standard output to /dev/null
 * useful for suppressing output in background processes.
 * used it to handle fileOutput too
 */

void handleDevNull(int devNull)
{
    // Check if opening /dev/null failed
    if (devNull == -1)
    {
        perror("open");
        exit(EXIT_FAILURE);
    }
    // Redirect STDOUT to /dev/null
    if (dup2(devNull, STDOUT_FILENO) == -1)
    {
        perror("dup2");
        exit(EXIT_FAILURE);
    }
    // Close the file
    if (close(devNull) == -1)
    {
        perror("close");
        exit(EXIT_FAILURE);
    }
    return;
}

// function to execute commands
void implementInput(struct Command cmd)
{
    if (cmd.isCd)
    { // change directory command
        if (cmd.cdPath == NULL)
        {
            switchDir(home); //switch to home directory if no path is provided
        }
        else
        {
            switchDir(cmd.cdPath); //else switch to named directory
        }
    }
    else if (strcmp(cmd.name, "exit") == 0)
    { // exit command
        if (numBg > 0)
        {
            killbgProcess(); // terminate all background processes
        }
        free(cwDir);
        exit(EXIT_SUCCESS);
    }
    else if (checkStatus(cmd.name))
    { // status command
        if (lastSignalCheck)
        {
            printf("terminated by signal %d\n", endSigStatus);
            fflush(stdout);
        }
        else
        {
            printf("exit value %d\n", endExitStatus);
            fflush(stdout);
        }
    }
    else if (!cmd.isComment)
    { // other commands
        lastSignalCheck = false;
        if (strcmp(cmd.name, "pkill") == 0)
        {
            lastSignalCheck = true; //inidcate that pkill invloves signals
        } // if fgMode is enabled, override background execution
        if (fgMode == true)
        {
            cmd.useBg = false;
        }
        // fork a child process to execute the command
        pid_t pid = fork();
        if (pid == -1) // incase fork fails
        {
            perror("fork");
            return;
        }
        else if (pid == 0)
        {   // child process executes command
            // fine the absolute path of command
            char *filePathToCommand = findcmdPath(cmd.name);
            cmd.commandPath = filePathToCommand;
            if (cmd.commandPath == NULL) // in case no valid command path is found
            {
                printf("Error: Command file path is NULL\n");
                fflush(stdout);
                exit(EXIT_FAILURE);
            }
            // If input redirection is specified, open the input file
            if (cmd.fileInput != NULL)
            {
                int fileInput = open(cmd.fileInput, O_RDONLY);
                if (fileInput == -1) //handle input redirection
                {
                    perror("open");
                    exit(EXIT_FAILURE);
                }
                if (dup2(fileInput, STDIN_FILENO) == -1)
                {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
                if (close(fileInput) == -1)
                {
                    perror("close");
                    exit(EXIT_FAILURE);
                }
            }
            // If output redirection is specified, open the output file
            if (cmd.fileOutput != NULL)
            {
                int fileOutput = open(cmd.fileOutput, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
                handleDevNull(fileOutput);
            }
            // if command is running in the background and no redirection is specified
            if (cmd.useBg)
            {
                if (cmd.fileInput == NULL) // if no inputFile is specified
                {
                    int devNull = open("/dev/null", O_RDONLY);
                    handleDevNull(devNull);
                }
                if (cmd.fileOutput == NULL) // if no output file is specfied
                {
                    int devNull = open("/dev/null", O_WRONLY);
                    handleDevNull(devNull);
                }
            }
            execv(cmd.commandPath, cmd.args); // execute the command
            perror("execv");
            fflush(stdout);
            exit(EXIT_FAILURE);
        }
        else
        { // parent process handles foreground/background processes
            int status;
            if (!cmd.useBg)
            {
                fgProcess = pid; // store the foreground pID
                if (waitpid(fgProcess, &status, 0) == -1)
                {
                    perror("waitpid");
                }
                // if process exited normally
                if (WIFEXITED(status))
                {
                    lastSignalCheck = false;           // no signal was involoved
                    endExitStatus = WIFEXITED(status); // update exit status
                }
                else if (WIFSIGNALED(status))
                {
                    lastSignalCheck = true;             // terminated by signal
                    endSigStatus = WIFSIGNALED(status); // store signal status
                }
            }
            else // background process handling
            {
                bgProcess[numBg] = pid; // store background pID in an array and increment the count
                numBg++;
                printf("background pid is %d\n", pid); // printing the bg pID
                fflush(stdout);
            }
        }
    }
}

int main()
{
    // get teh user's home directory
    home = getenv("HOME");
    cwDir = malloc(sizeof(char) * (1025));
    getcwd(cwDir, sizeof(cwDir)); // get the current working directory

    signal(SIGINT, killFgProcess); // setting up signal handlers
    signal(SIGTSTP, stop);
    // main loop for continuously processing user inputs
    while (true)
    {
        struct Command cmd = userCommands();
        if (cmd.name != NULL && !cmd.isComment)
        {
            implementInput(cmd);
        }
    }
}

