//Ensure WriteTerminal() works independently for multiple terminals.
// testing concurrent write to different terminals

#include <stdio.h>
#include <stdlib.h>
#include <threads.h>
#include <terminals.h>

void writer1(void *);
void writer2(void *);
void wait_for_esc();

char string1[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ\n";
int length1 = sizeof(string1) - 1;

char string2[] = "abcdefghij\n";
int length2 = sizeof(string2) - 1;

int main() {
    InitTerminalDriver();
    InitTerminal(1);
    InitTerminal(2);

    ThreadCreate(writer1, (void *)1);
    ThreadCreate(writer2, (void *)2);

    ThreadWaitAll();
    wait_for_esc();
    exit(0);
}

void writer1(void *arg) {
    int term = (__intptr_t)arg;
    int status = WriteTerminal(term, string1, length1);
    if (status != length1)
        fprintf(stderr, "Error: writer1 status = %d, length1 = %d\n", status, length1);
}

void writer2(void *arg) {
    int term = (__intptr_t)arg;
    int status = WriteTerminal(term, string2, length2);
    if (status != length2)
        fprintf(stderr, "Error: writer2 status = %d, length2 = %d\n", status, length2);
}

void wait_for_esc() {
    int ch;
    printf("Press ESC to exit...\n");
    fflush(stdout);
    while ((ch = getchar()) != 27) { // 27 is the ASCII code for ESC
        // Do nothing, just wait for ESC key
    }
}