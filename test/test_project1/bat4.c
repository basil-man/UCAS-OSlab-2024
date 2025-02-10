#include <kernel.h>
#define DATA_ADDR 0x52190000
#define MAX_STR_LEN 20
#define MAX_LIST_LEN 10
void memcpy_riscv(void *dest_ptr, const void *src_ptr, int size);
char a[MAX_LIST_LEN][MAX_STR_LEN];

int main(void) {
  // bios_putstr("[bat4] Info: reading shared mem...\n");
  int a_iter = 0;
  char *src_ptr = (char *)DATA_ADDR;
  while (1) {
    memcpy_riscv((void *)a[a_iter], (void *)src_ptr, MAX_STR_LEN);
    if (a[a_iter][0] == '\0') {
      break;
    }
    bios_putstr(a[a_iter]);
    a_iter++;

    bios_putchar('\n');
    src_ptr += MAX_STR_LEN;
  }
  // bios_putstr("[bat4] Info: editing strings.\n");
  int j;
  for (j = 0; j < a_iter; j++) {
    for (int i = MAX_STR_LEN - 3; i >= 0; i--) {
      a[j][i + 2] = a[j][i];
    }
    a[j][0] = j + '0';
    a[j][1] = ':';
  }
  char name[] = "renhaolin";
  int i;
  for (i = 0; name[i] != '\0'; i++) {
    a[j][i] = name[i];
  }
  a[j][i] = '\0';

  bios_putstr("[bat4] Info: editing strings done:\n");
  for (int i = 0; i < a_iter + 1; i++) {
    bios_putstr(a[i]);
    bios_putchar('\n');
  }
  return 0;
  bios_putchar('\n');
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
