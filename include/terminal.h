#pragma once
#include "EnigSh.h"
#include "GUI.h"

#ifndef COLS
#define COLS 80
#endif
#ifndef ROWS
#define ROWS 24
#endif

#ifndef MIN_FONT_SIZE
#define MIN_FONT_SIZE 8
#endif
#ifndef MAX_FONT_SIZE
#define MAX_FONT_SIZE 72
#endif

int ShellMain(int argc, char** argv, int pty_slave_fd);
int RunGUI(void);
