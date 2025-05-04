#include <stdio.h>
#include <comp421/yalnix.h>
#include <comp421/iolib.h>
#include <comp421/filesystem.h>
#include <stdlib.h>
#include <string.h>

int main() {
    int status, fd1, fd2;
    char buffer1[100], buffer2[100];
    struct Stat stat_buf1, stat_buf2;

    printf("=== Link Function Test ===\n");

    // 1. Create original file
    fd1 = Create("/file1");
    if (fd1 == ERROR) {
        printf("Error: Unable to create /file1\n");
        Exit(1);
    }
    printf("Successfully created file /file1, file descriptor: %d\n", fd1);

    // 2. Write content to the original file
    const char *test_content = "This is test content for link file";
    status = Write(fd1, (void *)test_content, strlen(test_content));
    printf("Write to /file1 status: %d\n", status);
    
    // Reposition file pointer to beginning
    // Seek(fd1, 0, SEEK_SET);

    // 3. Create hard link
    printf("\nAttempting to create link /link1 -> /file1...\n");
    status = Link("/file1", "/link1");
    printf("Link status: %d (0 means success, ERROR means failure)\n", status);

    // 4. Get Stat information through original file and linked file
    if (Stat("/file1", &stat_buf1) == ERROR) {
        printf("Error: Unable to get status for /file1\n");
    } else {
        printf("\n/file1 information:\n");
        printf("  inode number: %d\n", stat_buf1.inum);
        printf("  type: %d (2=file, 1=directory)\n", stat_buf1.type);
        printf("  size: %d bytes\n", stat_buf1.size);
        printf("  link count: %d\n", stat_buf1.nlink);
    }

    if (Stat("/link1", &stat_buf2) == ERROR) {
        printf("Error: Unable to get status for /link1\n");
    } else {
        printf("\n/link1 information:\n");
        printf("  inode number: %d\n", stat_buf2.inum);
        printf("  type: %d (2=file, 1=directory)\n", stat_buf2.type);
        printf("  size: %d bytes\n", stat_buf2.size);
        printf("  link count: %d\n", stat_buf2.nlink);
    }
    
    // 5. Check if both point to the same inode
    if (stat_buf1.inum == stat_buf2.inum) {
        printf("\nTest successful: /file1 and /link1 point to the same inode, link created successfully\n");
    } else {
        printf("\nTest failed: /file1 and /link1 point to different inodes\n");
    }
    
    // 6. Open linked file and read content
    printf("\nAttempting to read file content through link...\n");
    fd2 = Open("/link1");
    if (fd2 == ERROR) {
        printf("Error: Unable to open /link1\n");
    } else {
        printf("Successfully opened /link1, file descriptor: %d\n", fd2);
        
        // Read linked file content
        memset(buffer2, 0, sizeof(buffer2));
        status = Read(fd2, buffer2, sizeof(buffer2));
        printf("Read from /link1 status: %d\n", status);
        printf("Content read: \"%s\"\n", buffer2);
        
        // Read original file content for comparison
        memset(buffer1, 0, sizeof(buffer1));
        printf("fd1 is %d\n", fd1);
        Seek(fd1, 0, SEEK_SET);
        status = Read(fd1, buffer1, sizeof(buffer1));
        printf("Read from /file1 status: %d\n", status);
        printf("Content read: \"%s\"\n", buffer1);
        
        // Compare if content is the same
        if (strcmp(buffer1, buffer2) == 0) {
            printf("Test successful: Same content read through both paths\n");
        } else {
            printf("Test failed: Content inconsistent\n");
            printf("Original file content: \"%s\"\n", buffer1);
            printf("Linked file content: \"%s\"\n", buffer2);
        }
        
        Close(fd2);
    }
    
    // 7. Test error case: linking to directory
    printf("\nTesting error case: Attempting to link to directory...\n");
    status = MkDir("/testdir");
    printf("Create directory /testdir status: %d\n", status);
    
    status = Link("/testdir", "/dirlink");
    printf("Link to directory status: %d (should return error)\n", status);
    
    // 8. Test error case: linking to existing file
    printf("\nTesting error case: Attempting to link to existing file...\n");
    fd2 = Create("/existing");
    Close(fd2);
    
    status = Link("/file1", "/existing");
    printf("Link to existing file status: %d (should return error)\n", status);
    
    // Clean up resources
    Close(fd1);
    
    printf("\n=== Link Test Complete ===\n");
    Shutdown();
    return 0;
}