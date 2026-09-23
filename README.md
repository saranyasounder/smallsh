# smallsh

A custom Unix shell written in C, implementing a subset of the functionality of `bash`. Built as part of CS 374 (Operating Systems I) at Oregon State University.

## Overview

`smallsh` provides an interactive command-line shell that:

- Prints a prompt and reads a command line from the user
- Parses commands with arguments, supports comments and blank lines
- Executes both built-in commands and external programs via `fork()`/`exec()`
- Supports running processes in the foreground and background
- Provides standard I/O redirection (`<`, `>`)
- Handles signals (`SIGINT`, `SIGTSTP`) with custom shell behavior
- Performs variable expansion of `$$` into the shell's process ID

## Built-in Commands

| Command | Description |
|---|---|
| `exit` | Exits the shell, terminating any other processes/jobs it has started |
| `cd [path]` | Changes the working directory; with no argument, changes to the `HOME` directory |
| `status` | Prints the exit status or terminating signal of the last foreground process |

Any other command is executed as an external program by forking a child process and using a member of the `exec()` family of functions.

## Command Syntax

```
command [arg1 arg2 ...] [< input_file] [> output_file] [&]
```

- Up to 512 arguments, each up to 2048 characters
- `<` redirects standard input from a file
- `>` redirects standard output to a file
- A trailing `&` runs the command in the **background** (ignored in foreground-only mode)
- Any instance of `$$` in a command line is expanded to the shell's own PID
- Blank lines and lines beginning with `#` are ignored (treated as comments)

## Signal Handling

- **`SIGINT` (Ctrl+C):** Ignored by the shell and background processes; terminates only the foreground child process, if one is running.
- **`SIGTSTP` (Ctrl+Z):** Toggles "foreground-only mode." When enabled, the shell ignores the `&` operator, forcing all commands to run in the foreground.

## Background Processes

- Commands ending in `&` run in the background (unless foreground-only mode is active).
- The shell immediately prints the background process's PID and continues accepting input without waiting.
- Before printing each new prompt, the shell checks for and reports any completed background processes, including their PID and exit status/terminating signal.

## Building

```bash
gcc -std=gnu99 -o smallsh smallsh.c
```

Or, if a Makefile is included:

```bash
make
```

## Running

```bash
./smallsh
```

## Example Session

```
: ls -l
total 8
-rwxr-xr-x 1 user user 4096 Sep 23 10:00 smallsh
: status
exit value 0
: sleep 30 &
Background pid is 12345
: ls
smallsh
: pid $$ is running
pid 4567 is running
: exit
```

## Project Structure

```
.
├── smallsh.c     # Main shell implementation
├── Makefile      # (if applicable) build instructions
└── README.md
```
