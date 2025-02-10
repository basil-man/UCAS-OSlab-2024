#ifndef __INCLUDE_OS_FS_H__
#define __INCLUDE_OS_FS_H__

#include <os/kernel.h>
#include <os/mm.h>
#include <os/sched.h>
#include <os/string.h>
#include <screen.h>
#include <type.h>

/* macros of file system */
#define CEIL_DIV(a, n) ((a) / (n) + (((a) % (n) == 0) ? 0 : 1))

#define SECTOR_SIZE 512
#define BLOCK_SIZE 4096
#define SUPERBLOCK_MAGIC 0xDF4C4459
#define NUM_FDESCS 16

#define TOTAL_INODES 1024           // INODE总数
#define TOTAL_DATA_BLOCKS (1 << 18) // 数据块总数（4KB为单位）, 共1GB
#define INODE_BITMAP_SECTOR_COUNT                                              \
  CEIL_DIV(TOTAL_INODES, SECTOR_SIZE * 8) // inode位图占用的sector数量
#define DATA_BITMAP_SECTOR_COUNT                                               \
  CEIL_DIV(TOTAL_DATA_BLOCKS, SECTOR_SIZE * 8) // 数据块位图占用的sector数量
#define INODE_TABLE_SECTOR_COUNT                                               \
  CEIL_DIV(sizeof(inode_t) * TOTAL_INODES,                                     \
           SECTOR_SIZE) // inode表占用的sector数量
#define DATA_SECTOR_COUNT                                                      \
  (TOTAL_DATA_BLOCKS / SECTOR_SIZE * BLOCK_SIZE) // 数据块占用的sector数量

#define DATA_BITMAP_OFFSET 1 // 第一个sector：superblock
#define INODE_BITMAP_OFFSET                                                    \
  (DATA_BITMAP_OFFSET + DATA_BITMAP_SECTOR_COUNT) // inode位图偏移量
#define INODE_TABLE_OFFSET                                                     \
  (INODE_BITMAP_OFFSET + INODE_BITMAP_SECTOR_COUNT) // inode表偏移量
#define DATA_BLOCKS_OFFSET                                                     \
  (INODE_TABLE_OFFSET + INODE_TABLE_SECTOR_COUNT) // 数据块偏移量

#define FILESYSTEM_SIZE_SECTORS                                                \
  (DATA_BLOCKS_OFFSET + DATA_SECTOR_COUNT) // 文件系统大小，以sector为单位
#define FILESYSTEM_START_SEC                                                   \
  ((1 << 29) / SECTOR_SIZE) // 文件系统起始sector（512MB）

#define T_DIR 0
#define T_FILE 1

#define IPSEC (SECTOR_SIZE / sizeof(inode_t))  // inode_t per sector
#define DPSEC (SECTOR_SIZE / sizeof(dentry_t)) // dentry_t per sector
#define DPBLK (BLOCK_SIZE / sizeof(dentry_t))  // dentry_t per block

#define IA_PER_BLOCK (BLOCK_SIZE / sizeof(uint32_t))
#define IA_PER_SECTOR (SECTOR_SIZE / sizeof(uint32_t))

#define DIRECT_SIZE (NDIRECT * BLOCK_SIZE)
#define INDIRECT_1ST_SIZE (3 * BLOCK_SIZE * IA_PER_BLOCK)
#define INDIRECT_2ND_SIZE (2 * BLOCK_SIZE * IA_PER_BLOCK * IA_PER_BLOCK)
#define INDIRECT_3RD_SIZE                                                      \
  (1 * BLOCK_SIZE * IA_PER_BLOCK * IA_PER_BLOCK * IA_PER_BLOCK)
#define MAX_FILE_SIZE                                                          \
  (DIRECT_SIZE + INDIRECT_1ST_SIZE + INDIRECT_2ND_SIZE + INDIRECT_3RD_SIZE)

/* modes of do_fopen */
#define O_RDONLY 1
#define O_WRONLY 2
#define O_RDWR 3

/* whence of do_lseek */
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

/* modes of get_data_blk_addr */
#define CHECK 0
#define SET_UP 1

/* data structures of file system */
typedef struct superblock_t {
  // TODO [P6-task1]: Implement the data structure of superblock
  uint32_t magic;
  uint32_t fs_start_sec;
  uint32_t fs_size; // Size of file system image (blocks)
  uint32_t bmap_offset;
  uint32_t imap_offset;
  uint32_t inode_offset;
  uint32_t inode_num;
  uint32_t data_block_offset;
  uint32_t data_block_num;
} superblock_t;

typedef struct dentry_t {
  // TODO [P6-task1]: Implement the data structure of directory entry
  char name[15];
  int ino;
} dentry_t;

#define NDIRECT 10
typedef struct inode_t {
  // TODO [P6-task1]: Implement the data structure of inode
  char type;
  char mode;
  short nlink; // Number of links to inode in file system
  uint32_t ino;
  uint32_t ctime;
  uint32_t atime;
  uint32_t mtime;
  uint32_t size;
  uint32_t direct_addrs[NDIRECT];
  uint32_t indirect_addrs1[3];
  uint32_t indirect_addrs2[2];
  uint32_t indirect_addrs3;
} inode_t;

typedef struct fdesc_t {
  // TODO [P6-task2]: Implement the data structure of file descriptor
  uint8_t valid;
  uint8_t mode;
  short ref; // reference count
  int ino;
  uint32_t write_ptr;
  uint32_t read_ptr;
} fdesc_t;

/* fs function declarations */
extern int is_fs_init();
extern int do_mkfs(void);
extern int do_statfs(void);
extern int do_cd(char *path);
extern int do_mkdir(char *path);
extern int do_rmdir(char *path);
extern int do_ls(char *path, int option);
extern int do_open(char *path, int mode);
extern int do_read(int fd, char *buff, int length);
extern int do_write(int fd, char *buff, int length);
extern int do_close(int fd);
extern int do_ln(char *src_path, char *dst_path);
extern int do_rm(char *path);
extern int do_lseek(int fd, int offset, int whence);
extern int do_touch(char *path);
extern int do_cat(char *path);
#endif