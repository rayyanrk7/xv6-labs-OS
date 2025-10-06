#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#include "kernel/riscv.h"

#define PAGE 4096
#define MAX_COPY 256   // maximum printed secret length

static int is_alnum(char c) {
  if (c >= '0' && c <= '9') return 1;
  if (c >= 'A' && c <= 'Z') return 1;
  if (c >= 'a' && c <= 'z') return 1;
  return 0;
}

int
main(int argc, char *argv[])
{
  char *p;
  int tries = 200;        // number of pages to try
  int i;

  // Try allocating many pages and scan each newly returned page.
  for (i = 0; i < tries; i++) {
    p = sbrk(PAGE);
    if (p == (char*)-1) {
      // no more memory
      break;
    }

    // Scan this page for alphanumeric runs.
    int best_len = 0;
    int best_off = -1;
    int off = 0;
    while (off < PAGE) {
      // skip non-alnum
      while (off < PAGE && !is_alnum(p[off])) off++;
      if (off >= PAGE) break;
      int start = off;
      while (off < PAGE && is_alnum(p[off])) off++;
      int len = off - start;
      if (len > best_len) {
        best_len = len;
        best_off = start;
      }
    }

    if (best_len > 0) {
      // copy and null-terminate the best candidate 
      int copylen = best_len < (MAX_COPY - 1) ? best_len : (MAX_COPY - 1);
      char buf[MAX_COPY];
      int j;
      for (j = 0; j < copylen; j++) buf[j] = p[best_off + j];
      buf[copylen] = '\0';

      // Print the candidate and exit 
      printf("%s\n", buf);
      exit(0);
    }
  }

  exit(1);
}
