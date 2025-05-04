#include <comp421/yalnix.h>
#include <comp421/iolib.h>
#include <comp421/filesystem.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "path.h"
#include "../cache/cache.h"

/**
 * Parse the given pathname from directory inum
 * @param pathname The pathname to parse
 * @param inum The inode number of the directory to start from
 * @param symlinkDepth The depth of the symbolic link resolution, should not exceed MAXSYMLINKS
 * @param resolveLastSymLink If true, resolve the last symlink in the pathname
 * @return The inode number of the pathname (file or directory), or ERROR if not found
 */
int resolvePath(char *pathname, int inum, int symlinkDepth, int resolveLastSymLink) {
    TracePrintf(0, "resolvePath: Resolving pathname %s from inode %d, resolveLastSymLink is %d\n", pathname, inum, resolveLastSymLink);
    if (pathname == NULL || (inum == -1 && pathname[0] != '/')) {
        return ERROR;
    }

    if (pathname[0] == '/' && pathname[1] == '\0') {
        TracePrintf(0, "resolvePath: Pathname is just root, returning ROOTINODE\n");
        return ROOTINODE;
    }

    // Start with the current working directory
    int currentDir = (pathname[0] == '/') ? ROOTINODE : inum;
    int currentInode;

    int index = 0;
    char* componentName = NULL;
    
    // Get the first component of the pathname
    index = GetComponent(pathname, &componentName, index);

    // Resolve each component of the pathname
    while (componentName != NULL) {
        // Get current dir
        struct InodeCacheEntry* currentDirEntry = GetInodeFromCache(currentDir);
        if (currentDirEntry == NULL) {
            free(componentName);
            return ERROR;
        }
        
        struct inode* dirInodeInfo = GetInodeFromCache(currentDir)->inodeInfo;
        TracePrintf(0, "resolvePath: componentName is %s, currentDir is type %d\n", componentName, dirInodeInfo->type);
        
        // If the current dir is a regular file and there are more components to resolve
        if ((dirInodeInfo->type == INODE_FREE) || (dirInodeInfo->type == INODE_REGULAR)) {
            TracePrintf(0, "resolvePath: dirInode is INODE_FREE or INODE_REGULAR, returning ERROR\n");
            free(componentName);
            return ERROR;
        }

        // If the current dir is a directory, we need to get the inode number of the component
        if (dirInodeInfo->type == INODE_DIRECTORY) {
            // Get the child component's inode number from the directory
            TracePrintf(0, "resolvePath: dirInodeInfo is INODE_DIRECTORY, doing GetInumByComponentName\n");
            currentInode = GetInumByComponentName(currentDirEntry, componentName);

            if (currentInode == ERROR || currentInode == 0) {
                TracePrintf(0, "resolvePath: GetInumByComponentName returned ERROR or 0\n");
                free(componentName);
                return ERROR;
            }

            struct InodeCacheEntry *currentInodeEntry = GetInodeFromCache(currentInode);
            if (currentInodeEntry == NULL) {
                TracePrintf(0, "resolvePath: currentInodeEntry is NULL\n");
                free(componentName);
                return ERROR;
            }
            struct inode* inodeInfo = currentInodeEntry->inodeInfo;

            // Check if we get an INODE_SYMLINK
            if (inodeInfo->type == INODE_SYMLINK) {
                // Get the next component of the pathname
                free(componentName);
                componentName = NULL;
                index = GetComponent(pathname, &componentName, index);

                // This is the last component of the path name
                // If the last component of the pathname is the name of a symbolic link, 
                // then that symbolic link must not be traversed unless the pathname is being looked up
                // for an Open, Create, or ChDir file system operation
                if (componentName == NULL) {
                    free(componentName);
                    // This is the last component of the path name and resolveLastSymLink is 1
                    // So we need to resolve the symbolic link
                    if (resolveLastSymLink) {
                        // Get inode number from symbolic link recursively
                        TracePrintf(0, "resolvePath: currentInode is INODE_SYMLINK, resolveLastSymLink is %d, resolving symlink\n", resolveLastSymLink);
                        currentInode = ResolveSymbolicLink(currentDir, inodeInfo, symlinkDepth + 1);
                        if (currentInode == ERROR) {
                            free(componentName);
                            return ERROR;
                        }
                        // “cd” into the link’s target before moving on
                        currentDir = currentInode;
                        return currentInode;
                    }
                    // This is the last component of the path name but resolveLastSymLink is 0 
                    // We simply return the inum of the INODE_SYMLINK
                    else {
                        TracePrintf(0, "resolvePath: currentInode is INODE_SYMLINK, resolveLastSymLink is %d, not resolving symlink\n", resolveLastSymLink);
                        return currentInode;
                    }
                }
                // If the next component is not NULL, we need to resolve it
                else {
                    TracePrintf(0, "resolvePath: currentInode is INODE_SYMLINK and next component is not NULL, continue resolving\n");
                    currentInode = ResolveSymbolicLink(currentDir, inodeInfo, symlinkDepth + 1);
                }
            }
            // If we get either a regular file or a directory, then we proceed to the next component
            // Get the next component of the pathname
            else {
                TracePrintf(0, "resolvePath: currentInode not INODE_SYMLINK, getting next component\n");
                currentDir = currentInode;
                free(componentName);
                componentName = NULL;
                index = GetComponent(pathname, &componentName, index);
            }
        }
    }

    TracePrintf(0, "resolvePath: Finished resolving pathname, componentName is %s currentInode is %d\n", componentName, currentInode);
    free(componentName);
    return currentInode;
}

/**
 * Get the next component of the pathname starting from the given index
 * @param pathname The pathname to parse
 * @param componentName The name of the component to be filled
 * @param index The index to start from
 * @return The index of the next component to start from
 */
int GetComponent(char *pathname, char **componentName, int index) {
    TracePrintf(0, "GetComponent: Parsing component from pathname %s at index %d\n", pathname, index);
    // Skip leading slashes
    while (pathname[index] == '/') {
        index += 1;
    }

    // Find the end of the component
    int startIndex = index;
    while (pathname[index] != '/' && pathname[index] != '\0') {
        index += 1;
    }
    int endIndex = index;

    // Copy the component name
    int componentLength = endIndex - startIndex;
    if (componentLength > 0) {
        *componentName = (char*)calloc(componentLength + 1, sizeof(char));   // Plus one for the null terminator
        memcpy(*componentName, pathname + startIndex, componentLength);
    } else {
        *componentName = NULL;
    }

    TracePrintf(0, "GetComponent: componentName is now %s, index is now %d\n", *componentName, index);
    return index;
}

/**
 * Get the inode number of the component name in the directory
 * @param parentInode The parent inode to search in
 * @param componentName The name of the component to search for
 * @return The inode number of the component, or 0 if not found
 */
int GetInumByComponentName(struct InodeCacheEntry *parentInodeEntry, char *componentName) {
    TracePrintf(0, "GetInumByComponentName: Searching for component %s in parent %d\n", componentName, parentInodeEntry->inodeNumber);
    struct inode *parentInode = parentInodeEntry->inodeInfo;
    if (parentInode->type != INODE_DIRECTORY) {
        return ERROR;
    }

    int totalDirEntries = parentInode->size / sizeof(struct dir_entry);
    TracePrintf(0, "GetInumByComponentName: Total directory entries %d\n", totalDirEntries);
    
    // Traverse the directory entries
    for (int i = 0; i < totalDirEntries; i++) {
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
        void *blockData = GetBlockFromCache(blockNumber)->data;
        struct dir_entry *dirEntry = (struct dir_entry *)(blockData + (i % DIRENTRY_PER_BLOCK) * sizeof(struct dir_entry));
        if (dirEntry->inum > 0) {
            TracePrintf(0, "GetInumByComponentName: Entry %d - comparing %s with %s\n", i, dirEntry->name, componentName);
            size_t dirEntryNameLen = 0;
            while (dirEntryNameLen < DIRNAMELEN && dirEntry->name[dirEntryNameLen] != '\0') {
                dirEntryNameLen++;
            }
            if ((memcmp(dirEntry->name, componentName, strlen(componentName)) == 0) && (dirEntryNameLen == strlen(componentName))) {
                return dirEntry->inum;
            }
        }
    }
    // If the component name is not found, return 0
    TracePrintf(0, "GetInumByComponentName: Component %s not found\n", componentName);
    return 0;
}

/**
 * Get the data block number of specific index from the indirect block
 * @param indirectBlockNum The block number of the indirect block
 * @param index The index of the block in the indirect block
 * @return The block number, or ERROR if not found
 */
int GetDataBlockNumberFromIndirectBlock(int indirectBlockNum, int index) {
    TracePrintf(0, "GetDataBlockNumberFromIndirectBlock: Getting block number from indirect block %d at index %d\n", indirectBlockNum, index);
    if (indirectBlockNum == 0) {
        return ERROR;
    }
    // Get the block data from the cache
    void *blockData = GetBlockFromCache(indirectBlockNum)->data;
    // Return the block number at the specified index
    return ((int*)blockData)[index];
}

/**
 * Resolve the symbolic link to get the inode number
 * @param inum The directory where the symbolic link is found
 * @param inodeInfo The inode of the symbolic link
 * @param symlinkDepth The depth of the symbolic link resolution, should not exceed MAXSYMLINKS
 * @return The inode number of the target file, or ERROR if not found or too deep
 */
int ResolveSymbolicLink(int inum, struct inode *inodeInfo, int symlinkDepth) {
    TracePrintf(0, "ResolveSymbolicLink: Resolving symbolic link from inum %d, symlinkDepth is %d\n", inum, symlinkDepth);
    if (symlinkDepth > MAXSYMLINKS || inodeInfo->type != INODE_SYMLINK) {
        TracePrintf(0, "ResolveSymbolicLink: Too many symlinks or not a symlink\n");
        return ERROR;
    }

    // Read the symbolic link data
    // Since MAXPATHNAMELEN <= BLOCKSIZE, the entire symbolic link data can fit in one block at direct[0] (Slide p.7)
    void *blockData = GetBlockFromCache(inodeInfo->direct[0])->data;
    char *newPathName = calloc(inodeInfo->size + 1, sizeof(char));
    memcpy(newPathName, blockData, inodeInfo->size);
    TracePrintf(0, "ResolveSymbolicLink: Link target is '%s'\n", newPathName);

    int result = resolvePath(newPathName, inum, symlinkDepth + 1, 1);
    if (result == ERROR) {
        TracePrintf(0, "ResolveSymbolicLink: Error resolving symbolic link\n");
        free(newPathName);
        return ERROR;
    }

    // Resolve the new pathname recursively
    free(newPathName);
    return result;
}

/**
 * Parse the pathname to get the parent inode number
 * @param inum The inode number of the directory to start from
 * @param pathname The pathname to parse
 * @return The inode number of the parent directory, or ERROR if any error occurs
 */
int GetParentInum(int inum, char *pathname) {
    TracePrintf(0, "GetParentInum: Getting parent inode number from pathname %s\n", pathname);
    char *lastSlash = strrchr(pathname, '/');
    
    // If that slash is the first char in the string, its parent is the root directory
    // pathname looks like "/foo" (or just "/")
    if (lastSlash == pathname) {
        return ROOTINODE;
    }

    // No slash at all: parent is “current” inum
    if (lastSlash == NULL) {
        return inum;
    }
    // Temporarily replace the last slash with a null terminator to get the parent pathname
    // This is safe because the pathname is null-terminated
    *lastSlash = '\0';
    int parentInum = resolvePath(pathname, inum, 0, 1);
    *lastSlash = '/';
    return parentInum;
}

/**
 * Removes trailing slashes from a path string in-place
 * @param path Path string to be modified in-place
 */
static void TrimTrailingSlashes(char *path) {
    if (path == NULL) return; 

    char *end = path + strlen(path) - 1; 

    while (end >= path && *end == '/') {
        *end = '\0';  
        --end;      
    }
}

/**
 * Extracts the last component (filename or directory name) from a path
 * @param pathname Source path to parse
 * @param component Buffer to store the extracted component (must be pre-allocated)
 * @return 0 on success, ERROR if component name exceeds maximum length
 */
static int ExtractLastComponent(char *pathname, char *component) {
    if (pathname == NULL || component == NULL) {
        return ERROR;  // Safety check for null pointers
    }

    // Start from the end of the pathname and move backwards to find the last slash
    char *end = pathname + strlen(pathname);
    char *lastSlash = NULL;

    while (end != pathname) {
        --end;
        if (*end == '/') {
            lastSlash = end;
            break;
        }
    }

    // Set componentStart to the character after the last slash (or start of pathname)
    char *componentStart = lastSlash ? lastSlash + 1 : pathname;

    // Compute length and validate
    int componentLength = strlen(componentStart);
    if (componentLength > DIRNAMELEN) {
        TracePrintf(0, "ExtractLastComponent: Component '%s' exceeds maximum length\n", componentStart);
        return ERROR;
    }

    // Safe copy
    strncpy(component, componentStart, DIRNAMELEN - 1);
    component[DIRNAMELEN - 1] = '\0';  // Ensure null-termination

    return 0;
}


/**
 * Extracts the parent directory path from a path string
 * @param pathname Source path to parse
 * @param parentPath Buffer to store the parent path (must be pre-allocated)
 */
static void ExtractParentPath(char *pathname, char *parentPath) {
    if (pathname == NULL || parentPath == NULL) {
        return;  // Safety check
    }

    int length = strlen(pathname);
    if (length == 0) {
        parentPath[0] = '\0';  // Empty pathname
        return;
    }

    // Start from the end of pathname and move backwards to find the last slash
    char *end = pathname + length - 1;
    while (end >= pathname && *end != '/') {
        --end;
    }

    if (end < pathname) {
        // No slash found - relative path has no explicit parent
        parentPath[0] = '\0';
    } else if (end == pathname) {
        // Single slash at the start (e.g., "/file") - parent is root
        strcpy(parentPath, "/");
    } else {
        // Copy up to the last slash (excluding it)
        int parentLength = end - pathname;
        memcpy(parentPath, pathname, parentLength);
        parentPath[parentLength] = '\0';  // Null-terminate
    }
}


/**
 * Constructs a full path from a target path and a parent path
 * Handles both absolute and relative symlink targets
 * @param targetPath The symlink target path
 * @param parentPath The parent directory path
 * @param fullPath Buffer to store the constructed path (must be pre-allocated)
 */
static void ConstructFullPath(char *targetPath, char *parentPath, char *fullPath) {
    // Clear fullPath buffer
    memset(fullPath, 0, MAXPATHNAMELEN);

    // Handle absolute target path first
    if (targetPath[0] == '/') {
        // Direct copy for absolute path
        strncpy(fullPath, targetPath, MAXPATHNAMELEN - 1);
        return;
    }

    // If parent path is provided, concatenate parent and target
    if (parentPath[0] != '\0') {
        size_t parentLen = strlen(parentPath); 
        strncpy(fullPath, parentPath, MAXPATHNAMELEN - 1);

        // Add a '/' if not already present at the end of parent
        if (parentLen > 0 && fullPath[parentLen - 1] != '/' && parentLen < MAXPATHNAMELEN - 1) {
            fullPath[parentLen] = '/';
            parentLen++;
        }

        // Append the targetPath after parent
        strncat(fullPath, targetPath, MAXPATHNAMELEN - parentLen - 1);
    } else {
        // No parent provided, just copy targetPath
        strncpy(fullPath, targetPath, MAXPATHNAMELEN - 1);
    }

    // Ensure null termination (though memset already clears the buffer)
    fullPath[MAXPATHNAMELEN - 1] = '\0';
}


/**
 * Locates the parent directory inode for a path and extracts the final component
 * Handles path normalization and symbolic link resolution
 * @param pathname Path to analyze
 * @param currentDirectory Starting directory inode number
 * @param component Buffer to store the final path component (must be pre-allocated)
 * @return Inode number of parent directory on success, ERROR on failure
 */
int CreateFindParent(char *pathname, int currentDirectory, char *component) {
    TracePrintf(0, "CreateFindParent: Analyzing path '%s' from directory %d\n", 
                pathname, currentDirectory);
    
    // Step 1: Normalize the path by removing trailing slashes
    TrimTrailingSlashes(pathname);
    
    // Step 2: Extract the final component (filename/dirname)
    if (ExtractLastComponent(pathname, component) == ERROR) {
        TracePrintf(0, "CreateFindParent: Failed to extract last component\n");
        return ERROR;
    }
    
    // Step 3: Extract the parent path
    char parentPath[MAXPATHNAMELEN];
    ExtractParentPath(pathname, parentPath);
    
    // Step 4: Resolve the parent directory inode
    TracePrintf(0, "CreateFindParent: Resolving parent path '%s'\n", parentPath);
    
    // If parent path is empty, use current directory as parent
    int parentInum;
    if (parentPath[0] == '\0') {
        parentInum = currentDirectory;
    } else {
        parentInum = resolvePath(parentPath, currentDirectory, 0, 1);
    }
    
    // Check if parent resolution failed
    if (parentInum <= 0) {
        TracePrintf(0, "CreateFindParent: Failed to resolve parent path\n");
        return ERROR;
    }
    
    // Step 5: Get the parent directory inode entry
    InodeCacheEntry *parentEntry = GetInodeFromCache(parentInum);
    if (!parentEntry) {
        TracePrintf(0, "CreateFindParent: Failed to retrieve parent inode %d\n", parentInum);
        return ERROR;
    }
    
    // Step 6: Check if the component already exists in parent directory
    int existingInum = GetInumByComponentName(parentEntry, component);
    
    // Step 7: Handle symbolic link resolution if needed
    if (existingInum > 0) {
        InodeCacheEntry *existingEntry = GetInodeFromCache(existingInum);
        
        // If the component is a symlink, resolve it recursively
        if (existingEntry && existingEntry->inodeInfo->type == INODE_SYMLINK) {
            // Read the symlink target
            char targetPath[MAXPATHNAMELEN];
            int targetLength = existingEntry->inodeInfo->size;
            
            // Ensure the target length is within bounds
            if (targetLength >= MAXPATHNAMELEN) {
                targetLength = MAXPATHNAMELEN - 1;
            }
            
            // Get the block containing the symlink target
            BlockCacheEntry *targetBlock = GetBlockFromCache(existingEntry->inodeInfo->direct[0]);
            if (!targetBlock) {
                TracePrintf(0, "CreateFindParent: Failed to read symlink target\n");
                return ERROR;
            }
            
            // Copy the target path and ensure null termination
            memcpy(targetPath, targetBlock->data, targetLength);
            targetPath[targetLength] = '\0';
            
            // Construct the full path and resolve recursively
            char fullPath[MAXPATHNAMELEN];
            ConstructFullPath(targetPath, parentPath, fullPath);
            
            TracePrintf(0, "CreateFindParent: Following symlink from '%s' to '%s'\n", 
                        component, fullPath);
            return CreateFindParent(fullPath, currentDirectory, component);
        }
    }
    
    // Return the parent inode number
    return parentInum;
}
/**
 * Get the filename from the pathname (last component before the last slash)
 * @param pathname The pathname to parse
 * @return The filename, or NULL if not found
 */
char* getFilename(char* pathname) {
	TracePrintf(1, "getFilename: Getting filename from %s\n", pathname);
    for (int i = strlen(pathname) - 1; i >= 0; i--) {
		if (pathname[i] == '/') {
            TracePrintf(1, "getFilename: Found filename %s\n", pathname + i + 1);
			return pathname + i + 1;
		}
    }
    // If no slash is found, return the entire pathname
    TracePrintf(1, "getFilename: No slash found, returning pathname %s\n", pathname);
    return pathname;
}

/**
 * Get the block number from the indirect block at specific index
 * @param indirectBlockNum The block number of the indirect block
 * @param index The index of the block in the indirect block
 * @return The block number, or ERROR if not found
 */
int GetBlockNumberFromIndirectBlock(int indirectBlockNum, int index) {
    TracePrintf(0, "GetBlockNumberFromIndirectBlock: Getting block number from indirect block %d at index %d\n", indirectBlockNum, index);
    if (indirectBlockNum == 0) {
        return ERROR;
    }
    // Get the block data from the cache
    void *blockData = GetBlockFromCache(indirectBlockNum)->data;
    // Return the block number at the specified index
    return ((int*)blockData)[index];
}

/**
 * Remove the directory entry for a file from the parent directory
 * @param fileInum The inode number of the file to remove
 * @param filename The name of the file to remove
 * @param parentInodeEntry The inode cache entry of the parent directory
 * @return 0 on success, or ERROR if not found
 */
int RemoveEntryFromDir(int fileInum, char *filename, struct InodeCacheEntry *parentInodeEntry) {
    TracePrintf(0, "RemoveEntryFromDir: Removing entry %s with inum %d from parent inode %d\n", filename, fileInum, parentInodeEntry->inodeNumber);
    struct inode *parentInode = parentInodeEntry->inodeInfo;
    if (parentInode->type != INODE_DIRECTORY) {
        return ERROR;
    }

    int totalDirEntries = parentInode->size / sizeof(struct dir_entry);
    parentInodeEntry->isDirty = 1;
    for (int i = 0; i < totalDirEntries; i++) {
        // Get the block number of this directory entry
        // see what block the dir_entry is in
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
        struct BlockCacheEntry *blockEntry = GetBlockFromCache(blockNumber);
        if (blockEntry == NULL) {
            return ERROR;
        }
        void *blockData = blockEntry->data;
        struct dir_entry *dirEntry = (struct dir_entry *)(blockData + (i % DIRENTRY_PER_BLOCK) * sizeof(struct dir_entry));

        // found the file with the same inum and name
        /*
        As file names are removed from the directory (on an Unlink or a RmDir), 
        the corresponding directory entry is modified so that its inum field is 0.
        The directory entry is then said to be free.
        */
        if (dirEntry->inum == fileInum) {
            TracePrintf(0, "RemoveEntryFromDir: Found entry %s with inum %d at index %d\n", filename, fileInum, i);
            dirEntry->inum = 0;
            memset(dirEntry->name, 0, DIRNAMELEN);
            blockEntry->isDirty = 1;
            return 0;
        }
    }
    return ERROR;
}

/**
 * Parse the given pathname to remove consecutive slashes and normalize it
 * The trailing slash is treated as if followed by '.'
 * @param originalPath The original pathname to parse
 * @return 0 on success, ERROR on failure
 */
int resolveTrailingSlash(char *originalPath) {

    TracePrintf(0, "checkTrailingSlash: Original path is %s\n", originalPath);
    
    // check if the originalPath is NULL or empty
    if (originalPath == NULL || strlen(originalPath) == 0) {
        TracePrintf(0, "parsePath: Empty pathname\n");
        return ERROR;
    }
    
    int originalLen = strlen(originalPath);

    // handle trailing '/', treat it as if followed by '.'
    if (originalPath[originalLen-1] == '/') {
        strcat(originalPath, ".");
    }
    
    TracePrintf(0, "checkTrailingSlash: Normalized path is %s\n", originalPath);
    
    return 0;
}

/**
 * Check if the process's cwdReuse is the same as the server side's reuse.
 * I.e., to check if the CWD of a process does not change after it ChDir
 * ONLY CHECK if the pathname is a relative pathname
 * @param pathname The pathname the process is trying to do operation on
 * @param currentWorkingDirectory The current working directory of the process
 * @param cwdReuse The reuse count of the current working directory
 * @return 0 if the reuse count is the same or the path is absolute, ERROR otherwise
 */
int verifyCwdReuse(char* pathname, int currentWorkingDirectory, int cwdReuse) {
    TracePrintf(0, "verifyCwdReuse: currentWorkingDirectory is %d, cwdReuse is %d\n", currentWorkingDirectory, cwdReuse);
    if (pathname == NULL) {
        return ERROR;
    }

    // If the pathname is absolute, we don't need to check the reuse count
    if (pathname[0] == '/') {
        return 0;
    }
    // If the pathname is relative, we need to check the reuse count
    struct InodeCacheEntry* currentInodeEntry = GetInodeFromCache(currentWorkingDirectory);
    if (currentInodeEntry == NULL) {
        return ERROR;
    }
    struct inode* inodeInfo = currentInodeEntry->inodeInfo;
    TracePrintf(0, "verifyCwdReuse: currentInodeEntry is %d, inodeInfo->reuse is %d, cwdReuse is %d\n", 
        currentInodeEntry->inodeNumber, inodeInfo->reuse, cwdReuse);
    if (inodeInfo->reuse != cwdReuse) {
        TracePrintf(0, "verifyCwdReuse: reuse count does not match\n");
        return ERROR;
    }
    TracePrintf(0, "verifyCwdReuse: reuse count matches\n");
    return 0;
}