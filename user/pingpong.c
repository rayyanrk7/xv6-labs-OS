#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int p2c[2]; // parent -> child
  int c2p[2]; // child -> parent
  char buf[16];

  if (pipe(p2c) < 0) {
    fprintf(2, "pipe failed\n");
    exit(1);
  }
  if (pipe(c2p) < 0) {
    fprintf(2, "pipe failed\n");
    exit(1);
  }

  int pid = fork();
  if (pid < 0) {
    fprintf(2, "fork failed\n");
    exit(1);
  }

  if (pid == 0) {
    // Child: read from parent, print, respond
    close(p2c[1]);    // close write end of parent->child
    close(c2p[0]);    // close read end of child->parent
    int n = read(p2c[0], buf, sizeof(buf));
    if (n > 0) {
      buf[n] = 0;
      printf("%d: received %s\n", getpid(), buf);
    }
    write(c2p[1], "pong", 4);
    close(p2c[0]);
    close(c2p[1]);
    exit(0);
  } else {
    // Parent: send ping, read response, print
    close(p2c[0]);    // close read end of parent->child
    close(c2p[1]);    // close write end of child->parent
    write(p2c[1], "ping", 4);
    int n = read(c2p[0], buf, sizeof(buf));
    if (n > 0) {
      buf[n] = 0;
      printf("%d: received %s\n", getpid(), buf);
    }
    close(p2c[1]);
    close(c2p[0]);
    wait(0);
  }
  exit(0);
}

