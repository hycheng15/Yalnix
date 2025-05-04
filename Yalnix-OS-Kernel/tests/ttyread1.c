#include <comp421/hardware.h>
#include <comp421/yalnix.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char line[TERMINAL_MAX_LINE];

int main() {
    int len;

    fprintf(stderr, "Doing TtyRead...\n");
    len = TtyRead(0, line, sizeof(line));
    fprintf(stderr, "Done with TtyRead: len %d\n", len);
    fprintf(stderr, "line '");
    fwrite(line, len, 1, stderr);
    fprintf(stderr, "'\n");

    Exit(0);
}
