# COMP521-Lab3 The Yalnix File System

## 1 Team Members

*  Hua-Yu Cheng (hc105)
*  Cho-Yun Lei (cl227)

## 2 Repository Structure

```
COMP521-Lab3/
├── yfs.c                    # Receive the msg from client, pack it and send to yfscall
├── yfscall.c                # Main implementation for the yfs 
├── global.h                 # Global definitions
├── cache/                   
│   ├── cache.c              # Cache & LRU implementation
│   ├── cache.h              
│
├── fs/                     
│   ├── path.c               # Path resolution, get inode info, path related functions
│   ├── path.h               
│
├── iolib/                   
|   ├── iolib.c              # Client calls definition
|
├── tests/                   # All the test cases
    ├── ...
```

* `yfs.c`: We implemented the logic to receive messages from the client, package them, and send them to the YFS (handle the implementation at `yfscall.c`). 
* `yfscall.c`: This is the main implementation of the YFS, where we handle the core file system functionalities.
* `global.h`: We defined global variables and constants used throughout the project.  
* `cache/cache.c:` We implemented the cache logic, including the LRU algorithm.  
* `fs/path.c`: This is the file that implemented path resolution, retrieving inode information, and other path-related functionalities.
* `iolib/iolib.c:` We defined the client-side calls and their interactions with the file system.

## 3 Running and Testing

We ran through and pass all the test cases in `/tests`
To run a simple testing on creating files, writing them and making dir with more files. You can simply run :

```
make clean && make
/clear/courses/comp421/pub/bin/mkyfs
/clear/courses/comp421/pub/bin/yalnix -lk 5 -lu 5 -ly 5 -n yfs tests/sample1
```

## 4 Implementation Overview

1. `yfs.c`  
    This file serves as the entry point for our file system server. We implemented the logic to initialize the file system, set up free blocks and inodes, and continuously listen for client requests. The main loop processes incoming messages, identifies the requested operation (e.g., file creation, reading, or writing), and delegates the task to the appropriate function in yfscall.c. This file also handles error reporting and ensures proper cleanup during shutdown.

2. `yfscall.c`  
    This is the heart of our file system implementation. We wrote functions to handle core operations such as:  
    * File Creation (YfsCreate): Allocates an inode and updates the directory structure.
    * File Reading (YfsRead): Reads data from a file, ensuring proper offset handling and boundary checks.
    * File Writing (YfsWrite): Writes data to a file, allocating new blocks as needed.
    * Directory Management (YfsMkDir, YfsRmDir): Creates and removes directories while maintaining the hierarchical structure.
    * Symbolic Links (YfsSymLink, YfsReadLink): Implements symlink creation and resolution. 

    Each function interacts with the cache and file system structures to ensure consistency and efficiency.

3. `global.h`  
    This file defines extra helper functions and variables that would help our development yfs more smooth. For example:
    * Message Structures: Used for communication between the client and server.
    * File System Limits: Defines maximum file sizes, block counts, etc.
    * Utility Functions: Includes helper functions like AllocateBlock for block management and AddDirEntry for directory updates.

4. `cache/cache.c`  
    We implemented a caching mechanism to improve performance by reducing disk I/O. This includes:
    * Inode Cache: Stores recently accessed inodes for quick retrieval.
    * Block Cache: Caches frequently used data blocks.
    * LRU Algorithm: Ensures that the least recently used items are evicted when the cache is full. 
    * Functions like AddBlockToCache and EvictBlockFromCache manage the cache lifecycle.

5. `fs/path.c`  
    This file handles all path-related operations. We implemented:  
    * Path Resolution (resolvePath): Parses a given path and traverses the directory structure to locate the target file or directory.
    * Inode Infos Retrieval: Retrieves the inode infos corresponding to a given path.
    * Error Handling: Ensures that invalid paths or inaccessible files are handled
gracefully.

6. `iolib/iolib.c`:
    This is the client-side library that provides an interface for user programs to interact with the file system. We implemented functions like:
    * Create: Sends a request to the server to create a new file.
    * Read and Write: Handle file I/O by communicating with the server.
    * MkDir and RmDir: Allow clients to create and remove directories. These functions abstract the complexity of IPC and provide a simple API for users.
    * SymLink: Creates a symbolic link from one path to another.
    * ReadLink: Retrieves the target path that a symbolic link points to.
    * Link: Creates a hard link from one file to another, allowing multiple directory entries to reference the same inode.
    * Unlink: Removes a file or symbolic link from the directory structure. When the link count reaches zero, the inode and associated data blocks are freed for reuse.
    * Stat: Provides information about a file or directory, including its type, size, inode number, and link count. This allows applications to query metadata without opening the file.
    * Sync: Flushes all pending changes from the cache to disk, ensuring data durability. This operation writes all modified inodes and data blocks to the disk, maintaining filesystem consistency.
    * Shutdown: Performs a graceful termination of the file system server after syncing all cached data to disk. This prevents data loss by ensuring all pending operations are completed before the system shuts down.

## 5 Codebase Description

Our codebase is designed to implement a file system for the Yalnix environment. The server (`yfs.c`) acts as the central component, processing client requests and delegating tasks to specialized modules. The modular structure ensures that each component has a clear responsibility:
* Core Logic: Implemented in yfscall.c, handles the main file system operations.
* Caching: Optimizes performance by reducing disk access.
* Path Handling: Ensures accurate and efficient file and directory lookups.
* Client Library: Provides a user-friendly interface for interacting with the file system.

This design allows us to maintain and extend the system easily while ensuring high performance and reliability. The use of caching and modularization ensures that the system can handle complex operations efficiently, even under heavy workloads.