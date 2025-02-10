#include <assert.h>
#include <csr.h>
#include <os/list.h>
#include <os/loader.h>
#include <os/lock.h>
#include <os/mm.h>
#include <os/rand.h>
#include <os/sched.h>
#include <os/smp.h>
#include <os/string.h>
#include <os/task.h>
#include <os/time.h>
#include <printk.h>
// LCG parameters
#define LCG_A 1664525
#define LCG_C 1013904223
#define LCG_M 4294967296 // 2^32
#define USER_ENTRYPOINT 0x10000

pcb_t pcb[NUM_MAX_TASK];
const ptr_t pid0_stack = INIT_KERNEL_STACK + PAGE_SIZE;
const ptr_t pid1_stack = INIT_KERNEL_STACK + 2 * PAGE_SIZE;
pcb_t pid0_pcb = {.pid = 0,
                  .kernel_sp = (ptr_t)pid0_stack,
                  .user_sp = (ptr_t)pid0_stack,
                  .cpu_mask = 0x3};
pcb_t pid1_pcb = {.pid = 0,
                  .kernel_sp = (ptr_t)pid1_stack,
                  .user_sp = (ptr_t)pid1_stack,
                  .cpu_mask = 0x3};
LIST_HEAD(ready_queue);
LIST_HEAD(sleep_queue);
LIST_HEAD(prior_queue);
// spin_lock_t ready_queue_lock = {UNLOCKED};
// spin_lock_t sleep_queue_lock = {UNLOCKED};
// spin_lock_t prior_queue_lock;
extern void ret_from_exception();

list_node_t *get_prior_node();
static inline void printl_queue(const list_head *queue);

/* global process id */
pid_t process_id = 1;
pcb_t *volatile current_running[2];

pcb_t *get_next_process(pcb_t *prev_running) {
  pcb_t *next_running = NULL;
  for (list_node_t *node = list_front(&ready_queue);
       node != &ready_queue && node != 0; node = node->next) {
    pcb_t *pcb = LIST2PCB(node);
    if ((pcb->cpu_mask == 0x3 || pcb->cpu_mask == (get_current_cpu_id() + 1)) &&
        (pcb->status != TASK_RUNNING)) {
      next_running = pcb;
      next_running->status = TASK_RUNNING;
      next_running->cpu = get_current_cpu_id();
      list_remove(node);
      break;
    }
  }
  return next_running == NULL
             ? (get_current_cpu_id() == 0x0 ? &pid0_pcb : &pid1_pcb)
             : next_running;
}

// void do_scheduler(void) {
//   // TODO: [p2-task3] Check sleep queue to wake up PCBs
//   check_sleeping(); // p3 protected by spin_lock
//   /************************************************************/
//   /* Do not touch this comment. Reserved for future projects. */
//   /************************************************************/

//   // // spin_lock_acquire(&ready_queue_lock);
//   // TODO: [p2-task1] Modify the current_running pointer.
//   pcb_t *prev_running = current_running[get_current_cpu_id()];
//   if (prev_running->status == TASK_RUNNING) {
//     prev_running->status = TASK_READY;
//     enqueue(&ready_queue, &prev_running->list);
//   }
//   current_running[get_current_cpu_id()] = get_next_process(prev_running);
//   if (current_running[0] == current_running[1]) {
//     printl("err");
//     assert(0);
//   }
//   if (current_running[get_current_cpu_id()]->pid != prev_running->pid) {
//     set_satp(SATP_MODE_SV39, current_running[get_current_cpu_id()]->pid,
//              kva2pa((uintptr_t)current_running[get_current_cpu_id()]->pgdir)
//              >>
//                  NORMAL_PAGE_SHIFT);
//     local_flush_tlb_all();
//     local_flush_icache_all();
//   }
//   switch_to(prev_running, current_running[get_current_cpu_id()]);
// }
void do_scheduler(void) {
  // TODO: [p2-task3] Check sleep queue to wake up PCBs
  check_sleeping(); // p3 protected by spin_lock
  /************************************************************/
  /* Do not touch this comment. Reserved for future projects. */
  /************************************************************/
  // // spin_lock_acquire(&ready_queue_lock);
  // TODO: [p2-task1] Modify the current_running pointer.
  pcb_t *prev_running = current_running[get_current_cpu_id()];
  if (prev_running->status == TASK_RUNNING) {
    prev_running->status = TASK_READY;
    enqueue(&ready_queue, &prev_running->list);
  }
  if (is_queue_empty(&ready_queue) == 0) {
    int len = queue_len(&ready_queue);
    int switch_flag = 0;
    for (int i = 0; i < len; i++) {
      list_node_t *node = dequeue(&ready_queue);
      pcb_t *next_running = LIST2PCB(node);
      if (next_running != &pid0_pcb && next_running != &pid1_pcb &&
          (next_running == current_running[get_current_cpu_id() ? 0 : 1])) {
        list_remove(node);
        continue;
      }
      if (next_running->cpu_mask == 0x3 ||
          next_running->cpu_mask == (get_current_cpu_id() + 1)) {
        next_running->status = TASK_RUNNING;
        next_running->cpu = get_current_cpu_id();
        current_running[get_current_cpu_id()] = next_running;
        // TODO: [p2-task1] switch_to current_running
        switch_flag = 1;
        // // spin_lock_release(&ready_queue_lock);
        if (current_running[0] == current_running[1]) {
          printl("err");
          assert(0);
        }
        if (current_running[get_current_cpu_id()]->pid != prev_running->pid) {
          set_satp(
              SATP_MODE_SV39, current_running[get_current_cpu_id()]->pid,
              kva2pa((uintptr_t)current_running[get_current_cpu_id()]->pgdir) >>
                  NORMAL_PAGE_SHIFT);
          local_flush_tlb_all();
          local_flush_icache_all();
        }
        switch_to(prev_running, current_running[get_current_cpu_id()]);
        break;
      } else {
        enqueue(&ready_queue, node);
      }
    }
    if (switch_flag == 0) {
      // // spin_lock_release(&ready_queue_lock);
    }
    if (current_running[get_current_cpu_id()]->pid != 0 && switch_flag == 0) {
      current_running[get_current_cpu_id()] =
          get_current_cpu_id() == 0 ? &pid0_pcb : &pid1_pcb;
      if (current_running[get_current_cpu_id()]->pid != prev_running->pid) {
        set_satp(
            SATP_MODE_SV39, current_running[get_current_cpu_id()]->pid,
            kva2pa((uintptr_t)current_running[get_current_cpu_id()]->pgdir) >>
                NORMAL_PAGE_SHIFT);
        local_flush_tlb_all();
        local_flush_icache_all();
      }
      switch_to(prev_running, current_running[get_current_cpu_id()]);
    }
    // TODO: [p2-task1] switch_to current_running
  } else if (current_running[get_current_cpu_id()]->pid != 0) {
    // // spin_lock_release(&ready_queue_lock);
    current_running[get_current_cpu_id()] =
        get_current_cpu_id() == 0 ? &pid0_pcb : &pid1_pcb;
    if (current_running[get_current_cpu_id()]->pid != prev_running->pid) {
      set_satp(
          SATP_MODE_SV39, current_running[get_current_cpu_id()]->pid,
          kva2pa((uintptr_t)current_running[get_current_cpu_id()]->pgdir) >>
              NORMAL_PAGE_SHIFT);
      local_flush_tlb_all();
      local_flush_icache_all();
    }
    switch_to(prev_running, current_running[get_current_cpu_id()]);
  } else {
    // // spin_lock_release(&ready_queue_lock);
  }
}

void do_sleep(uint32_t sleep_time) {
  // TODO: [p2-task3] sleep(seconds)
  // NOTE: you can assume: 1 second = 1 `timebase` ticks
  // 1. block the current_running
  // 2. set the wake up time for the blocked task
  // 3. reschedule because the current_running is blocked.
  // spin_lock_acquire(&sleep_queue_lock);
  current_running[get_current_cpu_id()]->wakeup_time = get_timer() + sleep_time;
  do_block(&current_running[get_current_cpu_id()]->list, &sleep_queue);
  // spin_lock_release(&sleep_queue_lock);
}

void do_block(list_node_t *pcb_node, list_head *queue) {
  // TODO: [p2-task2] block the pcb task into the block queue);
  pcb_t *current_pcb = LIST2PCB(pcb_node);
  // spin_lock_acquire(&current_pcb->lock);
  enqueue(queue, pcb_node);
  current_pcb->status = TASK_BLOCKED;
  // spin_lock_release(&current_pcb->lock);
  // spin_lock_release(lock);
  do_scheduler();
  // spin_lock_acquire(lock);
}

void do_unblock(list_node_t *pcb_node) {
  // TODO: [p2-task2] unblock the `pcb` from the block queue
  pcb_t *current_pcb = LIST2PCB(pcb_node);
  // spin_lock_acquire(&current_pcb->lock);
  current_pcb->status = TASK_READY;
  enqueue(&ready_queue, pcb_node); // protected by spin_lock?
  // spin_lock_release(&current_pcb->lock);
}

void set_sche_workload(int num) {
  // p2-task5
  if (current_running[get_current_cpu_id()]->priority_base == -1) {
    // init priority_base for priority tasks
    current_running[get_current_cpu_id()]->priority_base = num;
    current_running[get_current_cpu_id()]->priority = 0;
  } else if (((current_running[get_current_cpu_id()]->priority_base - num) *
              MAX_PRIORITY) /
                 current_running[get_current_cpu_id()]->priority_base <
             current_running[get_current_cpu_id()]->priority) {
    current_running[get_current_cpu_id()]->priority = MAX_PRIORITY;
  } else {
    current_running[get_current_cpu_id()]->priority =
        ((current_running[get_current_cpu_id()]->priority_base - num) *
         MAX_PRIORITY) /
        current_running[get_current_cpu_id()]->priority_base;
  }
  printl("current_running->pid: %d    current_running->priority: %d\n",
         current_running[get_current_cpu_id()]->pid,
         current_running[get_current_cpu_id()]->priority);
}

list_node_t *get_prior_node() {
  // spin_lock_acquire(&prior_queue_lock);
  if (list_is_empty(&prior_queue)) {
    return NULL;
  }
  list_node_t *node = (&prior_queue)->next;
  list_node_t *target_node = node;
  uint32_t min_priority = MAX_PRIORITY;
  while (node != &prior_queue) {
    pcb_t *pcb = LIST2PCB(node);
    if (pcb->priority < min_priority) {
      min_priority = pcb->priority;
      target_node = node;
    }
    node = node->next;
  }
  if (min_priority == MAX_PRIORITY) {
    node = (&prior_queue)->next;
    while (node != &prior_queue) {
      pcb_t *pcb = LIST2PCB(node);
      pcb->priority = 0;
      node = node->next;
    }
  }
  list_remove(target_node);
  // spin_lock_release(&prior_queue_lock);
  return target_node;
}

static inline void printl_queue(const list_head *queue) {
  list_node_t *node = queue->next;
  while (node != queue) {
    printl("%d->", ((pcb_t *)((void *)node - 16))->pid);
    node = node->next;
  }
  printl("\n");
}

/*[p3]*********************************************************************************************/
regs_context_t *init_pcb_stack(ptr_t kernel_stack, ptr_t user_stack,
                               ptr_t entry_point, pcb_t *pcb) {
  /* TODO: [p2-task3] initialization of registers on kernel stack
   * HINT: sp, ra, sepc, sstatus
   * NOTE: To run the task in user mode, you should set corresponding bits
   *     of sstatus(SPP, SPIE, etc.).
   */
  regs_context_t *pt_regs =
      (regs_context_t *)(kernel_stack - sizeof(regs_context_t));
  for (int i = 0; i < 32; i++) {
    pt_regs->regs[i] = 0;
  }
  pt_regs->sepc = (reg_t)entry_point;
  pt_regs->sstatus = (reg_t)SR_SPIE | SR_SUM;
  // for (int i = 0; i < 32; i++) {
  //   pt_regs->regs[i] = 0;
  // }
  pt_regs->regs[1] = (reg_t)entry_point; // sp
  pt_regs->regs[2] = (reg_t)user_stack;  // sp
  pt_regs->regs[4] = (reg_t)pcb;         // tp

  /* TODO: [p2-task1] set sp to simulate just returning from switch_to
   * NOTE: you should prepare a stack, and push some values to
   * simulate a callee-saved context.
   */
  switchto_context_t *pt_switchto =
      (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));
  // for (int i = 0; i < 14; i++) {
  //   pt_switchto->regs[i] = 0;
  // }
  pt_switchto->regs[0] = (reg_t)ret_from_exception; // ra
  pt_switchto->regs[1] = (reg_t)pt_switchto;        // sp

  pcb->kernel_sp = (reg_t)pt_switchto;
  pcb->user_sp = user_stack;
  return pt_regs;
}
pid_t do_exec(char *name, int argc, char *argv[]) {
  for (int i = 0; i < argc; i++) {
    printl("argv[%d]: %s\n", i, argv[i]);
  }
  for (int i = 1; i < NUM_MAX_TASK; i++) {
    if ((pcb[i].status == TASK_EMPTY || pcb[i].status == TASK_EXITED)) {
      // page table
      pcb[i].page_list.next = &pcb[i].page_list;
      pcb[i].page_list.prev = &pcb[i].page_list;
      pcb[i].swap_list.next = &pcb[i].swap_list;
      pcb[i].swap_list.prev = &pcb[i].swap_list;
      pcb[i].wait_list.next = &pcb[i].wait_list;
      pcb[i].wait_list.prev = &pcb[i].wait_list;
      pcb[i].list.next = &pcb[i].list;
      pcb[i].list.prev = &pcb[i].list;
      pcb[i].pid = process_id++;
      pcb[i].tid = -1;
      pcb[i].pgdir = pcb[i].pgdir_origin = _allocPage(-1, &pcb[i]);
      clear_pgdir(pcb[i].pgdir);
      share_pgtable(pcb[i].pgdir, pa2kva(PGDIR_PA));
      uint64_t entry_addr;
      if ((entry_addr = load_task_img_va(name, TASK_MAXNUM, &pcb[i])) == 0) {
        assert(0);
        return -1;
      }
      // alloc stacks
      _allocPage(-1, &pcb[i]);
      _allocPage(-1, &pcb[i]);
      pcb[i].kernel_sp = _allocPage(-1, &pcb[i]) + PAGE_SIZE;
      pcb[i].user_sp = 0xf00010000;
      uint64_t user_sp_kva =
          _alloc_page_helper(pcb[i].user_sp - PAGE_SIZE, &pcb[i]) + PAGE_SIZE;

      pcb[i].status = TASK_READY;
      pcb[i].cursor_x = 0;
      pcb[i].cursor_y = 0;
      strncpy(pcb[i].name, name, strlen(name));
      pcb[i].name[strlen(name)] = '\0';
      pcb[i].priority = 0; // p2-task5 for none priority task, set to 0(highest)
      pcb[i].priority_base = -1;
      pcb[i].cpu_mask = current_running[get_current_cpu_id()]->cpu_mask;
      // ptr_t user_sp_original = pcb[i].user_sp;
      // ptr_t *argv_base =
      //     (ptr_t *)(user_sp_original - (argc + 1) * sizeof(char *));
      // ptr_t user_sp_now = (ptr_t)argv_base;
      // argv_base[argc] = (uint64_t)0;
      // for (int i = 0; i < argc; i++) {
      //   user_sp_now -= strlen(argv[i]) + 1;
      //   strcpy((char *)user_sp_now, argv[i]);
      //   argv_base[i] = user_sp_now;
      // }
      // user_sp_now &= ~0x7; // align 8
      // pcb[i].user_sp = user_sp_now;
      // pt_regs->regs[2] = (reg_t)user_sp_now;
      // pt_regs->regs[10] = (reg_t)argc;
      // pt_regs->regs[11] = (reg_t)argv_base;
      regs_context_t *pt_regs =
          init_pcb_stack(pcb[i].kernel_sp, pcb[i].user_sp, entry_addr, &pcb[i]);
      ptr_t kva_user_sp_original = user_sp_kva;
      ptr_t uva_user_sp_original = pcb[i].user_sp;

      ptr_t *kva_argv_base =
          (ptr_t *)(kva_user_sp_original - (argc + 1) * sizeof(char *));
      ptr_t *uva_argv_base =
          (ptr_t *)(uva_user_sp_original - (argc + 1) * sizeof(char *));

      ptr_t kva_user_sp_now = (ptr_t)kva_argv_base;
      ptr_t uva_user_sp_now = (ptr_t)uva_argv_base;

      kva_argv_base[argc] = (uint64_t)0;
      for (int i = 0; i < argc; i++) {
        kva_user_sp_now -= strlen(argv[i]) + 1;
        uva_user_sp_now -= strlen(argv[i]) + 1;
        strcpy((char *)kva_user_sp_now, argv[i]);
        kva_argv_base[i] = uva_user_sp_now;
      }

      kva_user_sp_now = ROUNDDOWN(kva_user_sp_now, 128);
      uva_user_sp_now = ROUNDDOWN(uva_user_sp_now, 128);

      pcb[i].user_sp = uva_user_sp_now;
      pt_regs->regs[2] = (reg_t)uva_user_sp_now;
      pt_regs->regs[10] = (reg_t)argc;
      pt_regs->regs[11] = (reg_t)uva_argv_base;
      // printk("\nINFO[exec]: execute \"%s\" successfully, pid: %d\n", name,
      //        pcb[i].pid);

      enqueue(&ready_queue, &pcb[i].list);

      return pcb[i].pid;
    }
  }
  return -1;
}
int do_taskset_p(uint16_t mask, pid_t pid) {
  for (int i = 0; i < NUM_MAX_TASK; i++) {
    // spin_lock_acquire(&pcb[i].lock);
    if (pcb[i].status != TASK_EMPTY && pcb[i].status != TASK_EXITED) {
      if (pcb[i].pid == pid) {
        pcb[i].cpu_mask = mask;
        // spin_lock_release(&pcb[i].lock);
        return 1;
      }
    }
    // spin_lock_release(&pcb[i].lock);
  }
  return -1;
}

pid_t do_taskset(char *name, int argc, char *argv[], uint16_t mask) {
  //   for (int i = 1; i < NUM_MAX_TASK; i++) {
  //     // spin_lock_acquire(&pcb[i].lock);
  //     if ((pcb[i].status == TASK_EMPTY || pcb[i].status == TASK_EXITED)) {
  //       uint64_t entry_addr;
  //       if ((entry_addr = load_task_img(name, TASK_MAXNUM)) == 0) {
  //         // printk("\nERROR[exec]: load task failed, task \"%s\" not
  //         found.\n",
  //         //        name);
  //         return -1;
  //       }
  //       pcb[i].wait_list.next = &pcb[i].wait_list;
  //       pcb[i].wait_list.prev = &pcb[i].wait_list;
  //       pcb[i].list.next = &pcb[i].list;
  //       pcb[i].list.prev = &pcb[i].list;
  //       pcb[i].pid = process_id++;
  //       pcb[i].status = TASK_READY;
  //       pcb[i].cursor_x = 0;
  //       pcb[i].cursor_y = 0;
  //       strncpy(pcb[i].name, name, strlen(name));
  //       pcb[i].name[strlen(name)] = '\0';
  //       pcb[i].priority = 0; // p2-task5 for none priority task, set to
  //       0(highest) pcb[i].priority_base = -1; pcb[i].cpu_mask = mask;
  //       regs_context_t *pt_regs =
  //           init_pcb_stack(allocPage(2) + 2 * PAGE_SIZE,
  //                          allocPage(2) + 2 * PAGE_SIZE, entry_addr,
  //                          &pcb[i]);

  //       ptr_t user_sp_original = pcb[i].user_sp;
  //       ptr_t *argv_base =
  //           (ptr_t *)(user_sp_original - (argc + 1) * sizeof(char *));
  //       ptr_t user_sp_now = (ptr_t)argv_base;
  //       argv_base[argc] = (uint64_t)0;
  //       for (int i = 0; i < argc; i++) {
  //         user_sp_now -= strlen(argv[i]) + 1;
  //         strcpy((char *)user_sp_now, argv[i]);
  //         argv_base[i] = user_sp_now;
  //       }
  //       user_sp_now &= ~0x7; // align 8
  //       pcb[i].user_sp = user_sp_now;
  //       pt_regs->regs[2] = (reg_t)user_sp_now;
  //       pt_regs->regs[10] = (reg_t)argc;
  //       pt_regs->regs[11] = (reg_t)argv_base;
  //       // printk("\nINFO[exec]: execute \"%s\" successfully, pid: %d\n",
  //       name,
  //       //        pcb[i].pid);
  //       // // spin_lock_acquire(&ready_queue_lock);
  //       enqueue(&ready_queue, &pcb[i].list);
  //       // // spin_lock_release(&ready_queue_lock);
  //       // spin_lock_release(&pcb[i].lock);
  //       return pcb[i].pid;
  //     }
  //     // spin_lock_release(&pcb[i].lock);
  //   }
  //   return -1;
  return 0;
}

void do_exit(void) {
  if (current_running[get_current_cpu_id()]->tid != -1) {
    do_kill_thread(current_running[get_current_cpu_id()]->pid,
                   current_running[get_current_cpu_id()]->tid);
  } else {
    do_kill(current_running[get_current_cpu_id()]->pid);
  }
  do_scheduler();
}
int do_kill_thread(pid_t pid, pid_t tid) {
  for (int i = 0; i < NUM_MAX_TASK; i++) {
    if (pcb[i].status != TASK_EMPTY) {
      if (pcb[i].pid == pid && pcb[i].tid == tid) {
        // printk("\nINFO[kill]: kill task %d\n", pid);
        pcb[i].status = TASK_EXITED;
        pcb[i].recycle = 1;
        list_remove(&pcb[i].list); // how to protect?
        list_node_t *node;
        while ((node = dequeue(&pcb[i].wait_list)) != NULL) {
          do_unblock(node);
        }

        for (int lock_id = 0; lock_id < LOCK_NUM; lock_id++) {
          if (mlocks[lock_id].owner_pid == pid &&
              mlocks[lock_id].owner_tid == tid) {
            mlocks[lock_id].owner_pid = -1;
            mlocks[lock_id].owner_tid = -1;
            mlocks[lock_id].key = -1;
            list_node_t *unblocked_node;

            if ((unblocked_node = dequeue(&mlocks[lock_id].block_queue)) ==
                NULL) {
              mlocks[lock_id].lock.status = UNLOCKED;
              mlocks[lock_id].owner_pid = -1;
              mlocks[lock_id].owner_tid = -1;
            } else {
              mlocks[lock_id].owner_pid = LIST2PCB(unblocked_node)->pid;
              mlocks[lock_id].owner_tid = LIST2PCB(unblocked_node)->tid;
              do_unblock(unblocked_node);
            }
          }
        }
        return 1;
      }
    }
  }
  return -1;
}
int do_kill(pid_t pid) {
  int have_son_threads = 0;
  for (int i = 0; i < NUM_MAX_TASK; i++) {
    if (pcb[i].status != TASK_EMPTY) {
      if (pcb[i].pid == pid) {
        // printk("\nINFO[kill]: kill task %d\n", pid);
        if (pcb[i].thread_num != 0 || pcb[i].tid != -1) {
          have_son_threads = 1;
        }
        pcb[i].status = TASK_EXITED;
        pcb[i].recycle = 1;
        list_remove(&pcb[i].list); // how to protect?
        list_node_t *node;
        while ((node = dequeue(&pcb[i].wait_list)) != NULL) {
          do_unblock(node);
        }

        for (int lock_id = 0; lock_id < LOCK_NUM; lock_id++) {
          if (mlocks[lock_id].owner_pid == pid &&
              mlocks[lock_id].owner_tid == -1) {
            mlocks[lock_id].owner_pid = -1;
            mlocks[lock_id].owner_tid = -1;
            mlocks[lock_id].key = -1;
            list_node_t *unblocked_node;

            if ((unblocked_node = dequeue(&mlocks[lock_id].block_queue)) ==
                NULL) {
              mlocks[lock_id].lock.status = UNLOCKED;
              mlocks[lock_id].owner_pid = -1;
              mlocks[lock_id].owner_tid = -1;
            } else {
              mlocks[lock_id].owner_pid = LIST2PCB(unblocked_node)->pid;
              mlocks[lock_id].owner_tid = LIST2PCB(unblocked_node)->tid;
              do_unblock(unblocked_node);
            }
          }
        }
        if (pcb[i].tid == -1) {
          // dealloc_all((PTE *)pcb[i].pgdir);
          while ((node = dequeue(&pcb[i].page_list)) != NULL) {
            pgcb_t *pgcb = LIST2PGCB(node);
            pgcb->va = 0;
            pgcb->owner_pid = -1;
            list_remove(node);
            enqueue(&free_list, node);
          }
          while ((node = dequeue(&pcb[i].swap_list)) != NULL) {
            swap_page_t *swap_page = LIST2SWAPPG(node);
            swap_page->va = 0;
            swap_page->owner_pid = -1;
            list_remove(node);
            enqueue(&free_swap_list, node);
          }
          pcb[i].swap_list.next = &pcb[i].swap_list;
          pcb[i].swap_list.prev = &pcb[i].swap_list;
        }

        if (!have_son_threads) {
          return 1;
        }
      }
    }
  }
  // printk("INFO[kill]: task %d not found\n", pid);
  if (!have_son_threads) {
    return -1;
  } else {
    return 1;
  }
}
int do_waitpid(pid_t pid) {
  for (int i = 0; i < NUM_MAX_TASK; i++) {
    if (pcb[i].status != TASK_EMPTY) {
      if (pcb[i].pid == pid) {
        if (pcb[i].status == TASK_EXITED) {
          // printk("do_waitpid: task %d had exited\n", pid);
          return -1;
        } else {
          // printk("do_waitpid: task %d is waiting for task %d\n",
          //        current_running->pid, pid);
          do_block(&current_running[get_current_cpu_id()]->list,
                   &pcb[i].wait_list);
          return pid;
        }
      }
    }
  }
  // printk("do_waitpid: task %d not found\n", pid);
  return -1;
}

void do_process_show() {
  printk("\n[PROCESS TABLE]:\n");
  int ps_num = 0;
  for (int i = 0; i < NUM_MAX_TASK; i++) {
    // spin_lock_acquire(&pcb[i].lock);
    if ((pcb[i].status != TASK_EMPTY && pcb[i].status != TASK_EXITED) &&
        pcb[i].pid != 0) {
      char status[10];
      int running = 0;
      if (pcb[i].status == TASK_RUNNING) {
        strcpy(status, "RUNNING");
        running = 1;
      } else if (pcb[i].status == TASK_READY) {
        strcpy(status, "READY");
      } else if (pcb[i].status == TASK_BLOCKED) {
        strcpy(status, "BLOCKED");
      }
      printk("[%d] ", ps_num++);
      printk("PID : %d  STATUS: %s  NAME: %s  mask: 0x%x", pcb[i].pid, status,
             pcb[i].name, pcb[i].cpu_mask);
      if (running) {
        printk("  Running on core %d\n", pcb[i].cpu);
      } else {
        printk("\n");
      }
    }
    // spin_lock_release(&pcb[i].lock);
  }
}
pid_t do_getpid() { return current_running[get_current_cpu_id()]->pid; }

void pthread_create(int32_t *tidptr, uint64_t start_routine, void *arg) {

  pcb_t *father_pcb = current_running[get_current_cpu_id()];
  for (int i = 1; i < NUM_MAX_TASK; i++) {
    if ((pcb[i].status == TASK_EMPTY || pcb[i].status == TASK_EXITED)) {
      // page table
      pcb[i].pid = father_pcb->pid;
      pcb[i].tid = father_pcb->thread_num++;
      *tidptr = pcb[i].tid;
      pcb[i].pgdir = father_pcb->pgdir;
      // alloc stacks
      pcb[i].kernel_sp = _allocPage(-1, father_pcb) + PAGE_SIZE;
      pcb[i].user_sp = 0xf10010000lu + 0x10000 * pcb[i].tid;
      _alloc_page_helper(pcb[i].user_sp - PAGE_SIZE, father_pcb);
      pcb[i].page_list = father_pcb->page_list;
      pcb[i].swap_list = father_pcb->swap_list;
      pcb[i].wait_list.next = &pcb[i].wait_list;
      pcb[i].wait_list.prev = &pcb[i].wait_list;
      pcb[i].list.next = &pcb[i].list;
      pcb[i].list.prev = &pcb[i].list;
      pcb[i].status = TASK_READY;
      pcb[i].cursor_x = 0;
      pcb[i].cursor_y = 0;
      strncpy(pcb[i].name, father_pcb->name, strlen(father_pcb->name));
      pcb[i].name[strlen(father_pcb->name)] = '\0';
      pcb[i].priority = 0; // p2-task5 for none priority task, set to 0(highest)
      pcb[i].priority_base = -1;
      pcb[i].cpu_mask = current_running[get_current_cpu_id()]->cpu_mask;

      regs_context_t *pt_regs = init_pcb_stack(pcb[i].kernel_sp, pcb[i].user_sp,
                                               start_routine, &pcb[i]);
      pt_regs->regs[10] = (reg_t)arg;
      // printk("\nINFO[exec]: execute \"%s\" successfully, pid: %d\n", name,
      //        pcb[i].pid);

      enqueue(&ready_queue, &pcb[i].list);
      return;
    }
  }
}

int pthread_join(pid_t tid) {
  for (int i = 0; i < NUM_MAX_TASK; i++) {
    if (pcb[i].status != TASK_EMPTY) {
      if (pcb[i].pid == current_running[get_current_cpu_id()]->pid &&
          pcb[i].tid == tid) {
        if (pcb[i].status == TASK_EXITED) {
          // printk("do_waitpid: task %d had exited\n", pid);
          return -1;
        } else {
          // printk("do_waitpid: task %d is waiting for task %d\n",
          //        current_running->pid, pid);
          do_block(&current_running[get_current_cpu_id()]->list,
                   &pcb[i].wait_list);
          return 1;
        }
      }
    }
  }
  // printk("do_waitpid: task %d not found\n", pid);
  return -1;
}
