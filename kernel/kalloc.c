// Physical memory allocator, for user processes,
// kernel stacks, page-table pages, and pipe buffers.
// Also supports 2 MB "superpages" for the pgtbl lab.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);
void superfree(void *pa_end); // forward declaration added ✅

extern char end[]; // first address after kernel (kernel.ld)

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

// ---- superpage allocator ----
struct srun {
  struct srun *next;
};

struct {
  struct spinlock lock;
  struct srun *freelist;
} supermem;
// -----------------------------

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
  // Add every 2 MB-aligned chunk
  uint64 p = (uint64)pa_start;
  if (p % SUPERPGSIZE)
    p += SUPERPGSIZE - (p % SUPERPGSIZE);

  for (; p + SUPERPGSIZE <= (uint64)pa_end; p += SUPERPGSIZE)
    superfree((void*)p);
}

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&supermem.lock, "supermem");

  freerange(end, (void*)PHYSTOP);
  // Reserve a handful of 2 MB superpages from the same region.
  superfreerange(end, (void*)PHYSTOP);
}

// ---------- 4 KB page allocator ----------
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
// ----------------------------------------

// ---------- 2 MB superpage allocator ----------
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

  if(((uint64)pa % SUPERPGSIZE) != 0 || (uint64)pa >= PHYSTOP)
    panic("superfree");

  memset(pa, 1, SUPERPGSIZE);

  r = (struct srun*)pa;

  acquire(&supermem.lock);
  r->next = supermem.freelist;
  supermem.freelist = r;
  release(&supermem.lock);
}
// ----------------------------------------------
