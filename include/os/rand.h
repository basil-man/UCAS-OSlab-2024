#ifndef INCLUDE_MYRAND_H_
#define INCLUDE_MYRAND_H_

#include <os/time.h>
#include <type.h>
// 线性同余生成器（LCG）参数
#define LCG_A 1664525
#define LCG_C 1013904223
#define LCG_M 4294967296 // 2^32

// 全局变量，保存生成的随机数的当前状态
static unsigned int lcg_state = 0;

// 初始化随机数生成器，使用当前时间作为种子
static inline void lcg_init() {
  lcg_state = (unsigned int)get_ticks(); // 使用当前时间作为种子
}

// 生成范围在 [min, max] 之间的随机数
static inline int generate_random_number(int min, int max) {
  // 使用线性同余生成器更新状态并生成随机数
  lcg_state = (LCG_A * lcg_state + LCG_C) % LCG_M;

  // 将生成的随机数映射到 [min, max) 范围
  return min + (lcg_state % (max - min));
}

#endif
