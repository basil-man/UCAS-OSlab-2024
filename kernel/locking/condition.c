#include <atomic.h>
#include <os/list.h>
#include <os/lock.h>
#include <os/sched.h>
#include <os/smp.h>
// typedef struct condition {
//   // TODO [P3-TASK2 condition]
//   spin_lock_t lock;
//   int key;
//   list_head wait_list;
//   int user_num;

// } condition_t;
condition_t conditions[CONDITION_NUM];
spin_lock_t condition_lock;
void init_conditions(void) {
  spin_lock_init(&condition_lock);
  for (int i = 0; i < CONDITION_NUM; i++) {
    conditions[i].wait_list.prev = &conditions[i].wait_list;
    conditions[i].wait_list.next = &conditions[i].wait_list;
    spin_lock_init(&conditions[i].lock);
    conditions[i].key = -1;
    conditions[i].user_num = 0;
  }
}
int do_condition_init(int key) {
  spin_lock_acquire(&condition_lock);
  for (int i = 0; i < CONDITION_NUM; i++) {
    // spin_lock_acquire(&conditions[i].lock);
    if (conditions[i].key == key && conditions[i].user_num > 0) {
      conditions[i].user_num++;
      // spin_lock_release(&conditions[i].lock);
      spin_lock_release(&condition_lock);
      return i;
    }
    // spin_lock_release(&conditions[i].lock);
  }
  for (int i = 0; i < CONDITION_NUM; i++) {
    // spin_lock_acquire(&conditions[i].lock);
    if (conditions[i].user_num == 0) {
      conditions[i].key = key;
      conditions[i].user_num = 1;
      // spin_lock_release(&conditions[i].lock);
      spin_lock_release(&condition_lock);
      return i;
    }
    // spin_lock_release(&conditions[i].lock);
  }
  spin_lock_release(&condition_lock);
  return -1;
}
void do_condition_wait(int cond_idx, int mutex_idx) {
  // spin_lock_acquire(&conditions[cond_idx].lock);
  spin_lock_acquire(&condition_lock);
  do_mutex_lock_release(mutex_idx);

  do_block(&current_running[get_current_cpu_id()]->list,
           &conditions[cond_idx].wait_list);

  // spin_lock_release(&conditions[cond_idx].lock);
  spin_lock_release(&condition_lock);
  do_mutex_lock_acquire(mutex_idx);
}
void do_condition_signal(int cond_idx) {
  // spin_lock_acquire(&conditions[cond_idx].lock);
  spin_lock_acquire(&condition_lock);
  list_node_t *node;
  spin_lock_acquire(&condition_lock);
  if ((node = dequeue(&conditions[cond_idx].wait_list)) != NULL) {
    do_unblock(node);
  }
  spin_lock_release(&condition_lock);
  // spin_lock_release(&conditions[cond_idx].lock);
  spin_lock_release(&condition_lock);
}
void do_condition_broadcast(int cond_idx) {
  // spin_lock_acquire(&conditions[cond_idx].lock);
  spin_lock_acquire(&condition_lock);
  list_node_t *node;
  // spin_lock_acquire(&ready_queue_lock);
  while ((node = dequeue(&conditions[cond_idx].wait_list)) != NULL) {
    do_unblock(node);
  }
  // spin_lock_release(&ready_queue_lock);
  // spin_lock_release(&conditions[cond_idx].lock);
  spin_lock_release(&condition_lock);
}
void do_condition_destroy(int cond_idx) {
  // spin_lock_acquire(&conditions[cond_idx].lock);
  spin_lock_acquire(&condition_lock);
  conditions[cond_idx].user_num = 0;
  list_node_t *node;
  // spin_lock_acquire(&ready_queue_lock);
  while ((node = dequeue(&conditions[cond_idx].wait_list)) != NULL) {
    do_unblock(node);
  }
  // spin_lock_release(&ready_queue_lock);
  // spin_lock_release(&conditions[cond_idx].lock);
  spin_lock_release(&condition_lock);
}