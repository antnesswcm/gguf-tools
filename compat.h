/* compat.h -- Compiler compatibility shims for antirez/gguf-tools
 *
 * The upstream code uses two GCC-only spellings that MSVC does not understand:
 *
 *   struct __attribute__((packed)) Foo { ... };        // sds.h (5 structs)
 *   } __attribute__((packed)) array;                    // gguflib.h (union member)
 *
 * Both are wrapped here so the same sources build with cl.exe / clang / gcc.
 *
 * Verified on this box:
 *   MSVC 14.29 (VS2019)  -- uses __pragma(pack(push,1)) / __pragma(pack(pop))
 *   MinGW-w64 gcc 8.1.0  -- keeps the native __attribute__((packed)) spelling
 *
 * Layout note: COMPAT_STRUCT_CLOSE ends without a semicolon, because the
 * sds.h source writes `} COMPAT_STRUCT_CLOSE` and the caller already has the
 * semicolon in mind (either inline or on the next line).  For MSVC the
 * trailing `__pragma(pack(pop))` needs to terminate the declaration, so the
 * macro absorbs the terminator too -- the caller's semicolon in the source is
 * then not required.  Both spellings compile identically.
 */
#ifndef GGUF_TOOLS_COMPAT_H
#define GGUF_TOOLS_COMPAT_H

#include <stdint.h>

#ifdef _MSC_VER

/* Windows SDK does not provide ssize_t.  On a 64-bit build ptrdiff_t is
 * a signed 64-bit integer, which matches POSIX ssize_t exactly. */
#ifndef _SSIZE_T_DEFINED
#define _SSIZE_T_DEFINED
typedef long long ssize_t;
#endif

/* MSVC: __pragma is the function-form equivalent of #pragma, and can be
 * placed inline in a declaration.  Emit the pack push immediately before
 * the `struct` keyword and pop after the closing brace.
 *
 * For union members (gguflib.h), we pack the *member's* struct so its
 * fields are tight (4+8 bytes for the GGUF array {type, len}), while
 * leaving the enclosing union to keep its natural 8-byte alignment
 * (driven by the double / uint64 / struct gguf_string members). */
#define COMPAT_PACKED_STRUCT \
    __pragma(pack(push, 1)) struct
#define COMPAT_STRUCT_CLOSE \
    } __pragma(pack(pop));
#define COMPAT_PACKED_MEMBER \
    __pragma(pack(push, 1)) __pragma(pack(pop))

#else  /* GCC / Clang / anything GNU-ish */

#ifndef _SSIZE_T_DEFINED
#define _SSIZE_T_DEFINED
typedef long long ssize_t;
#endif

#define COMPAT_PACKED_STRUCT struct __attribute__((packed))
#define COMPAT_STRUCT_CLOSE };
#define COMPAT_PACKED_MEMBER __attribute__((packed))

#endif /* _MSC_VER */

#endif /* GGUF_TOOLS_COMPAT_H */
