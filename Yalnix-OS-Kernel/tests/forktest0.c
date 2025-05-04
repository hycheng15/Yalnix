#include <comp421/hardware.h>
#include <comp421/yalnix.h>

int main(int argc, char** argv) {
    if (Fork() == 0) {
        TracePrintf(0, "CHILD\n");
    } else {
        TracePrintf(0, "PARENT\n");
        Delay(8);
    }

    Exit(0);
}
