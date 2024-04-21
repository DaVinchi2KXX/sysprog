#include "parser.h"
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdlib.h>

int cmd_cd(const struct expr *e) {
    if (e->cmd.arg_count == 0) {
        return 0;
    }

    return chdir(e->cmd.args[0]);
}

void cmd_exit(const struct expr *e) {
    if (e->cmd.arg_count == 0) {
        exit(0);
    }
    exit(atoi(e->cmd.args[0]));
}

int execute_base_command(const struct expr *e) {
    char *exec_args[e->cmd.arg_count + 2];
    exec_args[0] = e->cmd.exe;

    for (uint32_t i = 0; i < e->cmd.arg_count; ++i)
        exec_args[i + 1] = e->cmd.args[i];

    exec_args[e->cmd.arg_count + 1] = NULL;
    return execvp(e->cmd.exe, exec_args);
}

static int execute_command(const struct command_line *line) {
    const struct expr *e = line->head;
    if (e->type == EXPR_TYPE_COMMAND) {
        pid_t pid;
        int pipe_fd[2];

        for (; e != NULL; e = e->next) {
            if (e->next && e->next->type == EXPR_TYPE_PIPE) {
                if (pipe(pipe_fd) == -1) {
                    perror("pipe");
                    return EXIT_FAILURE;
                }

                pid = fork();

                if (pid == -1) {
                    perror("fork");
                    return EXIT_FAILURE;
                } else if (pid == 0) {
                    close(pipe_fd[0]);

                    dup2(pipe_fd[1], STDOUT_FILENO);
                    close(pipe_fd[1]);

                    return execute_base_command(e);
                } else {
                    close(pipe_fd[1]);

                    dup2(pipe_fd[0], STDIN_FILENO);
                    close(pipe_fd[0]);

                    waitpid(pid, NULL, 0);
                }
            } else if (e->cmd.exe != NULL) {
                int output_fd;

                if (line->out_type == OUTPUT_TYPE_FILE_APPEND) {
                    output_fd = open(line->out_file, O_APPEND | O_RDWR | O_CREAT, 0664);
                } else if (line->out_type == OUTPUT_TYPE_FILE_NEW) {
                    output_fd = open(line->out_file, O_CREAT | O_WRONLY | O_TRUNC, 0664);
                } else {
                    output_fd = STDOUT_FILENO;
                }

                pid = fork();

                if (pid == -1) {
                    perror("fork");
                    return EXIT_FAILURE;
                } else if (pid == 0) {
                    if (line->out_type == OUTPUT_TYPE_FILE_NEW || line->out_type == OUTPUT_TYPE_FILE_APPEND) {
                        dup2(output_fd, STDOUT_FILENO);
                        close(output_fd);
                    }
                    return execute_base_command(e);
                } else {
                    if (!line->is_background) {
                        waitpid(pid, NULL, 0);
                    } else {
                        printf("Background process ID: %d\n", pid);
                    }
                }
            }
        }
    }
    return 0;
}

static int execute_command_line(const struct command_line *line) {

    if (line->head->cmd.exe != NULL && !strcmp(line->head->cmd.exe, "cd")) {
        cmd_cd(line->head);
    } else if (line->head->cmd.exe != NULL && !strcmp(line->head->cmd.exe, "exit")) {
        cmd_exit(line->head);
    } else {
//        pid_t pid = fork();
//        if (pid == 0) {
//            return execute_command(line);
//        } else {
//            wait(NULL);
//        }
            return execute_command(line);
    }
    return 0;
}

int main(void) {
    int exit_code = 0;
    const size_t buf_size = 1024;
    char buf[buf_size];
    int rc;
    struct parser *p = parser_new();
    while ((rc = read(STDIN_FILENO, buf, buf_size)) > 0) {
        parser_feed(p, buf, rc);
        struct command_line *line = NULL;
        while (exit_code == 0) {
            enum parser_error err = parser_pop_next(p, &line);
            if (err == PARSER_ERR_NONE && line == NULL)
                break;
            if (err != PARSER_ERR_NONE) {
                fprintf(stderr, "Error: %d\n", (int)err);
                continue;
            }
            exit_code = execute_command_line(line);
            command_line_delete(line);
        }
    }
    if (rc < 0) {
        perror("read");
        exit_code = EXIT_FAILURE;
    }
    parser_delete(p);
    return exit_code;
}
