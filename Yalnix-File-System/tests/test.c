#include <stdio.h>
#include <stdlib.h>
#include <comp421/yalnix.h>
#include <comp421/iolib.h>

int main() {
    printf("Starting test program\n");
    int fd = Create("/foo");
    if (fd == ERROR) {
        printf("Error creating file /foo\n");
        return 1;
    }
    printf("File /foo created with fd: %d\n", fd);

    int status = Close(fd);
    printf("Close status %d\n", status);

    fd = Open("/foo");
    if (fd == ERROR) {
        printf("Error opening file /foo\n");
        return 1;
    }
    printf("File /foo open with fd: %d\n", fd);

    // Create a large buffer
    printf("Creating a large buffer\n");
    char buf[10000];
    for (int i = 0; i < 10000; i++) {
        buf[i] = 'a' + (i % 26);
    }

    int nch = Write(fd, buf, 10000);
    if (nch < 0) {
        perror("Write");
        Close(fd);
        return 1;
    }

    fd = Open("/foo");
    printf("Open fd %d\n", fd);

    char read_buf[10000];
    nch = Read(fd, read_buf, 10000);
    if (nch < 0) {
        perror("Read");
        Close(fd);
        return 1;
    }
    printf("Read %d bytes\n", nch);
    status = Close(fd);
    printf("Close status %d\n", status);

    printf("mkdir /a\n");
    printf("%d\n", MkDir("/a"));

    printf("\nmkdir /a/b\n");
    printf("%d\n", MkDir("/a/b"));

    printf("\nsymlink /a/b/c -> d/f.txt\n");
    printf("%d\n", SymLink("d/f.txt", "/a/b/c"));

    printf("\nmkdir /a/b/d\n");
    printf("%d\n", MkDir("/a/b/d"));

    printf("\ncreate /a/b/c/\n");
    printf("%d\n", Create("/a/b/c/"));

    Shutdown();
}