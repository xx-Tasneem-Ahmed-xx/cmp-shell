# cmpsh - Command Shell

## 1. Introduction

**cmpsh** (Command Shell) is a command-line interpreter implemented in C. It provides an interface for users to execute commands, including external programs found in specified paths, and offers several built-in functionalities. This version supports command piping, output redirection, dynamic path management, and execution of commands from a file.

## 2. Compilation and Execution

- To compile, run:
  ```bash
  make
  ```
- To execute, run:
  ```bash
  ./cmpsh
  ```

### Modes of Operation

- **Interactive Mode**: Run the shell directly to enter commands one by one.
- **Non-Interactive Mode**: Provide a filename as a command-line argument. `cmpsh` will read and execute commands from this file line by line, then exit.

Example:

```bash
./cmpsh <script_filename>
```

## 3. Features and Usage

### a) Command Prompt

In interactive mode, the shell indicates readiness with the `cmpsh>` prompt.

### b) Command Execution

Enter a command followed by its arguments and press Enter. cmpsh determines how to handle the command:

- **Built-in Commands**: Handled directly by the shell.
- **Piped Commands**: Commands linked by `|`.
- **External Commands**: Executable programs found in the configured search paths.

### c) Built-in Commands

- `exit`: Terminates the cmpsh shell.
- `pwd`: Prints the current working directory.
- `cd <directory>`: Changes the current working directory.
    - Supports relative (., ..) and absolute (/) paths.
- `path`: Manages the search path list.
    - `path` with no arguments: Clears the search path.
    - `path <dir1> [<dir2> ...]`: Adds directories to the search path.

**Note:** Initially, `/bin` is the only directory in the search path.

### d) External Commands

Any non-built-in, non-piped command is treated as external.

- `cmpsh` searches executable files in its internal `pathsArray`.
- Forks a new process and executes using `execv`.
- Example: `ls -l`, `grep pattern file.txt`, `gcc myprog.c`

### e) Output Redirection (`>`)

- Redirects standard output to a file.
- Syntax: `command [args...] > filename`
- Overwrites if file exists; creates it otherwise.

### f) Piping (`|`)

- Connects the output of one command to the input of another.
- Syntax: `command1 [args...] | command2 [args...] | ...`
- Commands are parsed and executed with pipes.

Example:

```bash
ls -1 | grep .c | wc -l
```

### g) Argument Parsing

- **Simple Commands**: Arguments split by spaces; quoted strings split.
- **Piped Commands**: Quoted arguments treated as a single unit.

Example:

```bash
grep 'search phrase' file.txt | wc -l
```

### h) Bash File Execution

- Commands starting with `./` are treated as batch scripts.
- Example:

```bash
cmpsh> ./my_script.txt
```

### i) Signal Handling

- **Ctrl+C (SIGINT)**: Forwards signal to running child processes.
- **Ctrl+Z (SIGTSTP)**: Forwarded to children; no job control.

**Note:** these signals won't affect the shell itself.

## 4. Limitations


- Basic file execution; no variables, loops, or conditionals.
- Ignores the system's `PATH`; uses its own `pathsArray`.

## 5. Exiting cmpsh

- Type `exit` and press Enter.
- Press `Ctrl+D` at an empty prompt (EOF).

## 6. Example Session

```bash
# Compile and run
$ make
$ ./cmpsh

# Interactive session
cmpsh> pwd
/home/user/project
cmpsh> path /usr/bin .
cmpsh> ls
cmpsh.c cmpsh
cmpsh> mkdir output
cmpsh> ls -la > output/listing.txt
cmpsh> cat output/listing.txt
... (contents of ls -la) ...
cmpsh> echo 'Hello World with Spaces'
'Hello World with Spaces'
cmpsh> echo "Another Test" | wc -c
13
cmpsh> cd output
cmpsh> pwd
/home/user/project/output
cmpsh> path
cmpsh> ls
Command not found
cmpsh> /bin/ls
listing.txt
cmpsh> exit

# Back in the original shell
$
```

---

Enjoy using **cmp shell**!

