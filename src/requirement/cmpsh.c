#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>

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


        char *filePath;
        for (int j = 0; j < *lastPath; j++) {
            if (j > 0) {
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
            fprintf(stderr, "An error has occurred!\n");
            exit(EXIT_FAILURE);
        }

        free(filePath);
        exit(-1);
    } else {
        current_child = pid;
         wait(&status);
        current_child = -1;
    }
    return status;
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

char **getCommandsArray(const char *string) {
    char *s = strdup(string);
    int count = numberOfOccurences(s, '|');
    char **array = malloc((count + 2) * sizeof(char *)); // Allocate memory for the array
    if (array == NULL) {
        perror("malloc failed");
        exit(EXIT_FAILURE);
    }
    char *token = strtok(s, " | ");
    int i = 0;
    while (token != NULL) {
        array[i++] = strdup(token);
        token = strtok(NULL, " | ");
    }
    array[i] = NULL;
    free(s);
    return array;
}

void pipedCommand(const char *cmd) {
    const int numPipes = numberOfOccurences(cmd, '|');
    int pipe_arr[numPipes][2];
    char **commandsArray = getCommandsArray(cmd);

    for (int i = 0; i < numPipes; i++) {
        if (pipe(pipe_arr[i]) == -1) {
            fprintf(stderr, "An error has occurred!\n");
            exit(-1);
        }
    }

    for (int i = 0; i < numPipes + 1; i++) {
        int pid = fork();
        if (pid == -1) {
            fprintf(stderr, "An error has occurred!\n");
            exit(-1);
        } else if (pid == 0) {
            if (i == 0) {
                dup2(pipe_arr[i][1], STDOUT_FILENO);
            } else if (i == numPipes) {
                dup2(pipe_arr[i - 1][0], STDIN_FILENO);
            } else {
                dup2(pipe_arr[i - 1][0], STDIN_FILENO);
                dup2(pipe_arr[i][1], STDOUT_FILENO);
            }
            for (int j = 0; j < numPipes; j++) {
                close(pipe_arr[j][0]);
                close(pipe_arr[j][1]);
            }

            char filePath[256];
            snprintf(filePath, sizeof(filePath), "/bin/%s", commandsArray[i]);

            char *args[] = {commandsArray[i], NULL};
            execv(filePath, args);
            fprintf(stderr, "An error has occurred!\n");
            exit(0);
        }
    }
    for (int k = 0; k < numPipes; k++) {
        close(pipe_arr[k][0]);
        close(pipe_arr[k][1]);
    }

    // Wait for all child processes to finish
    for (int k = 0; k < numPipes + 1; k++) {
        wait(NULL);
    }
    for (int j = 0; commandsArray[j] != NULL; j++) {
        free(commandsArray[j]);
    }
    free(commandsArray);
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
    } else if (strchr(cmd, '|') != NULL) {
        pipedCommand(cmd);
    } else if (strcmp(token, "cd") == 0) {
        char *directory = strtok(0, " \0");
        commandCD(directory);
    } else if (strcmp(cmd, "pwd") == 0) {
        commandPWD();
    } else if (strcmp(token, "path") == 0) {
        char *files = strtok(0, " \0");
        commandPATHS(files, pathsArray, lastPath);
    } else {
        statusCode = commandExternal(cmd, pathsArray, lastPath);
    }
    free(cmd_copy);
    return statusCode;
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

        int status = analyzeCommand(cmd, pathsArray, lastPath);
        if (status != 0) {
            fprintf(stderr, "An error has occurred!\n");
        }
    }

    free(cmd); // Free getline buffer
    fclose(file);
    exit(0);
}


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

            int status = analyzeCommand(command, paths, &lastPath);
            if (status != 0)
                fprintf(stderr, "An error has occured!");
        }
    }
    return 0;
}
