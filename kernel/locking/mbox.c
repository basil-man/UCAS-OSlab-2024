#include <atomic.h>
#include <os/list.h>
#include <os/lock.h>
#include <os/sched.h>
#include <os/smp.h>
#include <os/string.h>
mailbox_t mboxs[MBOX_NUM];
// typedef struct mailbox {
//   // TODO [P3-TASK2 mailbox]
//   spin_lock_t lock;
//   list_head wait_list;
//   int user_num;
//   char box[MAILBOX_MAXLEN];
//   int send_ptr;
//   int recv_ptr;
//   char name[20];
// } mailbox_t;
spin_lock_t mbox_lock;
void init_mbox() {
  spin_lock_init(&mbox_lock);
  for (int i = 0; i < MBOX_NUM; i++) {
    mboxs[i].wait_list.prev = &mboxs[i].wait_list;
    mboxs[i].wait_list.next = &mboxs[i].wait_list;
    spin_lock_init(&mboxs[i].lock);
    mboxs[i].user_num = 0;
    mboxs[i].send_ptr = 0;
    mboxs[i].recv_ptr = 0;
  }
}
int do_mbox_open(char *name) {
  spin_lock_acquire(&mbox_lock);
  for (int i = 0; i < MBOX_NUM; i++) {
    // spin_lock_acquire(&mboxs[i].lock);
    if (strcmp(mboxs[i].name, name) == 0 && mboxs[i].user_num > 0) {
      mboxs[i].user_num++;
      // spin_lock_release(&mboxs[i].lock);
      spin_lock_release(&mbox_lock);
      return i;
    }
    // spin_lock_release(&mboxs[i].lock);
  }
  for (int i = 0; i < MBOX_NUM; i++) {
    // spin_lock_acquire(&mboxs[i].lock);
    if (mboxs[i].user_num == 0) {
      mboxs[i].user_num = 1;
      strcpy(mboxs[i].name, name);
      // spin_lock_release(&mboxs[i].lock);
      spin_lock_release(&mbox_lock);
      return i;
    }
    // spin_lock_release(&mboxs[i].lock);
  }
  spin_lock_release(&mbox_lock);
  return -1;
}
void do_mbox_close(int mbox_idx) {
  // spin_lock_acquire(&mboxs[mbox_idx].lock);
  spin_lock_acquire(&mbox_lock);
  mboxs[mbox_idx].user_num = 0;
  list_node_t *node;
  // spin_lock_acquire(&ready_queue_lock);
  while ((node = dequeue(&mboxs[mbox_idx].wait_list)) != NULL) {
    do_unblock(node);
  }
  // spin_lock_release(&ready_queue_lock);
  mboxs[mbox_idx].send_ptr = 0;
  mboxs[mbox_idx].recv_ptr = 0;
  // spin_lock_release(&mboxs[mbox_idx].lock);
  spin_lock_release(&mbox_lock);
}
int do_mbox_send(int mbox_idx, void *msg, int msg_length) {
  int block = 0;
  // spin_lock_acquire(&mboxs[mbox_idx].lock);
  spin_lock_acquire(&mbox_lock);
  if (mboxs[mbox_idx].user_num == 0) {
    // spin_lock_release(&mboxs[mbox_idx].lock);
    spin_lock_release(&mbox_lock);
    return -1;
  }
  if (msg_length > MAX_MBOX_LENGTH) {
    // spin_lock_release(&mboxs[mbox_idx].lock);
    spin_lock_release(&mbox_lock);
    return -2;
  }
  while (mboxs[mbox_idx].send_ptr + msg_length > MAX_MBOX_LENGTH) {
    block++;
    spin_lock_release(&mbox_lock);
    do_block(&current_running[get_current_cpu_id()]->list,
             &mboxs[mbox_idx].wait_list);
    spin_lock_acquire(&mbox_lock);
  }
  memcpy((uint8_t *)(mboxs[mbox_idx].box +
                     mboxs[mbox_idx].send_ptr * sizeof(char)),
         msg, msg_length);
  mboxs[mbox_idx].send_ptr += msg_length;
  list_node_t *node;
  // spin_lock_acquire(&ready_queue_lock);
  while ((node = dequeue(&mboxs[mbox_idx].wait_list)) != NULL) {
    do_unblock(node);
  }
  // spin_lock_release(&ready_queue_lock);
  // spin_lock_release(&mboxs[mbox_idx].lock);
  spin_lock_release(&mbox_lock);
  return block;
}
int do_mbox_recv(int mbox_idx, void *msg, int msg_length) {
  int block = 0;
  // spin_lock_acquire(&mboxs[mbox_idx].lock);
  spin_lock_acquire(&mbox_lock);
  if (mboxs[mbox_idx].user_num == 0) {
    // spin_lock_release(&mboxs[mbox_idx].lock);
    spin_lock_release(&mbox_lock);
    return -1;
  }
  while (mboxs[mbox_idx].recv_ptr + msg_length > mboxs[mbox_idx].send_ptr) {
    block++;
    spin_lock_release(&mbox_lock);
    do_block(&current_running[get_current_cpu_id()]->list,
             &mboxs[mbox_idx].wait_list);
    spin_lock_acquire(&mbox_lock);
  }
  memcpy(msg,
         (uint8_t *)(mboxs[mbox_idx].box +
                     mboxs[mbox_idx].recv_ptr * sizeof(char)),
         msg_length);
  memcpy((uint8_t *)mboxs[mbox_idx].box +
             mboxs[mbox_idx].recv_ptr * sizeof(char),
         (uint8_t *)(mboxs[mbox_idx].box +
                     (mboxs[mbox_idx].recv_ptr + msg_length) * sizeof(char)),
         mboxs[mbox_idx].send_ptr - mboxs[mbox_idx].recv_ptr - msg_length);
  mboxs[mbox_idx].send_ptr -= msg_length;
  list_node_t *node;
  // spin_lock_acquire(&ready_queue_lock);
  while ((node = dequeue(&mboxs[mbox_idx].wait_list)) != NULL) {
    do_unblock(node);
  }
  // spin_lock_release(&ready_queue_lock);
  // spin_lock_release(&mboxs[mbox_idx].lock);
  spin_lock_release(&mbox_lock);
  return block;
}