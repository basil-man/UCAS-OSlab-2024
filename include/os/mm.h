/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * *
 * * * * * * Copyright (C) 2018 Institute of Computing Technology, CAS
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * *
 * * * * * * Memory Management
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * *
 * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * *
 * * * * * */
#ifndef MM_H
#define MM_H

#include <os/sched.h>
#include <pgtable.h>
#include <type.h>

#define MAP_KERNEL 1
#define MAP_USER 2
#define MEM_SIZE 32
#define PAGE_SIZE 4096 // 4K
#define INIT_KERNEL_STACK 0xffffffc052000000
#define FREEMEM_KERNEL (INIT_KERNEL_STACK + PAGE_SIZE)

/* Rounding; only works for n = power of two */
#define ROUND(a, n) (((((uint64_t)(a)) + (n) - 1)) & ~((n) - 1))
#define ROUNDDOWN(a, n) (((uint64_t)(a)) & ~((n) - 1))
#define LIST2PGCB(listptr) ((pgcb_t *)((void *)listptr))
#define LIST2SWAPPG(listptr) ((swap_page_t *)((void *)listptr))
extern ptr_t allocPage(int numPage);
// TODO [P4-task1] */
void freePage(ptr_t baseAddr);

// #define S_CORE
// NOTE: only need for S-core to alloc 2MB large page
#ifdef S_CORE
#define LARGE_PAGE_FREEMEM 0xffffffc056000000
#define USER_STACK_ADDR 0x400000
extern ptr_t allocLargePage(int numPage);
#else
// NOTE: A/C-core
#define USER_STACK_ADDR 0xf00010000
#define USER_PAGE_NUM_MAX 256
#endif

typedef struct pgcb {
  list_node_t list;
  uint64_t kva;
  uint64_t va;
  pid_t owner_pid;
  pcb_t *pcb;
} pgcb_t;
extern pgcb_t pgcbs[USER_PAGE_NUM_MAX];

extern uint32_t img_end_sector;
extern uint32_t swap_out_page_sector_offset;
extern list_head free_list;

// swap
#define SWAP_PAGE_NUM_MAX 2048
typedef struct swap_page {
  list_node_t list;
  int block_id;
  uint64_t va;
  pid_t owner_pid;
} swap_page_t;
extern swap_page_t swap_pages[SWAP_PAGE_NUM_MAX];
extern list_head free_swap_list;

// shm
#define SHM_PAGE_NUM_MAX 30
#define SHM_VA_START 0xf80000000lu
#define SHM_VA_END 0xf80100000lu
typedef struct shm_page {
  int key;
  pgcb_t *pgcb;
  int user_num;
} shm_page_t;
extern shm_page_t shm_pages[SHM_PAGE_NUM_MAX];

// TODO [P4-task1] */
void init_pages();
extern void *kmalloc(size_t size);
extern void share_pgtable(uintptr_t dest_pgdir, uintptr_t src_pgdir);
extern uintptr_t alloc_page_helper(uintptr_t va, uintptr_t pgdir);
void unmap_boot_pages();
// TODO [P4-task4]: shm_page_get/dt */
uintptr_t shm_page_get(int key);
void shm_page_dt(uintptr_t addr);
void handle_page_fault(regs_context_t *regs, uint64_t stval, uint64_t scause);
void swap_out_page(int pgcb_idx);
void swap_out();
void swap_in(swap_page_t *swap_page, pcb_t *owner_pcb);
ptr_t _allocPage(uintptr_t va, pcb_t *pcb);
uintptr_t _alloc_page_helper(uintptr_t va, pcb_t *pcb);
void dealloc_all(PTE *pgdir);
uintptr_t check_get_kva_of_va(uintptr_t va, pcb_t *pcb);
void map_page_for_kernel(uintptr_t va, uintptr_t pa);
#endif /* MM_H */
