#include <stdio.h>
#include <comp421/hardware.h>
#include <comp421/loadinfo.h>
#include <comp421/yalnix.h>

int main() {
    printf("Init starting, pid = %d\n", GetPid());

    // Generate some large stuff
    char large_array[1000];
    for (int i = 0; i < 1000; i++) {
        large_array[i] = 'a';
    }
    printf("Init process with large array: %s\n", large_array);

    int pid = Fork();
    if (pid == 0) {
        // In child process
        printf("Child process about to Exec 'second'\n");
        char *args[] = { "second", NULL };
        Exec("second", args);
        // Exec only returns if there is an error
        printf("Exec failed!\n");
        Exit(1);
    }
    else if (pid > 0)
    {
        // In parent (init) process
        printf("Parent waiting for child (pid = %d)\n", pid);
        int status;
        int child = Wait(&status);
        printf("Child (pid = %d) exited with status %d\n", child, status);
    }
    else
    {
        printf("Fork failed!\n");
    }

    printf("Init process finished waiting for its child!\n");

    for (int i = 0; i < 5; i++) {
        printf("Init process shouts more %d\n", i);
        Pause();
    }

    printf("Init process exiting\n");
    Exit(0);
    return 0;
}
