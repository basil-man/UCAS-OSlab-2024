#include <e1000.h>
#include <os/list.h>
#include <os/sched.h>
#include <os/smp.h>
#include <os/string.h>
#include <type.h>

static LIST_HEAD(send_block_queue);
static LIST_HEAD(recv_block_queue);

int do_net_send(void *txpacket, int length) {
  // TODO: [p5-task1] Transmit one network packet via e1000 device
  // TODO: [p5-task3] Call do_block when e1000 transmit queue is full
  // TODO: [p5-task4] Enable TXQE interrupt if transmit queue is full
  int len;
  for (;;) {
    len = e1000_transmit(txpacket, length);
    if (len == 0) {
      e1000_write_reg(e1000, E1000_IMS, E1000_IMS_TXQE);
      local_flush_dcache();
      do_block(&current_running[get_current_cpu_id()]->list, &send_block_queue);
    } else {
      break;
    }
  }
  return len; // Bytes it has transmitted
}

int do_net_recv(void *rxbuffer, int pkt_num, int *pkt_lens) {
  // TODO: [p5-task2] Receive one network packet via e1000 device
  // TODO: [p5-task3] Call do_block when there is no packet on the way
  int i;
  uint64_t received_bytes = 0;
  uint8_t *buffer = (uint8_t *)rxbuffer;
  for (i = 0; i < pkt_num; i++) {
    while ((pkt_lens[i] = e1000_poll(buffer + received_bytes)) == 0) {
      do_block(&current_running[get_current_cpu_id()]->list, &recv_block_queue);
    }
    received_bytes += pkt_lens[i];
  }
  return received_bytes; // Bytes it has received
}

int do_net_send_pack(void *txpacket, int *length, int num) {
  // TODO: [p5-task1] Transmit one network packet via e1000 device
  // TODO: [p5-task3] Call do_block when e1000 transmit queue is full
  // TODO: [p5-task4] Enable TXQE interrupt if transmit queue is full
  int len = 0;
  for (int i = 0; i < num; i++) {
    for (;;) {
      int single_len = e1000_transmit_pack(txpacket + len, length[i]);
      if (single_len == 0) {
        e1000_write_reg(e1000, E1000_IMS, E1000_IMS_TXQE);
        local_flush_dcache();
        do_block(&current_running[get_current_cpu_id()]->list,
                 &send_block_queue);
      } else {
        len += single_len;
        break;
      }
    }
  }
  return len; // Bytes it has transmitted
}

int do_net_recv_pack(void *rxbuffer, int pkt_num, int *pkt_lens) {
  // TODO: [p5-task2] Receive one network packet via e1000 device
  // TODO: [p5-task3] Call do_block when there is no packet on the way
  int i;
  uint8_t magic_code = 0x42;
  uint64_t received_bytes = 0;
  uint8_t *buffer = (uint8_t *)rxbuffer;
  for (i = 0; i < pkt_num; i++) {
  start:
    while ((pkt_lens[i] = e1000_poll_pack(buffer + received_bytes)) == 0) {
      do_block(&current_running[get_current_cpu_id()]->list, &recv_block_queue);
    }
    if (buffer[received_bytes] != magic_code) {
      goto start;
    }
    received_bytes += pkt_lens[i];
  }
  return received_bytes; // Bytes it has received
}

void net_handle_irq(void) {
  // TODO: [p5-task4] Handle interrupts from network device
  uint32_t icr = e1000_read_reg(e1000, E1000_ICR);
  // e1000_handle_txqe
  if (icr & E1000_ICR_TXQE) {
    if (!list_is_empty(&send_block_queue)) {
      do_unblock(dequeue(&send_block_queue));
    }
  }
  // e1000_handle_rxdm0
  if (icr & E1000_ICR_RXDMT0) {
    if (!list_is_empty(&recv_block_queue)) {
      do_unblock(dequeue(&recv_block_queue));
    }
  }
}