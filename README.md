# Yalnix Project

This repository contains the implementation of the Yalnix Operating System, File System, and a Device Driver. The project is divided into three main components:

* **Device-Driver**: A device driver for terminal and hardware interaction
* **Yalnix-OS-Kernel**: The core operating system kernel
* **Yalnix-File-System**: The file system for Yalnix

## Repository Structure

### 1. Device-Driver
This directory contains the implementation of a device driver for terminal and hardware interaction. It includes:
* `montty.c`: The main driver code
* `include/`: Header files for hardware, terminals, and threads
* `tests/`: Test cases for the device driver

### 2. Yalnix-OS-Kernel
This directory contains the implementation of the Yalnix Operating System kernel. It includes:
* Core kernel files such as `init.c`, `memory.c`, `syscalls.c`, and `trap_handler.c`
* `pcb.c` and `pcb.h`: Process control block implementation
* `tests/`: Test cases for the kernel

### 3. Yalnix-File-System
This directory contains the implementation of the Yalnix File System. It includes:
* `yfs.c` and `yfscall.c`: Core file system logic
* `cache/`, `fs/`, `iolib/`: Submodules for caching, file system operations, and I/O library
* `tests/`: Test cases for the file system

## Building and Running

Please refer to the individual README.md files under each directory.  

## License

This project is for educational purposes and is part of the COMP521 Operating Systems course.  