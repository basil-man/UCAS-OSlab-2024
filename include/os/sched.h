/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * *
 * * * * * * Copyright (C) 2018 Institute of Computing Technology, CAS Author :
 * Han Shukai (email : hanshukai@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * *
 * * * * * * Process scheduling related content, such as: scheduler, process
 * blocking, process wakeup, process creation, process kill, etc.
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

#ifndef INCLUDE_SCHEDULER_H_
#define INCLUDE_SCHEDULER_H_

#include <os/list.h>
#include <os/lock.h>
#include <type.h>

#define NUM_MAX_TASK 16

#define MAX_PRIORITY 100

#define LIST2PCB(listptr) ((pcb_t *)((void *)(listptr) - 32))

/* used to save register infomation */
typedef struct regs_context {
  /* Saved main processor registers.*/
  reg_t regs[32];

  /* Saved special registers. */
  reg_t sstatus;
  reg_t sepc;
  reg_t sbadaddr;
  reg_t scause;
} regs_context_t;

/* used to save register infomation in switch_to */
typedef struct switchto_context {
  /* Callee saved registers.*/
  reg_t regs[14];
  // typedef struct context {
  //   uint64_t ra;
  //   uint64_t sp;

  //   // callee-saved
  //   uint64_t s0;
  //   uint64_t s1;
  //   uint64_t s2;
  //   uint64_t s3;
  //   uint64_t s4;
  //   uint64_t s5;
  //   uint64_t s6;
  //   uint64_t s7;
  //   uint64_t s8;
  //   uint64_t s9;
  //   uint64_t s10;
  //   uint64_t s11;
  // } context;
} switchto_context_t;

typedef enum {
  TASK_BLOCKED,
  TASK_RUNNING,
  TASK_READY,
  TASK_EXITED,
  TASK_EMPTY,
} task_status_t;

/* Process Control Block */
typedef struct pcb {
  /* register context */
  // NOTE: this order must be preserved, which is defined in regs.h!!
  reg_t kernel_sp;
  reg_t user_sp;
  ptr_t kernel_stack_base;
  ptr_t user_stack_base;

  /* previous, next pointer */
  list_node_t list;
  list_head wait_list;

  /* process id */
  pid_t pid;

  /* BLOCK | READY | RUNNING */
  task_status_t status;

  /* cursor position */
  int cursor_x;
  int cursor_y;

  /* time(seconds) to wake up sleeping PCB */
  uint64_t wakeup_time;

  char name[15];

  uint32_t priority;     // p2-task5, 0~100, 0 is the highest
  int32_t priority_base; // p2-task5
  spin_lock_t lock;
  uint16_t cpu;
  uint16_t cpu_mask;
  uintptr_t pgdir;
  uintptr_t pgdir_origin;

  list_head swap_list;
  list_head page_list;
  uint16_t recycle;
  pid_t tid;
  uint32_t thread_num;
} pcb_t;

/* ready queue to run */
extern list_head ready_queue;

extern list_head prior_queue;

/* sleep queue to be blocked in */
extern list_head sleep_queue;

// extern spin_lock_t ready_queue_lock;
// extern spin_lock_t sleep_queue_lock;
// extern spin_lock_t prior_queue_lock;

/* current running task PCB */
extern pcb_t *volatile current_running[2];
extern pid_t process_id;

extern pcb_t pcb[NUM_MAX_TASK];
extern pcb_t pid0_pcb;
extern const ptr_t pid0_stack;
extern pcb_t pid1_pcb;
extern const ptr_t pid1_stack;

extern void switch_to(pcb_t *prev, pcb_t *next);
void do_scheduler(void);
void do_sleep(uint32_t);

void do_block(list_node_t *, list_head *queue);
void do_unblock(list_node_t *);
void set_sche_workload(int num); // p2-task5

/************************************************************/
/* TODO [P3-TASK1] exec exit kill waitpid ps*/
#ifdef S_CORE
extern pid_t do_exec(int id, int argc, uint64_t arg0, uint64_t arg1,
                     uint64_t arg2);
#else
extern pid_t do_exec(char *name, int argc, char *argv[]);
#endif
extern void do_exit(void);
extern int do_kill(pid_t pid);
extern int do_waitpid(pid_t pid);
extern void do_process_show();
extern pid_t do_getpid();
extern int do_taskset_p(uint16_t mask, pid_t pid);
extern pid_t do_taskset(char *name, int argc, char *argv[], uint16_t mask);
void pthread_create(int32_t *tidptr, uint64_t start_routine, void *arg);
int pthread_join(pid_t tid);
int do_kill_thread(pid_t pid, pid_t tid);
/************************************************************/

#endif
