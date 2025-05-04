#include <stdio.h>
#include <comp421/yalnix.h>
#include <comp421/iolib.h>

int main() {
    int status;
    
    printf("=== Testing operations after removing current working directory ===\n\n");
    
    // 1. Create a directory
    printf("1. Creating directory /testdir\n");
    status = MkDir("/testdir");
    printf("   MkDir(\"/testdir\") result: %d\n", status);
    
    // 2. Change into the directory
    printf("\n2. Changing directory to /testdir\n");
    status = ChDir("/testdir");
    printf("   ChDir(\"/testdir\") result: %d\n", status);
    
    // 3. Remove the current working directory using absolute path
    printf("\n3. Removing current working directory using absolute path\n");
    status = RmDir("/testdir");
    printf("   RmDir(\"/testdir\") result: %d\n", status);
    
    // 4. Try to create a file using relative path
    printf("\n4. Attempting to create a file with relative path in removed directory\n");
    int fd = Create("newfile.txt");
    printf("   Create(\"newfile.txt\") result: %d\n", fd);
    
    if (fd >= 0) {
        printf("   Unexpected success! Writing to the file...\n");
        status = Write(fd, "This shouldn't work", 19);
        printf("   Write result: %d\n", status);
        Close(fd);
    }

    else {
        printf("   Create failed as expected, no file created.\n");
    }
    
    printf("\n=== Test complete ===\n");
    Shutdown();
    
    return 0;
}