#include <asm/biosdef.h>
#include <csr.h>
// os size location (os_size could be sectors [p1-task3] or bytes [p1-task4])
.equ os_size_loc, 0x502001fc

// kernel address (move kernel to here, and jmp here to start kernel)
.equ kernel, 0x50202000

// BIOS function entry (jump here to use BIOS APIs)
.equ bios_func_entry, 0x50150000

.text
.global main

main:
	// fence on all memory and I/O
	fence
    // a0 is mhartid
	bnez a0, secondary
	li t0, SR_SUM
  csrs sstatus, t0
	// TODO: [p1-task1] call BIOS to print string "It's bootblock!"
	li a7,BIOS_PUTSTR
	la a0,msg
	call bios_func_entry
	// TODO: [p1-task2] call BIOS to read kernel in SD card
	li a7,BIOS_SDREAD
	la a0,kernel
	li s0, os_size_loc
	lh a1, 0(s0)
	li a2, 1
	call bios_func_entry
	// TODO: [p1-task4] load task-related arguments and pass them to kernel

	// TODO: [p1-task2] jump to kernel to start UCAS-OS
	j kernel
	/************************************************************/
secondary:
	/* TODO [P3-task3]: 
	 * 1. Mask all interrupts
	 * 2. let stvec pointer to kernel_main
	 * 3. enable software interrupt for ipi
	 */
	li a7,BIOS_PUTSTR
	la a0,msg2
	call bios_func_entry
	//1. Mask all interrupts
	li t0, SR_SIE
  csrc CSR_SSTATUS, t0
	//2. let stvec pointer to kernel_main
	la t0, kernel
	csrw stvec, t0

	//3. enable software interrupt for ipi
	li t0, SIE_SSIE
	csrs sie, t0
	li t0, SR_SIE
  csrs sstatus, t0
	li t0, SR_SUM
  csrs sstatus, t0


wait_for_wakeup:
	wfi
	j wait_for_wakeup
	/************************************************************/
// while(1) --> stop here
stop:
	j stop
	nop

.data

msg: .string "It's [Ren Haolin]'s bootloader...\n\r"
msg2: .string "It's [Ren Haolin]'s secondary hart...\n\r"
