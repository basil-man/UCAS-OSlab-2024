#include <atomic.h>
#include <os/irq.h>
#include <os/kernel.h>
#include <os/lock.h>
#include <os/sched.h>

spin_lock_t kernel_lock;
spin_lock_t page_fault_lock;
spin_lock_t driver_lock;

void smp_init() {
  /* TODO: P3-TASK3 multicore*/
  spin_lock_init(&kernel_lock);
  spin_lock_init(&page_fault_lock);
  spin_lock_init(&driver_lock);
}

void wakeup_other_hart() {
  /* TODO: P3-TASK3 multicore*/
  send_ipi(NULL);
}

void lock_kernel(uint64_t scause) { /* TODO: P3-TASK3 multicore*/
  if (scause == EXCC_INST_PAGE_FAULT || scause == EXCC_LOAD_PAGE_FAULT ||
      scause == EXCC_STORE_PAGE_FAULT)
    spin_lock_acquire(&page_fault_lock);
  else if (scause == (IRQC_S_EXT | (1UL << 63)))
    spin_lock_acquire(&driver_lock);
  else
    spin_lock_acquire(&kernel_lock);
}

void unlock_kernel() { /* TODO: P3-TASK3 multicore*/
  if (page_fault_lock.status == LOCKED)
    spin_lock_release(&page_fault_lock);
  else if (driver_lock.status == LOCKED)
    spin_lock_release(&driver_lock);
  else
    spin_lock_release(&kernel_lock);
}
