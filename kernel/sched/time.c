#include <os/list.h>
#include <os/lock.h>
#include <os/sched.h>
#include <type.h>
uint64_t time_elapsed = 0;
uint64_t time_base = 0;

uint64_t get_ticks() {
  __asm__ __volatile__("rdtime %0" : "=r"(time_elapsed));
  return time_elapsed;
}

uint64_t get_timer() { return get_ticks() / time_base; }

uint64_t get_time_base() { return time_base; }

void latency(uint64_t time) {
  uint64_t begin_time = get_timer();

  while (get_timer() - begin_time < time)
    ;
  return;
}

void check_sleeping(void) {
  // TODO: [p2-task3] Pick out tasks that should wake up from the sleep queue
  // and add them to the ready queue.
  // spin_lock_acquire(&sleep_queue_lock);
  list_node_t *node = (&sleep_queue)->next;
  list_node_t *next_node = node->next;
  while (node != &sleep_queue) {
    pcb_t *pcb = LIST2PCB(node);
    if (get_timer() >= pcb->wakeup_time) {
      pcb->status = TASK_READY;
      list_remove(node);
      if (pcb->priority_base == -1)
        enqueue(&ready_queue, node);
      else
        enqueue(&prior_queue, node);
    }
    node = next_node;
    next_node = node->next;
  }
  // spin_lock_release(&sleep_queue_lock);
}