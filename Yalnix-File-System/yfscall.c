#include <comp421/yalnix.h>
#include <comp421/iolib.h>
#include <comp421/filesystem.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "global.h"
#include "fs/path.h"
#include "cache/cache.h"

void YfsOpen(YfsMsg* msg, int senderPid) {
    TracePrintf(0, "YfsOpen: Received message from process %d\n", senderPid);
    char pathname[MAXPATHNAMELEN + 1];
    if (CopyFrom(senderPid, pathname, msg->addr1, MAXPATHNAMELEN) == ERROR) {
        TracePrintf(0, "YfsOpen: Error copying pathname from process %d\n", senderPid);
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    if (resolveTrailingSlash(pathname)) {
        TracePrintf(0, "YfsOpen - ERROR: path is NULL or 0\n");
        printf("ERROR: Failed to add trailing dot after path\n");
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    if (verifyCwdReuse(pathname, msg->data1, msg->data2) == ERROR) {
        TracePrintf(0, "YfsOpen - ERROR: cwdReuse does not match\n");
        printf("ERROR: Current working directory has changed\n");
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    // Parse the pathname and find the inode number
    int inum = resolvePath(pathname, msg->data1, 0, 1);
    // if the the file is not found, inum will be 0
    // so when it is not found, it should also return ERROR
    if (inum == ERROR || inum == 0) {
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
    	return;
    }

    struct InodeCacheEntry* inodeEntry = GetInodeFromCache(inum);
    if (inodeEntry == NULL) {
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

	msg->data1 = inum;
    msg->data2 = inodeEntry->inodeInfo->reuse;
	Reply((void*)msg, senderPid);
    return;
}

void YfsCreate(YfsMsg* msg, int senderPid) {
    TracePrintf(0, "YfsCreate: Received message from process %d\n", senderPid);
    char pathname[MAXPATHNAMELEN + 1];
    if (CopyFrom(senderPid, pathname, msg->addr1, MAXPATHNAMELEN) == ERROR) {
        TracePrintf(0, "YfsCreate: Error copying pathname from process %d\n", senderPid);
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    if (resolveTrailingSlash(pathname)) {
        TracePrintf(0, "YfsCreate - ERROR: path is NULL or 0\n");
        printf("ERROR: Failed to add trailing dot after path\n");
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    if (verifyCwdReuse(pathname, msg->data1, msg->data2) == ERROR) {
        TracePrintf(0, "YfsCreate - ERROR: cwdReuse does not match\n");
        printf("ERROR: Current working directory has changed\n");
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    // Parse the pathname and find the inum of the parent directory and the target file to be created
    // Make sure that all directories in the path exist, if not return ERROR

    // Remove "/." at the end
    int len = strlen(pathname);
    if (len >= 2 && pathname[len - 1] == '.' && pathname[len - 2] == '/') {
        pathname[len - 2] = '\0';
    }

    char filename[DIRNAMELEN];
    int parentInum = CreateFindParent(pathname, msg->data1, filename);
    TracePrintf(0, "YfsCreate: Parent inode number is %d\n", parentInum);
    TracePrintf(0, "YfsCreate: Target file name is %s\n", filename);

    if (strlen(filename) == 0) {
        TracePrintf(0, "YfsCreate: Filename is empty\n");
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    if (parentInum == ERROR) {
        TracePrintf(0, "YfsCreate: Error getting parent inode number from process %d\n", senderPid);
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    if (strlen(filename) == 0) {
        TracePrintf(0, "YfsCreate: Filename is empty\n");
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    // Check if the file already exists
    struct InodeCacheEntry* parentInodeEntry = GetInodeFromCache(parentInum);
    if( parentInodeEntry == NULL) {
        TracePrintf(0, "YfsCreate: Error getting parent inode entry from process %d\n", senderPid);
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    TracePrintf(0, "YfsCreate: Parent inode type is %d\n", parentInodeEntry->inodeInfo->type);
    if (parentInodeEntry->inodeInfo->type != INODE_DIRECTORY) {
        TracePrintf(0, "YfsCreate: Parent inode is not a directory\n");
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    int fileInum = GetInumByComponentName(parentInodeEntry, filename);

    if (fileInum == ERROR) {
        TracePrintf(0, "YfsCreate: Error getting file inode number\n");
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    // If the file already exists, truncate it to 0
    if (fileInum != 0) {
        if (strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0 || strcmp(filename, "/") == 0) {
            TracePrintf(0, "YfsCreate: Cannot create . or .. or root\n");
            msg->type = ERROR;
            Reply((void*)msg, senderPid);
            return;
        }

        TracePrintf(0, "YfsCreate: File already exists, truncating to 0\n");
        struct InodeCacheEntry* inodeEntry = GetInodeFromCache(fileInum);
        if (inodeEntry == NULL) {
            msg->type = ERROR;
            Reply((void*)msg, senderPid);
            return;
        }

        // Truncate the file size to 0
        TruncateFile(inodeEntry);
        msg->data1 = fileInum;
        msg->data2 = inodeEntry->inodeInfo->reuse;
        Reply((void*)msg, senderPid);
        return;
    }

    // Otherwise, create a new file (fileInum == 0)
    fileInum = AllocateInode();
    if (fileInum == ERROR) {
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    // Add the new file to the parent directory entry
    if (AddDirEntry(fileInum, filename, parentInodeEntry) == ERROR) {
        TracePrintf(0, "YfsCreate: Error adding directory entry\n");
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    // Add the new file to inode
    struct InodeCacheEntry* inodeEntry = GetInodeFromCache(fileInum);
    struct inode* inodeInfo = inodeEntry->inodeInfo;
    inodeInfo->type = INODE_REGULAR;
    inodeInfo->nlink = 1;
    inodeInfo->size = 0;
    // Since we allocate a new inode, we increment the reused count
    inodeInfo->reuse += 1;
    inodeInfo->indirect = 0;
    for (int i = 0; i < NUM_DIRECT; i++) {
        inodeInfo->direct[i] = 0;
    }
    inodeEntry->isDirty = 1;
    
    msg->data1 = fileInum;
    msg->data2 = inodeInfo->reuse;
    Reply((void*)msg, senderPid);
    return;
}

void YfsRead(YfsMsg* msg, int senderPid) {
    TracePrintf(0, "YfsRead: Received message from process %d\n", senderPid);
    int inodeNumber = msg->data1;
    int offset = msg->data2;            // Offset to start reading from
    int size = msg->data3;              // Number of bytes to read
    int reuse = (int)(long)msg->addr2;
    void* buf = msg->addr1;

    TracePrintf(0, "YfsRead: inodeNumber %d, offset %d, size %d, reuse %d, buf %p\n", inodeNumber, offset, size, reuse, buf);
    struct InodeCacheEntry* inodeEntry = GetInodeFromCache(inodeNumber);
    if (inodeEntry == NULL) {
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    struct inode* inodeInfo = inodeEntry->inodeInfo;
    if (inodeInfo->reuse != reuse) {
        TracePrintf(0, "YfsRead: inode reuse count mismatch, expected %d, got %d\n", reuse, inodeInfo->reuse);
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    // It is NOT an ERROR to read from a directory
    if (inodeInfo->type == INODE_FREE) {
        TracePrintf(0, "YfsRead: inode is a free node\n");
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    // If already at the end of file, return 0
    if (offset >= inodeInfo->size) {
        TracePrintf(0, "YfsRead: Offset %d at EOF\n", offset);
        msg->data1 = 0;
        Reply((void*)msg, senderPid);
        return;
    }

    // Adjust the size if it exceeds the file size
    if (offset + size > inodeInfo->size) {
        size = inodeInfo->size - offset;
    }

    int bytesRead = 0;
    char* tempBuf = (char*)malloc(size);

    // The direct or indirect blocks to start reading from
    int startBlock = offset / BLOCKSIZE;
    int endBlock = (offset + size - 1) / BLOCKSIZE;
    TracePrintf(0, "YfsRead: startBlock %d, endBlock %d\n", startBlock, endBlock);

    for (int i = startBlock; i <= endBlock; i++) {
        // Get the data block number
        int blockNum;
        if (i < NUM_DIRECT) {
            blockNum = inodeInfo->direct[i];
        } 
        else {
            blockNum = GetDataBlockNumberFromIndirectBlock(inodeInfo->indirect, i - NUM_DIRECT);
        }

        // Get the data block from the cache
        struct BlockCacheEntry* blockEntry = GetBlockFromCache(blockNum);
        if (blockEntry == NULL) {
            msg->type = ERROR;
            Reply((void*)msg, senderPid);
            return;
        }
        char* blockData = (char*)blockEntry->data;

        if (i == startBlock && i == endBlock) {
            // Read from the same block, and this is the only block to read from
            int offsetInBlock = offset % BLOCKSIZE;
            memcpy(tempBuf, blockData + offsetInBlock, size);
            bytesRead += size;
        } 
        else if (i == startBlock) {
            // Read from the start block
            int offsetInBlock = offset % BLOCKSIZE;
            int bytesToRead = BLOCKSIZE - offsetInBlock;
            memcpy(tempBuf, blockData + offsetInBlock, bytesToRead);
            bytesRead += bytesToRead;
        } 
        else if (i == endBlock) {
            // Read from the end block
            int bytesToRead = size - bytesRead;
            memcpy(tempBuf + bytesRead, blockData, bytesToRead);
            bytesRead += bytesToRead;
        } 
        else {
            // Read from a full block
            memcpy(tempBuf + bytesRead, blockData, BLOCKSIZE);
            bytesRead += BLOCKSIZE;
        }
    }

    TracePrintf(0, "YfsRead: Read %d bytes from file (inode %d)\n", bytesRead, inodeNumber);
    // Copy the data to the buffer
    if (CopyTo(senderPid, buf, tempBuf, bytesRead) == ERROR) {
        TracePrintf(0, "YfsRead: Error copying data to process %d\n", senderPid);
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        free(tempBuf);
        return;
    }

    free(tempBuf);
    msg->data1 = bytesRead;
    Reply((void*)msg, senderPid);
    return;
}

void YfsWrite(YfsMsg* msg, int senderPid) {
    TracePrintf(0, "YfsWrite: Received message from process %d\n", senderPid);
    int inodeNumber = msg->data1;
    int offset = msg->data2;            // Offset to start writing to
    int size = msg->data3;              // Number of bytes to write
    int reuse = (int)(long)msg->addr2;
    void* buf = msg->addr1;

    TracePrintf(0, "YfsWrite: inodeNumber %d, offset %d, size %d, reuse %d, buf %p\n", inodeNumber, offset, size, reuse, buf);
    struct InodeCacheEntry* inodeEntry = GetInodeFromCache(inodeNumber);
    if (inodeEntry == NULL) {
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    struct inode* inodeInfo = inodeEntry->inodeInfo;
    if (inodeInfo->reuse != reuse) {
        TracePrintf(0, "YfsWrite: inode reuse count mismatch, expected %d, got %d\n", reuse, inodeInfo->reuse);
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    // ERROR if attempting to write to a directory
    if (inodeInfo->type != INODE_REGULAR) {
        TracePrintf(0, "YfsRead: inode is a free node\n");
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    // If the offset is beyond the maximum file size, return ERROR
    if (offset >= MAX_FILE_SIZE) {
        TracePrintf(0, "YfsWrite: Offset %d exceeds maximum file size\n", offset);
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    // If attempting to write more than MAX_FILE_SIZE, truncate the size
    if (offset + size > MAX_FILE_SIZE) {
        size = MAX_FILE_SIZE - offset;
    }

    // Allocate new data blocks up to the (offset + size - 1)
    int fileEndBlock = (inodeInfo->size == 0) ? -1 : (inodeInfo->size - 1) / BLOCKSIZE;
    int writeEndBlock = (offset + size - 1) / BLOCKSIZE;
    int blocksToAllocate = writeEndBlock - fileEndBlock;
    for (int i = 0; i < blocksToAllocate; i++) {
        if (AllocateBlockInInode(inodeEntry) == ERROR) {
            TracePrintf(0, "YfsWrite: Not enough block to allocate new data block\n");
            msg->type = ERROR;
            Reply((void*)msg, senderPid);
            return;
        }
    }

    // For holes between the current file size and the offset - 1, fill with zeros
    // Since when we allocate a new block, we zero out its content, here we don't need to refill the holes

    // Write the data to the file
    int bytesWrite = 0;

    // The direct or indirect blocks to start reading from
    int startBlock = offset / BLOCKSIZE;
    int endBlock = (offset + size - 1) / BLOCKSIZE;
    TracePrintf(0, "YfsWrite: startBlock %d, endBlock %d\n", startBlock, endBlock);
    for (int i = startBlock; i <= endBlock; i++) {
        // Get the data block number
        int blockNum;
        if (i < NUM_DIRECT) {
            blockNum = inodeInfo->direct[i];
        } 
        else {
            blockNum = GetDataBlockNumberFromIndirectBlock(inodeInfo->indirect, i - NUM_DIRECT);
        }

        // Get the data block from the cache
        struct BlockCacheEntry* blockEntry = GetBlockFromCache(blockNum);
        if (blockEntry == NULL) {
            msg->type = ERROR;
            Reply((void*)msg, senderPid);
            return;
        }
        char* blockData = (char*)blockEntry->data;

        if (i == startBlock && i == endBlock) {
            // Read from the same block, and this is the only block to read from
            int offsetInBlock = offset % BLOCKSIZE;
            CopyFrom(senderPid, blockData + offsetInBlock, buf, size);
            bytesWrite += size;
            blockEntry->isDirty = 1;
        } 
        else if (i == startBlock) {
            // Read from the start block
            int offsetInBlock = offset % BLOCKSIZE;
            int bytesToWrite = BLOCKSIZE - offsetInBlock;
            CopyFrom(senderPid, blockData + offsetInBlock, buf + bytesWrite, bytesToWrite);
            bytesWrite += bytesToWrite;
            blockEntry->isDirty = 1;
        } 
        else if (i == endBlock) {
            // Read from the end block
            int bytesToWrite = size - bytesWrite;
            CopyFrom(senderPid, blockData, buf + bytesWrite, bytesToWrite);
            bytesWrite += bytesToWrite;
            blockEntry->isDirty = 1;
        } 
        else {
            // Read from a full block
            CopyFrom(senderPid, blockData, buf + bytesWrite, BLOCKSIZE);
            bytesWrite += BLOCKSIZE;
            blockEntry->isDirty = 1;
        }
    }

    TracePrintf(0, "YfsWrite: Wrote %d bytes to file (inode %d)\n", bytesWrite, inodeNumber);
    inodeInfo->size = offset + bytesWrite;
    inodeEntry->isDirty = 1;
    msg->data1 = bytesWrite;
    Reply((void*)msg, senderPid);
    return;
}

void YfsSeek(YfsMsg* msg, int senderPid) {
    TracePrintf(0, "YfsSeek: Received message from process %d\n", senderPid);
    
    int inodeNumber = msg->data1;
    int offset = msg->data2;
    int whence = msg->data3;
    int currentOffset = (int)(long)msg->addr1;
    int reuse = (int)(long)msg->addr2;

    TracePrintf(0, "YfsSeek: inodeNumber %d, offset %d, whence %d, currentOffset %d, reuse %d\n", inodeNumber, offset, whence, currentOffset, reuse);
    struct InodeCacheEntry* inodeEntry = GetInodeFromCache(inodeNumber);
    if (inodeEntry == NULL) {
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    struct inode* inodeInfo = inodeEntry->inodeInfo;
    if (inodeInfo->reuse != reuse) {
        TracePrintf(0, "YfsSeek: inode reuse count mismatch, expected %d, got %d\n", reuse, inodeInfo->reuse);
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    // Can seek beyond the end of the file,
    // but the file size is not changed, until later written at this offest and the holes are \0
    // Calculate the new offset based on whence
    int targetOffset;
    switch (whence) {
        case SEEK_SET:
            targetOffset = offset;
            break;
        case SEEK_CUR:
            targetOffset =  currentOffset + offset;
            break;
        case SEEK_END:
            targetOffset = inodeInfo->size + offset;
            break;
        default:
            TracePrintf(0, "iolib: Seek - ERROR: Invalid whence\n");
            printf("ERROR: Invalid whence\n");
            msg->type = ERROR;
            Reply((void*)msg, senderPid);
            return;
    }

    // ERROR if the targetOffset is smaller than 0
    if (targetOffset < 0) {
        TracePrintf(0, "YfsSeek: targetOffset %d is less than 0\n", targetOffset);
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        TracePrintf(0, "iolib: Seek - ERROR: Invalid offset\n");
        return;
    }

    TracePrintf(0, "YfsSeek: targetOffset of file (inode %d) is %d\n", inodeNumber, targetOffset);
    msg->data1 = targetOffset;
    Reply((void*)msg, senderPid);
    return;
}

/**
 * Creates a hard link from the file named by oldname to the file named by newname
 * @param oldname The name of the file to link to, must not be a directory
 * @param newname The name of the new link
 * @return 0 on success, or ERROR on any error
 */
void YfsLink(YfsMsg* msg, int senderPid) {
    // The files oldname and newname need not be in the same directory. 
    // The file oldname must not be a directory. 
    // It is an error if the file newname already exists.

    TracePrintf(0, "YfsLink: Received message from process %d\n", senderPid);

    char oldname[MAXPATHNAMELEN + 1];
    char newname[MAXPATHNAMELEN + 1];

    // getting old pathname from client side
    if (CopyFrom(senderPid, oldname, msg->addr1, MAXPATHNAMELEN) == ERROR) {
        TracePrintf(0, "YfsLink: Error copying oldname from process %d\n", senderPid);
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    // getting new pathname from client side
    if (CopyFrom(senderPid, newname, msg->addr2, MAXPATHNAMELEN) == ERROR) {
        TracePrintf(0, "YfsLink: Error copying newname from process %d\n", senderPid);
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    if (resolveTrailingSlash(oldname)) {
        TracePrintf(0, "YfsLink - ERROR: path is NULL or 0\n");
        printf("ERROR: Failed to add trailing dot after path\n");
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    if (verifyCwdReuse(oldname, msg->data1, msg->data2) == ERROR) {
        TracePrintf(0, "YfsLink - ERROR: cwdReuse does not match\n");
        printf("ERROR: Current working directory has changed\n");
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    if (resolveTrailingSlash(newname)) {
        TracePrintf(0, "YfsLink - ERROR: path is NULL or 0\n");
        printf("ERROR: Failed to add trailing dot after path\n");
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    if (verifyCwdReuse(newname, msg->data1, msg->data2) == ERROR) {
        TracePrintf(0, "YfsLink - ERROR: cwdReuse does not match\n");
        printf("ERROR: Current working directory has changed\n");
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    // find the according inum of oldname
    int oldInum = resolvePath(oldname, msg->data1, 0, 0);
    if (oldInum == ERROR) {
        TracePrintf(0, "YfsLink: Error resolving oldname %s\n", oldname);
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    // get inode info of oldinode
    struct InodeCacheEntry* oldInodeEntry = GetInodeFromCache(oldInum);
    if (oldInodeEntry == NULL) {
        TracePrintf(0, "YfsLink: Error getting inode entry for oldname %s\n", oldname);
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    // check if the oldInode is a directory
    if (oldInodeEntry->inodeInfo->type == INODE_DIRECTORY) {
        TracePrintf(0, "YfsLink: Error: oldname %s is a directory\n", oldname);
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    // get the parent inode number of newname
    int newParentInum = GetParentInum(msg->data1, newname);
    if (newParentInum == ERROR) {
        TracePrintf(0, "YfsLink: Error getting parent inode number for newname %s\n", newname);
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }
    
    // get the filename of newname
    char* newFilename = getFilename(newname);

    // check if newname's parent inode is a directory and exist
    struct InodeCacheEntry* newnodeParentInodeEntry = GetInodeFromCache(newParentInum);
    if (newnodeParentInodeEntry->inodeInfo->type != INODE_DIRECTORY || newnodeParentInodeEntry == NULL) {
        TracePrintf(0, "YfsLink: Error: newname %s is not a directory\n", newname);
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    struct InodeCacheEntry* oldnodeParentInodeEntry = GetInodeFromCache(oldInodeEntry->inodeNumber);
    if (oldnodeParentInodeEntry == NULL) {
        TracePrintf(0, "YfsLink: Error: oldname %s parent inode entry not found\n", oldname);
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    // check if newname already exists
    int existingInum = GetInumByComponentName(newnodeParentInodeEntry, newFilename);

    if (existingInum == ERROR) {
        TracePrintf(0, "YfsLink: Error getting inode number for newname %s\n", newname);
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    if (existingInum != 0) {
        TracePrintf(0, "YfsLink: Error: newname %s already exists\n", newname);
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    // add a newFilename link to oldInum in newnodeParentInodeEntry
    if (AddDirEntry(oldInum, newFilename, newnodeParentInodeEntry) == ERROR) {
        TracePrintf(0, "YfsLink: Error adding directory entry for newname %s\n", newname);
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    oldInodeEntry->inodeInfo->nlink += 1;
    oldInodeEntry->isDirty = 1;

    msg->type = 0;
    Reply((void*)msg, senderPid);
    return;
}

void YfsUnlink(YfsMsg* msg, int senderPid) {
    TracePrintf(0, "YfsUnlink: Received message from process %d\n", senderPid);

    // get pathname from client side
    char pathname[MAXPATHNAMELEN + 1];
    if (CopyFrom(senderPid, pathname, msg->addr1, MAXPATHNAMELEN) == ERROR) {
        TracePrintf(0, "YfsUnlink: Error copying pathname from process %d\n", senderPid);
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    if (resolveTrailingSlash(pathname)) {
        TracePrintf(0, "YfsUnlink - ERROR: path is NULL or 0\n");
        printf("ERROR: Failed to add trailing dot after path\n");
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    if (verifyCwdReuse(pathname, msg->data1, msg->data2) == ERROR) {
        TracePrintf(0, "YfsUnlink - ERROR: cwdReuse does not match\n");
        printf("ERROR: Current working directory has changed\n");
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    // Parse the parent directory and get the inode number
    int parentInum = GetParentInum(msg->data1, pathname);
    if (parentInum == ERROR) {
        TracePrintf(0, "YfsUnlink: Error getting parent inode number from process %d\n", senderPid);
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    // get the filename from pathname
    char* filename = getFilename(pathname);
    struct InodeCacheEntry* parentInodeEntry = GetInodeFromCache(parentInum);
    if (parentInodeEntry->inodeInfo->type != INODE_DIRECTORY) {
        TracePrintf(0, "YfsUnlink: Parent inode is not a directory\n");
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    // check if the file exists
    int fileInum = GetInumByComponentName(parentInodeEntry, filename);
    if (fileInum == ERROR || fileInum == 0) {
        TracePrintf(0, "YfsUnlink: File not found\n");
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    // get the inode entry of the file
    struct InodeCacheEntry* fileInodeEntry = GetInodeFromCache(fileInum);
    if (fileInodeEntry == NULL) {
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    // Check if the file is a directory, ERROR if pathname is a directory
    if (fileInodeEntry->inodeInfo->type == INODE_DIRECTORY) {
        TracePrintf(0, "YfsUnlink: Cannot unlink a directory\n");
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    // Remove the directory entry from the parent inode
    int status = RemoveEntryFromDir(fileInum, filename, parentInodeEntry);
    if (status == ERROR) {
        TracePrintf(0, "YfsUnlink: Error removing directory entry\n");
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    fileInodeEntry->inodeInfo->nlink -= 1;
    fileInodeEntry->isDirty = 1;

    // If the file has no more links, free the inode
    if (fileInodeEntry->inodeInfo->nlink == 0) {
        TracePrintf(0, "YfsUnlink: File has no more links, freeing inode\n");

        TruncateFile(fileInodeEntry);

        fileInodeEntry->inodeInfo->type = INODE_FREE;
        fileInodeEntry->isDirty = 1;

        // Add the inode to the free inode list
        freeInodesList[fileInum] = 1;
        freeInodesCount += 1;
        TracePrintf(0, "YfsUnlink: Added inode %d to free inodes list\n", fileInum);
    }

    msg->type = 0;
    Reply((void*)msg, senderPid);
    return;
}

void YfsSymLink(YfsMsg* msg, int senderPid) {
    TracePrintf(0, "YfsSymLink: Received message from process %d\n", senderPid);

    char oldname[MAXPATHNAMELEN + 1];
    char newname[MAXPATHNAMELEN + 1];

    // get oldname from client side
    if (CopyFrom(senderPid, oldname, msg->addr1, MAXPATHNAMELEN) == ERROR) {
        TracePrintf(0, "YfsSymLink: Error copying oldname from process %d\n", senderPid);
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    // get newname from client side
    if (CopyFrom(senderPid, newname, msg->addr2, MAXPATHNAMELEN) == ERROR) {
        TracePrintf(0, "YfsSymLink: Error copying newname from process %d\n", senderPid);
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    if (resolveTrailingSlash(newname)) {
        TracePrintf(0, "YfsSymlink - ERROR: path is NULL or 0\n");
        printf("ERROR: Failed to add trailing dot after path\n");
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    if (verifyCwdReuse(newname, msg->data1, msg->data2) == ERROR) {
        TracePrintf(0, "YfsSymlink - ERROR: cwdReuse does not match\n");
        printf("ERROR: Current working directory has changed\n");
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    // checking oldname can't be empty
    if (strlen(oldname) == 0) {
        TracePrintf(0, "YfsSymLink: Error: oldname is empty\n");
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    // getting newname's parent inode number
    int newParentInum = GetParentInum(msg->data1, newname);
    if (newParentInum == ERROR) {
        TracePrintf(0, "YfsSymLink: Error getting parent inode number for newname %s\n", newname);
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    // getting newname's filename
    char* newFilename = getFilename(newname);

    // checking newname's parent inode is a directory and exist
    struct InodeCacheEntry* parentInodeEntry = GetInodeFromCache(newParentInum);
    if (parentInodeEntry == NULL || parentInodeEntry->inodeInfo->type != INODE_DIRECTORY) {
        TracePrintf(0, "YfsSymLink: Parent inode is not a valid directory\n");
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    // checking if newname already exists
    int existingInum = GetInumByComponentName(parentInodeEntry, newFilename);

    if (existingInum == ERROR) {
        TracePrintf(0, "YfsSymLink: Error getting inode number for newname %s\n", newname);
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    if (existingInum != 0) {
        TracePrintf(0, "YfsSymLink: Error: newname %s already exists\n", newname);
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    // allocate a new inode for the symlink
    int symlinkInum = AllocateInode();
    if (symlinkInum == ERROR) {
        TracePrintf(0, "YfsSymLink: Error allocating inode\n");
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    // allocate a new block for the symlink
    struct InodeCacheEntry* symlinkInodeEntry = GetInodeFromCache(symlinkInum);
    struct inode* symlinkInode = symlinkInodeEntry->inodeInfo;

    // initialize the inode
    symlinkInode->type = INODE_SYMLINK;
    symlinkInode->nlink = 1;
    symlinkInode->size = strlen(oldname);
    symlinkInode->reuse += 1; 
    symlinkInode->indirect = 0;
    
    // clear all direct blocks
    for (int i = 0; i < NUM_DIRECT; i++) {
        symlinkInode->direct[i] = 0;
    }

    // allocate a block to store the symlink target
    int dataBlockNum = AllocateBlock();
    if (dataBlockNum == ERROR) {
        TracePrintf(0, "YfsSymLink: Error allocating data block, freeing symlinkInode\n");
        
        symlinkInode->type = INODE_FREE;
        symlinkInodeEntry->isDirty = 1;
        freeInodesList[symlinkInum] = 1;
        freeInodesCount += 1;
        
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    // set the first direct block for storing the symlink target
    symlinkInode->direct[0] = dataBlockNum;
    symlinkInodeEntry->isDirty = 1;

    // save the oldname in the allocated block
    // and mark the block as dirty
    struct BlockCacheEntry* blockEntry = GetBlockFromCache(dataBlockNum);
    memcpy(blockEntry->data, oldname, strlen(oldname));
    blockEntry->isDirty = 1;

    // add the symlink to the parent directory
    if (AddDirEntry(symlinkInum, newFilename, parentInodeEntry) == ERROR) {
        TracePrintf(0, "YfsSymLink: Error adding directory entry\n");
        
        // if adddirentry failed, free the inode block
        symlinkInode->type = INODE_FREE;
        symlinkInodeEntry->isDirty = 1;
        freeInodesList[symlinkInum] = 1;
        freeInodesCount += 1;
        
        // release the allocated data block
        // clear the direct block
        symlinkInode->direct[0] = 0;
        freeInodesList[dataBlockNum] = 1;
        freeBlocksCount += 1;
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    int checkInum = GetInumByComponentName(parentInodeEntry, newFilename);
    TracePrintf(0, "YfsSymLink: After adding symlink, checking if it exists: %d (should be %d)\n", checkInum, symlinkInum);

    // return 0, means success
    msg->type = 0; 
    Reply((void*)msg, senderPid);
    return;
}

void YfsReadLink(YfsMsg* msg, int senderPid) {
    TracePrintf(0, "YfsReadLink: Received message from process %d\n", senderPid);
    
    char pathname[MAXPATHNAMELEN + 1];
    if (CopyFrom(senderPid, pathname, msg->addr1, MAXPATHNAMELEN) == ERROR) {
        TracePrintf(0, "YfsReadLink: Error copying pathname from process %d\n", senderPid);
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    if (resolveTrailingSlash(pathname)) {
        TracePrintf(0, "YfsReadLink - ERROR: path is NULL or 0\n");
        printf("ERROR: Failed to add trailing dot after path\n");
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    if (verifyCwdReuse(pathname, msg->data1, msg->data2) == ERROR) {
        TracePrintf(0, "YfsReadLink - ERROR: cwdReuse does not match\n");
        printf("ERROR: Current working directory has changed\n");
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    // resolve the path to find the inode number, but do not resolve the last symlink
    int inum = resolvePath(pathname, msg->data1, 0, 0);
    if (inum == ERROR || inum == 0) {
        TracePrintf(0, "YfsReadLink: Error resolving pathname %s\n", pathname);
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    // get the inode symlink is connected to
    struct InodeCacheEntry* inodeEntry = GetInodeFromCache(inum);
    if (inodeEntry == NULL) {
        TracePrintf(0, "YfsReadLink: Error getting inode entry for pathname %s\n", pathname);
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }
    
    // check if the inode is a symlink
    if (inodeEntry->inodeInfo->type != INODE_SYMLINK) {
        TracePrintf(0, "YfsReadLink: Error: %s is not a symbolic link\n", pathname);
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    int dataBlockNum = inodeEntry->inodeInfo->direct[0];
    if (dataBlockNum == 0) {
        TracePrintf(0, "YfsReadLink: Error: symbolic link %s has no data block\n", pathname);
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    struct BlockCacheEntry* blockEntry = GetBlockFromCache(dataBlockNum);
    if (blockEntry == NULL) {
        TracePrintf(0, "YfsReadLink: Error getting data block for symbolic link %s\n", pathname);
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    int len = msg->data3;
    int linkSize = inodeEntry->inodeInfo->size;
    int bytesToCopy = (linkSize < len) ? linkSize : len;

    if (CopyTo(senderPid, msg->addr2, blockEntry->data, bytesToCopy) == ERROR) {
        TracePrintf(0, "YfsReadLink: Error copying link target to client\n");
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    msg->data1 = bytesToCopy;
    Reply((void*)msg, senderPid);
    return;
}

void YfsMkDir(YfsMsg* msg, int senderPid) {
    TracePrintf(0, "YfsMkDir: Received message from process %d\n", senderPid);

    char pathname[MAXPATHNAMELEN + 1];
    if (CopyFrom(senderPid, pathname, msg->addr1, MAXPATHNAMELEN) == ERROR) {
        TracePrintf(0, "YfsMkDir: Error copying pathname from process %d\n", senderPid);
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    if (resolveTrailingSlash(pathname)) {
        TracePrintf(0, "YfsMkDir - ERROR: path is NULL or 0\n");
        printf("ERROR: Failed to add trailing dot after path\n");
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    if (verifyCwdReuse(pathname, msg->data1, msg->data2) == ERROR) {
        TracePrintf(0, "YfsMkDir - ERROR: cwdReuse does not match\n");
        printf("ERROR: Current working directory has changed\n");
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    int parentInum = GetParentInum(msg->data1, pathname);
    if (parentInum == ERROR) {
        TracePrintf(0, "YfsMkDir: Error getting parent inode number from process %d\n", senderPid);
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    // last component before slash
    char* dirName = getFilename(pathname);

    struct InodeCacheEntry* parentInodeEntry = GetInodeFromCache(parentInum);
    if (parentInodeEntry == NULL || parentInodeEntry->inodeInfo->type != INODE_DIRECTORY) {
        TracePrintf(0, "YfsMkDir: Parent inode is not a directory\n");
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    int existingInum = GetInumByComponentName(parentInodeEntry, dirName);

    if (existingInum == ERROR) {
        TracePrintf(0, "YfsMkDir: Error getting existing inode number for directory %s\n", dirName);
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    if (existingInum != 0) {
        TracePrintf(0, "YfsMkDir: Directory already exists\n");
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    int newDirInum = AllocateInode();
    if (newDirInum == ERROR) {
        TracePrintf(0, "YfsMkDir: Error allocating inode\n");
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    struct InodeCacheEntry* newDirInodeEntry = GetInodeFromCache(newDirInum);
    struct inode* newDirInode = newDirInodeEntry->inodeInfo;

    newDirInode->type = INODE_DIRECTORY;
    newDirInode->nlink = 2; // "." and ".."
    newDirInode->size = 2 * sizeof(struct dir_entry);
    newDirInode->reuse += 1;
    newDirInode->indirect = 0;

    for (int i = 0; i < NUM_DIRECT; i++) {
        newDirInode->direct[i] = 0;
    }
    newDirInodeEntry->isDirty = 1;

    int dataBlockNum = AllocateBlock();
    if (dataBlockNum == ERROR) {
        TracePrintf(0, "YfsMkDir: Error allocating data block\n");

        // Free the inode if block allocation fails
        newDirInode->type = INODE_FREE;
        newDirInodeEntry->isDirty = 1;
        freeInodesList[newDirInum] = 1;
        freeInodesCount += 1;

        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    newDirInode->direct[0] = dataBlockNum;

    if (AddDirEntry(newDirInum, dirName, parentInodeEntry) == ERROR) {
        TracePrintf(0, "YfsMkDir: Error adding directory entry\n");

        // Free the inode and data block if adding directory entry fails
        newDirInode->type = INODE_FREE;
        newDirInode->direct[0] = 0;
        newDirInodeEntry->isDirty = 1;
        freeInodesList[newDirInum] = 1;
        freeInodesCount += 1;
        freeBlocksList[dataBlockNum] = 1;
        freeBlocksCount += 1;

        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    parentInodeEntry->inodeInfo->nlink += 1;
    parentInodeEntry->isDirty = 1;

    // Add "." and ".." entries to the new directory
    struct BlockCacheEntry* blockEntry = GetBlockFromCache(dataBlockNum);
    struct dir_entry *dotEntry = (struct dir_entry *)blockEntry->data;
    struct dir_entry *dotdotEntry = (struct dir_entry *)(blockEntry->data + sizeof(struct dir_entry));


    memset(dotEntry->name, 0, DIRNAMELEN);
    memcpy(dotEntry->name, ".", 1);

    memset(dotdotEntry->name, 0, DIRNAMELEN);
    memcpy(dotdotEntry->name, "..", 2);

    dotEntry->inum = newDirInum;
    dotdotEntry->inum = parentInum;

    blockEntry->isDirty = 1;

    msg->type = 0;
    Reply((void*)msg, senderPid);
    return;
}

void YfsRmDir(YfsMsg* msg, int senderPid) {
    TracePrintf(0, "YfsRmDir: Received message from process %d\n", senderPid);

    char pathname[MAXPATHNAMELEN + 1];
    if (CopyFrom(senderPid, pathname, msg->addr1, MAXPATHNAMELEN) == ERROR) {
        TracePrintf(0, "YfsRmDir: Error copying pathname from process %d\n", senderPid);
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    if (resolveTrailingSlash(pathname)) {
        TracePrintf(0, "YfsRmDir - ERROR: path is NULL or 0\n");
        printf("ERROR: Failed to add trailing dot after path\n");
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    if (verifyCwdReuse(pathname, msg->data1, msg->data2) == ERROR) {
        TracePrintf(0, "YfsRmDir - ERROR: cwdReuse does not match\n");
        printf("ERROR: Current working directory has changed\n");
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    int parentInum = GetParentInum(msg->data1, pathname);
    if (parentInum == ERROR) {
        TracePrintf(0, "YfsRmDir: Error getting parent inode number from process %d\n", senderPid);
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }
    TracePrintf(0, "YfsRmDir: Parent inode number is %d\n", parentInum);
    struct InodeCacheEntry* parentInodeEntry = GetInodeFromCache(parentInum);
    if (parentInodeEntry == NULL || parentInodeEntry->inodeInfo->type != INODE_DIRECTORY) {
        TracePrintf(0, "YfsRmDir: Parent inode is not a directory\n");
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    char* dirName = getFilename(pathname);
    
    if (strcmp(dirName, "/") == 0) {
        TracePrintf(0, "YfsRmDir: Cannot remove root directory\n");
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    if (strcmp(dirName, ".") == 0 || strcmp(dirName, "..") == 0) {
        TracePrintf(0, "YfsRmDir: Cannot remove '.' or '..'\n");
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }
    // Print out parent name and directory name
    TracePrintf(0, "YfsRmDir: Attempting to remove directory '%s' from parent with inum %d\n", 
                dirName, parentInum);
    int dirInum = GetInumByComponentName(parentInodeEntry, dirName);
    
    if (dirInum == ERROR) {
        TracePrintf(0, "YfsRmDir: Error getting directory inode number for %s\n", dirName);
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    if (dirInum == 0) {
        TracePrintf(0, "YfsRmDir: Directory not found\n");
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    struct InodeCacheEntry* dirInodeEntry = GetInodeFromCache(dirInum);
    if (dirInodeEntry == NULL || dirInodeEntry->inodeInfo->type != INODE_DIRECTORY) {
        TracePrintf(0, "YfsRmDir: Target delete file is not a directory\n");
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    // Check if there are entries other than . and .. that are valid, if yes, return ERROR 
    // The directory must contain no directory entries other than the “.” and “..” entries, 
    // and possibly some free entries (spec p.18)
    struct inode* dirInode = dirInodeEntry->inodeInfo;
    int totalDirEntries = dirInode->size / sizeof(struct dir_entry);

    int i;
    // Traverse the directory entries
    for (i = 0; i < totalDirEntries; i++) {
        // Get the block number of this directory entry
        int index = i / DIRENTRY_PER_BLOCK;
        if (index >= NUM_DIRECT && dirInode->indirect == 0) {
            msg->type = ERROR;
            Reply((void*)msg, senderPid);
            return;
        }
        int blockNumber;
        // The entry is using the direct block
        if (index < NUM_DIRECT) {
            blockNumber = dirInode->direct[index];
        }
        // The entry is using the indirect block
        else {
            blockNumber = GetDataBlockNumberFromIndirectBlock(dirInode->indirect, index - NUM_DIRECT);
        }
        
        // Get the data block from the cache and check the directory entry
        struct BlockCacheEntry* block = GetBlockFromCache(blockNumber);
        if (block == NULL) {
            msg->type = ERROR;
            Reply((void*)msg, senderPid);
            return;
        }
        void *blockData = block->data;
        struct dir_entry *dirEntry = (struct dir_entry *)(blockData + (i % DIRENTRY_PER_BLOCK) * sizeof(struct dir_entry));
        // If the entry is valid and not . or .., return ERROR
        if (dirEntry->inum > 0) {
            TracePrintf(0, "YfsRmDir: Found valid directory entry %s\n", dirEntry->name);
            if ( (dirEntry->name[0] == '.' && dirEntry->name[1] == '\0') || (dirEntry->name[1] == '.' && dirEntry->name[2] == '\0')) {
                TracePrintf(0, "YfsRmDir: Found . or .. entry, skipping\n");
                continue;
            } else {
                TracePrintf(0, "YfsRmDir: Directory is not empty, contains entry '%s'\n", dirEntry->name);
                msg->type = ERROR;
                Reply((void*)msg, senderPid);
                return;
            }
        }
    }

    // Remove the directory entry from the parent inode
    int status = RemoveEntryFromDir(dirInum, dirName, parentInodeEntry);
    if (status == ERROR) {
        TracePrintf(0, "YfsRmDir: Error removing directory entry\n");
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    // Decrease the link count of the parent inode
    parentInodeEntry->inodeInfo->nlink -= 1;
    parentInodeEntry->isDirty = 1;

    // Free the directory inode
    // Clear the data block of this inode
    TruncateFile(dirInodeEntry);
    // Mark the inode as free
    dirInodeEntry->inodeInfo->type = INODE_FREE;
    dirInodeEntry->isDirty = 1;
    freeInodesList[dirInum] = 1;
    freeInodesCount += 1;

    TracePrintf(0, "YfsRmDir: Removed directory %s successfully\n", pathname);

    msg->type = 0;
    Reply((void*)msg, senderPid);
    return;

}

void YfsChDir(YfsMsg* msg, int senderPid) {
    TracePrintf(0, "YfsChDir: Received message from process %d\n", senderPid);

    char pathname[MAXPATHNAMELEN + 1];
    if (CopyFrom(senderPid, pathname, msg->addr1, MAXPATHNAMELEN) == ERROR) {
        TracePrintf(0, "YfsChDir: Error copying pathname from process %d\n", senderPid);
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    if (resolveTrailingSlash(pathname)) {
        TracePrintf(0, "YfsChDir - ERROR: path is NULL or 0\n");
        printf("ERROR: Failed to add trailing dot after path\n");
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    if (verifyCwdReuse(pathname, msg->data1, msg->data2) == ERROR) {
        TracePrintf(0, "YfsChDir - ERROR: cwdReuse does not match\n");
        printf("ERROR: Current working directory has changed\n");
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    int targetInum = resolvePath(pathname, msg->data1, 0, 1);
    if (targetInum == ERROR || targetInum == 0) {
        TracePrintf(0, "YfsChDir: Error resolving pathname %s\n", pathname);
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    struct InodeCacheEntry* targetInodeEntry = GetInodeFromCache(targetInum);
    if (targetInodeEntry == NULL || targetInodeEntry->inodeInfo->type != INODE_DIRECTORY) {
        TracePrintf(0, "YfsChDir: Error: %s is not a directory\n", pathname);
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    TracePrintf(0, "YfsChDir: Changing directory to %s (inum: %d)\n", pathname, targetInum);
    msg->type = 0;
    msg->data1 = targetInum;
    msg->data2 = targetInodeEntry->inodeInfo->reuse;
    Reply((void*)msg, senderPid);
    return;
}

void YfsStat(YfsMsg* msg, int senderPid) {
    TracePrintf(0, "YfsStat: Received message from process %d\n", senderPid);
    char pathname[MAXPATHNAMELEN + 1];
    if (CopyFrom(senderPid, pathname, msg->addr1, MAXPATHNAMELEN) == ERROR) {
        TracePrintf(0, "YfsStat: Error copying pathname from process %d\n", senderPid);
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    if (resolveTrailingSlash(pathname)) {
        TracePrintf(0, "YfsStat - ERROR: path is NULL or 0\n");
        printf("ERROR: Failed to add trailing dot after path\n");
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    if (verifyCwdReuse(pathname, msg->data1, msg->data2) == ERROR) {
        TracePrintf(0, "YfsStat - ERROR: cwdReuse does not match\n");
        printf("ERROR: Current working directory has changed\n");
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    // Resolve the path to find the inode number
    int inum = resolvePath(pathname, msg->data1, 0, 0);
    if (inum == ERROR || inum == 0) {
        TracePrintf(0, "YfsStat: Error resolving pathname %s\n", pathname);
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    // Get the inode entry
    TracePrintf(0, "pathname: %s, inum: %d\n", pathname, inum);
    struct InodeCacheEntry* inodeEntry = GetInodeFromCache(inum);
    if (inodeEntry == NULL) {
        TracePrintf(0, "YfsStat: Error getting inode entry for pathname %s (inum %d)\n", pathname, inum);
        msg->type = ERROR;
        Reply((void*)msg, senderPid);
        return;
    }

    // Copy the inode information to the message
    struct inode* inodeInfo = inodeEntry->inodeInfo;
    msg->data1 = inum;
    msg->data2 = inodeInfo->type;
    msg->data3 = inodeInfo->size;
    msg->addr1 = (void*)(long)inodeInfo->nlink; // Since we don't have data4, we use addr1 to store nlink
    Reply((void*)msg, senderPid);
}

void YfsSync(YfsMsg* msg, int senderPid) {
    TracePrintf(0, "YfsSync: Received message from process %d\n", senderPid);
    // Flush all modified data to the disk
    SyncCache();

    // Reply to the sender process so that it can continue
    msg->type = 0;
    Reply((void*)msg, senderPid);
}

void YfsShutDown(YfsMsg* msg, int senderPid) {
    TracePrintf(0, "YfsShutDown: Received message from process %d\n", senderPid);
    // Flush all modified data to the disk
    SyncCache();

    // Reply to the sender process so that it can continue
    msg->type = 0;
    Reply((void*)msg, senderPid);

    // Server should print informative message indicating it is shutting down
    TracePrintf(0, "YfsShutDown: Shutting down file server process\n");
    printf("ShutDown called by process %d, YFS server shutting down...\n", senderPid);

    // Shut down the file server process (call Exit)
    Exit(0);
}