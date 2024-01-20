
#ifndef _ALLOC_H
#define _ALLOC_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "debug.h"
// test

// #define FILE_NAME "/root/test/llvm_pass/shm_id.txt"
#define MAX_LINE 10

/***
// Get file data
#define get_file(buf) do { \
  FILE* fp; \
	int len; \
	if ((fp = fopen(FILE_NAME, "r")) == NULL) FATAL("fail to read!"); \
	while (fgets(buf, MAX_LINE, fp) != NULL) { \
		len = strlen(buf); \
		buf[len - 1] = '\0'; \
    } \
} while(0)
*/

/* User-facing macro to sprintf() to a dynamically allocated buffer. */

#define alloc_printf(_str...) ({ \
    u8* _tmp; \
    s32 _len = snprintf(NULL, 0, _str); \
    if (_len < 0) FATAL("Whoa, snprintf() fails?!"); \
    _tmp = ck_alloc(_len + 1); \
    snprintf((char*)_tmp, _len + 1, _str); \
    _tmp; \
  })

/* Macro to enforce allocation limits as a last-resort defense against
   integer overflows. */

#define ALLOC_CHECK_SIZE(_s) do { \
    if ((_s) > MAX_ALLOC) \
      ABORT("Bad alloc request: %u bytes", (_s)); \
  } while (0)

/* Macro to check malloc() failures and the like. */

#define ALLOC_CHECK_RESULT(_r, _s) do { \
    if (!(_r)) \
      ABORT("Out of memory: can't allocate %u bytes", (_s)); \
  } while (0)

/* Magic tokens used to mark used / freed chunks. */

#define ALLOC_MAGIC_C1  0xFF00FF00 /* Used head (dword)  */
#define ALLOC_MAGIC_F   0xFE00FE00 /* Freed head (dword) */
#define ALLOC_MAGIC_C2  0xF0       /* Used tail (byte)   */

/* Positions of guard tokens in relation to the user-visible pointer. */

#define ALLOC_C1(_ptr)  (((u32*)(_ptr))[-2])
#define ALLOC_S(_ptr)   (((u32*)(_ptr))[-1])
#define ALLOC_C2(_ptr)  (((u8*)(_ptr))[ALLOC_S(_ptr)])

#define ALLOC_OFF_HEAD  8
#define ALLOC_OFF_TOTAL (ALLOC_OFF_HEAD + 1)

/* Allocator increments for ck_realloc_block(). */

#define ALLOC_BLK_INC    256

/* Sanity-checking macros for pointers. */

#define CHECK_PTR(_p) do { \
    if (_p) { \
      if (ALLOC_C1(_p) ^ ALLOC_MAGIC_C1) {\
        if (ALLOC_C1(_p) == ALLOC_MAGIC_F) \
          ABORT("Use after free."); \
        else ABORT("Corrupted head alloc canary."); \
      } \
      if (ALLOC_C2(_p) ^ ALLOC_MAGIC_C2) \
        ABORT("Corrupted tail alloc canary."); \
    } \
  } while (0)

#define CHECK_PTR_EXPR(_p) ({ \
    typeof (_p) _tmp = (_p); \
    CHECK_PTR(_tmp); \
    _tmp; \
  })


/* Allocate a buffer, explicitly not zeroing it. Returns NULL for zero-sized
   requests. */

static inline void* DFL_ck_alloc_nozero(u32 size) {

  void* ret;

  if (!size) return NULL;

  ALLOC_CHECK_SIZE(size);
  ret = malloc(size + ALLOC_OFF_TOTAL);
  ALLOC_CHECK_RESULT(ret, size);

  // change void* to char*
  ret = (u8*)ret + ALLOC_OFF_HEAD;

  ALLOC_C1(ret) = ALLOC_MAGIC_C1;
  ALLOC_S(ret)  = size;
  ALLOC_C2(ret) = ALLOC_MAGIC_C2;

  return ret;

}


/* Allocate a buffer, returning zeroed memory. */

static inline void* DFL_ck_alloc(u32 size) {

  void* mem;

  if (!size) return NULL;
  mem = DFL_ck_alloc_nozero(size);

  return memset(mem, 0, size);

}


/* Free memory, checking for double free and corrupted heap. When DEBUG_BUILD
   is set, the old memory will be also clobbered with 0xFF. */

static inline void DFL_ck_free(void* mem) {

  if (!mem) return;

  CHECK_PTR(mem);

#ifdef DEBUG_BUILD

  /* Catch pointer issues sooner. */
  memset(mem, 0xFF, ALLOC_S(mem));

#endif /* DEBUG_BUILD */

  ALLOC_C1(mem) = ALLOC_MAGIC_F;

  // change void* to char*
  free((u8*)mem - ALLOC_OFF_HEAD);

}


/* Re-allocate a buffer, checking for issues and zeroing any newly-added tail.
   With DEBUG_BUILD, the buffer is always reallocated to a new addresses and the
   old memory is clobbered with 0xFF. */

static inline void* DFL_ck_realloc(void* orig, u32 size) {

  void* ret;
  u32   old_size = 0;

  if (!size) {

    DFL_ck_free(orig);
    return NULL;

  }

  if (orig) {

    CHECK_PTR(orig);

#ifndef DEBUG_BUILD
    ALLOC_C1(orig) = ALLOC_MAGIC_F;
#endif /* !DEBUG_BUILD */

    old_size  = ALLOC_S(orig);

    // change void* to char*
    orig = (u8*)orig     - ALLOC_OFF_HEAD;

    ALLOC_CHECK_SIZE(old_size);

  }

  ALLOC_CHECK_SIZE(size);

#ifndef DEBUG_BUILD

  ret = realloc(orig, size + ALLOC_OFF_TOTAL);
  ALLOC_CHECK_RESULT(ret, size);

#else

  /* Catch pointer issues sooner: force relocation and make sure that the
     original buffer is wiped. */

  ret = malloc(size + ALLOC_OFF_TOTAL);
  ALLOC_CHECK_RESULT(ret, size);

  if (orig) {

    memcpy(ret + ALLOC_OFF_HEAD, orig + ALLOC_OFF_HEAD, MIN(size, old_size));
    memset(orig + ALLOC_OFF_HEAD, 0xFF, old_size);

    ALLOC_C1(orig + ALLOC_OFF_HEAD) = ALLOC_MAGIC_F;

    free(orig);

  }

#endif /* ^!DEBUG_BUILD */

  ret = (u8*)ret +  ALLOC_OFF_HEAD;

  ALLOC_C1(ret) = ALLOC_MAGIC_C1;
  ALLOC_S(ret)  = size;
  ALLOC_C2(ret) = ALLOC_MAGIC_C2;

  if (size > old_size)
    memset((u8*)ret + old_size, 0, size - old_size);

  return ret;

}


/* Re-allocate a buffer with ALLOC_BLK_INC increments (used to speed up
   repeated small reallocs without complicating the user code). */

static inline void* DFL_ck_realloc_block(void* orig, u32 size) {

#ifndef DEBUG_BUILD

  if (orig) {

    CHECK_PTR(orig);

    if (ALLOC_S(orig) >= size) return orig;

    size += ALLOC_BLK_INC;

  }

#endif /* !DEBUG_BUILD */

  return DFL_ck_realloc(orig, size);

}


/* Create a buffer with a copy of a string. Returns NULL for NULL inputs. */

static inline u8* DFL_ck_strdup(u8* str) {

  void* ret;
  u32   size;

  if (!str) return NULL;

  size = strlen((char*)str) + 1;

  ALLOC_CHECK_SIZE(size);
  ret = malloc(size + ALLOC_OFF_TOTAL);
  ALLOC_CHECK_RESULT(ret, size);

  ret = (u8*)ret +  ALLOC_OFF_HEAD;

  ALLOC_C1(ret) = ALLOC_MAGIC_C1;
  ALLOC_S(ret)  = size;
  ALLOC_C2(ret) = ALLOC_MAGIC_C2;

  return (u8*)memcpy(ret, str, size);

}


/* Create a buffer with a copy of a memory block. Returns NULL for zero-sized
   or NULL inputs. */

static inline void* DFL_ck_memdup(void* mem, u32 size) {

  void* ret;

  if (!mem || !size) return NULL;

  ALLOC_CHECK_SIZE(size);
  ret = malloc(size + ALLOC_OFF_TOTAL);
  ALLOC_CHECK_RESULT(ret, size);
  
  ret = (u8*)ret +  ALLOC_OFF_HEAD;

  ALLOC_C1(ret) = ALLOC_MAGIC_C1;
  ALLOC_S(ret)  = size;
  ALLOC_C2(ret) = ALLOC_MAGIC_C2;

  return memcpy(ret, (u8*)mem, size);

}


/* Create a buffer with a block of text, appending a NUL terminator at the end.
   Returns NULL for zero-sized or NULL inputs. */

static inline u8* DFL_ck_memdup_str(u8* mem, u32 size) {

  u8* ret;

  if (!mem || !size) return NULL;

  ALLOC_CHECK_SIZE(size);
  ret = (u8*)malloc(size + ALLOC_OFF_TOTAL + 1);
  ALLOC_CHECK_RESULT(ret, size);
  
  ret = (u8*)ret +  ALLOC_OFF_HEAD;

  ALLOC_C1(ret) = ALLOC_MAGIC_C1;
  ALLOC_S(ret)  = size;
  ALLOC_C2(ret) = ALLOC_MAGIC_C2;

  memcpy(ret, mem, size);
  ret[size] = 0;

  return ret;

}


#ifndef DEBUG_BUILD

/* In non-debug mode, we just do straightforward aliasing of the above functions
   to user-visible names such as ck_alloc(). */

#define ck_alloc          DFL_ck_alloc
#define ck_alloc_nozero   DFL_ck_alloc_nozero
#define ck_realloc        DFL_ck_realloc
#define ck_realloc_block  DFL_ck_realloc_block
#define ck_strdup         DFL_ck_strdup
#define ck_memdup         DFL_ck_memdup
#define ck_memdup_str     DFL_ck_memdup_str
#define ck_free           DFL_ck_free

#define alloc_report()

#endif /* ! _DEBUG_BUILD */

#endif /* ! _HAVE_ALLOC_INL_H */
