#ifndef GLOBAL_H
#define GLOBAL_H

#include <comp421/filesystem.h>

struct InodeCacheEntry;			// Forward declaration of InodeCacheEntry struct

#define INODES_PER_BLOCK (BLOCKSIZE / INODESIZE) // Number of inodes per block = 8
#define DIRENTRY_PER_BLOCK (int)(BLOCKSIZE / sizeof(struct dir_entry))
#define MESSAGE_SIZE 32
#define MAX_DIRECT_FILE_SIZE BLOCKSIZE * NUM_DIRECT
#define MAX_INDIRECT_FILE_SIZE BLOCKSIZE * (BLOCKSIZE / sizeof(int))
#define MAX_FILE_SIZE (int)(MAX_DIRECT_FILE_SIZE + MAX_INDIRECT_FILE_SIZE)

#define YFS_OPEN 1
#define YFS_CLOSE 2
#define YFS_CREATE 3
#define YFS_READ 4
#define YFS_WRITE 5
#define YFS_SEEK 6
#define YFS_LINK 7
#define YFS_UNLINK 8
#define YFS_SYMLINK 9
#define YFS_READLINK 10
#define YFS_MKDIR 11
#define YFS_RMDIR 12
#define YFS_CHDIR 13
#define YFS_STAT 14
#define YFS_SYNC 15
#define YFS_SHUTDOWN 16

extern struct fs_header *fsHeader;

// YfsMsg struct should be exactly 32 bytes for message sending
typedef struct YfsMsg {
	int type;       // e.g., YFS_OPEN, YFS_READ, 4 bytes
    int data1;      // 4 bytes
	int data2;      // 4 bytes
    int data3;      // 4 bytes
	void* addr1;    // 8 bytes
	void* addr2;    // 8 bytes
} YfsMsg;

void TruncateFile(struct InodeCacheEntry* inodeEntry);
int AllocateBlock();
int AllocateInode();
int AllocateBlockInInode(struct InodeCacheEntry* inodeEntry);
int AddDirEntry(int inum, char* filename, struct InodeCacheEntry* parentInodeEntry);

void YfsOpen(YfsMsg* msg, int senderPid);
void YfsCreate(YfsMsg* msg, int senderPid);
void YfsRead(YfsMsg* msg, int senderPid);
void YfsWrite(YfsMsg* msg, int senderPid);
void YfsSeek(YfsMsg* msg, int senderPid);
void YfsLink(YfsMsg* msg, int senderPid);
void YfsUnlink(YfsMsg* msg, int senderPid);
void YfsSymLink(YfsMsg* msg, int senderPid);
void YfsReadLink(YfsMsg* msg, int senderPid);
void YfsMkDir(YfsMsg* msg, int senderPid);
void YfsRmDir(YfsMsg* msg, int senderPid);
void YfsChDir(YfsMsg* msg, int senderPid);
void YfsStat(YfsMsg* msg, int senderPid);
void YfsSync(YfsMsg* msg, int senderPid);
void YfsShutDown(YfsMsg* msg, int senderPid);

#endif /* GLOBAL_H */