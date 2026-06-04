#!/usr/bin/bash

mkdir -p src include exe build

for file in *.c; do
    if [ -f "$file" ]; then
        mv "$file" src/
        echo "Moved $file → src/"
    fi
done

for file in *.h; do
    if [ -f "$file" ]; then
        mv "$file" include/
        echo "Moved $file → include/"
    fi
done

echo "Project organized!"
