/*
 * syscall_map.h
 * ChrysalisOS Linux Compatibility - Syscall Mapping
 * 
 * Maps Chrysalis syscalls to Linux syscalls
 */

#pragma once

#include "types_linux.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

/* ============================================================================
   MEMORY MANAGEMENT - Map to Linux mmap/malloc
   ============================================================================ */

/* Chrysalis-style memory allocation maps to Linux malloc */
#define sys_kmalloc(size)            malloc(size)
#define sys_kfree(ptr)               free(ptr)
#define sys_krealloc(ptr, size)      realloc(ptr, size)

/* Chrysalis-style page allocation maps to Linux mmap */
#define sys_alloc_page()             mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)
#define sys_free_page(addr)          munmap(addr, 4096)

/* ============================================================================
   FILE SYSTEM - Map to Linux open/read/write/close
   ============================================================================ */

/* File operations */
#define sys_open(path, flags)        open(path, flags, 0644)
#define sys_close(fd)                close(fd)
#define sys_read(fd, buf, size)      read(fd, buf, size)
#define sys_write(fd, buf, size)     write(fd, buf, size)
#define sys_seek(fd, offset, whence) lseek(fd, offset, whence)
#define sys_stat(path, stat)         stat(path, (struct stat*)stat)
#define sys_fstat(fd, stat)          fstat(fd, (struct stat*)stat)
#define sys_mkdir(path, mode)        mkdir(path, mode)
#define sys_rmdir(path)              rmdir(path)
#define sys_remove(path)             unlink(path)
#define sys_rename(old, new)         rename(old, new)

/* Directory operations */
#define sys_opendir(path)            opendir(path)
#define sys_closedir(dir)            closedir(dir)
#define sys_readdir(dir)             readdir(dir)

/* ============================================================================
   PROCESS MANAGEMENT - Map to Linux fork/exec/wait
   ============================================================================ */

/* Process operations */
#define sys_fork()                   fork()
#define sys_exit(code)               exit(code)
#define sys_wait(status)             wait((int*)status)
#define sys_waitpid(pid, status)     waitpid(pid, (int*)status, 0)
#define sys_exec(path, args)         execv(path, (char*const*)args)
#define sys_getpid()                 getpid()
#define sys_getppid()                getppid()
#define sys_kill(pid, signal)        kill(pid, signal)
#define sys_usleep(us)               usleep(us)
#define sys_sleep(sec)               sleep(sec)

/* ============================================================================
   TERMINAL/CONSOLE - Map to Linux stdout/ioctl
   ============================================================================ */

/* Console operations */
#define sys_putchar(c)               putchar(c)
#define sys_printf                   printf
#define sys_puts(s)                  puts(s)

/* ============================================================================
   DEVICE/HARDWARE - Map to Linux /dev files
   ============================================================================ */

/* Common device operations */
#define sys_open_device(name)        open("/dev/" name, O_RDWR)
#define sys_ioctl(fd, cmd, arg)      ioctl(fd, cmd, arg)

/* Framebuffer device */
#define SYS_FB_DEVICE                "/dev/fb0"
#define sys_open_framebuffer()       open(SYS_FB_DEVICE, O_RDWR)

/* Input device */
#define SYS_INPUT_DEVICE             "/dev/input/mice"
#define sys_open_input()             open(SYS_INPUT_DEVICE, O_RDONLY)

/* ============================================================================
   SIGNAL HANDLING - Map to Linux signals
   ============================================================================ */

/* Signal operations */
#define sys_signal(sig, handler)     signal(sig, handler)
#define sys_sigaction(sig, act)      sigaction(sig, act, NULL)

/* ============================================================================
   TIME - Map to Linux time functions
   ============================================================================ */

/* Time operations */
#define sys_time(t)                  time(t)
#define sys_gettimeofday(tv)         gettimeofday((struct timeval*)tv, NULL)
#define sys_getrlimit(resource, rlim) getrlimit(resource, (struct rlimit*)rlim)

/* ============================================================================
   THREAD/PTHREAD - Map to Linux pthreads (if needed)
   ============================================================================ */

#ifdef USE_PTHREADS
#include <pthread.h>

#define sys_thread_create(tid, start, arg)  pthread_create((pthread_t*)tid, NULL, start, arg)
#define sys_thread_join(tid, retval)        pthread_join(*(pthread_t*)tid, retval)
#define sys_thread_exit(code)               pthread_exit((void*)(intptr_t)code)
#define sys_mutex_init(m)                   pthread_mutex_init((pthread_mutex_t*)m, NULL)
#define sys_mutex_lock(m)                   pthread_mutex_lock((pthread_mutex_t*)m)
#define sys_mutex_unlock(m)                 pthread_mutex_unlock((pthread_mutex_t*)m)

#endif

/* ============================================================================
   ENVIRONMENT - Map to Linux environment variables
   ============================================================================ */

#define sys_getenv(name)             getenv(name)
#define sys_setenv(name, value)      setenv(name, value, 1)

/* ============================================================================
   UTILITY FUNCTIONS
   ============================================================================ */

static inline void sys_panic(const char* msg) {
    fprintf(stderr, "PANIC: %s\n", msg);
    exit(1);
}

static inline void sys_assert(int condition, const char* msg) {
    if (!condition) {
        sys_panic(msg);
    }
}

/* ============================================================================
   COMPATIBILITY: Flags mapping
   ============================================================================ */

/* Open flags */
#define SYS_O_RDONLY    O_RDONLY
#define SYS_O_WRONLY    O_WRONLY
#define SYS_O_RDWR      O_RDWR
#define SYS_O_CREAT     O_CREAT
#define SYS_O_TRUNC     O_TRUNC
#define SYS_O_APPEND    O_APPEND
#define SYS_O_EXCL      O_EXCL

/* Memory protection */
#define SYS_PROT_READ   PROT_READ
#define SYS_PROT_WRITE  PROT_WRITE
#define SYS_PROT_EXEC   PROT_EXEC

/* File mode bits */
#define SYS_S_IRUSR     S_IRUSR  /* Owner read */
#define SYS_S_IWUSR     S_IWUSR  /* Owner write */
#define SYS_S_IXUSR     S_IXUSR  /* Owner execute */
#define SYS_S_IRGRP     S_IRGRP  /* Group read */
#define SYS_S_IWGRP     S_IWGRP  /* Group write */
#define SYS_S_IROTH     S_IROTH  /* Others read */

/* Signal numbers */
#define SYS_SIGKILL     SIGKILL
#define SYS_SIGTERM     SIGTERM
#define SYS_SIGINT      SIGINT
#define SYS_SIGHUP      SIGHUP
