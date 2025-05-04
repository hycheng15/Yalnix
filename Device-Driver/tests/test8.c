// ensures echoing of user input immediately

#include <stdio.h>
#include <stdlib.h>
#include <threads.h>
#include <terminals.h>

void echo_tester(void *);

int main() {
    InitTerminalDriver();
    InitTerminal(1);

    ThreadCreate(echo_tester, (void *)1);

    ThreadWaitAll();
    exit(0);
}

void echo_tester(void *arg) {
    int term = (__intptr_t)arg;
    char buf[100];

    printf("Type characters. They should appear immediately. Press Enter to end:\n");
    int len = ReadTerminal(term, buf, sizeof(buf));
    printf("Read %d bytes: %.*s\n", len, len, buf);
}
