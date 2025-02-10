#include <kernel.h>
#define DATA_ADDR 0x52190000
#define MAX_STR_LEN 20
#define MAX_LIST_LEN 10
void memcpy_riscv(void *dest_ptr, const void *src_ptr, int size);
char a[MAX_LIST_LEN][MAX_STR_LEN];

void swap(char a[], char b[]) {
  char temp[MAX_STR_LEN];
  int i = 0;
  // a to temp
  while (a[i] != '\0') {
    temp[i] = a[i];
    i++;
  }
  temp[i] = '\0';
  // b to a
  i = 0;
  while (b[i] != '\0') {
    a[i] = b[i];
    i++;
  }
  a[i] = '\0';
  // temp to b
  i = 0;
  while (temp[i] != '\0') {
    b[i] = temp[i];
    i++;
  }
  b[i] = '\0';
}

int compareStrings(const char a[], const char b[]) {
  int i = 0;
  while (a[i] != '\0' && b[i] != '\0') {
    if (a[i] < b[i])
      return -1;
    if (a[i] > b[i])
      return 1;
    i++;
  }
  return a[i] == '\0' ? -1 : b[i] == '\0' ? 1 : 0;
}

void sortStrings(char a[][MAX_STR_LEN], int n) {
  int i, j;
  for (i = 0; i < n - 1; i++) {
    for (j = 0; j < n - i - 1; j++) {
      if (a[j][0] != '\0' && a[j + 1][0] != '\0' &&
          compareStrings(a[j], a[j + 1]) > 0) {
        swap(a[j], a[j + 1]);
      }
    }
  }
}

int main(void) {
  // bios_putstr("[bat2] Info: reading shared mem...\n");
  int a_iter = 0;
  char *src_ptr = (char *)DATA_ADDR;
  while (1) {
    memcpy_riscv((void *)a[a_iter], (void *)src_ptr, MAX_STR_LEN);
    if (a[a_iter][0] == '\0') {
      break;
    }
    a_iter++;

    // bios_putstr(a[a_iter++]);
    // bios_putchar('\n');
    src_ptr += MAX_STR_LEN;
  }
  // bios_putstr("[bat2] Info: reading shared mem done.\n");

  sortStrings(a, a_iter);
  bios_putstr("[bat2] Info: sorted list:\n");
  for (int i = 0; i < a_iter; i++) {
    bios_putstr(a[i]);
    bios_putchar('\n');
  }

  char *dest_ptr = (char *)DATA_ADDR;
  bios_putstr("[bat2] Info: writing shared mem...\n");
  memcpy_riscv((void *)dest_ptr, (void *)a, sizeof(a));
  bios_putstr("[bat2] Info: writing shared mem done.\n");
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
