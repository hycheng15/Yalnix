#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <comp421/yalnix.h>
#include <comp421/iolib.h>

void PrintStat(const char* label, struct Stat* sb) {
    printf("%s:\n", label);
    printf("  Inode Number: %d\n", sb->inum);
    printf("  Type: %d\n", sb->type);
    printf("  Size: %d bytes\n", sb->size);
    printf("  Link Count: %d\n", sb->nlink);
}

int main() {
    const char* filepath = "/stat_test_file";
    const char* content = "Hello, Stat!";
    int fd, written;

    // Create file
    fd = Create((char*)filepath);
    if (fd == ERROR) {
        printf("Error: Failed to create file %s\n", filepath);
        return 1;
    }
    printf("Created file %s with fd %d\n", filepath, fd);

    // Write content
    written = Write(fd, (void*)content, strlen(content));
    if (written < 0) {
        printf("Error: Write failed\n");
        return 1;
    }
    printf("Wrote %d bytes to file\n", written);
    Close(fd);

    // Call Stat
    struct Stat sb;
    int result = Stat((char*)filepath, &sb);
    if (result == ERROR) {
        printf("Error: Stat failed on %s\n", filepath);
        return 1;
    }
    PrintStat("Stat result", &sb);

    // Call Sync
    printf("Calling Sync()...\n");
    int syncStatus = Sync();
    printf("Sync completed with %d.\n", syncStatus);

    // Call Shutdown
    printf("Calling Shutdown()...\n");
    int shutdownStatus = Shutdown();
    printf("FS should shut down now with status %d.\n", shutdownStatus);
}