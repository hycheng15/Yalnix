/*
 * ttytest.c - Test the Yalnix terminal I/O system
 */

#include <comp421/hardware.h>
#include <comp421/yalnix.h>
#include <stdio.h>
#include <string.h>

#define BUFFER_SIZE TERMINAL_MAX_LINE
#define TEST_ITERATIONS 3
#define TERMINAL_DELAY 1  // Delay constant

char buffer[BUFFER_SIZE];

void test_basic_io(int tty_id);
void test_nonblocking_read(int tty_id);
void test_concurrent_terminals();
void test_large_write(int tty_id);

int main() {
    int pid;

    TtyWrite(TTY_CONSOLE, "=== Yalnix Terminal I/O Test Program ===\n", 41);
    Delay(TERMINAL_DELAY);

    // Test 1: Basic read/write operations
    TtyWrite(TTY_CONSOLE, "\n== Test 1: Basic Read/Write Operations ==\n", 42);
    Delay(TERMINAL_DELAY);

    test_basic_io(TTY_CONSOLE);

    // Test 2: Test multiple terminals
    TtyWrite(TTY_CONSOLE, "\n== Test 2: Multiple Terminal Test ==\n", 37);
    Delay(TERMINAL_DELAY);

    // Create a child process for each terminal
    for (int i = 0; i < NUM_TERMINALS; i++) {
        pid = Fork();

        if (pid == 0) {  // Child process
            int my_pid = GetPid();
            snprintf(buffer, BUFFER_SIZE,
                     "Child process (PID: %d) running on terminal %d\n", my_pid,
                     i);
            TtyWrite(TTY_CONSOLE, buffer, strlen(buffer));
            Delay(TERMINAL_DELAY);

            test_basic_io(i);

            snprintf(buffer, BUFFER_SIZE,
                     "Child process (PID: %d) completed test on terminal %d\n",
                     my_pid, i);
            TtyWrite(TTY_CONSOLE, buffer, strlen(buffer));
            Delay(TERMINAL_DELAY);

            Exit(0);
        }
    }

    // Wait for all child processes to complete
    for (int i = 0; i < NUM_TERMINALS; i++) {
        int status;
        Wait(&status);
        snprintf(buffer, BUFFER_SIZE, "Child process exited with status: %d\n",
                 status);
        TtyWrite(TTY_CONSOLE, buffer, strlen(buffer));
        Delay(TERMINAL_DELAY);
    }

    // Test 3: Large data write test
    TtyWrite(TTY_CONSOLE, "\n== Test 3: Large Data Write Test ==\n", 36);
    Delay(TERMINAL_DELAY);

    test_large_write(TTY_CONSOLE);

    // Test 4: Non-blocking read test
    TtyWrite(TTY_CONSOLE, "\n== Test 4: Non-blocking Read Test ==\n", 37);
    Delay(TERMINAL_DELAY);

    test_nonblocking_read(TTY_CONSOLE);

    TtyWrite(TTY_CONSOLE, "\n=== All Tests Completed ===\n", 29);
    return 0;
}

/*
 * Test basic terminal read/write functionality
 */
void test_basic_io(int tty_id) {
    char input_buffer[BUFFER_SIZE];
    int bytes_read, bytes_written;

    for (int i = 0; i < TEST_ITERATIONS; i++) {
        // Prompt for user input
        snprintf(buffer, BUFFER_SIZE,
                 "Terminal %d> Test %d: Please enter some text: ", tty_id,
                 i + 1);
        bytes_written = TtyWrite(tty_id, buffer, strlen(buffer));
        Delay(TERMINAL_DELAY);

        if (bytes_written < 0) {
            snprintf(buffer, BUFFER_SIZE, "TtyWrite error: %d\n",
                     bytes_written);
            TtyWrite(TTY_CONSOLE, buffer, strlen(buffer));
            Delay(TERMINAL_DELAY);
            continue;
        }

        // Read input from terminal
        memset(input_buffer, 0, BUFFER_SIZE);
        bytes_read = TtyRead(tty_id, input_buffer, TERMINAL_MAX_LINE);

        if (bytes_read < 0) {
            snprintf(buffer, BUFFER_SIZE, "TtyRead error: %d\n", bytes_read);
            TtyWrite(TTY_CONSOLE, buffer, strlen(buffer));
            Delay(TERMINAL_DELAY);
            continue;
        }

        // Display the read result
        snprintf(buffer, BUFFER_SIZE,
                 "Terminal %d> Received %d bytes: \"%.*s\"\n", tty_id,
                 bytes_read, BUFFER_SIZE - 50, input_buffer);
        TtyWrite(TTY_CONSOLE, buffer, strlen(buffer));
        Delay(TERMINAL_DELAY);

        // Brief pause
        Delay(1);
    }
}

/*
 * Test non-blocking read (attempted using Fork)
 */
void test_nonblocking_read(int tty_id) {
    int pid;

    // Prepare for non-blocking test
    snprintf(buffer, BUFFER_SIZE, "Preparing for non-blocking read test...\n");
    TtyWrite(tty_id, buffer, strlen(buffer));
    Delay(TERMINAL_DELAY);

    Delay(1);  // Give user time to read the prompt

    pid = Fork();

    if (pid == 0) {  // Child process
        // Child waits 2 seconds before writing to the terminal
        Delay(2);

        snprintf(buffer, BUFFER_SIZE,
                 "This is data written by child process (PID: %d)\n", GetPid());
        TtyWrite(tty_id, buffer, strlen(buffer));
        Delay(TERMINAL_DELAY);

        Exit(0);
    } else {  // Parent process
        // Indicate start of non-blocking test
        snprintf(buffer, BUFFER_SIZE,
                 "Starting to read from terminal %d (will block until child "
                 "writes data)...\n",
                 tty_id);
        TtyWrite(TTY_CONSOLE, buffer, strlen(buffer));
        Delay(TERMINAL_DELAY);

        // Attempt to read data from terminal (this will block until data is
        // available)
        char read_buffer[BUFFER_SIZE];
        int bytes_read = TtyRead(tty_id, read_buffer, TERMINAL_MAX_LINE);

        if (bytes_read < 0) {
            snprintf(buffer, BUFFER_SIZE, "TtyRead error: %d\n", bytes_read);
            TtyWrite(TTY_CONSOLE, buffer, strlen(buffer));
            Delay(TERMINAL_DELAY);
        } else {
            read_buffer[bytes_read] = '\0';
            snprintf(buffer, BUFFER_SIZE,
                     "Successfully read %d bytes: \"%.*s\"\n", bytes_read,
                     BUFFER_SIZE - 50, read_buffer);
            TtyWrite(TTY_CONSOLE, buffer, strlen(buffer));
            Delay(TERMINAL_DELAY);
        }

        // Wait for child process to complete
        int status;
        Wait(&status);
    }
}

/*
 * Test large data writing
 */
void test_large_write(int tty_id) {
    char large_buffer[BUFFER_SIZE];

    // Generate large test data
    for (int i = 0; i < BUFFER_SIZE - 1; i++) {
        large_buffer[i] = 'A' + (i % 26);
    }
    large_buffer[BUFFER_SIZE - 1] = '\0';

    // Try writing large amount of data at once
    snprintf(buffer, BUFFER_SIZE,
             "Attempting to write %d bytes to terminal %d...\n",
             (int)strlen(large_buffer), tty_id);
    TtyWrite(TTY_CONSOLE, buffer, strlen(buffer));
    Delay(TERMINAL_DELAY);

    int bytes_written = TtyWrite(tty_id, large_buffer, strlen(large_buffer));
    Delay(TERMINAL_DELAY * 2);  // Longer delay for large data

    snprintf(buffer, BUFFER_SIZE, "Actually wrote %d bytes\n", bytes_written);
    TtyWrite(TTY_CONSOLE, buffer, strlen(buffer));
    Delay(TERMINAL_DELAY);

    // Write data in smaller chunks
    snprintf(buffer, BUFFER_SIZE,
             "Now attempting to write the same data in chunks...\n");
    TtyWrite(TTY_CONSOLE, buffer, strlen(buffer));
    Delay(TERMINAL_DELAY);

    int total_written = 0;
    int chunk_size = 100;  // Write 100 bytes at a time

    for (int i = 0; i < (int)strlen(large_buffer); i += chunk_size) {
        int to_write = ((i + chunk_size) > (int)strlen(large_buffer))
                           ? ((int)strlen(large_buffer) - i)
                           : chunk_size;

        bytes_written = TtyWrite(tty_id, large_buffer + i, to_write);
        Delay(TERMINAL_DELAY);

        if (bytes_written < 0) {
            snprintf(buffer, BUFFER_SIZE, "TtyWrite error: %d\n",
                     bytes_written);
            TtyWrite(TTY_CONSOLE, buffer, strlen(buffer));
            Delay(TERMINAL_DELAY);
            break;
        }

        total_written += bytes_written;

        // Brief delay to prevent flooding
        Delay(1);
    }

    snprintf(buffer, BUFFER_SIZE,
             "Chunk writing completed, wrote %d bytes total\n", total_written);
    TtyWrite(TTY_CONSOLE, buffer, strlen(buffer));
    Delay(TERMINAL_DELAY);
}