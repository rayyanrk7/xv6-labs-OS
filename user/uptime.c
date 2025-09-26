#include "kernel/types.h"
#include "user/user.h"
#include "kernel/stat.h"

int main(int argc, char *argv[]) {
    int ticks = uptime(); // call the uptime system call
    printf("Uptime: %d ticks\n", ticks);
    exit(0);
}

