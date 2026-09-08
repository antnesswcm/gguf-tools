/* wincompat.h -- Windows/MinGW portability shim for antirez/gguf-tools
 *
 * gguflib.c is written for Linux and uses three POSIX primitives that
 * either the Windows SDK does not provide or provides incorrectly:
 *
 *   mmap() / munmap()   memory-map a whole file -- SDK has no equivalent
 *   fstat()             SDK's _fstat fails with ENOMORe on files >~2 GB
 *                       (verified on the 2.48 GB Spark-X2.5-4B model)
 *
 * Everything else it needs is already provided by the SDK:
 *
 *   io.h / fcntl.h      open / close / write / O_RDONLY / O_WRONLY /
 *                       O_RDWR / O_APPEND / O_ACCMODE
 *   sys/stat.h          struct stat / S_IFREG
 *   winbase.h           CreateFileMappingW / MapViewOfFileEx
 *   compat.h            ssize_t (see compat.h)
 *
 * Important detail about fd -> HANDLE
 * -----------------------------------
 * The SDK's open() returns a POSIX-ish *fd*, which is NOT the same object as
 * a Windows HANDLE.  You have to convert it with _get_osfhandle() (io.h).
 * Creating a mapping on the raw fd value fails with ERROR_INVALID_HANDLE
 * because the fd and the HANDLE only happen to be numerically close on this
 * platform for small descriptors.  We always go through _get_osfhandle().
 *
 * This shim is purely additive: on non-Windows it resolves to nothing.
 */
#ifndef GGUF_TOOLS_WINCOMPAT_H
#define GGUF_TOOLS_WINCOMPAT_H

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>    /* O_RDONLY / O_WRONLY / O_RDWR / O_APPEND / O_ACCMODE */
#include <sys/stat.h> /* struct stat / S_IFREG */

#if defined(_WIN32)

#include <windows.h>
#include <io.h>       /* _get_osfhandle */

/* MapViewOfFileEx / CreateFileMappingW / WriteFile / CloseHandle 需要
 * Windows Vista 或以上的目标版本。vcvars64.bat 通常已经给出 0x0A00，
 * 这里做个兜底，保证在没有 vcvars 的调用方式下也能编译通过。 */
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#if (_WIN32_WINNT < 0x0600) && !defined(WINVER)
#define WINVER 0x0600
#endif

#ifndef MAP_FAILED
#define MAP_FAILED ((void*)-1)
#endif

#ifndef PROT_READ
#define PROT_READ    0x1
#define PROT_WRITE   0x2
#define MAP_SHARED   0x01
#endif

/* ---------------------------------------------------------------- Win <-> POSIX
 * errno codes.  MapWin32Error() fills in errno. */
static inline void wincompat_set_errno(void) {
    DWORD e = GetLastError();
    switch (e) {
    case ERROR_FILE_NOT_FOUND:  errno = ENOENT; break;
    case ERROR_ACCESS_DENIED:   errno = EACCES; break;
    case ERROR_PATH_NOT_FOUND:  errno = ENOENT; break;
#if (_WIN32_WINNT >= 0x0600) && defined(ERROR_INSUFFICIENT_DISK_SPACE)
    case ERROR_INSUFFICIENT_DISK_SPACE: errno = ENOSPC; break;
#endif
    case ERROR_NOT_ENOUGH_MEMORY: errno = ENOMEM; break;
    case ERROR_INVALID_HANDLE:  errno = EBADF;  break;
    case ERROR_NO_MORE_FILES:   errno = ENOENT; break;
    default:
        errno = EIO;
        break;
    }
}

/* Convert a POSIX fd (as returned by open()) to a Windows HANDLE.  This is
 * required for CreateFileMappingW / MapViewOfFileEx / GetFileSizeEx, which
 * all want the underlying HANDLE, not the fd wrapper the SDK's open() hands
 * back.  The conversion is a cheap lookup; do not skip it. */
static inline HANDLE wincompat_fd_to_handle(int fd) {
    return (HANDLE)_get_osfhandle(fd);
}

/* --- fstat() --------------------------------------------------------------
 * The Windows SDK's struct stat has a 4-byte st_size, which overflows for
 * files >~2 GB (verified on the 2.48 GB Spark-X2.5-4B-Q4_K_M model).
 * gguf_remap() is now compiled with `struct _stat64` on Windows (see the
 * #if in gguflib.c), so st_size is 8 bytes and _fstat64() fills it correctly.
 * This shim just forwards to _fstat64 with the right pointer type.
 *
 * gguflib.c only reads st_size; the rest of the struct is left as-is. */
static inline int wincompat_fstat(int fd, void *st) {
#if defined(_WIN32)
    return _fstat64(fd, (struct _stat64 *)st);
#else
    struct stat *sb = (struct stat *)st;
    if (fstat(fd, sb) == -1) {
        wincompat_set_errno();
        return -1;
    }
    return 0;
#endif
}

/* --- mmap() / munmap() ---------------------------------------------------- */
#define WINCOMPAT_MAX_MAPPINGS 32

typedef struct {
    int    used;
    int    fd;
    HANDLE hmap;
    void  *addr;
} wincompat_mapping;

static wincompat_mapping wincompat_maps[WINCOMPAT_MAX_MAPPINGS];

static inline void *wincompat_mmap(void *addr, size_t len, int prot,
                                   int flags, int fd, long offset) {
    HANDLE h, hmap;
    void *view;
    int i;
    BOOL  is_write  = (prot & PROT_WRITE) != 0;
    DWORD mapaccess = is_write ? FILE_MAP_WRITE : FILE_MAP_READ;

    (void)addr;
    if (len == 0 || offset != 0) { errno = EINVAL; return MAP_FAILED; }

    h = wincompat_fd_to_handle(fd);
    if (h == INVALID_HANDLE_VALUE || h == NULL) {
        errno = EBADF;
        return MAP_FAILED;
    }
    hmap = CreateFileMappingW(h, NULL,
                              is_write ? PAGE_READWRITE : PAGE_READONLY,
                              0, 0, NULL);
    if (hmap == NULL) { wincompat_set_errno(); return MAP_FAILED; }

    if (flags & 0x04) {          /* MAP_COPY: snapshot copy */
        view = MapViewOfFileEx(hmap, FILE_MAP_WRITE, 0, 0, (SIZE_T)len, NULL);
    } else {
        view = MapViewOfFileEx(hmap, mapaccess, 0, 0, (SIZE_T)len, NULL);
    }
    if (view == NULL) { wincompat_set_errno(); CloseHandle(hmap); return MAP_FAILED; }

    for (i = 0; i < WINCOMPAT_MAX_MAPPINGS; i++) {
        if (!wincompat_maps[i].used) break;
    }
    if (i == WINCOMPAT_MAX_MAPPINGS) {
        UnmapViewOfFile(view); CloseHandle(hmap);
        errno = ENOMEM;
        return MAP_FAILED;
    }
    wincompat_maps[i].used = 1;
    wincompat_maps[i].fd   = fd;
    wincompat_maps[i].hmap = hmap;
    wincompat_maps[i].addr = view;
    return view;
}

static inline int wincompat_munmap(void *addr, size_t len) {
    int i;
    (void)len;
    for (i = 0; i < WINCOMPAT_MAX_MAPPINGS; i++) {
        if (wincompat_maps[i].used && wincompat_maps[i].addr == addr) {
            UnmapViewOfFile(wincompat_maps[i].addr);
            CloseHandle(wincompat_maps[i].hmap);
            memset(&wincompat_maps[i], 0, sizeof(wincompat_maps[i]));
            return 0;
        }
    }
    errno = EINVAL;
    return -1;
}

/* --- bindings ------------------------------------------------------------
 * Bind mmap/munmap/fstat to our shims; the SDK's open/close/write are kept. */
#ifndef mmap
#define mmap(a, l, p, f, fd, o) wincompat_mmap((a), (l), (p), (f), (fd), (o))
#endif
#ifndef munmap
#define munmap(a, l)            wincompat_munmap((a), (l))
#endif
#ifndef fstat
#define fstat(fd, s)            wincompat_fstat((fd), (s))
#endif

#endif /* _WIN32 */
#endif /* GGUF_TOOLS_WINCOMPAT_H */
