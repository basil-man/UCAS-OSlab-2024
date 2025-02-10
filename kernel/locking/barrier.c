#include <atomic.h>
#include <os/list.h>
#include <os/lock.h>
#include <os/sched.h>
#include <os/smp.h>
barrier_t barriers[BARRIER_NUM];
spin_lock_t barrier_lock;
// typedef struct barrier {
//   // TODO [P3-TASK2 barrier]
//   spin_lock_t lock;
//   int goal;
//   int num;
//   int key;
//   list_head wait_list;
//   int user_num;
// } barrier_t;

void init_barriers(void) {
  spin_lock_init(&barrier_lock);
  for (int i = 0; i < BARRIER_NUM; i++) {
    barriers[i].wait_list.prev = &barriers[i].wait_list;
    barriers[i].wait_list.next = &barriers[i].wait_list;
    spin_lock_init(&barriers[i].lock);
    barriers[i].num = 0;
    barriers[i].key = -1;
    barriers[i].user_num = 0;
  }
}
int do_barrier_init(int key, int goal) {
  spin_lock_acquire(&barrier_lock);
  for (int i = 0; i < BARRIER_NUM; i++) {
    // spin_lock_acquire(&barriers[i].lock);
    if (barriers[i].key == key && barriers[i].num > 0) {
      barriers[i].user_num++;
      barriers[i].goal = goal;
      // spin_lock_release(&barriers[i].lock);
      spin_lock_release(&barrier_lock);
      return i;
    }
    // spin_lock_release(&barriers[i].lock);
  }
  for (int i = 0; i < BARRIER_NUM; i++) {
    // spin_lock_acquire(&barriers[i].lock);
    if (barriers[i].user_num == 0) {
      barriers[i].key = key;
      barriers[i].goal = goal;
      barriers[i].num = 0;
      barriers[i].user_num = 1;
      // spin_lock_release(&barriers[i].lock);
      spin_lock_release(&barrier_lock);
      return i;
    }
    // spin_lock_release(&barriers[i].lock);
  }
  spin_lock_release(&barrier_lock);
  return -1;
}
void do_barrier_wait(int bar_idx) {
  // spin_lock_acquire(&barriers[bar_idx].lock);
  spin_lock_acquire(&barrier_lock);
  barriers[bar_idx].num++;
  if (barriers[bar_idx].num >= barriers[bar_idx].goal) {
    list_head *node;
    // spin_lock_acquire(&ready_queue_lock);
    while ((node = dequeue(&barriers[bar_idx].wait_list)) != NULL) {
      do_unblock(node);
    }
    // spin_lock_release(&ready_queue_lock);
    barriers[bar_idx].num = 0;
  } else {
    spin_lock_release(&barrier_lock);
    do_block(&current_running[get_current_cpu_id()]->list,
             &barriers[bar_idx].wait_list);
    spin_lock_acquire(&barrier_lock);
  }
  // spin_lock_release(&barriers[bar_idx].lock);
  spin_lock_release(&barrier_lock);
}
void do_barrier_destroy(int bar_idx) {
  // spin_lock_acquire(&barriers[bar_idx].lock);
  spin_lock_acquire(&barrier_lock);
  barriers[bar_idx].user_num = 0;
  list_head *node;
  // spin_lock_acquire(&ready_queue_lock);
  while ((node = dequeue(&barriers[bar_idx].wait_list)) != NULL) {
    do_unblock(node);
  }
  // spin_lock_release(&ready_queue_lock);
  // spin_lock_release(&barriers[bar_idx].lock);
  spin_lock_release(&barrier_lock);
}