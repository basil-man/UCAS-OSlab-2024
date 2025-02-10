#include <asm.h>
#include <asm/unistd.h>
#include <assert.h>
#include <common.h>
#include <csr.h>
#include <e1000.h>
#include <os/fs.h>
#include <os/ioremap.h>
#include <os/irq.h>
#include <os/kernel.h>
#include <os/loader.h>
#include <os/lock.h>
#include <os/mm.h>
#include <os/net.h>
#include <os/sched.h>
#include <os/smp.h>
#include <os/string.h>
#include <os/task.h>
#include <os/time.h>
#include <plic.h>
#include <printk.h>
#include <screen.h>
#include <sys/syscall.h>
#include <type.h>

extern void ret_from_exception();

// Task info array
task_info_t tasks[TASK_MAXNUM];
// Task num (task4)
uint16_t tasknum;
uint32_t img_end_sector;

spin_lock_t slave_lock = {LOCKED};
void disable_tmp_map() {
  PTE *pgdir = (PTE *)pa2kva(PGDIR_PA);
  for (uint64_t va = 0x50000000lu; va < 0x51000000lu; va += 0x200000lu) {
    va &= VA_MASK;
    uint64_t vpn2 = va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
    uint64_t vpn1 = (vpn2 << PPN_BITS) ^ (va >> (NORMAL_PAGE_SHIFT + PPN_BITS));
    PTE *pmd = (PTE *)pa2kva(get_pa(pgdir[vpn2]));
    // 表项置零
    pmd[vpn1] = 0;
  }
}
void wr_tp(uint64_t tp) { asm volatile("mv tp, %0" : : "r"(tp)); }
static void init_jmptab(void) {
  volatile long (*(*jmptab))() = (volatile long (*(*))())KERNEL_JMPTAB_BASE;

  jmptab[CONSOLE_PUTSTR] = (volatile long (*)())port_write;
  jmptab[CONSOLE_PUTCHAR] = (volatile long (*)())port_write_ch;
  jmptab[CONSOLE_GETCHAR] = (volatile long (*)())port_read_ch;
  jmptab[SD_READ] = (volatile long (*)())sd_read;
  jmptab[SD_WRITE] = (volatile long (*)())sd_write;
  jmptab[QEMU_LOGGING] = (volatile long (*)())qemu_logging;
  jmptab[SET_TIMER] = (volatile long (*)())set_timer;
  jmptab[READ_FDT] = (volatile long (*)())read_fdt;
  jmptab[MOVE_CURSOR] = (volatile long (*)())screen_move_cursor;
  jmptab[PRINT] = (volatile long (*)())printk;
  jmptab[YIELD] = (volatile long (*)())do_scheduler;
  jmptab[MUTEX_INIT] = (volatile long (*)())do_mutex_lock_init;
  jmptab[MUTEX_ACQ] = (volatile long (*)())do_mutex_lock_acquire;
  jmptab[MUTEX_RELEASE] = (volatile long (*)())do_mutex_lock_release;

  // TODO: [p2-task1] (S-core) initialize system call table.
  jmptab[WRITE] = (volatile long (*)())screen_write;
  jmptab[REFLUSH] = (volatile long (*)())screen_reflush;
}

static void init_task_info(void) {
  // TODO: [p1-task4] Init 'tasks' array via reading app-info sector
  // NOTE: You need to get some related arguments from bootblock first
  tasknum = *(uint16_t *)0xffffffc0502001fa;

  uint32_t task_end_addr = *(uint32_t *)0xffffffc0502001f2;
  uint32_t nbytes_taskinfo = *(uint32_t *)0xffffffc0502001f6;
  int task_end_block = task_end_addr / SECTOR_SIZE;
  int block_num = NBYTES2SEC(task_end_addr + nbytes_taskinfo) - task_end_block;
  img_end_sector = task_end_block + block_num;
  bios_sd_read(0x59000000, block_num, task_end_block);
  task_info_t *taskinfo_offset =
      (task_info_t *)(0xffffffc059000000 +
                      (void *)((uint64_t)task_end_addr % SECTOR_SIZE));
  memcpy((uint8_t *)tasks, (uint8_t *)taskinfo_offset,
         TASK_MAXNUM * sizeof(task_info_t));
}

/************************************************************/
// static void init_pcb_stack(ptr_t kernel_stack, ptr_t user_stack,
//                            ptr_t entry_point, pcb_t *pcb) {
//   /* TODO: [p2-task3] initialization of registers on kernel stack
//    * HINT: sp, ra, sepc, sstatus
//    * NOTE: To run the task in user mode, you should set corresponding bits
//    *     of sstatus(SPP, SPIE, etc.).
//    */
//   regs_context_t *pt_regs =
//       (regs_context_t *)(kernel_stack - sizeof(regs_context_t));

//   pt_regs->sepc = (reg_t)entry_point;
//   pt_regs->sstatus = (reg_t)SR_SPIE;
//   // for (int i = 0; i < 32; i++) {
//   //   pt_regs->regs[i] = 0;
//   // }
//   pt_regs->regs[1] = (reg_t)entry_point; // sp
//   pt_regs->regs[2] = (reg_t)user_stack;  // sp
//   pt_regs->regs[4] = (reg_t)pcb;         // tp

//   /* TODO: [p2-task1] set sp to simulate just returning from switch_to
//    * NOTE: you should prepare a stack, and push some values to
//    * simulate a callee-saved context.
//    */
//   switchto_context_t *pt_switchto =
//       (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));
//   // for (int i = 0; i < 14; i++) {
//   //   pt_switchto->regs[i] = 0;
//   // }
//   pt_switchto->regs[0] = (reg_t)ret_from_exception; // ra
//   pt_switchto->regs[1] = (reg_t)pt_switchto;        // sp

//   pcb->kernel_sp = (reg_t)pt_switchto;
//   pcb->user_sp = user_stack;
// }

static void init_pcb(void) {
  /* TODO: [p2-task1] load needed tasks and init their corresponding PCB */
  // char task_list[][15] = {"print1", "print2", "lock1", "lock2",
  //                         "sleep",  "timer",  "fly"};

  // [p2-task5] task_list
  // char task_list[][15] = {"print1", "print2", "lock1", "lock2",
  //                         "sleep",  "timer",  "fly",   "fly1",
  //                         "fly2",   "fly3",   "fly4",  "fly5"};

  // [p3-task1] task_list
  // char task_list[][15] = {"shell"};
  // for (int i = 1; i < NUM_MAX_TASK; i++) {
  //   pcb[i].status = TASK_EMPTY;
  // }

  // for (int i = 1; i <= sizeof(task_list) / 15; i++) {
  //   pcb[i].list.next = &pcb[i].list;
  //   pcb[i].list.prev = &pcb[i].list;
  //   pcb[i].wait_list.next = &pcb[i].wait_list;
  //   pcb[i].wait_list.prev = &pcb[i].wait_list;
  //   printl("process_id: %d\n", process_id);
  //   pcb[i].pid = process_id++;
  //   pcb[i].status = TASK_READY;
  //   pcb[i].cursor_x = 0;
  //   pcb[i].cursor_y = 0;
  //   strncpy(pcb[i].name, task_list[i - 1], sizeof(pcb[i].name) - 1);
  //   pcb[i].name[sizeof(pcb[i].name) - 1] = '\0';
  //   pcb[i].priority = 0; // p2-task5 for none priority task, set to
  //   0(highest) pcb[i].priority_base = -1; pcb[i].cpu_mask = 0x3; pcb[i].cpu =
  //   get_current_cpu_id(); init_pcb_stack(allocKernelPage(1) + PAGE_SIZE,
  //   allocUserPage(1) + PAGE_SIZE,
  //                  load_task_img(task_list[i - 1], tasknum), &pcb[i]);
  //   enqueue(&ready_queue, &pcb[i].list);
  // }
  // pcb[0].list.next = &pcb[0].list;
  // pcb[0].list.prev = &pcb[0].list;
  // pcb[0].wait_list.next = &pcb[0].wait_list;
  // pcb[0].wait_list.prev = &pcb[0].wait_list;
  // pcb[0].pid = 0;
  // pcb[0].status = TASK_EMPTY;
  // pcb[0].cursor_x = 0;
  // pcb[0].cursor_y = 0;
  // pcb[0].priority = 0;
  // pcb[0].priority_base = -1;
  // pcb[0].cpu_mask = 0x3;
  // pcb[0].cpu = get_current_cpu_id();
  // /* TODO: [p2-task1] remember to initialize 'current_running' */
  // pid0_pcb.status = TASK_BLOCKED;
  // pid1_pcb.status = TASK_BLOCKED;
  // current_running[get_current_cpu_id()] = &pid0_pcb;
  pid0_pcb.status = TASK_BLOCKED;
  pid0_pcb.list.next = &pid0_pcb.list;
  pid0_pcb.list.prev = &pid0_pcb.list;
  pid0_pcb.pid = 0;
  pid0_pcb.pgdir = pa2kva(0x51000000lu);
  pid0_pcb.cpu_mask = 0x3;
  pid0_pcb.recycle = 0;
  pid0_pcb.page_list.next = &pid0_pcb.page_list;
  pid0_pcb.page_list.prev = &pid0_pcb.page_list;
  pid0_pcb.swap_list.next = &pid0_pcb.swap_list;
  pid0_pcb.swap_list.prev = &pid0_pcb.swap_list;
  pid1_pcb.status = TASK_BLOCKED;
  pid1_pcb.list.next = &pid1_pcb.list;
  pid1_pcb.list.prev = &pid1_pcb.list;
  pid1_pcb.cpu_mask = 0x3;
  pid1_pcb.pid = 0;
  pid1_pcb.pgdir = pa2kva(0x51000000lu);
  pid1_pcb.recycle = 0;
  pid1_pcb.page_list.next = &pid1_pcb.page_list;
  pid1_pcb.page_list.prev = &pid1_pcb.page_list;
  pid1_pcb.swap_list.next = &pid1_pcb.swap_list;
  pid1_pcb.swap_list.prev = &pid1_pcb.swap_list;
  int pid = 0;
  for (int i = 0; i < NUM_MAX_TASK; i++) {
    pcb[i].status = TASK_EMPTY;
    pcb[i].list.next = &pcb[i].list;
    pcb[i].list.prev = &pcb[i].list;
    pcb[i].wait_list.next = &pcb[i].wait_list;
    pcb[i].wait_list.prev = &pcb[i].wait_list;
    pcb[i].swap_list.next = &pcb[i].swap_list;
    pcb[i].swap_list.prev = &pcb[i].swap_list;
    pcb[i].page_list.next = &pcb[i].page_list;
    pcb[i].page_list.prev = &pcb[i].page_list;
    pcb[i].pid = pid++;
    pcb[i].status = TASK_EMPTY;
    pcb[i].cursor_x = 0;
    pcb[i].cursor_y = 0;
    pcb[i].priority = 0;
    pcb[i].priority_base = -1;
    pcb[i].cpu_mask = 0x3;
    pcb[i].cpu = get_current_cpu_id();
    pcb[i].recycle = 0;
    pcb[i].tid = -1;
    pcb[i].thread_num = 0;
  }
  current_running[get_current_cpu_id()] = &pid0_pcb;
  wr_tp((uint64_t)&pid0_pcb);
}

static void init_syscall(void) {
  // TODO: [p2-task3] initialize system call table.
  syscall[SYSCALL_SLEEP] = (long (*)())do_sleep;
  syscall[SYSCALL_YIELD] = (long (*)())do_scheduler;
  syscall[SYSCALL_WRITE] = (long (*)())screen_write;
  syscall[SYSCALL_CURSOR] = (long (*)())screen_move_cursor;
  syscall[SYSCALL_REFLUSH] = (long (*)())screen_reflush;
  syscall[SYSCALL_GET_TIMEBASE] = (long (*)())get_time_base;
  syscall[SYSCALL_GET_TICK] = (long (*)())get_ticks;
  syscall[SYSCALL_LOCK_INIT] = (long (*)())do_mutex_lock_init;
  syscall[SYSCALL_LOCK_ACQ] = (long (*)())do_mutex_lock_acquire;
  syscall[SYSCALL_LOCK_RELEASE] = (long (*)())do_mutex_lock_release;
  syscall[SYSCALL_PRINTL] = (long (*)())printl;
  syscall[SYSCALL_SET_SCHE_WORKLOAD] =
      (long (*)())set_sche_workload; // p2-task5
  syscall[SYSCALL_EXEC] = (long (*)())do_exec;
  syscall[SYSCALL_EXIT] = (long (*)())do_exit;
  syscall[SYSCALL_KILL] = (long (*)())do_kill;
  syscall[SYSCALL_WAITPID] = (long (*)())do_waitpid;
  syscall[SYSCALL_PS] = (long (*)())do_process_show;
  syscall[SYSCALL_GETPID] = (long (*)())do_getpid;
  syscall[SYSCALL_READCH] = (long (*)())bios_getchar;
  syscall[SYSCALL_CLEAR] = (long (*)())screen_clear;
  syscall[SYSCALL_BACKSPACE] = (long (*)())screen_backspace;
  syscall[SYSCALL_BARR_INIT] = (long (*)())do_barrier_init;
  syscall[SYSCALL_BARR_WAIT] = (long (*)())do_barrier_wait;
  syscall[SYSCALL_BARR_DESTROY] = (long (*)())do_barrier_destroy;
  syscall[SYSCALL_COND_INIT] = (long (*)())do_condition_init;
  syscall[SYSCALL_COND_WAIT] = (long (*)())do_condition_wait;
  syscall[SYSCALL_COND_SIGNAL] = (long (*)())do_condition_signal;
  syscall[SYSCALL_COND_BROADCAST] = (long (*)())do_condition_broadcast;
  syscall[SYSCALL_COND_DESTROY] = (long (*)())do_condition_destroy;
  syscall[SYSCALL_MBOX_OPEN] = (long (*)())do_mbox_open;
  syscall[SYSCALL_MBOX_CLOSE] = (long (*)())do_mbox_close;
  syscall[SYSCALL_MBOX_SEND] = (long (*)())do_mbox_send;
  syscall[SYSCALL_MBOX_RECV] = (long (*)())do_mbox_recv;
  syscall[SYSCALL_TASKSET_P] = (long (*)())do_taskset_p;
  syscall[SYSCALL_THREAD_CREATE] = (long (*)())pthread_create;
  syscall[SYSCALL_THREAD_JOIN] = (long (*)())pthread_join;
  syscall[SYSCALL_SHM_GET] = (long (*)())shm_page_get;
  syscall[SYSCALL_SHM_DT] = (long (*)())shm_page_dt;
  // p5
  syscall[SYSCALL_NET_SEND] = (long (*)())do_net_send;
  syscall[SYSCALL_NET_RECV] = (long (*)())do_net_recv;
  syscall[SYSCALL_NET_SEND_PACK] = (long (*)())do_net_send_pack;
  syscall[SYSCALL_NET_RECV_PACK] = (long (*)())do_net_recv_pack;
  // p6
  syscall[SYSCALL_FS_MKFS] = (long (*)())do_mkfs;
  syscall[SYSCALL_FS_STATFS] = (long (*)())do_statfs;
  syscall[SYSCALL_FS_CD] = (long (*)())do_cd;
  syscall[SYSCALL_FS_MKDIR] = (long (*)())do_mkdir;
  syscall[SYSCALL_FS_RMDIR] = (long (*)())do_rmdir;
  syscall[SYSCALL_FS_LS] = (long (*)())do_ls;

  syscall[SYSCALL_FS_OPEN] = (long (*)())do_open;
  syscall[SYSCALL_FS_READ] = (long (*)())do_read;
  syscall[SYSCALL_FS_WRITE] = (long (*)())do_write;
  syscall[SYSCALL_FS_CLOSE] = (long (*)())do_close;
  syscall[SYSCALL_FS_LN] = (long (*)())do_ln;
  syscall[SYSCALL_FS_RM] = (long (*)())do_rm;
  syscall[SYSCALL_FS_LSEEK] = (long (*)())do_lseek;
  syscall[SYSCALL_FS_TOUCH] = (long (*)())do_touch;
  syscall[SYSCALL_FS_CAT] = (long (*)())do_cat;
}

/************************************************************/

/*
 * Once a CPU core calls this function,
 * it will stop executing!
 */
// static void kernel_brake(void) {
//   disable_interrupt();
//   while (1)
//     __asm__ volatile("wfi");
// }

int main(void) {
  if (get_current_cpu_id() == 0) {
    smp_init();
    lock_kernel();

    // Init jump table provided by kernel and bios(ΦωΦ)
    init_jmptab();

    // Init task information (〃'▽'〃)
    init_task_info();
    init_pages();
    // Init Process Control Blocks |•'-'•) ✧
    init_pcb();
    printk("> [INIT] PCB initialization succeeded.\n");

    // Read Flatten Device Tree (｡•ᴗ-)_
    time_base = bios_read_fdt(TIMEBASE);
    e1000 = (volatile uint8_t *)bios_read_fdt(ETHERNET_ADDR);
    uint64_t plic_addr = bios_read_fdt(PLIC_ADDR);
    uint32_t nr_irqs = (uint32_t)bios_read_fdt(NR_IRQS);
    printk("> [INIT] e1000: %lx, plic_addr: %lx, nr_irqs: %lx.\n", e1000,
           plic_addr, nr_irqs);

    // IOremap
    plic_addr =
        (uintptr_t)ioremap((uint64_t)plic_addr, 0x4000 * NORMAL_PAGE_SIZE);
    e1000 = (uint8_t *)ioremap((uint64_t)e1000, 8 * NORMAL_PAGE_SIZE);

    printk("> [INIT] IOremap initialization succeeded.\n");

    // Init lock mechanism o(´^｀)o
    init_locks();
    init_barriers();
    init_conditions();
    init_mbox();
    printk("> [INIT] Lock mechanism initialization succeeded.\n");

    // Init interrupt (^_^)
    init_exception();
    printk("> [INIT] Interrupt processing initialization succeeded.\n");

    // TODO: [p5-task4] Init plic
    plic_init(plic_addr, nr_irqs);
    printk(
        "> [INIT] PLIC initialized successfully. addr = 0x%lx, nr_irqs=0x%x\n",
        plic_addr, nr_irqs);

    // Init network device
    e1000_init();
    printk("> [INIT] E1000 device initialized successfully.\n");

    // Init system call table (0_0)
    init_syscall();
    printk("> [INIT] System call initialized successfully.\n");

    // Init screen (QAQ)
    init_screen();
    printk("> [INIT] SCREEN initialization succeeded.\n");

    if (is_fs_init()) {
      printk("> [INIT] File system initialized.\n");
    }

    wr_tp((uint64_t)&pid0_pcb);
    unlock_kernel();
    wakeup_other_hart();
    clear_sip();
    lock_kernel();

    do_exec("shell", 0, NULL);
  } else {
    lock_kernel();
    current_running[get_current_cpu_id()] = &pid1_pcb;
    wr_tp((uint64_t)&pid1_pcb);
    spin_lock_release(&slave_lock);
  }
  setup_exception();
  bios_set_timer(get_ticks() + TIMER_INTERVAL);
  // TODO: [p2-task4] Setup timer interrupt and enable all interrupt globally
  // NOTE: The function of sstatus.sie is differlent from sie's

  enable_interrupt();
  printk("> [INIT] CPU %d initialized successfully.\n", get_current_cpu_id());
  printk("> [INIT] CPU #%u has entered kernel with VM!\n",
         (unsigned int)get_current_cpu_id());

  unlock_kernel();
  if (get_current_cpu_id() == 0) {
    spin_lock_acquire(&slave_lock);
    disable_tmp_map();
  }

  while (1) {
    enable_preempt();
    asm volatile("wfi");
  }

  return 0;
}