#include <stdio.h>
#include <stdlib.h>
#include <comp421/yalnix.h>
#include <comp421/iolib.h>

void PrintResult(const char* label, int result) {
    if (result == ERROR) {
        printf("%s failed\n", label);
    } else {
        printf("%s returned %d\n", label, result);
    }
}

int main() {
    int fd = Create("/seektest");
    if (fd == ERROR) {
        printf("Failed to create file /seektest\n");
        return 1;
    }
    printf("Created /seektest with fd %d\n", fd);

    // Write some data
    char* data = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    int len = 26;
    int nch = Write(fd, data, len);
    printf("Wrote %d bytes\n", nch);

    // SEEK_SET test: Go to beginning
    int pos = Seek(fd, 0, SEEK_SET);
    PrintResult("Seek SEEK_SET", pos);

    char read_buf[27];
    int r = Read(fd, read_buf, 26);
    read_buf[r] = '\0';
    printf("Read after SEEK_SET: %s\n", read_buf);

    // SEEK_CUR test: Move forward 5 bytes
    Seek(fd, 0, SEEK_SET);
    Read(fd, read_buf, 5); // move 5 bytes forward
    pos = Seek(fd, 3, SEEK_CUR);
    PrintResult("Seek SEEK_CUR (+3)", pos);

    r = Read(fd, read_buf, 5);
    read_buf[r] = '\0';
    printf("Read after SEEK_CUR: %s\n", read_buf);

    // SEEK_END test: Move 0 bytes from EOF
    pos = Seek(fd, 0, SEEK_END);
    PrintResult("Seek SEEK_END", pos);

    // Seek beyond EOF: go 100 bytes past EOF
    pos = Seek(fd, 100, SEEK_END);
    PrintResult("Seek SEEK_END + 100", pos);

    // Write something at the new position
    char* hole_data = "XYZ";
    nch = Write(fd, hole_data, 3);
    printf("Wrote %d bytes at position %d (past EOF)\n", nch, pos);

    // Seek back to where we wrote and read back
    Seek(fd, -3, SEEK_CUR);
    char verify[4];
    r = Read(fd, verify, 3);
    verify[r] = '\0';
    printf("Read after hole write: %s\n", verify);

    // Now try reading from one of the holes (between old EOF and new write)
    Seek(fd, 20, SEEK_SET);
    char check_hole[200];
    r = Read(fd, check_hole, 110);
    check_hole[r] = '\0';
    printf("Read from hole (should be zero-filled): ");
    int null_count = 0;
    for (int i = 0; i < r; i++) {
        printf("%02x ", (unsigned char)check_hole[i]);
        if (check_hole[i] == '\0') {
            null_count++;
        }
    }
    printf("\n");
    printf("Number of null bytes: %d\n", null_count);

    Close(fd);
    Shutdown();
    return 0;
}