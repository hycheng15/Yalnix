#include "test_utils.h"

void
print_td_stats(struct termstat *stats) {
    for (int i = 0; i < NUM_TERMINALS; i++) {
        printf("\nTERMINAL %d :\n", i);
        printf("tty_in (ReadDataRegister call count): %d\n", stats[i].tty_in);
        printf("tty_out (WriteDataRegister call count): %d\n", stats[i].tty_out);
        printf("user_in (WriteTerminal buflen count): %d\n", stats[i].user_in);
        printf("user_out (ReadTerminal return count): %d\n", stats[i].user_out);
    }
}


void
print_char_arr(char *buf, int len) {
    for (int i = 0; i < len; i++) {
        printf("%c", buf[i]);
        if (buf[i] == '\n') {
            break;
        }
    }
    printf("\n");
}