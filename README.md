# EnTerm
A project focused on low-level terminal interfacing and system-level programming implemented in C.

## Overview
EnTerm is designed to explore system-level programming and hardware interaction by building components from the ground up. This project reflects an ongoing investigation into robust C implementation, efficient system design, and low-level architectural control.

## Features
* **Low-Level Architecture:** Built with a focus on high-performance C.
* **System Design:** Optimized for efficient resource management and direct hardware interaction.
* **Custom Implementation:** Developed without unnecessary external dependencies to maintain a lightweight footprint.

## Prerequisites
* **Environment:** Developed and tested on Linux systems.
* **Compiler:** `gcc` or `clang` with C11 or higher support.

## Building and Running
To compile and run the project, navigate to the root directory and use:
```bash
# Building the project
./sh/build.sh
```

Running the CLI terminal
```bash
# Executing CLI version within the default terminal
./exe/enterm --text

# Or
./exe/enterm -t
```

And the execution of GUI terminal
```bash
# Used for running the GUI terminal with Raylib and libtsm
./exe/enterm --gui

# Or
./exe/enterm -g
```

## Architecture
This project focuses on providing a clean, efficient approach to terminal interaction and embedded-style programming. It leverages direct system calls and low-level memory management to achieve its functionality.

## Contributing
As this is a project focusing on deep technical exploration, contributions are currently focused on architectural refinement and code optimization. Please feel free to open an issue or pull request if you wish to contribute to the codebase.

# Licence
This project contains and protected by GPL-3.0 Licence 
