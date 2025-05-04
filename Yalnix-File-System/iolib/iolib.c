/*
* A YFS file system library
* Each of the procedure communicates with the server process using Yalnix IPC kernel calls
*/

#include <comp421/yalnix.h>
#include <comp421/iolib.h>
#include <comp421/filesystem.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "iolib.h"
#include "../cache/cache.h"
#include "../fs/path.h"

OpenFile *openFiles[MAX_OPEN_FILES];        // Array to hold pointers to OpenFile structures
int numOpenFiles = 0;                       // Number of open files
int currentWorkingDirectory = ROOTINODE;    // Process's current working directory's inode number
int cwdReuse = 1;                           // Reuse count for the current working directory        

/**
 * Finds the first available file descriptor
 * @return The file descriptor number, or ERROR if no file descriptor is available
 */
int findAvailableFD() {
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (openFiles[i] == NULL) {
            TracePrintf(0, "iolib: findAvailableFD - Found available fd: %d\n", i);
            return i;
        }
    }
    TracePrintf(0, "iolib: findAvailableFD - ERROR: No available file descriptor.\n");
    return ERROR; // No available file descriptor
}

/** 
 * Opens the file named by path name
 * It is not an ERROR to open a directory
 * @param pathname The name of the file to open
 * @return The file descriptor number of the opened file, or ERROR if the file does not exists
*/
int Open(char *pathname) {
    TracePrintf(0, "iolib: Open - %s\n", pathname);

    // Check if the pathname is valid
    if (strlen(pathname) + 1 > MAXPATHNAMELEN) {
        TracePrintf(0, "iolib: Open - ERROR: pathname exceeds MAXPATHNAMELEN\n");
        printf("ERROR: pathname exceeds MAXPATHNAMELEN\n");
        return ERROR;
    }

    if (numOpenFiles >= MAX_OPEN_FILES) {
        TracePrintf(0, "iolib: Open - ERROR: Maximum number of open files reached.\n");
        printf("ERROR: Maximum number of open files reached.\n");
        return ERROR; // No available file descriptor
    }

    // Send the request to the server
    YfsMsg *msg = calloc(1, sizeof(YfsMsg));
    msg->type = YFS_OPEN;
    msg->data1 = currentWorkingDirectory;
    msg->data2 = cwdReuse;
    msg->addr1 = (void*)pathname;

    if (Send((void*)msg, -FILE_SERVER) == ERROR) {
        free(msg);
		return ERROR;
    }

    if (msg->type == ERROR) {
        free(msg);
        TracePrintf(0, "iolib: Open - ERROR: File does not exist.\n");
        printf("ERROR: File does not exist.\n");
        return ERROR;
    }
    int fd = findAvailableFD();
    // Check if there exists available fd again
    if (fd == ERROR) {
        free(msg);
        TracePrintf(0, "iolib: Open - ERROR: No available file descriptor.\n");
        printf("ERROR: No available file descriptor.\n");
        return ERROR;
    }
    openFiles[fd] = malloc(sizeof(OpenFile));
    openFiles[fd]->fd = fd;
    openFiles[fd]->inodeNumber = msg->data1;
    openFiles[fd]->offset = 0;
    openFiles[fd]->reuse = msg->data2;
    numOpenFiles++;
    free(msg);
    TracePrintf(0, "iolib: Open - fd: %d, inodeNumber: %d, reuse: %d\n", fd, openFiles[fd]->inodeNumber, openFiles[fd]->reuse);
    return fd;
}

/**
 * Closes the file descriptor
 * @param fd The file descriptor to close
 * @return 0 on success, or ERROR if the file descriptor is invalid
*/
int Close(int fd) {
    TracePrintf(0, "iolib: Close - %d\n", fd);
    if (fd < 0 || fd >= MAX_OPEN_FILES || openFiles[fd] == NULL) {
        TracePrintf(0, "iolib: Close - ERROR: Invalid file descriptor.\n");
        printf("ERROR: Invalid file descriptor.\n");
        return ERROR;
    }

    free(openFiles[fd]);
    openFiles[fd] = NULL;
    numOpenFiles--;

    return 0;
}

/**
 * Creates and opens a new file with the given pathname.
 * Creates only the last file name component of the pathname.
 * If the file already exists, its size is truncated to 0
 * @param pathname The name of the file to create
 * @return The file descriptor number of the created file
 */
int Create(char *pathname) {
    if (strlen(pathname) + 1 > MAXPATHNAMELEN) {
        TracePrintf(0, "iolib: Create - ERROR: pathname exceeds MAXPATHNAMELEN\n");
        printf("ERROR: pathname exceeds MAXPATHNAMELEN\n");
		return ERROR;
    }

    if (numOpenFiles >= MAX_OPEN_FILES) {
        TracePrintf(0, "iolib: Create - ERROR: Maximum number of open files reached.\n");
        printf("ERROR: Maximum number of open files reached.\n");
        return ERROR; // No available file descriptor
    }

    // Send the request to the server
    YfsMsg *msg = calloc(1, sizeof(YfsMsg));
    msg->type = YFS_CREATE;
    msg->data1 = currentWorkingDirectory;
    msg->data2 = cwdReuse;
    msg->addr1 = strdup(pathname);

    if (Send((void*)msg, -FILE_SERVER) == ERROR) {
        free(msg);
        free(msg->addr1);
        return ERROR;
    }

    if (msg->type == ERROR) {
        free(msg);
        free(msg->addr1);
        TracePrintf(0, "iolib: Create - ERROR: Cannot create file.\n");
        printf("ERROR: Cannot create file.\n");
        return ERROR;
    }

    // Create a new file descriptor
    int fd = findAvailableFD();
    // Check if there exists available fd again
    if (fd == ERROR) {
        free(msg);
        TracePrintf(0, "iolib: Create - ERROR: No available file descriptor.\n");
        printf("ERROR: No available file descriptor.\n");
        return ERROR;
    }
    openFiles[fd] = malloc(sizeof(OpenFile));
    openFiles[fd]->fd = fd;
    openFiles[fd]->inodeNumber = msg->data1;
    openFiles[fd]->offset = 0;
    openFiles[fd]->reuse = msg->data2;
    numOpenFiles++;
    free(msg);
    free(msg->addr1);
    TracePrintf(0, "iolib: Create - fd: %d, inodeNumber: %d, reuse: %d\n", fd, openFiles[fd]->inodeNumber, openFiles[fd]->reuse);
    return fd;
}

/**
 * Reads size bytes beginning at the offest in the file represented by fd into the buffer buf.
 * It is not an ERROR to read from a directory
 * @param fd The file descriptor to read from
 * @param buf The buffer addr in the requesting process to read into
 * @param size The number of bytes to read
 * @return The number of bytes read, or ERROR on any error
 */
int Read(int fd, void *buf, int size) {
    TracePrintf(0, "iolib: Read - fd: %d, buf: %p, size: %d\n", fd, buf, size);
    if (fd < 0 || fd >= MAX_OPEN_FILES || buf == NULL || size < 0 || openFiles[fd] == NULL) {
        TracePrintf(0, "iolib: Read - ERROR: Invalid argument\n");
        printf("ERROR: Invalid argument\n");
        return ERROR;
    }

    // Send the request to the server
    YfsMsg *msg = calloc(1, sizeof(YfsMsg));
    msg->type = YFS_READ;
    msg->data1 = openFiles[fd]->inodeNumber;
    msg->data2 = openFiles[fd]->offset;
    msg->data3 = size;
    msg->addr1 = (void*)buf;
    msg->addr2 = (void*)(long)(openFiles[fd]->reuse);       // Since we don't have data4 slot, we use addr2 to store reuse
    if (Send((void*)msg, -FILE_SERVER) == ERROR) {
        free(msg);
        return ERROR;
    }

    if (msg->type == ERROR) {
        free(msg);
        TracePrintf(0, "iolib: Read - ERROR: Cannot read file.\n");
        printf("ERROR: Cannot read file.\n");
        return ERROR;
    }
    // Upon completion, the current position in the file
    // should be advance by the number of bytes read
    int bytesRead = msg->data1;
    openFiles[fd]->offset += bytesRead;
    free(msg);
    return bytesRead;
}

/**
 * Writes size bytes beginning at the offset in the file represented by fd into the buffer buf.
 * It is an ERROR to write to a directory
 * @param fd The file descriptor to write to
 * @param buf The buffer addr in the requesting process to write to
 * @param size The number of bytes to write
 * @return The number of bytes write, or ERROR on any error
 */
int Write(int fd, void *buf, int size) {
    TracePrintf(0, "iolib: Write - fd: %d, buf: %p, size: %d\n", fd, buf, size);
    if (fd < 0 || fd >= MAX_OPEN_FILES || buf == NULL || size < 0 || openFiles[fd] == NULL) {
        TracePrintf(0, "iolib: Read - ERROR: Invalid argument\n");
        printf("ERROR: Invalid argument\n");
        return ERROR;
    }

    // Send the request to the server
    YfsMsg *msg = calloc(1, sizeof(YfsMsg));
    msg->type = YFS_WRITE;
    msg->data1 = openFiles[fd]->inodeNumber;
    msg->data2 = openFiles[fd]->offset;
    msg->data3 = size;
    msg->addr1 = (void*)buf;
    msg->addr2 = (void*)(long)(openFiles[fd]->reuse);       // Since we don't have data4 slot, we use addr2 to store reuse
    if (Send((void*)msg, -FILE_SERVER) == ERROR) {
        free(msg);
        return ERROR;
    }

    if (msg->type == ERROR) {
        free(msg);
        TracePrintf(0, "iolib: Write - ERROR: Cannot write file.\n");
        printf("ERROR: Cannot write file.\n");
        return ERROR;
    }

    // Upon completion, the current position in the file
    // should be advance by the number of bytes written
    int bytesWrite = msg->data1;
    openFiles[fd]->offset += bytesWrite;
    free(msg);
    return bytesWrite;
}


/**
 * Changes the current file position of the file descriptor fd
 * to the given offset relative to the given whence
 * @param fd The file descriptor to seek
 * @param offset The offset to seek to, can be negative, positive, or zero
 * @param whence The reference point for the offset: SEEK_SET, SEEK_CUR, SEEK_END
 * @return The new file position, or ERROR on any error
 */
int Seek(int fd, int offset, int whence) {
    TracePrintf(0, "iolib: Seek - fd: %d, offset: %d, whence: %d\n", fd, offset, whence);
    if (fd < 0 || fd >= MAX_OPEN_FILES || openFiles[fd] == NULL) {
        TracePrintf(0, "iolib: Read - ERROR: Invalid argument\n");
        printf("ERROR: Invalid argument\n");
        return ERROR;
    }

    // Send the request to the server
    YfsMsg *msg = calloc(1, sizeof(YfsMsg));
    msg->type = YFS_SEEK;
    msg->data1 = openFiles[fd]->inodeNumber;
    msg->data2 = offset;
    msg->data3 = whence;
    msg->addr1 = (void*)(long)(openFiles[fd]->offset);      // Since we don't have data4 slot, we use addr1 to store currentOffset
    msg->addr2 = (void*)(long)(openFiles[fd]->reuse);       // Since we don't have data5 slot, we use addr2 to store reuse

    if (Send((void*)msg, -FILE_SERVER) == ERROR) {
        free(msg);
        return ERROR;
    }

    if (msg->type == ERROR) {
        free(msg);
        TracePrintf(0, "iolib: Seek - ERROR: Cannot seek to target.\n");
        printf("ERROR: Cannot seek to target.\n");
        return ERROR;
    }

    // Upon completion, set the current offset to the new offset, and return the new offset
    int newOffset = msg->data1;
    openFiles[fd]->offset = newOffset;
    free(msg);
    return newOffset;
}

/**
 * Creates a hard link from the file named by newname to the file named by oldname
 * @param oldname The name of the file to link to, must not be a directory
 * @param newname The name of the new link
 * @return 0 on success, or ERROR on any error
 */
int Link(char *oldname, char *newname) {
    TracePrintf(0, "iolib: Link - oldname: %s, newname: %s\n", oldname, newname);

    // error if oldname and newname is longer than MAXPATHNAMELEN
    if (strlen(oldname) + 1 > MAXPATHNAMELEN || strlen(newname) + 1 > MAXPATHNAMELEN) {
        TracePrintf(0, "iolib: Link - ERROR: pathname exceeds MAXPATHNAMELEN\n");
        printf("ERROR: pathname exceeds MAXPATHNAMELEN\n");
        return ERROR;
    }

    YfsMsg *msg = calloc(1, sizeof(YfsMsg));
    msg->type = YFS_LINK;
    msg->data1 = currentWorkingDirectory;
    msg->data2 = cwdReuse;
    msg->addr1 = strdup(oldname);
    msg->addr2 = strdup(newname);

    if (Send((void *)msg, -FILE_SERVER) == ERROR) {
        free(msg);
        free(msg->addr1);
        free(msg->addr2);
        return ERROR;
    }

    if (msg->type == ERROR) {
        free(msg);
        free(msg->addr1);
        free(msg->addr2);
        TracePrintf(0, "iolib: Link - ERROR: Cannot create link.\n");
        printf("ERROR: Cannot create link.\n");
        return ERROR;
    }

    free(msg);
    free(msg->addr1);
    free(msg->addr2);
    return 0;
}

/**
 * Removes the directory entry for pathname
 * @param pathname The name of the file to unlink, must not be a directory
 * @return 0 on success, or ERROR on any error
 */
int Unlink(char *pathname) {
    TracePrintf(0, "iolib: Unlink - %s\n", pathname);
    if (strlen(pathname) + 1 > MAXPATHNAMELEN) {
        TracePrintf(0, "iolib: Unlink - ERROR: pathname exceeds MAXPATHNAMELEN\n");
        printf("ERROR: pathname exceeds MAXPATHNAMELEN\n");
        return ERROR;
    }

    YfsMsg *msg = calloc(1, sizeof(YfsMsg));
    msg->type = YFS_UNLINK;
    msg->data1 = currentWorkingDirectory;
    msg->data2 = cwdReuse;
    msg->addr1 = strdup(pathname);

    if (Send((void *)msg, -FILE_SERVER) == ERROR) {
        free(msg);
        free(msg->addr1);
        return ERROR;
    }

    if (msg->type == ERROR) {
        free(msg);
        free(msg->addr1);
        TracePrintf(0, "iolib: Unlink - ERROR: Cannot unlink file.\n");
        printf("ERROR: Cannot unlink file.\n");
        return ERROR;
    }

    // If pathname is the last link to the file,
    // the file should be deleted by freeing its inode
    // * if no problem occurs, return 0
    free(msg);
    free(msg->addr1);
    return 0;
}

/**
 * Creates a symbolic link from the file named by newname to the file named by oldname
 * @param oldname The name of the file to link to, need not currently exist 
 * @param newname The name of the new link
 * @return 0 on success, or ERROR on any error
 */
int SymLink(char *oldname, char *newname) {
    TracePrintf(0, "iolib: SymLink - oldname: %s, newname: %s\n", oldname, newname);
    
    // check length of oldname and newname
    if (strlen(oldname) + 1 > MAXPATHNAMELEN || strlen(newname) + 1 > MAXPATHNAMELEN) {
        TracePrintf(0, "iolib: SymLink - ERROR: pathname exceeds MAXPATHNAMELEN\n");
        printf("ERROR: pathname exceeds MAXPATHNAMELEN\n");
        return ERROR;
    }
    
    // check if oldname is empty
    if (strlen(oldname) == 0) {
        TracePrintf(0, "iolib: SymLink - ERROR: oldname is empty\n");
        printf("ERROR: oldname is empty\n");
        return ERROR;
    }
    
    YfsMsg *msg = calloc(1, sizeof(YfsMsg));
    msg->type = YFS_SYMLINK;
    msg->data1 = currentWorkingDirectory;
    msg->data2 = cwdReuse;
    msg->addr1 = strdup(oldname);
    msg->addr2 = strdup(newname);
    
    if (Send((void*)msg, -FILE_SERVER) == ERROR) {
        free(msg);
        free(msg->addr1);
        free(msg->addr2);
        return ERROR;
    }
    
    if (msg->type == ERROR) {
        free(msg);
        free(msg->addr1);
        free(msg->addr2);
        TracePrintf(0, "iolib: SymLink - ERROR: Cannot create symbolic link.\n");
        printf("ERROR: Cannot create symbolic link.\n");
        return ERROR;
    }
    
    free(msg);
    free(msg->addr1);
    free(msg->addr2);
    return 0;
}

/**
 * Reads the symbolic link named by pathname into the buffer buf
 * @param pathname The name of the symbolic link to read
 * @param buf The buffer where the name the link points to will be stored
 * @param len 
 * @return Min(the length of the name that pathname points to, len), or ERROR on any error
 */
int ReadLink(char *pathname, char *buf, int len) {
    TracePrintf(0, "iolib: ReadLink - %s\n", pathname);
    
    if (pathname == NULL || buf == NULL || len <= 0) {
        TracePrintf(0, "iolib: ReadLink - ERROR: Invalid arguments\n");
        printf("ERROR: Invalid arguments\n");
        return ERROR;
    }
    
    if (strlen(pathname) + 1 > MAXPATHNAMELEN) {
        TracePrintf(0, "iolib: ReadLink - ERROR: pathname exceeds MAXPATHNAMELEN\n");
        printf("ERROR: pathname exceeds MAXPATHNAMELEN\n");
        return ERROR;
    }

    YfsMsg *msg = calloc(1, sizeof(YfsMsg));
    msg->type = YFS_READLINK;
    msg->data1 = currentWorkingDirectory;
    msg->data2 = cwdReuse;
    msg->data3 = len; 
    msg->addr1 = strdup(pathname);
    msg->addr2 = (void*)buf;     
            
    if (Send((void*)msg, -FILE_SERVER) == ERROR) {
        free(msg);
        free(msg->addr1);
        return ERROR;
    }
    
    if (msg->type == ERROR) {
        free(msg);
        free(msg->addr1);
        TracePrintf(0, "iolib: ReadLink - ERROR: Cannot read symbolic link\n");
        printf("ERROR: Cannot read symbolic link\n");
        return ERROR;
    }
    
    int bytesRead = msg->data1;
    free(msg);
    free(msg->addr1);
    
    return bytesRead;
}


/**
 * Creates a new directory named pathname
 * @param pathname The name of the directory to create
 * @return 0 on success, or ERROR on any error
 */
int MkDir(char *pathname) {
    // ERROR if pathname already exists
    TracePrintf(0, "iolib: MkDir - %s\n", pathname);

    if (pathname == NULL || strlen(pathname) + 1 > MAXPATHNAMELEN) {
        TracePrintf(0, "iolib: MkDir - ERROR: Invalid arguments\n");
        printf("ERROR: Invalid arguments\n");
        return ERROR;
    }

    YfsMsg *msg = calloc(1, sizeof(YfsMsg));
    msg->type = YFS_MKDIR;
    msg->data1 = currentWorkingDirectory;
    msg->data2 = cwdReuse;
    msg->addr1 = strdup(pathname);

    if (Send((void*)msg, -FILE_SERVER) == ERROR) {
        free(msg);
        free(msg->addr1);
        return ERROR;
    }

    if (msg->type == ERROR) {
        free(msg);
        free(msg->addr1);
        TracePrintf(0, "iolib: MkDir - ERROR: Cannot create directory.\n");
        printf("ERROR: Cannot create directory.\n");
        return ERROR;
    }

    free(msg);
    free(msg->addr1);
    return 0;
}

/**
 * Deletes the directory named pathname
 * @param pathname The name of the directory to remove
 * @return 0 on success, or ERROR on any error
 */
int RmDir(char *pathname) {
    TracePrintf(0, "iolib: RmDir - %s\n", pathname);

    if (pathname == NULL || strlen(pathname) + 1 > MAXPATHNAMELEN) {
        TracePrintf(0, "iolib: RmDir - ERROR: Invalid pathname or pathname exceeds MAXPATHNAMELEN\n");
        printf("ERROR: Invalid pathname or pathname exceeds MAXPATHNAMELEN\n");
        return ERROR;
    }

    // Error of trying to remove the root directory
    if (strcmp(pathname, "/") == 0) {
        TracePrintf(0, "iolib: RmDir - ERROR: Cannot remove root directory\n");
        printf("ERROR: Cannot remove root directory\n");
        return ERROR;
    }

    // Error of trying to remove individually the '.' or '..' directory from a directory
    char *lastComponent = strrchr(pathname, '/');
    lastComponent = (lastComponent == NULL) ? pathname : lastComponent + 1;
    if (strcmp(lastComponent, ".") == 0 || strcmp(lastComponent, "..") == 0) {
        TracePrintf(0, "iolib: RmDir - ERROR: Cannot remove '.' or '..' directory\n");
        printf("ERROR: Cannot remove '.' or '..' directory\n");
        return ERROR;
    }

    YfsMsg *msg = calloc(1, sizeof(YfsMsg));
    msg->type = YFS_RMDIR;
    msg->data1 = currentWorkingDirectory;
    msg->data2 = cwdReuse;
    msg->addr1 = strdup(pathname);

    if (Send((void*)msg, -FILE_SERVER) == ERROR) {
        free(msg);
        free(msg->addr1);
        return ERROR;
    }
    
    if (msg->type == ERROR) {
        free(msg);
        free(msg->addr1);
        TracePrintf(0, "iolib: RmDir - ERROR: Cannot remove directory (might not be empty)\n");
        printf("ERROR: Cannot remove directory (might not be empty)\n");
        return ERROR;
    }
    
    free(msg);
    free(msg->addr1);
    return 0;
}

/**
 * Changes the current working directory to pathname
 * @param pathname The name of the directory to change to
 * @return 0 on success, or ERROR on any error
 */
int ChDir(char *pathname) {
    TracePrintf(0, "iolib: ChDir - %s\n", pathname);

    if (pathname == NULL || strlen(pathname) + 1 > MAXPATHNAMELEN) {
        TracePrintf(0, "iolib: ChDir - ERROR: Invalid pathname or pathname exceeds MAXPATHNAMELEN\n");
        printf("ERROR: Invalid pathname or pathname exceeds MAXPATHNAMELEN\n");
        return ERROR;
    }

    YfsMsg *msg = calloc(1, sizeof(YfsMsg));
    msg->type = YFS_CHDIR;
    msg->data1 = currentWorkingDirectory;
    msg->data2 = cwdReuse;
    msg->addr1 = strdup(pathname);

    if (Send((void*)msg, -FILE_SERVER) == ERROR) {
        free(msg);
        free(msg->addr1);
        return ERROR;
    }

    if (msg->type == ERROR) {
        free(msg);
        free(msg->addr1);
        TracePrintf(0, "iolib: ChDir - ERROR: Cannot change directory\n");
        printf("ERROR: Cannot change directory\n");
        return ERROR;
    }

    currentWorkingDirectory = msg->data1;
    cwdReuse = msg->data2;
    free(msg);
    free(msg->addr1);
    return 0;
}

/**
 * Gets the status of the file named by pathname in the information structure statbuf.
 * @param pathname The name of the file to get the status of
 * @param statbuf The address of the structure to store the status in
 * @return 0 on success, or ERROR on any error
 */
int Stat(char *pathname, struct Stat *statbuf) {
    TracePrintf(0, "iolib: Stat - pathname: %s, statbuf at: %p\n", pathname, statbuf);
    if (pathname == NULL || statbuf == NULL) {
        TracePrintf(0, "iolib: Stat - ERROR: Invalid argument\n");
        printf("ERROR: Invalid argument\n");
        return ERROR;
    }

    // Send the request to the server
    YfsMsg *msg = calloc(1, sizeof(YfsMsg));
    msg->type = YFS_STAT;
    msg->data1 = currentWorkingDirectory;
    msg->data2 = cwdReuse;
    msg->addr1 = strdup(pathname);

    if (Send((void*)msg, -FILE_SERVER) == ERROR) {
        free(msg);
        free(msg->addr1);
        return ERROR;
    }

    if (msg->type == ERROR) {
        free(msg);
        free(msg->addr1);
        TracePrintf(0, "iolib: Stat - ERROR: Cannot get file status\n");
        printf("ERROR: Cannot get file status\n");
        return ERROR;
    }

    // Copy the status information to the statbuf structure
    statbuf->inum = msg->data1;
    statbuf->type = msg->data2;
    statbuf->size = msg->data3;
    statbuf->nlink = (int)(long)msg->addr1; // Since we don't have data4 slot, we use addr1 to store nlink
    
    free(msg);
    free(msg->addr1);
    return 0;
}

/**
 * Flushes all dirty inode caches and and dirty blocks to the disk
 * @return 0, always
 */
int Sync(void) {
    TracePrintf(0, "iolib: Sync - Flushing all dirty caches to disk\n");
    // Send the request to the server
    YfsMsg *msg = calloc(1, sizeof(YfsMsg));
    msg->type = YFS_SYNC;

    if (Send((void*)msg, -FILE_SERVER) == ERROR) {
        free(msg);
        return ERROR;
    }

    free(msg);
    return 0;
}

/**
 * Shuts down the file server process
 * @return 0, always
 */
int Shutdown(void) {
    TracePrintf(0, "iolib: Shutdown - Shutting down the file server\n");
    // Send the request to the server
    YfsMsg *msg = calloc(1, sizeof(YfsMsg));
    msg->type = YFS_SHUTDOWN;

    if (Send((void*)msg, -FILE_SERVER) == ERROR) {
        free(msg);
        return ERROR;
    }

    free(msg);
    return 0;
}