// user/bttest.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main() {
    printf("bttest starting...\n");
    pause(10); // This calls sys_pause, triggering your backtrace() hook
    printf("bttest done.\n");
    exit(0);
}
