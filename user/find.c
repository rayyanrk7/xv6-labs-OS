#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/param.h" 

char buf[512];

void
find(char *path, char *filename, int doexec, char *cmd[], int cmdargc)
{
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if((fd = open(path, 0)) < 0){
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }

  if(fstat(fd, &st) < 0){
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }

  switch(st.type){
  case T_FILE:
    if(strcmp(path+strlen(path)-strlen(filename), filename) == 0){
      if(doexec){
        // build argv
        char *argv[MAXARG];
        for(int i = 0; i < cmdargc; i++)
          argv[i] = cmd[i];
        argv[cmdargc] = path;
        argv[cmdargc+1] = 0;

        if(fork() == 0){
          exec(argv[0], argv);
          fprintf(2, "exec %s failed\n", argv[0]);
          exit(1);
        }
        wait(0);
      } else {
        printf("%s\n", path);
      }
    }
    break;

  case T_DIR:
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      printf("find: path too long\n");
      break;
    }
    strcpy(buf, path);
    p = buf+strlen(buf);
    *p++ = '/';
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      if(de.inum == 0)
        continue;
      if(strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
        continue;
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      find(buf, filename, doexec, cmd, cmdargc);
    }
    break;
  }
  close(fd);
}

int
main(int argc, char *argv[])
{
  if(argc < 3){
    fprintf(2, "Usage: find <path> <filename> [-exec command ...]\n");
    exit(1);
  }

  int execflag = 0;
  char *cmd[MAXARG];
  int cmdargc = 0;

  // check if "-exec" is provided
  for(int i = 3; i < argc; i++){
    if(strcmp(argv[i], "-exec") == 0){
      execflag = 1;
      for(int j = i+1; j < argc; j++){
        cmd[cmdargc++] = argv[j];
      }
      break;
    }
  }

  find(argv[1], argv[2], execflag, cmd, cmdargc);
  exit(0);
}
