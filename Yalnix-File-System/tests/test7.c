#include <comp421/yalnix.h>
#include <comp421/iolib.h>
#include <stdio.h>
#include <stdlib.h>

int
main()
{
    int res;
    int fd;

    printf("=== Directory‐Reuse Test ===\n");

    // 1) Make a directory “d”
    res = MkDir("d");
    printf("MkDir(\"d\"): %d\n", res);

    // 2) cd into it
    res = ChDir("d");
    printf("ChDir(\"d\"): %d\n", res);

    // 3) Create a file “f” inside d
    fd = Create("f");
    printf("Create(\"f\"): fd=%d\n", fd);
    if (fd >= 0) Close(fd);

    // 3.5) Unlink the file “f” inside d
    Unlink("f"); // remove the file

    res = RmDir("/d"); // remove the directory
    fd = Create("f2");
    printf("Create(\"f2\"): fd=%d\n", fd);

    // 4) Cleanly shut everything down
    Shutdown();
    return 0;
}