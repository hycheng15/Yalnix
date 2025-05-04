#include <comp421/hardware.h>
#include <comp421/yalnix.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TTY_WRITE_STR(term, str) TtyWrite(term, str, strlen(str))

int main() {
    int i;

    for (i = 0; i < 10; i++)
        TTY_WRITE_STR(0, "Hello world\n");

    Exit(0);
}
