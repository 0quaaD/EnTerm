#include "GUI.h"

static struct tsm_screen *tsm_scr = NULL;
static struct tsm_vte *tsm_vte = NULL;
static int pty_fd = -1;

static Font font;
int FONT_SIZE = 24;

static int cell_w, cell_h;
static float y_offset;

static Color GetColorFromPalette(uint8_t idx) {
    static const Color ansi_colors[] = {
        [0]  = (Color){ 0x00, 0x00, 0x00, 0xFF }, // Black
        [1]  = (Color){ 0xFF, 0x55, 0x55, 0xFF }, // Red (Brightened)
        [2]  = (Color){ 0x55, 0xFF, 0x55, 0xFF }, // Green (Brightened)
        [3]  = (Color){ 0xFF, 0xFF, 0x55, 0xFF }, // Yellow (Brightened)
        [4]  = (Color){ 0x55, 0x55, 0xFF, 0xFF }, // Blue (Brightened)
        [5]  = (Color){ 0xFF, 0x55, 0xFF, 0xFF }, // Magenta (Brightened)
        [6]  = (Color){ 0x55, 0xFF, 0xFF, 0xFF }, // Cyan (Brightened)
        [7]  = (Color){ 0xE5, 0xE5, 0xE5, 0xFF }, // White
        [8]  = (Color){ 0x7F, 0x7F, 0x7F, 0xFF }, // Bright Black
        [9]  = (Color){ 0xFF, 0x00, 0x00, 0xFF }, // Bright Red
        [10] = (Color){ 0x00, 0xFF, 0x00, 0xFF }, // Bright Green
        [11] = (Color){ 0xFF, 0xFF, 0x00, 0xFF }, // Bright Yellow
        [12] = (Color){ 0x00, 0x00, 0xFF, 0xFF }, // Bright Blue
        [13] = (Color){ 0xFF, 0x00, 0xFF, 0xFF }, // Bright Magenta
        [14] = (Color){ 0x00, 0xFF, 0xFF, 0xFF }, // Bright Cyan
        [15] = (Color){ 0xFF, 0xFF, 0xFF, 0xFF }  // Bright White
    };
    
    if (idx < 16) {
        return ansi_colors[idx];
    }
    
    if (idx >= 16 && idx <= 231) {
        uint8_t offset = idx - 16;
        uint8_t r = (offset / 36) ? (offset / 36) * 40 + 55 : 0;
        uint8_t g = ((offset % 36) / 6) ? ((offset % 36) / 6) * 40 + 55 : 0;
        uint8_t b = (offset % 6) ? (offset % 6) * 40 + 55 : 0;
        return (Color){ r, g, b, 255 };
    }
    
    // Grayscale (232-255)
    if (idx >= 232 && idx <= 255) {
        uint8_t shade = (idx - 232) * 10 + 8;
        return (Color){ shade, shade, shade, 255 };
    }
    
    return (Color){ 0xFF, 0xFF, 0xFF, 0xFF }; // White default
}

static void vte_write_cb(struct tsm_vte *vte, const char* u8, size_t len, void* data) {
    (void)vte;
    int fd = *(int*)data;
    write(fd, u8, len);
}

static void utf8_encode(uint32_t codepoint, char* out) {
    if (codepoint <= 0x7F) {
        out[0] = (char)codepoint;
        out[1] = '\0';
    } else if (codepoint <= 0x7FF) {
        out[0] = (char)(0xC0 | (codepoint >> 6));
        out[1] = (char)(0x80 | (codepoint & 0x3F));
        out[2] = '\0';
    } else {
        out[0] = (char)(0xE0 | (codepoint >> 12));
        out[1] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
        out[2] = (char)(0x80 | (codepoint & 0x3F));
        out[3] = '\0';
    }
}

static int draw_cell_cb(struct tsm_screen *scr,
                        tsm_age_t id,
                        const uint32_t *ch,
                        tsm_age_t len,
                        unsigned int width,
                        unsigned int x,
                        unsigned int y,
                        const struct tsm_screen_attr *attr,
                        tsm_age_t age,
                        void *data)
{
    (void)scr; (void)id; (void)len; (void)width; (void)age; (void)data;

    int px = x * cell_w;
    int py = y * cell_h;

    Color fg = GetColorFromPalette(attr->fccode);
    if (fg.r < 50 && fg.g < 50 && fg.b < 50) {
        fg = (Color){ 0xE5, 0xE5, 0xE5, 0xFF };
    }

    Color bg = (Color){ 0, 0, 0, 255 };

    DrawRectangle(px, py, cell_w, cell_h, bg);

    if (1) {
        char buf[5] = {0};
        utf8_encode(*ch, buf);
        DrawTextEx(font, buf,
                   (Vector2){ px, py + y_offset },
                   FONT_SIZE, 0, fg);
    }

    return 0;
}

void PasteFromClipboard(int master_fd) {
    const char* clip = GetClipboardText();
    if(clip && *clip) write(master_fd, clip, strlen(clip));
}

static void ApplyZoom(char* font_path, int delta) {
    int new_size = FONT_SIZE + delta;
    if (new_size < MIN_FONT_SIZE || new_size > MAX_FONT_SIZE) return;
    
    FONT_SIZE = new_size;
    
    UnloadFont(font);
    font = FileExists(font_path) ? LoadFontEx(font_path, FONT_SIZE, 0, 0) : GetFontDefault();
    if (font.texture.id == 0) font = GetFontDefault();
    
    Vector2 measure = MeasureTextEx(font, "W", FONT_SIZE, 0);
    cell_w = (int)measure.x;
    cell_h = (int)measure.y;
    
    SetWindowSize(COLS * cell_w, ROWS * cell_h);
    
    tsm_screen_resize(tsm_scr, COLS, ROWS);
    struct winsize ws = {
        .ws_row = ROWS,
        .ws_col = COLS,
        .ws_xpixel = COLS * cell_w,
        .ws_ypixel = ROWS * cell_h
    };
    ioctl(pty_fd, TIOCSWINSZ, &ws);
}

int RunGUI(void) {
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(800, 600, "EnTerm");
    SetTargetFPS(60);

    char font_path[PATH_MAX];
    snprintf(font_path, sizeof(font_path), "assets/fonts/JetBrainsMono-Regular.ttf");
    
    font = FileExists(font_path) ? LoadFontEx(font_path, FONT_SIZE, 0, 0) : GetFontDefault();

    if (font.texture.id == 0) {
        fprintf(stderr, "Failed to load font, using default\n");
        font = GetFontDefault();
    }

    Vector2 measure = MeasureTextEx(font, "W", FONT_SIZE, 0);
    cell_w = (int)measure.x;
    cell_h = (int)measure.y;
    y_offset = 0;

    int term_w = COLS * cell_w, term_h = ROWS * cell_h;
    SetWindowSize(term_w, term_h);

    pid_t pid = forkpty(&pty_fd, NULL, NULL, NULL);
    if (pid < 0) return 1;
    
    char bashrc_path[256];
    char* home = getenv("HOME");

    if(home != NULL){
        snprintf(bashrc_path, sizeof(bashrc_path), "%s/.bashrc", home);
    } else {
        bashrc_path[0] = '\0';
    }

    if (pid == 0) {
        setenv("TERM", "xterm-256color", 1);
        if (bashrc_path[0] != '\0')
            execlp("bash", "bash", "--rcfile", bashrc_path, NULL);
        
        else execlp("bash", "bash", NULL);
        _exit(1);
    }

    fcntl(pty_fd, F_SETFL, O_NONBLOCK);
    tsm_screen_new(&tsm_scr, NULL, NULL);
    
    struct tsm_screen_attr default_attr = {
        .fccode = 7, 
        .bccode = 0  
    };
    tsm_screen_set_def_attr(tsm_scr, &default_attr);
    tsm_screen_reset(tsm_scr);
    tsm_screen_resize(tsm_scr, COLS, ROWS);
    
    tsm_vte_new(&tsm_vte, tsm_scr, vte_write_cb, &pty_fd, NULL, NULL);

    struct winsize ws = {
        .ws_row = ROWS,
        .ws_col = COLS,
        .ws_xpixel = COLS * cell_w,
        .ws_ypixel = ROWS * cell_h
    };
    ioctl(pty_fd, TIOCSWINSZ, &ws);

    // Main loop
    while (!WindowShouldClose()) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(pty_fd, &rfds);
        
        struct timeval tv = {0, 10000};  // 10ms timeout
        
        if (select(pty_fd + 1, &rfds, NULL, NULL, &tv) > 0) {
            if (FD_ISSET(pty_fd, &rfds)) {
                char buf[4096];
                int n = read(pty_fd, buf, sizeof(buf));
                if (n > 0) {
                    tsm_vte_input(tsm_vte, buf, n);
                } else if (n == 0) break;
                else if (n < 0 && errno != EAGAIN) break;
            }
        }
        
        int key = GetCharPressed();
        while (key > 0) {
            if (key >= 32 && key <= 126) {  
                char c = key;
                write(pty_fd, &c, 1);
            }
            key = GetCharPressed();
        }
        
        // Special keys
        if (IsKeyPressed(KEY_ENTER)) write(pty_fd, "\r", 1);
        if (IsKeyPressed(KEY_BACKSPACE)) { char c = 127; write(pty_fd, &c, 1); }
        if (IsKeyPressed(KEY_UP)) write(pty_fd, "\033[A", 3);
        if (IsKeyPressed(KEY_DOWN)) write(pty_fd, "\033[B", 3);
        if (IsKeyPressed(KEY_RIGHT)) write(pty_fd, "\033[C", 3);
        if (IsKeyPressed(KEY_LEFT)) write(pty_fd, "\033[D", 3);
        if (IsKeyPressed(KEY_TAB)) write(pty_fd, "\t", 1);
        
        // Ctrl+ Shift + C
        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyDown(KEY_LEFT_SHIFT) && IsKeyPressed(KEY_C)) {
            char c = 3; 
            write(pty_fd, &c, 1);
        }
        
        // Ctrl+ Shift + V 
        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyDown(KEY_LEFT_SHIFT) && IsKeyPressed(KEY_V)) PasteFromClipboard(pty_fd);
        
        // Ctrl+L
        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_L)) {
            char c = 12;
            write(pty_fd, &c, 1);
        }

        // Bigger Terminal 
        bool ctrl_plus = (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyDown(KEY_LEFT_SHIFT) && IsKeyPressed(KEY_EQUAL)) || (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_KP_ADD)) || (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_EQUAL)); 
        if (ctrl_plus) {
            if (FONT_SIZE < MAX_FONT_SIZE) {
                ApplyZoom(font_path, +2);
            }
        }

        // Smaller Terminal
        bool ctrl_minus = (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_MINUS) || (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_KP_SUBTRACT)));
        if (ctrl_minus) {
            if (FONT_SIZE > MIN_FONT_SIZE) {
                ApplyZoom(font_path, -2);              
            }
        }
        
        // Render
        BeginDrawing();
        ClearBackground((Color){ 20, 20, 20, 255 });
        tsm_screen_draw(tsm_scr, draw_cell_cb, NULL);

        int term_height = ROWS * cell_h;
        if (term_height < 600) {
            DrawRectangle(0, term_height, 800, 600 - term_height, (Color){ 20, 20, 20, 255 });
        }

        unsigned int cx, cy;
        cx = tsm_screen_get_cursor_x(tsm_scr); 
        cy = tsm_screen_get_cursor_y(tsm_scr);
        
        int cpx = cx * cell_w;
        int cpy = cy * cell_h;
        
        DrawRectangle(cpx, cpy, cell_w, cell_h, (Color){ 255, 255, 255, 180 });
        EndDrawing();
    }
    
    // Cleanup
    tsm_vte_unref(tsm_vte);
    tsm_screen_unref(tsm_scr);
    close(pty_fd);
    UnloadFont(font);
    CloseWindow();
    
    int status;
    waitpid(pid, &status, 0);

    return 0;
}
