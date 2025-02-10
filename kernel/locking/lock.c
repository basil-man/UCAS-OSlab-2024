#include <atomic.h>
#include <os/list.h>
#include <os/lock.h>
#include <os/sched.h>
#include <os/smp.h>
mutex_lock_t mlocks[LOCK_NUM];
lock_status_t my_atomic_swap_d(lock_status_t new_value, lock_status_t *lock) {
  lock_status_t old_value = *lock;
  *lock = new_value;
  return old_value;
}
void init_locks(void) {
  /* TODO: [p2-task2] initialize mlocks */
  for (int i = 0; i < LOCK_NUM; i++) {
    mlocks[i].block_queue.prev = &mlocks[i].block_queue;
    mlocks[i].block_queue.next = &mlocks[i].block_queue;
    mlocks[i].lock.status = UNLOCKED;
    mlocks[i].key = -1;
    mlocks[i].owner_pid = -1;
    mlocks[i].owner_tid = -1;
    spin_lock_init(&mlocks[i].mutex_lock_lock);
  }
  // p3 init spin locks
  // spin_lock_init(&ready_queue_lock);
  // spin_lock_init(&sleep_queue_lock);
  // spin_lock_init(&prior_queue_lock);
}

void spin_lock_init(spin_lock_t *lock) {
  /* TODO: [p2-task2] initialize spin lock */
  lock->status = UNLOCKED;
}

int spin_lock_try_acquire(spin_lock_t *lock) {
  /* TODO: [p2-task2] try to acquire spin lock */
  return atomic_swap_d(LOCKED, (ptr_t)lock);
}

void spin_lock_acquire(spin_lock_t *lock) {
  /* TODO: [p2-task2] acquire spin lock */
  while (spin_lock_try_acquire(lock) == LOCKED)
    ;
}

void spin_lock_release(spin_lock_t *lock) {
  /* TODO: [p2-task2] release spin lock */
  atomic_swap_d(UNLOCKED, (ptr_t)&lock->status);
}

int do_mutex_lock_init(int key) {
  /* TODO: [p2-task2] initialize mutex lock */
  for (int i = 0; i < LOCK_NUM; i++) {
    if (mlocks[i].key == key) {
      return i;
    }
  }
  for (int i = 0; i < LOCK_NUM; i++) {
    if (mlocks[i].key == -1) {
      mlocks[i].key = key;
      mlocks[i].owner_pid = current_running[get_current_cpu_id()]->pid;
      mlocks[i].owner_tid = current_running[get_current_cpu_id()]->tid;
      return i;
    }
  }
  return 0;
}

void do_mutex_lock_acquire(int mlock_idx) {
  spin_lock_acquire(&mlocks[mlock_idx].mutex_lock_lock);
  /* TODO: [p2-task2] acquire mutex lock */
  if (atomic_swap_d(LOCKED, (ptr_t)&mlocks[mlock_idx].lock.status) == LOCKED) {
    spin_lock_release(&mlocks[mlock_idx].mutex_lock_lock);
    do_block(&current_running[get_current_cpu_id()]->list,
             &mlocks[mlock_idx].block_queue);
    spin_lock_acquire(&mlocks[mlock_idx].mutex_lock_lock);
  }
  mlocks[mlock_idx].owner_pid = current_running[get_current_cpu_id()]->pid;
  mlocks[mlock_idx].owner_tid = current_running[get_current_cpu_id()]->tid;
  spin_lock_release(&mlocks[mlock_idx].mutex_lock_lock);
}

void do_mutex_lock_release(int mlock_idx) {
  /* TODO: [p2-task2] release mutex lock */
  spin_lock_acquire(&mlocks[mlock_idx].mutex_lock_lock);
  list_node_t *unblocked_node;
  if ((unblocked_node = dequeue(&mlocks[mlock_idx].block_queue)) == NULL) {
    mlocks[mlock_idx].lock.status = UNLOCKED;
    mlocks[mlock_idx].owner_pid = -1;
    mlocks[mlock_idx].owner_tid = -1;
  } else {
    mlocks[mlock_idx].owner_pid = LIST2PCB(unblocked_node)->pid;
    mlocks[mlock_idx].owner_tid = LIST2PCB(unblocked_node)->tid;
    // spin_lock_acquire(&ready_queue_lock);
    do_unblock(unblocked_node);
    // spin_lock_release(&ready_queue_lock);
  }
  spin_lock_release(&mlocks[mlock_idx].mutex_lock_lock);
}
