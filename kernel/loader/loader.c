#include <assert.h>
#include <os/kernel.h>
#include <os/loader.h>
#include <os/mm.h>
#include <os/string.h>
#include <os/task.h>
#include <printk.h>
#include <type.h>

#define USER_ENTRYPOINT 0x10000
// uint64_t load_task_img(int taskid) {
//   /**
//    * TODO:
//    * 1. [p1-task3] load task from image via task id, and return its
//    entrypoint
//    * 2. [p1-task4] load task via task name, thus the arg should be 'char
//    * *taskname'
//    */
//   uint64_t entry_addr = TASK_MEM_BASE + (taskid - 1) * TASK_SIZE;
//   bios_sd_read(entry_addr, 15, 1 + taskid * 15);
//   return entry_addr;
// }

uint64_t load_task_img(char *taskname, int tasknum) {
  /**
   * TODO:
   * 1. [p1-task3] load task from image via task id, and return its entrypoint
   * 2. [p1-task4] load task via task name, thus the arg should be 'char
   * *taskname'
   */
  // typedef struct {
  //   char task_name[15];
  //   int offset;
  //   int nbytes_task;
  // } task_info_t;
  int j;
  for (j = 0; j < tasknum; j++) {
    if (strcmp(taskname, tasks[j].task_name) == 0)
      break;
  }
  if (j >= tasknum) {
    return 0;

  } else {
    int begin_sector = tasks[j].offset / SECTOR_SIZE;
    int num_sectors =
        NBYTES2SEC(tasks[j].offset + tasks[j].nbytes_task) - begin_sector;
    uint64_t sd_read_addr = TASK_MEM_BASE + j * TASK_SIZE;
    bios_sd_read(sd_read_addr, num_sectors, begin_sector);
    memcpy((uint8_t *)sd_read_addr,
           (uint8_t *)(sd_read_addr + tasks[j].offset % SECTOR_SIZE),
           tasks[j].nbytes_task);
    return sd_read_addr;
  }
}

uint64_t load_task_img_va(char *taskname, int tasknum, pcb_t *pcb) {
  int j;
  for (j = 0; j < tasknum; j++) {
    if (strcmp(taskname, tasks[j].task_name) == 0)
      break;
  }
  if (j >= tasknum) {
    return 0;
  } else {
    int begin_sector = tasks[j].offset / SECTOR_SIZE;
    int num_sectors =
        NBYTES2SEC(tasks[j].offset + tasks[j].nbytes_task) - begin_sector;
    uint64_t sd_read_addr = 0x59000000;
    while (num_sectors > 0) {
      bios_sd_read(sd_read_addr, num_sectors > 64 ? 64 : num_sectors,
                   begin_sector);
      sd_read_addr += SECTOR_SIZE * 64;
      begin_sector += 64;
      num_sectors -= 64;
    }
    uintptr_t src = pa2kva(0x59000000 + tasks[j].offset % SECTOR_SIZE);
    uint64_t user_va_end = USER_ENTRYPOINT + tasks[j].p_memsz;
    for (uint64_t va = USER_ENTRYPOINT; va < user_va_end; va += PAGE_SIZE) {
      uintptr_t page_va = _alloc_page_helper(va, pcb);
      int cp_size =
          (user_va_end - va) > PAGE_SIZE ? PAGE_SIZE : (user_va_end - va);
      memcpy((void *)page_va, (void *)(src + va - USER_ENTRYPOINT), cp_size);
    }
    return USER_ENTRYPOINT;
  }
  assert(0);
  return 0;
}