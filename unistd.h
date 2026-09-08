/* unistd.h -- no-op shim for Windows/MSVC
 *
 * gguflib.c includes <unistd.h> for the POSIX open/close/write/fstat
 * primitives.  On Windows the shims for those live in wincompat.h; this file
 * only exists so the include succeeds.  MinGW-w64 provides a real unistd.h,
 * so it will only be used when compiling with cl.exe.
 *
 * wincompat.h must be included before <fcntl.h> by any translation unit that
 * needs open/close/write (gguflib.c does so explicitly).
 */
#ifndef GGUF_TOOLS_UNISTD_H
#define GGUF_TOOLS_UNISTD_H

#ifdef _WIN32
/* Everything gguflib.c needs from unistd.h (close/write/dup) is provided by
 * wincompat.h, which is included by gguflib.c before this header.  Include
 * stdio.h for NULL and basic types. */
#include <stdio.h>
#include <stdlib.h>
#endif

#endif
