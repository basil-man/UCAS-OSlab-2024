#include <asm.h>
//#define KERNEL_STACK		0x50500000
.section ".entry_function","ax"
ENTRY(_start)

 /* TODO: [p1-task3] setup C runtime environment for the user program */
    addi sp, sp, -16 
    sd s0, 8(sp)           
    sd ra, (sp)            
    addi s0, sp, 16        
    
    la x5, __bss_start        
    la x6, __BSS_END__        
do_clear:  
    sw  x0, (x5)          // store 0 to x5
    add x5, x5, 0x04      // point to next word
    ble x5, x6, do_clear

    /* TODO: [p1-task3] enter main function */
    call main



    /* TODO: [p1-task3] finish task and return to the kernel, replace this in p3-task2! */
    # ld ra, 0(sp)
    # addi sp, sp, 8
    # ret
    /************************************************************/
    /* TODO: [P3] call sys_exit */
    call sys_exit
    /************************************************************/
// while(1) loop, unreachable here
loop:
    wfi
    j loop

END(_start)
