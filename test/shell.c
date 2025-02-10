/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * *
 * * * * * * Copyright (C) 2018 Institute of Computing Technology, CAS Author :
 * Han Shukai (email : hanshukai@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * *
 * * * * * * The shell acts as a task running in user mode. The main function is
 * to make system calls through the user's output.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * *
 * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * *
 * * * * * */

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SHELL_BEGIN 20
#define BUFLEN 64

#define MAX_ARGS 10

char current_dir[64] = "/";

int get_char();
void parse_input(char *input_buf, int *argc, char *argv[]);
char *mystrchr(const char *str, int c);
int main(void) {
  sys_clear();
  sys_move_cursor(0, SHELL_BEGIN);
  printf("------------------- COMMAND -------------------\n");
  printf("> root@UCAS_OS:%s# ", current_dir);
  char input_buf[BUFLEN];
  while (1) {
    // TODO [P3-task1]: call syscall to read UART port
    // TODO [P3-task1]: parse input
    // note: backspace maybe 8('\b') or 127(delete)
    int input_buf_index = 0;
    int ch;
    while ((ch = get_char()) != '\n' && ch != '\r') {
      if (ch == 8 || ch == 127) {
        if (input_buf_index > 0) {
          input_buf_index--;
          sys_backspace();
        }
      } else {
        if (input_buf_index < BUFLEN - 1) {
          input_buf[input_buf_index++] = ch;
          printf("%c", ch);
        } else {
          input_buf_index = 0;
          printf("\nERROR[shell]: Input too long\n");
          printf("> root@UCAS_OS:%s# ", current_dir);
          continue;
        }
      }
    }
    input_buf[input_buf_index] = '\0';
    // TODO [P3-task1]: ps, exec, kill, clear
    int argc;
    char *argv[MAX_ARGS];
    parse_input(input_buf, &argc, argv);
    if (argc == 0) {
      printf("\n> root@UCAS_OS:%s# ", current_dir);
      continue;
    }
    int wait_flag = strcmp(argv[argc - 1], "&") == 0;
    if (wait_flag) {
      argv[argc - 1] = NULL;
      argc--;
    }
    if (strcmp(argv[0], "ps") == 0) {
      sys_ps();
    } else if (strcmp(argv[0], "exec") == 0) {
      pid_t pid = sys_exec(argv[1], argc - 1, argv + 1);
      if (pid == -1) {
        printf("\nERROR[exec]: execute \"%s\" failed\n", argv[1]);
      } else {
        printf("\nINFO[exec]: execute \"%s\" successfully, pid: %d\n", argv[1],
               pid);
        if (!wait_flag) {
          sys_waitpid(pid);
        }
      }
    } else if (strcmp(argv[0], "kill") == 0) {
      if (atoi(argv[1]) < 1) {
        printf("\nERROR[kill]: wrong operand\n");
      } else {
        int kill_info = sys_kill(atoi(argv[1]));
        if (kill_info == -1) {
          printf("\nERROR[kill]: task %d not found\n", atoi(argv[1]));
        } else {
          printf("\nINFO[kill]: kill task %d\n", atoi(argv[1]));
        }
      }
    } else if (strcmp(argv[0], "clear") == 0 || strcmp(argv[0], "c") == 0) {
      sys_clear();
      sys_move_cursor(0, SHELL_BEGIN);
      printf("------------------- COMMAND -------------------\n");
    } else if (strcmp(argv[0], "mkfs") == 0) {
      sys_mkfs();
    } else if (strcmp(argv[0], "statfs") == 0) {
      sys_statfs();
    } else if (strcmp(argv[0], "cd") == 0) {
      char cd_dir[64];
      strcpy(cd_dir, argv[1]);
      if (sys_cd(argv[1])) {
        printf("\nERROR[cd]: cd failed\n");
      } else {
        int i = 0;
        while (i < strlen(cd_dir)) {
          // 读取一个子目录名称
          char name[15];
          int j = 0;
          while (i < strlen(cd_dir) && cd_dir[i] != '/') {
            name[j++] = cd_dir[i++];
          }
          name[j] = '\0';

          // 如果当前字符是'/'，跳过它
          if (i < strlen(cd_dir) && cd_dir[i] == '/') {
            i++;
          }

          if (strcmp(name, "") == 0 || strcmp(name, ".") == 0) {
            // 空或"."不需要改动
            continue;
          } else if (strcmp(name, "..") == 0) {
            // 退回上一级目录
            if (strcmp(current_dir, "/") != 0 && strlen(current_dir) > 0) {
              int k = strlen(current_dir) - 1;
              while (k > 0 && current_dir[k] != '/') {
                current_dir[k--] = '\0';
              }
              current_dir[k] = (k == 0) ? '/' : '\0';
            }
          } else {
            // 普通子目录，拼接路径
            if (strcmp(current_dir, "/") == 0) {
              strcat(current_dir, name);
            } else {
              strcat(current_dir, "/");
              strcat(current_dir, name);
            }
          }
        }
      }
      printf("\n");
    } else if (strcmp(argv[0], "mkdir") == 0) {
      if (argc == 1) {
        printf("\nERROR[mkdir]: missing operand\n");
      } else {
        sys_mkdir(argv[1]);
        printf("\n");
      }
    } else if (strcmp(argv[0], "rmdir") == 0) {
      if (argc == 1) {
        printf("\nERROR[rmdir]: missing operand\n");
      } else {
        int info = sys_rmdir(argv[1]);
        if (info == -1) {
          printf("\nERROR[rmdir]: rmdir failed\n");
        } else {
          printf("\n");
        }
      }
    } else if (strcmp(argv[0], "ls") == 0) {
      int option = 0;
      if (argc == 2 && strcmp(argv[1], "-l") == 0) {
        option = 1;
      }
      if (argc == 1 || (argc == 2 && option)) {
        sys_ls(NULL, option);
      } else if (argc == 2) {
        sys_ls(argv[1], option);
      }
    } else if (strcmp(argv[0], "touch") == 0) {
      sys_touch(argv[1]);
      printf("\n");
    } else if (strcmp(argv[0], "cat") == 0) {
      sys_cat(argv[1]);
      printf("\n");
    } else if (strcmp(argv[0], "ln") == 0) {
      sys_ln(argv[1], argv[2]);
      printf("\n");
    } else if (strcmp(argv[0], "rm") == 0) {
      sys_rm(argv[1]);
      printf("\n");
    }

    else {
      printf("\nERROR[shell]: Unknown command: %s\n", argv[0]);
    }
    printf("> root@UCAS_OS:%s# ", current_dir);

    /************************************************************/
    // TODO [P6-task1]: mkfs, statfs, cd, mkdir, rmdir, ls

    // TODO [P6-task2]: touch, cat, ln, ls -l, rm
    /************************************************************/
  }

  return 0;
}

int get_char() {
  int c;
  while ((c = sys_getchar()) == -1)
    ;
  return c;
}

// 自定义字符串分割函数
char *my_strtok(char *str, const char *delim) {
  static char *static_str = NULL; // 保存上一次调用时剩余的字符串
  if (str != NULL) {
    static_str = str;
  }
  if (static_str == NULL) {
    return NULL;
  }

  // 跳过前导分隔符
  char *token_start = static_str;
  while (*token_start && mystrchr(delim, *token_start)) {
    token_start++;
  }
  if (*token_start == '\0') {
    static_str = NULL;
    return NULL;
  }

  // 查找下一个分隔符
  char *token_end = token_start;
  while (*token_end && !mystrchr(delim, *token_end)) {
    token_end++;
  }
  if (*token_end == '\0') {
    static_str = NULL;
  } else {
    *token_end = '\0';
    static_str = token_end + 1;
  }

  return token_start;
}

// 解析输入字符串
void parse_input(char *input_buf, int *argc, char *argv[]) {
  // 初始化 argc
  *argc = 0;

  // 使用自定义的 my_strtok 分割字符串
  char *token = my_strtok(input_buf, " \t\n");
  while (token != NULL && *argc < MAX_ARGS - 1) {
    argv[*argc] = token;
    (*argc)++;
    token = my_strtok(NULL, " \t\n");
  }
  argv[*argc] = NULL; // 末尾添加NULL，以便execvp使用
}

char *mystrchr(const char *str, int c) {
  while (*str != '\0') {
    if (*str == (char)c) {
      return (char *)str;
    }
    str++;
  }
  if (c == '\0') {
    return (char *)str;
  }
  return NULL;
}