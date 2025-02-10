#ifndef INCLUDE_STRING_H_
#define INCLUDE_STRING_H_

#include <type.h>

void memcpy(uint8_t *dest, const uint8_t *src, uint32_t len);
void memset(void *dest, uint8_t val, uint32_t len);
void bzero(void *dest, uint32_t len);
int strcmp(const char *str1, const char *str2);
int strncmp(const char *str1, const char *str2, uint32_t n);
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, int n);
char *strcat(char *dest, const char *src);
int strlen(const char *src);
static inline int is_delim(char c, const char *delim) {
  while (*delim) {
    if (c == *delim) {
      return 1;
    }
    delim++;
  }
  return 0;
}
static inline char *my_strtok(char *str, const char *delim) {
  static char *next = NULL;
  if (str != NULL) {
    next = str;
  }
  if (next == NULL) {
    return NULL;
  }

  // 跳过前导分隔符
  while (*next && is_delim(*next, delim)) {
    next++;
  }

  if (*next == '\0') {
    next = NULL;
    return NULL;
  }

  // 标记的开始
  char *start = next;

  // 找到分隔符或字符串结束
  while (*next && !is_delim(*next, delim)) {
    next++;
  }

  if (*next) {
    *next = '\0';
    next++;
  } else {
    next = NULL;
  }

  return start;
}

#endif
