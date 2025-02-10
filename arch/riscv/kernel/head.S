/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 */

#include <asm.h>
#include <csr.h>

#define KERNEL_STACK		0xffffffc050501000
#define PAGE_SIZE       4096
.section ".entry_function","ax"
ENTRY(_start)
  /* Mask all interrupts */
  csrw CSR_SIE, zero
  csrw CSR_SIP, zero

  csrr t0, CSR_MHARTID
  bnez t0, secondary_start

  /* TODO: [p1-task2] clear BSS for flat non-ELF images */
  la s0,__bss_start
  la s1,__BSS_END__
loop_bss:
  bge s0,s1,end_loop_bss
  sw zero,0(s0)
  addi s0,s0,4
  j loop_bss
end_loop_bss:
  /* TODO: [p1-task2] setup C environment */
  li sp,KERNEL_STACK
  j main

secondary_start:
  li sp,KERNEL_STACK
  li t1, PAGE_SIZE
  add sp, sp, t1
  add sp, sp, t1
  add sp, sp, t1
  add sp, sp, t1
  j main

loop:
  wfi
  j loop

END(_start)
