#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  if(argc < 3){
    printf("usage: sandbox mask path -- command ...\n");
    exit(1);
  }
  int mask = atoi(argv[1]);
  char *path = argv[2];
  int pid = fork();
  if(pid < 0){
    fprintf(2, "fork failed\n");
    exit(1);
  }
  if(pid == 0){
    // child
    interpose(mask, path);
    // exec remaining args
    exec(argv[3], &argv[3]);
    // exec failed
    fprintf(2, "exec %s failed\n", argv[3]);
    exit(1);
  } else {
    wait(0);
  }
  exit(0);
}
