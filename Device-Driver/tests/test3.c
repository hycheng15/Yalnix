#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include <terminals.h>

void writer(void *);

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    InitTerminalDriver();
    InitTerminal(1);

    ThreadCreate(writer, NULL);

    ThreadWaitAll();
    while (1) {}
    exit(0);
}


void
writer(void *dummy)
{
    int i, j, n;
    int status;
    char buf[128];
    char *cp;
    int len;

    (void)dummy;

    n = 0;
    for (i = 0; i < 10; i++)
    {
	cp = buf;
	for (j = 0; j < 5; j++)
	{
	    sprintf(cp, "%2d... ", ++n);
	    cp += strlen(cp);
	}
	strcpy(cp, "\n");
	len = cp - buf + 1;
	status = WriteTerminal(1, buf, len);
	if (status != len)
	    fprintf(stderr, "Error: writer status = %d, len = %d\n",
		status, len);
    }
}
