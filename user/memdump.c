#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

void memdump(char *fmt, char *data);

int
main(int argc, char *argv[])
{
  if(argc == 1){
    printf("Example 1:\n");
    int a[2] = { 61810, 2025 };
    memdump("ii", (char*) a);
    
    printf("Example 2:\n");
    memdump("S", "a string");
    
    printf("Example 3:\n");
    char *s = "another";
    memdump("s", (char *) &s);

    struct sss {
      char *ptr;
      int num1;
      short num2;
      char byte;
      char bytes[8];
    } example;
    
    example.ptr = "hello";
    example.num1 = 1819438967;
    example.num2 = 100;
    example.byte = 'z';
    strcpy(example.bytes, "xyzzy");
    
    printf("Example 4:\n");
    memdump("pihcS", (char*) &example);
    
    printf("Example 5:\n");
    memdump("sccccc", (char*) &example);
  } else if(argc == 2){
    // format in argv[1], up to 512 bytes of data from standard input.
    char data[512];
    int n = 0;
    memset(data, '\0', sizeof(data));
    while(n < sizeof(data)){
      int nn = read(0, data + n, sizeof(data) - n);
      if(nn <= 0)
        break;
      n += nn;
    }
    memdump(argv[1], data);
  } else {
    printf("Usage: memdump [format]\n");
    exit(1);
  }
  exit(0);
}

void
memdump(char *fmt, char *data)
{
  // Your code here.
    char *ptr = data;
    for(int i = 0; fmt[i]; i++){
        switch(fmt[i]){
        case 'i': { // 4-byte integer
            int val = *(int*)ptr;
            printf("%d\n", val);
            ptr += 4;
            break;
        }
        case 'p': { // 8-byte pointer
            unsigned long val = *(unsigned long*)ptr;
            printf("%lx\n", val);
            ptr += 8;
            break;
        }
        case 'h': { // 2-byte short
            short val = *(short*)ptr;
            printf("%d\n", val);
            ptr += 2;
            break;
        }
        case 'c': { // 1-byte char
            char val = *ptr;
            printf("%c\n", val);
            ptr += 1;
            break;
        }
        case 's': { // 8-byte pointer to string
            char **pstr = (char**)ptr;
            if(*pstr)
                printf("%s\n", *pstr);
            ptr += 8;
            break;
        }
        case 'S': { // null-terminated string
            printf("%s\n", ptr);
            while(*ptr) ptr++; // advance to null
            ptr++; // skip null
            break;
        }
        default:
            // unknown format, skip
            break;
        }
    }



}
