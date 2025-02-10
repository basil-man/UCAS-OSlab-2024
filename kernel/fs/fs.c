#include <os/fs.h>
#include <os/string.h>
#include <os/time.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))

static fdesc_t fdesc_array[NUM_FDESCS];
static superblock_t superblock;
static inode_t current_inode;
static char imap[TOTAL_INODES / 8];
static char bmap[TOTAL_DATA_BLOCKS / 8];
static char buf[BLOCK_SIZE];
static char buf2[BLOCK_SIZE];
static char buf3[BLOCK_SIZE]; // for do_rm
static char buf4[BLOCK_SIZE]; // for do_rm

int is_fs_init() {
  if (superblock.magic == SUPERBLOCK_MAGIC) {
    return 1;
  } else {
    bios_sd_read(kva2pa((uintptr_t)&superblock), 1, FILESYSTEM_START_SEC);
    if (superblock.magic == SUPERBLOCK_MAGIC) {
      bios_sd_read(kva2pa((uintptr_t)imap), INODE_BITMAP_SECTOR_COUNT,
                   FILESYSTEM_START_SEC + INODE_BITMAP_OFFSET);
      bios_sd_read(kva2pa((uintptr_t)bmap), DATA_BITMAP_SECTOR_COUNT,
                   FILESYSTEM_START_SEC + DATA_BITMAP_OFFSET);
      bios_sd_read(kva2pa((uintptr_t)buf), 1,
                   FILESYSTEM_START_SEC + INODE_TABLE_OFFSET);
      memcpy((uint8_t *)&current_inode, (uint8_t *)buf, sizeof(inode_t));
      return 1;
    }
  }
  return 0;
}

static inline int alloc_block(uint32_t *addr_array, int num) {
  int i, j, allocated = 0;
  for (i = 0; i < TOTAL_DATA_BLOCKS / 8 && allocated < num; i++) {
    if (bmap[i] != 0xff) {
      for (j = 0; j < 8 && allocated < num; j++) {
        if ((bmap[i] & (1 << j)) == 0) {
          bmap[i] |= (1 << j);
          addr_array[allocated] = (i * 8 + j) * BLOCK_SIZE / SECTOR_SIZE +
                                  FILESYSTEM_START_SEC + DATA_BLOCKS_OFFSET;
          allocated++;
        }
      }
    }
  }
  bios_sd_write(kva2pa((uintptr_t)bmap), DATA_BITMAP_SECTOR_COUNT,
                FILESYSTEM_START_SEC + DATA_BITMAP_OFFSET);
  return (allocated == num) ? 0 : -1;
}

static inline int alloc_inode() {
  int i, j;
  for (i = 0; i < TOTAL_INODES / 8; i++) {
    if (imap[i] != 0xff) {
      for (j = 0; j < 8; j++) {
        if ((imap[i] & (1 << j)) == 0) {
          imap[i] |= (1 << j);
          bios_sd_write(kva2pa((uintptr_t)imap), INODE_BITMAP_SECTOR_COUNT,
                        FILESYSTEM_START_SEC + INODE_BITMAP_OFFSET);
          return i * 8 + j;
        }
      }
    }
  }
  return -1;
}

static inline inode_t set_inode(uint8_t type, int mode, int ino) {
  inode_t inode;
  bzero((uint8_t *)&inode, sizeof(inode_t));
  inode.type = type;
  inode.mode = mode;
  inode.ino = ino;
  inode.nlink = 1;
  inode.size = 0;
  inode.atime = inode.ctime = inode.mtime = get_timer();
  return inode;
}

static inline inode_t *get_inode(int ino) {
  if (ino < 0 || ino >= TOTAL_INODES) {
    return NULL;
  }
  bios_sd_read(kva2pa((uintptr_t)buf), 1,
               FILESYSTEM_START_SEC + INODE_TABLE_OFFSET + ino / IPSEC);
  return (inode_t *)(buf + (ino % IPSEC) * sizeof(inode_t));
}

int do_mkfs(void) {
  // TODO [P6-task1]: Implement do_mkfs
  if (is_fs_init()) {
    printk("\n[FS]: File system has been initialized.\n");
    return -1;
  }
  printk("\n[FS]: Initializing file system...\n");
  printk("[FS]: Setting superblock...\n");
  superblock.magic = SUPERBLOCK_MAGIC;
  superblock.fs_start_sec = FILESYSTEM_START_SEC;
  superblock.fs_size = FILESYSTEM_SIZE_SECTORS;
  superblock.bmap_offset = DATA_BITMAP_OFFSET;
  superblock.imap_offset = INODE_BITMAP_OFFSET;
  superblock.inode_offset = INODE_TABLE_OFFSET;
  superblock.data_block_offset = DATA_BLOCKS_OFFSET;
  superblock.data_block_num = TOTAL_DATA_BLOCKS;
  superblock.inode_num = TOTAL_INODES;
  printk("      magic: 0x%x\n", superblock.magic);
  printk("      num sector: %d, start sector: %d\n", superblock.fs_size,
         superblock.fs_start_sec);
  printk("      block map offset: %d(%d)\n", superblock.bmap_offset,
         DATA_BITMAP_SECTOR_COUNT);
  printk("      inode map offset: %d(%d)\n", superblock.imap_offset,
         INODE_BITMAP_SECTOR_COUNT);
  printk("      inode offset: %d(%d)\n", superblock.inode_offset,
         INODE_TABLE_SECTOR_COUNT);
  printk("      data block offset: %d(%d)\n", superblock.data_block_offset,
         DATA_SECTOR_COUNT);
  bios_sd_write(kva2pa((uintptr_t)&superblock), 1, FILESYSTEM_START_SEC);
  // Setting inode-map
  printk("[FS]: Setting inode-map...\n");
  bzero((uint8_t *)imap, TOTAL_INODES / 8);
  bios_sd_write(kva2pa((uintptr_t)imap), INODE_BITMAP_SECTOR_COUNT,
                FILESYSTEM_START_SEC + INODE_BITMAP_OFFSET);
  // Setting sector-map
  printk("[FS]: Setting sector-map...\n");
  bzero((uint8_t *)bmap, TOTAL_DATA_BLOCKS / 8);
  bios_sd_write(kva2pa((uintptr_t)bmap), DATA_BITMAP_SECTOR_COUNT,
                FILESYSTEM_START_SEC + DATA_BITMAP_OFFSET);
  // Setting inode
  printk("[FS]: Setting inode...\n");
  int root_ino = alloc_inode();
  if (root_ino < 0) {
    printk("[FS]: Error: No inode available.\n");
    return -1;
  }
  bzero((uint8_t *)buf, BLOCK_SIZE);
  dentry_t *dentry = (dentry_t *)buf;
  strcpy(dentry[0].name, ".");
  strcpy(dentry[1].name, "..");
  dentry[0].ino = dentry[1].ino = root_ino;
  uint32_t block_addr;
  if (alloc_block(&block_addr, 1) < 0) {
    printk("[FS]: Error: No block available.\n");
    return -1;
  }
  bios_sd_write(kva2pa((uintptr_t)buf), 1, block_addr);
  inode_t *root_inode = get_inode(root_ino);
  *root_inode = set_inode(T_DIR, O_RDONLY, root_ino);
  root_inode->direct_addrs[0] = block_addr;
  root_inode->size = 2 * sizeof(dentry_t);
  current_inode = *root_inode;
  bios_sd_write(kva2pa((uintptr_t)buf), 1,
                FILESYSTEM_START_SEC + INODE_TABLE_OFFSET + root_ino / IPSEC);
  bzero(fdesc_array, sizeof(fdesc_array));
  printk("[FS]: File system initialized.\n");
  return 0; // do_mkfs succeeds
}

int do_statfs(void) {
  // TODO [P6-task1]: Implement do_statfs
  if (!is_fs_init()) {
    printk("\n[FS]: File system has not been initialized.\n");
    return -1;
  }
  int inode_used = 0, block_used = 0;
  for (int i = 0; i < TOTAL_INODES / 8; i++) {
    for (int j = 0; j < 8; j++) {
      if (imap[i] & (1 << j)) {
        inode_used++;
      }
    }
  }
  for (int i = 0; i < TOTAL_DATA_BLOCKS / 8; i++) {
    for (int j = 0; j < 8; j++) {
      if (bmap[i] & (1 << j)) {
        block_used++;
      }
    }
  }

  printk("\n[FS] state:\n");
  printk("      magic: 0x%x\n", superblock.magic);
  printk("      used sector: %d/%d, start sector: %d(0x%08x)\n",
         INODE_TABLE_OFFSET + CEIL_DIV(inode_used, IPSEC) +
             block_used * (BLOCK_SIZE / SECTOR_SIZE),
         superblock.fs_size, superblock.fs_start_sec, superblock.fs_start_sec);
  printk("      block map offset: %d, occupied sector: %d, block used: %d/%d\n",
         superblock.bmap_offset, DATA_BITMAP_SECTOR_COUNT, block_used,
         TOTAL_DATA_BLOCKS);
  printk("      inode map offset: %d, occupied sector: %d, inode used: %d/%d\n",
         superblock.imap_offset, INODE_BITMAP_SECTOR_COUNT, inode_used,
         TOTAL_INODES);
  printk("      inode offset: %d, occupied sector: %d\n",
         superblock.inode_offset, INODE_TABLE_SECTOR_COUNT);
  printk("      data offset: %d, occupied sector: %d\n",
         superblock.data_block_offset, DATA_SECTOR_COUNT);
  printk("      inode size: %dB, dir entry size: %dB\n", sizeof(inode_t),
         sizeof(dentry_t));
  return 0; // do_statfs succeeds
}
static inline int get_inode_by_name(inode_t current_node, const char *name,
                                    inode_t *pinode) {
  if (current_node.type != T_DIR) {
    return 0;
  }
  bios_sd_read(kva2pa((uintptr_t)buf), 8, current_node.direct_addrs[0]);
  dentry_t *dentry = (dentry_t *)buf;
  for (int i = 0; i < DPBLK; i++) {
    if (strcmp(dentry[i].name, name) == 0) {
      if (pinode == NULL) {
        return 1;
      }
      *pinode = *get_inode(dentry[i].ino);
      return 1;
    }
  }
  return 0;
}
static inline int parse_path(char *path, inode_t current_node,
                             inode_t *res_inode) {
  if (path == NULL || res_inode == NULL) {
    return 0;
  }

  char *token;
  const char delim[] = "/";
  inode_t inode = current_node;

  token = my_strtok(path, delim);
  while (token != NULL) {
    inode_t pinode;
    if (get_inode_by_name(inode, token, &pinode) == 0) {
      return 0;
    }
    inode = pinode;
    token = my_strtok(NULL, delim);
  }

  *res_inode = inode;
  return 1;
}
int do_cd(char *path) {
  // TODO [P6-task1]: Implement do_cd
  if (!is_fs_init()) {
    printk("[FS]: File system has not been initialized.\n");
    return -1;
  }
  inode_t inode;
  if (parse_path(path, current_inode, &inode) == 0) {
    printk("[FS]: Path not found.\n");
    return -2;
  }
  if (inode.type != T_DIR) {
    printk("[FS]: Not a directory.\n");
    return -3;
  }
  current_inode = inode;
  return 0; // do_cd succeeds
}

int do_mkdir(char *path) {
  // TODO [P6-task1]: Implement do_mkdir
  if (!is_fs_init()) {
    printk("\n[FS]: File system has not been initialized.");
    return -1;
  }
  if (get_inode_by_name(current_inode, path, NULL)) {
    printk("\n[FS]: Directory already exists.");
    return -2;
  }

  int ino = alloc_inode();

  // create dentry
  bzero((uint8_t *)buf, BLOCK_SIZE);
  dentry_t *dentry = (dentry_t *)buf;
  strcpy(dentry[0].name, ".");
  strcpy(dentry[1].name, "..");
  dentry[0].ino = ino;
  dentry[1].ino = current_inode.ino;
  uint32_t block_addr;
  if (alloc_block(&block_addr, 1) < 0) {
    printk("\n[FS] Error: No block available.");
    return -1;
  }
  bios_sd_write(kva2pa((uintptr_t)buf), 1, block_addr);
  // init inode
  inode_t *inode = get_inode(ino);
  *inode = set_inode(T_DIR, O_RDONLY, ino);
  inode->direct_addrs[0] = block_addr;
  inode->size = 2 * sizeof(dentry_t);
  bios_sd_write(kva2pa((uintptr_t)buf), 1,
                FILESYSTEM_START_SEC + INODE_TABLE_OFFSET +
                    ino / IPSEC); // buf read by get_inode

  // update current_inode
  bios_sd_read(kva2pa((uintptr_t)buf), BLOCK_SIZE / SECTOR_SIZE,
               current_inode.direct_addrs[0]);
  dentry_t *dentry_current = (dentry_t *)buf;
  for (int i = 0; i < DPBLK; i++) {
    if (dentry_current[i].name[0] == 0) {
      strcpy(dentry_current[i].name, path);
      dentry_current[i].ino = ino;
      break;
    }
  }
  bios_sd_write(kva2pa((uintptr_t)buf), 8, current_inode.direct_addrs[0]);
  inode_t *current_inode_ptr = get_inode(current_inode.ino);
  current_inode_ptr->size += sizeof(dentry_t);
  bios_sd_write(kva2pa((uintptr_t)buf), 1,
                FILESYSTEM_START_SEC + INODE_TABLE_OFFSET +
                    current_inode.ino / IPSEC);
  return 0; // do_mkdir succeeds
}

int do_rmdir(char *path) {
  // TODO [P6-task1]: Implement do_rmdir
  if (!is_fs_init()) {
    printk("\n[FS]: File system has not been initialized.");
    return -1;
  }
  inode_t inode;
  if (parse_path(path, current_inode, &inode) == 0) {
    printk("\n[FS]: Path not found.");
    return -2;
  }
  if (inode.type != T_DIR) {
    printk("\n[FS]: Not a directory.");
    return -3;
  }
  if (inode.size > 2 * sizeof(dentry_t)) {
    printk("\n[FS]: Directory not empty.");
    return -4;
  }
  // delete inode
  inode_t *inode_ptr = get_inode(inode.ino); // buf read by get_inode
  bzero((uint8_t *)inode_ptr, sizeof(inode_t));
  bios_sd_write(kva2pa((uintptr_t)buf), 1,
                FILESYSTEM_START_SEC + INODE_TABLE_OFFSET + inode.ino / IPSEC);
  // reset imap
  imap[inode.ino / 8] &= ~(1 << (inode.ino % 8));
  bios_sd_write(kva2pa((uintptr_t)imap), INODE_BITMAP_SECTOR_COUNT,
                FILESYSTEM_START_SEC + INODE_BITMAP_OFFSET);
  // reset dentry
  bzero((uint8_t *)buf, BLOCK_SIZE);
  bios_sd_write(kva2pa((uintptr_t)buf), 1, inode.direct_addrs[0]);
  // reset bmap
  int block_idx =
      (inode.direct_addrs[0] - FILESYSTEM_START_SEC - DATA_BLOCKS_OFFSET) /
      (BLOCK_SIZE / SECTOR_SIZE);
  bmap[block_idx / 8] &= ~(1 << (block_idx % 8));
  bios_sd_write(kva2pa((uintptr_t)bmap), DATA_BITMAP_SECTOR_COUNT,
                FILESYSTEM_START_SEC + DATA_BITMAP_OFFSET);
  // update current_inode
  bios_sd_read(kva2pa((uintptr_t)buf), 8, current_inode.direct_addrs[0]);
  dentry_t *dentry = (dentry_t *)buf;
  for (int i = 0; i < DPBLK; i++) {
    if (dentry[i].ino == inode.ino) {
      dentry[i].ino = 0;
      dentry[i].name[0] = '\0';
      break;
    }
  }
  bios_sd_write(kva2pa((uintptr_t)buf), 8, current_inode.direct_addrs[0]);
  inode_t *current_inode_ptr = get_inode(current_inode.ino);
  current_inode_ptr->size -= sizeof(dentry_t);
  bios_sd_write(kva2pa((uintptr_t)buf), 1,
                FILESYSTEM_START_SEC + INODE_TABLE_OFFSET +
                    current_inode.ino / IPSEC);
  return 0; // do_rmdir succeeds
}

int do_ls(char *path, int option) {
  // TODO [P6-task1]: Implement do_ls
  // Note: argument 'option' serves for 'ls -l' in A-core
  if (!is_fs_init()) {
    printk("[FS]: File system has not been initialized.\n");
    return -1;
  }
  inode_t inode;
  if (path == NULL) {
    inode = current_inode;
  } else if (parse_path(path, current_inode, &inode) == 0) {
    printk("[FS]: Path not found.\n");
    return -2;
  }
  if (inode.type != T_DIR) {
    printk("[FS]: Not a directory.\n");
    return -3;
  }
  printk("\n");
  bzero((uint8_t *)buf2, BLOCK_SIZE);
  bios_sd_read(kva2pa((uintptr_t)buf2), 8, inode.direct_addrs[0]);
  dentry_t *dentry = (dentry_t *)buf2;
  for (int i = 0; i < DPBLK; i++) {
    if (dentry[i].name[0] != 0) {
      if (option == 0) {
        printk("%s\n", dentry[i].name);
      } else {
        inode_t *inode_ptr = get_inode(dentry[i].ino);
        printk("%c%c%c nlink:%d  size:%d ctime:%d mtime:%d atime:%d name:%s\n",
               (inode_ptr->type == T_DIR) ? 'd' : '-',
               (inode_ptr->mode & O_RDONLY) ? 'r' : '-',
               (inode_ptr->mode & O_WRONLY) ? 'w' : '-', inode_ptr->nlink,
               inode_ptr->size, inode_ptr->ctime, inode_ptr->mtime,
               inode_ptr->atime, dentry[i].name);
      }
    }
  }

  return 0; // do_ls succeeds
}

int do_open(char *path, int mode) {
  // TODO [P6-task2]: Implement do_open
  if (!is_fs_init()) {
    printk("\n[FS]: File system has not been initialized.");
    return -1;
  }
  inode_t inode;
  if (parse_path(path, current_inode, &inode) == 0) {
    printk("\n[FS]: Path not found.");
    return -2;
  }
  if (inode.type != T_FILE) {
    printk("\n[FS]: Not a file.");
    return -3;
  }
  int fd;
  for (fd = 0; fd < NUM_FDESCS; fd++) {
    if (fdesc_array[fd].valid == 0) {
      break;
    }
  }
  if (fd == NUM_FDESCS) {
    printk("\n[FS]: No available file descriptor.");
    return -4;
  }
  fdesc_array[fd].valid = 1;
  fdesc_array[fd].mode = mode;
  fdesc_array[fd].ref++;
  fdesc_array[fd].ino = inode.ino;
  fdesc_array[fd].write_ptr = 0;
  fdesc_array[fd].read_ptr = 0;
  return fd; // return the id of file descriptor
}

static inline uint32_t get_block_addr(inode_t *inode, int block_idx) {
  int items_per_block = BLOCK_SIZE / sizeof(uint32_t);
  uint32_t addr;
  int ret;

  // 直接地址
  if (block_idx < NDIRECT) {
    if (inode->direct_addrs[block_idx] == 0) {
      ret = alloc_block(&addr, 1);
      if (ret != 0)
        return 0;
      inode->direct_addrs[block_idx] = addr;
      bios_sd_write(kva2pa((uintptr_t)buf), 1,
                    FILESYSTEM_START_SEC + INODE_TABLE_OFFSET +
                        inode->ino / IPSEC);
    }
    return inode->direct_addrs[block_idx];
  }
  block_idx -= NDIRECT;

  // 单重间接
  int single_max = 3 * items_per_block;
  if (block_idx < single_max) {
    int b1 = block_idx / items_per_block;
    int off = block_idx % items_per_block;
    if (inode->indirect_addrs1[b1] == 0) {
      ret = alloc_block(&addr, 1);
      if (ret != 0)
        return 0;
      inode->indirect_addrs1[b1] = addr;
      bios_sd_write(kva2pa((uintptr_t)buf), 1,
                    FILESYSTEM_START_SEC + INODE_TABLE_OFFSET +
                        inode->ino / IPSEC);
    }
    bios_sd_read(kva2pa((uintptr_t)buf2), 8, inode->indirect_addrs1[b1]);
    if (((uint32_t *)buf2)[off] == 0) {
      ret = alloc_block(&addr, 1);
      if (ret != 0)
        return 0;
      ((uint32_t *)buf2)[off] = addr;
      bios_sd_write(kva2pa((uintptr_t)buf2), 8, inode->indirect_addrs1[b1]);
    }
    return ((uint32_t *)buf2)[off];
  }
  block_idx -= single_max;

  // 双重间接
  int double_max = 2 * items_per_block * items_per_block;
  if (block_idx < double_max) {
    int b2 = block_idx / (items_per_block * items_per_block);
    int remain = block_idx % (items_per_block * items_per_block);
    int b2_idx = remain / items_per_block;
    int off = remain % items_per_block;
    if (inode->indirect_addrs2[b2] == 0) {
      ret = alloc_block(&addr, 1);
      if (ret != 0)
        return 0;
      inode->indirect_addrs2[b2] = addr;
      bios_sd_write(kva2pa((uintptr_t)buf), 1,
                    FILESYSTEM_START_SEC + INODE_TABLE_OFFSET +
                        inode->ino / IPSEC);
    }
    bios_sd_read(kva2pa((uintptr_t)buf2), 8, inode->indirect_addrs2[b2]);
    uint32_t next = ((uint32_t *)buf2)[b2_idx];
    if (next == 0) {
      ret = alloc_block(&addr, 1);
      if (ret != 0)
        return 0;
      ((uint32_t *)buf2)[b2_idx] = addr;
      bios_sd_write(kva2pa((uintptr_t)buf2), 8, inode->indirect_addrs2[b2]);
    }
    next = ((uint32_t *)buf2)[b2_idx];
    bios_sd_read(kva2pa((uintptr_t)buf2), 8, next);
    if (((uint32_t *)buf2)[off] == 0) {
      ret = alloc_block(&addr, 1);
      if (ret != 0)
        return 0;
      ((uint32_t *)buf2)[off] = addr;
      bios_sd_write(kva2pa((uintptr_t)buf2), 8, next);
    }
    return ((uint32_t *)buf2)[off];
  }
  block_idx -= double_max;

  // 三重间接
  int triple_max = items_per_block * items_per_block * items_per_block;
  if (block_idx < triple_max) {
    int b3_1 = block_idx / (items_per_block * items_per_block);
    int remain = block_idx % (items_per_block * items_per_block);
    int b3_2 = remain / items_per_block;
    int off = remain % items_per_block;
    if (inode->indirect_addrs3 == 0) {
      ret = alloc_block(&addr, 1);
      if (ret != 0)
        return 0;
      inode->indirect_addrs3 = addr;
      bios_sd_write(kva2pa((uintptr_t)buf), 1,
                    FILESYSTEM_START_SEC + INODE_TABLE_OFFSET +
                        inode->ino / IPSEC);
    }
    bios_sd_read(kva2pa((uintptr_t)buf2), 8, inode->indirect_addrs3);
    uint32_t next1 = ((uint32_t *)buf2)[b3_1];
    if (next1 == 0) {
      ret = alloc_block(&addr, 1);
      if (ret != 0)
        return 0;
      ((uint32_t *)buf2)[b3_1] = addr;
      bios_sd_write(kva2pa((uintptr_t)buf2), 8, inode->indirect_addrs3);
    }
    next1 = ((uint32_t *)buf2)[b3_1];
    bios_sd_read(kva2pa((uintptr_t)buf2), 8, next1);
    uint32_t next2 = ((uint32_t *)buf2)[b3_2];
    if (next2 == 0) {
      ret = alloc_block(&addr, 1);
      if (ret != 0)
        return 0;
      ((uint32_t *)buf2)[b3_2] = addr;
      bios_sd_write(kva2pa((uintptr_t)buf2), 8, next1);
    }
    next2 = ((uint32_t *)buf2)[b3_2];
    bios_sd_read(kva2pa((uintptr_t)buf2), 8, next2);
    if (((uint32_t *)buf2)[off] == 0) {
      ret = alloc_block(&addr, 1);
      if (ret != 0)
        return 0;
      ((uint32_t *)buf2)[off] = addr;
      bios_sd_write(kva2pa((uintptr_t)buf2), 8, next2);
    }
    return ((uint32_t *)buf2)[off];
  }
  return 0;
}

int do_read(int fd, char *buff, int length) {
  // 验证文件描述符
  if (fd < 0 || fd >= NUM_FDESCS || fdesc_array[fd].valid == 0) {
    printk("\n[FS]: Invalid file descriptor.");
    return -1;
  }

  inode_t *inode = get_inode(fdesc_array[fd].ino);
  inode->atime = get_timer();
  bios_sd_write(kva2pa((uintptr_t)buf), 1,
                FILESYSTEM_START_SEC + INODE_TABLE_OFFSET +
                    fdesc_array[fd].ino / IPSEC);
  if (fdesc_array[fd].mode == O_WRONLY) {
    printk("\n[FS]: No permission to read.");
    return -2;
  }

  // 调整读取长度以防越界
  if (fdesc_array[fd].read_ptr + length > inode->size) {
    length = inode->size - fdesc_array[fd].read_ptr;
  }

  int bytes_read = 0;
  while (bytes_read < length) {
    int current_ptr = fdesc_array[fd].read_ptr + bytes_read;
    int block_idx = current_ptr / BLOCK_SIZE;
    int block_offset = current_ptr % BLOCK_SIZE;
    int bytes_to_read = BLOCK_SIZE - block_offset;

    if (bytes_to_read > (length - bytes_read)) {
      bytes_to_read = length - bytes_read;
    }

    uint32_t block_addr = get_block_addr(inode, block_idx);
    if (block_addr == 0) {
      printk("\n[FS]: Failed to get block address.");
      return bytes_read > 0 ? bytes_read : -3;
    }

    // 读取块数据到临时缓冲区
    bios_sd_read(kva2pa((uintptr_t)buf), BLOCK_SIZE / SECTOR_SIZE, block_addr);

    // 拷贝数据到用户缓冲区
    memcpy((uint8_t *)buff + bytes_read, (uint8_t *)buf + block_offset,
           bytes_to_read);
    bytes_read += bytes_to_read;
  }

  // 更新文件描述符的读取指针
  fdesc_array[fd].read_ptr += bytes_read;
  return bytes_read;
}

int do_write(int fd, char *buff, int length) {
  // 验证文件描述符
  if (fd < 0 || fd >= NUM_FDESCS || fdesc_array[fd].valid == 0) {
    printk("\n[FS]: Invalid file descriptor.");
    return -1;
  }

  inode_t *inode = get_inode(fdesc_array[fd].ino);
  if (fdesc_array[fd].mode == O_RDONLY) {
    printk("\n[FS]: No Permission to write");
    return -2;
  }
  inode->atime = inode->mtime = get_timer();
  bios_sd_write(kva2pa((uintptr_t)buf), 1,
                FILESYSTEM_START_SEC + INODE_TABLE_OFFSET +
                    fdesc_array[fd].ino / IPSEC);

  // 调整写入长度以防超过最大文件大小
  // if (fdesc_array[fd].write_ptr + length > MAX_FILE_SIZE) {
  //   length = MAX_FILE_SIZE - fdesc_array[fd].write_ptr;
  // }

  int bytes_written = 0;
  while (bytes_written < length) {
    int current_ptr = fdesc_array[fd].write_ptr + bytes_written;
    int block_idx = current_ptr / BLOCK_SIZE;
    int block_offset = current_ptr % BLOCK_SIZE;
    int bytes_to_write = BLOCK_SIZE - block_offset;

    if (bytes_to_write > (length - bytes_written)) {
      bytes_to_write = length - bytes_written;
    }

    uint32_t block_addr = get_block_addr(inode, block_idx);
    if (block_addr == 0) {
      printk("\n[FS]: Failed to loacte block");
      return bytes_written > 0 ? bytes_written : -3;
    }

    // 如果需要部分修改块，先读取现有数据
    if (bytes_to_write < BLOCK_SIZE) {
      bios_sd_read(kva2pa((uintptr_t)buf2), BLOCK_SIZE / SECTOR_SIZE,
                   block_addr);
      memcpy((uint8_t *)buf2 + block_offset, (uint8_t *)buff + bytes_written,
             bytes_to_write);
      bios_sd_write(kva2pa((uintptr_t)buf2), BLOCK_SIZE / SECTOR_SIZE,
                    block_addr);
    } else {
      bzero((uint8_t *)buf2, BLOCK_SIZE);
      memcpy((uint8_t *)buf2, (uint8_t *)buff + bytes_written, bytes_to_write);
      bios_sd_write(kva2pa((uintptr_t)buf2), BLOCK_SIZE / SECTOR_SIZE,
                    block_addr);
    }

    bytes_written += bytes_to_write;
  }

  // 更新写指针
  fdesc_array[fd].write_ptr += bytes_written;

  // 更新 inode 的大小如果写入超出原有大小
  if (fdesc_array[fd].write_ptr > inode->size) {
    inode->size = fdesc_array[fd].write_ptr;
    bios_sd_write(kva2pa((uintptr_t)buf), 1,
                  FILESYSTEM_START_SEC + INODE_TABLE_OFFSET +
                      fdesc_array[fd].ino / IPSEC);
  }

  return bytes_written;
}

int do_close(int fd) {
  // TODO [P6-task2]: Implement do_close
  if (fd >= NUM_FDESCS || fd < 0) {
    printk("\n[FS]: invalid fd");
    return -1;
  }
  fdesc_array[fd].ref--;
  if (fdesc_array[fd].ref == 0) {
    bzero(&fdesc_array[fd], sizeof(fdesc_t));
  }
  return 0; // do_close succeeds
}

int do_ln(char *src_path, char *dst_path) {
  // TODO [P6-task2]: Implement do_ln
  if (!is_fs_init()) {
    printk("\n[FS]: File system has not been initialized.");
    return -1;
  }
  inode_t src_inode;
  if (parse_path(src_path, current_inode, &src_inode) == 0) {
    printk("\n[FS]: Source path not found.");
    return -2;
  }
  if (src_inode.type != T_FILE) {
    printk("\n[FS]: Source path is not a file.");
    return -3;
  }
  inode_t dst_inode;
  if (parse_path(dst_path, current_inode, &dst_inode) == 1) {
    printk("\n[FS]: Destination path already exists.");
    return -4;
  }
  // update src_inode
  inode_t *src_inode_ptr = get_inode(src_inode.ino);
  src_inode_ptr->nlink++;
  bios_sd_write(kva2pa((uintptr_t)buf), 1,
                FILESYSTEM_START_SEC + INODE_TABLE_OFFSET +
                    src_inode.ino / IPSEC);
  // update current dir's dentry
  bios_sd_read(kva2pa((uintptr_t)buf), 8, current_inode.direct_addrs[0]);
  dentry_t *dentry = (dentry_t *)buf;
  for (int i = 0; i < DPBLK; i++) {
    if (dentry[i].name[0] == 0) {
      strcpy(dentry[i].name, dst_path);
      dentry[i].ino = src_inode.ino;
      break;
    }
  }
  bios_sd_write(kva2pa((uintptr_t)buf), 8, current_inode.direct_addrs[0]);
  // update current_inode
  inode_t *current_inode_ptr = get_inode(current_inode.ino);
  current_inode_ptr->size += sizeof(dentry_t);
  bios_sd_write(kva2pa((uintptr_t)buf), 1,
                FILESYSTEM_START_SEC + INODE_TABLE_OFFSET +
                    current_inode.ino / IPSEC);
  return 0; // do_ln succeeds
}

int do_rm(char *path) {
  // Check if the filesystem is initialized
  if (!is_fs_init()) {
    printk("[FS]: File system has not been initialized.\n");
    return -1;
  }

  int len = strlen(path);
  for (int i = 0; i < len; i++) {
    if (path[i] == '/') {
      printk("[FS]: Don't support dirs.\n");
      return -2;
    }
  }

  inode_t inode;
  // Parse the path to get the inode
  if (parse_path(path, current_inode, &inode) == 0) {
    printk("[FS]: Path not found.\n");
    return -2;
  }

  // Ensure the inode is a file
  if (inode.type != T_FILE) {
    printk("[FS]: Not a file.\n");
    return -3;
  }

  // Decrement the link count
  inode.nlink--;

  if (inode.nlink == 0) {
    // reset imap
    imap[inode.ino / 8] &= ~(1 << (inode.ino % 8));
    bios_sd_write(kva2pa((uintptr_t)imap), INODE_BITMAP_SECTOR_COUNT,
                  FILESYSTEM_START_SEC + INODE_BITMAP_OFFSET);
    // delete inode
    inode_t *inode_ptr = get_inode(inode.ino); // buf read by get_inode
    bzero((uint8_t *)inode_ptr, sizeof(inode_t));
    bios_sd_write(kva2pa((uintptr_t)buf), 1,
                  FILESYSTEM_START_SEC + INODE_TABLE_OFFSET +
                      inode.ino / IPSEC);
    // recycle block
    // direct
    for (int i = 0; i < NDIRECT; i++) {
      if (inode.direct_addrs[i] != 0) {
        bzero((uint8_t *)buf, BLOCK_SIZE);
        bios_sd_write(kva2pa((uintptr_t)buf), BLOCK_SIZE / SECTOR_SIZE,
                      inode.direct_addrs[i]);
        int block_idx = (inode.direct_addrs[i] - FILESYSTEM_START_SEC -
                         DATA_BLOCKS_OFFSET) /
                        (BLOCK_SIZE / SECTOR_SIZE);
        bmap[block_idx / 8] &= ~(1 << (block_idx % 8));
        bios_sd_write(kva2pa((uintptr_t)bmap), DATA_BITMAP_SECTOR_COUNT,
                      FILESYSTEM_START_SEC + DATA_BITMAP_OFFSET);
      }
    }
    // single indirect
    for (int i = 0; i < 3; i++) {
      if (inode.indirect_addrs1[i] != 0) {
        bios_sd_read(kva2pa((uintptr_t)buf), 8, inode.indirect_addrs1[i]);
        for (int j = 0; j < BLOCK_SIZE / sizeof(uint32_t); j++) {
          if (((uint32_t *)buf)[j] != 0) {
            bzero((uint8_t *)buf2, BLOCK_SIZE);
            bios_sd_write(kva2pa((uintptr_t)buf2), BLOCK_SIZE / SECTOR_SIZE,
                          ((uint32_t *)buf)[j]);
            int block_idx = (((uint32_t *)buf)[j] - FILESYSTEM_START_SEC -
                             DATA_BLOCKS_OFFSET) /
                            (BLOCK_SIZE / SECTOR_SIZE);
            bmap[block_idx / 8] &= ~(1 << (block_idx % 8));
            bios_sd_write(kva2pa((uintptr_t)bmap), DATA_BITMAP_SECTOR_COUNT,
                          FILESYSTEM_START_SEC + DATA_BITMAP_OFFSET);
          }
        }
        int block_idx = (inode.indirect_addrs1[i] - FILESYSTEM_START_SEC -
                         DATA_BLOCKS_OFFSET) /
                        (BLOCK_SIZE / SECTOR_SIZE);
        bmap[block_idx / 8] &= ~(1 << (block_idx % 8));
        bios_sd_write(kva2pa((uintptr_t)bmap), DATA_BITMAP_SECTOR_COUNT,
                      FILESYSTEM_START_SEC + DATA_BITMAP_OFFSET);
      }
    }
    // double indirect
    for (int i = 0; i < 2; i++) {
      if (inode.indirect_addrs2[i] != 0) {
        bios_sd_read(kva2pa((uintptr_t)buf), 8, inode.indirect_addrs2[i]);
        for (int j = 0; j < BLOCK_SIZE / sizeof(uint32_t); j++) {
          if (((uint32_t *)buf)[j] != 0) {
            bios_sd_read(kva2pa((uintptr_t)buf2), 8, ((uint32_t *)buf)[j]);
            for (int k = 0; k < BLOCK_SIZE / sizeof(uint32_t); k++) {
              if (((uint32_t *)buf2)[k] != 0) {
                bzero((uint8_t *)buf3, BLOCK_SIZE);
                bios_sd_write(kva2pa((uintptr_t)buf3), BLOCK_SIZE / SECTOR_SIZE,
                              ((uint32_t *)buf2)[k]);
                int block_idx = (((uint32_t *)buf2)[k] - FILESYSTEM_START_SEC -
                                 DATA_BLOCKS_OFFSET) /
                                (BLOCK_SIZE / SECTOR_SIZE);
                bmap[block_idx / 8] &= ~(1 << (block_idx % 8));
                bios_sd_write(kva2pa((uintptr_t)bmap), DATA_BITMAP_SECTOR_COUNT,
                              FILESYSTEM_START_SEC + DATA_BITMAP_OFFSET);
              }
            }
            int block_idx = (((uint32_t *)buf)[j] - FILESYSTEM_START_SEC -
                             DATA_BLOCKS_OFFSET) /
                            (BLOCK_SIZE / SECTOR_SIZE);
            bmap[block_idx / 8] &= ~(1 << (block_idx % 8));
            bios_sd_write(kva2pa((uintptr_t)bmap), DATA_BITMAP_SECTOR_COUNT,
                          FILESYSTEM_START_SEC + DATA_BITMAP_OFFSET);
          }
        }
        int block_idx = (inode.indirect_addrs2[i] - FILESYSTEM_START_SEC -
                         DATA_BLOCKS_OFFSET) /
                        (BLOCK_SIZE / SECTOR_SIZE);
        bmap[block_idx / 8] &= ~(1 << (block_idx % 8));
        bios_sd_write(kva2pa((uintptr_t)bmap), DATA_BITMAP_SECTOR_COUNT,
                      FILESYSTEM_START_SEC + DATA_BITMAP_OFFSET);
      }
    }
    // triple indirect
    if (inode.indirect_addrs3 != 0) {
      bios_sd_read(kva2pa((uintptr_t)buf), 8, inode.indirect_addrs3);
      for (int i = 0; i < BLOCK_SIZE / sizeof(uint32_t); i++) {
        if (((uint32_t *)buf)[i] != 0) {
          bios_sd_read(kva2pa((uintptr_t)buf2), 8, ((uint32_t *)buf)[i]);
          for (int j = 0; j < BLOCK_SIZE / sizeof(uint32_t); j++) {
            if (((uint32_t *)buf2)[j] != 0) {
              bios_sd_read(kva2pa((uintptr_t)buf3), 8, ((uint32_t *)buf2)[j]);
              for (int k = 0; k < BLOCK_SIZE / sizeof(uint32_t); k++) {
                if (((uint32_t *)buf3)[k] != 0) {
                  bzero((uint8_t *)buf4, BLOCK_SIZE);
                  bios_sd_write(kva2pa((uintptr_t)buf4),
                                BLOCK_SIZE / SECTOR_SIZE,
                                ((uint32_t *)buf3)[k]);
                  int block_idx = (((uint32_t *)buf3)[k] -
                                   FILESYSTEM_START_SEC - DATA_BLOCKS_OFFSET) /
                                  (BLOCK_SIZE / SECTOR_SIZE);
                  bmap[block_idx / 8] &= ~(1 << (block_idx % 8));
                  bios_sd_write(kva2pa((uintptr_t)bmap),
                                DATA_BITMAP_SECTOR_COUNT,
                                FILESYSTEM_START_SEC + DATA_BITMAP_OFFSET);
                }
              }
              int block_idx = (((uint32_t *)buf2)[j] - FILESYSTEM_START_SEC -
                               DATA_BLOCKS_OFFSET) /
                              (BLOCK_SIZE / SECTOR_SIZE);
              bmap[block_idx / 8] &= ~(1 << (block_idx % 8));
              bios_sd_write(kva2pa((uintptr_t)bmap), DATA_BITMAP_SECTOR_COUNT,
                            FILESYSTEM_START_SEC + DATA_BITMAP_OFFSET);
            }
          }
          int block_idx = (((uint32_t *)buf)[i] - FILESYSTEM_START_SEC -
                           DATA_BLOCKS_OFFSET) /
                          (BLOCK_SIZE / SECTOR_SIZE);
          bmap[block_idx / 8] &= ~(1 << (block_idx % 8));
          bios_sd_write(kva2pa((uintptr_t)bmap), DATA_BITMAP_SECTOR_COUNT,
                        FILESYSTEM_START_SEC + DATA_BITMAP_OFFSET);
        }
      }
      int block_idx =
          (inode.indirect_addrs3 - FILESYSTEM_START_SEC - DATA_BLOCKS_OFFSET) /
          (BLOCK_SIZE / SECTOR_SIZE);
      bmap[block_idx / 8] &= ~(1 << (block_idx % 8));
      bios_sd_write(kva2pa((uintptr_t)bmap), DATA_BITMAP_SECTOR_COUNT,
                    FILESYSTEM_START_SEC + DATA_BITMAP_OFFSET);
    }

  } else {
    inode_t *inode_ptr = get_inode(inode.ino);
    inode_ptr->nlink = inode.nlink;
    bios_sd_write(kva2pa((uintptr_t)buf), 1,
                  FILESYSTEM_START_SEC + INODE_TABLE_OFFSET +
                      inode.ino / IPSEC);
  }
  bios_sd_read(kva2pa((uintptr_t)buf), 8, current_inode.direct_addrs[0]);
  dentry_t *dentry = (dentry_t *)buf;
  for (int i = 0; i < DPBLK; i++) {
    if (dentry[i].ino == inode.ino && strcmp(dentry[i].name, path) == 0) {
      dentry[i].ino = 0;
      dentry[i].name[0] = '\0';
      break;
    }
  }
  bios_sd_write(kva2pa((uintptr_t)buf), 8, current_inode.direct_addrs[0]);
  inode_t *current_inode_ptr = get_inode(current_inode.ino);
  current_inode_ptr->size -= sizeof(dentry_t);
  bios_sd_write(kva2pa((uintptr_t)buf), 1,
                FILESYSTEM_START_SEC + INODE_TABLE_OFFSET +
                    current_inode.ino / IPSEC);
  return 0; // do_rm succeedss
}

int do_lseek(int fd, int offset, int whence) {
  // TODO [P6-task2]: Implement do_lseek
  if (fd < 0 || fd >= NUM_FDESCS || fdesc_array[fd].valid == 0) {
    printk("[FS]: Invalid file descriptor.\n");
    return -1;
  }
  inode_t *inode;
  switch (whence) {
  case SEEK_SET:
    fdesc_array[fd].read_ptr = offset;
    fdesc_array[fd].write_ptr = offset;
    return offset;
    break;
  case SEEK_CUR:
    fdesc_array[fd].read_ptr += offset;
    fdesc_array[fd].write_ptr += offset;
    return fdesc_array[fd].read_ptr;
    break;
  case SEEK_END:
    inode = get_inode(fdesc_array[fd].ino);
    fdesc_array[fd].read_ptr = inode->size + offset;
    fdesc_array[fd].write_ptr = inode->size + offset;
    return fdesc_array[fd].read_ptr;
    break;
  default:
    printk("[FS]: Invalid whence.\n");
    return -2;
  }

  return -3; // the resulting offset location from the beginning of the file
}

int do_touch(char *path) {
  if (!is_fs_init()) {
    printk("\n[FS]: File system has not been initialized.");
    return -1;
  }
  inode_t inode;
  if (parse_path(path, current_inode, &inode) == 1) {
    printk("\n[FS]: File already exists.");
    return -2;
  }
  int ino = alloc_inode();
  inode_t *inode_ptr = get_inode(ino);
  *inode_ptr = set_inode(T_FILE, O_RDWR, ino);
  bios_sd_write(kva2pa((uintptr_t)buf), 1,
                FILESYSTEM_START_SEC + INODE_TABLE_OFFSET +
                    ino / IPSEC); // buf read by get_inode
  bios_sd_read(kva2pa((uintptr_t)buf), 8, current_inode.direct_addrs[0]);
  dentry_t *dentry = (dentry_t *)buf;
  for (int i = 0; i < DPBLK; i++) {
    if (dentry[i].name[0] == 0) {
      strcpy(dentry[i].name, path);
      dentry[i].ino = ino;
      break;
    }
  }
  bios_sd_write(kva2pa((uintptr_t)buf), 8, current_inode.direct_addrs[0]);
  inode_t *current_inode_ptr = get_inode(current_inode.ino);
  current_inode_ptr->size += sizeof(dentry_t);
  bios_sd_write(kva2pa((uintptr_t)buf), 1,
                FILESYSTEM_START_SEC + INODE_TABLE_OFFSET +
                    current_inode.ino / IPSEC);
  return 0; // do_touch succeeds
}

int do_cat(char *path) {
  if (!is_fs_init()) {
    printk("\n[FS]: File system has not been initialized.");
    return -1;
  }
  inode_t inode;
  if (parse_path(path, current_inode, &inode) == 0) {
    printk("\n[FS]: Path not found.");
    return -2;
  }
  if (inode.type != T_FILE) {
    printk("\n[FS]: Not a file.");
    return -3;
  }
  printk("\n");
  for (int i = 0; i < inode.size; i += BLOCK_SIZE) {
    bios_sd_read(kva2pa((uintptr_t)buf), BLOCK_SIZE / SECTOR_SIZE,
                 get_block_addr(&inode, i / BLOCK_SIZE));
    for (int j = 0; j < MIN(BLOCK_SIZE, inode.size - i); j++) {
      printk("%c", buf[j]);
    }
  }
  return 0; // do_cat succeeds
}