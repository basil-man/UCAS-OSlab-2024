#ifndef __INCLUDE_LOADER_H__
#define __INCLUDE_LOADER_H__

#include <os/sched.h>
#include <type.h>
uint64_t load_task_img(char *taskname, int tasknum);
uint64_t load_task_img_va(char *taskname, int tasknum, pcb_t *pcb);
#endif