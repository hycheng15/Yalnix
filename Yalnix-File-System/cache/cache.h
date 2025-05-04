#ifndef _CACHE_H
#define _CACHE_H

#include <comp421/yalnix.h>
#include <comp421/iolib.h>
#include <comp421/filesystem.h>
#include "../global.h"

extern void InitializeCache();
extern void SyncCache();

/**
 * Block cache
 */
extern int *freeBlocksList;         // An array to keep track of free blocks, 1 for free, 0 for used
extern int freeBlocksCount;         // Number of free blocks available

typedef struct BlockCacheEntry {
    int blockNumber;
    int isDirty;
    void* data;
    struct BlockCacheEntry *lruPrev;
    struct BlockCacheEntry *lruNext;
    struct BlockCacheEntry *hashPrev;
    struct BlockCacheEntry *hashNext;
} BlockCacheEntry;

// LRU linked list for block cache
extern BlockCacheEntry *blockCacheLruHead;      // The head of the list is the MOST recently used
extern BlockCacheEntry *blockCacheLruTail;      // The tail of the list is the LEAST recently used
extern int blockCacheCount;

// Hash table for block cache
extern BlockCacheEntry *blockCacheHashHead[BLOCK_CACHESIZE];
extern BlockCacheEntry *blockCacheHashTail[BLOCK_CACHESIZE];

void AddBlockToCache(BlockCacheEntry* blockEntry);
void EvictBlockFromCache(BlockCacheEntry* blockEntry);
BlockCacheEntry* GetBlockFromCache(int blockNumber);
void MoveBlockToHead(BlockCacheEntry* blockEntry);
void MarkBlockDirty(int blockNumber);

/**
 * Inode cache
 */
extern int *freeInodesList;         // An array to keep track of free inodes, 1 for free, 0 for used
extern int freeInodesCount;         // Number of free inodes available

typedef struct InodeCacheEntry {
    int inodeNumber;
    int isDirty;
    struct inode *inodeInfo;
    struct InodeCacheEntry *lruPrev;
    struct InodeCacheEntry *lruNext;
    struct InodeCacheEntry *hashPrev;
    struct InodeCacheEntry *hashNext;
} InodeCacheEntry;

// LRU linked list for inode cache
extern InodeCacheEntry *inodeCacheLruHead;      // The head of the list is the MOST recently used
extern InodeCacheEntry *inodeCacheLruTail;      // The tail of the list is the LEAST recently used
extern int inodeCacheCount;

// Hash table for inode cache
extern InodeCacheEntry *inodeCacheHashHead[INODE_CACHESIZE];
extern InodeCacheEntry *inodeCacheHashTail[INODE_CACHESIZE];

void AddInodeToCache(InodeCacheEntry *inodeEntry);
void EvictInodeFromCache(InodeCacheEntry* inodeEntry);
InodeCacheEntry* GetInodeFromCache(int inodeNumber);
void MoveInodeToHead(InodeCacheEntry* inodeEntry);
void MarkInodeDirty(int inodeNumber);

void PrintBlockLRUCache();
void PrintBlockHashTable();
void PrintInodeLRUCache();
void PrintInodeHashTable();

#endif /* _CACHE_H */