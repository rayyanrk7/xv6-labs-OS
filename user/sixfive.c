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

static void
print_num(int n)
{
  char buf[16];
  int i = 0;
  if(n == 0){
    write(1, "0\n", 2);
    return;
  }
  while(n > 0 && i < 16){
    buf[i++] = '0' + (n % 10);
    n /= 10;
  }
  for(int j = i-1; j >= 0; j--){
    write(1, &buf[j], 1);
  }
  write(1, "\n", 1); // add newline after each number
}


int
main(int argc, char *argv[])
{
  if(argc < 2){
    fprintf(2, "Usage: sixfive file...\n");
    exit(1);
  }

  char outbuf[512];
  int outidx = 0;

  // loop through all input files
  for(int f = 1; f < argc; f++){
    int fd = open(argv[f], 0);
    if(fd < 0){
      fprintf(2, "sixfive: cannot open %s\n", argv[f]);
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
            print_num(n);
          }
          idx = 0;
        }
      } else if(c >= '0' && c <= '9'){
        if(idx < (int)sizeof(buf) - 1){
          buf[idx++] = c;
        }
      } else {
        if(idx > 0){
          buf[idx] = 0;
          int n = atoi(buf);
          if(n % 5 == 0 || n % 6 == 0){
            print_num(n);
          }
          idx = 0;
        }
      }
    }

    // flush last number if needed
    if(idx > 0){
      buf[idx] = 0;
      int n = atoi(buf);
      if(n % 5 == 0 || n % 6 == 0){
        print_num(n);
      }
    }

    close(fd);
  }

  // print collected result on one line
  if(outidx > 0){
    printf("%s\n", outbuf);
  }

  exit(0);
}


