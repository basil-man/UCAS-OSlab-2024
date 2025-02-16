#include <assert.h>
#include <e1000.h>
#include <os/irq.h>
#include <os/kernel.h>
#include <os/mm.h>
#include <os/net.h>
#include <os/sched.h>
#include <os/smp.h>
#include <os/string.h>
#include <os/time.h>
#include <plic.h>
#include <printk.h>
#include <screen.h>
handler_t irq_table[IRQC_COUNT];
handler_t exc_table[EXCC_COUNT];
void handle_page_fault(regs_context_t *regs, uint64_t stval, uint64_t scause);

void interrupt_helper(regs_context_t *regs, uint64_t stval, uint64_t scause) {
  // TODO: [p2-task3] & [p2-task4] interrupt handler.
  // call corresponding handler by the value of `scause`
  // printl("regs->regs[17]: %d\n", regs->regs[17]);
  // printl("Interrupt: %d\n", scause);
  if (scause & (1UL << 63)) {
    irq_table[scause & ~(1UL << 63)](regs, stval, scause & ~(1UL << 63));
  } else {
    exc_table[scause](regs, stval, scause);
  }
}

void handle_irq_timer(regs_context_t *regs, uint64_t stval, uint64_t scause) {
  // TODO: [p2-task4] clock interrupt handler.
  // Note: use bios_set_timer to reset the timer and remember to reschedule
  bios_set_timer(get_ticks() + TIMER_INTERVAL);
  do_scheduler();
}

void handle_irq_ext(regs_context_t *regs, uint64_t stval, uint64_t scause) {
  // TODO: [p5-task4] external interrupt handler.
  // Note: plic_claim and plic_complete will be helpful ...
  uint32_t claim = plic_claim();
  if (claim == PLIC_E1000_PYNQ_IRQ || claim == PLIC_E1000_QEMU_IRQ) {
    printl("handle_irq_ext: claim: %d\n", claim);
    net_handle_irq();
  }
  plic_complete(claim);
}

void init_exception() {
  /* TODO: [p2-task3] initialize exc_table */
  /* NOTE: handle_syscall, handle_other, etc.*/
  for (int i = 0; i < EXCC_COUNT; i++) {
    exc_table[i] = handle_other;
  }

  exc_table[EXCC_SYSCALL] = handle_syscall;
  exc_table[EXCC_INST_PAGE_FAULT] = (handler_t)handle_page_fault;
  exc_table[EXCC_LOAD_PAGE_FAULT] = (handler_t)handle_page_fault;
  exc_table[EXCC_STORE_PAGE_FAULT] = (handler_t)handle_page_fault;
  /* TODO: [p2-task4] initialize irq_table */
  /* NOTE: handle_int, handle_other, etc.*/
  for (int i = 0; i < IRQC_COUNT; i++) {
    irq_table[i] = handle_other;
  }
  irq_table[IRQC_S_TIMER] = handle_irq_timer;
  irq_table[IRQC_S_EXT] = handle_irq_ext;

  /* TODO: [p2-task3] set up the entrypoint of exceptions */
  setup_exception();
}

void handle_other(regs_context_t *regs, uint64_t stval, uint64_t scause) {
  char *reg_name[] = {
      "zero ", " ra  ", " sp  ", " gp  ", " tp  ", " t0  ", " t1  ", " t2  ",
      "s0/fp", " s1  ", " a0  ", " a1  ", " a2  ", " a3  ", " a4  ", " a5  ",
      " a6  ", " a7  ", " s2  ", " s3  ", " s4  ", " s5  ", " s6  ", " s7  ",
      " s8  ", " s9  ", " s10 ", " s11 ", " t3  ", " t4  ", " t5  ", " t6  "};
  for (int i = 0; i < 32; i += 3) {
    for (int j = 0; j < 3 && i + j < 32; ++j) {
      printk("%s : %016lx ", reg_name[i + j], regs->regs[i + j]);
    }
    printk("\n\r");
  }
  printk("sstatus: 0x%lx sbadaddr: 0x%lx scause: %lu\n\r", regs->sstatus,
         regs->sbadaddr, regs->scause);
  printk("sepc: 0x%lx\n\r", regs->sepc);
  printk("tval: 0x%lx cause: 0x%lx\n", stval, scause);
  assert(0);
}
