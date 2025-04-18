#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <ctype.h>
pid_t current_child = -1;

void parentSignalHandler(int sig) {
    if (current_child > 0) {
        kill(current_child, sig);
    }
    signal(SIGINT, parentSignalHandler);
    signal(SIGTSTP, parentSignalHandler);
}


int outputToFile(char *redirect_file) {
    if (redirect_file) {
        int fd = open(redirect_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd == -1) return -1;

        if (dup2(fd, STDOUT_FILENO) == -1) {
            close(fd);
            return -1;
        }
        close(fd);
    }
    return 0;
}

bool isRedirectable(char *cmd, char **redirection) {
    char *cmd_copy = strdup(cmd);
    if (!cmd_copy) return false;

    *redirection = strstr(cmd_copy, ">");
    if (!*redirection) {
        free(cmd_copy);
        return false;
    }

    **redirection = '\0';
    (*redirection)++;

    while (**redirection == ' ') {
        (*redirection)++;
    }

    *redirection = strdup(*redirection);
    if (!*redirection) {
        free(cmd_copy);
        return false;
    }

    strcpy(cmd, cmd_copy);

    free(cmd_copy);
    return true;
}

int commandCD(char *directory) {
    int returnStatus = chdir(directory);
    return returnStatus;
}

void commandPWD() {
    char cwd[1024];

    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s\n", cwd);
    }
}

void commandPATHS(char *path, char **pathsArray, int *lastPath) {
    if (path == NULL) {
        for (int i = 0; i < *lastPath; i++) {
            free(pathsArray[i]);
            pathsArray[i] = NULL;
        }

        *lastPath = 0;
    }

    while (path != NULL) {
        pathsArray[*lastPath] = strdup(path);
        (*lastPath)++;
        path = strtok(0, " ");
    }
}

int numberOfOccurences(const char *string, char c) {
    const char *ptr = string;
    int count = 0;
    while ((ptr = strchr(ptr, c)) != NULL) {
        count++;
        ptr++;
    }
    return count;
}
char** getCommandsArray(char* cmd) {
    char** commands = malloc(sizeof(char*) * 10); // Start with space for 10 commands
    int count = 0;
    char* saveptr;
    char* token = strtok_r(cmd, "|", &saveptr);

    while (token != NULL) {
        // Trim whitespace from the token
        while (*token == ' ' || *token == '\t') token++;
        char* end = token + strlen(token) - 1;
        while (end > token && (*end == ' ' || *end == '\t')) end--;
        *(end + 1) = '\0';

        commands[count] = strdup(token);
        count++;

        if (count % 10 == 0) {
            commands = realloc(commands, sizeof(char*) * (count + 10));
        }

        token = strtok_r(NULL, "|", &saveptr);
    }

    commands[count] = NULL; // NULL-terminate the array
    return commands;
}

char **parse_command_args(char *cmd, int *arg_count) {
    // Skip leading whitespace
    while (*cmd == ' ' || *cmd == '\t') cmd++;

    char **args = malloc(sizeof(char*));
    *arg_count = 0;
    char *current = cmd;
    int in_quotes = 0;
    char quote_char = 0;

    while (*current) {
        if (!in_quotes && (*current == ' ' || *current == '\t')) {
            *current = '\0';
            args = realloc(args, (*arg_count + 1) * sizeof(char*));
            args[(*arg_count)++] = cmd;

            current++;
            while (*current == ' ' || *current == '\t') current++;
            cmd = current;
            continue;
        }

        if (*current == '\'' || *current == '"') {
            if (!in_quotes) {
                quote_char = *current;
                in_quotes = 1;
                memmove(current, current+1, strlen(current));
            } else if (*current == quote_char) {
                in_quotes = 0;
                memmove(current, current+1, strlen(current));
            } else {
                current++;
            }
            continue;
        }

        current++;
    }

    // Add the last argument
    if (*cmd) {
        args = realloc(args, (*arg_count + 1) * sizeof(char*));
        args[(*arg_count)++] = cmd;
    }

    // NULL terminate
    args = realloc(args, (*arg_count + 1) * sizeof(char*));
    args[*arg_count] = NULL;

    return args;
}

  void pipedCommand(char *cmd, char **pathsArray,  int *lastPath) {
      const int numPipes = numberOfOccurences(cmd, '|');
      int pipe_arr[numPipes][2];
      char **commandsArray = getCommandsArray(cmd);

      for (int i = 0; i < numPipes; i++) {
          if (pipe(pipe_arr[i]) == -1) {
              exit(EXIT_FAILURE);
          }
      }

      for (int i = 0; i < numPipes + 1; i++) {
          int pid = fork();

          if (pid == -1) {
              exit(EXIT_FAILURE);
          }

          if (pid == 0) {
              // Set up output redirection for all except first pipe
              if (i != 0) {
                  dup2(pipe_arr[i-1][0], STDIN_FILENO);
              }

              // Set up output redirection for all except last pipe
              if (i != numPipes) {
                  dup2(pipe_arr[i][1], STDOUT_FILENO);
              }

              // Close all pipe ends in child
              for (int j = 0; j < numPipes; j++) {
                  close(pipe_arr[j][0]);
                  close(pipe_arr[j][1]);
              }

              // Parse the current command
              int arg_count = 0;
              char** args = parse_command_args(commandsArray[i], &arg_count);
              if (!args) {
                  _exit(EXIT_FAILURE);
              }

              char filePath[512];

              for (int j = 0; j < *lastPath; j++) {
                  strcpy(filePath, pathsArray[j]);
                  strcat(filePath, "/");
                  strcat(filePath, args[0]);
                  execv(filePath, args);
              }

              _exit(EXIT_FAILURE);
          }
      }

      // Parent process close all pipe ends
      for (int j = 0; j < numPipes; j++) {
          close(pipe_arr[j][0]);
          close(pipe_arr[j][1]);
      }

      // Wait for all children
      for (int k = 0; k < numPipes + 1; k++) {
          wait(NULL);
      }

      // Clean up commands array
      for (int j = 0; commandsArray[j] != NULL; j++) {
          free(commandsArray[j]);
      }
      free(commandsArray);
  }



void executeCommandsFromFile(const char *filename, char **pathsArray, int *lastPath);
int commandExternal(char *cmd, char **pathsArray, int *lastPath);
int analyzeCommand(char *cmd, char **pathsArray, int *lastPath);
void parentProcessHandler(int signum);

int main(int argc, char *argv[]) {
    signal(SIGINT, parentSignalHandler);
    signal(SIGTSTP, parentSignalHandler);

    char *prompt = "cmpsh> ";
    size_t size = 32;
    char *command = (char *) malloc(size * sizeof(char));

    char *paths[200] = {NULL};
    paths[0] = strdup("/bin");
    int lastPath = 1;

    if (argc > 1) {
        executeCommandsFromFile(argv[1], paths, &lastPath);
    } else {
        while (1) {
            printf("%s", prompt);
            getline(&command, &size, stdin);

            // remove new line chars read from stdin
            command[strcspn(command, "\n")] = '\0';

            if (command == "")
                continue;


            analyzeCommand(command, paths, &lastPath);
        }
    }
    return 0;
}

int commandExternal(char *cmd, char **pathsArray, int *lastPath) {
    int status = 0;
    int pid;
    pid = fork();

    if (pid == -1)
        return -1;

    if (pid == 0) {
        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);

        char *args[100];
        int i = 0;

        char *redirect_file;
        if (isRedirectable(cmd, &redirect_file)) {
            outputToFile(redirect_file);
            free(redirect_file);
        }

        char *token = strtok(cmd, " ");
        while (token != NULL) {
            args[i++] = token;
            token = strtok(NULL, " ");
        }
        args[i] = NULL;


        char *filePath = NULL;
        for (int j = 0; j < *lastPath; j++) {
            if (filePath != NULL) {
                free(filePath);
            }

            filePath = malloc(strlen(pathsArray[j]) + strlen(args[0]) + 2);
            //one for the null terminator and the other for backslash
            if (filePath == NULL)
                return -1;

            // creating command file path
            strcpy(filePath, pathsArray[j]);
            strcat(filePath, "/");
            strcat(filePath, args[0]);

            execv(filePath, args);
        }

        free(filePath);
        exit(0);
    } else {
        current_child = pid;
        wait(&status);
        current_child = -1;
    }
    return status;
}

void executeCommandsFromFile(const char *filename, char **pathsArray, int *lastPath) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        return;
    }

    char *cmd = NULL;
    size_t size = 0;

    while (getline(&cmd, &size, file) != -1) {
        cmd[strcspn(cmd, "\n")] = '\0';

        if (strlen(cmd) == 0) {
            continue;
        }
        if (strstr(cmd, "./"))
            commandExternal(cmd, pathsArray, lastPath);
        else analyzeCommand(cmd, pathsArray, lastPath);
    }

    free(cmd); // Free getline buffer
    fclose(file);
    exit(0);
}
int analyzeCommand(char *cmd, char **pathsArray, int *lastPath) {
    int statusCode = 0;
    char *cmd_copy = malloc(strlen(cmd) + 1);

    if (cmd_copy == NULL)
        return -1;

    strcpy(cmd_copy, cmd);

    char *token = strtok(cmd_copy, " ");

    if (strcmp(cmd, "exit") == 0) {
        free(cmd_copy);
        free(cmd);
        exit(0);
    }
    if (strchr(cmd, '|') != NULL) {
        pipedCommand(cmd, pathsArray, lastPath);
    } else if (strcmp(token, "cd") == 0) {
        char *directory = strtok(0, " \0");
        commandCD(directory);
    } else if (strcmp(cmd, "pwd") == 0) {
        commandPWD();
    } else if (strcmp(token, "path") == 0) {
        char *files = strtok(0, " \0");
        commandPATHS(files, pathsArray, lastPath);
    }
    else if (strstr(token, "./") != NULL) {
        executeCommandsFromFile(cmd, pathsArray, lastPath);
    }
    else {
        statusCode = commandExternal(cmd, pathsArray, lastPath);
    }
    free(cmd_copy);
    return statusCode;
}