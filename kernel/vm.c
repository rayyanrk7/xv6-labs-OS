// kernel/vm.c
#include "param.h"
#include "types.h"
#include "memlayout.h"
#include "elf.h"
#include "riscv.h"
#include "defs.h"
#include "spinlock.h"
#include "proc.h"
#include "fs.h"

#ifndef SUPERPGSIZE
#define SUPERPGSIZE (2 * 1024 * 1024) // 2MB
#endif

/*
 * the kernel's page table.
 */
pagetable_t kernel_pagetable;

extern char etext[];  // kernel.ld sets this to end of kernel code.
extern char trampoline[]; // trampoline.S

// Make a direct-map page table for the kernel.
pagetable_t
kvmmake(void)
{
  pagetable_t kpgtbl;

  kpgtbl = (pagetable_t) kalloc();
  memset(kpgtbl, 0, PGSIZE);

  // uart registers
  kvmmap(kpgtbl, UART0, UART0, PGSIZE, PTE_R | PTE_W);

  // virtio mmio disk interface
  kvmmap(kpgtbl, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);

#ifdef LAB_NET
  // PCI-E ECAM (configuration space), for pci.c
  kvmmap(kpgtbl, 0x30000000L, 0x30000000L, 0x10000000, PTE_R | PTE_W);

  // pci.c maps the e1000's registers here.
  kvmmap(kpgtbl, 0x40000000L, 0x40000000L, 0x20000, PTE_R | PTE_W);
#endif  

  // PLIC
  kvmmap(kpgtbl, PLIC, PLIC, 0x4000000, PTE_R | PTE_W);

  // map kernel text executable and read-only.
  kvmmap(kpgtbl, KERNBASE, KERNBASE, (uint64)etext-KERNBASE, PTE_R | PTE_X);

  // map kernel data and the physical RAM we'll make use of.
  kvmmap(kpgtbl, (uint64)etext, (uint64)etext, PHYSTOP-(uint64)etext, PTE_R | PTE_W);

  // map the trampoline for trap entry/exit to
  // the highest virtual address in the kernel.
  kvmmap(kpgtbl, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);

  // allocate and map a kernel stack for each process.
  proc_mapstacks(kpgtbl);
  
  return kpgtbl;
}

// Initialize the kernel_pagetable, shared by all CPUs.
void
kvminit(void)
{
  kernel_pagetable = kvmmake();
}

// Switch the current CPU's h/w page table register to
// the kernel's page table, and enable paging.
void
kvminithart()
{
  // wait for any previous writes to the page table memory to finish.
  sfence_vma();

  w_satp(MAKE_SATP(kernel_pagetable));

  // flush stale entries from the TLB.
  sfence_vma();
}

// Return the address of the PTE in page table pagetable
// that corresponds to virtual address va.  If alloc!=0,
// create any required page-table pages.
pte_t *
walk(pagetable_t pagetable, uint64 va, int alloc)
{
  if(va >= MAXVA)
    panic("walk");

  for(int level = 2; level > 0; level--) {
    pte_t *pte = &pagetable[PX(level, va)];
    if(*pte & PTE_V) {
      #ifdef LAB_PGTBL
      // TASK 4: Superpage check (PTE_LEAF)
      if(PTE_LEAF(*pte)) {
        // a leaf at higher level (superpage) -- return it
        return pte;
      }
      #endif
      pagetable = (pagetable_t)PTE2PA(*pte);
    } else {
      if(!alloc || (pagetable = (pde_t*)kalloc()) == 0)
        return 0;
      memset(pagetable, 0, PGSIZE);
      *pte = PA2PTE(pagetable) | PTE_V;
    }
  }
  return &pagetable[PX(0, va)];
}

// Look up a virtual address, return the physical address,
// or 0 if not mapped.
// Can only be used to look up user pages.
uint64
walkaddr(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  uint64 pa;

  if(va >= MAXVA)
    return 0;

  pte = walk(pagetable, va, 0);
  if(pte == 0)
    return 0;
  if((*pte & PTE_V) == 0)
    return 0;
  
  // All user memory accesses must check PTE_U
  if((*pte & PTE_U) == 0)
     return 0;

  pa = PTE2PA(*pte);

  // TASK 4: Superpage offset calculation
  #ifdef LAB_PGTBL
  if(PTE_LEAF(*pte)) {
    // If it's a leaf at L1 (superpage), use the lower 21 bits (2MB - 1) as the offset.
    pa = pa | (va & (SUPERPGSIZE - 1));
  }
  #endif

  return pa;
}

#if defined(LAB_PGTBL) || defined(SOL_MMAP) || defined(SOL_COW)
// Recursively print page table entries
void
vmprintwalk(pagetable_t pagetable, int level, uint64 va_base)
{
  // there are 2^9 = 512 PTEs in a page table.
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    
    // Only print valid PTEs
    if(pte & PTE_V){
      // Calculate the virtual address for this PTE
      uint64 va = va_base | ((uint64)i << (12 + 9 * (2 - level)));
      
      // Print indentation based on level (0, 1, or 2)
      for(int j = 0; j <= level; j++){
        printf(" ..");
      }
      
      uint64 pa = PTE2PA(pte);
      printf("%p: pte %p pa %p\n", (void *)va, (void *)pte, (void *)pa);
      
      // If this PTE points to another page table (not a leaf), recurse
      if((pte & (PTE_R|PTE_W|PTE_X)) == 0){
        // This is a pointer to a lower-level page table
        uint64 child = PTE2PA(pte);
        vmprintwalk((pagetable_t)child, level + 1, va);
      }
    }
  }
}

void
vmprint(pagetable_t pagetable)
{
  printf("page table %p\n", pagetable);
  vmprintwalk(pagetable, 0, 0);
}
#endif

// add a mapping to the kernel page table.
// only used when booting.
// does not flush TLB or enable paging.
void
kvmmap(pagetable_t kpgtbl, uint64 va, uint64 pa, uint64 sz, int perm)
{
  if(mappages(kpgtbl, va, sz, pa, perm) != 0)
    panic("kvmmap");
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa.
// va and size MUST be page-aligned.
// Returns 0 on success, -1 if walk() couldn't
// allocate a needed page-table page.
int
mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm)
{
  uint64 a, last;
  pte_t *pte;
  uint64 szinc = PGSIZE; // Default step/page size

  if((va % PGSIZE) != 0)
    panic("mappages: va not aligned");
  if((size % PGSIZE) != 0)
    panic("mappages: size not aligned");
  if(size == 0)
    panic("mappages: size");
  
  
  a = va;
  last = va + size - 1; // Last byte address

  for(;;){
    szinc = PGSIZE; // Reset for 4KB mapping
    
    // TASK 4: Superpage-aware mapping logic
    #ifdef LAB_PGTBL
    if((a % SUPERPGSIZE) == 0 && (pa % SUPERPGSIZE) == 0 && (last - a + 1) >= SUPERPGSIZE) {
      
      // 1. Traverse and allocate down to L1
      pagetable_t current_pgtbl = pagetable;
      int failed = 0;
      for(int level = 2; level > 1; level--) {
          pte_t *pte_ptr = &current_pgtbl[PX(level, a)];
          if(*pte_ptr & PTE_V) {
              current_pgtbl = (pagetable_t)PTE2PA(*pte_ptr);
          } else {
              if((current_pgtbl = (pde_t*)kalloc()) == 0) {
                failed = 1;
                break;
              }
              memset(current_pgtbl, 0, PGSIZE);
              *pte_ptr = PA2PTE(current_pgtbl) | PTE_V;
          }
      }

      if (!failed) {
        // 2. Check L1 PTE
        pte_t *l1_pte = &current_pgtbl[PX(1, a)];

        // If L1 is free, use it for superpage map
        if((*l1_pte & PTE_V) == 0) {
          *l1_pte = PA2PTE(pa) | perm | PTE_V;
          szinc = SUPERPGSIZE;
        } 
      }
      // If failed or L1 not free, szinc remains PGSIZE, and we fall through.
    }
    #endif

    // If superpage wasn't used (szinc == PGSIZE), proceed with 4KB mapping
    if(szinc == PGSIZE) {
        if((pte = walk(pagetable, a, 1)) == 0)
          return -1;
        if(*pte & PTE_V)
          panic("mappages: remap");

        *pte = PA2PTE(pa) | perm | PTE_V;
    }

    if(a + szinc > last + 1)
      break;
    
    a += szinc;
    pa += szinc;
    if(a > last)
      break;
  }
  return 0;
}

// create an empty user page table.
// returns 0 if out of memory.
pagetable_t
uvmcreate()
{
  pagetable_t pagetable;
  pagetable = (pagetable_t) kalloc();
  if(pagetable == 0)
    return 0;
  memset(pagetable, 0, PGSIZE);
  return pagetable;
}

// Remove npages of mappings starting from va. va must be
// page-aligned. It's OK if the mappings don't exist.
// Optionally free the physical memory.
void
uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
{
  uint64 a;
  pte_t *pte;
  uint64 sz = PGSIZE; // Use uint64 for the step size

  if((va % PGSIZE) != 0)
    panic("uvmunmap: not aligned");

  // npages * PGSIZE is the total size of the region
  for(a = va; a < va + npages*PGSIZE; a += sz){
    sz = PGSIZE; // Default step size
    
    if((pte = walk(pagetable, a, 0)) == 0)
      continue;
    
    if((*pte & PTE_V) == 0)
      continue;
      
    // TASK 4: Check for superpage (L1 leaf)
    #ifdef LAB_PGTBL
    if(PTE_LEAF(*pte) && (a % SUPERPGSIZE) == 0) {
      uint64 pa = PTE2PA(*pte);
      if((pa % SUPERPGSIZE) == 0) {
        sz = SUPERPGSIZE;
      }
    }
    #endif

    if((PTE_FLAGS(*pte) & (PTE_R|PTE_W|PTE_X)) == 0) {
        if (sz == SUPERPGSIZE) 
             panic("uvmunmap: L1 PTE is a table pointer");
        panic("uvmunmap: not a leaf");
    }

    if(do_free){
      uint64 pa = PTE2PA(*pte);
      if (sz == SUPERPGSIZE) {
        superfree((void*)pa);
      } else {
        kfree((void*)pa);
      }
    }
    
    // Invalidate the PTE and flush TLB
    *pte = 0;
    sfence_vma(); 
  }
}


// Allocate PTEs and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
uint64
uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz, int xperm)
{
  char *mem;
  uint64 a;
  uint64 sz; 
  
  if(newsz < oldsz)
    return oldsz;

  oldsz = PGROUNDUP(oldsz);
  for(a = oldsz; a < newsz; a += sz){
    sz = PGSIZE; // Default allocation size

    // TASK 4: Try to allocate a superpage if aligned and enough size left
    #ifdef LAB_PGTBL
    if ((a % SUPERPGSIZE) == 0 && (newsz - a) >= SUPERPGSIZE) {
      mem = superalloc();
      if (mem != 0) {
        sz = SUPERPGSIZE;
      } else {
        mem = kalloc(); 
      }
    } else {
      mem = kalloc();
    }
    #else
    mem = kalloc();
    #endif

    if(mem == 0){
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }

#ifndef LAB_SYSCALL
    memset(mem, 0, sz);
#endif
    if(mappages(pagetable, a, sz, (uint64)mem, PTE_R|PTE_U|xperm) != 0){
      if (sz == SUPERPGSIZE) {
        superfree(mem);
      } else {
        kfree(mem);
      }
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
  }
  return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
uint64
uvmdealloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  if(newsz >= oldsz)
    return oldsz;

  if(PGROUNDUP(newsz) < PGROUNDUP(oldsz)){
    uint64 total_size = PGROUNDUP(oldsz) - PGROUNDUP(newsz);
    int npages = total_size / PGSIZE;
    uvmunmap(pagetable, PGROUNDUP(newsz), npages, 1);
  }

  return newsz;
}

// Recursively free page-table pages.
// All leaf mappings must already have been removed.
void
freewalk(pagetable_t pagetable)
{
  // there are 2^9 = 512 PTEs in a page table.
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    // Check if the PTE is a non-leaf pointer to a lower-level page table
    if((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0){
      // this PTE points to a lower-level page table.
      uint64 child = PTE2PA(pte);
      freewalk((pagetable_t)child);
      pagetable[i] = 0;
    } else if(pte & PTE_V){
      if((pte & (PTE_R|PTE_W|PTE_X)) != 0) {
         panic("freewalk: leaf not removed");
      }
      panic("freewalk: leaf");
    }
  }
  kfree((void*)pagetable);
}

// Free user memory pages,
// then free page-table pages.
void
uvmfree(pagetable_t pagetable, uint64 sz)
{
  if(sz > 0)
    uvmunmap(pagetable, 0, PGROUNDUP(sz)/PGSIZE, 1);
  freewalk(pagetable);
}

// Given a parent process's page table, copy
// its memory into a child's page table.
// returns 0 on success, -1 on failure.
int
uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;
  char *mem;
  uint64 szinc = PGSIZE; 

  for(i = 0; i < sz; i += szinc){
    szinc = PGSIZE; 
    uint64 pagesize = PGSIZE; 

    if((pte = walk(old, i, 0)) == 0)
      continue;
    if((*pte & PTE_V) == 0) {
      continue;
    }

    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);

    // TASK 4: Superpage detection and handling
    #ifdef LAB_PGTBL
    if(PTE_LEAF(*pte) && (i % SUPERPGSIZE) == 0) {
      if ((pa % SUPERPGSIZE) == 0) {
          szinc = SUPERPGSIZE;
          pagesize = SUPERPGSIZE;
      }
    }
    #endif

    // 1. Allocate the physical memory (4KB or 2MB)
    if(pagesize == SUPERPGSIZE) {
         mem = superalloc();
    } else {
         mem = kalloc();
    }

    if(mem == 0)
      goto err;

    // 2. Copy the memory (4KB or 2MB)
    memmove(mem, (char*)pa, pagesize);

    // 3. Map the memory in the child's page table
    if(mappages(new, i, pagesize, (uint64)mem, flags) != 0){
      if (pagesize == SUPERPGSIZE) {
        superfree(mem);
      } else {
        kfree(mem);
      }
      goto err;
    }
  }
  return 0;

  err:
  uvmunmap(new, 0, i / PGSIZE, 1);
  return -1;
}

// mark a PTE invalid for user access.
// used by exec for the user stack guard page.
void
uvmclear(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  
  pte = walk(pagetable, va, 0);
  if(pte == 0)
    panic("uvmclear");
  *pte &= ~PTE_U;
}

// Copy from kernel to user.
// Copy len bytes from src to virtual address dstva in a given page table.
// Return 0 on success, -1 on error.
int
copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
{
  uint64 n, va0, pa0;
  pte_t *pte;

  while(len > 0){
    va0 = PGROUNDDOWN(dstva);
    if (va0 >= MAXVA)
      return -1;

    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0) {
      if((pa0 = vmfault(pagetable, va0, 0)) == 0) {
        return -1;
      }
    }

    if((pte = walk(pagetable, va0, 0)) == 0) {
      return -1;
    }

    // forbid copyout over read-only user text pages.
    if((*pte & PTE_W) == 0)
      return -1;
    
    n = PGSIZE - (dstva - va0);
    if(n > len)
      n = len;
    
    memmove((void *)(pa0 + (dstva - va0)), src, n);

    len -= n;
    src += n;
    dstva = va0 + PGSIZE;
  }
  return 0;
}

// Copy from user to kernel.
// Copy len bytes to dst from virtual address srcva in a given page table.
// Return 0 on success, -1 on error.
int
copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)
{
  uint64 n, va0, pa0;
  
  while(len > 0){
    va0 = PGROUNDDOWN(srcva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0) {
      if((pa0 = vmfault(pagetable, va0, 0)) == 0) {
        return -1;
      }
    }
    n = PGSIZE - (srcva - va0);
    if(n > len)
      n = len;
    
    memmove(dst, (void *)(pa0 + (srcva - va0)), n);

    len -= n;
    dst += n;
    srcva = va0 + PGSIZE;
  }
  return 0;
}

// Copy a null-terminated string from user to kernel.
int
copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max)
{
  uint64 n, va0, pa0;
  int got_null = 0;

  while(got_null == 0 && max > 0){
    va0 = PGROUNDDOWN(srcva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (srcva - va0);
    if(n > max)
      n = max;

    char *p = (char *) (pa0 + (srcva - va0));
    while(n > 0){
      if(*p == '\0'){
        *dst = '\0';
        got_null = 1;
        break;
      } else {
        *dst = *p;
      }
      --n;
      --max;
      p++;
      dst++;
    }

    srcva = va0 + PGSIZE;
  }
  if(got_null){
    return 0;
  } else {
    return -1;
  }
}

// allocate and map user memory if process is referencing a page
// that was lazily allocated in sys_sbrk().
uint64
vmfault(pagetable_t pagetable, uint64 va, int read)
{
  uint64 mem;
  struct proc *p = myproc();
  uint64 size = PGSIZE; // Default allocation size
  

  if (va >= p->sz)
    return 0;
  va = PGROUNDDOWN(va);
  if(ismapped(pagetable, va)) {
    return 0;
  }
  
  // TASK 4: Try to allocate a superpage if 2MB aligned and enough size left
  #ifdef LAB_PGTBL
  if ((va % SUPERPGSIZE) == 0 && (p->sz - va) >= SUPERPGSIZE) {
      mem = (uint64) superalloc();
      if (mem != 0) {
          size = SUPERPGSIZE;
      } else {
          mem = (uint64) kalloc(); // Fall back to 4KB
      }
  } else {
      mem = (uint64) kalloc();
  }
  #else
  mem = (uint64) kalloc();
  #endif
  
  if(mem == 0)
    return 0;
  
  memset((void *) mem, 0, size);
  if (mappages(pagetable, va, size, mem, PTE_W|PTE_U|PTE_R) != 0) {
    if (size == SUPERPGSIZE) {
        superfree((void *)mem);
    } else {
        kfree((void *)mem);
    }
    return 0;
  }
  return mem;
}

int
ismapped(pagetable_t pagetable, uint64 va) {
  pte_t *pte = walk(pagetable, va, 0);
  if (pte == 0) {
    return 0;
  }
  if (*pte & PTE_V){
    return 1;
  }
  return 0;
}

#ifdef LAB_PGTBL
pte_t*
pgpte(pagetable_t pagetable, uint64 va) {
  return walk(pagetable, va, 0);
}
#endif
