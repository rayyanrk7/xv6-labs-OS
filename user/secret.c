// user/secret.c
// write a secret string into heap memory and exit.
// The attack program should be able to find it later
// (this relies on the lab's intentional allocator bug).

#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  if(argc < 2){
    fprintf(2, "usage: secret <str>\n");
    exit(1);
  }

  // allocate some memory on the heap and copy the secret there
  char *buf = malloc(128);
  if(buf == 0){
    fprintf(2, "malloc failed\n");
    exit(1);
  }

  // copy the argument into the allocated buffer
  strcpy(buf, argv[1]);

  // optionally print something (not required)
  // printf("secret stored\n");

  // exit, freeing the memory (so the memory will be available to later sbrk allocations)
  exit(0);
}
