/*
* A YFS file server process
* Receives messages from clients processes and execute the requested file system operations
* Returns the reply message to the requesting processes
*/

#include <comp421/yalnix.h>
#include <comp421/iolib.h>
#include <comp421/filesystem.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "global.h"
#include "cache/cache.h"
#include "fs/path.h"

struct fs_header *fsHeader;

// Free blocks tracking
int *freeBlocksList;
int freeBlocksCount;

// Free inodes tracking
int *freeInodesList;
int freeInodesCount;

/**
 * Initializes the freeInodesList array and freeInodesCount
 */
void initializeFreeInodes() {
    TracePrintf(0, "initializeFreeInodes: Initializing free inodes\n");
    freeInodesCount = 0;
    // num_inodes does not contain inode 0 (fsHeader) 
    freeInodesList = (int*)malloc(sizeof(int) * (fsHeader->num_inodes + 1));
    TracePrintf(0, "initializeFreeInodes: fsHeader->num_inodes is %d\n", fsHeader->num_inodes);
    freeInodesList[0] = 0; // inode 0 is not free for fsHeader
    for (int i = 1; i <= fsHeader->num_inodes; i++) {
        InodeCacheEntry* currInode = GetInodeFromCache(i);
        if (currInode->inodeInfo->type == INODE_FREE) {
            freeInodesList[i] = 1;
            freeInodesCount += 1;
        } else {
            freeInodesList[i] = 0;
        }
    }
    TracePrintf(0, "initializeFreeInodes: There are %d free inodes\n", freeInodesCount);
}

/**
 * Initializes the freeBlocksList array and freeBlocksCount
 */
void initializeFreeBlocks() {
    TracePrintf(0, "initializeFreeBlocks: Initializing free blocks\n");
    freeBlocksCount = fsHeader->num_blocks;
    TracePrintf(0, "initializeFreeBlocks: fsHeader->num_blocks is %d\n", fsHeader->num_blocks);

    // num_blocks includes the total number of blocks in the file system
    freeBlocksList = malloc(sizeof(int) * fsHeader->num_blocks);
    for (int i = 0; i < fsHeader->num_blocks; i++) {
        freeBlocksList[i] = 1; // Assume all blocks are free initially
    }

    // Mark the boot block as used
    freeBlocksList[0] = 0;
    freeBlocksCount -= 1;
    TracePrintf(0, "initializeFreeBlocks: Marking boot block as used, freeBlocksCount is %d\n", freeBlocksCount);

    // Mark the blocks used by inodes as used
    int inodeBlockCount = (fsHeader->num_inodes + 1) / INODES_PER_BLOCK;
    for (int i = 1; i <= inodeBlockCount; i++) {
        freeBlocksList[i] = 0;
        freeBlocksCount -= 1;
    }
    TracePrintf(0, "initializeFreeBlocks: Marking %d inode blocks as used, freeBlocksCount is %d\n", inodeBlockCount, freeBlocksCount);

    // Go through each inode and mark the data blocks it uses as used
    for (int i = 1; i <= fsHeader->num_inodes; i++) {
        InodeCacheEntry* currInode = GetInodeFromCache(i);
        if (currInode->inodeInfo->type != INODE_FREE) {
            int j = 0;

            // Deal with direct blocks
            while (j < NUM_DIRECT && j * BLOCKSIZE < currInode->inodeInfo->size) {
                if (currInode->inodeInfo->direct[j] != 0) {
                    // Only mark the block as used if it is within the size of the file
                    TracePrintf(0, "initializeFreeBlocks: Marking direct block %d as used\n", currInode->inodeInfo->direct[j]);
                    freeBlocksList[currInode->inodeInfo->direct[j]] = 0;
                    freeBlocksCount -= 1;
                }
                j++;
            }

            // Deal with indirect blocks
            if (currInode->inodeInfo->indirect != 0) {
                TracePrintf(0, "initializeFreeBlocks: Marking indirect block %d as used\n", currInode->inodeInfo->indirect);
                freeBlocksList[currInode->inodeInfo->indirect] = 0;
                freeBlocksCount -= 1;

                // Round the block size to the nearest block
                int lastBlock = (currInode->inodeInfo->size + BLOCKSIZE - 1) / BLOCKSIZE;
                while (j < lastBlock) {
                    int indrectBlockNum = j - NUM_DIRECT;
                    TracePrintf(0, "initializeFreeBlocks: Marking indirect block %d as used\n", indrectBlockNum);
                    freeBlocksList[indrectBlockNum] = 0;
                    freeBlocksCount -= 1;
                    j++;
                }
            }
        }
    }

    TracePrintf(0, "initializeFreeBlocks: There are %d free blocks\n", freeBlocksCount);
}

/**
 * Truncates the file to size 0 and frees the data blocks.
 * Also marks the inode entry as dirty.
 * @param inodeEntry The inode entry of the file to truncate
 */
void TruncateFile(struct InodeCacheEntry* inodeEntry) {
    TracePrintf(0, "TruncateFile: Truncating file with inode number %d\n", inodeEntry->inodeNumber);
    struct inode* inodeInfo = inodeEntry->inodeInfo;

    // Free the direct data block
    for (int i = 0; i < NUM_DIRECT; i++) {
        if (inodeInfo->direct[i] == 0) {
            break;
        }
        TracePrintf(0, "TruncateFile: Freeing direct data block %d\n", inodeInfo->direct[i]);
        freeBlocksList[inodeInfo->direct[i]] = 1;
        freeBlocksCount += 1;
        inodeInfo->direct[i] = 0;
    }

    // Free the indirect block and its data blocks
    if (inodeInfo->indirect > 0) {
        TracePrintf(0, "TruncateFile: Traversing through data blocks pointing by indirect block %d\n", inodeInfo->indirect);
    	for (int i = 0; i < (int)(BLOCKSIZE / sizeof(int)); i++) {
    		int blockNum = GetDataBlockNumberFromIndirectBlock(inodeInfo->indirect, i);
            if (blockNum == 0) {
                break;
            }
            TracePrintf(0, "TruncateFile: Freeing indirect data block %d\n", blockNum);
            freeBlocksList[blockNum] = 1;
            freeBlocksCount += 1;
    	}
        TracePrintf(0, "TruncateFile: Freeing indirect block %d\n", inodeInfo->indirect);
        freeBlocksList[inodeInfo->indirect] = 1;
        freeBlocksCount += 1;
        inodeInfo->indirect = 0;
    }

    inodeInfo->size = 0;
    inodeEntry->isDirty = 1;
}

/**
 * Allocates a block from the free blocks list
 * @return The block number, or ERROR if no free blocks are available
 */
int AllocateBlock() {
    TracePrintf(0, "AllocateBlock: Allocating a block\n");
    if (freeBlocksCount <= 0) {
        TracePrintf(0, "AllocateBlock: No free blocks available\n");
        return ERROR;
    }
    for (int i = 1; i < fsHeader->num_blocks; i++) {
        if (freeBlocksList[i] == 1) {
            freeBlocksList[i] = 0;
            freeBlocksCount -= 1;

            // Make sure the block is zeroed out
            struct BlockCacheEntry* blockEntry = GetBlockFromCache(i);
            if (blockEntry == NULL) {
                TracePrintf(0, "AllocateBlock: Failed to get block from cache\n");
                return ERROR;
            }
            void* block = blockEntry->data;
            memset(block, 0, BLOCKSIZE);
            blockEntry->isDirty = 1;
            return i;
        }
    }
    return ERROR;
}

/**
 * Allocates an inode from the free inodes list
 * @return The inode number, or ERROR if no free inodes are available
 */
int AllocateInode() {
    TracePrintf(0, "AllocateInode: Allocating an inode\n");
    if (freeInodesCount <= 0) {
        TracePrintf(0, "AllocateInode: No free inodes available\n");
        return ERROR;
    }
    for (int i = 1; i <= fsHeader->num_inodes; i++) {
        if (freeInodesList[i] == 1) {
            freeInodesList[i] = 0;
            freeInodesCount -= 1;
            return i;
        }
    }
    return ERROR;
}

/**
 * Allocates a new data block in the inode
 * @param inodeEntry The inode entry to allocate the block in
 * @return 0 on success, ERROR if no free blocks are available
 */
int AllocateBlockInInode(struct InodeCacheEntry* inodeEntry) {
    struct inode* inodeInfo = inodeEntry->inodeInfo;
    // Check if there is space in the direct blocks
	for (int i = 0; i < NUM_DIRECT; i++) {
		if (inodeInfo->direct[i] == 0) {
			int blockNum = AllocateBlock();
			if (blockNum == ERROR) {
				return ERROR;
			}
			inodeInfo->direct[i] = blockNum;
			inodeEntry->isDirty = 1;
			return 0;
		}
	}

    // If all direct blocks are used, use / allocate an indirect block
	if (inodeInfo->indirect == 0) {
		int blockNum = AllocateBlock();
		if (blockNum == ERROR) {
			return ERROR;
		}

        struct BlockCacheEntry* blockEntry = GetBlockFromCache(blockNum);
        if (blockEntry == NULL) {
            return ERROR;
        }
		void* block = blockEntry->data;
		memset(block, 0, BLOCKSIZE);
		inodeInfo->indirect = blockNum;
        inodeEntry->isDirty = 1;
	}

    struct BlockCacheEntry* blockEntry = GetBlockFromCache(inodeInfo->indirect);
    if (blockEntry == NULL) {
        return ERROR;
    }
	void* block = blockEntry->data;
	for (int i = 0; i < (int)(BLOCKSIZE / sizeof(int)); i++) {
		if (((int*)block)[i] == 0) {
			int blockNum = AllocateBlock();
			if (blockNum == ERROR) {
				return ERROR;
			}
			((int*)block)[i] = blockNum;
			blockEntry->isDirty = 1;
			return 0;
		}
	}

	return ERROR;
}

/**
 * Creates and add a new directory entry to the parent inode
 * @param inum The inode number of the file to add
 * @param filename The name of the file to add
 * @param parentInodeEntry The inode cache entry of the parent directory
 * @return 0 on success, ERROR on failure
 */
int AddDirEntry(int inum, char* filename, struct InodeCacheEntry* parentInodeEntry) {
    TracePrintf(0, "AddDirEntry: Adding directory entry name (%s) and inum (%d)\n", filename, inum);
    int filenameLen = strlen(filename);

    if (filenameLen > DIRNAMELEN) {
        TracePrintf(0, "AddDirEntry: Filename length exceeds maximum length\n");
        return ERROR;
    }

    // Get the first free directory entry in the parent inode
    struct inode* parentInode = parentInodeEntry->inodeInfo;
    int totalDirEntries = parentInode->size / sizeof(struct dir_entry);

    int i;
    // Traverse the directory entries
    for (i = 0; i < totalDirEntries; i++) {
        // Get the block number of this directory entry
        int index = i / DIRENTRY_PER_BLOCK;
        if (index >= NUM_DIRECT && parentInode->indirect == 0) {
            return ERROR;
        }
        int blockNumber;
        // The entry is using the direct block
        if (index < NUM_DIRECT) {
            blockNumber = parentInode->direct[index];
        }
        // The entry is using the indirect block
        else {
            blockNumber = GetDataBlockNumberFromIndirectBlock(parentInode->indirect, index - NUM_DIRECT);
        }
        
        // Get the block data from the cache and check the directory entry
        struct BlockCacheEntry* block = GetBlockFromCache(blockNumber);
        if (block == NULL) {
            return ERROR;
        }
        void *blockData = block->data;
        struct dir_entry *dirEntry = (struct dir_entry *)(blockData + (i % DIRENTRY_PER_BLOCK) * sizeof(struct dir_entry));
        if (dirEntry->inum == 0) {
            TracePrintf(0, "AddDirEntry: Add using existing entry %d\n", i);
            dirEntry->inum = inum;
            memset(dirEntry->name, 0, DIRNAMELEN);
            memcpy(dirEntry->name, filename, filenameLen);
            block->isDirty = 1;
            parentInodeEntry->isDirty = 1;
            return 0;
        }
    }

    // If no free directory entry is found, setup a new dirEntry out of the current size

    TracePrintf(0, "AddDirEntry: No free directory entry found\n");
    // Allocate a new data block in the parent inode if needed
    if (parentInode->size % BLOCKSIZE == 0) {
        if (AllocateBlockInInode(parentInodeEntry) < 0) {
            return ERROR;
        }
    }

    parentInode->size += sizeof(struct dir_entry);
    int index = i / DIRENTRY_PER_BLOCK;
    if (index >= NUM_DIRECT && parentInode->indirect == 0) {
        return ERROR;
    }
    int blockNumber;
    // The entry is using the direct block
    if (index < NUM_DIRECT) {
        blockNumber = parentInode->direct[index];
    }
    // The entry is using the indirect block
    else {
        blockNumber = GetDataBlockNumberFromIndirectBlock(parentInode->indirect, index - NUM_DIRECT);
    }
    struct BlockCacheEntry* block = GetBlockFromCache(blockNumber);
    if (block == NULL) {
        return ERROR;
    }
    TracePrintf(0, "AddDirEntry: Adding new directory entry in block %d\n", blockNumber);
    void *blockData = block->data;
    struct dir_entry *dirEntry = (struct dir_entry *)(blockData + (i % DIRENTRY_PER_BLOCK) * sizeof(struct dir_entry));
    dirEntry->inum = inum;
    memset(dirEntry->name, 0, DIRNAMELEN);
    memcpy(dirEntry->name, filename, filenameLen);
    block->isDirty = 1;
    parentInodeEntry->isDirty = 1;
    return 0;
}

int main(int argc, char **argv) {
    TracePrintf(0, "main: YFS server initializing\n");
    Register(FILE_SERVER);

    // Get the file system header
    void *firstBlock = malloc(BLOCKSIZE);
    if (ReadSector(1, firstBlock) == 0) {
        fsHeader = (struct fs_header*)firstBlock;
        TracePrintf(0, "main: File system header read successfully\n");
        TracePrintf(0, "main: File system header: num_blocks = %d, num_inodes = %d\n", fsHeader->num_blocks, fsHeader->num_inodes);
    }
    else {
        TracePrintf(0, "main: Error reading file system header\n");
        free(firstBlock);
        Exit(ERROR);
    }

    InitializeCache();
    initializeFreeInodes();
    initializeFreeBlocks();

    TracePrintf(0, "main: YFS server initialized\n");

    // Fork a child process and enter while(1)
    int pid = Fork();
    if (pid == 0) {
        // Child process
        Exec(argv[1], argv + 1);
    }

    while (1) {
        YfsMsg msg;
        int senderPid = Receive(&msg);

        // Receive will return 0 if there's deadlock, otherwise returns the senderPid
        if (senderPid <= 0) {
            TracePrintf(0, "main: Error receiving message, senderPid is %d\n", senderPid);
            return ERROR;
        }

        int msgType = msg.type;
        TracePrintf(0, "main: Received message of type %d from process %d\n", msgType, senderPid);

        switch (msgType) {
            case YFS_OPEN:
                YfsOpen(&msg, senderPid);
                break;
            case YFS_CREATE:
                YfsCreate(&msg, senderPid);
                break;
            case YFS_READ:
                YfsRead(&msg, senderPid);
                break;
            case YFS_WRITE:
                YfsWrite(&msg, senderPid);
                break;
            case YFS_SEEK:
                YfsSeek(&msg, senderPid);
                break;
            case YFS_LINK:
                YfsLink(&msg, senderPid);
                break;
            case YFS_UNLINK:
                YfsUnlink(&msg, senderPid);
                break;
            case YFS_SYMLINK:
                YfsSymLink(&msg, senderPid);
                break;
            case YFS_READLINK:
                YfsReadLink(&msg, senderPid);
                break;
            case YFS_MKDIR:
                YfsMkDir(&msg, senderPid);
                break;
            case YFS_RMDIR:
                YfsRmDir(&msg, senderPid);
                break;
            case YFS_CHDIR:
                YfsChDir(&msg, senderPid);
                break;
            case YFS_STAT:
                YfsStat(&msg, senderPid);
                break;
            case YFS_SYNC:
                YfsSync(&msg, senderPid);
                break;
            case YFS_SHUTDOWN:
                YfsShutDown(&msg, senderPid);
                break;  
            default:
                TracePrintf(0, "main: Unknown message type %d\n", msgType);
                break;
        }
    }

    return 0;
}

