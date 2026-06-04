#pragma once

#define _POSIX_C_SOURCE 200809L

#include <raylib.h>
#include <unistd.h>
#include <pty.h>
#include <sys/select.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <fcntl.h>
#include <math.h>
#include <sys/wait.h>
#include <libtsm.h>

#include "terminal.h"
#include "common.h"

extern int FONT_SIZE;

void PasteFromClipboard(int master_fd);
void FontIncDec(Font font, char* font_path, int key);
int RunGUI(void);
