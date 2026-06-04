#include "EnigSh.h"
#include "terminal.h"

int ShellMain(int argc, char** argv, int pty_slave_fd){
    signal(SIGINT, SIG_IGN);

    if(pty_slave_fd != -1) {
        dup2(pty_slave_fd, STDIN_FILENO);
        dup2(pty_slave_fd, STDOUT_FILENO);
        dup2(pty_slave_fd, STDERR_FILENO);
        close(pty_slave_fd);
    }

    char *input = NULL;
    char expanded_input[MAX_INPUT];
    char *args[MAX_ARGS];


    // CONFIG HANDLING
    LoadConfig();
    rl_bind_key('\t', rl_complete);
    
#ifdef HAVE_RL_COMPLETION_IGNORE_CASE
    rl_completion_ignore_case = 1;
#endif
    rl_attempted_completion_function = CustomCompletion;
    rl_completion_append_character = '\0';
    rl_completion_query_items = 100;

    // HISTORY FILE SETUP
    char* home = getenv("HOME");
    char hist_path[512] = {0};
    if (home) {
        snprintf(hist_path, sizeof(hist_path), "%s/.enigsh_history", home);
        read_history(hist_path);
        stifle_history(1000);
    }

    // MAIN SHELL LOOP
    while (1) {

        input = readline("enigsh> ");
        if (!input){
            printf("\n");
            break;
        }

        if (strlen(input) > 0){
            add_history(input);
            if (hist_path[0]) {
                append_history(1, hist_path);
            }
        }

        ExpandEnvVariables(input, expanded_input, NULL, 0);
        if(strlen(expanded_input) == 0){
            free(input);
            continue;
        }

        int i=0;
        char *token = strtok(expanded_input, " ");
        while (token && i < MAX_ARGS - 1){
            args[i] = token;
            i++;
            token = strtok(NULL, " ");
        }
        args[i] = NULL;

        if (!args[0]){
            free(input);
            continue;
        }

        for (int a = 0; a < alias_count; a++){
            if (strcmp(args[0], alias_table[a].shortcut) == 0){
                args[0] = alias_table[a].real_cmd;
                break;
            }
        }

        if(CheckExecFunc(args)){
            free(input);
            continue;
        }

        // PIPE HANDLING
        int pipe_index = -1;
        for (int p = 0; args[p] != NULL; p++) {
            if (strcmp(args[p], "|") == 0) {
                pipe_index = p;
                break;
            }
        }

        if (pipe_index != -1) {
            args[pipe_index] = NULL;
            char **left_args = args;
            char **right_args = &args[pipe_index + 1];

            int pipe_fds[2];
            if (pipe(pipe_fds) < 0) {
                perror("enigsh: pipe allocation failed");
                free(input);
                continue;
            }

            pid_t pid1 = fork();
            if (pid1 == 0) {
                signal(SIGINT, SIG_DFL);
                dup2(pipe_fds[1], STDOUT_FILENO);
                close(pipe_fds[0]);
                close(pipe_fds[1]);
                
                execvp(left_args[0], left_args);
                fprintf(stderr, "enigsh: command not found: %s\n", left_args[0]);
                _exit(1);
            }

            pid_t pid2 = fork();
            if (pid2 == 0) {
                signal(SIGINT, SIG_DFL);
                dup2(pipe_fds[0], STDIN_FILENO);
                close(pipe_fds[0]);
                close(pipe_fds[1]);
                
                char* right_out_file = CheckOutputRedirection(right_args);
                if (right_out_file != NULL) {
                    FILE *f = fopen(right_out_file, "w");
                    if (f != NULL) {
                        dup2(fileno(f), STDOUT_FILENO);
                        fclose(f);
                    }
                }

                execvp(right_args[0], right_args);
                fprintf(stderr, "enigsh: command not found: %s\n", right_args[0]);
                _exit(1);
            }

            close(pipe_fds[0]);
            close(pipe_fds[1]);

            waitpid(pid1, NULL, 0);
            waitpid(pid2, NULL, 0);

            free(input);
            continue; 
        }

        // BUILTIN LINUX COMMANDS
        if ((strcmp(args[0], "exit") == 0)){
            free(input);
            break;
        }

        if (strcmp(args[0], "cd") == 0){
            char *target_dir = args[1];
            if (!target_dir){
                target_dir = getenv("HOME");
            }

            if(chdir(target_dir) != 0){
                perror("enigsh: cd");
            }
            free(input);
            continue;
        }

        if(strcmp(args[0], "pwd") == 0){
            char path[MAX_BUF];
            if(getcwd(path, MAX_BUF) != NULL){
                printf("%s\n",path);
            } else {
                perror("getcwd() error");
            }
            free(input);
            continue;
        }

        // HISTORY COMMAND
        if (strcmp(args[0], "history") == 0) {
            HIST_ENTRY **hist_list = history_list();
            if(!hist_list) {
                printf("No history entries\n");
            } else {
                for (int i=0;hist_list[i];i++) {
                    printf("%5d | %s\n", i+history_base, hist_list[i]->line);
                }
            }
            free(input);
            continue;
        }

        // CUSTOM MADE COMMANDS
        if (strcmp(args[0], "defun") == 0) {
            HandleDEFUN(args);
            free(input);
            continue;
        }
        if (strcmp(args[0], "lfunc") == 0) {
            ListFunctions();
            free(input);
            continue;
        }

        if (strcmp(args[0], "rmfunc") == 0) {
            if (args[1] == NULL) {
                printf("Usage: rmfunc <function name>\n");
            } else {
                RemoveFunction(args[1]);
            }
            free(input);
            continue;
        }

        char* out_file = CheckOutputRedirection(args);

        pid_t pid = fork();
        if (pid < 0){
            perror("Fork failed!");
            free(input);
            continue;
        }

        if (pid == 0) {
            signal(SIGINT, SIG_DFL);
            
            if(out_file != NULL){
                FILE *f = fopen(out_file, "w");
                if (f == NULL){
                    perror("enigsh: redirection open failed");
                    _exit(1);
                }
                int file_fd = fileno(f);
                dup2(file_fd, STDOUT_FILENO);
                fclose(f);
            }
            
            execvp(args[0], args);
            fprintf(stderr, "enigsh: command not found: %s\n", args[0]);
            _exit(1);
        } else {
            waitpid(pid, NULL, 0);
        }

        free(input);
    }

    if (hist_path[0]) write_history(hist_path);
    return 0;
}
