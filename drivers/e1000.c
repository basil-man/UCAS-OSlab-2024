#include <assert.h>
#include <e1000.h>
#include <os/net.h>
#include <os/string.h>
#include <os/time.h>
#include <pgtable.h>
#include <type.h>

// E1000 Registers Base Pointer
volatile uint8_t *e1000; // use virtual memory address

// E1000 Tx & Rx Descriptors
static struct e1000_tx_desc tx_desc_array[TXDESCS] __attribute__((aligned(16)));
static struct e1000_rx_desc rx_desc_array[RXDESCS] __attribute__((aligned(16)));

// E1000 Tx & Rx packet buffer
static char tx_pkt_buffer[TXDESCS][TX_PKT_SIZE];
static char rx_pkt_buffer[RXDESCS][RX_PKT_SIZE];

// Fixed Ethernet MAC Address of E1000
static const uint8_t enetaddr[6] = {0x00, 0x0a, 0x35, 0x00, 0x1e, 0x53};
struct ethhdr ethhdr;
/**
 * e1000_reset - Reset Tx and Rx Units; mask and clear all interrupts.
 **/
static void e1000_reset(void) {
  /* Turn off the ethernet interface */
  e1000_write_reg(e1000, E1000_RCTL, 0);
  e1000_write_reg(e1000, E1000_TCTL, 0);

  /* Clear the transmit ring */
  e1000_write_reg(e1000, E1000_TDH, 0);
  e1000_write_reg(e1000, E1000_TDT, 0);

  /* Clear the receive ring */
  e1000_write_reg(e1000, E1000_RDH, 0);
  e1000_write_reg(e1000, E1000_RDT, 0);

  /**
   * Delay to allow any outstanding PCI transactions to complete before
   * resetting the device
   */
  latency(1);

  /* Clear interrupt mask to stop board from generating interrupts */
  e1000_write_reg(e1000, E1000_IMC, 0xffffffff);

  /* Clear any pending interrupt events. */
  while (0 != e1000_read_reg(e1000, E1000_ICR))
    ;
}

/**
 * e1000_configure_tx - Configure 8254x Transmit Unit after Reset
 **/
static void e1000_configure_tx(void) {
  /* TODO: [p5-task1] Initialize tx descriptors */
  for (int i = 0; i < TXDESCS; i++) {
    tx_desc_array[i].addr = kva2pa((uintptr_t)tx_pkt_buffer[i]);
    printl("tx_desc_array[%d].addr: %x\n", i, tx_desc_array[i].addr);
    tx_desc_array[i].length = TX_PKT_SIZE;
    tx_desc_array[i].status = E1000_TXD_STAT_DD;
    tx_desc_array[i].cmd = E1000_TXD_CMD_RS | E1000_TXD_CMD_EOP;
  }
  /* TODO: [p5-task1] Set up the Tx descriptor base address and length */
  e1000_write_reg(e1000, E1000_TDBAL,
                  kva2pa((uintptr_t)tx_desc_array) << 32 >> 32);
  e1000_write_reg(e1000, E1000_TDBAH, kva2pa((uintptr_t)tx_desc_array) >> 32);
  e1000_write_reg(e1000, E1000_TDLEN, sizeof(tx_desc_array));
  /* TODO: [p5-task1] Set up the HW Tx Head and Tail descriptor pointers */
  e1000_write_reg(e1000, E1000_TDH, 0);
  e1000_write_reg(e1000, E1000_TDT, 0);
  /* TODO: [p5-task1] Program the Transmit Control Register */
  e1000_write_reg(e1000, E1000_TCTL,
                  E1000_TCTL_EN | E1000_TCTL_PSP | (0x10 << 4) | (0x40 << 12));

  local_flush_dcache();
}

/**
 * e1000_configure_rx - Configure 8254x Receive Unit after Reset
 **/
static void e1000_configure_rx(void) {
  /* TODO: [p5-task2] Set e1000 MAC Address to RAR[0] */
  e1000_write_reg_array(e1000, E1000_RA, 0,
                        enetaddr[0] | enetaddr[1] << 8 | enetaddr[2] << 16 |
                            enetaddr[3] << 24);
  e1000_write_reg_array(e1000, E1000_RA, 1,
                        enetaddr[4] | enetaddr[5] << 8 | E1000_RAH_AV);
  /* TODO: [p5-task2] Initialize rx descriptors */
  for (int i = 0; i < RXDESCS; i++) {
    rx_desc_array[i].addr = kva2pa((uintptr_t)rx_pkt_buffer[i]);
    rx_desc_array[i].length = 0;
    rx_desc_array[i].status = 0;
  }

  /* TODO: [p5-task2] Set up the Rx descriptor base address and length */
  e1000_write_reg(e1000, E1000_RDBAL,
                  kva2pa((uintptr_t)rx_desc_array) << 32 >> 32);
  e1000_write_reg(e1000, E1000_RDBAH, kva2pa((uintptr_t)rx_desc_array) >> 32);
  e1000_write_reg(e1000, E1000_RDLEN, sizeof(rx_desc_array));
  /* TODO: [p5-task2] Set up the HW Rx Head and Tail descriptor pointers */
  e1000_write_reg(e1000, E1000_RDH, 0);
  e1000_write_reg(e1000, E1000_RDT, RXDESCS - 1);
  /* TODO: [p5-task2] Program the Receive Control Register */
  e1000_write_reg(e1000, E1000_RCTL, E1000_RCTL_EN | E1000_RCTL_BAM);
  /* TODO: [p5-task4] Enable RXDMT0 Interrupt */
  e1000_write_reg(e1000, E1000_IMS, E1000_IMS_RXDMT0);

  local_flush_dcache();
}

void init_ethhdr() {
  ethhdr.ether_dmac[0] = 0xff;
  ethhdr.ether_dmac[1] = 0xff;
  ethhdr.ether_dmac[2] = 0xff;
  ethhdr.ether_dmac[3] = 0xff;
  ethhdr.ether_dmac[4] = 0xff;
  ethhdr.ether_dmac[5] = 0xff;
  ethhdr.ether_smac[0] = enetaddr[0];
  ethhdr.ether_smac[1] = enetaddr[1];
  ethhdr.ether_smac[2] = enetaddr[2];
  ethhdr.ether_smac[3] = enetaddr[3];
  ethhdr.ether_smac[4] = enetaddr[4];
  ethhdr.ether_smac[5] = enetaddr[5];
  ethhdr.ether_type = ETH_P_IP;
}

/**
 * e1000_init - Initialize e1000 device and descriptors
 **/
void e1000_init(void) {
  /* Reset E1000 Tx & Rx Units; mask & clear all interrupts */
  e1000_reset();

  /* Configure E1000 Tx Unit */
  e1000_configure_tx();

  /* Configure E1000 Rx Unit */
  e1000_configure_rx();

  /* Initialize Ethernet Header */
  init_ethhdr();
}

/**
 * e1000_transmit - Transmit packet through e1000 net device
 * @param txpacket - The buffer address of packet to be transmitted
 * @param length - Length of this packet
 * @return - Number of bytes that are transmitted successfully
 **/
int e1000_transmit(void *txpacket, int length) {
  /* TODO: [p5-task1] Transmit one packet from txpacket */
  int index = e1000_read_reg(e1000, E1000_TDT);
  if (!(tx_desc_array[index].status & E1000_TXD_STAT_DD)) {
    // printk("[e1000 driver]: waiting for transmition done.\n");
    return 0;
  }
  uint8_t *buffer = (uint8_t *)tx_pkt_buffer[index];
  // memcpy(buffer, (uint8_t *)&ethhdr, sizeof(struct ethhdr));
  // memcpy(buffer + sizeof(struct ethhdr), txpacket, length);
  memcpy(buffer, txpacket, length);
  tx_desc_array[index].length = length;
  tx_desc_array[index].status &= ~E1000_TXD_STAT_DD;
  e1000_write_reg(e1000, E1000_TDT, (index + 1) % TXDESCS);
  local_flush_dcache();
  return length;
}

/**
 * e1000_poll - Receive packet through e1000 net device
 * @param rxbuffer - The address of buffer to store received packet
 * @return - Length of received packet
 **/
int e1000_poll(void *rxbuffer) {
  /* TODO: [p5-task2] Receive one packet and put it into rxbuffer */
  int index = e1000_read_reg(e1000, E1000_RDT);
  int tail = (index + 1) % RXDESCS;

  if (!(rx_desc_array[tail].status & E1000_RXD_STAT_DD)) {
    // printk("[e1000 driver]: waiting for receiving done.\n");
    return 0;
  }
  int length = rx_desc_array[tail].length;
  // memcpy(rxbuffer, (uint8_t *)rx_pkt_buffer[tail] + sizeof(struct ethhdr),
  // length);
  memcpy(rxbuffer, (uint8_t *)rx_pkt_buffer[tail], length);

  rx_desc_array[tail].status &= ~E1000_RXD_STAT_DD;
  e1000_write_reg(e1000, E1000_RDT, tail);
  local_flush_dcache();
  return length;
}
void print_packet_content(uint8_t *packet) {
  for (int i = 0; i < 1024; i += 2) {
    printl("%02x%02x ", packet[i], packet[i + 1]);
    if ((i + 2) % 32 == 0)
      printl("\n");
  }
  printl("\n\n");
}
int e1000_transmit_pack(void *txpacket, int length) {
  /* TODO: [p5-task1] Transmit one packet from txpacket */
  int index = e1000_read_reg(e1000, E1000_TDT);
  if (!(tx_desc_array[index].status & E1000_TXD_STAT_DD)) {
    // printk("[e1000 driver]: waiting for transmition done.\n");
    return 0;
  }
  uint8_t *buffer = (uint8_t *)tx_pkt_buffer[index];
  // print_packet_content(txpacket);
  memcpy(buffer, (uint8_t *)&ethhdr, sizeof(struct ethhdr));
  memcpy(buffer + sizeof(struct ethhdr), txpacket, length);
  tx_desc_array[index].length = length + sizeof(struct ethhdr);
  tx_desc_array[index].status &= ~E1000_TXD_STAT_DD;
  e1000_write_reg(e1000, E1000_TDT, (index + 1) % TXDESCS);
  local_flush_dcache();
  return length;
}

/**
 * e1000_poll - Receive packet through e1000 net device
 * @param rxbuffer - The address of buffer to store received packet
 * @return - Length of received packet
 **/
int e1000_poll_pack(void *rxbuffer) {
  /* TODO: [p5-task2] Receive one packet and put it into rxbuffer */
  int index = e1000_read_reg(e1000, E1000_RDT);
  int tail = (index + 1) % RXDESCS;
  if (!(rx_desc_array[tail].status & E1000_RXD_STAT_DD)) {
    // printk("[e1000 driver]: waiting for receiving done.\n");
    return 0;
  }
  int length = rx_desc_array[tail].length;
  // printl("[e1000_poll_pack]: dest: %lx, src: %lx, len: %d\n", rxbuffer,
  //        (uint8_t *)(rx_pkt_buffer[tail] + sizeof(struct ethhdr)),
  //        length - sizeof(struct ethhdr));
  // print_packet_content((uint8_t *)rx_pkt_buffer[tail] + sizeof(struct
  // ethhdr));
  memcpy(rxbuffer, (uint8_t *)(rx_pkt_buffer[tail] + sizeof(struct ethhdr)),
         length - sizeof(struct ethhdr));

  rx_desc_array[tail].status &= ~E1000_RXD_STAT_DD;
  e1000_write_reg(e1000, E1000_RDT, tail);
  local_flush_dcache();
  return length - sizeof(struct ethhdr);
}