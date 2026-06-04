#include "terminal.h"

int main(int argc, char** argv) {
    for (int i=1; i< argc; i++) {
        if (strcmp(argv[i], "--text") == 0 || strcmp(argv[i], "-t") == 0) {
            return ShellMain(argc, argv, -1);
        }
        
        if (strcmp(argv[i], "--gui") == 0 || strcmp(argv[i], "-g") == 0) {
            return RunGUI();
        }

    }
    return 0;
}
