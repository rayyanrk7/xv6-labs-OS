// kernel/sysproc.c
#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "vm.h" // For vmprint and pgpte

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  kexit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  // For Task 2: Speed up system calls, the userspace code (ugetpid)
  // will now try to read the PID from the shared USYSCALL page first.
  // This kernel function remains unchanged, as it's the fallback.
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return kfork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return kwait(p);
}

// System call to change the size of the process's address space.
// Argument n is the amount to grow (positive) or shrink (negative).
// Argument t is the allocation type (SBRK_EAGER or SBRK_LAZY).
uint64
sys_sbrk(void)
{
  uint64 addr;
  int t;
  int n;

  argint(0, &n);
  argint(1, &t);
  addr = myproc()->sz;

  // The process of growing/shrinking the memory (and potentially using
  // superpages, if growproc handles it) is delegated to growproc().
  if(t == SBRK_EAGER || n < 0) {
    if(growproc(n) < 0) {
      return -1;
    }
  } else {
    // Lazily allocate memory: increase size but don't map pages.
    // Memory will be mapped on demand in vmfault().
    if(addr + n < addr)
      return -1;
    myproc()->sz += n;
  }
  
  return addr;
}

uint64
sys_pause(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if(n < 0)
    n = 0;
  backtrace(); //t2 backtrace call in sys_pause for test
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}


// TASK 1: Inspect a user-process page table
int
sys_pgpte(void)
{
  uint64 va;
  struct proc *p;

  p = myproc();
  argaddr(0, &va);
  
  // pgpte is a helper function defined in vm.c to get the PTE
  // for a given virtual address.
  pte_t *pte = pgpte(p->pagetable, va); 
  
  if(pte != 0) {
      return (uint64) *pte;
  }
  return 0;
}

// TASK 3: Print a page table
int
sys_kpgtbl(void)
{
  struct proc *p;

  p = myproc();
  
  // vmprint is the function you implemented in vm.c
  vmprint(p->pagetable);
  
  return 0;
}


uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kkill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
