#include <assert.h>
#include <os/irq.h>
#include <os/kernel.h>
#include <os/lock.h>
#include <os/mm.h>
#include <os/rand.h>
#include <os/smp.h>
#include <os/string.h>
#include <os/task.h>
#include <pgtable.h>
#include <printk.h>
#include <type.h>

typedef uint64_t PTE;
LIST_HEAD(free_list);
LIST_HEAD(free_swap_list);
pgcb_t pgcbs[USER_PAGE_NUM_MAX];
swap_page_t swap_pages[SWAP_PAGE_NUM_MAX];
shm_page_t shm_pages[SHM_PAGE_NUM_MAX];
// NOTE: A/C-core
static ptr_t kernMemCurr = FREEMEM_KERNEL;

void init_pages() {

  for (int i = 0; i < USER_PAGE_NUM_MAX; i++) {
    pgcbs[i].kva = allocPage(1);
    pgcbs[i].owner_pid = -1;
    pgcbs[i].va = 0;
    pgcbs[i].pcb = NULL;
    pgcbs[i].list.next = &pgcbs[i].list;
    pgcbs[i].list.prev = &pgcbs[i].list;
    enqueue(&free_list, &pgcbs[i].list);
  }
  int my_img_end_sector = img_end_sector + 1;
  for (int i = 0; i < SWAP_PAGE_NUM_MAX; i++) {
    swap_pages[i].va = 0;
    swap_pages[i].owner_pid = -1;
    swap_pages[i].block_id = my_img_end_sector;
    swap_pages[i].list.next = NULL;
    swap_pages[i].list.prev = NULL;
    my_img_end_sector += PAGE_SIZE / SECTOR_SIZE;
    enqueue(&free_swap_list, &swap_pages[i].list);
  }
  for (int i = 0; i < SHM_PAGE_NUM_MAX; i++) {
    shm_pages[i].key = 0;
    shm_pages[i].pgcb = NULL;
    shm_pages[i].user_num = 0;
  }
}

ptr_t allocPage(int numPage) {
  // align PAGE_SIZE
  ptr_t ret = ROUND(kernMemCurr, PAGE_SIZE);
  kernMemCurr = ret + numPage * PAGE_SIZE;
  return ret;
}

// NOTE: Only need for S-core to alloc 2MB large page
#ifdef S_CORE
static ptr_t largePageMemCurr = LARGE_PAGE_FREEMEM;
ptr_t allocLargePage(int numPage) {
  // align LARGE_PAGE_SIZE
  ptr_t ret = ROUND(largePageMemCurr, LARGE_PAGE_SIZE);
  largePageMemCurr = ret + numPage * LARGE_PAGE_SIZE;
  return ret;
}
#endif

void freePage(ptr_t baseAddr) {
  // TODO [P4-task1] (design you 'freePage' here if you need):
}

void *kmalloc(size_t size) {
  // TODO [P4-task1] (design you 'kmalloc' here if you need):
}

/* this is used for mapping kernel virtual address into user page table */
void share_pgtable(uintptr_t dest_pgdir, uintptr_t src_pgdir) {
  // TODO [P4-task1] share_pgtable:
  memcpy((uint8_t *)dest_pgdir, (uint8_t *)src_pgdir, PAGE_SIZE);
}
void unmap_boot_pages() {
  PTE *boot_pgdir = (PTE *)pa2kva(PGDIR_PA);
  for (uint64_t pa = 0x50000000lu; pa < 0x51000000lu; pa += 0x200000lu) {
    uint64_t va = pa;
    va &= VA_MASK;
    uint64_t vpn2 = va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
    boot_pgdir[vpn2] = 0;
  }
}
/* allocate physical page for `va`, mapping it into `pgdir`,
   return the kernel virtual address for the page
   */
uintptr_t alloc_page_helper(uintptr_t va, uintptr_t pgdir) {
  // TODO [P4-task1] alloc_page_helper:
  va &= VA_MASK;
  uint64_t vpn2 = va >> PPN2_SHIFT;
  uint64_t vpn1 = (va >> PPN1_SHIFT) & PPN_MASK;
  uint64_t vpn0 = (va >> PPN0_SHIFT) & PPN_MASK;

  PTE *vpn2_PTE = (PTE *)pgdir;
  if (vpn2_PTE[vpn2] == 0) {
    // alloc a new second-level page directory
    set_pfn(&vpn2_PTE[vpn2], kva2pa(allocPage(1)) >> NORMAL_PAGE_SHIFT);
    set_attribute(&vpn2_PTE[vpn2], _PAGE_PRESENT);
    clear_pgdir(pa2kva(get_pa(vpn2_PTE[vpn2])));
  }
  PTE *vpn1_PTE = (PTE *)pa2kva(get_pa(vpn2_PTE[vpn2]));
  if (vpn1_PTE[vpn1] == 0) {
    set_pfn(&vpn1_PTE[vpn1], kva2pa(allocPage(1)) >> NORMAL_PAGE_SHIFT);
    set_attribute(&vpn1_PTE[vpn1], _PAGE_PRESENT);
    clear_pgdir(pa2kva(get_pa(vpn1_PTE[vpn1])));
  }
  PTE *vpn0_PTE = (PTE *)pa2kva(get_pa(vpn1_PTE[vpn1]));
  if (vpn0_PTE[vpn0] == 0) {
    set_pfn(&vpn0_PTE[vpn0], kva2pa(allocPage(1)) >> NORMAL_PAGE_SHIFT);
  }
  set_attribute(&vpn0_PTE[vpn0], _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE |
                                     _PAGE_EXEC | _PAGE_ACCESSED | _PAGE_DIRTY |
                                     _PAGE_USER);
  return pa2kva(get_pa(vpn0_PTE[vpn0]));
}

void map_page(uintptr_t va, uintptr_t pa, pcb_t *pcb) {
  va &= VA_MASK;
  uint64_t vpn2 = va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
  uint64_t vpn1 = (vpn2 << PPN_BITS) ^ (va >> (NORMAL_PAGE_SHIFT + PPN_BITS));
  uint64_t vpn0 = (vpn2 << (PPN_BITS + PPN_BITS)) ^ (vpn1 << PPN_BITS) ^
                  (va >> (NORMAL_PAGE_SHIFT));

  PTE *vpn2_PTE = (PTE *)pcb->pgdir;
  if (!get_attribute(vpn2_PTE[vpn2], _PAGE_PRESENT)) {
    // alloc a new second-level page directory
    set_pfn(&vpn2_PTE[vpn2], kva2pa(_allocPage(0, pcb)) >> NORMAL_PAGE_SHIFT);
    set_attribute(&vpn2_PTE[vpn2], _PAGE_PRESENT);
    clear_pgdir(pa2kva(get_pa(vpn2_PTE[vpn2])));
  }
  PTE *vpn1_PTE = (PTE *)pa2kva(get_pa(vpn2_PTE[vpn2]));
  if (!get_attribute(vpn1_PTE[vpn1], _PAGE_PRESENT)) {
    set_pfn(&vpn1_PTE[vpn1], kva2pa(_allocPage(0, pcb)) >> NORMAL_PAGE_SHIFT);
    set_attribute(&vpn1_PTE[vpn1], _PAGE_PRESENT);
    clear_pgdir(pa2kva(get_pa(vpn1_PTE[vpn1])));
  }
  PTE *vpn0_PTE = (PTE *)pa2kva(get_pa(vpn1_PTE[vpn1]));
  set_pfn(&vpn0_PTE[vpn0], pa >> NORMAL_PAGE_SHIFT);
  set_attribute(&vpn0_PTE[vpn0], _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE |
                                     _PAGE_EXEC | _PAGE_USER);
  local_flush_tlb_all();
}

void map_page_for_kernel(uintptr_t va, uintptr_t pa) {
  va &= VA_MASK;
  uint64_t vpn2 = va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
  uint64_t vpn1 = (vpn2 << PPN_BITS) ^ (va >> (NORMAL_PAGE_SHIFT + PPN_BITS));
  uint64_t vpn0 = (vpn2 << (PPN_BITS + PPN_BITS)) ^ (vpn1 << PPN_BITS) ^
                  (va >> (NORMAL_PAGE_SHIFT));

  PTE *pgd = (PTE *)pa2kva(PGDIR_PA);
  if (!get_attribute(pgd[vpn2], _PAGE_PRESENT)) {
    // alloc a new page directory
    set_pfn(&pgd[vpn2], kva2pa(allocPage(1)) >> NORMAL_PAGE_SHIFT);
    set_attribute(&pgd[vpn2], _PAGE_PRESENT);
    clear_pgdir(pa2kva(get_pa(pgd[vpn2])));
  }
  PTE *pmd = (PTE *)pa2kva(get_pa(pgd[vpn2]));
  if (!get_attribute(pmd[vpn1], _PAGE_PRESENT)) {
    set_pfn(&pmd[vpn1], kva2pa(allocPage(1)) >> NORMAL_PAGE_SHIFT);
    set_attribute(&pmd[vpn1], _PAGE_PRESENT);
    clear_pgdir(pa2kva(get_pa(pmd[vpn1])));
  }

  PTE *pte = (PTE *)pa2kva(get_pa(pmd[vpn1]));
  assert(!get_attribute(pte[vpn0], _PAGE_PRESENT));
  set_pfn(&pte[vpn0], pa >> NORMAL_PAGE_SHIFT);
  set_attribute(&pte[vpn0], _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE |
                                _PAGE_EXEC | _PAGE_USER);
  local_flush_tlb_all();
}

uintptr_t shm_page_get(int key) {
  // TODO [P4-task4] shm_page_get:
  for (int i = 0; i < SHM_PAGE_NUM_MAX; i++) {
    if (shm_pages[i].user_num > 0 && shm_pages[i].key == key) {
      for (uintptr_t va = SHM_VA_START;
           va < SHM_VA_START + PAGE_SIZE * SHM_PAGE_NUM_MAX; va += PAGE_SIZE) {
        if (check_get_kva_of_va(va, current_running[get_current_cpu_id()]) ==
            0) {
          shm_pages[i].user_num++;
          map_page(va, kva2pa(shm_pages[i].pgcb->kva),
                   current_running[get_current_cpu_id()]);
          return va;
        }
      }
      printl("[shm_page_get]: no available va\n");
      assert(0);
    }
  }

  for (int i = 0; i < SHM_PAGE_NUM_MAX; i++) {
    if (shm_pages[i].user_num == 0) {
      shm_pages[i].key = key;
      shm_pages[i].user_num++;
      if (list_is_empty(&free_list)) {
        swap_out();
      }
      shm_pages[i].pgcb = LIST2PGCB(dequeue(&free_list));
      clear_pgdir(shm_pages[i].pgcb->kva);
      for (uintptr_t va = SHM_VA_START; va < SHM_VA_END; va += PAGE_SIZE) {
        if (check_get_kva_of_va(va, current_running[get_current_cpu_id()]) ==
            0) {
          map_page(va, kva2pa(shm_pages[i].pgcb->kva),
                   current_running[get_current_cpu_id()]);
          return va;
        }
      }
      printl("[shm_page_get]: no available va\n");
      assert(0);
    }
  }
  return 0;
}

void shm_page_dt(uintptr_t addr) {
  // TODO [P4-task4] shm_page_dt:
  uintptr_t kva =
      get_kva_of_va(addr, current_running[get_current_cpu_id()]->pgdir);
  for (int i = 0; i < SHM_PAGE_NUM_MAX; i++) {
    if (kva == shm_pages[i].pgcb->kva && shm_pages[i].user_num > 0) {
      printl("[shm_page_dt]: disvalid addr:%lx\n", addr);
      disvalid_pte(addr, current_running[get_current_cpu_id()]->pgdir);
      shm_pages[i].user_num--;
      if (shm_pages[i].user_num == 0) {
        enqueue(&free_list, &shm_pages[i].pgcb->list);
      }
      return;
    }
  }
}
static inline PTE *get_pte(uintptr_t va, uintptr_t pgdir) {
  va &= VA_MASK;
  uint64_t vpn2 = va >> PPN2_SHIFT;
  uint64_t vpn1 = (va >> PPN1_SHIFT) & PPN_MASK;
  uint64_t vpn0 = (va >> PPN0_SHIFT) & PPN_MASK;

  PTE *pgd = (PTE *)pgdir;
  if (!get_attribute(pgd[vpn2], _PAGE_PRESENT)) {
    // no such entry
    return 0;
  } else if (get_attribute(pgd[vpn2], _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC)) {
    // pa found
    return &pgd[vpn2];
  }

  PTE *pmd = (PTE *)pa2kva(get_pa(pgd[vpn2]));
  if (!get_attribute(pmd[vpn1], _PAGE_PRESENT)) {
    // no such entry
    return 0;
  } else if (get_attribute(pmd[vpn1], _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC)) {
    // pa foundfind_swap_page
    return &pmd[vpn1];
  }

  PTE *pte = (PTE *)pa2kva(get_pa(pmd[vpn1]));
  if (!get_attribute(pte[vpn0], _PAGE_PRESENT)) {
    // no such entry
    return 0;
  } else if (get_attribute(pte[vpn0], _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC)) {
    // pa found
    return &pte[vpn0];
  }

  // four level pages
  assert(0);
  return 0;
}
list_node_t *find_swap_page(uintptr_t va, pcb_t *pcb) {
  list_node_t *node = pcb->swap_list.next;
  while (node != &pcb->swap_list) {
    swap_page_t *swap_page = LIST2SWAPPG(node);
    if ((swap_page->va >> NORMAL_PAGE_SHIFT) == (va >> NORMAL_PAGE_SHIFT) &&
        swap_page->owner_pid == pcb->pid) {
      return node;
    }
    node = node->next;
  }
  return NULL;
}
list_node_t *find_page(uintptr_t va, list_head *page_list) {
  list_node_t *node = page_list->next;
  while (node != page_list) {
    swap_page_t *swap_page = LIST2SWAPPG(node);
    if (swap_page->va == va) {
      return node;
    }
    node = node->next;
  }
  return NULL;
}
uintptr_t check_get_kva_of_va(uintptr_t va, pcb_t *pcb) {
  uintptr_t kva = get_kva_of_va(va, pcb->pgdir);
  if (!kva) {
    list_node_t *node = find_swap_page(va, pcb);
    if (node) {
      swap_page_t *swap_page = LIST2SWAPPG(node);
      swap_in(swap_page, pcb);
    } else {
      return 0;
    }
  }
  kva = get_kva_of_va(va, pcb->pgdir);
  return kva;
}
void printl_pagelist(list_head *list) {
  list_node_t *node = list->next;
  printl("[printl_pagelist]:\n");
  while (node != list) {
    pgcb_t *pgcb = LIST2PGCB(node);
    printl("pgcb->va:%lx,pgcb->kva:%lx,pgcb->owner_pid:%d\n", pgcb->va,
           pgcb->kva, pgcb->owner_pid);
    node = node->next;
  }
}
void printl_swaplist(list_head *list) {
  list_node_t *node = list->next;
  printl("[printl_swaplist]:\n");
  while (node != list) {
    swap_page_t *swap_page = LIST2SWAPPG(node);
    printl(
        "swap_page->va:%lx,swap_page->block_id:%lx,swap_page->owner_pid:%d\n",
        swap_page->va, swap_page->block_id, swap_page->owner_pid);
    node = node->next;
  }
}
void handle_other1(regs_context_t *regs, uint64_t stval, uint64_t scause) {
  char *reg_name[] = {
      "zero ", " ra  ", " sp  ", " gp  ", " tp  ", " t0  ", " t1  ", " t2  ",
      "s0/fp", " s1  ", " a0  ", " a1  ", " a2  ", " a3  ", " a4  ", " a5  ",
      " a6  ", " a7  ", " s2  ", " s3  ", " s4  ", " s5  ", " s6  ", " s7  ",
      " s8  ", " s9  ", " s10 ", " s11 ", " t3  ", " t4  ", " t5  ", " t6  "};
  for (int i = 0; i < 32; i += 3) {
    for (int j = 0; j < 3 && i + j < 32; ++j) {
      printk("%s : %016lx ", reg_name[i + j], regs->regs[i + j]);
    }
    printk("\n\r");
  }
  printk("sstatus: 0x%lx sbadaddr: 0x%lx scause: %lu\n\r", regs->sstatus,
         regs->sbadaddr, regs->scause);
  printk("sepc: 0x%lx\n\r", regs->sepc);
  printk("tval: 0x%lx cause: 0x%lx\n", stval, scause);
  assert(0);
}
void handle_page_fault(regs_context_t *regs, uint64_t stval, uint64_t scause) {
  printl("\n\n[page fault cpu %d]:\nstval: %lx\nscause: %d\npid: %d\n",
         get_current_cpu_id(), stval, scause,
         current_running[get_current_cpu_id()]->pid);
  pcb_t *pcb = current_running[get_current_cpu_id()];
  // printl_pagelist(&pcb->page_list);
  // printl_swaplist(&pcb->swap_list);
  uint64_t pgdir = pcb->pgdir;
  PTE *pte = (PTE *)get_pte(stval, pgdir);

  if (pte) {
    if (scause == EXCC_INST_PAGE_FAULT || scause == EXCC_LOAD_PAGE_FAULT) {
      assert(get_attribute(*pte, _PAGE_ACCESSED));
      set_attribute(pte, _PAGE_ACCESSED | get_attribute(*pte, 0xfflu));
    } else if (scause == EXCC_STORE_PAGE_FAULT) {
      if (!get_attribute(*pte, _PAGE_WRITE)) {
        // cannot write
        uintptr_t bad_kva =
            check_get_kva_of_va(stval, pcb) & ~(NORMAL_PAGE_SIZE - 1);
        uintptr_t new_kva =
            _alloc_page_helper(stval, pcb) & ~(NORMAL_PAGE_SIZE - 1);
        memcpy((void *)new_kva, (void *)bad_kva, PAGE_SIZE);
      } else if (!get_attribute(*pte, _PAGE_ACCESSED) ||
                 !get_attribute(*pte, _PAGE_DIRTY)) {
        set_attribute(pte, _PAGE_ACCESSED | _PAGE_DIRTY |
                               get_attribute(*pte, 0xfflu));
      } else {
        assert(0);
      }
    }
  } else {
    if (find_page(stval, &pcb->page_list)) {
      // page is in page_list, but not in pgdir
      printl_pagelist(&pcb->page_list);
      printl_swaplist(&pcb->swap_list);
      handle_other1(regs, stval, scause);
    } else {
      // page is not in page_list
      list_node_t *node = find_swap_page(stval, pcb);
      if (!node) {
        _alloc_page_helper(stval, pcb);
      } else {
        swap_in(LIST2SWAPPG(node), pcb);
      }
    }
  }
  local_flush_tlb_all();
  local_flush_icache_all();
}
void swap_out_page(int pgcb_idx) {
  assert((pgcbs[pgcb_idx].va >> 32) != 0xffffffc0);
  printl("[swap_out]\n");
  list_node_t *node = dequeue(&free_swap_list);
  assert(node != NULL);
  swap_page_t *swap_page = LIST2SWAPPG(node);
  swap_page->va = pgcbs[pgcb_idx].va;
  swap_page->owner_pid = pgcbs[pgcb_idx].owner_pid;

  printl("swap_out_page_va %lx\n", swap_page->va);
  printl("swap_out_page->block_id: %d\n", swap_page->block_id);
  printl("swap_out_page->owner_pid: %d\n", swap_page->owner_pid);
  enqueue(&pgcbs[pgcb_idx].pcb->swap_list, &swap_page->list);

  bios_sd_write(kva2pa(pgcbs[pgcb_idx].kva), PAGE_SIZE / SECTOR_SIZE,
                swap_page->block_id);
  printl("memaddress:%lx\nnumber of blocks:%d\nblock_id:%d\n",
         kva2pa(pgcbs[pgcb_idx].kva), PAGE_SIZE / SECTOR_SIZE,
         swap_page->block_id);
  printl("disvalid_pte:%lx\n", pgcbs[pgcb_idx].va);
  disvalid_pte(pgcbs[pgcb_idx].va, pgcbs[pgcb_idx].pcb->pgdir);
  pgcbs[pgcb_idx].va = 0;
  pgcbs[pgcb_idx].owner_pid = -1;
  list_remove(&pgcbs[pgcb_idx].list);
  enqueue(&free_list, &pgcbs[pgcb_idx].list);
}

void swap_out() {
  // int r = generate_random_number(0, USER_PAGE_NUM_MAX);
  // while (pgcbs[r].va == 0) {
  //   r = generate_random_number(0, USER_PAGE_NUM_MAX);
  // }
  // printl("swap_out_pgcb_idx:%d\n", r);
  // swap_out_page(r);
  static int x = 2022; // seed
  uint64_t tmp = 0x5deece66dll * x + 0xbll;
  x = tmp & 0x7fffffff;

  int lucky_pf_id = x % USER_PAGE_NUM_MAX;
  int found = 0;
  do {
    if (pgcbs[lucky_pf_id].va && (pgcbs[lucky_pf_id].va >> 32) != 0xffffffc0) {
      // the page is not a pgdir or kernel page
      // thus swappable
      found = 1;
      break;
    }
    lucky_pf_id++;
    lucky_pf_id %= USER_PAGE_NUM_MAX;
  } while (lucky_pf_id != x % USER_PAGE_NUM_MAX);

  if (!found) {
    // no available page to swap
    assert(0); // temporarily
  }
  swap_out_page(lucky_pf_id);
}

void swap_in(swap_page_t *swap_page, pcb_t *owner_pcb) {
  printl("[swap_in]\n");
  printl("swap_in_page_va %lx\n", swap_page->va);
  printl("swap_in_page->block_id: %d\n", swap_page->block_id);
  printl("swap_in_page->pid: %d\n", swap_page->owner_pid);
  ptr_t kva = _alloc_page_helper(swap_page->va, owner_pcb);
  bios_sd_read(kva2pa(kva), PAGE_SIZE / SECTOR_SIZE, swap_page->block_id);
  swap_page->va = 0;
  swap_page->owner_pid = -1;
  list_remove(&swap_page->list);
  enqueue(&free_swap_list, &swap_page->list);
}

//_allocPage of managed page table version, if va == -1, then use kva
ptr_t _allocPage(uintptr_t va, pcb_t *pcb) {
  if (list_is_empty(&free_list)) {
    swap_out();
  }
  list_node_t *node = dequeue(&free_list);
  pgcb_t *pgcb = LIST2PGCB(node);
  if (va == -1) {
    pgcb->va = pgcb->kva;
  } else {
    pgcb->va = va & (~(NORMAL_PAGE_SIZE - 1));
  }
  pgcb->owner_pid = pcb->pid;
  pgcb->pcb = pcb;
  enqueue(&pcb->page_list, &pgcb->list);
  return pgcb->kva;
}

//_alloc_page_helper of managed page table version
uintptr_t _alloc_page_helper(uintptr_t va, pcb_t *pcb) {
  // TODO [P4-task1] alloc_page_helper:
  va &= VA_MASK;
  uint64_t vpn2 = va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
  uint64_t vpn1 = (vpn2 << PPN_BITS) ^ (va >> (NORMAL_PAGE_SHIFT + PPN_BITS));
  uint64_t vpn0 = (vpn2 << (PPN_BITS + PPN_BITS)) ^ (vpn1 << PPN_BITS) ^
                  (va >> (NORMAL_PAGE_SHIFT));

  PTE *vpn2_PTE = (PTE *)pcb->pgdir;
  if (!get_attribute(vpn2_PTE[vpn2], _PAGE_PRESENT)) {
    // alloc a new second-level page directory
    set_pfn(&vpn2_PTE[vpn2], kva2pa(_allocPage(-1, pcb)) >> NORMAL_PAGE_SHIFT);
    set_attribute(&vpn2_PTE[vpn2], _PAGE_PRESENT);
    clear_pgdir(pa2kva(get_pa(vpn2_PTE[vpn2])));
  }
  PTE *vpn1_PTE = (PTE *)pa2kva(get_pa(vpn2_PTE[vpn2]));
  if (!get_attribute(vpn1_PTE[vpn1], _PAGE_PRESENT)) {
    set_pfn(&vpn1_PTE[vpn1], kva2pa(_allocPage(-1, pcb)) >> NORMAL_PAGE_SHIFT);
    set_attribute(&vpn1_PTE[vpn1], _PAGE_PRESENT);
    clear_pgdir(pa2kva(get_pa(vpn1_PTE[vpn1])));
  }
  PTE *vpn0_PTE = (PTE *)pa2kva(get_pa(vpn1_PTE[vpn1]));
  set_pfn(&vpn0_PTE[vpn0], kva2pa(_allocPage(va, pcb)) >> NORMAL_PAGE_SHIFT);
  set_attribute(&vpn0_PTE[vpn0], _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE |
                                     _PAGE_EXEC | _PAGE_USER);
  local_flush_tlb_all();
  return pa2kva(get_pa(vpn0_PTE[vpn0]));
}
void dealloc_all(PTE *pgdir) {
  int mask = _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC | _PAGE_USER;
  for (uint64_t i = 0; i < (NUM_PTE_ENTRY >> 1); i++) {
    if (pgdir[i] & _PAGE_PRESENT && (!get_attribute(pgdir[i], mask))) {
      PTE *pmd = (PTE *)pa2kva(get_pa(pgdir[i]));
      for (uint64_t j = 0; j < NUM_PTE_ENTRY; j++) {
        if (pmd[j] & _PAGE_PRESENT &&
            (!get_attribute(pmd[j], _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC |
                                        _PAGE_USER))) {
          PTE *pgt = (PTE *)pa2kva(get_pa(pmd[j]));
          for (uint64_t k = 0; k < NUM_PTE_ENTRY; k++) {
            printl("dealloc_all: entey:%lx\n", &pgt[k]);
            set_attribute(&pgt[k], 0);
            memset((void *)&pgt[k], 0, PAGE_SIZE);
          }
        }
      }
    }
  }
}