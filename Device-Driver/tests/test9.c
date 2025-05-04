//large buffer handling
//ensuring large writes are handled correctly without data loss.


#include <stdio.h>
#include <stdlib.h>
#include <threads.h>
#include <terminals.h>

#define BUFFER_SIZE 1024

void big_writer(void *);
void wait_for_esc();

char large_string[BUFFER_SIZE * 2];

int main() {
    for (size_t i = 0; i < sizeof(large_string) - 1; i++) {
        large_string[i] = 'A' + (i % 26);
    }
    large_string[sizeof(large_string) - 1] = '\n'; // End with newline

    InitTerminalDriver();
    InitTerminal(1);

    ThreadCreate(big_writer, (void *)1);

    ThreadWaitAll();
    wait_for_esc();
    exit(0);
}

void big_writer(void *arg) {
    int term = (__intptr_t)arg;
    int status = WriteTerminal(term, large_string, sizeof(large_string));
    if (status != sizeof(large_string))
        fprintf(stderr, "Error: big_writer wrote %d bytes, expected %ld\n", status, sizeof(large_string));
}

void wait_for_esc() {
    int ch;
    printf("Press ESC to exit...\n");
    fflush(stdout);
    while ((ch = getchar()) != 27) { // 27 is the ASCII code for ESC
        // Do nothing, just wait for ESC key
    }
}
