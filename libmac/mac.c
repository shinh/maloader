// Copyright 2011 Shinichiro Hamaji. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
//   1. Redistributions of source code must retain the above copyright
//      notice, this list of  conditions and the following disclaimer.
//
//   2. Redistributions in binary form must reproduce the above
//      copyright notice, this list of conditions and the following
//      disclaimer in the documentation and/or other materials
//      provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY Shinichiro Hamaji ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL Shinichiro Hamaji OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
// USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
// OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
// SUCH DAMAGE.

// Emulation for functions in Mac.

#define _GNU_SOURCE
#define _DONT_USE_CTYPE_INLINE_		/* Prevent ctype functions conflict */

#define __mb_cur_max __host_mb_cur_max

#include <sys/param.h>
#include <dirent.h>
#include <err.h>
#include <inttypes.h>
#include <locale.h>
#include <pthread.h>
#include <signal.h>
#include <spawn.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#ifdef __GLIBC__
#include <sys/vfs.h>
#else
#include <sys/mount.h>
#endif
#include <time.h>
#include <unistd.h>

#ifdef __GLIBC__
#define HAVE_STAT64
#define HAVE_STATFS64
#define HAVE_FSEEKO64
#endif

#include <mac-ctype.h>
#include <mac-runetype.h>
#include <uuid/uuid.h>

#ifdef NOLOG
# define LOGF(...) if (0) fprintf(stderr, __VA_ARGS__)
#else
int LIBMAC_LOG;
# define LOGF(...) if (LIBMAC_LOG) fprintf(stderr, __VA_ARGS__)
#endif

typedef __darwin_rune_t rune_t;
typedef int __darwin_ct_rune_t;     /* ct_rune_t */

#define _INVALID_RUNE _MacDefaultRuneLocale.__invalid_rune

#include "errno.c"
#include "none.c"
#include "popcountdi2.c"
#include "runetable.c"
#include "stack_protector-obsd.c"

#undef __mb_cur_max

struct __darwin_timespec {
  time_t tv_sec;
  long tv_nsec;
};

typedef int32_t __darwin_dev_t;
typedef uint16_t __darwin_mode_t;
typedef uint16_t __darwin_nlink_t;
typedef uint64_t __darwin_ino64_t;
typedef uint32_t __darwin_uid_t;
typedef uint32_t __darwin_gid_t;
typedef uint32_t __darwin_sigset_t;

// From /usr/include/sys/stat.h
struct __darwin_stat64 {
  __darwin_dev_t st_dev;
  __darwin_mode_t st_mode;
  __darwin_nlink_t st_nlink;
  __darwin_ino64_t st_ino;
  __darwin_uid_t st_uid;
  __darwin_gid_t st_gid;
  __darwin_dev_t st_rdev;
  struct __darwin_timespec st_atimespec;
  struct __darwin_timespec st_mtimespec;
  struct __darwin_timespec st_ctimespec;
  struct __darwin_timespec st_birthtimespec;
  off_t st_size;
  // TODO(hamaji): the size is not checked after this field.
  blkcnt_t st_blocks;
  blksize_t st_blksize;
  __uint32_t st_flags;
  __uint32_t st_gen;
  __int32_t st_lspare;
  __int64_t st_qspare[2];
};

struct __darwin_stat {
  __darwin_dev_t st_dev;
  uint32_t st_ino;
  __darwin_mode_t st_mode;
  __darwin_nlink_t st_nlink;
  __darwin_uid_t st_uid;
  __darwin_gid_t st_gid;
  __darwin_dev_t st_rdev;
  struct __darwin_timespec st_atimespec;
  struct __darwin_timespec st_mtimespec;
  struct __darwin_timespec st_ctimespec;
  off_t st_size;
  // TODO(hamaji): the size is not checked after this field.
  blkcnt_t st_blocks;
  blksize_t st_blksize;
  __uint32_t st_flags;
  __uint32_t st_gen;
  __int32_t st_lspare;
  __int64_t st_qspare[2];
};

#ifndef HAVE_STAT64
#define stat64 stat
#define lstat64 lstat
#define fstat64 fstat
#endif

static void __translate_stat64(struct stat64* linux_buf,
                               struct __darwin_stat64* mac) {
  // TODO(hamaji): this memset seems to cause overflow... why?
  //memset(mac, 0, sizeof(*mac));
  mac->st_dev = linux_buf->st_dev;
  mac->st_mode = linux_buf->st_mode;
  mac->st_nlink = linux_buf->st_nlink;
  mac->st_ino = linux_buf->st_ino;
  mac->st_uid = linux_buf->st_uid;
  mac->st_gid = linux_buf->st_gid;
  mac->st_rdev = linux_buf->st_rdev;
  mac->st_size = linux_buf->st_size;
  mac->st_blksize = linux_buf->st_blksize;
  mac->st_blocks = linux_buf->st_blocks;
  mac->st_atimespec.tv_sec = linux_buf->st_atime;
  mac->st_mtimespec.tv_sec = linux_buf->st_mtime;
  mac->st_ctimespec.tv_sec = linux_buf->st_ctime;
}

static void __translate_stat(struct stat* host_buf,
                             struct __darwin_stat* mac) {
  // TODO(hamaji): this memset seems to cause overflow... why?
  //memset(mac, 0, sizeof(*mac));
  mac->st_dev = host_buf->st_dev;
  mac->st_mode = host_buf->st_mode;
  mac->st_nlink = host_buf->st_nlink;
  mac->st_ino = host_buf->st_ino;
  mac->st_uid = host_buf->st_uid;
  mac->st_gid = host_buf->st_gid;
  mac->st_rdev = host_buf->st_rdev;
  mac->st_size = host_buf->st_size;
  mac->st_blksize = host_buf->st_blksize;
  mac->st_blocks = host_buf->st_blocks;
  mac->st_atimespec.tv_sec = host_buf->st_atime;
  mac->st_mtimespec.tv_sec = host_buf->st_mtime;
  mac->st_ctimespec.tv_sec = host_buf->st_ctime;
}

static const char kSysname[] = "Darwin";
static const char kRelease[] = "15.6.0";
static const char kVersion[] = "Darwin Kernel Version 15.6.0: "
    "Mon Aug 29 20:21:34 PDT 2016; root:xnu-3248.60.11~1/RELEASE_X86_64";
static const char kMachine[] = "x86_64";

int __darwin_stat64(const char* path, struct __darwin_stat64* mac) {
  LOGF("stat64: path=%s buf=%p\n", path, mac);
  struct stat64 linux_buf;
  int ret = stat64(path, &linux_buf);
  __translate_stat64(&linux_buf, mac);
  return ret;
}

int __darwin_fstat64(int fd, struct __darwin_stat64* mac) {
  LOGF("fstat64: fd=%d buf=%p\n", fd, mac);
  LOGF("fstat: size_offset=%d\n", (int)((char*)&mac->st_size - (char*)mac));
  struct stat64 linux_buf;
  int ret = fstat64(fd, &linux_buf);
  __translate_stat64(&linux_buf, mac);
  return ret;
}

int __darwin_lstat64(const char* path, struct __darwin_stat64* mac) {
  LOGF("lstat64: path=%s buf=%p\n", path, mac);
  struct stat64 linux_buf;
  int ret = lstat64(path, &linux_buf);
  __translate_stat64(&linux_buf, mac);
  return ret;
}

int __darwin_stat(const char* path, struct __darwin_stat* mac) {
  LOGF("stat: path=%s buf=%p\n", path, mac);
  struct stat host_buf;
  int ret = stat(path, &host_buf);
  __translate_stat(&host_buf, mac);
  return ret;
}

int __darwin_fstat(int fd, struct __darwin_stat* mac) {
  LOGF("fstat: fd=%d buf=%p\n", fd, mac);
  LOGF("fstat: size_offset=%d\n", (int)((char*)&mac->st_size - (char*)mac));
  struct stat host_buf;
  int ret = fstat(fd, &host_buf);
  __translate_stat(&host_buf, mac);
  return ret;
}

int __darwin_lstat(const char* path, struct __darwin_stat* mac) {
  LOGF("lstat: path=%s buf=%p\n", path, mac);
  struct stat host_buf;
  int ret = lstat(path, &host_buf);
  __translate_stat(&host_buf, mac);
  return ret;
}

// From /usr/include/sys/dirent.h
#define __DARWIN_MAXPATHLEN 1024
struct __darwin_dirent64 {
  uint64_t d_ino;
  uint64_t d_seekoff;
  uint16_t d_reclen;
  uint16_t d_namlen;
  uint8_t d_type;
  char d_name[__DARWIN_MAXPATHLEN];
};

#define __DARWIN_MAXNAMLEN 255
struct __darwin_dirent {
  uint32_t d_ino;
  uint16_t d_reclen;
  uint8_t d_type;
  uint8_t d_namlen;
  char d_name[__DARWIN_MAXNAMLEN + 1];
};

struct __darwin_dirent64* __darwin_readdir64(DIR* dirp) {
  static struct __darwin_dirent64 mac;
  struct dirent* linux_buf = readdir(dirp);
  if (!linux_buf) {
    return NULL;
  }
  mac.d_ino = linux_buf->d_ino;
  mac.d_reclen = linux_buf->d_reclen;
  mac.d_type = linux_buf->d_type;
  mac.d_namlen = strlen(linux_buf->d_name);
  // TODO(hamaji): I hope this won't be used.
  mac.d_seekoff = 0;
  strcpy(mac.d_name, linux_buf->d_name);
  LOGF("readdir64: %s\n", mac.d_name);
  return &mac;
}

struct __darwin_dirent* __darwin_readdir(DIR* dirp) {
  static struct __darwin_dirent mac;
  struct dirent* linux_buf = readdir(dirp);
  if (!linux_buf) {
    return NULL;
  }
  mac.d_ino = linux_buf->d_ino;
  mac.d_reclen = linux_buf->d_reclen;
  mac.d_type = linux_buf->d_type;
  mac.d_namlen = strlen(linux_buf->d_name);
  strcpy(mac.d_name, linux_buf->d_name);
  LOGF("readdir: %s\n", mac.d_name);
  return &mac;
}

// From /usr/include/sys/mount.h
#define __DARWIN_MFSNAMELEN 15
#define __DARWIN_MFSTYPENAMELEN 16
// assume __DARWIN_64_BIT_INO_T is false.
#define __DARWIN_MNAMELEN 90
struct __darwin_fsid { int32_t val[2]; };

struct __darwin_statfs64 {
  uint32_t f_bsize;
  int32_t  f_iosize;
  uint64_t f_blocks;
  uint64_t f_bfree;
  uint64_t f_bavail;
  uint64_t f_files;
  uint64_t f_ffree;
  struct __darwin_fsid   f_fsid;
  __darwin_uid_t    f_owner;
  uint32_t f_type;
  uint32_t f_flags;
  uint32_t f_fssubtype;
  char     f_fstypename[__DARWIN_MFSTYPENAMELEN];
  char     f_mntonname[__DARWIN_MAXPATHLEN];
  char     f_mntfromname[__DARWIN_MAXPATHLEN];
  uint32_t f_reserved[8];
};

#ifndef HAVE_STATFS64
#define statfs64 statfs
#define fstatfs64 fstatfs
#endif

static void __translate_statfs64(struct statfs64* linux_buf,
                                 struct __darwin_statfs64* mac) {
  memset(mac, 0, sizeof(*mac));
  mac->f_iosize = linux_buf->f_bsize;
  mac->f_blocks = linux_buf->f_blocks;
  mac->f_bfree = linux_buf->f_bfree;
  mac->f_bavail = linux_buf->f_bavail;
  mac->f_files = linux_buf->f_files;
  mac->f_ffree = linux_buf->f_ffree;
  mac->f_type = linux_buf->f_type;

  memcpy(&mac->f_fsid, &linux_buf->f_fsid, sizeof(mac->f_fsid));
  // Followings are just ignored:
  // f_bsize, f_owner, f_flags, f_fssubtype, f_fstypename, f_mntonname,
  // f_mntfromname.
}

int __darwin_statfs64(const char* path, struct __darwin_statfs64* mac) {
  LOGF("statfs64: path=%s buf=%p\n", path, mac);
  struct statfs64 linux_buf;
  int ret = statfs64(path, &linux_buf);
  if (ret == 0)
    __translate_statfs64(&linux_buf, mac);
  return ret;
}

int __darwin_fstatfs64(int fd, struct __darwin_statfs64* mac) {
  LOGF("fstatfs64: fd=%d buf=%p\n", fd, mac);
  struct statfs64 linux_buf;
  int ret = fstatfs64(fd, &linux_buf);
  if (ret == 0)
    __translate_statfs64(&linux_buf, mac);
  return ret;
}

int __maskrune(__darwin_ct_rune_t _c, unsigned long _f) {
  return _MacDefaultRuneLocale.__runetype[_c & 0xff] & _f;
}

int __maskrune_l(__darwin_ct_rune_t _c, unsigned long _f, void* l) {
  return __maskrune(_c, _f);
}

size_t mbstowcs_l(wchar_t* pwcs, const char* s, size_t n, void* l) {
  return mbstowcs(pwcs, s, n);
}

size_t wcswidth_l(wchar_t* pwcs, size_t n, void* l) {
  return wcswidth(pwcs, n);
}

void libiconv_set_relocation_prefix(const char* orig, const char* curr) {
  // TODO: What should we do?
  abort();
}

// TODO: We need rdtsc.
struct mach_timebase_info {
  uint32_t numer;
  uint32_t denom;
};

uint64_t mach_absolute_time() {
  return clock();
}

int mach_timebase_info(struct mach_timebase_info* info) {
  info->numer = 1;
  info->denom = CLOCKS_PER_SEC;
  return 0;
}

void* mach_task_self_;

// From /usr/include/mach/host_info.h
struct __darwin_host_basic_info {
  int max_cpus;
  int avail_cpus;
  int memory_size;
  unsigned int cpu_type;
  unsigned int cpu_subtype;
  unsigned int cpu_threadtype;
  int physical_cpu;
  int physical_cpu_max;
  int logical_cpu;
  int logical_cpu_max;
  uint64_t max_mem;
};

int host_info(int host, int flavor,
              struct __darwin_host_basic_info* info, int* info_cnt) {
  if (flavor != 1) {
    fprintf(stderr, "host_info with flavor=%d isn't supported yet.\n", flavor);
    abort();
  }

  // TODO(hamaji): only supports x86-64.
  memset(info, 0, sizeof(*info));
  info->cpu_type = 16777223;
  info->cpu_subtype = 3;

  *info_cnt = sizeof(*info);

  return 0;
}

struct __darwin_host_load_info {
  int avenrun[3];
  int mach_factor[3];
};

int host_statistics(int host, int flavor,
                    struct __darwin_host_load_info* info, int* info_cnt) {
  // TODO(hamaji): implement this.
  info->avenrun[0] = 0;
  info->avenrun[1] = 0;
  info->avenrun[2] = 0;
  info->mach_factor[0] = 0;
  info->mach_factor[1] = 0;
  info->avenrun[2] = 0;

  *info_cnt = sizeof(*info);

  return 0;
}

char* mach_error_string() {
  //abort();
  //return "";
  return "mach_error_string";
}

void* mach_host_self() {
  //abort();
  return NULL;
}

int mach_port_deallocate() {
  // TODO(hamaji): leak
  //abort();
  return 0;
}

/* FIXME implement vm_function correctly.
 * OznOg Obviosly, all this remain completelly wrong because completely void.
 * This functions allow programs to start correctly and usually to run (almost)
 * correctly, but the memory managment remains wrong. I do not really have good ideas
 * to handle all this without reimplementing the whole memory managment now. Feel free
 * to give me some good ideas.
 * I do not think implementing vm_allocate vm_deallocate and vm_msync is a priority
 * but I guess some programs really need them to work correctly.
 */
int vm_msync(int target_task, void** addr, size_t size, int flags) {
  return 0;
}

int vm_allocate(int target_task, void** addr, size_t size, int flags) {
  *addr = calloc(size, 1);
  return 0;
}

int vm_deallocate() {
  // TODO(hamaji): munmap, maybe
  return 0;
}

void *__darwin_mmap(void *addr, size_t length, int prot, int flags,
                    int fd, off_t offset) {
  LOGF("mmap: addr=%p length=%lu prot=%d flags=%d fd=%d offset=%lu\n",
       addr, (unsigned long)length, prot, flags, fd, (unsigned long int)offset);

  // MAP_ANON is 0x1000 on darwin but 0x20 on linux.
  //
  // The following flags are darwin only.
  // #define MAP_RENAME       0x0020 /* Sun: rename private pages to file */
  // #define MAP_NORESERVE    0x0040 /* Sun: don't reserve needed swap area */
  // #define MAP_RESERVED0080 0x0080 /* previously unimplemented MAP_INHERIT */
  // #define MAP_NOEXTEND     0x0100 /* for MAP_FILE, don't change file size */
  // #define MAP_HASSEMAPHORE 0x0200 /* region may contain semaphores */
  // #define MAP_NOCACHE      0x0400 /* don't cache pages for this mapping */
  flags = (flags & 0x1f) | (flags & 0x1000 ? MAP_ANONYMOUS : 0);
  return mmap(addr, length, prot, flags, fd, offset);
}

// From /usr/include/sys/sysctl.h

#define CTL_KERN 1
#define CTL_HW 6

#define HW_NCPU 3
#define HW_PHYSMEM 5
#define HW_USERMEM 6
#define HW_AVAILCPU 25

#define KERN_OSRELEASE 2

int __darwin_sysctl(int* name, u_int namelen,
                    unsigned int* oldp, size_t* oldlenp,
                    void* newp, size_t newlen) {
  int i;
  LOGF("sysctl: namelen=%u", namelen);
  for (i = 0; i < namelen; i++) {
    LOGF(" name[%d]=%d", i, name[i]);
  }
  LOGF(" newp=%p\n", newp);

  if (newp) {
    fprintf(stderr, "sysctl with newp isn't supported yet.\n");
    abort();
  }

  if (namelen != 2) {
    fprintf(stderr, "sysctl with namelen=%u isn't supported yet.\n", namelen);
    abort();
  }

  switch (name[0]) {
    case CTL_HW: {
      if (*oldlenp != sizeof(unsigned int)) {
        fprintf(stderr,
                "sysctl(HW) with oldlenp=%lu isn't supported yet.\n",
                (unsigned long)*oldlenp);
        abort();
      }

      unsigned int val = 0;
      switch (name[1]) {
        case HW_NCPU:
        case HW_AVAILCPU:
          val = 1;
          break;

        case HW_PHYSMEM:
        case HW_USERMEM:
          val = 2147483648U;
          break;

        default:
          fprintf(stderr, "sysctl(HW) with name[1]=%d isn't supported yet.\n",
                  name[1]);
          abort();
      }

      *oldp = val;
      return 0;
    }

    case CTL_KERN: {
      switch (name[1]) {
        case KERN_OSRELEASE:
          strcpy((char*)oldp, kRelease);
          *oldlenp = sizeof(kRelease);
          break;

        default:
          fprintf(stderr, "sysctl(KERN) with oldp=%u isn't supported yet.\n",
                  *oldp);
          abort();
      }
      return 0;
    }

    default:
      fprintf(stderr,
              "sysctl with name[0]=%d isn't supported yet.\n", name[0]);
      abort();
  }
}

#define __DARWIN_F_GETPATH       50

int __darwin_fcntl(int fildes, int cmd, ...) {
  LOGF("fcntl\n");
  int ret = -1;
  va_list ap;
  va_start(ap, cmd);
  if (cmd == __DARWIN_F_GETPATH) {
    char* buf = va_arg(ap, char *);
    char proc_path[64];
    snprintf(proc_path, sizeof(proc_path), "/proc/self/fd/%d", fildes);
    LOGF("fcntl proc_path=%s\n", proc_path);
    ssize_t cnt = readlink(proc_path, buf, PATH_MAX);
    if (cnt > 0) {
      LOGF("fcntl buf=%s\n", buf);
      buf[cnt] = '\0';
      ret = 0;
    }
  } else {
    fprintf(stderr, "fcntl only supports F_GETPATH\n");
  }
  va_end(ap);
  return ret;
}


/* stdio buffers */
struct __darwin_sbuf {
        unsigned char   *_base;
        int             _size;
};

// Unfortunately, putc_nolock depends on FILE's layout,
// so we need to wrap linux's FILE with darwin's layout.
typedef struct __darwin_sFILE {
  unsigned char *_p;      /* current position in (some) buffer */
  int     _r;             /* read space left for getc() */
  int     _w;             /* write space left for putc() */
  // TODO(hamaji): we need to modify this value with ferror and feof...
  short   _flags;         /* flags, below; this FILE is free if 0 */
  short   _file;          /* fileno, if Unix descriptor, else -1 */
  struct  __darwin_sbuf _bf;     /* the buffer (at least 1 byte, if !NULL) */
  int     _lbfsize;       /* 0 or -_bf._size, for inline putc */

#if 0
  /* operations */
  void    *_cookie;       /* cookie passed to io functions */
  int     (*_close)(void *);
  int     (*_read) (void *, char *, int);
  fpos_t  (*_seek) (void *, fpos_t, int);
  int     (*_write)(void *, const char *, int);

  /* separate buffer for long sequences of ungetc() */
  struct  __sbuf _ub;     /* ungetc buffer */
  struct __sFILEX *_extra; /* additions to FILE to not break ABI */
  int     _ur;            /* saved _r when _r is counting ungetc data */

  /* tricks to meet minimum requirements even when malloc() fails */
  unsigned char _ubuf[3]; /* guarantee an ungetc() buffer */
  unsigned char _nbuf[1]; /* guarantee a getc() buffer */

  /* separate buffer for fgetln() when line crosses buffer boundary */
  struct  __sbuf _lb;     /* buffer for fgetln() */

  /* Unix stdio files get aligned to block boundaries on fseek() */
  int     _blksize;       /* stat.st_blksize (may be != _bf._size) */
  fpos_t  _offset;        /* current lseek offset (see WARNING) */

#endif

  FILE* linux_fp;
} __darwin_FILE;

__darwin_FILE* __darwin_stdin;
__darwin_FILE* __darwin_stdout;
__darwin_FILE* __darwin_stderr;

static __darwin_FILE* __init_darwin_FILE(FILE* linux_fp) {
  if (!linux_fp)
    return NULL;

  __darwin_FILE* fp = (__darwin_FILE*)malloc(sizeof(__darwin_FILE));
  fp->_p = NULL;
  fp->_r = -1;
  fp->_w = -1;
  fp->_flags = 0;
  fp->_file = fileno(linux_fp);
  fp->_lbfsize = 0;
  fp->linux_fp = linux_fp;
  return fp;
}

__darwin_FILE* __darwin_fopen(const char* path, const char* mode) {
  return __init_darwin_FILE(fopen(path, mode));
}

__darwin_FILE* __darwin_fdopen(int fd, const char* mode) {
  return __init_darwin_FILE(fdopen(fd, mode));
}

__darwin_FILE* __darwin_freopen(const char* path, const char* mode,
                                __darwin_FILE* fp) {
  return __init_darwin_FILE(freopen(path, mode, fp->linux_fp));
}

int __darwin_fclose(__darwin_FILE* fp) {
  int r = fclose(fp->linux_fp);
  free(fp);
  return r;
}

size_t __darwin_fread(void* ptr, size_t size, size_t nmemb,
                      __darwin_FILE* fp) {
  return fread(ptr, size, nmemb, fp->linux_fp);
}

size_t __darwin_fwrite(void* ptr, size_t size, size_t nmemb,
                       __darwin_FILE* fp) {
  return fwrite(ptr, size, nmemb, fp->linux_fp);
}

int __darwin_fseek(__darwin_FILE* fp, long offset, int whence) {
  return fseek(fp->linux_fp, offset, whence);
}

int __darwin_fseeko(__darwin_FILE* fp, uint64_t offset, int whence) {
  LOGF("fseeko: %p %" PRIu64 " %d\n", fp, offset, whence);
  return fseeko(fp->linux_fp, offset, whence);
}

#ifndef HAVE_FSEEKO64
#define fseeko64 fseeko
#endif

int __darwin_fseeko64(__darwin_FILE* fp, uint64_t offset, int whence) {
  LOGF("fseeko64: %p %" PRIu64 " %d\n", fp, offset, whence);
  return fseeko64(fp->linux_fp, offset, whence);
}

long __darwin_ftell(__darwin_FILE* fp) {
  return ftell(fp->linux_fp);
}

void __darwin_rewind(__darwin_FILE* fp) {
  return rewind(fp->linux_fp);
}

int __darwin_getc(__darwin_FILE* fp) {
  return getc(fp->linux_fp);
}

int __darwin_fgetc(__darwin_FILE* fp) {
  return fgetc(fp->linux_fp);
}

int __darwin_ungetc(int c, __darwin_FILE* fp) {
  return ungetc(c, fp->linux_fp);
}

char* __darwin_fgets(char* s, int size, __darwin_FILE* fp) {
  return fgets(s, size, fp->linux_fp);
}

int __darwin_putc(int c, __darwin_FILE* fp) {
  return putc(c, fp->linux_fp);
}

int __darwin_fputc(int c, __darwin_FILE* fp) {
  return fputc(c, fp->linux_fp);
}

int __darwin_fputs(const char* s, __darwin_FILE* fp) {
  return fputs(s, fp->linux_fp);
}

int __darwin_fprintf(__darwin_FILE* fp, const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int r = vfprintf(fp->linux_fp, fmt, ap);
  va_end(ap);
  return r;
}

int __darwin_vfscanf(__darwin_FILE* fp, const char* fmt, va_list ap) {
  return vfscanf(fp->linux_fp, fmt, ap);
}

int __darwin_fscanf(__darwin_FILE* fp, const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int r = vfscanf(fp->linux_fp, fmt, ap);
  va_end(ap);
  return r;
}

int __darwin_vfprintf(__darwin_FILE* fp, const char* fmt, va_list ap) {
  return vfprintf(fp->linux_fp, fmt, ap);
}

int __darwin_fflush(__darwin_FILE* fp) {
  if (!fp)
    return fflush(NULL);
  return fflush(fp->linux_fp);
}

void __darwin_setbuf(__darwin_FILE* fp, char* buf) {
  setbuf(fp->linux_fp, buf);
}

void __darwin_setbuffer(__darwin_FILE* fp, char* buf, size_t size) {
  setbuffer(fp->linux_fp, buf, size);
}

int __darwin_feof(__darwin_FILE* fp) {
  LOGF("feof: %d\n", feof(fp->linux_fp));
  return feof(fp->linux_fp);
}

int __darwin_ferror(__darwin_FILE* fp) {
  LOGF("ferror: %d\n", ferror(fp->linux_fp));
  return ferror(fp->linux_fp);
}

int __darwin_fileno(__darwin_FILE* fp) {
  return fileno(fp->linux_fp);
}

__darwin_FILE* __darwin_tmpfile() {
  return __init_darwin_FILE(tmpfile());
}

static __thread char* g_fgetln_buf;
char* __darwin_fgetln(__darwin_FILE* fp, size_t* len) {
  free(g_fgetln_buf);
  g_fgetln_buf = malloc(4096);
  if (fgets(g_fgetln_buf, 4096, fp->linux_fp) == NULL) {
    return NULL;
  }
  *len = strlen(g_fgetln_buf);
  if (*len >= 4095) {
    fprintf(stderr, "Insufficient buffer size in fgetln\n");
    abort();
  }
  return g_fgetln_buf;
}

char __darwin_executable_path[PATH_MAX];
char __loader_path[PATH_MAX];

int _NSGetExecutablePath(char* buf, unsigned int* size) {
  strcpy(buf, __darwin_executable_path);
  *size = strlen(__darwin_executable_path);
  return 0;
}

int __darwin_open(const char* path, int flags, mode_t mode) {
  LOGF("open path=%s flags=%d\n", path, flags);
  int host_flags = 0;
  host_flags |= flags & O_ACCMODE;
  // O_SHLOCK
  if (flags & 0x10) {
    fprintf(stderr, "Unsupported open flag=%d\n", flags);
    abort();
  }
  // O_EXLOCK
  if (flags & 0x20) {
    fprintf(stderr, "Unsupported open flag=%d\n", flags);
    abort();
  }
  if (flags & 0x40) host_flags |= O_ASYNC;
  if (flags & 0x80) host_flags |= O_SYNC;
  if (flags & 0x100) host_flags |= O_NOFOLLOW;
  if (flags & 0x200) host_flags |= O_CREAT;
  if (flags & 0x400) host_flags |= O_TRUNC;
  if (flags & 0x800) host_flags |= O_EXCL;
  // O_EVTONLY
  if (flags & 0x8000) {
    fprintf(stderr, "Unsupported open flag=%d\n", flags);
    abort();
  }
  if (flags & 0x20000) host_flags |= O_NOCTTY;
  if (flags & 0x100000) host_flags |= O_DIRECTORY;
  // O_SYMLINK
  if (flags & 0x200000) {
    fprintf(stderr, "Unsupported open flag=%d\n", flags);
    abort();
  }
#ifdef O_DSYNC
  if (flags & 0x400000) host_flags |= O_DSYNC;
#endif
  // O_POPUP
  if (flags & 0x80000000) {
    fprintf(stderr, "Unsupported open flag=%d\n", flags);
    abort();
  }
  // O_ALERT
  if (flags & 0x20000000) {
    fprintf(stderr, "Unsupported open flag=%d\n", flags);
    abort();
  }

  return open(path, host_flags, mode);
}

static char** add_loader_to_argv(char* argv[]) {
  int i, argc;
  for (argc = 0; argv[argc]; argc++);
  LOGF("\n");
  char** new_argv = malloc(sizeof(char*) * (argc + 2));
  new_argv[0] = __loader_path;
  for (i = 0; i < argc + 1; i++) {
    new_argv[i + 1] = argv[i];
  }
  return new_argv;
}

int __darwin_execv(const char* path, char* argv[]) {
  char** new_argv = add_loader_to_argv(argv);
  LOGF("execv: path=%s\n", path);
  int i;
  for (i = 0; new_argv[i]; i++) {
    LOGF("%s ", new_argv[i]);
  }
  LOGF("\n");

  return execvp(__loader_path, new_argv);
}

// Finds executable "file" in path. Returns path name to execute.
// It leaks memory, but we don't care because it will be released
// in exec(3) call.
static char* find_in_path(const char* file) {
  size_t len = strlen(file) + 1;
  if (*file == '/') {
    char* name = malloc(len + 1);
    strcpy(name, file);
    return name;
  }
  char* path = getenv("PATH");
  if (path == NULL) {
    // Get default path.
    size_t len = confstr(_CS_PATH, (char*)NULL, 0);
    path = (char*)alloca(len + 1);
    path[0] = ':';
    (void)confstr(_CS_PATH, path + 1, len);
  }

  size_t pathlen = strlen(path);

  char* name = malloc(pathlen + len + 1);
  // Puts "file" at the end of "name".
  name = (char*)memcpy(name + pathlen + 1, file, len);
  // "name" points '/file".
  *--name = '/';

  char* p = path;  // next path to try.
  do {
    char* startp;
    path = p;
    p = strchrnul(path, ':');
    if (p == path) {
      // empty path, so it means current directory.
      // so try relative path (== file).
      startp = name + 1;
    } else {
      // add path before "/file".
      startp = (char*)memcpy(name - (p - path), path, p - path);
    }
    // Otherwise, use startp as relative path.
    int r = access(startp, X_OK);
    if (r == 0) {
      // If it is X_OK, assume we can execute it.
      return startp;
    }
    // Otherwise, try another path.
  } while (*p++ != '\0');
  return NULL;
}

int __darwin_execvp(const char* file, char* argv[]) {
  // TODO(hamaji): Incorrect.
  if (access(file, X_OK) == 0) {
    return __darwin_execv(file, argv);
  } else {
    char* path = find_in_path(file);
    LOGF("execvp: file=%s path=%s\n", file, path);
    if (!path) {
      // This should fail.
      return execvp(file, argv);
    }

    argv[0] = path;
    return __darwin_execv(path, argv);
  }
}

int __darwin_execl(const char* path, ...) {
  err(1, "execl is not implemented yet\n");
  return 0;
}

int __darwin_execlp(const char* file, const char* arg, ...) {
  err(1, "execlp is not implemented yet\n");
  return 0;
}

int __darwin_execve(const char* file, const char** argv, const char** envp) {
  err(1, "execve is not implemented yet\n");
  return 0;
}

int __darwin_execle(const char* file, const char* arg, ...) {
  err(1, "execle is not implemented yet\n");
  return 0;
}

// It seems the size of posix_spawn_file_actions_t of Mac is smaller
// than Linux so modifying posix_spawn_file_actions_t allocated in
// Mac's stack would cause stack overflow. Let's wrap it.

int __darwin_posix_spawn(pid_t* pid,
                         const char* path,
                         const posix_spawn_file_actions_t** file_actions,
                         const posix_spawnattr_t* attrp,
                         char* argv[],
                         char* const envp[]) {
  char** new_argv = add_loader_to_argv(argv);
  const posix_spawn_file_actions_t* fa = NULL;
  if (file_actions)
    fa = *file_actions;
  int r = posix_spawn(pid,
                      __loader_path,
                      fa,
                      attrp,
                      new_argv,
                      envp);
  free(new_argv);
  return r;
}

int __darwin_posix_spawnp(pid_t *pid,
                          const char* file,
                          const posix_spawn_file_actions_t* file_actions,
                          const posix_spawnattr_t* attrp,
                          char* const argv[],
                          char* const envp[]) {
  err(1, "posix_spawnp is not implemented yet\n");
  return 0;
}

int __darwin_posix_spawn_file_actions_init(posix_spawn_file_actions_t** p) {
  *p = (posix_spawn_file_actions_t*)malloc(sizeof(posix_spawn_file_actions_t));
  return posix_spawn_file_actions_init(*p);
}

int __darwin_posix_spawn_file_actions_destroy(posix_spawn_file_actions_t** p) {
  int r = posix_spawn_file_actions_destroy(*p);
  free(*p);
  return r;
}

int __darwin_posix_spawn_file_actions_addopen(
    posix_spawn_file_actions_t** file_actions,
    int fd,
    const char* path,
    int oflag,
    int mode) {
    return posix_spawn_file_actions_addopen(
        *file_actions, fd, path, oflag, mode);
}

int __darwin_posix_spawn_file_actions_addclose(
    posix_spawn_file_actions_t** file_actions,
    int fd) {
    return posix_spawn_file_actions_addclose(*file_actions, fd);
}

int __darwin_posix_spawn_file_actions_adddup2(
    posix_spawn_file_actions_t** file_actions,
    int fd,
    int newfd) {
    return posix_spawn_file_actions_adddup2(*file_actions, fd, newfd);
}

typedef struct {
  const char *name;
  unsigned int cputype;
  unsigned int cpusubtype;
  int byteorder;
  const char *description;
} NXArchInfo;

// This was dumped on El Capitan
NXArchInfo __darwin_all_arch_infos[] = {
  // The first entry indicates the local arch.
  { "hppa", 11, 0, 2, "HP-PA" },
  { "i386", 7, 3, 1, "Intel 80x86" },
  { "x86_64", 16777223, 3, 1, "Intel x86-64" },
  { "x86_64h", 16777223, 8, 1, "Intel x86-64h Haswell" },
  { "i860", 15, 0, 2, "Intel 860" },
  { "m68k", 6, 1, 2, "Motorola 68K" },
  { "m88k", 13, 0, 2, "Motorola 88K" },
  { "ppc", 18, 0, 2, "PowerPC" },
  { "ppc64", 16777234, 0, 2, "PowerPC 64-bit" },
  { "sparc", 14, 0, 2, "SPARC" },
  { "arm", 12, 0, 1, "ARM" },
  { "arm64", 16777228, 0, 1, "ARM64" },
  { "any", -1, -1, 0, "Architecture Independent" },
  { "veo", 255, 2, 2, "veo" },
  { "hppa7100LC", 11, 1, 2, "HP-PA 7100LC" },
  { "m68030", 6, 3, 2, "Motorola 68030" },
  { "m68040", 6, 2, 2, "Motorola 68040" },
  { "i486", 7, 4, 1, "Intel 80486" },
  { "i486SX", 7, 132, 1, "Intel 80486SX" },
  { "pentium", 7, 5, 1, "Intel Pentium" },
  { "i586", 7, 5, 1, "Intel 80586" },
  { "pentpro", 7, 22, 1, "Intel Pentium Pro" },
  { "i686", 7, 22, 1, "Intel Pentium Pro" },
  { "pentIIm3", 7, 54, 1, "Intel Pentium II Model 3" },
  { "pentIIm5", 7, 86, 1, "Intel Pentium II Model 5" },
  { "pentium4", 7, 10, 1, "Intel Pentium 4" },
  { "x86_64h", 7, 8, 1, "Intel x86-64h Haswell" },
  { "ppc601", 18, 1, 2, "PowerPC 601" },
  { "ppc603", 18, 3, 2, "PowerPC 603" },
  { "ppc603e", 18, 4, 2, "PowerPC 603e" },
  { "ppc603ev", 18, 5, 2, "PowerPC 603ev" },
  { "ppc604", 18, 6, 2, "PowerPC 604" },
  { "ppc604e", 18, 7, 2, "PowerPC 604e" },
  { "ppc750", 18, 9, 2, "PowerPC 750" },
  { "ppc7400", 18, 10, 2, "PowerPC 7400" },
  { "ppc7450", 18, 11, 2, "PowerPC 7450" },
  { "ppc970", 18, 100, 2, "PowerPC 970" },
  { "ppc970-64", 16777234, 100, 2, "PowerPC 970 64-bit" },
  { "armv4t", 12, 5, 1, "arm v4t" },
  { "armv5", 12, 7, 1, "arm v5" },
  { "xscale", 12, 8, 1, "arm xscale" },
  { "armv6", 12, 6, 1, "arm v6" },
  { "armv6m", 12, 14, 1, "arm v6m" },
  { "armv7", 12, 9, 1, "arm v7" },
  { "armv7f", 12, 10, 1, "arm v7f" },
  { "armv7s", 12, 11, 1, "arm v7s" },
  { "armv7k", 12, 12, 1, "arm v7k" },
  { "armv7m", 12, 15, 1, "arm v7m" },
  { "armv7em", 12, 16, 1, "arm v7em" },
  { "armv8", 12, 13, 1, "arm v8" },
  { "arm64", 16777228, 1, 1, "arm64 v8" },
  { "little", -1, 0, 1, "Little Endian" },
  { "big", -1, 1, 2, "Big Endian" },
  { "veo1", 255, 1, 2, "veo 1" },
  { "veo2", 255, 2, 2, "veo 2" },
  { NULL, 0, 0, 0, NULL },
};

NXArchInfo __darwin_local_arch_info = { "i486", 7, 4, 1, "Intel 80486" };

const NXArchInfo* NXGetAllArchInfos() {
  return __darwin_all_arch_infos;
}

const NXArchInfo* NXGetLocalArchInfo() {
  return &__darwin_local_arch_info;
}

const NXArchInfo* NXGetArchInfoFromName(const char *name) {
  NXArchInfo* info = __darwin_all_arch_infos;
  for (; info->name; info++) {
    if (!strcmp(info->name, name))
      return info;
  }
  return NULL;
}

void __assert_rtn(const char* func, const char* file, int line,
                  const char* failedexpr) {
  fprintf(stderr,
          "Assertion failed: (%s), function %s, file %s, line %d.\n",
          failedexpr, func, file, line);
  abort();
}

// We don't have DWARF to handle exception. Let's show some info and abort.
// This works well for apple's ld.
void __darwin___cxa_throw(char** obj) {
  fprintf(stderr, "__cxa_throw: obj=%p\n", obj);
  fprintf(stderr, "%s\n", *obj);
  abort();
}

size_t strlcpy(char* dst, const char* src, size_t size) {
  LOGF("strlcpy: dst=%p src=%p size=%zu\n", dst, src, size);
  size_t src_size = strlen(src) + 1;  // +1 for '\0'
  LOGF("strlcpy: len(src)=%zu\n", src_size);
  if (size != 0) {
    // We don't use strncpy because strncpy is slow if |size| is
    // much larger than strlen(|src|) e.g. 16K bytes vs 64 bytes.
    //
    // The reason of this is explained by glibc's man:
    //
    // If the length of src is less than n, strncpy() writes additional
    // null bytes to dest to ensure that a total of n bytes are written.
    //
    // Also, we already know the length of |src|, we can use memcpy.
    if (src_size < size)
      size = src_size;
    dst[size - 1] = '\0';
    memcpy(dst, src, size - 1);
  }
  // According to the strlcpy(3) manual, the length of |src| is always returned.
  // https://developer.apple.com/library/mac/#documentation/darwin/reference/manpages/man3/strlcpy.3.html
  return src_size - 1;
}

size_t strlcat(char* dst, const char* src, size_t size) {
  dst[size - 1] = '\0';
  strncat(dst, src, size - strlen(dst) - 1);
  return strlen(dst);
}

// TODO(hamaji): The following three functions are slow.

void memset_pattern4(char* b, const char* pattern4, size_t len) {
  size_t i;
  for (i = 0; i < len; i++) {
    b[i] = pattern4[i % 4];
  }
}

void memset_pattern8(char* b, const char* pattern4, size_t len) {
  size_t i;
  for (i = 0; i < len; i++) {
    b[i] = pattern4[i % 8];
  }
}

void memset_pattern16(char* b, const char* pattern4, size_t len) {
  size_t i;
  for (i = 0; i < len; i++) {
    b[i] = pattern4[i % 16];
  }
}

int __mb_cur_max() {
  // TODO(hamaji): Incorrect for most locales.
  return 1;
}

typedef struct {
  void* ss_sp;
  size_t ss_size;
  int ss_flags;
} __darwin_stack_t;

int __darwin_sigaltstack(const __darwin_stack_t* ss, __darwin_stack_t* oss) {
  return 0;

#if 0
  stack_t linux_ss;
  stack_t linux_oss;
  linux_ss.ss_sp = ss->ss_sp;
  linux_ss.ss_size = ss->ss_size;
  linux_ss.ss_flags = ss->ss_flags;
  if (oss) {
    linux_oss.ss_sp = oss->ss_sp;
    linux_oss.ss_size = oss->ss_size;
    linux_oss.ss_flags = oss->ss_flags;
  }
  int r = sigaltstack(&linux_ss, &linux_oss);
  if (oss) {
    oss->ss_sp = linux_oss.ss_sp;
    oss->ss_size = linux_oss.ss_size;
    oss->ss_flags = linux_oss.ss_flags;
  }
  return r;
#endif
}

int __darwin_sigaction() {
  return 0;
}

void* __darwin_signal() {
  return NULL;
}

int __darwin_sigprocmask(
    int how, const __darwin_sigset_t *set, __darwin_sigset_t *oldset) {
  // TODO: Translate parameters before and after calling sigprocmask.
  return 0;
}

int __darwin_pthread_sigmask(
    int how, const __darwin_sigset_t *set, __darwin_sigset_t *oldset) {
  // TODO: Translate parameters before and after calling sigmask.
  return 0;
}

int __darwin_sigfillset(__darwin_sigset_t *set) {
  *(uint32_t*)(set) = ~0U;
  return 0;
}

typedef struct malloc_statistics_t {
  unsigned blocks_in_use;
  size_t size_in_use;
  size_t max_size_in_use;
  size_t size_allocated;
} __darwin_malloc_statistics_t;

void* malloc_default_zone() {
  return NULL;
}

void malloc_zone_statistics(void* zone, __darwin_malloc_statistics_t* stats) {
  fprintf(stderr, "malloc_zone_statistics\n");
  memset(stats, 0, sizeof(*stats));
}

int task_get_exception_ports() {
  fprintf(stderr, "task_get_exception_ports\n");
  return 0;
}

int task_set_exception_ports() {
  fprintf(stderr, "task_set_exception_ports\n");
  return 0;
}

char*** _NSGetEnviron() {
#ifndef __GLIBC__
  extern char **environ;
#endif
  return &environ;
}

#define __DARWIN_PTHREAD_MUTEX_NORMAL 0
#define __DARWIN_PTHREAD_MUTEX_ERRORCHECK 1
#define __DARWIN_PTHREAD_MUTEX_RECURSIVE 2
int __darwin_pthread_mutexattr_settype(pthread_mutexattr_t* attr, int kind) {
  switch (kind) {
  case __DARWIN_PTHREAD_MUTEX_NORMAL:
#ifdef __GLIBC__
    kind = PTHREAD_MUTEX_FAST_NP;
#else
    kind = PTHREAD_MUTEX_NORMAL;
#endif
    break;
  case __DARWIN_PTHREAD_MUTEX_ERRORCHECK:
#ifdef __GLIBC__
    kind = PTHREAD_MUTEX_ERRORCHECK_NP;
#else
    kind = PTHREAD_MUTEX_ERRORCHECK;
#endif
    break;
  case __DARWIN_PTHREAD_MUTEX_RECURSIVE:
#ifdef __GLIBC__
    kind = PTHREAD_MUTEX_RECURSIVE_NP;
#else
    kind = PTHREAD_MUTEX_RECURSIVE;
#endif
    break;
  default:
    fprintf(stderr, "Unknown pthread_mutexattr_settype kind: %d\n", kind);
  }
  return pthread_mutexattr_settype(attr, kind);
}

#define __DARWIN_PTHREAD_PROCESS_SHARED 1
#define __DARWIN_PTHREAD_PROCESS_PRIVATE 2
static int __translate_pshared(int pshared) {
  switch (pshared) {
  case __DARWIN_PTHREAD_PROCESS_SHARED:
    return PTHREAD_PROCESS_SHARED;
  case __DARWIN_PTHREAD_PROCESS_PRIVATE:
    return PTHREAD_PROCESS_PRIVATE;
  default:
    fprintf(stderr, "Unknown pthread_mutexattr_setpshared pshared: %d\n",
            pshared);
    return pshared;
  }
}
int __darwin_pthread_mutexattr_setpshared(pthread_mutexattr_t* attr,
                                          int pshared) {
  pshared = __translate_pshared(pshared);
  return pthread_mutexattr_setpshared(attr, pshared);
}
int __darwin_pthread_rwlockattr_setpshared(pthread_rwlockattr_t* attr,
                                           int pshared) {
  pshared = __translate_pshared(pshared);
  return pthread_rwlockattr_setpshared(attr, pshared);
}

void* mach_init_routine;
void* _cthread_init_routine;
static void do_nothing() {
}

void __keymgr_dwarf2_register_sections() {
}

void _ZNSt13basic_filebufIcSt11char_traitsIcEE7seekoffExSt12_Ios_SeekdirSt13_Ios_Openmode() {
  fprintf(stderr, "_ZNSt13basic_filebufIcSt11char_traitsIcEE7seekoffExSt12_Ios_SeekdirSt13_Ios_Openmode called\n");
  abort();
}

void _ZNSi5seekgExSt12_Ios_Seekdir() {
  fprintf(stderr, "_ZNSi5seekgExSt12_Ios_Seekdir called\n");
  abort();
}

void* (*ld_mac_dlopen)(const char* filename, int flag);
int (*ld_mac_dlclose)(void* handle);
char* (*ld_mac_dlerror)(void);
void* (*ld_mac_dlsym)(void* handle, const char* symbol);

void* __darwin_dlopen(const char* filename, int flag) {
  return ld_mac_dlopen(filename, flag);
}

int __darwin_dlclose(void* handle) {
  return ld_mac_dlclose(handle);
}

char* __darwin_dlerror(void) {
  return ld_mac_dlerror();
}

void* __darwin_dlsym(void* handle, const char* symbol) {
  return ld_mac_dlsym(handle, symbol);
}

#define __DARWIN_SYS_NAMELEN 256
typedef struct {
  char sysname[__DARWIN_SYS_NAMELEN];
  char nodename[__DARWIN_SYS_NAMELEN];
  char release[__DARWIN_SYS_NAMELEN];
  char version[__DARWIN_SYS_NAMELEN];
  char machine[__DARWIN_SYS_NAMELEN];
} __darwin_utsname;

int __darwin_uname(__darwin_utsname* buf) {
  struct utsname linux_buf;
  int r = uname(&linux_buf);
  if (r)
    return r;

  // TODO(hamaji): Emulate Snow leopard
  strcpy(buf->sysname, kSysname);
  strcpy(buf->nodename, linux_buf.nodename);
  strcpy(buf->release, kRelease);
  strcpy(buf->version, kVersion);
  strcpy(buf->machine, kMachine);
  return 0;
}

typedef struct {
  int (*compar)(void*, const void*, const void*);
  void* thunk;
} __darwin_qsort_r_context;

#ifdef __GLIBC__
int __darwin_qsort_r_helper(const void* a, const void* b, void* data) {
#else
int __darwin_qsort_r_helper(void *data, const void* a, const void* b) {
#endif
  __darwin_qsort_r_context* ctx = (__darwin_qsort_r_context*)data;
  return ctx->compar(ctx->thunk, a, b);
}

void __darwin_qsort_r(void* base, size_t nel, size_t width, void* thunk,
                      int (*compar)(void*, const void*, const void*)) {
  LOGF("qsort_r: %p %p\n", compar, thunk);
  __darwin_qsort_r_context ctx;
  ctx.compar = compar;
  ctx.thunk = thunk;
#ifdef __GLIBC__
  qsort_r(base, nel, width, &__darwin_qsort_r_helper, &ctx);
#else
  qsort_r(base, nel, width, &ctx, &__darwin_qsort_r_helper);
#endif
}

// uuid_t is unsigned char[16] both on Linux and Mac.
int __darwin_gethostuuid(uuid_t id, const struct timespec *wait) {
  // TODO(mayah): Returns the same uuid for now.
  // It might be better if we can generate uuid per host
  // from mac adderss or something?
  uuid_parse("550e8400-e29b-41d4-a716-446655440000", id);
  return 0;
}

unsigned int arc4random() {
  static int initialized = 0;
  if (!initialized) {
    srand(time(NULL));
    initialized = 1;
  }
  return rand();
}

#define __DARWIN_LC_ALL_MASK            (  __DARWIN_LC_COLLATE_MASK \
                                         | __DARWIN_LC_CTYPE_MASK \
                                         | __DARWIN_LC_MESSAGES_MASK \
                                         | __DARWIN_LC_MONETARY_MASK \
                                         | __DARWIN_LC_NUMERIC_MASK \
                                         | __DARWIN_LC_TIME_MASK )
#define __DARWIN_LC_COLLATE_MASK        (1 << 0)
#define __DARWIN_LC_CTYPE_MASK          (1 << 1)
#define __DARWIN_LC_MESSAGES_MASK       (1 << 2)
#define __DARWIN_LC_MONETARY_MASK       (1 << 3)
#define __DARWIN_LC_NUMERIC_MASK        (1 << 4)
#define __DARWIN_LC_TIME_MASK           (1 << 5)

locale_t __darwin_newlocale(int category_mask, const char* locale,
                            locale_t base) {
  LOGF("newlocale: %d %p %p\n", category_mask, locale, base);
  int linux_category_mask = 0;
  if (__DARWIN_LC_COLLATE_MASK & category_mask)
    linux_category_mask |= LC_COLLATE_MASK;
  if (__DARWIN_LC_CTYPE_MASK & category_mask)
    linux_category_mask |= LC_CTYPE_MASK;
  if (__DARWIN_LC_MESSAGES_MASK & category_mask)
    linux_category_mask |= LC_MESSAGES_MASK;
  if (__DARWIN_LC_MONETARY_MASK & category_mask)
    linux_category_mask |= LC_MONETARY_MASK;
  if (__DARWIN_LC_NUMERIC_MASK & category_mask)
    linux_category_mask |= LC_NUMERIC_MASK;
  if (__DARWIN_LC_TIME_MASK & category_mask)
    linux_category_mask |= LC_TIME_MASK;
  // Apple's newlocale allows base=LC_GLOBAL_LOCALE, while glibc crashes.
  // It seems the behavior is unspecified for this case.
  // http://pubs.opengroup.org/onlinepubs/9699919799/functions/newlocale.html
  // We'll use current locale (NULL) instead of the global locale.
  if (base == LC_GLOBAL_LOCALE)
    base = NULL;
  // It seems the following 5 locales are the same as "C" for Mac.
  if (!strcmp(locale, "en_US") ||
      !strcmp(locale, "en_US.ISO8859-1") ||
      !strcmp(locale, "en_US.ISO8859-15") ||
      !strcmp(locale, "en_US.US-ASCII") ||
      !strcmp(locale, "en_US.UTF-8"))
    locale = "C";
  return newlocale(linux_category_mask, locale, base);
}

int __darwin_compat_mode(const char* function, const char* mode) {
  LOGF("compat_mode: %s %s\n", function, mode);
  // We don't consider bootstrap, legacy, and error.
  // http://opensource.apple.com/source/Libc/Libc-763.13/gen/get_compat.c
  // TODO(hamaji): Support binary operators.
  return !strcasecmp(mode, "unix2003");
}

int32_t OSAtomicAdd32(int32_t theAmount, volatile int32_t *theValue) {
  return __sync_fetch_and_add(theValue, theAmount);
}

int32_t OSAtomicAdd64(int64_t theAmount, volatile int64_t *theValue) {
  return __sync_fetch_and_add(theValue, theAmount);
}

__attribute__((constructor)) void initMac() {
  __darwin_stdin = __init_darwin_FILE(stdin);
  __darwin_stdout = __init_darwin_FILE(stdout);
  __darwin_stderr = __init_darwin_FILE(stderr);
  mach_init_routine = &do_nothing;
  _cthread_init_routine = &do_nothing;
}

#define __DARWIN_PTHREAD_MUTEX_SIG_init              0x32AAABA7
#define __DARWIN_PTHREAD_ERRORCHECK_MUTEX_SIG_init   0x32AAABA1
#define __DARWIN_PTHREAD_RECURSIVE_MUTEX_SIG_init    0x32AAABA2
#define __DARWIN_PTHREAD_FIRSTFIT_MUTEX_SIG_init     0x32AAABA3

#define __DARWIN_PTHREAD_MUTEX_SIZE     56
struct __darwin_pthread_mutex_t {
  long sig;
  char opaque[__DARWIN_PTHREAD_MUTEX_SIZE];
};

#if !defined __GLIBC__ && !defined __linux__ && !defined PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP
#define PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP PTHREAD_MUTEX_INITIALIZER
#endif

int __darwin_pthread_mutex_lock(struct __darwin_pthread_mutex_t *mutex) {
  // convert pthread_mutex_t initialized by Mac's PTHREAD_MUTEX_INITIALIZER to
  // that of Linux.
  if (mutex->sig ==__DARWIN_PTHREAD_MUTEX_SIG_init) {
    pthread_mutex_t expected_value = PTHREAD_MUTEX_INITIALIZER;
    memcpy(mutex, &expected_value, sizeof(expected_value));
  } else if (mutex->sig ==__DARWIN_PTHREAD_ERRORCHECK_MUTEX_SIG_init) {
    pthread_mutex_t expected_value = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;
    memcpy(mutex, &expected_value, sizeof(expected_value));
  } else if (mutex->sig ==__DARWIN_PTHREAD_RECURSIVE_MUTEX_SIG_init) {
    pthread_mutex_t expected_value = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;
    memcpy(mutex, &expected_value, sizeof(expected_value));
  } else if (mutex->sig ==__DARWIN_PTHREAD_FIRSTFIT_MUTEX_SIG_init) {
    pthread_mutex_t expected_value = PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP;
    memcpy(mutex, &expected_value, sizeof(expected_value));
  }
  return pthread_mutex_lock((pthread_mutex_t*)mutex);
}

// Dummy implementation of Block.
void * _NSConcreteStackBlock[32] = { 0 };
void *_Block_copy(const void *arg) {
  // ugly but seems to work.
  return (void*)(arg);
}
void _Block_release(const void *arg) {
}

// Dummy implementation of __cxa_demangle.
char* __cxa_demangle(const char* mangled_name, char* output_buffer,
                     size_t* length, int* status) {
  return NULL;  // Always fails.
}

// Dummy implementation of kdebug_trace.
int kdebug_trace(uint32_t debugid, uint64_t arg1, uint64_t arg2, uint64_t arg3,
                 uint64_t arg4) {
  return 0;
}

// Dummy implementation of kdebug_trace_string.
uint64_t kdebug_trace_string(uint32_t debugid, uint64_t str_id,
                             const char *str) {
  return 0;
}

void __darwin__ZNSt3__15mutex4lockEv(struct __darwin_pthread_mutex_t *t) {
  LOGF("std::mutex::lock: sig=%lx", t->sig);
  __darwin_pthread_mutex_lock(t);
}
