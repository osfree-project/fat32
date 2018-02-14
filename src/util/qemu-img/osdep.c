/*
 * QEMU low level functions
 *
 * Copyright (c) 2003 Fabrice Bellard
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
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#ifdef CONFIG_SOLARIS
#include <sys/types.h>
#include <sys/statvfs.h>
#endif

/* Needed early for CONFIG_BSD etc. */
#include "config-host.h"

#ifdef __OS2__
#define  INCL_BASE
#include <os2.h>
#endif

#ifdef _WIN32
#include <windows.h>
#elif defined(CONFIG_BSD)
#include <stdlib.h>
#else
#include <malloc.h>
#endif

#include "qemu-common.h"
#include "sysemu.h"
//#include "qemu_socket.h"

#if !defined(_POSIX_C_SOURCE) || defined(_WIN32) || defined(__OS2__) || defined(__sun__)
static void *oom_check(void *ptr)
{
    if (ptr == NULL) {
        abort();
    }
    return ptr;
}
#endif

#if defined(_WIN32)
void *qemu_memalign(size_t alignment, size_t size)
{
    if (!size) {
        abort();
    }
    return oom_check(VirtualAlloc(NULL, size, MEM_COMMIT, PAGE_READWRITE));
}

void *qemu_vmalloc(size_t size)
{
    /* FIXME: this is not exactly optimal solution since VirtualAlloc
       has 64Kb granularity, but at least it guarantees us that the
       memory is page aligned. */
    if (!size) {
        abort();
    }
    return oom_check(VirtualAlloc(NULL, size, MEM_COMMIT, PAGE_READWRITE));
}

void qemu_vfree(void *ptr)
{
    VirtualFree(ptr, 0, MEM_RELEASE);
}

#elif defined(__OS2__)
void *qemu_memalign(size_t alignment, size_t size)
{
APIRET rc;
void *ptr;

    if (!size) {
        abort();
    }

    rc = DosAllocMem(&ptr, size, PAG_READ | PAG_WRITE | PAG_COMMIT);
    if (rc)
    {
        abort();
    }

    return oom_check(ptr);
}

void *qemu_vmalloc(size_t size)
{
APIRET rc;
void *ptr;

    if (!size) {
        abort();
    }

    rc = DosAllocMem(&ptr, size, PAG_READ | PAG_WRITE | PAG_COMMIT);
    if (rc)
    {
        abort();
    }

    return oom_check(ptr);
}

void qemu_vfree(void *ptr)
{
    DosFreeMem(ptr);
}

#else

void *qemu_memalign(size_t alignment, size_t size)
{
#if defined(_POSIX_C_SOURCE) && !defined(__sun__)
    int ret;
    void *ptr;
    ret = posix_memalign(&ptr, alignment, size);
    if (ret != 0)
        abort();
    return ptr;
#elif defined(CONFIG_BSD)
    return oom_check(valloc(size));
#else
    return oom_check(memalign(alignment, size));
#endif
}

/* alloc shared memory pages */
void *qemu_vmalloc(size_t size)
{
    return qemu_memalign(getpagesize(), size);
}

void qemu_vfree(void *ptr)
{
    free(ptr);
}

#endif

#ifndef __OS2__

int qemu_create_pidfile(const char *filename)
{
    char buffer[128];
    int len;
#ifndef _WIN32
    int fd;

    fd = qemu_open(filename, O_RDWR | O_CREAT, 0600);
    if (fd == -1)
        return -1;

    if (lockf(fd, F_TLOCK, 0) == -1)
        return -1;

    len = snprintf(buffer, sizeof(buffer), "%ld\n", (long)getpid());
    if (write(fd, buffer, len) != len)
        return -1;
#else
    HANDLE file;
    DWORD flags;
    OVERLAPPED overlap;
    BOOL ret;

    /* Open for writing with no sharing. */
    file = CreateFile(filename, GENERIC_WRITE, 0, NULL,
		      OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    if (file == INVALID_HANDLE_VALUE)
      return -1;

    flags = LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY;
    overlap.hEvent = 0;
    /* Lock 1 byte. */
    ret = LockFileEx(file, flags, 0, 0, 1, &overlap);
    if (ret == 0)
      return -1;

    /* Write PID to file. */
    len = snprintf(buffer, sizeof(buffer), "%ld\n", (long)getpid());
    ret = WriteFileEx(file, (LPCVOID)buffer, (DWORD)len,
		      &overlap, NULL);
    if (ret == 0)
      return -1;
#endif
    return 0;
}

#endif

#ifdef _WIN32

/* Offset between 1/1/1601 and 1/1/1970 in 100 nanosec units */
#define _W32_FT_OFFSET (116444736000000000ULL)

int qemu_gettimeofday(qemu_timeval *tp)
{
  union {
    unsigned long long ns100; /*time since 1 Jan 1601 in 100ns units */
    FILETIME ft;
  }  _now;

  if(tp)
    {
      GetSystemTimeAsFileTime (&_now.ft);
      tp->tv_usec=(long)((_now.ns100 / 10ULL) % 1000000ULL );
      tp->tv_sec= (long)((_now.ns100 - _W32_FT_OFFSET) / 10000000ULL);
    }
  /* Always return 0 as per Open Group Base Specifications Issue 6.
     Do not set errno on error.  */
  return 0;
}
#endif /* _WIN32 */

#ifdef __OS2__

int is_leap(int year)
{
  if (year % 400)
  {
    if (year % 4)
      return 0;
    else
      return 1;
  }
  else
    return 0;
}

int qemu_gettimeofday(qemu_timeval *tp)
{
  DATETIME dt;
  unsigned long long ns100 = 0;
  unsigned short leap_years = 0, ordinary_years = 0, year;
  unsigned long days;

  DosGetDateTime(&dt);

  for (year = 1970; year < dt.year; year++)
  {
    if (is_leap(year))
      leap_years++;
    else
      ordinary_years++;
  }

  // days before beginning of the current year
  days = ordinary_years * 365 + leap_years * 366;

  // plus days passed before the current month start
  switch (dt.month)
  {
  case 1: // Jan
    break;

  case 2: // Feb
    days += 31;
    break;

  case 3: // Mar
    days += 31 + 28;
    break;

  case 4: // Apr
    days += 31 + 28 + 31;
    break;

  case 5: // May
    days += 31 + 28 + 31 + 30;
    break;

  case 6: // Jun
    days += 31 + 28 + 31 + 30 + 31;
    break;

  case 7: // Jul
    days += 31 + 28 + 31 + 30 + 31 + 30;
    break;

  case 8: // Aug
    days += 31 + 28 + 31 + 30 + 31 + 30 + 31;
    break;

  case 9: // Sep
    days += 31 + 28 + 31 + 30 + 31 + 30 + 31 + 31;
    break;

  case 10: // Oct
    days += 31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 +
      + 30;
    break;

  case 11: // Nov
    days += 31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 +
      + 30 + 31;
    break;

  case 12: // Dec
    days += 31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 +
      + 30 + 31 + 30;
    break;
  }

  // plus a leap day (if any)
  if (is_leap(dt.year) && dt.month > 2)
    days++;

  // plus days passed since the beginning of the current month
  days += dt.day - 1;

  ns100 += (unsigned long long)days * 24 * 3600 * 10000000;
  ns100 += (unsigned long long)dt.hours * 3600 * 10000000;
  ns100 += (unsigned long long)dt.minutes * 60 * 10000000;
  ns100 += (unsigned long long)dt.seconds * 10000000;
  ns100 += (unsigned long long)dt.hundredths * 1000000000;

  if(tp)
    {
      tp->tv_usec = (long)((ns100 / 10ULL) % 1000000ULL );
      tp->tv_sec  = (long)((ns100) / 10000000ULL);
    }
  /* Always return 0 as per Open Group Base Specifications Issue 6.
     Do not set errno on error.  */
  return 0;
}

void qemu_set_cloexec(int fd)
{
}

#else

#ifdef _WIN32
void socket_set_nonblock(int fd)
{
    unsigned long opt = 1;
    ioctlsocket(fd, FIONBIO, &opt);
}

int inet_aton(const char *cp, struct in_addr *ia)
{
    uint32_t addr = inet_addr(cp);
    if (addr == 0xffffffff)
	return 0;
    ia->s_addr = addr;
    return 1;
}

void qemu_set_cloexec(int fd)
{
}

#else

void socket_set_nonblock(int fd)
{
    int f;
    f = fcntl(fd, F_GETFL);
    fcntl(fd, F_SETFL, f | O_NONBLOCK);
}

void qemu_set_cloexec(int fd)
{
    int f;
    f = fcntl(fd, F_GETFD);
    fcntl(fd, F_SETFD, f | FD_CLOEXEC);
}

#endif

#endif

/*
 * Opens a file with FD_CLOEXEC set
 */
int qemu_open(const char *name, int flags, ...)
{
    int ret;
    int mode = 0;

    if (flags & O_CREAT) {
        va_list ap;

        va_start(ap, flags);
        mode = va_arg(ap, int);
        va_end(ap);
    }

#ifdef O_CLOEXEC
    ret = open(name, flags | O_CLOEXEC, mode);
#else
    ret = open(name, flags, mode);
    if (ret >= 0) {
        qemu_set_cloexec(ret);
    }
#endif

    return ret;
}

#ifndef __OS2__

#ifndef _WIN32
/*
 * Creates a pipe with FD_CLOEXEC set on both file descriptors
 */
int qemu_pipe(int pipefd[2])
{
    int ret;

#ifdef CONFIG_PIPE2
    ret = pipe2(pipefd, O_CLOEXEC);
    if (ret != -1 || errno != ENOSYS) {
        return ret;
    }
#endif
    ret = pipe(pipefd);
    if (ret == 0) {
        qemu_set_cloexec(pipefd[0]);
        qemu_set_cloexec(pipefd[1]);
    }

    return ret;
}
#endif

/*
 * Opens a socket with FD_CLOEXEC set
 */
int qemu_socket(int domain, int type, int protocol)
{
    int ret;

#ifdef SOCK_CLOEXEC
    ret = socket(domain, type | SOCK_CLOEXEC, protocol);
    if (ret != -1 || errno != EINVAL) {
        return ret;
    }
#endif
    ret = socket(domain, type, protocol);
    if (ret >= 0) {
        qemu_set_cloexec(ret);
    }

    return ret;
}

/*
 * Accept a connection and set FD_CLOEXEC
 */
int qemu_accept(int s, struct sockaddr *addr, socklen_t *addrlen)
{
    int ret;

#ifdef CONFIG_ACCEPT4
    ret = accept4(s, addr, addrlen, SOCK_CLOEXEC);
    if (ret != -1 || errno != EINVAL) {
        return ret;
    }
#endif
    ret = accept(s, addr, addrlen);
    if (ret >= 0) {
        qemu_set_cloexec(ret);
    }

    return ret;
}

#endif
