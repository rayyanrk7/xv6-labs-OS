#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
    uint ticks = uptime(); // call the uptime system call
    printf("Uptime: %d ticks\n", ticks);
    exit(0);
}

