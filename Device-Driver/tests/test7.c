// Test Simultaneous Reads and Writes
// ensuring writes do not interfere with input handling.

#include <stdio.h>
#include <stdlib.h>
#include <threads.h>
#include <terminals.h>

void writer(void *);
void reader(void *);

char string1[] = "Writing while reading...\n";
int length1 = sizeof(string1) - 1;

int main() {
    InitTerminalDriver();
    InitTerminal(1);

    ThreadCreate(writer, (void *)1);
    ThreadCreate(reader, (void *)1);
    while(1) {}
    ThreadWaitAll();
    exit(0);
}

void writer(void *arg) {
    int term = (__intptr_t)arg;
    WriteTerminal(term, string1, length1);
}

void reader(void *arg) {
    int term = (__intptr_t)arg;
    char buf[2];

    printf("Type something while writing occurs:\n");
    int len = ReadTerminal(term, buf, sizeof(buf));
    printf("Read %d bytes: %.*s\n", len, len, buf);
}
