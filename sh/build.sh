#!/usr/bin/bash
set -e

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${YELLOW}[INFO] ==== Setting up EnTerm ====${NC}"
mkdir -p src include exe build
echo -e "${GREEN}[OK] Directories created${NC}"

echo -e "${YELLOW}[INFO] Building enigsh (text + GUI)...${NC}"
gcc src/main.c src/shell.c src/EnigSh.c src/GUI.c \
    -o exe/enterm \
    -Iinclude \
    -lreadline -lraylib -ltsm -lm\
    -D_GNU_SOURCE

if [ $? -eq 0 ]; then
    echo -e "${GREEN}[OK] EnTerm built --> exe/enterm${NC}"
else
    echo -e "${RED}[ERROR] Build failed${NC}"
    exit 1
fi

echo -e "${GREEN}[OK] Build complete!${NC}"
echo -e "${YELLOW}Run: ./exe/enterm --gui (or -g) (GUI) or ./exe/enterm --text (or -t) (shell)${NC}"
