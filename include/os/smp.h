#ifndef SMP_H
#define SMP_H

#define NR_CPUS 2
#include <type.h>
extern void smp_init();
extern void wakeup_other_hart();
extern uint64_t get_current_cpu_id();
extern void lock_kernel();
extern void unlock_kernel();
extern void lock_irq_page_fault();
extern void unlock_irq_page_fault();
#endif /* SMP_H */
