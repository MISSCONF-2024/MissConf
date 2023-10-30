
#ifndef _DEBUG_H
#define _DEBUG_H

#include <errno.h>

#include "types.h"

/************************
 * Debug & error macros *
 ************************/

/* Just print stuff to the appropriate stream. */

#ifdef MESSAGES_TO_STDOUT
#  define SAYF(x...)    printf(x)
#else 
#  define SAYF(x...)    fprintf(stderr, x)
#endif /* ^MESSAGES_TO_STDOUT */

/* Show a prefixed warning. */

#define WARNF(x...) do { \
    SAYF("[!] ""WARNING: " x); \
    SAYF("\n"); \
  } while (0)


/* Die with a verbose non-OS fatal error message. */

#define FATAL(x...) do { \
    SAYF("\n[-] PROGRAM ABORT : " x); \
    exit(1); \
  } while (0)

/* Die by calling abort() to provide a core dump. */

#define ABORT(x...) do { \
    SAYF("\n[-] PROGRAM ABORT : " x); \
    abort(); \
  } while (0)

/* Die while also including the output of perror(). */

#define PFATAL(x...) do { \
    fflush(stdout); \
    SAYF("\n[-]  SYSTEM ERROR : " x); \
    SAYF("       OS message : " "%s\n", strerror(errno)); \
    exit(1); \
  } while (0)

/* Die with FAULT() or PFAULT() depending on the value of res (used to
   interpret different failure modes for read(), write(), etc). */

#define RPFATAL(res, x...) do { \
    if (res < 0) PFATAL(x); else FATAL(x); \
  } while (0)

/* Error-checking versions of read() and write() that call RPFATAL() as
   appropriate. */

#define ck_write(fd, buf, len, fn) do { \
    u32 _len = (len); \
    s32 _res = write(fd, buf, _len); \
    if (_res != _len) RPFATAL(_res, "Short write to %s", fn); \
  } while (0)

#define ck_read(fd, buf, len, fn) do { \
    u32 _len = (len); \
    s32 _res = read(fd, buf, _len); \
    if (_res != _len) RPFATAL(_res, "Short read from %s", fn); \
  } while (0)

#endif /* ! _HAVE_DEBUG_H */
