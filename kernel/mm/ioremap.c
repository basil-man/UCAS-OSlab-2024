#include <os/ioremap.h>
#include <os/mm.h>
#include <pgtable.h>
#include <type.h>

// maybe you can map it to IO_ADDR_START ?
static uintptr_t io_base = IO_ADDR_START;

void *ioremap(unsigned long phys_addr, unsigned long size) {
  // TODO: [p5-task1] map one specific physical region to virtual address
  int page_num = (size + PAGE_SIZE - 1) / PAGE_SIZE;
  uintptr_t init_io_base = io_base;
  while (page_num--) {
    map_page_for_kernel(io_base, phys_addr);
    io_base += PAGE_SIZE;
    phys_addr += PAGE_SIZE;
  }
  local_flush_tlb_all();
  return (void *)init_io_base;
}

void iounmap(void *io_addr) {
  // TODO: [p5-task1] a very naive iounmap() is OK
  // maybe no one would call this function?
}
