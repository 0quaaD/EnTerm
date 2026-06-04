#include "EnigSh.h"


ShellFunc function_table[MAX_FUNCTIONS];
int function_count = 0;

Alias alias_table[MAX_ALIASES];
int alias_count = 0;

void LoadConfig(){
    char config_path[512];
    char* home = getenv("HOME");
    int inside_function = 0;

    if (home) {
        char hist_path[1024];
        snprintf(hist_path, sizeof(hist_path), "%s/.enigsh_history", home);
        read_history(hist_path);
        stifle_history(1000);
    }

    snprintf(config_path, sizeof(config_path), "%s/.enigshrc", home);

    FILE* file = fopen(config_path, "r");
    if (file == NULL){
        return;
    }

    char line[256];
    while(fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = 0;
        line[strcspn(line, "\r")] = 0;

        // Function detection
        if (strlen(line) == 0 || line[0] == '#') continue;
        if (inside_function){
            if(strcmp(line, "}") == 0){
                inside_function = 0;
                function_count++;
                continue;
            }

            strcat(function_table[function_count].body, line);
            strcat(function_table[function_count].body, "\n");
            continue;
        }

        // Alias detection
        if (strncmp(line, "alias ", 6) == 0){
            char* content = line + 6;
            char *eq = strchr(content, '=');
            if (eq != NULL && alias_count < MAX_ALIASES) {
                *eq = '\0';

                char* name = content;
                char* cmd = eq + 1;

                strncpy(alias_table[alias_count].shortcut, name, 32);
                strncpy(alias_table[alias_count].real_cmd, cmd, 256); 
                alias_count++;
            }
            continue;
        }

        // Function declaration
        char* func_check = strstr(line, "() {");
        if (func_check != NULL && function_count < MAX_FUNCTIONS){
            *func_check = '\0';
            char *name_ptr = line;
            while(*name_ptr == ' ') name_ptr++;

            strncpy(function_table[function_count].name, name_ptr, 32);
            memset(function_table[function_count].body, 0, MAX_FUNCTION_BODY);
            inside_function = 1;
        }
    }
    fclose(file);
}

void ExpandEnvVariables(char* input, char* output, char** func_args, int func_arg_count) {
    char* src = input;
    char* dest = output;
    *dest = '\0';

    while (*src != '\0') {
        
        if (*src == '@') {
            src++;
            
            if (*src >= '1' && *src <= '9') {
                int arg_num = *src - '0'; 
                src++;
                
                if (arg_num < func_arg_count && func_args[arg_num] != NULL) {
                    char* val = func_args[arg_num];
                    while (*val != '\0') {
                        *dest++ = *val++;
                    }
                }
                continue;
            }

            char var_name[32];
            int i = 0;
            while ((*src >= 'A' && *src <= 'Z') || (*src >= 'a' && *src <= 'z') || (*src == '_')) {
                if (i < 31) var_name[i++] = *src;
                src++;
            }
            var_name[i] = '\0';

            char* env_val = getenv(var_name);
            if (env_val != NULL) {
                while (*env_val != '\0') {
                    *dest++ = *env_val++;
                }
            }
        } else {
            *dest++ = *src++;
        }
    }
    *dest = '\0';
}

char* CheckOutputRedirection(char** args){
    for (int i=0; args[i] != NULL; i++){
        if (strcmp(args[i], ">") == 0){
            char *file_name = args[i+1];
            if (file_name == NULL){
                fprintf(stderr, "enigsh: syntax error near unexpected token 'newline'\n");
                return NULL;
            }
            args[i] = NULL;
            return file_name;
        }
    }
    return NULL;
}

int CheckExecFunc(char **args) {
    for (int f = 0; f < function_count; f++) {
        if (strcmp(args[0], function_table[f].name) == 0) {

            int func_arg_count = 0;
            while(args[func_arg_count] != NULL) func_arg_count++;

            char** func_params = &args[1];
            int param_count = func_arg_count - 1;

            char body_copy[MAX_FUNCTION_BODY];
            char *saveptr1, *saveptr2;
            
            strncpy(body_copy, function_table[f].body, MAX_FUNCTION_BODY);
            
            char *line_token = strtok_r(body_copy, "\n", &saveptr1);
            while (line_token != NULL) {
                while (*line_token == ' ' || *line_token == '\t') line_token++;
                
                if (strlen(line_token) == 0) {
                    line_token = strtok_r(NULL, "\n", &saveptr1);
                    continue;
                }

                char expanded_line[MAX_BUF];
                ExpandEnvVariables(line_token, expanded_line, func_params, param_count);

                char *sub_args[MAX_ARGS];
                int i = 0;
                char *token = strtok_r(expanded_line, " ", &saveptr2);
                while (token != NULL && i < MAX_ARGS - 1) {
                    sub_args[i++] = token;
                    token = strtok_r(NULL, " ", &saveptr2);
                }
                sub_args[i] = NULL;

                if (sub_args[0] != NULL) {
                    if (strcmp(sub_args[0], "cd") == 0 || strcmp(sub_args[0], "pushd") == 0) {
                        char *target = sub_args[1];
                        if (target == NULL) target = getenv("HOME");
                        if (chdir(target) != 0) perror("enigsh: cd");
                    }
                    else if (strcmp(sub_args[0], "pwd") == 0) {
                        char path[MAX_BUF];
                        if (getcwd(path, MAX_BUF)) printf("%s\n", path);
                    }
                    else {
                        pid_t pid = fork();
                        if (pid == 0) {
                            signal(SIGINT, SIG_DFL);
                            execvp(sub_args[0], sub_args);
                            fprintf(stderr, "enigsh: command not found: %s\n", sub_args[0]);
                            _exit(1);
                        } else if (pid > 0) {
                            waitpid(pid, NULL, 0);
                        }
                    }
                }
                
                line_token = strtok_r(NULL, "\n", &saveptr1);
            }
            return 1;
        }
    }
    return 0;
}

void DefineFunction(char* name, char* body) {
    if(function_count < MAX_FUNCTIONS) {
        strcpy(function_table[function_count].name, name);
        strcpy(function_table[function_count].body, body);
        function_count++;
        printf("Function '%s' is defined\n", name);
    }
}

void RemoveFunction(char* name) {
    if (function_count == 0) {
        printf("No functions to remove\n");
        return;
    }

    int _idx = -1;
    for (int i=0; i< function_count; i++) {
        if (strcmp(name, function_table[i].name) == 0) {
            _idx = i;
            break;
        }
    }

    if (_idx == -1) {
        printf("Couldn't find the function %s\n", name);
        return;
    }

    for (int i=_idx;i < function_count - 1; i++) {
        strcpy(function_table[i].name, function_table[i+1].name);
        strcpy(function_table[i].body, function_table[i+1].body);
    }

    memset(&function_table[function_count-1], 0, sizeof(ShellFunc));
    function_count--;
    printf("Function %s removed successfully\n", name);
}

void ListFunctions() {
    if (function_count == 0) {
        printf("No functions found\n");
        return;
    }

    printf("Defined functions:\n");
    for(int i=0;i < function_count; i++){
        printf("%s()\n", function_table[i].name);
        printf("Body: \n");

        char body_copy[MAX_FUNCTION_BODY];
        strncpy(body_copy, function_table[i].body, MAX_FUNCTION_BODY);

        char *saveptr;
        char *line = strtok_r(body_copy, "\n", &saveptr);
        while(line != NULL) {
            printf("    %s\n", line);
            line = strtok_r(NULL, "\n", &saveptr);
        }
    }
}

void HandleDEFUN(char** args){
    if(args[1] == NULL){
        printf("Usage: defun <function name> <body>\n");
        printf("Example: defun myls 'ls -lah\necho done'\n");
        return;
    }

    char body[MAX_FUNCTION_BODY] = "";
    for (int i=2; args[i] != NULL; i++){
        strcat(body, args[i]);
        if(args[i+1] != NULL) strcat(body, " ");
    }

    if (body[0] == '"' || body[0] == '\'') {
        char quote = body[0];
        int len = strlen(body);
        if(body[len-1] == quote) {
            memmove(body, body+1, len-2);
            body[len-2] = '\0';
        }
    }

    DefineFunction(args[1], body);
}

char* CommandGenerator(const char* text, int state){
    static int l_idx, len;
    static char** path_cmd = NULL;
    static int cmd_count = 0;

    if (!state) {
        if (path_cmd) {
            for (int i=0; i< cmd_count; i++) free(path_cmd[i]);
            free(path_cmd);
            path_cmd = NULL;
            cmd_count = 0;
        }

        len = strlen(text);
        l_idx = 0;

        char* path_env = getenv("PATH");
        if (!path_env) return NULL;

        char* path_cpy = strdup(path_env);
        char* saveptr;
        char* dir = strtok_r(path_cpy, ":", &saveptr);

        char** temp_list = malloc(MAX_CMDS * sizeof(char*));
        int temp_count = 0;

        while(dir && temp_count < MAX_CMDS) {
            DIR* d = opendir(dir);
            if (d) {
                struct dirent *entry;
                while((entry = readdir(d)) != NULL && temp_count < MAX_CMDS) {
                    if(entry->d_name[0] == '.') continue;
                    char fullpath[PATH_MAX];
                    snprintf(fullpath, sizeof(fullpath), "%s/%s", dir, entry->d_name);
                    if(IsExecutable(fullpath)) {
                        temp_list[temp_count++] = strdup(entry->d_name);
                    }
                }
                closedir(d);
            }
            dir = strtok_r(NULL, ":", &saveptr);
        }
        free(path_cpy);

        if(temp_count > 0) {
            path_cmd = malloc(temp_count * sizeof(char*));
            cmd_count = temp_count;
            for (int i=0;i<temp_count;i++) {
                path_cmd[i] = temp_list[i];
            }
        }
        free(temp_list);

    }

    while(l_idx < cmd_count) {
        char* cmd = path_cmd[l_idx++];
        if (strncmp(cmd, text, len) == 0)
            return strdup(cmd);
    }

    return NULL;

}

char* FunctionGenerator(const char* text, int state) {
    static int l_idx, len;
    if (!state) {
        len = strlen(text);
        l_idx = 0;
    }

    while(l_idx < function_count) {
        char* name = function_table[l_idx++].name;
        if (strncmp(name, text, len) == 0) {
            return strdup(name);
        }
    }
    return NULL;
}

char* FilenameGenerator(const char* text, int state) {
    static DIR* dir = NULL;
    static struct dirent* entry;
    static size_t len;

    if (!state) {
        if (dir) closedir(dir);
        dir = opendir(".");
        if (!dir) return NULL;
        len = strlen(text); 
    }

    while((entry = readdir(dir)) != NULL) {
        if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        if (strncmp(entry->d_name, text, len) == 0) {
            char* match = strdup(entry->d_name);
            return match;
        }
    }

    closedir(dir);
    dir = NULL;
    return NULL;
}

char* PathGenerator(const char* text, int state) {
    static DIR* dir = NULL;
    static size_t len;
    static char dir_path[PATH_MAX];
    static char *base_part = NULL;
    static struct dirent *entry;

    if (!state) {
        if (dir) closedir(dir);
        if (base_part) free(base_part);
        base_part = NULL;

        char *last_slash = strrchr(text, '/');
        if (last_slash) {
            size_t dir_len = last_slash - text + 1;
            if (dir_len >= sizeof(dir_path)) dir_len = sizeof(dir_path) - 1;
            strncpy(dir_path, text, dir_len);
            dir_path[dir_len] = '\0';
            base_part = strdup(last_slash + 1);
            len = strlen(base_part);
        } else {
            strcpy(dir_path, ".");
            base_part = strdup(text);
            len = strlen(text);
        }

        // Remove trailing slash if present (except for root)
        size_t dirlen = strlen(dir_path);
        if (dirlen > 1 && dir_path[dirlen - 1] == '/')
            dir_path[dirlen - 1] = '\0';

        dir = opendir(dir_path);
        if (!dir) {
            free(base_part);
            base_part = NULL;
            return NULL;
        }
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        if (strncmp(entry->d_name, base_part, len) == 0) {
            static char fullpath[PATH_MAX];
            // Safely build the full path
            int needed = snprintf(fullpath, sizeof(fullpath), "%s/%s",
                                  (strcmp(dir_path, ".") == 0) ? "" : dir_path,
                                  entry->d_name);
            if (needed < 0 || (size_t)needed >= sizeof(fullpath)) {
                // Path too long – skip this entry
                continue;
            }
            // For root (dir_path == "."), we prefixed with "" so there's an extra leading slash
            if (strcmp(dir_path, ".") == 0) {
                // Remove the leading slash by shifting left
                memmove(fullpath, fullpath + 1, strlen(fullpath));
            }

            if (entry->d_type == DT_DIR)
                strcat(fullpath, "/");
            return strdup(fullpath);
        }
    }


    closedir(dir);
    dir = NULL;
    free(base_part);
    base_part = NULL;
    return NULL;
}

char** CustomCompletion(const char* text, int start, int end) {
    (void)end;

    if (start == 0) {
        char **matches = NULL;
        int m_count = 0;
        rl_attempted_completion_over = 1;
        return rl_completion_matches(text, CommandGenerator);
    }

    rl_attempted_completion_over = 1;
    return rl_completion_matches(text, PathGenerator);
}

int IsExecutable(const char* path) {
    assert(path != NULL);
    assert(strlen(path) > 0);
    assert(strlen(path) < PATH_MAX);

    if (path == NULL || path[0] == '\0'){
        errno = EINVAL;
        return 0;
    }

    struct stat st;
    if (stat(path, &st) != 0) {
        
        #ifdef DEBUG_COMPLETION
        fprintf(stderr, "IsExecutable: stat failed for '%s': '%s'\n", path, stderror(errno));
        #endif

        return 0;
    }
    int executable = (st.st_mode & S_IXUSR) || (st.st_mode & S_IXGRP) || (st.st_mode & S_IXOTH);

    #ifdef DEBUG_COMPLETION
    if (!executable) {
        fprintf(stderr, "IsExecutable: '%s' is not executable\n", path);
    }
    #endif
    return executable;
}
