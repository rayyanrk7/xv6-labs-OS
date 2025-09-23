#include "kernel/types.h"
#include "user/user.h"

// separators string
static char *seps = " -\r\t\n./,";

static int
is_sep(char c)
{
  char *p = seps;
  while(*p){
    if(*p == c) return 1;
    p++;
  }
  return 0;
}

int
main(int argc, char *argv[])
{
  if(argc < 2){
    fprintf(2, "Usage: sixfive file\n");
    exit(1);
  }

  int fd = open(argv[1], 0); // 0 == O_RDONLY in xv6
  if(fd < 0){
    fprintf(2, "sixfive: cannot open %s\n", argv[1]);
    exit(1);
  }

  char c;
  char buf[32];
  int idx = 0;
  int r;

  while((r = read(fd, &c, 1)) == 1){
    if(is_sep(c)){
      if(idx > 0){
        buf[idx] = 0;
        int n = atoi(buf);
        if(n % 5 == 0 || n % 6 == 0){
          printf("%d\n", n);
        }
        idx = 0;
      }
      // else skip consecutive separators
    } else if(c >= '0' && c <= '9'){
      if(idx < (int)sizeof(buf) - 1){
        buf[idx++] = c;
      } else {
        // number too big for buffer: skip until separator
        while((r = read(fd, &c, 1)) == 1 && !is_sep(c)) { }
        idx = 0;
        if(r != 1) break;
      }
    } else {
      // non-digit non-sep -> treat as separator
      if(idx > 0){
        buf[idx] = 0;
        int n = atoi(buf);
        if(n % 5 == 0 || n % 6 == 0){
          printf("%d\n", n);
        }
        idx = 0;
      }
    }
  }

  // flush last number if file ended in digits
  if(idx > 0){
    buf[idx] = 0;
    int n = atoi(buf);
    if(n % 5 == 0 || n % 6 == 0){
      printf("%d\n", n);
    }
  }

  close(fd);
  exit(0);
}
