// kernel/kalloc.c
// Physical memory allocator, for user processes,
// kernel stacks, page-table pages, and pipe buffers.
// Also supports 2 MB "superpages" for the pgtbl lab.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

#ifndef SUPERPGSIZE
#define SUPERPGSIZE (2 * 1024 * 1024) // 2MB
#endif

void freerange(void *pa_start, void *pa_end);
void superfree(void *pa); 
void superfreerange(void *pa_start, void *pa_end); 

extern char end[]; // first address after kernel (kernel.ld)

// 4KB page allocator structs
struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

// 2MB superpage allocator structs
struct srun {
  struct srun *next;
};

struct {
  struct spinlock lock;
  struct srun *freelist;
} supermem;

// Reserves 10 * 2MB = 20MB for superpages at the end of PHYSTOP.
#define NUM_SUPERPAGES_RESERVED 10
#define SUPERPAGE_RESERVED_SIZE (NUM_SUPERPAGES_RESERVED * SUPERPGSIZE)

void
freerange(void *pa_start, void *pa_end)
{
  char *p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

void
superfreerange(void *pa_start, void *pa_end)
{
  uint64 p = (uint64)pa_start;
  // Align start address to 2MB boundary
  if (p % SUPERPGSIZE)
    p += SUPERPGSIZE - (p % SUPERPGSIZE);

  for (; p + SUPERPGSIZE <= (uint64)pa_end; p += SUPERPGSIZE)
    superfree((void*)p);
}

void
kinit()
{
  uint64 standard_pa_end;
  uint64 super_pa_start;

  initlock(&kmem.lock, "kmem");
  initlock(&supermem.lock, "supermem");

  // Calculate the split point for the two pools, ensuring 2MB alignment for the start.
  super_pa_start = (uint64)PHYSTOP - SUPERPAGE_RESERVED_SIZE;
  // Ensure the superpage start is 2MB aligned
  if (super_pa_start % SUPERPGSIZE != 0) {
      super_pa_start = PGROUNDDOWN(super_pa_start);
  }
  
  standard_pa_end = super_pa_start;

  // 1. Initialize the standard 4KB page allocator pool (end to super_pa_start).
  freerange(end, (void*)standard_pa_end);

  // 2. Initialize the 2MB superpage allocator pool (super_pa_start to PHYSTOP).
  superfreerange((void*)super_pa_start, (void*)PHYSTOP);
}


// --- 4 KB Page Allocator ---

void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE);
  return (void*)r;
}

// --- 2 MB Superpage Allocator ---

void *
superalloc(void)
{
  struct srun *r;

  acquire(&supermem.lock);
  r = supermem.freelist;
  if(r)
    supermem.freelist = r->next;
  release(&supermem.lock);

  if(r)
    memset((char*)r, 0, SUPERPGSIZE); 
  return (void*)r;
}

void
superfree(void *pa)
{
  struct srun *r;

  if(((uint64)pa % SUPERPGSIZE) != 0 || (uint64)pa >= PHYSTOP)
    panic("superfree");

  memset(pa, 1, SUPERPGSIZE);

  r = (struct srun*)pa;

  acquire(&supermem.lock);
  r->next = supermem.freelist;
  supermem.freelist = r;
  release(&supermem.lock);
}
