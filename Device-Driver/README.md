# COMP521-Lab1
> Threads, Concurrency, and Operating System Device Drivers

## 1 Team members
*  Hua-Yu Cheng (hc105)
*  Cho-Yun Lei (cl227)

## 2 Implementation Overview

In our implementation, each terminal from `0` to `NUM_TERMINALS-1` is represented by a `Terminal` structure, each has its own buffers for storing data, conditional variables and locks to manage synchronization to name a few.

### 2.1 Buffers and Their Positions

We use three sets of buffers and counters/position for each of them, namely input, output, and echo

1. Buffers - `char[]`
    * `input_buffer`, `output_buffer`, and `echo_buffer`
    * The buffers are to temporarily store data for either `ReadTerminal()` or `WriteTerminal()` to consume
2. Count - `int`
    * `input_count`, `output_count`, `echo_count`
    * The count is to record how many characters are there in the corresponding buffer
3. Read position and write position - `int`
    * `input_read_pos`, `output_read_pos`, `echo_read_pos`
    * `input_write_pos`, `output_write_pos`, `echo_write_pos`
    * These are the positions where the interrupt handler or device driver should write to / read from the buffer
    * Since multiple threads may access the buffers simultaneously. Having separate read and write positions prevents race conditions where a reader might consume data before a writer has fully written it

### 2.2 Conditional Variables

We use 4 conditional variables in our implementation to control the synchronization and flow of the threads

1. `cond_id_t readTerminal_lock` and `int readTerminal_count`
    * We use them to ensure two concurrent `ReadTerminal()` calls **reading** from the same terminal will **not alternate** the characters read from that terminal
    * `readTerminal_count` is to record the number of `ReadTerminal()` calls that is performing its operation
    * If a `ReadTerminal()` wishes to run, it will check if there is any preceding `ReadTerminal()` calls by checking `readTerminal_count > 0`
        * If there is, it waits on `readTerminal_lock`
        * If there is not, it increase `readTerminal_count` by one and perform its actions, then decrease `readTerminal_count` by one and signal `readTerminal_lock` after it finishes

2. `cond_id_t writeTerminal_lock` and `int writeTerminal_count`
    * We use them to ensure that when there are more than one `WriteTerminal()` calls occur concurrently to the same terminal, the **output** characters from these `WriteTerminal()` calls must **not be interleaved**
    * `writeTerminal_count` is to record the number of `WriteTerminal()` calls that is performing its operation
    * If a `WriteTerminal()` wishes to run, it will check if there is any preceding `WriteTerminal()` calls by checking `writeTerminal_count > 0`
        * If there is, it waits on `writeTerminal_lock`
        * If there is not, it increase `writeTerminal_count` by one and perform its actions, then decrease `writeTerminal_count` by one and signal `writeTerminal_lock` after it finishes

3. `cond_id_t output_full`
    * We use an output buffer to store intermediate data before it is consumed/write to terminal by the `WriteDataRegister()` and the `TransmitInterrupt()` cycle
    * However, we should not impose the `buflen` of `WriteTerminal()` user program calls, using this intermediate output buffer will limit the user program's `buflen` to the size of the input buffer in our implementation
    * As a result, when we are copying characters from user buffer to output buffer in the `WriteTerminal()`, we check whether the size of current output buffer is larger than or equal to `BUFFER_SIZE - 2`
        * If it is, we consume the output buffer to make room for subsequent characters by calling `WriteDataRegister()` and wait for `output_full`
        * When the `WriteDataRegister()` finishes and calls `TransmitInterrupt()`, it will signal `output_full` so that the `WriteTerminal()` can continue
    * We check `>= BUFFER_SIZE - 2` instead of `>= BUFFER_SIZE - 1` because we need to take care of character processing (`\n` → `\r\n`)

4. `cond_id_t newline_block` and `int newline_count`
    * Since we need to implement line-oriented terminal, we would like to block `ReadTerminal()` until a newline is entered by the user
    * Hence, we use `newline_count` to record how many newlines are there in the input buffer
        * If there are no newlines in the input buffer, the `ReadTerminal()` will block and wait on `newline_block`
    * When we receive a newline in the `ReceiveInterrupt()`, we will increase the `newline_count` and signal the `newline_block`

## 3 Testing
1. test1.c - Tests basic initialization of the terminal driver.
2. test2.c - Tests writing a string to the terminal using WriteTerminal().
3. test3.c - Tests multiple sequential writes to the terminal.
4. test4.c - Tests concurrent writes from multiple threads to the same terminal.
5. test5.c - Tests concurrent writes to multiple terminals independently.
6. test6.c - Tests reading input from the terminal using ReadTerminal().
7. test7.c - Tests simultaneous reading and writing to ensure they do not interfere.
8. test8.c - Tests immediate echoing of user input.
9. test9.c - Tests handling of large buffer writes to ensure data is not lost.
10. test10.c - Tests backspace functionality while reading input.
11. test11.c - Tests reading input from a terminal multiple times, ensuring consistency in the number of characters read and verifying terminal driver statistics​.
12. test12.c - Tests writing a long string to the terminal, verifying that large sequential writes are correctly handled​.
13. test13.c - Tests simultaneous reading and writing on the same terminal, ensuring that responses are echoed correctly without data corruption​.
14. test14.c - Tests reading input from multiple terminals (1, 2, and 3) to validate the ability to handle multiple streams of input independently​.
15. test15.c - Tests writing different strings to multiple terminals (1, 2, and 3) to verify independent terminal writing operations​.
16. test16.c - Tests simultaneous reading and writing between two terminals (0 and 1), validating concurrent I/O handling​.
17. test17.c - Tests concurrent reading from the same terminal by multiple threads, ensuring read operations are synchronized correctly​.
18. test18.c - Tests concurrent writing from multiple threads to the same terminal, verifying the proper handling of concurrent write operations​.
19. test19.c - Tests multiple concurrent readers accessing different terminals, checking synchronization and correctness of multi-terminal reads​.
20. test20.c - Tests multiple concurrent writers writing to different terminals, ensuring no interference or data corruption across terminals​.
21. test21.c - Tests edge cases and invalid operations on the terminal driver

## 4 Note for TA
During testing, whenever a user types on the X11 terminal, the local terminal will output the current input buffer. This allows for easier tracking of the input buffer’s real-time status.

Additionally, for comprehensive debugging and validation, we have implemented a test_utils.c file that is responsible for printing detailed information about terminal operations. This includes:

* `tty_in (ReadDataRegister call count)` – Tracks the number of times data has been read from the terminal.
* `tty_out (WriteDataRegister call count)` – Tracks the number of times data has been written to the terminal.
* `user_in (WriteTerminal buflen count)` – Displays the total length of data written to the terminal.
* `user_out (ReadTerminal return count)` – Displays the number of characters successfully read from the terminal.

These logs will assist in debugging by providing insights into the terminal driver’s behavior and ensuring correct read/write operations.