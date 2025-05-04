#include <comp421/yalnix.h>
#include <comp421/hardware.h>
#include <stdio.h>
#include <stdlib.h>

#define NUM_CHILDREN 5

int main(int argc, char **argv) {
    int i, pid;
    int status;

    printf("Parent process starting (pid=%d)\n", GetPid());

    // Fork NUM_CHILDREN children.
    for (i = 0; i < NUM_CHILDREN; i++) {
        pid = Fork();
        if (pid == 0) { 
            // Child process
            printf("Child process (pid=%d) created in first batch\n", GetPid());
            Delay(1);
            Exit(0);
        } else {
            printf("Parent: forked child with pid=%d\n", pid);
        }
    }

    // Parent waits for all first batch children to exit.
    for (i = 0; i < NUM_CHILDREN; i++) {
        pid = Wait(&status);
        printf("Parent: collected child (pid=%d) with status=%d\n", pid, status);
    }

    printf("First batch complete. Now forking new children to test slot re-use.\n");

    // Fork another batch of children
    for (i = 0; i < NUM_CHILDREN; i++) {
        pid = Fork();
        if (pid == 0) { 
            // Child process
            printf("New child process (pid=%d) created in second batch\n", GetPid());
            Delay(1);
            Exit(0);
        } else {
            printf("Parent: forked new child with pid=%d\n", pid);
        }
    }

    // Parent waits for all second batch children to exit.
    for (i = 0; i < NUM_CHILDREN; i++) {
        pid = Wait(&status);
        printf("Parent: collected new child (pid=%d) with status=%d\n", pid, status);
    }

    printf("Parent process finishing.\n");
    Exit(0);
}