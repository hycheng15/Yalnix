//testing backspace and deleting character

#include <stdio.h>
#include <stdlib.h>
#include <threads.h>
#include <terminals.h>

void backspace_tester(void *);

int main() {
    InitTerminalDriver();
    InitTerminal(1);

    ThreadCreate(backspace_tester, (void *)1);

    ThreadWaitAll();
    exit(0);
}

void backspace_tester(void *arg) {
    int term = (__intptr_t)arg;
    char buf[50];

    printf("Type something and use backspace. Press Enter when done:\n");
    int len = ReadTerminal(term, buf, sizeof(buf));
    printf("Read %d bytes: %.*s\n", len, len, buf);
}
