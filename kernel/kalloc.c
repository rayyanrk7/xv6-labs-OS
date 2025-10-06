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

// Define SUPERPGSIZE if it's not in memlayout.h
#ifndef SUPERPGSIZE
#define SUPERPGSIZE (2 * 1024 * 1024) // 2MB
#endif

void freerange(void *pa_start, void *pa_end);
void superfree(void *pa); // Forward declaration
void superfreerange(void *pa_start, void *pa_end); // Forward declaration

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

// --- Initialization and Range Allocation ---

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
  // Only add 2MB-aligned chunks.
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

  // Calculate the split point for the two pools.
  // We reserve SUPERPAGE_RESERVED_SIZE at the top of memory for superpages.
  super_pa_start = (uint64)PHYSTOP - SUPERPAGE_RESERVED_SIZE;
  standard_pa_end = super_pa_start;

  // 1. Initialize the standard 4KB page allocator pool.
  // This prevents double-initialization of memory with the superpage pool.
  freerange(end, (void*)standard_pa_end);

  // 2. Initialize the 2MB superpage allocator pool with the reserved memory.
  superfreerange((void*)super_pa_start, (void*)PHYSTOP);
}

// --- 4 KB Page Allocator ---

void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");
    
  // NOTE: A more complex check could ensure this page isn't part of a superpage.
  // We rely on the memory split in kinit for correctness.

  memset(pa, 1, PGSIZE); // fill with junk to catch use-after-free

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
    memset((char*)r, 5, PGSIZE); // fill with junk to prevent leaks
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
    memset((char*)r, 0, SUPERPGSIZE); // clear entire 2 MB region
  return (void*)r;
}

void
superfree(void *pa)
{
  struct srun *r;

  // Check alignment and bounds. Superpages MUST be 2MB aligned.
  if(((uint64)pa % SUPERPGSIZE) != 0 || (uint64)pa >= PHYSTOP)
    panic("superfree");

  memset(pa, 1, SUPERPGSIZE); // fill with junk

  r = (struct srun*)pa;

  acquire(&supermem.lock);
  r->next = supermem.freelist;
  supermem.freelist = r;
  release(&supermem.lock);
}
