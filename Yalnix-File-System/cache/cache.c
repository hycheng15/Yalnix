#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cache.h"

// Block cache variables
BlockCacheEntry *blockCacheLruHead;
BlockCacheEntry *blockCacheLruTail;
int blockCacheCount;
BlockCacheEntry *blockCacheHashHead[BLOCK_CACHESIZE];
BlockCacheEntry *blockCacheHashTail[BLOCK_CACHESIZE];

// Inode cache variables
InodeCacheEntry *inodeCacheLruHead;
InodeCacheEntry *inodeCacheLruTail;
int inodeCacheCount;
InodeCacheEntry *inodeCacheHashHead[INODE_CACHESIZE];
InodeCacheEntry *inodeCacheHashTail[INODE_CACHESIZE];

/**
 * Initialize the block cache and inode cache
 */
void InitializeCache() {
    // Initialize block cache
    blockCacheLruHead = NULL;
    blockCacheLruTail = NULL;
    blockCacheCount = 0;

    for (int i = 0; i < BLOCK_CACHESIZE; i++) {
        blockCacheHashHead[i] = NULL;
        blockCacheHashTail[i] = NULL;
    }

    // Initialize inode cache
    inodeCacheLruHead = NULL;
    inodeCacheLruTail = NULL;
    inodeCacheCount = 0;

    for (int i = 0; i < INODE_CACHESIZE; i++) {
        inodeCacheHashHead[i] = NULL;
        inodeCacheHashTail[i] = NULL;
    }
}

/** 
 * Sync both the block cache and inode cache to disk
 */
void SyncCache() {
    TracePrintf(5, "SyncCache: Syncing cache\n");
    // Sync all inodes in the inode cache first
    InodeCacheEntry *inodeEntry = inodeCacheLruHead;
    while (inodeEntry) {
        // Write the inode back to its block cache if the inode is dirty, and mark the block as dirty
        if (inodeEntry->isDirty) {
            TracePrintf(6, "SyncCache: Inode %d is dirty, writing back to its block cache\n", inodeEntry->inodeNumber);
            struct BlockCacheEntry* blockEntry = GetBlockFromCache(inodeEntry->inodeNumber / INODES_PER_BLOCK + 1);
            struct inode* overwrite = (struct inode*)(blockEntry->data) + (inodeEntry->inodeNumber % INODES_PER_BLOCK);
            memcpy(overwrite, inodeEntry->inodeInfo, sizeof(struct inode));
            blockEntry->isDirty = 1;
            inodeEntry->isDirty = 0; // Mark the inode as clean after writing back
        }
        inodeEntry = inodeEntry->lruNext;
    }

    // Then sync all blocks in the block cache
    BlockCacheEntry *blockEntry = blockCacheLruHead;
    while (blockEntry) {
        // Write back to disk if the block is dirty
        if (blockEntry->isDirty) {
            TracePrintf(6, "SyncCache: Block %d is dirty, writing back to disk\n", blockEntry->blockNumber);
            WriteSector(blockEntry->blockNumber, blockEntry->data);
            blockEntry->isDirty = 0; // Mark the block as clean after writing back
        }
        blockEntry = blockEntry->lruNext;
    }
    TracePrintf(5, "SyncCache: Finish syncing cache\n");
}

/**
 * Add a blockEntry to the top of the LRU cache and the hash table
 * @param blockEntry The block entry to add
 */
void AddBlockToCache(BlockCacheEntry *blockEntry) {
    TracePrintf(6, "AddBlockToCache: Adding block %d to cache\n", blockEntry->blockNumber);
    PrintBlockLRUCache();
    if (blockCacheCount == BLOCK_CACHESIZE) {
        TracePrintf(6, "AddBlockToCache: Current block cache is full with %d blocks, removing tail\n", blockCacheCount);
        EvictBlockFromCache(blockCacheLruTail);
    }

    // Add to the top of the LRU linked list
    if (blockCacheLruHead == NULL) {
        TracePrintf(6, "AddBlockToCache: Current block cache is empty, add to head and tail\n");
        blockCacheLruHead = blockEntry;
        blockCacheLruTail = blockEntry;
    } 
    else {
        TracePrintf(6, "AddBlockToCache: Add to head of the cache\n");
        blockEntry->lruNext = blockCacheLruHead;
        blockCacheLruHead->lruPrev = blockEntry;
        blockCacheLruHead = blockEntry;
    }

    // Add to the hash table
    // If collision occurs, add to the top of the linked list
    int hashIndex = blockEntry->blockNumber % BLOCK_CACHESIZE;
    if (blockCacheHashHead[hashIndex] == NULL) {
        TracePrintf(6, "AddBlockToCache: Add to hash table head\n");
        blockCacheHashHead[hashIndex] = blockEntry;
        blockCacheHashTail[hashIndex] = blockEntry;
    } 
    else {
        TracePrintf(6, "AddBlockToCache: Hash collision occurs, add to linked list\n");
        blockEntry->hashNext = blockCacheHashHead[hashIndex];
        blockCacheHashHead[hashIndex]->hashPrev = blockEntry;
        blockCacheHashHead[hashIndex] = blockEntry;
    }

    blockCacheCount += 1;
    PrintBlockLRUCache();
}

/**
 * Evict a blockEntry from the LRU cache and the hash table.
 * Write back to disk if the block is dirty
 * @param blockEntry The block entry to remove
 */
void EvictBlockFromCache(BlockCacheEntry *blockEntry) {
    TracePrintf(6, "EvictBlockFromCache: Evicting block %d from cache\n", blockEntry->blockNumber);
    PrintBlockLRUCache();
    // Write back to disk if the block is dirty
    if (blockEntry->isDirty) {
        TracePrintf(6, "EvictBlockFromCache: Block %d is dirty, writing back to disk\n", blockEntry->blockNumber);
        WriteSector(blockEntry->blockNumber, blockEntry->data);
    }

    // Remove from the LRU linked list
    TracePrintf(6, "EvictBlockFromCache: Removing block %d from LRU linked list\n", blockEntry->blockNumber);
    if (blockEntry->lruNext == NULL && blockEntry->lruPrev == NULL) {
        // This is the only entry in the list
        blockCacheLruHead = NULL;
        blockCacheLruTail = NULL;
    } 
    else if (blockEntry->lruNext == NULL) {
        // This is the tail of the list
        blockCacheLruTail = blockEntry->lruPrev;
        blockCacheLruTail->lruNext = NULL;
    } 
    else if (blockEntry->lruPrev == NULL) {
        // This is the head of the list
        blockCacheLruHead = blockEntry->lruNext;
        blockCacheLruHead->lruPrev = NULL;
    } 
    else {
        // This is in the middle of the list
        blockEntry->lruPrev->lruNext = blockEntry->lruNext;
        blockEntry->lruNext->lruPrev = blockEntry->lruPrev;
    }

    // Remove from the hash table
    TracePrintf(6, "EvictBlockFromCache: Removing block %d from hash table\n", blockEntry->blockNumber);
    int hashIndex = blockEntry->blockNumber % BLOCK_CACHESIZE;
    if (blockEntry->hashNext == NULL && blockEntry->hashPrev == NULL) {
        // This is the only entry in the list
        blockCacheHashHead[hashIndex] = NULL;
        blockCacheHashTail[hashIndex] = NULL;
    } 
    else if (blockEntry->hashNext == NULL) {
        // This is the tail of the list
        blockCacheHashTail[hashIndex] = blockEntry->hashPrev;
        blockCacheHashTail[hashIndex]->hashNext = NULL;
    } 
    else if (blockEntry->hashPrev == NULL) {
        // This is the head of the list
        blockCacheHashHead[hashIndex] = blockEntry->hashNext;
        blockCacheHashHead[hashIndex]->hashPrev = NULL;
    } 
    else {
        // This is in the middle of the list
        blockEntry->hashPrev->hashNext = blockEntry->hashNext;
        blockEntry->hashNext->hashPrev = blockEntry->hashPrev;
    }

    free(blockEntry->data);
    free(blockEntry);
    blockCacheCount -= 1;
    PrintBlockLRUCache();
}

/**
 * Get a block from the cache. 
 * If the block is not in the cache, read it from disk and add it to the cache.
 * @param blockNumber The block number to get
 * @return The block entry from the cache
 */
BlockCacheEntry *GetBlockFromCache(int blockNumber) {
    TracePrintf(6, "GetBlockFromCache: Getting block %d from cache\n", blockNumber);
    
    // Check if blockNumber is valid
    if (blockNumber < 0 || blockNumber >= fsHeader->num_blocks) {
        TracePrintf(6, "GetBlockFromCache: Invalid block number %d\n", blockNumber);
        return NULL;
    }

    // Find the block in the cache first
    int hashIndex = blockNumber % BLOCK_CACHESIZE;
    BlockCacheEntry *blockEntry = blockCacheHashHead[hashIndex];
    while (blockEntry) {
        if (blockEntry->blockNumber == blockNumber) {
            TracePrintf(6, "GetBlockFromCache: Block %d found in cache\n", blockNumber);
            MoveBlockToHead(blockEntry);
            return blockEntry;
        }
        blockEntry = blockEntry->hashNext;
    }

    // If the block is not in the cache, read it from disk
    TracePrintf(6, "GetBlockFromCache: Block %d not found in cache, reading from disk\n", blockNumber);
    blockEntry = malloc(sizeof(BlockCacheEntry));
    blockEntry->blockNumber = blockNumber;
    blockEntry->isDirty = 0;
    blockEntry->lruPrev = NULL;
    blockEntry->lruNext = NULL;
    blockEntry->hashPrev = NULL;
    blockEntry->hashNext = NULL;
    blockEntry->data = malloc(BLOCKSIZE);
    ReadSector(blockNumber, blockEntry->data);
    TracePrintf(6, "GetBlockFromCache: Block %d read from disk\n", blockNumber);

    // Add the block to the cache
    AddBlockToCache(blockEntry);

    return blockEntry;
}

/**
 * Move a block entry to the head of the LRU linked list
 * @param blockEntry The block entry to move
 */
void MoveBlockToHead(BlockCacheEntry *blockEntry) {
    TracePrintf(6, "MoveBlockToHead: Moving block %d to head of LRU linked list\n", blockEntry->blockNumber);
    PrintBlockLRUCache();
    // Return directly if the block is already at the head
    if (blockEntry == blockCacheLruHead) {
        return;
    }

    // if this was the tail, fix up the tail pointer
    if (blockEntry == blockCacheLruTail) {
        blockCacheLruTail = blockEntry->lruPrev;
        blockCacheLruTail->lruNext = NULL;
    }

    if (blockEntry->lruPrev) {
        blockEntry->lruPrev->lruNext = blockEntry->lruNext;
    }
    if (blockEntry->lruNext) {
        blockEntry->lruNext->lruPrev = blockEntry->lruPrev;
    }

    blockEntry->lruNext = blockCacheLruHead;
    blockCacheLruHead->lruPrev = blockEntry;
    blockEntry->lruPrev = NULL;
    blockCacheLruHead = blockEntry;
    PrintBlockLRUCache();
}

/**
 * Mark a block as dirty and move it to the head of the LRU linked list
 * @param blockNumber The block number to mark as dirty
 */
void MarkBlockDirty(int blockNumber) {
    TracePrintf(6, "MarkBlockDirty: Marking block %d as dirty\n", blockNumber);
    // Find the block in the cache using the hash table
    int hashIndex = blockNumber % BLOCK_CACHESIZE;
    BlockCacheEntry *blockEntry = blockCacheHashHead[hashIndex];
    while (blockEntry) {
        if (blockEntry->blockNumber == blockNumber) {
            TracePrintf(6, "MarkBlockDirty: Block %d found in cache\n", blockNumber);
            MoveBlockToHead(blockEntry);
            blockEntry->isDirty = 1;
            return;
        }
        blockEntry = blockEntry->hashNext;
    }
    // If the block is not in the cache, print an error message
    TracePrintf(6, "MarkBlockDirty: Block %d not found in cache\n", blockNumber);
}

// ===============================================================================================

/**
 * Add an inodeEntry to the top of the LRU cache and the hash table
 * @param inodeEntry The inode entry to add
 */
void AddInodeToCache(InodeCacheEntry *inodeEntry) {    
    inodeEntry->lruNext = NULL;
    inodeEntry->lruPrev = NULL;
    inodeEntry->hashNext = NULL;
    inodeEntry->hashPrev = NULL;

    TracePrintf(6, "AddInodeToCache: Adding inode %d to cache\n", inodeEntry->inodeNumber);
    PrintInodeLRUCache();
    if (inodeCacheCount == INODE_CACHESIZE) {
        TracePrintf(6, "AddInodeToCache: Current inode cache is full with %d inodes, removing tail\n", inodeCacheCount);
        EvictInodeFromCache(inodeCacheLruTail);
    }

    // Add to the top of the LRU linked list
    if (inodeCacheLruHead == NULL) {
        TracePrintf(6, "AddInodeToCache: Current inode cache is empty, add to head and tail\n");
        inodeCacheLruHead = inodeEntry;
        inodeCacheLruTail = inodeEntry;
    } 
    else {
        TracePrintf(6, "AddInodeToCache: Add to head of the cache\n");
        TracePrintf(6, "AddInodeToCache: Current inodeCacheLruHead is %d\n", inodeCacheLruHead->inodeNumber);
        inodeEntry->lruNext = inodeCacheLruHead;
        inodeCacheLruHead->lruPrev = inodeEntry;
        inodeCacheLruHead = inodeEntry;
    }

    // Add to the hash table
    // If collision occurs, add to the top of the linked list
    int hashIndex = inodeEntry->inodeNumber % INODE_CACHESIZE;
    if (inodeCacheHashHead[hashIndex] == NULL) {
        TracePrintf(6, "AddInodeToCache: Add to hash table head\n");
        inodeCacheHashHead[hashIndex] = inodeEntry;
        inodeCacheHashTail[hashIndex] = inodeEntry;
    } 
    else {
        TracePrintf(6, "AddInodeToCache: Hash collision occurs, add to linked list\n");
        inodeEntry->hashNext = inodeCacheHashHead[hashIndex];
        inodeCacheHashHead[hashIndex]->hashPrev = inodeEntry;
        inodeCacheHashHead[hashIndex] = inodeEntry;
    }

    inodeCacheCount += 1;
    PrintInodeLRUCache();
}

/**
 * Evict an inode Entry from the LRU cache and the hash table.
 * Write back to its block in cache if the inode is dirty
 * @param inodeEntry The inode entry to remove
 */
void EvictInodeFromCache(InodeCacheEntry *inodeEntry) {
    TracePrintf(6, "EvictInodeFromCache: Evicting inode %d from cache\n", inodeEntry->inodeNumber);
    
    // Write the inode back to its block cache if the inode is dirty, and mark the block as dirty
    if (inodeEntry->isDirty) {
        TracePrintf(6, "EvictInodeFromCache: Inode %d is dirty, writing back to its block cache\n", inodeEntry->inodeNumber);
        struct BlockCacheEntry* blockEntry = GetBlockFromCache(inodeEntry->inodeNumber / INODES_PER_BLOCK + 1);
        struct inode* overwrite = (struct inode*)(blockEntry->data) + (inodeEntry->inodeNumber % INODES_PER_BLOCK);
        memcpy(overwrite, inodeEntry->inodeInfo, sizeof(struct inode));
        blockEntry->isDirty = 1;
    }
    
    // Remove from the LRU linked list
    TracePrintf(6, "EvictInodeFromCache: Removing inode %d from LRU linked list\n", inodeEntry->inodeNumber);
    if (inodeEntry->lruNext == NULL && inodeEntry->lruPrev == NULL) {
        // This is the only entry in the list
        inodeCacheLruHead = NULL;
        inodeCacheLruTail= NULL;
    } 
    else if (inodeEntry->lruNext == NULL) {
        // This is the tail of the list
        inodeCacheLruTail = inodeEntry->lruPrev;
        inodeCacheLruTail->lruNext = NULL;
    } 
    else if (inodeEntry->lruPrev == NULL) {
        // This is the head of the list
        inodeCacheLruHead = inodeEntry->lruNext;
        inodeCacheLruHead->lruPrev = NULL;
    } 
    else {
        // This is in the middle of the list
        inodeEntry->lruPrev->lruNext = inodeEntry->lruNext;
        inodeEntry->lruNext->lruPrev = inodeEntry->lruPrev;
    }

    // Remove from the hash table
    TracePrintf(6, "EvictInodeFromCache: Removing inode %d from hash table\n", inodeEntry->inodeNumber);
    int hashIndex = inodeEntry->inodeNumber % INODE_CACHESIZE;
    if (inodeEntry->hashNext == NULL && inodeEntry->hashPrev == NULL) {
        // This is the only entry in the list
        inodeCacheHashHead[hashIndex] = NULL;
        inodeCacheHashTail[hashIndex] = NULL;
    } 
    else if (inodeEntry->hashNext == NULL) {
        // This is the tail of the list
        inodeCacheHashTail[hashIndex] = inodeEntry->hashPrev;
        inodeCacheHashTail[hashIndex]->hashNext = NULL;
    } 
    else if (inodeEntry->hashPrev == NULL) {
        // This is the head of the list
        inodeCacheHashHead[hashIndex] = inodeEntry->hashNext;
        inodeCacheHashHead[hashIndex]->hashPrev = NULL;
    } 
    else {
        // This is in the middle of the list
        inodeEntry->hashPrev->hashNext = inodeEntry->hashNext;
        inodeEntry->hashNext->hashPrev = inodeEntry->hashPrev;
    }

    free(inodeEntry->inodeInfo);
    free(inodeEntry);
    inodeCacheCount -= 1;
}

/**
 * Get an inode from the cache. 
 * If the inode is not in the cache, read it from block cache and add it to the cache.
 * @param inodeNumber The inode number to get
 * @return The inode entry from the cache
 */
InodeCacheEntry *GetInodeFromCache(int inodeNumber) {
    TracePrintf(6, "GetInodeFromCache: Getting inode %d from cache\n", inodeNumber);

    // Check if inodeNumber is valid
    if (inodeNumber < 0 || inodeNumber > fsHeader->num_inodes) {
        TracePrintf(6, "GetInodeFromCache: Invalid inode number %d\n", inodeNumber);
        return NULL;
    }

    // Find the inode in the cache first
    int hashIndex = inodeNumber % INODE_CACHESIZE;
    InodeCacheEntry *inodeEntry = inodeCacheHashHead[hashIndex];
    while (inodeEntry) {
        if (inodeEntry->inodeNumber == inodeNumber) {
            TracePrintf(6, "GetInodeFromCache: Inode %d found in cache\n", inodeNumber);
            MoveInodeToHead(inodeEntry);
            return inodeEntry;
        }
        inodeEntry = inodeEntry->hashNext;
    }

    // If the inode is not in the cache, read it block cache
    TracePrintf(6, "GetInodeFromCache: Inode %d not found in cache, reading from block cache\n", inodeNumber);
    int blockNumber = inodeNumber / INODES_PER_BLOCK + 1;
    BlockCacheEntry *blockEntry = GetBlockFromCache(blockNumber);
    if (blockEntry == NULL) {
        TracePrintf(6, "GetInodeFromCache: Block number %d invalid\n", blockNumber);
        return NULL;
    }

    // Create a new inode entry
    InodeCacheEntry *newInodeEntry = malloc(sizeof(InodeCacheEntry));
    newInodeEntry->inodeNumber = inodeNumber;
    newInodeEntry->isDirty = 0;
    newInodeEntry->lruPrev = NULL;
    newInodeEntry->lruNext = NULL;
    newInodeEntry->hashPrev = NULL;
    newInodeEntry->hashNext = NULL;
    newInodeEntry->inodeInfo = malloc(sizeof(struct inode));
    memcpy(newInodeEntry->inodeInfo, ((struct inode*)blockEntry->data) + (inodeNumber % INODES_PER_BLOCK), sizeof(struct inode));

    TracePrintf(6, "GetInodeFromCache: Inode %d get from block entry %d\n", inodeNumber, blockNumber);
    
    // Add the inode to the cache
    AddInodeToCache(newInodeEntry);
    return newInodeEntry;
}

/**
 * Move an inode entry to the head of the LRU linked list
 * @param inodeEntry The inode entry to move
 */
void MoveInodeToHead(InodeCacheEntry *inodeEntry) {
    TracePrintf(6, "MoveInodeToHead: Moving inode %d to head of LRU linked list\n", inodeEntry->inodeNumber);
    // Return directly if the inode is already at the head
    if (inodeEntry == inodeCacheLruHead) {
        return;
    }

    // if this was the tail, fix up the tail pointer
    if (inodeEntry == inodeCacheLruTail) {
        inodeCacheLruTail = inodeEntry->lruPrev;
        inodeCacheLruTail->lruNext = NULL;
    }

    if (inodeEntry->lruPrev) {
        inodeEntry->lruPrev->lruNext = inodeEntry->lruNext;
    }
    if (inodeEntry->lruNext) {
        inodeEntry->lruNext->lruPrev = inodeEntry->lruPrev;
    }

    inodeEntry->lruNext = inodeCacheLruHead;
    inodeCacheLruHead->lruPrev = inodeEntry;
    inodeEntry->lruPrev = NULL;
    inodeCacheLruHead = inodeEntry;
}

/**
 * Mark a inode as dirty and move it to the head of the LRU linked list
 * @param inodeNumber The inode number to mark as dirty
 */
void MarkInodeDirty(int inodeNumber) {
    TracePrintf(6, "MarkInodeDirty: Marking inode %d as dirty\n", inodeNumber);
    // Find the inode in the cache using the hash table
    int hashIndex = inodeNumber % INODE_CACHESIZE;
    InodeCacheEntry *inodeEntry = inodeCacheHashHead[hashIndex];
    while (inodeEntry) {
        if (inodeEntry->inodeNumber == inodeNumber) {
            TracePrintf(6, "MarkInodeDirty: Inode %d found in cache\n", inodeNumber);
            MoveInodeToHead(inodeEntry);
            inodeEntry->isDirty = 1;
            return;
        }
        inodeEntry = inodeEntry->hashNext;
    }
    // If the inode is not in the cache, print an error message
    TracePrintf(6, "MarkInodeDirty: Inode %d not found in cache\n", inodeNumber);
}


/**
 * Print current block LRU cache from head to tail
 */
void PrintBlockLRUCache() {
    TracePrintf(6, "===== Block LRU Cache =====\n");
    BlockCacheEntry *curr = blockCacheLruHead;
    while (curr) {
        TracePrintf(6, "Block #%d | Dirty: %d\n", curr->blockNumber, curr->isDirty);
        curr = curr->lruNext;
    }
    TracePrintf(6, "===========================\n");
}

/**
 * Print block cache hash table
 */
void PrintBlockHashTable() {
    TracePrintf(6, "===== Block Hash Table =====\n");
    for (int i = 0; i < BLOCK_CACHESIZE; i++) {
        TracePrintf(6, "[%d]: ", i);
        BlockCacheEntry *entry = blockCacheHashHead[i];
        while (entry) {
            TracePrintf(6, "-> Block #%d ", entry->blockNumber);
            entry = entry->hashNext;
        }
        TracePrintf(6, "\n");
    }
    TracePrintf(6, "============================\n");
}

/**
 * Print current inode LRU cache from head to tail
 */
void PrintInodeLRUCache() {
    TracePrintf(6, "===== Inode LRU Cache =====\n");
    InodeCacheEntry *curr = inodeCacheLruHead;
    while (curr) {
        TracePrintf(6, "Inode #%d | Dirty: %d\n", curr->inodeNumber, curr->isDirty);
        curr = curr->lruNext;
    }
    TracePrintf(6, "===========================\n");
}

/**
 * Print inode cache hash table
 */
void PrintInodeHashTable() {
    TracePrintf(6, "===== Inode Hash Table =====\n");
    for (int i = 0; i < INODE_CACHESIZE; i++) {
        TracePrintf(6, "[%d]: ", i);
        InodeCacheEntry *entry = inodeCacheHashHead[i];
        while (entry) {
            TracePrintf(6, "-> Inode #%d ", entry->inodeNumber);
            entry = entry->hashNext;
        }
        TracePrintf(6, "\n");
    }
    TracePrintf(6, "============================\n");
}