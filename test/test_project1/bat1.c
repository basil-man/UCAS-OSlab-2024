#include <kernel.h>

#define MAX_STR_LEN 20
#define DATA_ADDR 0x52190000
void memcpy_riscv(void *dest_ptr, const void *src_ptr, int size);
char a[][MAX_STR_LEN] = {"aaa",
                         "a",
                         "banana",
                         "student",
                         "operating",
                         "system",
                         "lab",
                         "apple",
                         "lab",
                         "system"
                         ""};
int main(void) {
  bios_putstr("[bat1] Info: print words:\n");
  int num_words = sizeof(a) / sizeof(a[0]);
  for (int i = 0; i < num_words; i++) {
    bios_putstr(a[i]);
    bios_putchar('\n');
  }

  char *dest_ptr = (char *)DATA_ADDR;
  // bios_putstr("[bat1] Info: writing shared mem...\n");
  memcpy_riscv((void *)dest_ptr, (void *)a, sizeof(a));
  // bios_putstr("[bat1] Info: writing shared mem done.\n");
  bios_putchar('\n');
  return 0;
}

void memcpy_riscv(void *dest_ptr, const void *src_ptr, int size) {
  asm volatile(
      "1:                              \n" // 标签1
      "    lbu t0, 0(%[src])          \n"  // 从src读取一个字节到t0
      "    sb t0, 0(%[dest])          \n"  // 将t0中的字节写入dest
      "    addi %[src], %[src], 1      \n" // 将src指针加1
      "    addi %[dest], %[dest], 1    \n" // 将dest指针加1
      "    addi %[size], %[size], -1   \n" // size减1
      "    bnez %[size], 1b            \n" // 如果size不为零，则跳回标签1
      : [dest] "+r"(dest_ptr), [src] "+r"(src_ptr), [size] "+r"(size)
      :
      : "t0", "memory");
}