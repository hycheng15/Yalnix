#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <comp421/hardware.h>
#include <comp421/yalnix.h>

char line[TERMINAL_MAX_LINE];

int main() {
    int i;
    int pid;

    if ((pid = Fork()) < 0) {
        fprintf(stderr, "Can't Fork!\n");
        Exit(1);
    }

    if (pid != 0) {
        for (i = 0; i < 10; i++) {
            sprintf(line, "Parent line %d\n", i);
            TtyWrite(0, line, strlen(line));
        }
    } else {
        for (i = 0; i < 10; i++) {
            sprintf(line, "Child line %d\n", i);
            TtyWrite(0, line, strlen(line));
        }
    }

    Exit(0);
}
