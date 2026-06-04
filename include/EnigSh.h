#pragma once

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <signal.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <assert.h>
#include <linux/limits.h>

#include "common.h"

#ifdef MAX_INPUT 
#undef MAX_INPUT
#endif

#define MAX_INPUT 1024
#define MAX_CMDS 8192
#define MAX_ARGS 64
#define MAX_BUF 200
#define MAX_ALIASES 32
#define MAX_FUNCTIONS 16
#define MAX_FUNCTION_BODY 2048
#define PATH_SIZE 1024

typedef struct {
    char shortcut[32];
    char real_cmd[256];
} Alias;

typedef struct {
    char name[32];
    char body[MAX_FUNCTION_BODY];
} ShellFunc;

extern Alias alias_table[MAX_ALIASES];
extern int alias_count;

extern ShellFunc function_table[MAX_FUNCTIONS];
extern int function_count;


int CheckExecFunc(char** args);
int IsExecutable(const char* path);

void LoadConfig();
void ExpandEnvVariables(char* input, char* output, char** func_args, int func_arg_count);

// SHELL FUNCTIONS AVAILABILITY
void HandleDEFUN(char **args);
void DefineFunction(char *name, char* body);
void ListFunctions();
void RemoveFunction(char* name);


// <TAB> BUTTON FUNCTIONALITY
char** CustomCompletion(const char* text, int start, int end);
char* CommandGenerator(const char* text, int state);
char* FunctionGenerator(const char* text, int state);
char* FilenameGenerator(const char* text, int state);
char* PathGenerator(const char* text, int state);

// REDIRECTION
char* CheckOutputRedirection(char** args);
