#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/param.h"
#include "user/regex.h" 

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
    if(match(filename, path + strlen(path) - strlen(filename))){
      if(doexec){
        // build argv
        char *argv[MAXARG];
	if(cmdargc + 2 > MAXARG){
   	 fprintf(2, "too many arguments for -exec\n");
   	 return;
	}
        for(int i = 0; i < cmdargc; i++)
          argv[i] = cmd[i];
        argv[cmdargc] = path;
        argv[cmdargc+1] = 0;

        int pid = fork();
	if(pid < 0){
   	 fprintf(2, "fork failed\n");
   	 exit(1);
	} else if(pid == 0){
   	 exec(argv[0], argv);
   	 fprintf(2, "exec %s failed\n", argv[0]);
   	 exit(1);
	} else {
   	 wait(0);
	}
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
      int namelen = 0;
      while(namelen < DIRSIZ && de.name[namelen] != '\0') {
    	 namelen++;
      }
      memmove(p, de.name, namelen);
      p[namelen] = 0;
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

int match(char*, char*);
int matchhere(char*, char*);
int matchstar(int, char*, char*);

int match(char *re, char *text)
{
  if(re[0] == '^')
    return matchhere(re+1, text);
  do{
    if(matchhere(re, text))
      return 1;
  }while(*text++ != '\0');
  return 0;
}

int matchhere(char *re, char *text)
{
  if(re[0] == '\0')
    return 1;
  if(re[1] == '*')
    return matchstar(re[0], re+2, text);
  if(re[0] == '$' && re[1] == '\0')
    return *text == '\0';
  if(*text!='\0' && (re[0]=='.' || re[0]==*text))
    return matchhere(re+1, text+1);
  return 0;
}

int matchstar(int c, char *re, char *text)
{
  do{
    if(matchhere(re, text))
      return 1;
  }while(*text!='\0' && (*text++==c || c=='.'));
  return 0;
}
