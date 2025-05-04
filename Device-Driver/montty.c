#include <stdio.h>
#include <stdlib.h>
#include <threads.h>
#include <hardware.h>
#include <terminals.h>
#include <string.h>

# define NUM_TERMINALS 4
# define BUFFER_SIZE 4096

typedef struct {
    char input_buffer[BUFFER_SIZE];
    char output_buffer[BUFFER_SIZE];
    char echo_buffer[BUFFER_SIZE];

    // To record whether this terminal has called InitTerminal
    int terminal_initialized;

    // To record whether writeDataRegister is occupied
    int WDR_busy;

    // Number of characters in input/output/echo buffer
    int input_count; 
    int output_count;
    int echo_count;

    // Next position to be read in input/output/echo buffer
    int input_read_pos;
    int output_read_pos;
    int echo_read_pos;

    // Next position to be write in input/output/echo buffer
    int input_write_pos;
    int output_write_pos;
    int echo_write_pos;
    
    // To ensure only one ReadTerminal for this term
    cond_id_t readTerminal_lock;
    int readTerminal_count; 

    // To ensure only one WriteTerminal for this term
    cond_id_t writeTerminal_lock;
    int writeTerminal_count; 

    // Hold the writeTerminal if the output buffer is full
    cond_id_t output_full;

    // Block readTerminal until a \n in input buffer
    cond_id_t newline_block;
    int newline_count;

    struct termstat stat;

} Terminal;

static Terminal terminals[NUM_TERMINALS];
static int terminalDriver_initialized = 0;

// Initialize the monitor and condition variables for the terminal driver
extern int InitTerminalDriver() {
    Declare_Monitor_Entry_Procedure();

    // To ensure that TerminalDriver is initialized only once
    if (terminalDriver_initialized == 1) {
        return -1;
    }

    terminalDriver_initialized = 1;
    for (int i = 0; i < NUM_TERMINALS; i++)
    {

        terminals[i].terminal_initialized = 0;
        terminals[i].WDR_busy = 0;

        terminals[i].input_count = 0;
        terminals[i].output_count = 0;
        terminals[i].echo_count = 0;

        terminals[i].input_read_pos = 0;
        terminals[i].output_read_pos = 0;
        terminals[i].echo_read_pos = 0;

        terminals[i].input_write_pos = 0;
        terminals[i].output_write_pos = 0;
        terminals[i].echo_write_pos = 0;

        terminals[i].readTerminal_lock = CondCreate();
        terminals[i].readTerminal_count = 0;

        terminals[i].writeTerminal_lock = CondCreate();
        terminals[i].writeTerminal_count = 0;

        terminals[i].output_full = CondCreate();

        terminals[i].newline_block = CondCreate();
        terminals[i].newline_count = 0;

        terminals[i].stat.tty_in = 0;
        terminals[i].stat.tty_out = 0;
        terminals[i].stat.user_in = 0;
        terminals[i].stat.user_out = 0;

    }
    return 0;
}

// Intialize the single terminal hardware
extern int InitTerminal(int term) {
    Declare_Monitor_Entry_Procedure();
    Terminal *t = &terminals[term];
    if (term < 0 || term >= NUM_TERMINALS || terminalDriver_initialized == 0 || t->terminal_initialized == 1) {
        return -1;
    }
    t->terminal_initialized = 1;
    InitHardware(term);
    return 0;
}

extern int TerminalDriverStatistics(struct termstat *stats) {
    Declare_Monitor_Entry_Procedure();

    for (int i = 0; i < NUM_TERMINALS; i++) {
        Terminal *t = &terminals[i];

        stats[i].tty_in = t->stat.tty_in;
        stats[i].tty_out = t->stat.tty_out;
        stats[i].user_in = t->stat.user_in;
        stats[i].user_out = t->stat.user_out;
    }

    return 0;
}

void PrintInputBuffer(int term) {
    Terminal *t = &terminals[term];
    printf("Terminal %d - Input Buffer: \"", term);
    
    int pos = t->input_read_pos;
    for (int i = 0; i < t->input_count; i++) {
        char ch = t->input_buffer[pos];
        if (ch == '\n') {
            printf("\\n");  // Print newline as \n for clarity
        } else if (ch == '\b' || ch == '\177') {
            printf("\\b");  // Print backspace as \b for clarity
        } else {
            putchar(ch);
        }
        pos = (pos + 1) % BUFFER_SIZE;
    }

    printf("\"\n");
}

extern void ReceiveInterrupt(int term) {
    Declare_Monitor_Entry_Procedure();
    Terminal *t = &terminals[term];

    if (term < 0 || term >= NUM_TERMINALS || t->terminal_initialized == 0) {
        return;
    }

    char ch = ReadDataRegister(term);

    // Input buffer size is 4096, user can only type backspace
    if (t->input_count == BUFFER_SIZE) {
        t->stat.tty_in++;
        if (ch == '\b' || ch == '\177') {

            int prev_pos = (t->input_write_pos - 1 + BUFFER_SIZE) % BUFFER_SIZE;
            if (t->input_buffer[prev_pos] == '\n') {
                return;  // Completely ignore backspace in this case
            }

            // Remove the last char from input buffer
            t->input_count -= 1;
            t->input_write_pos = (t->input_write_pos - 1 + BUFFER_SIZE) % BUFFER_SIZE;

            t->echo_buffer[t->echo_write_pos] = '\b';
            t->echo_write_pos = (t->echo_write_pos + 1) % BUFFER_SIZE;
            t->echo_buffer[t->echo_write_pos] = ' ';
            t->echo_write_pos = (t->echo_write_pos + 1) % BUFFER_SIZE;
            t->echo_buffer[t->echo_write_pos] = '\b';
            t->echo_write_pos = (t->echo_write_pos + 1) % BUFFER_SIZE;
            t->echo_count += 3;
        }
    }
    
    // Input buffer still have space
    else if (t->input_count < BUFFER_SIZE) {
        t->stat.tty_in++;

        // Process return carriage and new line
        if (ch == '\r' || ch == '\n') {
            
            t->input_buffer[t->input_write_pos] = '\n';
            t->input_write_pos = (t->input_write_pos + 1) % BUFFER_SIZE;
            t->input_count += 1;

            t->echo_buffer[t->echo_write_pos] = '\r';
            t->echo_write_pos = (t->echo_write_pos + 1) % BUFFER_SIZE;
            t->echo_buffer[t->echo_write_pos] = '\n';
            t->echo_write_pos = (t->echo_write_pos + 1) % BUFFER_SIZE;
            t->echo_count += 2;

            t->newline_count += 1;
            CondSignal(t->newline_block);
            PrintInputBuffer(term);
        }
        else if (ch == '\b' || ch == '\177') {
            if (t->input_count > 0) {

                int prev_pos = (t->input_write_pos - 1 + BUFFER_SIZE) % BUFFER_SIZE;
                if (t->input_buffer[prev_pos] == '\n') {
                    return;  // Completely ignore backspace in this case
                }

                // Remove the last char from input buffer
                t->input_count -= 1;
                t->input_write_pos = (t->input_write_pos - 1 + BUFFER_SIZE) % BUFFER_SIZE;

                t->echo_buffer[t->echo_write_pos] = '\b';
                t->echo_write_pos = (t->echo_write_pos + 1) % BUFFER_SIZE;
                t->echo_buffer[t->echo_write_pos] = ' ';
                t->echo_write_pos = (t->echo_write_pos + 1) % BUFFER_SIZE;
                t->echo_buffer[t->echo_write_pos] = '\b';
                t->echo_write_pos = (t->echo_write_pos + 1) % BUFFER_SIZE;
                t->echo_count += 3;
                PrintInputBuffer(term);
            }
        }
        else {
            t->input_buffer[t->input_write_pos] = ch;
            t->input_write_pos = (t->input_write_pos + 1) % BUFFER_SIZE;
            t->input_count += 1;

            t->echo_buffer[t->echo_write_pos] = ch;
            t->echo_write_pos = (t->echo_write_pos + 1) % BUFFER_SIZE;
            t->echo_count += 1;

            PrintInputBuffer(term);
        }
    }

    // Input buffer is full, drop (ignore) the new ch
    else {
        return;
    }

    // Trigger the first WriteDataRegister when the echo buffer is not empty
    if(!t->WDR_busy && t->echo_count > 0){
        t->WDR_busy = 1;
        WriteDataRegister(term, t->echo_buffer[t->echo_read_pos]);
        t->echo_read_pos = (t->echo_read_pos + 1) % BUFFER_SIZE;
        t->stat.tty_out++;
        t->echo_count--;
    }
}

extern int ReadTerminal(int term, char *buf, int buflen) {
    Declare_Monitor_Entry_Procedure();
    Terminal *t = &terminals[term];

    if (term < 0 || term >= NUM_TERMINALS || !buf || buflen < 0 || t->terminal_initialized == 0 ) {
        return -1;
    }
    
    // Make sure only one ReadTerminal for a given term
    while (t->readTerminal_count > 0) {
        CondWait(t->readTerminal_lock);
    }

    while (t->newline_count == 0) {
        CondWait(t->newline_block);
    }

    t->readTerminal_count += 1;

    if (buflen == 0) {
        return 0;
    }

    int count = 0;
    while (count < buflen) {
        buf[count++] = t->input_buffer[t->input_read_pos];
        t->input_read_pos = (t->input_read_pos + 1) % BUFFER_SIZE;
        t->input_count -= 1;
        if (buf[count - 1] == '\n') {
            t->newline_count -= 1;
            break;
        }
    }
    t->stat.user_out += count;
    if (t->input_count == t->input_read_pos) {
        t->input_count = 0;
        t->input_read_pos = 0;
    }

    t->readTerminal_count -= 1;
    CondSignal(t->readTerminal_lock);
    return count;
}

extern void TransmitInterrupt(int term) {
    Declare_Monitor_Entry_Procedure();
    Terminal *t = &terminals[term];

    if (term < 0 || term >= NUM_TERMINALS || t->terminal_initialized == 0) {
        return;
    }

    // Prioritize echoed characters first
    if (t->echo_count > 0) {
        t->WDR_busy = 1;
        WriteDataRegister(term, t->echo_buffer[t->echo_read_pos]);
        t->echo_read_pos = (t->echo_read_pos + 1) % BUFFER_SIZE;
        t->stat.tty_out++;
        t->echo_count--;
    }
    // Process output buffer
    else if (t->output_count > 0) {
        t->WDR_busy = 1;
        WriteDataRegister(term, t->output_buffer[t->output_read_pos]);
        t->output_read_pos = (t->output_read_pos + 1) % BUFFER_SIZE;
        t->stat.tty_out++;
        t->output_count--;

        // If buffer was previously full, signal WriteTerminal
        CondSignal(t->output_full);
    }
    // Mark WDR as available when no data remains
    else {
        t->WDR_busy = 0;
    }
}

extern int WriteTerminal(int term, char *buf, int buflen) {
    Declare_Monitor_Entry_Procedure();
    Terminal *t = &terminals[term];

    if (term < 0 || term >= NUM_TERMINALS || !buf || buflen < 0 || t->terminal_initialized == 0) {
        return -1;
    }

    // Ensure only one WriteTerminal at a time
    while (t->writeTerminal_count > 0) {
        CondWait(t->writeTerminal_lock);
    }
    t->writeTerminal_count += 1;
    t->stat.user_in += buflen;

    int i = 0;
    while (i < buflen) {
        // Ensure there is enough space in the output buffer
        // Use BUFFER_SIZE - 2 because we need to take care of \n -> \r\n
        while (t->output_count >= BUFFER_SIZE - 2) {  
            if (!t->WDR_busy) {
                t->WDR_busy = 1;
                WriteDataRegister(term, t->output_buffer[t->output_read_pos]);
                t->output_read_pos = (t->output_read_pos + 1) % BUFFER_SIZE;  
                t->stat.tty_out++;
                t->output_count--;
            }
            CondWait(t->output_full);
        }

        // Convert newline into carriage return + newline
        if (buf[i] == '\n') {
            t->output_buffer[t->output_write_pos] = '\r';
            t->output_write_pos = (t->output_write_pos + 1) % BUFFER_SIZE;  
            t->output_count++;

            if (t->output_count >= BUFFER_SIZE - 1) {  
                CondWait(t->output_full);
            }
        }

        t->output_buffer[t->output_write_pos] = buf[i];
        t->output_write_pos = (t->output_write_pos + 1) % BUFFER_SIZE;  
        t->output_count++;
        i++;
    }

    // Start transmission if WDR is not busy
    if (!t->WDR_busy && t->output_count > 0) {
        t->WDR_busy = 1;
        WriteDataRegister(term, t->output_buffer[t->output_read_pos]);
        t->output_read_pos = (t->output_read_pos + 1) % BUFFER_SIZE;  
        t->stat.tty_out++;
        t->output_count--;
    }

    t->writeTerminal_count -= 1;
    CondSignal(t->writeTerminal_lock);

    return buflen;
}

