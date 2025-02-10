#include <assert.h>
#include <elf.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#define IMAGE_FILE "./image"
#define ARGS "[--extended] [--vm] <bootblock> <executable-file> ..."

#define SECTOR_SIZE 512
#define BOOT_LOADER_SIG_OFFSET 0x1fe
#define OS_SIZE_LOC (BOOT_LOADER_SIG_OFFSET - 2)
#define BOOT_LOADER_SIG_1 0x55
#define BOOT_LOADER_SIG_2 0xaa

#define NBYTES2SEC(nbytes)                                                     \
  (((nbytes) / SECTOR_SIZE) + ((nbytes) % SECTOR_SIZE != 0))

/* TODO: [p1-task4] design your own task_info_t */
typedef struct {
  char task_name[15];
  int offset;      // start_phyaddr
  int nbytes_task; // filesz
  int p_memsz;
} task_info_t;

#define TASK_MAXNUM 16
static task_info_t taskinfo[TASK_MAXNUM];

/* structure to store command line options */
static struct {
  int vm;
  int extended;
} options;

/* prototypes of local functions */
static void create_image(int nfiles, char *files[]);
static void error(char *fmt, ...);
static void read_ehdr(Elf64_Ehdr *ehdr, FILE *fp);
static void read_phdr(Elf64_Phdr *phdr, FILE *fp, int ph, Elf64_Ehdr ehdr);
static uint64_t get_entrypoint(Elf64_Ehdr ehdr);
static uint32_t get_filesz(Elf64_Phdr phdr);
static uint32_t get_memsz(Elf64_Phdr phdr);
static void write_segment(Elf64_Phdr phdr, FILE *fp, FILE *img, int *phyaddr);
static void write_padding(FILE *img, int *phyaddr, int new_phyaddr);
static void write_img_info(int nbytes_kernel, task_info_t *taskinfo,
                           short tasknum, FILE *img, int phyaddr);

int main(int argc, char **argv) {
  char *progname = argv[0];

  /* process command line options */
  options.vm = 0;
  options.extended = 0;
  while ((argc > 1) && (argv[1][0] == '-') && (argv[1][1] == '-')) {
    char *option = &argv[1][2];

    if (strcmp(option, "vm") == 0) {
      options.vm = 1;
    } else if (strcmp(option, "extended") == 0) {
      options.extended = 1;
    } else {
      error("%s: invalid option\nusage: %s %s\n", progname, progname, ARGS);
    }
    argc--;
    argv++;
  }
  if (options.vm == 1) {
    error("%s: option --vm not implemented\n", progname);
  }
  if (argc < 3) {
    /* at least 3 args (createimage bootblock main) */
    error("usage: %s %s\n", progname, ARGS);
  }
  create_image(argc - 1, argv + 1);
  return 0;
}

/* TODO: [p1-task4] assign your task_info_t somewhere in 'create_image' */
static void create_image(int nfiles, char *files[]) {
  int tasknum = nfiles - 2;
  int nbytes_kernel = 0;
  int phyaddr = 0;
  int start_phyaddr = 0;

  // int beginaddr = 0; // used in task3
  FILE *fp = NULL, *img = NULL;
  Elf64_Ehdr ehdr;
  Elf64_Phdr phdr;

  /* open the image file */
  img = fopen(IMAGE_FILE, "w");
  assert(img != NULL);

  /* for each input file */
  for (int fidx = 0; fidx < nfiles; ++fidx) {
    start_phyaddr = phyaddr; // task4
    int p_memsz = 0;
    int p_filesz = 0;
    int taskidx = fidx - 2;

    /* open input file */
    fp = fopen(*files, "r");
    assert(fp != NULL);

    /* read ELF header */
    read_ehdr(&ehdr, fp);
    printf("0x%04lx: %s\n", ehdr.e_entry, *files);

    /* for each program header */
    for (int ph = 0; ph < ehdr.e_phnum; ph++) {

      /* read program header */
      read_phdr(&phdr, fp, ph, ehdr);

      if (phdr.p_type != PT_LOAD)
        continue;

      /* write segment to the image */
      write_segment(phdr, fp, img, &phyaddr);

      /* update nbytes_kernel */
      if (strcmp(*files, "main") == 0) {
        nbytes_kernel += get_filesz(phdr);
      }

      if (phdr.p_type == PT_LOAD) {
        p_memsz += get_memsz(phdr);
        p_filesz += get_filesz(phdr);
      }
    }

    // init taskinfo for task4
    strcpy(taskinfo[taskidx].task_name, *files);
    taskinfo[taskidx].offset = start_phyaddr;
    taskinfo[taskidx].nbytes_task = p_filesz;
    taskinfo[taskidx].p_memsz = p_memsz;
    /* write padding bytes */
    /**
     * TODO:
     * 1. [p1-task3] do padding so that the kernel and every app program
     *  occupies the same number of sectors
     * 2. [p1-task4] only padding bootblock is allowed!
     */
    if (strcmp(*files, "bootblock") == 0) {
      // beginaddr += SECTOR_SIZE;
      write_padding(img, &phyaddr, SECTOR_SIZE);
    }

    // reserved for task3
    //  else {
    //   beginaddr += 15 * SECTOR_SIZE;
    //   write_padding(img, &phyaddr, beginaddr);
    // }

    fclose(fp);
    files++;
  }
  write_img_info(nbytes_kernel, taskinfo, tasknum, img, phyaddr);
  for (int i = 0; i < tasknum; i++) {
    printf("taskinfo[%d]:\n", i);
    printf("\ttask_name: %s\n", taskinfo[i].task_name);
    printf("\toffset: %d\n", taskinfo[i].offset);
    printf("\tnbytes_task: %d\n", taskinfo[i].nbytes_task);
    printf("\tp_memsz: %d\n", taskinfo[i].p_memsz);
  }
  fclose(img);
}

static void read_ehdr(Elf64_Ehdr *ehdr, FILE *fp) {
  int ret;

  ret = fread(ehdr, sizeof(*ehdr), 1, fp);
  assert(ret == 1);
  assert(ehdr->e_ident[EI_MAG1] == 'E');
  assert(ehdr->e_ident[EI_MAG2] == 'L');
  assert(ehdr->e_ident[EI_MAG3] == 'F');
}

static void read_phdr(Elf64_Phdr *phdr, FILE *fp, int ph, Elf64_Ehdr ehdr) {
  int ret;

  fseek(fp, ehdr.e_phoff + ph * ehdr.e_phentsize, SEEK_SET);
  ret = fread(phdr, sizeof(*phdr), 1, fp);
  assert(ret == 1);
  if (options.extended == 1) {
    printf("\tsegment %d\n", ph);
    printf("\t\toffset 0x%04lx", phdr->p_offset);
    printf("\t\tvaddr 0x%04lx\n", phdr->p_vaddr);
    printf("\t\tfilesz 0x%04lx", phdr->p_filesz);
    printf("\t\tmemsz 0x%04lx\n", phdr->p_memsz);
  }
}

// static uint64_t get_entrypoint(Elf64_Ehdr ehdr) { return ehdr.e_entry; }

static uint32_t get_filesz(Elf64_Phdr phdr) { return phdr.p_filesz; }

static uint32_t get_memsz(Elf64_Phdr phdr) { return phdr.p_memsz; }

static void write_segment(Elf64_Phdr phdr, FILE *fp, FILE *img, int *phyaddr) {
  if (phdr.p_memsz != 0 && phdr.p_type == PT_LOAD) {
    /* write the segment itself */
    /* NOTE: expansion of .bss should be done by kernel or runtime env! */
    if (options.extended == 1) {
      printf("\t\twriting 0x%04lx bytes\n", phdr.p_filesz);
    }
    fseek(fp, phdr.p_offset, SEEK_SET);
    while (phdr.p_filesz-- > 0) {
      fputc(fgetc(fp), img);
      (*phyaddr)++;
    }
  }
}

static void write_padding(FILE *img, int *phyaddr, int new_phyaddr) {
  if (options.extended == 1 && *phyaddr < new_phyaddr) {
    printf("\t\twrite 0x%04x bytes for padding\n", new_phyaddr - *phyaddr);
  }

  while (*phyaddr < new_phyaddr) {
    fputc(0, img);
    (*phyaddr)++;
  }
}

static void write_img_info(int nbytes_kernel, task_info_t *taskinfo,
                           short tasknum, FILE *img, int phyaddr) {
  // TODO: [p1-task3] & [p1-task4] write image info to some certain places
  // NOTE: os size, infomation about app-info sector(s) ...
  uint32_t task_end_addr = phyaddr;
  int kernel_nsec = NBYTES2SEC(nbytes_kernel);
  uint32_t nbytes_taskinfo = sizeof(task_info_t) * tasknum;
  printf("sizeof(task_info_t): %lu\ntasknum: %d\ntask_end_addr: "
         "%d\nnbytes_taskinfo:%d\nnblocks_taskinfo:%d\n",
         sizeof(task_info_t), tasknum, task_end_addr, nbytes_taskinfo,
         NBYTES2SEC(nbytes_taskinfo));
  fseek(img, OS_SIZE_LOC - 10, SEEK_SET);
  fwrite(&task_end_addr, 4, 1, img);
  fwrite(&nbytes_taskinfo, 4, 1, img);
  fwrite(&tasknum, 2, 1, img);
  fwrite(&kernel_nsec, 2, 1, img);
  printf("Kernel size: %d sectors\n", kernel_nsec);

  fseek(img, 0, SEEK_END);
  fwrite(taskinfo, nbytes_taskinfo, 1, img);
}

/* print an error message and exit */
static void error(char *fmt, ...) {
  va_list args;

  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  if (errno != 0) {
    perror(NULL);
  }
  exit(EXIT_FAILURE);
}
