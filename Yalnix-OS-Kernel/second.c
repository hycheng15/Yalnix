#include <stdio.h>
#include <comp421/hardware.h>
#include <comp421/loadinfo.h>
#include <comp421/yalnix.h>

int main() {
    int pid = GetPid();
    printf("Forked process with pid: %d\n", pid);
    int i = 0;
    while (1) {
        printf("PID %d shouts: AHHHH * %d\n", pid, i++);
        if (i == 2) Delay(10);
        if (i == 1000) Exit(0);
    }
    return 0;
}
