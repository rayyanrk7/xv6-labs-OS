#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

#define EXEC  1
#define REDIR 2
#define PIPE  3
#define LIST  4
#define BACK  5

#define MAXARGS 10

int strncmp(const char *p, const char *q, uint n) {
  while(n > 0 && *p && *p == *q) {
    n--; p++; q++;
  }
  if(n == 0) return 0;
  return (uchar)*p - (uchar)*q;
}

char* strncpy(char *s, const char *t, int n) {
  char *os = s;
  while(n-- > 0 && (*s++ = *t++) != 0);
  while(n-- > 0) *s++ = 0;
  return os;
}

char* strcat(char *dest, const char *src) {
  char *d = dest + strlen(dest);
  while((*d++ = *src++) != 0);
  return dest;
}

#define HIST_SIZE 16
#define MAX_CMD   128
char history[HIST_SIZE][MAX_CMD];
int hist_count = 0;

void add_history(char *cmd) {
  if(strlen(cmd) == 0) return;
  int idx = hist_count % HIST_SIZE;
  strncpy(history[idx], cmd, MAX_CMD-1);
  history[idx][MAX_CMD-1] = 0;
  hist_count++;
}

void show_history(void) {
  int start = hist_count > HIST_SIZE ? hist_count - HIST_SIZE : 0;
  for(int i = start; i < hist_count; i++) {
    printf("%d: %s\n", i+1, history[i % HIST_SIZE]);
  }
}



struct cmd {
  int type;
};

struct execcmd {
  int type;
  char *argv[MAXARGS];
  char *eargv[MAXARGS];
};

struct redircmd {
  int type;
  struct cmd *cmd;
  char *file;
  char *efile;
  int mode;
  int fd;
};

struct pipecmd {
  int type;
  struct cmd *left;
  struct cmd *right;
};

struct listcmd {
  int type;
  struct cmd *left;
  struct cmd *right;
};

struct backcmd {
  int type;
  struct cmd *cmd;
};

int fork1(void);  // Fork but panics on failure.
void panic(char*);
struct cmd *parsecmd(char*);
void runcmd(struct cmd*) __attribute__((noreturn));

// Execute cmd.  Never returns.
void
runcmd(struct cmd *cmd)
{
  int p[2];
  struct backcmd *bcmd;
  struct execcmd *ecmd;
  struct listcmd *lcmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;

  if(cmd == 0)
    exit(1);

  switch(cmd->type){
  default:
    panic("runcmd");

  case EXEC:
    ecmd = (struct execcmd*)cmd;
    if(ecmd->argv[0] == 0)
      exit(1);
    exec(ecmd->argv[0], ecmd->argv);
    fprintf(2, "exec %s failed\n", ecmd->argv[0]);
    break;

  case REDIR:
    rcmd = (struct redircmd*)cmd;
    close(rcmd->fd);
    if(open(rcmd->file, rcmd->mode) < 0){
      fprintf(2, "open %s failed\n", rcmd->file);
      exit(1);
    }
    runcmd(rcmd->cmd);
    break;

  case LIST:
    lcmd = (struct listcmd*)cmd;
    if(fork1() == 0)
      runcmd(lcmd->left);
    wait(0);
    runcmd(lcmd->right);
    break;

  case PIPE:
    pcmd = (struct pipecmd*)cmd;
    if(pipe(p) < 0)
      panic("pipe");
    if(fork1() == 0){
      close(1);
      dup(p[1]);
      close(p[0]);
      close(p[1]);
      runcmd(pcmd->left);
    }
    if(fork1() == 0){
      close(0);
      dup(p[0]);
      close(p[0]);
      close(p[1]);
      runcmd(pcmd->right);
    }
    close(p[0]);
    close(p[1]);
    wait(0);
    wait(0);
    break;

  case BACK:
    bcmd = (struct backcmd*)cmd;
    if(fork1() == 0)
      runcmd(bcmd->cmd);
    break;
  }
  exit(0);
}

int
getcmd(char *buf, int nbuf, int interactive)
{
  if(interactive)
    write(2, "$ ", 2);

  memset(buf, 0, nbuf);
  if(gets(buf, nbuf) == 0)
    return -1;
  return 0;
}

// return pointer to 1-based history entry (1 = oldest shown entry).
//returns 0 if not present.
static char*
get_history(int n) {
  if(n <= 0 || n > hist_count) return 0;
  int start = hist_count > HIST_SIZE ? hist_count - HIST_SIZE : 0;
  int idx = (start + (n - 1)) % HIST_SIZE;
  return history[idx];
}

void
panic(char *s)
{
  fprintf(2, "%s\n", s);
  exit(1);
}

int
fork1(void)
{
  int pid;

  pid = fork();
  if(pid == -1)
    panic("fork");
  return pid;
}


struct cmd*
execcmd(void)
{
  struct execcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = EXEC;
  return (struct cmd*)cmd;
}

struct cmd*
redircmd(struct cmd *subcmd, char *file, char *efile, int mode, int fd)
{
  struct redircmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = REDIR;
  cmd->cmd = subcmd;
  cmd->file = file;
  cmd->efile = efile;
  cmd->mode = mode;
  cmd->fd = fd;
  return (struct cmd*)cmd;
}

struct cmd*
pipecmd(struct cmd *left, struct cmd *right)
{
  struct pipecmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = PIPE;
  cmd->left = left;
  cmd->right = right;
  return (struct cmd*)cmd;
}

struct cmd*
listcmd(struct cmd *left, struct cmd *right)
{
  struct listcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = LIST;
  cmd->left = left;
  cmd->right = right;
  return (struct cmd*)cmd;
}

struct cmd*
backcmd(struct cmd *subcmd)
{
  struct backcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = BACK;
  cmd->cmd = subcmd;
  return (struct cmd*)cmd;
}
//PAGEBREAK!
// Parsing

char whitespace[] = " \t\r\n\v";
char symbols[] = "<|>&;()";

int
gettoken(char **ps, char *es, char **q, char **eq)
{
  char *s;
  int ret;

  s = *ps;
  while(s < es && strchr(whitespace, *s))
    s++;
  if(q)
    *q = s;
  ret = *s;
  switch(*s){
  case 0:
    break;
  case '|':
  case '(':
  case ')':
  case ';':
  case '&':
  case '<':
    s++;
    break;
  case '>':
    s++;
    if(*s == '>'){
      ret = '+';
      s++;
    }
    break;
  default:
    ret = 'a';
    while(s < es && !strchr(whitespace, *s) && !strchr(symbols, *s))
      s++;
    break;
  }
  if(eq)
    *eq = s;

  while(s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return ret;
}

int
peek(char **ps, char *es, char *toks)
{
  char *s;

  s = *ps;
  while(s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return *s && strchr(toks, *s);
}

struct cmd *parseline(char**, char*);
struct cmd *parsepipe(char**, char*);
struct cmd *parseexec(char**, char*);
struct cmd *nulterminate(struct cmd*);

struct cmd*
parsecmd(char *s)
{
  char *es;
  struct cmd *cmd;

  es = s + strlen(s);
  cmd = parseline(&s, es);
  peek(&s, es, "");
  if(s != es){
    fprintf(2, "leftovers: %s\n", s);
    panic("syntax");
  }
  nulterminate(cmd);
  return cmd;
}

struct cmd*
parseline(char **ps, char *es)
{
  struct cmd *cmd;

  cmd = parsepipe(ps, es);
  while(peek(ps, es, "&")){
    gettoken(ps, es, 0, 0);
    cmd = backcmd(cmd);
  }
  if(peek(ps, es, ";")){
    gettoken(ps, es, 0, 0);
    cmd = listcmd(cmd, parseline(ps, es));
  }
  return cmd;
}

struct cmd*
parsepipe(char **ps, char *es)
{
  struct cmd *cmd;

  cmd = parseexec(ps, es);
  if(peek(ps, es, "|")){
    gettoken(ps, es, 0, 0);
    cmd = pipecmd(cmd, parsepipe(ps, es));
  }
  return cmd;
}

struct cmd*
parseredirs(struct cmd *cmd, char **ps, char *es)
{
  int tok;
  char *q, *eq;

  while(peek(ps, es, "<>")){
    tok = gettoken(ps, es, 0, 0);
    if(gettoken(ps, es, &q, &eq) != 'a')
      panic("missing file for redirection");
    switch(tok){
    case '<':
      cmd = redircmd(cmd, q, eq, O_RDONLY, 0);
      break;
    case '>':
      cmd = redircmd(cmd, q, eq, O_WRONLY|O_CREATE|O_TRUNC, 1);
      break;
    case '+':  // >>
      cmd = redircmd(cmd, q, eq, O_WRONLY|O_CREATE, 1);
      break;
    }
  }
  return cmd;
}

struct cmd*
parseblock(char **ps, char *es)
{
  struct cmd *cmd;

  if(!peek(ps, es, "("))
    panic("parseblock");
  gettoken(ps, es, 0, 0);
  cmd = parseline(ps, es);
  if(!peek(ps, es, ")"))
    panic("syntax - missing )");
  gettoken(ps, es, 0, 0);
  cmd = parseredirs(cmd, ps, es);
  return cmd;
}

struct cmd*
parseexec(char **ps, char *es)
{
  char *q, *eq;
  int tok, argc;
  struct execcmd *cmd;
  struct cmd *ret;

  if(peek(ps, es, "("))
    return parseblock(ps, es);

  ret = execcmd();
  cmd = (struct execcmd*)ret;

  argc = 0;
  ret = parseredirs(ret, ps, es);
  while(!peek(ps, es, "|)&;")){
    if((tok=gettoken(ps, es, &q, &eq)) == 0)
      break;
    if(tok != 'a')
      panic("syntax");
    cmd->argv[argc] = q;
    cmd->eargv[argc] = eq;
    argc++;
    if(argc >= MAXARGS)
      panic("too many args");
    ret = parseredirs(ret, ps, es);
  }
  cmd->argv[argc] = 0;
  cmd->eargv[argc] = 0;
  return ret;
}

struct cmd*
nulterminate(struct cmd *cmd)
{
  int i;
  struct backcmd *bcmd;
  struct execcmd *ecmd;
  struct listcmd *lcmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;

  if(cmd == 0)
    return 0;

  switch(cmd->type){
  case EXEC:
    ecmd = (struct execcmd*)cmd;
    for(i=0; ecmd->argv[i]; i++)
      *ecmd->eargv[i] = 0;
    break;

  case REDIR:
    rcmd = (struct redircmd*)cmd;
    nulterminate(rcmd->cmd);
    *rcmd->efile = 0;
    break;

  case PIPE:
    pcmd = (struct pipecmd*)cmd;
    nulterminate(pcmd->left);
    nulterminate(pcmd->right);
    break;

  case LIST:
    lcmd = (struct listcmd*)cmd;
    nulterminate(lcmd->left);
    nulterminate(lcmd->right);
    break;

  case BACK:
    bcmd = (struct backcmd*)cmd;
    nulterminate(bcmd->cmd);
    break;
  }
  return cmd;
}

int
main(void)
{
  static char buf[128];
  int fd;

  // set up console fds (same approach as stock xv6)
  if((fd = open("console", O_RDWR)) < 0){
    mknod("console", 1, 1);
    fd = open("console", O_RDWR);
  }
  dup(fd);  // stdout
  dup(fd);  // stderr

  // Heuristic: interactive if stdin is a console (fd == 0 after open)
  int interactive = (fd == 0);

  while(getcmd(buf, sizeof(buf), interactive) >= 0){

    char *cmd = buf;

    while(*cmd == ' ' || *cmd == '\t') cmd++;

    // blank line
    if(*cmd == '\n' || *cmd == 0) continue;

    // HISTORY RECALL: !! or !n (before stripping newline)
    if(cmd[0] == '!') {
      if(cmd[1] == '!' || cmd[1] == '\n' || cmd[1] == 0) {
        if(hist_count == 0){
          fprintf(2, "no history\n");
          continue;
        }
        char *last = get_history(hist_count > HIST_SIZE ? HIST_SIZE : hist_count);
        if(!last){
          fprintf(2, "no history\n");
          continue;
        }
        strncpy(buf, last, sizeof(buf)-1);
        buf[sizeof(buf)-1] = 0;
        cmd = buf;
        while(*cmd == ' ' || *cmd == '\t') cmd++;
      } else {
        int num = 0, i = 1;
        while(cmd[i] >= '0' && cmd[i] <= '9'){
          num = num*10 + (cmd[i]-'0');
          i++;
        }
        if(num == 0){
          fprintf(2, "bad history reference\n");
          continue;
        }
        char *h = get_history(num);
        if(!h){
          fprintf(2, "no such history entry\n");
          continue;
        }
        strncpy(buf, h, sizeof(buf)-1);
        buf[sizeof(buf)-1] = 0;
        cmd = buf;
        while(*cmd == ' ' || *cmd == '\t') cmd++;
      }
    }

    //strip trailing newline
    int len = strlen(cmd);
    if(len > 0 && cmd[len-1] == '\n') cmd[len-1] = 0;

    // cd
    if(strncmp(cmd, "cd ", 3) == 0){
      if(chdir(cmd+3) < 0)
        fprintf(2, "cannot cd %s\n", cmd+3);
      {
        char tmp[MAX_CMD];
        int tlen = strlen(cmd);
        if(tlen >= (int)sizeof(tmp)-1) tlen = sizeof(tmp)-2;
        memmove(tmp, cmd, tlen);
        tmp[tlen] = '\n';
        tmp[tlen+1] = 0;
        add_history(tmp);
      }
      continue;
    }

    // history
    if(strcmp(cmd, "history") == 0){
      show_history();
      {
        char tmp[MAX_CMD];
        int tlen = strlen(cmd);
        if(tlen >= (int)sizeof(tmp)-1) tlen = sizeof(tmp)-2;
        memmove(tmp, cmd, tlen);
        tmp[tlen] = '\n';
        tmp[tlen+1] = 0;
        add_history(tmp);
      }
      continue;
    }

    // wait
    if(strcmp(cmd, "wait") == 0){
      {
        char tmp[MAX_CMD];
        int tlen = strlen(cmd);
        if(tlen >= (int)sizeof(tmp)-1) tlen = sizeof(tmp)-2;
        memmove(tmp, cmd, tlen);
        tmp[tlen] = '\n';
        tmp[tlen+1] = 0;
        add_history(tmp);
      }
      while(wait(0) > 0) ;
      continue;
    }

    //complete <prefix>
    if(strncmp(cmd, "complete ", 9) == 0){
      char prefix[MAX_CMD];
      strncpy(prefix, cmd+9, sizeof(prefix)-1);
      prefix[sizeof(prefix)-1] = 0;
      
      // Remove trailing newline if present
      int plen = strlen(prefix);
      if(plen > 0 && prefix[plen-1] == '\n') prefix[plen-1] = 0;
      
      printf("Completions for '%s':\n", prefix);
      
      int fd = open(".", 0);
      if(fd >= 0) {
        struct dirent de;
        int found = 0;
        while(read(fd, &de, sizeof(de)) == sizeof(de)) {
          if(de.inum == 0) continue;
          if(strncmp(de.name, prefix, strlen(prefix)) == 0) {
            printf("  %s\n", de.name);
            found = 1;
          }
        }
        close(fd);
        if(!found) printf("  (no matches)\n");
      }
      
      {
        char tmp[MAX_CMD];
        int tlen = strlen(cmd);
        if(tlen >= (int)sizeof(tmp)-1) tlen = sizeof(tmp)-2;
        memmove(tmp, cmd, tlen);
        tmp[tlen] = '\n';
        tmp[tlen+1] = 0;
        add_history(tmp);
      }
      continue;
    }

    // normal command — add to history
    {
      char tmp[MAX_CMD];
      int tlen = strlen(cmd);
      if(tlen >= (int)sizeof(tmp)-1) tlen = sizeof(tmp)-2;
      memmove(tmp, cmd, tlen);
      tmp[tlen] = '\n';
      tmp[tlen+1] = 0;
      add_history(tmp);
    }

    // fork+exec
    if(fork1() == 0)
      runcmd(parsecmd(cmd));
    wait(0);
  }
  exit(0);
}
