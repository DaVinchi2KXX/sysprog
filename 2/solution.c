#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <assert.h>
#include "parser.h"

static void execute_command(const struct command *cmd) {
    pid_t child_pid = fork();
    if (child_pid == -1) {
        perror("fork");
    }

    if (child_pid == 0) {
        execvp(cmd->exe, cmd->args);
        perror("execvp");
    } else {
        int status;
        waitpid(child_pid, &status, 0);

        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            fprintf(stderr, "Command failed: %s\n", cmd->exe);
            }
    }
}

static void execute_pipe(const struct command *cmd1, const struct command *cmd2) {
    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) {
        perror("pipe");
    }

    pid_t child_pid1 = fork();
    if (child_pid1 == -1) {
        perror("fork");
    }

    if (child_pid1 == 0) {
        close(pipe_fd[0]);
        dup2(pipe_fd[1], STDOUT_FILENO);
        close(pipe_fd[1]);

        execvp(cmd1->exe, cmd1->args);
        perror("execvp");
    } else {
        pid_t child_pid2 = fork();
        if (child_pid2 == -1) {
            perror("fork");
            }

        if (child_pid2 == 0) {
            close(pipe_fd[1]);
            dup2(pipe_fd[0], STDIN_FILENO);
            close(pipe_fd[0]);

            execvp(cmd2->exe, cmd2->args);
            perror("execvp");
            } else {
            close(pipe_fd[0]);
            close(pipe_fd[1]);

            int status1, status2;
            waitpid(child_pid1, &status1, 0);
            waitpid(child_pid2, &status2, 0);

            if ((WIFEXITED(status1) && WEXITSTATUS(status1) != 0) ||
                (WIFEXITED(status2) && WEXITSTATUS(status2) != 0)) {
                fprintf(stderr, "Command in the pipeline failed.\n");
                    }
        }
    }
}

static void execute_command_line(const struct command_line *line) {
    const struct expr *e = line->head;

    while (e != NULL) {
        if (e->type == EXPR_TYPE_COMMAND) {
            if (line->is_background) {
                execute_command(&e->cmd);
            } else {
                execute_command(&e->cmd);
            }
        } else if (e->type == EXPR_TYPE_PIPE) {

            const struct command *cmd1 = &e->cmd;
            const struct command *cmd2 = &e->next->cmd;
            execute_pipe(cmd1, cmd2);
            e = e->next;
        } else if (e->type == EXPR_TYPE_AND) {

            const struct command *cmd1 = &e->cmd;

            int status1;
            pid_t child_pid1 = fork();
            if (child_pid1 == -1) {
                perror("fork");
                    }

            if (child_pid1 == 0) {
                execute_command(cmd1);
            } else {
                waitpid(child_pid1, &status1, 0);


                if (!(WIFEXITED(status1) && WEXITSTATUS(status1) == 0)) {
                    fprintf(stderr, "Command in AND failed.\n");
                            }
            }
        } else if (e->type == EXPR_TYPE_OR) {

            const struct command *cmd1 = &e->cmd;

            int status1;
            pid_t child_pid1 = fork();
            if (child_pid1 == -1) {
                perror("fork");
                    }

            if (child_pid1 == 0) {
                execute_command(cmd1);
            } else {
                waitpid(child_pid1, &status1, 0);


                if (!(WIFEXITED(status1) && WEXITSTATUS(status1) != 0)) {
                    fprintf(stderr, "Command in OR failed.\n");
                }
            }
        }

        e = e->next;
    }

    assert(line != NULL);
    printf("================================\n");
    printf("Command line:\n");
    printf("Is background: %d\n", (int)line->is_background);
    printf("Output: ");
    if (line->out_type == OUTPUT_TYPE_STDOUT) {
        printf("stdout\n");
    } else if (line->out_type == OUTPUT_TYPE_FILE_NEW) {
        printf("new file - \"%s\"\n", line->out_file);
    } else if (line->out_type == OUTPUT_TYPE_FILE_APPEND) {
        printf("append file - \"%s\"\n", line->out_file);
    } else {
        assert(false);
    }
    printf("Expressions:\n");
    const struct expr *e_res = line->head;
    while (e_res != NULL) {
        if (e_res->type == EXPR_TYPE_COMMAND) {
            printf("\tCommand: %s", e_res->cmd.exe);
            for (uint32_t i = 0; i < e_res->cmd.arg_count; ++i)
                printf(" %s", e_res->cmd.args[i]);
            printf("\n");
        } else if (e_res->type == EXPR_TYPE_PIPE) {
            printf("\tPIPE\n");
        } else if (e_res->type == EXPR_TYPE_AND) {
            printf("\tAND\n");
        } else if (e_res->type == EXPR_TYPE_OR) {
            printf("\tOR\n");
        } else {
            assert(false);
        }
        e_res = e_res->next;
    }
    if (line->out_type == OUTPUT_TYPE_FILE_NEW || line->out_type == OUTPUT_TYPE_FILE_APPEND) {
        close(STDOUT_FILENO);
    }
}


int
main(void)
{
	const size_t buf_size = 1024;
	char buf[buf_size];
	int rc;
	struct parser *p = parser_new();
	while ((rc = read(STDIN_FILENO, buf, buf_size)) > 0) {
		parser_feed(p, buf, rc);
		struct command_line *line = NULL;
		while (true) {
			enum parser_error err = parser_pop_next(p, &line);
			if (err == PARSER_ERR_NONE && line == NULL)
				break;
			if (err != PARSER_ERR_NONE) {
				printf("Error: %d\n", (int)err);
				continue;
			}
			execute_command_line(line);
			command_line_delete(line);
		}
	}
	parser_delete(p);
	return 0;
}
