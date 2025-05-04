#ifndef _PATH_H_
#define _PATH_H_

#include <comp421/yalnix.h>
#include <comp421/iolib.h>
#include <comp421/filesystem.h>
#include "../cache/cache.h"

int resolvePath(char* pathname,int inum, int symlinkDepth, int resolveLastSymLink);
int GetComponent(char* pathname, char** componentName, int index);
int GetInumByComponentName(struct InodeCacheEntry *parentInodeEntry, char *componentName);
int GetDataBlockNumberFromIndirectBlock(int indirectBlockNum, int index);
int ResolveSymbolicLink(int inum, struct inode* inodeInfo, int symlinkDepth);
int GetParentInum(int inum, char* pathname);
int CreateFindParent(char* pathname, int currentDirectory, char* component);
char* getFilename(char* pathname);
int RemoveEntryFromDir(int fileInum, char* filename, struct InodeCacheEntry* parentInodeEntry);
int GetBlockNumberFromIndirectBlock(int indirectBlockNum, int index);
int resolveTrailingSlash(char* originalPath);
int verifyCwdReuse(char* pathname, int currentWorkingDirectory, int cwdReuse);

// currentworking dir
extern int currentWorkingDirectory;

#endif /* _PATH_H_ */