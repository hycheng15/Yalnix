#ifndef _IOLIB_OUR_H
#define _IOLIB_OUR_H

#include <comp421/yalnix.h>
#include <comp421/iolib.h>
#include <comp421/filesystem.h>
#include "../global.h"

typedef struct OpenFile {
    int fd;             // File descriptor
    int inodeNumber;    // Inode number of the file
    int offset;         // Current offset in the file
    int reuse;          // Reuse count for the inode
} OpenFile;

OpenFile *openFiles[MAX_OPEN_FILES];    // Array to hold pointers to OpenFile structures
int numOpenFiles;                       // Number of open files
int currentWorkingDirectory;
int cwdReuse;

#endif /* _IOLIB_OUR_H */