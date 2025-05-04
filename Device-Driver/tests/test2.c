#include <stdio.h>
#include <stdlib.h>
#include <threads.h>
#include <terminals.h>

void writer(void *);

char string[] = "abcdefghijklmnopqrstuvwxyz";
int length = sizeof(string) - 1;

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    InitTerminalDriver();
    InitTerminal(1);

    ThreadCreate(writer, NULL);

    ThreadWaitAll();
    while(1) {}
    exit(0);
}

void
writer(void *arg)
{
    int status;

    (void)arg;

    printf("Doing WriteTerminal... '");
    fflush(stdout);
    status = WriteTerminal(1, string, length);
    printf("'. Done: status = %d.\n", status);
    fflush(stdout);
}
