// Ensure input is read correctly and follows the line-based approach.
// testing readTerminal Behavior

#include <stdio.h>
#include <stdlib.h>
#include <threads.h>
#include <terminals.h>

void reader(void *);
void wait_for_esc();

int main() {
    InitTerminalDriver();
    InitTerminal(1);

    ThreadCreate(reader, (void *)1);

    ThreadWaitAll();
    wait_for_esc();
    exit(0);
}

void reader(void *arg) {
    int term = (__intptr_t)arg;
    char buf[2];

    printf("Type something and press Enter:\n");
    while (1) 
    {
        int len = ReadTerminal(term, buf, sizeof(buf));
        printf("Read %d bytes: %.*s\n", len, len, buf);
    }
}

void wait_for_esc() {
    int ch;
    printf("Press ESC to exit...\n");
    fflush(stdout);
    while ((ch = getchar()) != 27) { // 27 is the ASCII code for ESC
        // Do nothing, just wait for ESC key
    }
}
