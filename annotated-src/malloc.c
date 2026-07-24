/* Malloc implementation for multiple threads without lock contention.
   Copyright (C) 1996-2026 Free Software Foundation, Inc.
   Copyright The GNU Toolchain Authors.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of the
   License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; see the file COPYING.LIB.  If
   not, see <https://www.gnu.org/licenses/>.  */

/* This is a version (aka ptmalloc2) of malloc/free/realloc 
   written by Doug Lea and adapted to multiple threads/arenas 
   by Wolfram Gloger.

  There have been substantial changes made after the integration 
  into glibc in all parts of the code. Do not look for much 
  commonality with the ptmalloc2 version.

* Version ptmalloc2-20011215
  based on:
  VERSION 2.7.0 Sun Mar 11 14:14:06 2001  Doug Lea  (dl at gee)

* Quickstart

  In order to compile this implementation, a Makefile is provided
  with the ptmalloc2 distribution, which has pre-defined targets 
  for some popular systems (e.g. "make posix" for Posix threads).
  All that is typically required with regard to compiler flags is 
  the selection of the thread package via defining one out of 
  USE_PTHREADS, USE_THR or USE_SPROC. Check the thread-m.h file 
  for what effects this has.
  Many/most systems will additionally require USE_TSD_DATA_HACK to 
  be defined, so this is the default for "make posix".

* Why use this malloc?

  This is not the fastest, most space-conserving, most portable, 
  or most tunable malloc ever written. However it is among the 
  fastest while also being among the most space-conserving, 
  portable and tunable.
  Consistent balance across these factors results in a good 
  general-purpose allocator for malloc-intensive programs.

  The main properties of the algorithms are:
  - For large (>= 512 bytes) requests, it is a pure best-fit 
    allocator, with ties normally decided via FIFO 
    (i.e. least recently used).
  - For small (<= 64 bytes by default) requests, it is a caching
    allocator, that maintains pools of quickly recycled chunks.
  - In between, and for combinations of large and small requests,
    it does the best it can trying to meet both goals at once.
  - For very large requests (>= 128KB by default), it relies on 
    the system's memory mapping facilities, if supported.

  For a longer but slightly out of date high-level description, see
     http://gee.cs.oswego.edu/dl/html/malloc.html

  You may already by default be using a C library containing 
  a malloc that is based on some version of this malloc (for
  example in linux). You might still want to use the one in 
  this file in order to customize settings or to avoid
  overheads associated with library versions.

* Contents, described in more detail in 
  "description of public routines" below.

  Standard (ANSI/SVID/...)  functions:
    malloc(size_t n);
    calloc(size_t n_elements, size_t element_size);
    free(void* p);
    realloc(void* p, size_t n);
    memalign(size_t alignment, size_t n);
    valloc(size_t n);
    mallinfo()
    mallopt(int parameter_number, int parameter_value)

  Additional functions:
    independent_calloc(size_t n_elements, size_t size, void* chunks[]);
    independent_comalloc(size_t n_elements, size_t sizes[], void* chunks[]);
    pvalloc(size_t n);
    malloc_trim(size_t pad);
    malloc_usable_size(void* p);
    malloc_stats();

* Vital statistics:

  Supported pointer representation:  4 bytes (32-bit); 8 bytes (64-bit)
  Supported size_t  representation:  4 bytes (32-bit); 8 bytes (64-bit)
    Note: `size_t` is allowed to be 4 bytes even if 
          the pointers are 8. You can adjust this by 
          defining INTERNAL_SIZE_T.

  Alignment:  2 * sizeof(size_t)
    This suffices for nearly all current machines and C 
    compilers. However, you can define MALLOC_ALIGNMENT 
    to be wider than this if necessary.

  Minimum overhead per allocated chunk:  size_t bytes (4/8 bytes on 32/64-bit)
    Each malloced chunk has a hidden word of overhead 
    holding size and status information.

  Minimum allocated size:
    32-bit (i.e 4-byte ptrs):  16 bytes  (including 4 overhead)
    64-bit (i.e 8-byte ptrs):  32 bytes  (INTERNAL_SIZE_T=8)
                               24 bytes  (INTERNAL_SIZE_T=4)
                               (including overhead, 8/4 bytes)

    When a chunk is freed, 12 (for 4byte ptrs) or 20 (for 8 byte
    ptrs but 4 byte size) or 24 (for 8/8) additional bytes are
    needed; 4 (8) for a trailing size field and 8 (16) bytes for
    free list pointers. Thus, the minimum allocatable size is
    16/24/32 bytes.

    Even a request for zero bytes (i.e., malloc(0)) returns a
    pointer to something of the minimum allocatable size.

    The maximum overhead wastage (i.e., number of extra bytes
    allocated than were requested in malloc) is less than or equal
    to the minimum size, except for requests >= mmap_threshold that
    are serviced via mmap(), where the worst case wastage is 2 *
    sizeof(size_t) bytes plus the remainder from a system page (the
    minimal mmap unit); typically 4096 or 8192 bytes.

  Maximum allocated size:  4-byte size_t: 2^32 minus about two pages
		                  	   8-byte size_t: 2^64 minus about two pages

    It is assumed that (possibly signed) size_t values suffice to
    represent chunk sizes. `Possibly signed' is due to the fact
    that `size_t' may be defined on a system as either a signed or
    an unsigned type. The ISO C standard says that it must be
    unsigned, but a few systems are known not to adhere to this.
    Additionally, even when size_t is unsigned, sbrk (which is by
    default used to obtain memory from system) accepts signed
    arguments, and may not be able to handle size_t-wide arguments
    with negative sign bit.  Generally, values that would
    appear as negative after accounting for overhead and alignment
    are supported only via mmap(), which does not have this
    limitation.

    Requests for sizes outside the allowed range will perform an optional
    failure action and then return null. (Requests may also
    also fail because a system is out of memory.)

  Thread-safety: thread-safe

  Compliance: I believe it is compliant with the 1997 
    Single Unix Specification; Also SVID/XPG, ANSI C, 
    and probably others as well.

* Synopsis of compile-time options:

    People have reported using previous versions of this malloc on all
    versions of Unix, sometimes by tweaking some of the defines
    below. It has been tested most extensively on Solaris and Linux.
    People also report using it in stand-alone embedded systems.

    The implementation is in straight, hand-tuned ANSI C.  It is not
    at all modular. (Sorry!)  It uses a lot of macros.  To be at all
    usable, this code should be compiled using an optimizing compiler
    (for example gcc -O3) that can simplify expressions and control
    paths. (FAQ: some macros import variables as arguments rather than
    declare locals because people reported that some debuggers
    otherwise get confused.)

    OPTION                     DEFAULT VALUE

    Compilation Environment options:

    HAVE_MREMAP                0

    Changing default word sizes:

    INTERNAL_SIZE_T            size_t

    Configuration and functionality options:

    USE_PUBLIC_MALLOC_WRAPPERS NOT defined
    USE_MALLOC_LOCK            NOT defined
    MALLOC_DEBUG               NOT defined
    REALLOC_ZERO_BYTES_FREES   1

    Options for customizing MORECORE:

    MORECORE                   sbrk
    MORECORE_FAILURE           -1
    MORECORE_CONTIGUOUS        1
    MORECORE_CANNOT_TRIM       NOT defined
    MORECORE_CLEARS            1
    MMAP_AS_MORECORE_SIZE      (1024 * 1024)

    Tuning options that are also dynamically changeable via mallopt:

    DEFAULT_TRIM_THRESHOLD     128 * 1024
    DEFAULT_TOP_PAD            0
    DEFAULT_MMAP_THRESHOLD     128 * 1024
    DEFAULT_MMAP_MAX           65536

    There are several other #defined constants and macros that you
    probably don't want to touch unless you are extending or adapting malloc.  */

#ifndef void
#define void  void
#endif /* void */

#include <stddef.h>   /* for size_t */
#include <stdlib.h>   /* for getenv(), abort() */
#include <unistd.h>   /* for __libc_enable_secure */

#include <atomic.h>
#include <_itoa.h>
#include <bits/wordsize.h>
#include <sys/sysinfo.h>

#include <ldsodefs.h>
#include <setvmaname.h>

#include <unistd.h>
#include <stdio.h>    /* for malloc_stats */
#include <errno.h>
#include <assert.h>
#include <intprops.h>

#include <shlib-compat.h>

/* For uintptr_t. */
#include <stdint.h>

/* For stdc_count_ones. */
#include <stdbit.h>

/* For va_arg, va_start, va_end. */
#include <stdarg.h>

/* For MIN, MAX, powerof2. */
#include <sys/param.h>

/* For ALIGN_UP et. al. */
#include <libc-pointer-arith.h>

/* For DIAG_PUSH/POP_NEEDS_COMMENT et al. */
#include <libc-diag.h>

/* For memory tagging. */
#include <libc-mtag.h>

#include <malloc/malloc-internal.h>

/* For SINGLE_THREAD_P. */
#include <sysdep-cancel.h>

#include <libc-internal.h>

/* For tcache double-free check. */
#include <random-bits.h>
#include <sys/random.h>
#include <not-cancel.h>

/* Enable debugging support with this macro. */

#ifndef MALLOC_DEBUG
#define MALLOC_DEBUG 0
#endif

/* The tcache infrastructure. */

#if USE_TCACHE

/* TCACHE bins are single linked lists. */

/* TCACHE_SMALL_BINS is a tunable parameter. */
#define  TCACHE_SMALL_BINS  64

/* TCACHE_LARGE_BINS is fixed. */
#define  TCACHE_LARGE_BINS  12

/* Total count of tcache bins. */
#define  TCACHE_MAX_BINS	  (TCACHE_SMALL_BINS + TCACHE_LARGE_BINS)

/* The upper ceiling for a size to be small.
   1040 on 64-bit and 540 on 32-bit. */
#define  MAX_TCACHE_SMALL_SIZE    tidx2csize(TCACHE_SMALL_BINS-1)


/* Tcache bin index to chunk size. */
#define  tidx2csize(idx)	(((size_t)(idx)) * MALLOC_ALIGNMENT + MINSIZE)

/* Tcache bin index to usable size. */
#define  tidx2usize(idx)	(((size_t)(idx)) * MALLOC_ALIGNMENT + MINSIZE - SIZE_SZ)


/* Chunk size to thread index. */
#define  csize2tidx(x)  ((x - MINSIZE) / MALLOC_ALIGNMENT)

/* User requested size to thread index. */
#define  usize2tidx(x)  csize2tidx(checked_request2size(x))


/* Each tcache bin will hold at most this number 
   of chunks. It is a tunable parameter. */
#define  TCACHE_FILL_COUNT 16

/* This is the upper ceiling for TCACHE_FILL_COUNT. 
   It is a fixed parameter.
    (TCACHE_FILL_COUNT <= MAX_TCACHE_COUNT)
*/
#define  MAX_TCACHE_COUNT  UINT16_MAX
#endif

/* [NOT EXPLORED YET] */
/* Safe-Linking: Use randomness from ASLR (mmap_base) to 
   protect single-linked lists of TCache. That is, mask 
   the "next" pointers of the lists' chunks, and also 
   perform allocation alignment checks on them.

   This mechanism reduces the risk of pointer hijacking, 
   as was done with Safe-Unlinking in the double-linked 
   lists of Small-Bins.

   It assumes a minimum page size of 4096 bytes (12 bits).
   Systems with larger pages provide less entropy, although 
   the pointer mangling still works.
*/
#define PROTECT_PTR(pos, ptr)    (( __typeof(ptr)) ( (((size_t)(pos)) >> 12) ^ ((size_t)(ptr)) ))
#define REVEAL_PTR(ptr)  PROTECT_PTR (&ptr, ptr)


/* The REALLOC_ZERO_BYTES_FREES macro controls the 
   behavior of realloc(p, 0) when p is nonnull.

  If the macro is non-zero, realloc returns NULL.
  Otherwise, it is equal to malloc(0).

  ISO C17 says the realloc call has implementation 
  defined behavior, and it might not even free p.
*/
#ifndef  REALLOC_ZERO_BYTES_FREES
#define  REALLOC_ZERO_BYTES_FREES  1
#endif


/* Definition for getting more memory from the OS. */
#include "morecore.c"

#define  MORECORE          (*__glibc_morecore)
#define  MORECORE_FAILURE  NULL


/* [NOT EXPLORED YET] */
/* Memory tagging.

  Some systems support the concept of tagging (also known as
  coloring) memory locations on a fine grained basis.
  - Each memory location is given a color (normally allocated
    randomly) and pointers are also colored.
  - When the pointer is dereferenced, the pointer's color is 
    checked against the memory's color and if they differ the
    access is faulted (sometimes lazily).

  We use this in glibc by maintaining a single color for the 
  malloc data structures that are interleaved with the user 
  data and then assigning separate colors for each block 
  allocation handed out.
  - This way, simple buffer overruns will be rapidly detected.
  - When memory is freed, the memory is recolored back to the 
    glibc default so that simple use-after-free errors can 
    also be detected.

  If memory is reallocated the buffer is recolored even if the
  address remains the same. This has a performance impact, but 
  guarantees that the old pointer cannot mistakenly be reused 
  (code that compares old against new will see a mismatch and 
  will then need to behave as though realloc moved the data to 
  a new location).

  Internal API for memory tagging support.
    The aim is to keep the code for memory tagging support as 
    close to the normal APIs in glibc as possible, so that if 
    tagging is not enabled in the library, or is disabled at 
    runtime then standard operations can continue to be used.
    Support macros are used to do this.

    void *tag_new_zero_region(void *ptr, size_t size)
    - Allocates a new tag, 
    - colors the memory with that tag, 
    - zeros the memory, and 
    - returns a pointer that is correctly colored for that
      location.
    The non-tagging version will simply call memset with 0.

    void *tag_region (void *ptr, size_t size)
    - Color the region of memory pointed to by PTR and
      size SIZE with the color of PTR.
    - Returns the original pointer.

    void *tag_new_usable (void *ptr)
    - Allocate a new random color and use it to color the 
      user region of a chunk; this may include data from 
      the subsequent chunk's header if tagging is 
      sufficiently fine grained.
    - Returns PTR suitably recolored for accessing the
      memory there.

    void *tag_at (void *ptr)
    - Read the current color of the memory at the address 
      pointed to by PTR (ignoring it's current color) and 
      return PTR recolored to that color. PTR must be valid 
      address in all other respects.
    - When tagging is not enabled, it simply returns the original pointer.
*/
#ifdef USE_MTAG
static bool mtag_enabled = false;
static int mtag_mmap_flags = 0;
#else
#define  mtag_enabled false
#define  mtag_mmap_flags     0
#endif

static __always_inline void*
tag_region (void *ptr, size_t size)
{
  if (__glibc_unlikely (mtag_enabled))
    return __libc_mtag_tag_region (ptr, size);
  return ptr;
}

static __always_inline void*
tag_new_zero_region (void *ptr, size_t size)
{
  if (__glibc_unlikely (mtag_enabled))
    return __libc_mtag_tag_zero_region (__libc_mtag_new_tag (ptr), size);
  return memset (ptr, 0, size);
}

/* Defined later. */
static void* tag_new_usable (void *ptr);

static __always_inline void*
tag_at (void *ptr)
{
  if (__glibc_unlikely (mtag_enabled))
    return __libc_mtag_address_get_tag (ptr);
  return ptr;
}


#include <string.h>

/* MORECORE-related declarations by default, rely on sbrk. */

#ifndef MORECORE
#define MORECORE sbrk
#endif

/* MORECORE_FAILURE is the value returned upon failure. */
#ifndef MORECORE_FAILURE
#define MORECORE_FAILURE (-1)
#endif

/* If MORECORE_CONTIGUOUS is true, consecutive calls to 
   MORECORE with positive arguments always return 
   contiguous increasing addresses.
*/
#ifndef MORECORE_CONTIGUOUS
#define MORECORE_CONTIGUOUS  1
#endif

/* Define MORECORE_CANNOT_TRIM if your version of 
   MORECORE cannot release space back to the system 
   when given negative arguments. This is generally 
   necessary only if you are using a hand-crafted 
   MORECORE function that cannot handle negative 
   arguments.
*/
/* #define MORECORE_CANNOT_TRIM */

/* MORECORE_CLEARS           (default 1)

  It defines the degree to which the routine mapped 
  to MORECORE zeroes out memory:
  - never (0), 
  - only for newly allocated space (1), or
  - always (2).
*/
#ifndef MORECORE_CLEARS
#define MORECORE_CLEARS 1
#endif

/* MMAP_AS_MORECORE_SIZE is the minimum mmap size 
   argument to use if sbrk fails, and mmap is used 
   as a backup. The value must be a multiple of 
   page size.

  This backup strategy generally applies only when 
  systems have "holes" in the address space, so sbrk 
  cannot perform contiguous expansion, but the system 
  still have space.

  Between this, and the fact that mmap regions tend 
  to be limited, the size should be large, to avoid 
  too many mmap calls and thus avoid running out of 
  kernel resources.
*/
#ifndef MMAP_AS_MORECORE_SIZE
#define MMAP_AS_MORECORE_SIZE (1024 * 1024)
#endif

/* Define HAVE_MREMAP to make realloc() use mremap()
   to re-allocate large blocks. */
#ifndef HAVE_MREMAP
#define HAVE_MREMAP 0
#endif


/* This version of malloc supports the standard SVID/XPG 
   mallinfo routine that returns a struct containing usage 
   properties and statistics.

  It should work on any SVID/XPG compliant system that has 
  a /usr/include/malloc.h defining struct mallinfo. If you 
  would like to install such a thing yourself, cut out the 
  preliminary declarations as described above and below and 
  save them in a malloc.h file. But there's no compelling 
  reason to be bothered to do this.

  The main declaration needed is the mallinfo struct that 
  is returned (by-copy) by mallinfo().

  The SVID/XPG mallinfo struct contains a bunch of fields
  that are not even meaningful in this version of malloc.
  These fields are are instead filled by mallinfo() with
  other numbers that might be of interest.
*/


/* ---------- Description of public routines ------------ */

#if IS_IN (libc)

/* malloc(size_t n)

  Returns a pointer to a newly allocated chunk of at least 
  n bytes, or null if no space is available. Additionally, 
  on failure, errno is set to ENOMEM on ANSI C systems.

  - If n is zero, malloc returns a minimum-sized chunk. The 
    minimum size is 16 bytes on most 32-bit systems, and 24 
    or 32 bytes on 64-bit systems.
  - On most systems, size_t is an unsigned type, so calls
    with negative arguments are interpreted as requests for 
    huge amounts of space, which will often fail.
  - The maximum supported value of n differs across systems, 
    but is in all cases less than the maximum representable 
    value of a size_t.
*/
void *__libc_malloc (size_t);
libc_hidden_proto (__libc_malloc)

static void *__libc_calloc2 (size_t);
static void *__libc_malloc2 (size_t);

/* free(void* p)

  Releases the chunk of memory pointed to by p, that had 
  been previously allocated using malloc or a related 
  routine such as realloc.
  - It has no effect if p is null. It can have arbitrary 
    (i.e., bad!) effects if p has already been freed.
  - Unless disabled (using mallopt), freeing very large 
    spaces will when possible, automatically trigger 
    operations that give back unused memory to the system, 
    thus reducing program footprint.
*/
void __libc_free(void*);
libc_hidden_proto (__libc_free)

/* calloc(size_t n_elements, size_t element_size);

  Returns a pointer to (n_elements * element_size) bytes, 
  with all locations set to zero.
*/
void* __libc_calloc(size_t, size_t);

/* realloc(void* p, size_t n)

  Returns a pointer to a chunk of size n that contains 
  the same data as does chunk p up to the minimum of 
  (n, p's size) bytes, or null if no space is available.

  The returned pointer may or may not be the same as p. 
  The algorithm prefers extending p when possible, 
  otherwise it employs the equivalent of a malloc-copy-free 
  sequence.

  If p is null, realloc is equivalent to malloc.

  If space is not available, realloc returns null, errno 
  is set (if on ANSI) and p is NOT freed.

  if n is for fewer bytes than already held by p, the newly 
  unused space is lopped off and freed if possible. Unless 
  the #define REALLOC_ZERO_BYTES_FREES is set, realloc with 
  a size argument ofzero (re)allocates a minimum-sized chunk.

  Large chunks that were internally obtained via mmap will 
  always be grown using malloc-copy-free sequences unless 
  the system supports MREMAP (currently only linux).

  The old unix realloc convention of allowing the last-free'd 
  chunk to be used as an argument to realloc is not supported.
*/
void* __libc_realloc(void*, size_t);
libc_hidden_proto (__libc_realloc)

/* memalign(size_t alignment, size_t n);

  Returns a pointer to a newly allocated chunk of n bytes, 
  aligned in accord with the alignment argument.

  The alignment argument should be a power of two.
  - If the argument is not a power of two, the nearest 
    greater power is used.
  - Alignment of MALLOC_ALIGNMENT bytes is guaranteed by 
    normal malloc calls, so don't bother calling memalign 
    with an argument of MALLOC_ALIGNMENT or less.

  Overreliance on memalign is a sure way to fragment space.
*/
void* __libc_memalign(size_t, size_t);
libc_hidden_proto (__libc_memalign)

/* valloc(size_t n);

  Equivalent to memalign(pagesize, n), where pagesize 
  is the page size of the system. If the pagesize is 
  unknown, 4096 is used.
*/
void* __libc_valloc(size_t);


/* mallinfo()

  Returns (by copy) a struct containing various summary 
  statistics:

  arena:     current total non-mmapped bytes allocated from system
  ordblks:   the number of free chunks
  hblks:     current number of mmapped regions
  hblkhd:    total bytes held in mmapped regions
  usmblks:   always 0
  uordblks:  current total allocated space (normal or mmapped)
  fordblks:  total free space
  keepcost:  the maximum number of bytes that could ideally be 
             released back to system via malloc_trim. ("ideally" 
             means that it ignores page restrictions etc.)

  Because these fields are ints, but internal bookkeeping may
  be kept as longs, the reported values may wrap around zero 
  and thus be inaccurate.
*/
struct mallinfo2 __libc_mallinfo2(void);
libc_hidden_proto (__libc_mallinfo2)

struct mallinfo __libc_mallinfo(void);


/* pvalloc(size_t n);

  Equivalent to valloc(minimum-page-that-holds(n)), that is,
  round up n to nearest pagesize.
*/
void* __libc_pvalloc(size_t);

/* malloc_trim(size_t pad);

  If possible, gives memory back to the system (via 
  negative arguments to sbrk) if there is unused 
  memory at the `high' end of the malloc pool.
  - You can call this after freeing large blocks of 
    memory to potentially reduce the system-level 
    memory requirements of a program.
  - However, it cannot guarantee to reduce memory. 
    Under some allocation patterns, some large free 
    blocks of memory will be locked between two used 
    chunks, so they cannot be given back to the system.

  The `pad' argument to malloc_trim represents the amount 
  of free trailing space to leave untrimmed.
  - If this argument is zero, only the minimum amount of 
    memory to maintain internal data structures will be left 
    (one page or less).
  - Non-zero arguments can be supplied to maintain enough 
    trailing space to service future expected allocations 
    without having to re-obtain memory from the system.

  Returns 1 if it actually released any memory, else 0.
  On systems that do not support "negative sbrks", it 
  will always return 0.
*/
int __malloc_trim(size_t);

/* malloc_usable_size(void* p);

  Returns the number of bytes you can actually use in an 
  allocated chunk, which may be more than you requested 
  (although often not) due to alignment and minimum size 
  constraints.
  - You can use this many bytes without worrying about
    overwriting other allocated objects. This is not a 
    particularly great programming practice. 
  - malloc_usable_size can be more useful in debugging and 
    assertions, for example:

  p = malloc(n);
  assert(malloc_usable_size(p) >= 256);
*/
size_t __malloc_usable_size(void*);

/* malloc_stats();

  Prints on stderr 
  - the amount of space obtained from the system (both 
    via sbrk and mmap), 
  - the maximum amount (which may be more than current 
    if malloc_trim and/or munmap got called), and 
  - the current number of bytes allocated via malloc 
    (or realloc, etc) but not yet freed. Note that this 
    is the number of bytes allocated, not the number 
    requested. It will be larger than the number requested
    because of alignment and bookkeeping overhead. Because 
    it includes alignment wastage as being in use, this 
    figure may be greater than zero even when no user-level 
    chunks are allocated.

  The reported current and maximum system memory can be 
  inaccurate if a program makes other calls to system 
  memory allocation functions (normally sbrk) outside 
  of malloc.

  malloc_stats prints only the most commonly interesting 
  statistics. More information can be obtained by calling 
  mallinfo.
*/
void __malloc_stats(void);

/* posix_memalign(void **memptr, size_t alignment, size_t size);

  POSIX wrapper like memalign(), checking for validity of size.
*/
int __posix_memalign(void **, size_t, size_t);

#endif /* IS_IN (libc) */

/* mallopt(int parameter_number, int parameter_value)

  - Sets tunable parameters. The format is to provide a
    (parameter-number, parameter-value) pair.
  - mallopt then sets the corresponding parameter to the
    argument value if it can (i.e., so long as the value 
    is meaningful), and returns 1 if successful else 0.

  - SVID/XPG/ANSI defines four standard param numbers for
    mallopt, normally defined in malloc.h.
  - These params (M_MXFAST, M_NLBLKS, M_GRAIN, M_KEEP) 
    don't apply to our malloc, so setting them has no effect.
  - But this malloc also supports four other options in 
    mallopt. See below for details. Briefly, supported 
    parameters are as follows (listed defaults are for
    "typical" configurations).

   | Symbol           | Param# | Default    | Allowed param values            |
   | ---------------- | ------ | ---------- | ------------------------------- |
   | M_MXFAST         |    1   | 64         | 0-80  (deprecated)              |
   | M_TRIM_THRESHOLD |   -1   | 128 * 1024 | any   (-1U disables trimming)   |
   | M_TOP_PAD        |   -2   | 0          | any                             |
   | M_MMAP_THRESHOLD |   -3   | 128 * 1024 | any   (or 0 if no MMAP support) |
   | M_MMAP_MAX       |   -4   | 65536      | any   (0 disables use of mmap)  |
*/
int      __libc_mallopt(int, int);
#if IS_IN (libc)
libc_hidden_proto (__libc_mallopt)
#endif

/* mallopt tuning options. */

/* M_TRIM_THRESHOLD is the maximum amount of unused 
   top-most memory to keep before releasing via 
   malloc_trim in free().

  Automatic trimming is mainly useful in long-lived 
  programs. Because, trimming via sbrk can be slow 
  on some systems, and can sometimes be wasteful (in 
  cases where programs immediately afterward allocate 
  more large chunks) the value should be high enough 
  so that the overall system performance improves by 
  releasing this much memory.

  The trim threshold and the mmap control parameters 
  (see below) can be traded off with one another.

  Trimming and mmapping are two different ways of 
  releasing unused memory back to the system. Using 
  them, it is often possible to keep system-level 
  demands of a long-lived program down to a bare 
  minimum. For example, in one test suite of sessions 
  measuring the XF86 X server on Linux, using a trim 
  threshold of 128K and a mmap threshold of 192K led 
  to near-minimal long term resource consumption.

  If you are using this malloc in a long-lived program, 
  it should pay to experiment with these values. As a 
  rough guide, you might set to a value close to the 
  average size of a process (program) running on your 
  system. Releasing this much memory would allow such 
  a process to run in memory.

  Generally, it's worth it to tune for trimming rather 
  than memory mapping when a program undergoes phases 
  where several large chunks are allocated and released 
  in ways that can reuse each other's storage, perhaps 
  mixed with phases where there are no such chunks at 
  all. And in well-behaved long-lived programs, controlling 
  release of large blocks via trimming versus mapping is 
  usually faster.

  However, in most programs, these parameters serve mainly 
  as protection against the system-level effects of carrying 
  around massive amounts of unneeded memory. Since frequent 
  calls to sbrk, mmap, and munmap otherwise degrade performance, 
  the default parameters are set to relatively high values 
  that serve only as safeguards.

  The trim value must be greater than page size to have any 
  useful effect. To disable trimming completely, you can set 
  it to `(unsigned long)(-1)`.

  You can force an attempted trim by calling malloc_trim.

  Also, trimming is not generally possible in cases where
  the main arena is obtained via mmap.
*/
#define M_TRIM_THRESHOLD    (-1)

#ifndef DEFAULT_TRIM_THRESHOLD
#define DEFAULT_TRIM_THRESHOLD  (128 * 1024)
#endif

/* M_TOP_PAD is the amount of extra padding space to 
   allocate or retain whenever sbrk is called. It is 
   used in two ways internally:
  - When sbrk is called to extend the top of the arena 
    to satisfy a new malloc request, this much padding 
    is added to the sbrk request.
  - When malloc_trim is called automatically from free(), 
    it is used as the `pad` argument.

  In both cases, the actual amount of padding is rounded 
  to a page boundary, so that the arena always ends at a 
  page boundary.

  The main reason for using padding is to avoid calling 
  sbrk so often. Having even a small pad greatly reduces 
  the likelihood that nearly every malloc request during 
  program start-up (or after trimming) will invoke sbrk, 
  which needlessly wastes time.

  Automatic rounding-up to page-size units is normally 
  sufficient to avoid measurable overhead, so the default 
  is 0. However, in systems where sbrk is relatively slow, 
  it can pay to increase this value, at the expense of 
  carrying around more memory than the program needs.
*/
#define M_TOP_PAD    (-2)

#ifndef DEFAULT_TOP_PAD
#define DEFAULT_TOP_PAD    (0)
#endif

/* MMAP_THRESHOLD_MAX and _MIN are the bounds
   on the dynamically adjusted MMAP_THRESHOLD. */
#ifndef DEFAULT_MMAP_THRESHOLD_MIN
#define DEFAULT_MMAP_THRESHOLD_MIN (128 * 1024)
#endif

#ifndef DEFAULT_MMAP_THRESHOLD_MAX

#if __WORDSIZE == 32
# define DEFAULT_MMAP_THRESHOLD_MAX  (512 * 1024)
#else
# define DEFAULT_MMAP_THRESHOLD_MAX  (4 * 1024 * 1024 * sizeof(long))
#endif

#endif

/* M_MMAP_THRESHOLD is the request size threshold for 
   using mmap() to service a request. Requests of at 
   least this size that cannot be allocated using 
   already-existing space will be serviced via mmap.
   (If enough normal freed space already exists it is 
   used instead.)

  Using mmap segregates relatively large chunks of 
  memory so that they can be individually obtained 
  and released from the host system. A request that 
  is serviced through mmap is never reused by any 
  other request (at least not directly; the system 
  may just so happen to remap successive requests 
  to the same locations).

  Segregating space in this way has the benefits that:

  [1] Mmapped space can ALWAYS be individually released 
      back to the system, which helps keep the system 
      level memory demands of a long-lived program low.
  [2] Mapped memory can never become 'locked' between 
      other chunks, as can happen with normally allocated 
      chunks, which means that even trimming via 
      malloc_trim would not release them.
  [3] On some systems with "holes" in address spaces, 
      mmap can obtain memory that sbrk cannot.

  However, it has the disadvantages that:

  [1] The space cannot be reclaimed, consolidated, and 
      used to service later requests, as happens with 
      normal chunks.
  [2] It can lead to more wastage because of mmap page 
      alignment requirements.
  [3] It causes malloc performance to be more dependent 
      on the host system's memory management support 
      routines which may vary in implementation quality 
      and may impose arbitrary limitations. Generally, 
      servicing a request via normal malloc steps is 
      faster than going through mmap.


  The goal is to serve really large requests directly 
  with mmap.
*/
#define M_MMAP_THRESHOLD    (-3)

#ifndef DEFAULT_MMAP_THRESHOLD
#define DEFAULT_MMAP_THRESHOLD  DEFAULT_MMAP_THRESHOLD_MIN
#endif

/* M_MMAP_MAX is the maximum number of requests to 
   simultaneously service using mmap. This parameter 
   exists because some systems have a limited number 
   of internal tables for use by mmap, and using more 
   than a few of them may degrade performance.

  The default is set to a value that serves only as 
  a safeguard. Setting to 0 disables use of mmap for 
  servicing large requests.
*/
#define M_MMAP_MAX    (-4)

#ifndef DEFAULT_MMAP_MAX
#define DEFAULT_MMAP_MAX    (65536)
#endif

#include <malloc.h>

#ifndef RETURN_ADDRESS
#define RETURN_ADDRESS(X_)  (NULL)
#endif

/* Forward declarations. */
struct malloc_chunk;
typedef struct malloc_chunk* mchunkptr;

/* Internal routines. */

static void* _int_malloc(mstate, size_t);
static void  _int_free_chunk(mstate, mchunkptr, INTERNAL_SIZE_T, int);
static void  _int_free_merge_chunk(mstate, mchunkptr, INTERNAL_SIZE_T);
static INTERNAL_SIZE_T _int_free_create_chunk(
  mstate,
	mchunkptr, INTERNAL_SIZE_T,
	mchunkptr, INTERNAL_SIZE_T
);
static void  _int_free_maybe_trim(mstate, INTERNAL_SIZE_T);
static void* _int_realloc(
  mstate, mchunkptr,
  INTERNAL_SIZE_T,
  INTERNAL_SIZE_T
);
static void* _int_memalign(mstate, size_t, size_t);

#if IS_IN (libc)
static void* _mid_memalign(size_t, size_t);
#endif

#if USE_TCACHE
static void  malloc_printerr_tail(const char *str);
#endif
static void malloc_printerr(const char *str) __attribute__ ((noreturn));

static void munmap_chunk(mchunkptr p);
#if HAVE_MREMAP
static mchunkptr mremap_chunk(mchunkptr p, size_t new_size);
#endif

static size_t musable(void *mem);


/* ------------------ MMAP support ------------------  */

#include <fcntl.h>
#include <sys/mman.h>

#if !defined(MAP_ANONYMOUS) && defined(MAP_ANON)
# define MAP_ANONYMOUS MAP_ANON
#endif

#define MMAP(addr, size, prot, flags)    ( \
  __mmap(    \
    (addr),  \ 
    (size),  \ 
    (prot),  \ 
    (flags)|MAP_ANONYMOUS|MAP_PRIVATE,  \
    -1, \
    0 \
  ) \
)


/* --------------- Chunk representations --------------- */

/* This struct declaration is misleading (but accurate and 
   necessary). It declares a "view" into memory allowing 
   access to necessary fields at known offsets from a given 
   base. */
struct malloc_chunk {

  INTERNAL_SIZE_T      mchunk_prev_size;  /* Size of previous chunk (if free).  */
  INTERNAL_SIZE_T      mchunk_size;       /* Size in bytes, including overhead. */

  struct malloc_chunk* fd;                /* double links -- used only if free. */
  struct malloc_chunk* bk;

  /* Only used for large blocks: pointer to next larger size. */
  struct malloc_chunk* fd_nextsize;       /* double links -- used only if free. */
  struct malloc_chunk* bk_nextsize;
};

/* malloc_chunk details:

  The following includes lightly edited explanations 
  by Colin Plumb.

  Chunks of memory are maintained using a boundary tag 
  method as described in e.g., Knuth or Standish. See 
  the paper by Paul Wilson 
    ftp://ftp.cs.utexas.edu/pub/garbage/allocsrv.ps
  for a survey of such techniques.

  Sizes of free chunks are stored both in the front of 
  each chunk and at the end. This makes consolidating 
  fragmented chunks into bigger chunks very fast.

  The size fields also hold bits representing whether 
  chunks are free or in use.

  An allocated chunk looks like this:

        chunk-> +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                |             Size of previous chunk, if unallocated (P clear)  |
                +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                |             Size of chunk, in bytes                     |A|M|P|
          mem-> +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                |             User data starts here...                          |
                .                                                               .
                .             (malloc_usable_size() bytes)                      .
                |                                                               |
    nextchunk-> +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                |             (size of chunk, but used for application data)    |
                +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                |             Size of next chunk, in bytes                |A|0|1|
                +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

    where 
    - "chunk" is the front of the chunk for the purpose 
      of most of the malloc code, but "mem" is the 
      pointer that is returned to the user. 
    - "Nextchunk" is the beginning of the next contiguous 
      chunk.

  Chunks always begin on even word boundaries, so the mem 
  portion (which is returned to the user) is also on an 
  even word boundary, and thus at least double-word aligned.

  Free chunks are stored in circular doubly-linked lists, 
  and look like this:

        chunk-> +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                |             Size of previous chunk, if unallocated (P clear)  |
                +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         `head` |             Size of chunk, in bytes                     |A|0|P|
          mem-> +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                |             Forward pointer to next chunk in list             |
                +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                |             Back pointer to previous chunk in list            |
                +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                |             Unused space (may be 0 bytes long)                |
                .                                                               .
                |                                                               |
    nextchunk-> +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         `foot` |             Size of chunk, in bytes                           |
                +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                |             Size of next chunk, in bytes                |A|0|0|
                +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

    The P (PREV_INUSE) bit, stored in the unused low-order bit 
    of the chunk size (which is always a multiple of two words), 
    is an in-use bit for the *previous* chunk.
    - If that bit is "clear", then the word before the current 
      chunk size contains the previous chunk size, and can be 
      used to find the front of the previous chunk.
    - The very first chunk allocated always has this bit set, 
      preventing access to non-existent (or non-owned) memory.
    - If prev_inuse is set for any given chunk, then you CANNOT 
      determine the size of the previous chunk, and might even 
      get a memory addressing fault when trying to do so.

    [NEED TO VERIFY THE PER-THREAD ARENA CLAIM]
    The A (NON_MAIN_ARENA) bit is cleared for chunks on the 
    initial, main arena, described by the main_arena variable. 
    When additional threads are spawned, each thread receives 
    its own arena (up to a configurable limit, after which 
    arenas are reused for multiple threads), and the chunks 
    in these arenas have the A bit set. To find the arena for 
    a chunk on such a non-main arena, heap_for_ptr performs a 
    bit mask operation and indirection through the ar_ptr member 
    of the per-heap header heap_info (see arena.c).

    Note that the `foot` of the current chunk is actually 
    represented as the prev_size of the NEXT chunk. This makes 
    it easier to deal with alignments etc but can be very 
    confusing when trying to extend or adapt this code.

    The two exceptions to all this are:

    [1] The top chunk doesn't bother using the trailing size 
        field since there is no next contiguous chunk that 
        would have to index off it. After initialization, the 
        top chunk exist always. If it would become less than 
        MINSIZE bytes long, it is replenished.

    [CONFUSED ABOUT THE TRAILING SIZE FIELD]
    [2] Chunks allocated via mmap, which have the second lowest 
        order bit M (IS_MMAPPED) set in their size fields. As 
        they are allocated one-by-one, each must contain its own 
        trailing size field. If the M bit is set, the other bits 
        are ignored (because mmapped chunks neither belong to an 
        arena, nor adjacent to a freed chunk). The M bit is also 
        used for chunks which originally came from a dumped heap
        via malloc_set_state in hooks.c.
*/


/* ---------- Size and alignment checks and conversions ---------- */

/* Conversion from malloc headers to user pointers, and back.

  When using memory tagging the user data and the malloc data 
  structure headers have distinct tags. Converting fully from 
  one to the other involves extracting the tag at the other 
  address and creating a suitable pointer using it. That can 
  be quite expensive. There are cases when the pointers are not 
  dereferenced (for example only used for alignment check) so 
  the tags are not relevant, and there are cases when user data 
  is not tagged distinctly from malloc headers (user data is 
  untagged because tagging is done late in malloc and early in 
  free). User memory tagging across internal interfaces:

    sysmalloc:     Returns untagged memory.
    _int_malloc:   Returns untagged memory.
    _int_memalign: Returns untagged memory.
    _mid_memalign: Returns tagged memory.
    _int_realloc:  Takes and returns tagged memory.
*/

/* ? */
#define  CHUNK_HDR_SZ  (2 * SIZE_SZ)

/* Return the pointer to the payload memory corresponding to 
   chunk (p), without correcting the tag. */
#define chunk2mem(p)  ((void*) ((char*)(p) + CHUNK_HDR_SZ))

/* Return the pointer to the payload memory corresponding to 
   chunk (p) and extract the right tag. */
#define chunk2mem_tag(p)  ((void*) tag_at((char*)(p) + CHUNK_HDR_SZ))

/* Return the pointer to the malloc_chunk associated with a 
   payload memory and extract the right tag. */
#define mem2chunk(mem)  ((mchunkptr) tag_at(((char*)(mem) - CHUNK_HDR_SZ)))

/* The smallest possible chunk structurally. */
#define MIN_CHUNK_SIZE  (offsetof(struct malloc_chunk, fd_nextsize))

/* The smallest possible chunk size after keeping alignment in 
   consideration. This is the smallest chunk that malloc returns. */
#define MINSIZE  (unsigned long) (((MIN_CHUNK_SIZE+MALLOC_ALIGN_MASK) & ~MALLOC_ALIGN_MASK))


/* Check if m has acceptable alignment. */
#define misaligned_mem(m)    ((uintptr_t)(m) & MALLOC_ALIGN_MASK)
#define misaligned_chunk(p)  (misaligned_mem(chunk2mem(p)))


/* Align the requested bytes to the allocator's size model.

  [Precondition]: The input has already been validated. It 
  only performs size normalization and reporting errors 
  is out of its scope.
*/
#define request2size(req)  (  \
  (req + SIZE_SZ + MALLOC_ALIGN_MASK < MINSIZE)  \
  ? MINSIZE  \
  : (req + SIZE_SZ + MALLOC_ALIGN_MASK) & ~MALLOC_ALIGN_MASK  \
)


/* Combines validation and size normalization together.
   - If the validation fails, it returns SIZE_MAX and 
     the caller decides what to do with it.
   - Otherwise, it returns request2size.
*/
static __always_inline size_t
checked_request2size(size_t req) __nonnull (1)
{
  /* A static assert checks a condition at compile-time 
     and stops compiling if that condition is evaluated 
     false. */
  _Static_assert(
    PTRDIFF_MAX <= (SIZE_MAX / 2),
    "PTRDIFF_MAX is not more than half of SIZE_MAX"
  );

  if (__glibc_unlikely(req > PTRDIFF_MAX))
    return SIZE_MAX;

  /* [Explore memory tagging and decide to keep it or not.] */

  /* When using tagged memory, we cannot share the end of 
     the user block with the header for the next chunk, so
     ensure that we allocate blocks that are rounded up to
     the granule size.

    Take care not to overflow from close to MAX_SIZE_T 
    to a small number. Ideally, this would be part of 
    request2size(), but that must be a macro that produces 
    a compile time constant if passed a constant literal.
  */
 /* ?? */
  if (__glibc_unlikely(mtag_enabled)){
    /* Ensure this is not evaluated if !mtag_enabled, 
       see gcc PR 99551. */
    asm ("");

    req = (
      req + (__MTAG_GRANULE_SIZE - 1)
    ) & ~(size_t)(__MTAG_GRANULE_SIZE - 1);
  }

  return request2size(req);
}

/* --------------- Physical chunk operations --------------- */

/* The PREV_INUSE bit is the lowest bit in mchunk_size (LSB). */
#define  PREV_INUSE  0x1

/* Extract the PREV_INUSE bit of the chunk (p). Used to 
   identify if the (p-1) chunk is free (0), or in-use (1). */
#define  prev_inuse(p)  ((p)->mchunk_size & PREV_INUSE)


/* The second lower order bit in mchunk_size is used 
   for chunks that are mmapped.

  [NOTE]: An mmapped chunk is a whole mmapped region 
  used as a chunk.
*/
#define  IS_MMAPPED  0x2

/* Check if a chunk is mmapped. */
#define chunk_is_mmapped(p)    ((p)->mchunk_size & IS_MMAPPED)


/* Chunk belonging to the non-main arena have this bit 
   set. It is is only set immediately before handing 
   the chunk to the user.
*/
#define  NON_MAIN_ARENA  0x4

/* Check if a chunk is from the main arena. */
#define  chunk_main_arena(p)  (((p)->mchunk_size & NON_MAIN_ARENA) == 0)

/* Mark the chunk a property of a non-main arena. */
#define  set_non_main_arena(p)  ((p)->mchunk_size |= NON_MAIN_ARENA)


/* Bits to mask off when extracting size. */
#define  SIZE_BITS  (PREV_INUSE|IS_MMAPPED|NON_MAIN_ARENA)

/* Chunk size, ignoring the metadata bits. */
#define  chunksize(p)  (chunksize_nomask(p) & ~(SIZE_BITS))

/* Chunk size, including the metadata bits. */
#define  chunksize_nomask(p)  ((p)->mchunk_size)

/* Pointer to the next malloc_chunk in memory, i.e. (p+1) chunk. */
#define  next_chunk(p)  ((mchunkptr) (((char*)(p)) + chunksize(p)))

/* Retrieve the size of (p-1) chunk using (p)->prev_size. */
#define  prev_size(p)  ((p)->mchunk_prev_size)

/* Set the prev_size of chunk (p). */
#define  set_prev_size(p, sz)  ((p)->mchunk_prev_size = (sz))

/* Pointer to the previous malloc_chunk in memory, i.e. (p-1) chunk. */
#define  prev_chunk(p)  ((mchunkptr) ((char*)(p) - prev_size(p)))

/* Treat the address (ptr + offset) as a malloc_chunk. */
#define  chunk_at_offset(p, s)  ((mchunkptr) ((char*)(p) + s))


/* Extract the PREV_INUSE bit of the (p+1) chunk and perform
   operations on it.

  There are two set of macros doing the same thing with a 
  small difference. */

/* [Set #1]: They use the mchunk_size of chunk (p) to reach 
    the (p+1) chunk. */

/* [1] Determine the status of chunk (p). */
#define  inuse(p)          (( ((mchunkptr) ((char*)(p) + chunksize(p)))->mchunk_size ) &    PREV_INUSE)

/* [2] Mark (p) as an in-use chunk. */
#define  set_inuse(p)      (( ((mchunkptr) ((char*)(p) + chunksize(p)))->mchunk_size ) |=   PREV_INUSE)

/* [3] Mark (p) as a free chunk. */
#define  clear_inuse(p)    (( ((mchunkptr) ((char*)(p) + chunksize(p)))->mchunk_size ) &= (~PREV_INUSE))


/* [Set #2]: They rely on a user supplied size to find 
    the (p+1) chunk.
    It is useful in situations like coalescing, where 
    the existing mchunk_size can not be used.
*/

/* [1] Determine the status of chunk (p). */
#define  inuse_bit_at_offset(p, s)  ( ((mchunkptr) ((char*)(p) + s))->mchunk_size & PREV_INUSE)

/* [2] Mark (p) as an in-use chunk. */
#define  set_inuse_bit_at_offset(p, s)  (((mchunkptr) (((char*)(p)) + s))->mchunk_size |= PREV_INUSE)

/* [3] Mark (p) as a free chunk. */
#define  clear_inuse_bit_at_offset(p, s)  (((mchunkptr) ((char*)(p) + s))->mchunk_size &= ~(PREV_INUSE))


/* Set the mchunk_size of chunk (p) without disturbing its 
   metadata bits. The metadata bits are first extracted and 
   then OR-ed with the new size.
*/
#define  set_head_size(p, s)  ((p)->mchunk_size = (((p)->mchunk_size & SIZE_BITS) | (s)))

/* Set the mchunk_size of chunk (p). */
#define  set_head(p, s)  ((p)->mchunk_size = (s))

/* Set the prev_size of the (p+1) chunk. */
#define  set_foot(p, s)  (((mchunkptr) ((char*)(p) + s))->mchunk_prev_size = (s))

#pragma GCC poison mchunk_size
#pragma GCC poison mchunk_prev_size


/* This is the size of the real usable data in 
   the chunk. Not valid for dumped heap chunks. */
#define memsize(p)    (  \
  __MTAG_GRANULE_SIZE > SIZE_SZ && __glibc_unlikely (mtag_enabled)  \
  ? chunksize(p) - CHUNK_HDR_SZ            \
  : chunksize(p) - CHUNK_HDR_SZ + SIZE_SZ
)

/* If memory tagging is enabled the layout changes to 
   accommodate the granule size, this is wasteful for
   small allocations so not done by default. Both the 
   chunk header and user data has to be granule aligned.
*/
_Static_assert(
  __MTAG_GRANULE_SIZE <= CHUNK_HDR_SZ,
  "memory tagging is not supported with large granule."
);

static __always_inline void*
tag_new_usable(void *ptr)
{
  if (__glibc_unlikely(mtag_enabled) && ptr){
    mchunkptr cp = mem2chunk(ptr);
    ptr = __libc_mtag_tag_region(__libc_mtag_new_tag(ptr), memsize(cp));
  }
  return ptr;
}

/* Huge page used for an mmap chunk. */
#define  MMAP_HP  0x1

/* Return whether an mmap chunk uses huge pages. */
static __always_inline bool
mmap_is_hp (mchunkptr p)
{
  return prev_size (p) & MMAP_HP;
}

/* Return the mmapped chunk's offset from mmap base. */
static __always_inline size_t
mmap_base_offset (mchunkptr p)
{
  return prev_size(p) & ~MMAP_HP;
}

/* Return the pointer to mmap base from a chunk with 
   IS_MMAPPED set. */
static __always_inline uintptr_t
mmap_base(mchunkptr p)
{
  return ((uintptr_t)(p) - mmap_base_offset(p));
}

/* Return the total mmap size of a chunk with 
   IS_MMAPPED set. */
static __always_inline size_t
mmap_size (mchunkptr p)
{
  return mmap_base_offset(p) + chunksize(p) + CHUNK_HDR_SZ;
}

/* Places malloc_chunk on an mmapped segment's base. */
static __always_inline mchunkptr mmap_set_chunk(
  uintptr_t mmap_base, 
  size_t mmap_size, 
  size_t offset, 
  bool is_hp
){
  mchunkptr p  = (mchunkptr)(mmap_base + offset);
  prev_size(p) = offset | (is_hp ? MMAP_HP : 0);
  set_head(
    p,
    (mmap_size - offset - CHUNK_HDR_SZ) | IS_MMAPPED
  );
  return p;
}


/* --------------- Internal data structures ---------------

  All internal state is held in an instance of malloc_state 
  defined below. */

/* Bins: An array of bin headers for free chunks.

  [1] A bin is implemented as a doubly linked list.
  [2] The bins are approximately proportionally (log) 
      spaced.
  [3] There are a lot of these bins (128). This may 
      look excessive, but works very well in practice. 
      Most bins hold sizes that are unusual as malloc 
      request sizes, but are more usual for fragments 
      and consolidated sets of chunks, which is what 
      these bins hold, so they can be found quickly.
  [4] All procedures maintain the invariant that a 
      free chunk is always surrounded by in-use chunks, 
      or the end of memory.

  Chunks in bins are kept in size order, with ties going 
  to the approximately least recently used chunk.

  Ordering isn't needed in small bins, but facilitates 
  best-fit allocation for larger chunks. Since these 
  lists are sequential, keeping them in order almost 
  never requires enough traversal to warrant using 
  fancier ordered data structures.

  [NEEDS VALIDATION]
  Chunks of the same size are linked with the most recently 
  freed at the front, and allocations are taken from the back.
  This results in LRU (FIFO) allocation order, which tends to 
  give each chunk an equal opportunity to be consolidated with 
  adjacent freed chunks, resulting in larger free chunks and 
  less fragmentation.

  To simplify use in double-linked lists, each bin header acts
  as a malloc_chunk. This avoids special-casing for headers.
  But to conserve space and improve locality, we allocate
  only the fd/bk pointers of bins, and then use repositioning
  tricks to treat these as the fields of a malloc_chunk*.
*/
typedef struct malloc_chunk *mbinptr;

/* bin_at(m, 0) does not exist */
#define bin_at(m, i)    (mbinptr) ( \
  ((char*) &( (m)->bins[(i-1)*2] )) - offsetof(struct malloc_chunk, fd)  \
)

/* Analog of ++bin */
#define next_bin(b)    (mbinptr) ((char*)(b) + (sizeof(mchunkptr) << 1))

/* Reminder about list directionality within bins, 
   where (b) is the bin_handler. */
#define first(b)     ((b)->fd)
#define last(b)      ((b)->bk)

#define NBINS         128
#define NSMALLBINS    64

#define SMALLBIN_WIDTH          MALLOC_ALIGNMENT
#define SMALLBIN_CORRECTION    (MALLOC_ALIGNMENT > CHUNK_HDR_SZ)
#define MIN_LARGE_SIZE         ((NSMALLBINS - SMALLBIN_CORRECTION) * SMALLBIN_WIDTH)

#define in_smallbin_range(sz)    ( \
  (unsigned long)(sz) < (unsigned long)(MIN_LARGE_SIZE) \
)

#define smallbin_index(sz)    ( \
  ( \
    (SMALLBIN_WIDTH == 16)     \
    ? (((unsigned)(sz)) >> 4)  \
    : (((unsigned)(sz)) >> 3)  \
  ) + SMALLBIN_CORRECTION      \
)

#define largebin_index_32(sz)  ( \
  (((unsigned long)(sz) >>  6) <= 38) ?  56 + ((unsigned long)(sz) >>  6) : \
  (((unsigned long)(sz) >>  9) <= 20) ?  91 + ((unsigned long)(sz) >>  9) : \
  (((unsigned long)(sz) >> 12) <= 10) ? 110 + ((unsigned long)(sz) >> 12) : \
  (((unsigned long)(sz) >> 15) <=  4) ? 119 + ((unsigned long)(sz) >> 15) : \
  (((unsigned long)(sz) >> 18) <=  2) ? 124 + ((unsigned long)(sz) >> 18) : \
  126 \
)

#define largebin_index_32_big(sz)  ( \
  (((unsigned long)(sz) >>  6) <= 45) ?  49 + ((unsigned long) (sz) >>  6) : \
  (((unsigned long)(sz) >>  9) <= 20) ?  91 + ((unsigned long) (sz) >>  9) : \
  (((unsigned long)(sz) >> 12) <= 10) ? 110 + ((unsigned long) (sz) >> 12) : \
  (((unsigned long)(sz) >> 15) <=  4) ? 119 + ((unsigned long) (sz) >> 15) : \
  (((unsigned long)(sz) >> 18) <=  2) ? 124 + ((unsigned long) (sz) >> 18) : \
  126 \
)

#define largebin_index_64(sz)   ( \
  (((unsigned long)(sz) >>  6) <= 48) ?  48 + ((unsigned long)(sz) >> 6)  : \
  (((unsigned long)(sz) >>  9) <= 20) ?  91 + ((unsigned long)(sz) >> 9)  : \
  (((unsigned long)(sz) >> 12) <= 10) ? 110 + ((unsigned long)(sz) >> 12) : \
  (((unsigned long)(sz) >> 15) <=  4) ? 119 + ((unsigned long)(sz) >> 15) : \
  (((unsigned long)(sz) >> 18) <=  2) ? 124 + ((unsigned long)(sz) >> 18) : \
  126 \
)

#define largebin_index(sz)    (  \
  (SIZE_SZ == 8)                 \
  ? largebin_index_64(sz)        \
  : (MALLOC_ALIGNMENT == 16)     \
    ? largebin_index_32_big(sz)  \
    : largebin_index_32(sz)      \
)

#define bin_index(sz)    ( \
  in_smallbin_range(sz)    \
  ? smallbin_index(sz)     \
  : largebin_index(sz)     \
)

/* Take a chunk off a bin list. */
static void unlink_chunk (mstate av, mchunkptr p)
{
  /* [TEST 1]: The prevsize of the (p+1) chunk 
      must be equal to the size of chunk p. */
  if (chunksize(p) != prev_size(next_chunk(p)))
    malloc_printerr("corrupted size vs. prev_size");

  /* (p+1) chunk in the bin. */
  mchunkptr fd = p->fd;

  /* (p-1) chunk in the bin. */
  mchunkptr bk = p->bk;

  /* [TEST 2]: The (p+1) and (p-1) chunks must have 
     correct links with chunk p. */
  if (__glibc_unlikely(fd->bk != p || bk->fd != p))
    malloc_printerr("corrupted double-linked list");

  /* Unlink p from its bin. */

  /* (p-1)->fd = (p+1) */
  fd->bk = bk;

  /* (p+1)->bk = (p-1) */
  bk->fd = fd;

  /* The skip list pointers are NULL in unsorted chunks, 
     and garbage in small chunks. But they are maintained 
     in large chunks.

    A large bin manage chunks of multiple sizes. The skip 
    list pointers of the first chunk in every size class 
    has a non-NULL value, while the duplicates are set NULL.

    If (p) was a large chunk and unique in it size class, 
    removing it from its large bin requires updating the 
    skip list pointers as well.
  */
  if (
    !in_smallbin_range(chunksize_nomask(p)) && 
    p->fd_nextsize != NULL
  ){
    /* [TEST]: The chunks next and previous to (p) in 
        the skip list must be pointing to (p). */
    if (
      (p->fd_nextsize)->bk_nextsize != p || 
      (p->bk_nextsize)->fd_nextsize != p
    )
    	malloc_printerr("corrupted double-linked list (not small)");

    /* If the (p+1) chunk has its nextsize pointers NULL, 
       it is a duplicate in the same size class as (p). */
    if (fd->fd_nextsize == NULL){
      /* If the large bin had chunks of only one size 
         class, the skip list pointers of (p) will be 
         pointing to itself. In this case, we just have 
         to update the (p+1) chunk's skip list pointers 
         to itself.
      */
  	  if (p->fd_nextsize == p)
	      fd->fd_nextsize = fd->bk_nextsize = fd;

      /* Otherwise, the large bin has chunks of multiple 
         size classes and we have to update the skip list 
         pointers to treat the (p+1) chunk as the new 
         unique chunk of that size class in this large bin.
      */
      else{
        /* (p+1)->fd_next = p->fd_nextsize */
        fd->fd_nextsize = p->fd_nextsize;

        /* (p+1)->bk_next = p->bk_nextsize */
        fd->bk_nextsize = p->bk_nextsize;

        /* Update the skip list pointers of the next 
           and the previous chunks in the skip list 
           to the (p+1) chunk.
        */
        p->fd_nextsize->bk_nextsize = fd;
        p->bk_nextsize->fd_nextsize = fd;
      }
    }

    /* Chunk p is the only chunk in its size class and 
       the (p+1) chunk is already the next chunk in the 
       skip list. */
    else{
      /* (p+1)->bk_next = p->bk_next */
      p->fd_nextsize->bk_nextsize = p->bk_nextsize;

      /* (p-1)->fd_next = p->fd_next */
      p->bk_nextsize->fd_nextsize = p->fd_nextsize;
    }
  }
}

/* Unsorted bin. */
#define unsorted_chunks(M)    bin_at(M, 1)

/* Top chunk is the chunk at the end of the available 
   memory. It is treated specially.

  [1] It is never included in any bin. 
  [2] Chunks are carved out of it if every other 
      allocation pathway has failed. 
  [3] From the left side, it always borders with an 
      in-use chunk. If that chunk is freed, it is 
      coalesced into the top chunk.
  [4] If the top chunk accumulates memory above the 
      trimming threshold (DEFAULT_TRIM_THRESHOLD), 
      excess memory is released back to the system, 
      reducing pressure on physical memory.
*/

/* Why av->top initially points to the unsorted bin?

  __ptmalloc_init() initializes the early allocator 
  metadata during startup. The top chunk is not 
  initialized as we have to acquire memory from kernel.

  The main_arena is initialized on static storage. So, 
  normal variables are zero and pointer variables are 
  NULL, i.e. (void*)(0). The only exception to this is
  manual initialization, which can happens in two ways.
  [1] The `.member` addressing, where except the targeted 
      member, every other member is initialized with zero. 
      `mutex` (1st), `next` (7th) and `attached_threads` 
      (9th) fields are initialized in this manner.
  [2] Initializing a member after the struct is initialized.

  On the first malloc call, the top chunk's mchunk_size 
  must be 0, so that no special casing is required in 
  sysmalloc.

  Normally, av->top would be NULL. Dereferencing it to 
  access mchunk_size will raise a segmentation fault, 
  as the Linux kernel doesn't map the first virtual page.

  The solution is to make av->top point at bin_at(M, 1), 
  i.e. the unsorted bin. This is done using individual 
  member initialzation in malloc_init_state(). Let's 
  understand how it solves our problem.
  - These are the members before bins[] in malloc_state: 
      mutex | flags | top | last_remainder | bins[]
  - Except mutex, the 3 members before bins[] are 0x0. 
  - bin_at(M, 1) resolves to `&bins[-2]`. So, &bins[-1] 
    will represent the mchunk_size. Notice that these 
    are basically `&top` and `&last_remainder` fields, 
    respectively. Upon dereferencing, we will get 0.

  Basically, we are putting the address of av->top in 
  av->top, i.e. 
      av->top = (mchunkptr)(&(av->top))
  - When we do (av->top)->mchunk_size, it is 
      *(&av->top + 8), 
    which is effectively *(&last_remainder), which is 0.
  - If av->top was 0x0, (av->top)->mchunk_size would be 
      *(0x0 + 8), leading to a segmentation fault.

  The kernel returns zeroed memory for mmap requests, 
  so this works for non-main arenas as well.
*/
#define initial_top(M)    unsorted_chunks(M)

/* Binmap: It is a bitvector recording whether bins are 
   definitely empty so they can be skipped over during 
   during traversals.

  It helps in compensating for the large number of bins,
  enabling bin-by-bin searching.

  The bits are NOT always cleared as soon as bins are 
  empty, but instead only when they are noticed to be 
  empty during traversal in malloc.
  [BUT WHY? WHAT'S THE ADVANTAGE OF THAT?]
*/

#define BINMAPSHIFT    5
#define BITSPERMAP     (1U << BINMAPSHIFT)
#define BINMAPSIZE     (NBINS / BITSPERMAP)

#define idx2block(i)    ((i) >> BINMAPSHIFT)
#define idx2bit(i)      ((1U << ((i) & ((1U << BINMAPSHIFT) - 1))))

#define mark_bin(m, i)      (m)->binmap[idx2block(i)] |=   idx2bit(i)
#define unmark_bin(m, i)    (m)->binmap[idx2block(i)] &= ~(idx2bit(i))
#define get_binmap(m, i)    (m)->binmap[idx2block(i)] &    idx2bit(i)


/* [BECAUSE TRIMMING IS DONE ON THE TOP CHUNK, WHAT IS 
    THE POINT OF THIS?] */
/* ATTEMPT_TRIMMING_THRESHOLD is the size of a chunk in 
   free() that may attempt trimming of an arena's heap.
   - This is a heuristic, so the exact value should not 
     matter too much.
   - It is defined at half the default trim threshold as 
     a compromise heuristic to only attempt trimming if 
     it is likely to release a significant amount of 
     memory.
*/
#define  ATTEMPT_TRIMMING_THRESHOLD  (65536UL)

/* [UPDATE AFTER UNDERSTANDING FOREIGN SBRK] */
/* NONCONTIGUOUS_BIT indicates that MORECORE does not 
   return contiguous regions. Otherwise, contiguity 
   is exploited in merging together, when possible, 
   results from consecutive MORECORE calls.

  The initial value comes from MORECORE_CONTIGUOUS, 
  but is changed dynamically if mmap is ever used 
  as an sbrk substitute. */
#define  NONCONTIGUOUS_BIT  (2U)

/* Checks contiguity. */
#define contiguous(M)          (((M)->flags & NONCONTIGUOUS_BIT) == 0)

/* Mark non-contiguity. */
#define set_noncontiguous(M)   ((M)->flags |= NONCONTIGUOUS_BIT)

/* Mark contiguity. */
/* It was first introduced in dlmalloc@2.7.0 and it 
   has never been used even once.

  The last change that this line received was 21 
  years from today (July 2026), it was in 2005.
  - The full commit hash: 
    9bf248c6c6290a2a8a729f10f1d94258868a0650
  - GitHub Link:
    https://github.com/bminor/glibc/commit/9bf248c6c6290a2a8a729f10f1d94258868a0650

   dlmalloc@2.7.0 was released on March 11, 2001. */
#define set_contiguous(M)      ((M)->flags &= ~NONCONTIGUOUS_BIT)


/* ----------- Internal state representation and initialization ----------- */
struct malloc_state
{
  /* Serialize access. */
  __libc_lock_define (, mutex);

  /* Flags  */
  int flags;

  /* Base of the topmost chunk -- not otherwise kept in a bin. */
  mchunkptr top;

  /* The remainder from the most recent split of a small request. */
  mchunkptr last_remainder;

  /* Normal bins packed as described above. */
  mchunkptr bins[NBINS * 2 - 2];

  /* Bitmap of bins. */
  unsigned int binmap[BINMAPSIZE];

  /* Linked list. */
  struct malloc_state *next;

  /* Linked list for free arenas. Access to this field 
     is serialized by free_list_lock in arena.c. */
  struct malloc_state *next_free;

  /* Number of threads attached to this arena. 0 if the 
     arena is on the free list. Access to this field is 
     serialized by free_list_lock in arena.c. */
  INTERNAL_SIZE_T attached_threads;

  /* Memory allocated from the system in this arena. */
  INTERNAL_SIZE_T system_mem;
  INTERNAL_SIZE_T max_system_mem;
};

struct malloc_par
{
  /* Tunable parameters */
  unsigned long trim_threshold;
  INTERNAL_SIZE_T top_pad;
  INTERNAL_SIZE_T mmap_threshold;
  INTERNAL_SIZE_T arena_test;
  INTERNAL_SIZE_T arena_max;

  /* Transparent Large Page support. */
  enum malloc_thp_mode_t thp_mode;
  INTERNAL_SIZE_T thp_pagesize;

  /* A value different than 0 means to align mmap 
     allocation to hp_pagesize add hp_flags on flags. */
  INTERNAL_SIZE_T hp_pagesize;
  int hp_flags;

  /* Memory map support. */
  int n_mmaps;
  int n_mmaps_max;
  int max_n_mmaps;

  /* The mmap_threshold is dynamic, until the user sets
     it manually, at which point we need to disable any
     dynamic behavior. */
  int no_dyn_threshold;

  /* Statistics */
  INTERNAL_SIZE_T mmapped_mem;
  INTERNAL_SIZE_T max_mmapped_mem;

  /* First address handed out by MORECORE/sbrk. */
  char *sbrk_base;

#if USE_TCACHE
  /* Maximum number of small buckets to use. */
  size_t tcache_small_bins;

  /* Maximum chunk size that the tcache bins manage. */
  size_t tcache_max_bytes;

  /* Maximum number of chunks in each bucket. */
  size_t tcache_count;

  /* Maximum number of chunks to remove from the 
     unsorted list, which aren't used to prefill 
     the cache. */
  size_t tcache_unsorted_limit;
#endif
};

/* There are several instances of this struct ("arenas") 
   in this malloc.

  [PRECONDITION]: malloc_state must be zero-initialized.
    If you are adapting this malloc in a way that does 
    NOT use a static or mmapped malloc_state, you MUST 
    explicitly zero-fill it before using.
*/
static struct malloc_state main_arena =
{
  .mutex = _LIBC_LOCK_INITIALIZER,
  .next  = &main_arena,
  .attached_threads = 1
};

/* There is only one instance of the malloc parameters. */
static struct malloc_par mp_ =
{
  .top_pad = DEFAULT_TOP_PAD,
  .n_mmaps_max = DEFAULT_MMAP_MAX,
  .mmap_threshold = DEFAULT_MMAP_THRESHOLD,
  .trim_threshold = DEFAULT_TRIM_THRESHOLD,

#define NARENAS_FROM_NCORES(n)  ((n) * (sizeof(long) == 4 ? 2 : 8))
  .arena_test = NARENAS_FROM_NCORES(1),
  .thp_mode = malloc_thp_mode_not_supported

#if USE_TCACHE
  ,
  .tcache_count = TCACHE_FILL_COUNT,
  .tcache_small_bins = TCACHE_SMALL_BINS,
  .tcache_max_bytes  = MAX_TCACHE_SMALL_SIZE + 1,
  .tcache_unsorted_limit = 0  /* No limit.  */
#endif
};

/* Initialize a malloc_state struct. It is called 
   from __ptmalloc_init() or from _int_new_arena() 
   while creating a new arena. */
static void malloc_init_state(mstate av)
{
  int i;
  mbinptr bin;

  /* Establish circular links for normal bins. */
  for (i = 1; i < NBINS; ++i){
    bin = bin_at(av, i);
    bin->fd = bin->bk = bin;
  }

#if MORECORE_CONTIGUOUS
  if (av != &main_arena)
#endif
    set_noncontiguous(av);

  av->top = initial_top(av);
}

/* Other internal utilities operating on mstates */

static void *sysmalloc(INTERNAL_SIZE_T, mstate);
static int   systrim(size_t, mstate);


/* ---------- Early definitions for debugging hooks ---------- */

/* This function is called from the arena shutdown 
   hook to free the thread cache (if it exists). */
static void tcache_thread_shutdown(void);

/* ------------------ Testing support ------------------ */

static int perturb_byte;

static void alloc_perturb(char *p, size_t n)
{
  if (__glibc_unlikely(perturb_byte))
    memset(p, perturb_byte ^ 0xff, n);
}

static void free_perturb(char *p, size_t n)
{
  if (__glibc_unlikely(perturb_byte))
    memset(p, perturb_byte, n);
}


#include <stap-probe.h>

/* ----------- Routines dealing with transparent huge pages ----------- */

static __always_inline void thp_init (void)
{
  /* Initialize only once if DEFAULT_THP_PAGESIZE is defined. */
  if (
    /* 0 in sysdeps/generic/malloc-hugepages.h. */
    DEFAULT_THP_PAGESIZE == 0 || 
    mp_.thp_mode != malloc_thp_mode_not_supported
  )
    return;

  /* Set thp_pagesize even if thp_mode is never.
     This reduces frequency of calls to MORECORE(). */
  mp_.thp_mode = __malloc_thp_mode();
  mp_.thp_pagesize = DEFAULT_THP_PAGESIZE;
}

static inline void madvise_thp(void *p, INTERNAL_SIZE_T size)
{
#ifdef MADV_HUGEPAGE
  thp_init();

  /* Only use __madvise if the system is using
     'madvise' mode and the size is at least a 
     huge page, otherwise the call is wasteful. */
  if (
    mp_.thp_mode != malloc_thp_mode_madvise || 
    size < mp_.thp_pagesize
  )
    return;

  /* Linux requires the input address to be 
     page-aligned, and unaligned inputs happens
     only for initial data segment. */
  if (__glibc_unlikely(!PTR_IS_ALIGNED(p, GLRO(dl_pagesize)))){
    void *q = PTR_ALIGN_UP(p, GLRO(dl_pagesize));
    size -= PTR_DIFF(q, p);
    p = q;
  }

  __madvise(p, size, MADV_HUGEPAGE);
#endif
}

/* ------------------- Support for multiple arenas -------------------- */
#include "arena.c"

/* Debugging support

  These routines make a number of assertions about the states
  of data structures that should be true at all times. If any
  are not true, it's very likely that a user program has
  somehow trashed memory. (It's also possible that there is a
  coding error in malloc. In which case, please report it!)

  By default, MALLOC_DEBUG is disabled, so these functions are 
  essentailly a no-op.
*/
#if !MALLOC_DEBUG

# define check_chunk(A, P)
# define check_free_chunk(A, P)
# define check_inuse_chunk(A, P)
# define check_malloced_chunk(A, P, N)
# define check_malloc_state(A)

#else

# define check_chunk(A, P)              do_check_chunk (A, P)
# define check_free_chunk(A, P)         do_check_free_chunk (A, P)
# define check_inuse_chunk(A, P)        do_check_inuse_chunk (A, P)
# define check_malloced_chunk(A, P, N)   do_check_malloced_chunk (A, P, N)
# define check_malloc_state(A)         do_check_malloc_state (A)

/* Properties of all chunks */
static void do_check_chunk(mstate av, mchunkptr p)
{
  unsigned long sz = chunksize(p);

  if (!chunk_is_mmapped(p)){
    /* min and max possible addresses assuming contiguous allocation */
    char *max_address = (char *) (av->top) + chunksize (av->top);
    char *min_address = max_address - av->system_mem;

    /* Has legal address ... */
    if (p != av->top){
      if (contiguous (av)){
        assert (((char *) p) >= min_address);
        assert (((char *) p + sz) <= ((char *) (av->top)));
      }
    }
    else{
      /* top size is always at least MINSIZE */
      assert((unsigned long)(sz) >= MINSIZE);

      /* top predecessor always marked inuse */
      assert(prev_inuse(p));
    }
  }
  else{
    /* chunk is page-aligned */
    assert ((mmap_size(p) & (GLRO(dl_pagesize) - 1)) == 0);

    /* mem is aligned */
    assert (!misaligned_chunk(p));
  }
}

/* Properties of free chunks */
static void do_check_free_chunk(mstate av, mchunkptr p)
{
  INTERNAL_SIZE_T sz = chunksize_nomask(p) & ~(PREV_INUSE | NON_MAIN_ARENA);
  mchunkptr next = chunk_at_offset(p, sz);

  do_check_chunk(av, p);

  /* Chunk must claim to be free ... */
  assert (!inuse(p));
  assert (!chunk_is_mmapped(p));

  /* Unless a special marker, must have OK fields */
  if ((unsigned long) (sz) >= MINSIZE){
    assert ((sz & MALLOC_ALIGN_MASK) == 0);
    assert (!misaligned_chunk (p));

    /* ... matching footer field */
    assert (prev_size (next_chunk (p)) == sz);

    /* ... and is fully consolidated */
    assert (prev_inuse (p));
    assert (next == av->top || inuse (next));

    /* ... and has minimally sane links */
    assert (p->fd->bk == p);
    assert (p->bk->fd == p);
  }

  /* markers are always of size SIZE_SZ */
  else
    assert (sz == SIZE_SZ);
}

/* Properties of inuse chunks */
static void do_check_inuse_chunk(mstate av, mchunkptr p)
{
  mchunkptr next;

  do_check_chunk(av, p);

  if (chunk_is_mmapped(p))
    return; /* mmapped chunks have no next/prev */

  /* Check whether it claims to be in use ... */
  assert(inuse(p));

  next = next_chunk(p);

  /* ... and is surrounded by OK chunks.
     Since more things can be checked with free chunks than inuse ones,
     if an inuse chunk borders them and debug is on, it's worth doing them.
   */
  if (!prev_inuse(p)){
    /* Note that we cannot even look at prev unless it is not inuse */
    mchunkptr prv = prev_chunk(p);
    assert (next_chunk(prv) == p);
    do_check_free_chunk(av, prv);
  }

  if (next == av->top){
    assert(prev_inuse(next));
    assert(chunksize(next) >= MINSIZE);
  }
  else if (!inuse(next))
    do_check_free_chunk(av, next);
}

/* Properties of chunks at the point they are malloced */
static void do_check_malloced_chunk(mstate av, mchunkptr p, INTERNAL_SIZE_T s)
{
  INTERNAL_SIZE_T sz = chunksize_nomask(p) & ~(PREV_INUSE | NON_MAIN_ARENA);

  if (!chunk_is_mmapped(p)){
    assert(av == arena_for_chunk(p));
    if (chunk_main_arena(p))
      assert(av == &main_arena);
    else
      assert(av != &main_arena);
  }

  do_check_inuse_chunkav, p);

  /* Legal size ... */
  assert ((sz & MALLOC_ALIGN_MASK) == 0);
  assert ((unsigned long) (sz) >= MINSIZE);

  /* ... and alignment */
  assert (!misaligned_chunk (p));

  /* chunk is less than MINSIZE more than request */
  assert ((long)(sz) - (long)(s) >= 0);
  assert ((long)(sz) - (long)(s + MINSIZE) < 0);

  /*
     ... plus,  must obey implementation invariant that prev_inuse is
     always true of any allocated chunk; i.e., that each allocated
     chunk borders either a previously allocated and still in-use
     chunk, or the base of its memory arena. This is ensured
     by making all allocations from the `lowest' part of any found
     chunk.
   */

  assert(prev_inuse(p));
}


/* Properties of malloc_state.

   This may be useful for debugging malloc, as well as detecting user
   programmer errors that somehow write into malloc_state.

   If you are extending or experimenting with this malloc, you can
   probably figure out how to hack this routine to print out or
   display chunk addresses, sizes, bins, and other instrumentation.
*/

static void do_check_malloc_state(mstate av)
{
  int i;
  mchunkptr p;
  mchunkptr q;
  mbinptr b;
  unsigned int idx;
  INTERNAL_SIZE_T size;
  unsigned long total = 0;

  /* internal size_t must be no wider than pointer type */
  assert (sizeof (INTERNAL_SIZE_T) <= sizeof (char *));

  /* alignment is a power of 2 */
  assert ((MALLOC_ALIGNMENT & (MALLOC_ALIGNMENT - 1)) == 0);

  /* Check the arena is initialized. */
  assert (av->top != 0);

  /* No memory has been allocated yet, so doing more tests is not possible.  */
  if (av->top == initial_top (av))
    return;

  /* pagesize is a power of 2 */
  assert (powerof2(GLRO (dl_pagesize)));

  /* A contiguous main_arena is consistent with sbrk_base.  */
  if (av == &main_arena && contiguous (av))
    assert ((char *) mp_.sbrk_base + av->system_mem ==
            (char *) av->top + chunksize (av->top));

  /* check normal bins */
  for (i = 1; i < NBINS; ++i)
    {
      b = bin_at (av, i);

      /* binmap is accurate (except for bin 1 == unsorted_chunks) */
      if (i >= 2)
        {
          unsigned int binbit = get_binmap (av, i);
          int empty = last (b) == b;
          if (!binbit)
            assert (empty);
          else if (!empty)
            assert (binbit);
        }

      for (p = last (b); p != b; p = p->bk)
        {
          /* each chunk claims to be free */
          do_check_free_chunk (av, p);
          size = chunksize (p);
          total += size;
          if (i >= 2)
            {
              /* chunk belongs in bin */
              idx = bin_index (size);
              assert (idx == i);
              /* lists are sorted */
              assert (p->bk == b ||
                      (unsigned long) chunksize (p->bk) >= (unsigned long) chunksize (p));

              if (!in_smallbin_range (size))
                {
                  if (p->fd_nextsize != NULL)
                    {
                      if (p->fd_nextsize == p)
                        assert (p->bk_nextsize == p);
                      else
                        {
                          if (p->fd_nextsize == first (b))
                            assert (chunksize (p) < chunksize (p->fd_nextsize));
                          else
                            assert (chunksize (p) > chunksize (p->fd_nextsize));

                          if (p == first (b))
                            assert (chunksize (p) > chunksize (p->bk_nextsize));
                          else
                            assert (chunksize (p) < chunksize (p->bk_nextsize));
                        }
                    }
                  else
                    assert (p->bk_nextsize == NULL);
                }
            }
          else if (!in_smallbin_range (size))
            assert (p->fd_nextsize == NULL && p->bk_nextsize == NULL);
          /* chunk is followed by a legal chain of inuse chunks */
          for (q = next_chunk (p);
               (q != av->top && inuse (q) &&
                (unsigned long) (chunksize (q)) >= MINSIZE);
               q = next_chunk (q))
            do_check_inuse_chunk (av, q);
        }
    }

  /* top chunk is OK */
  check_chunk (av, av->top);
}
#endif


/* ----------------- Support for debugging hooks -------------------- */

#if IS_IN (libc)
#include "hooks.c"
#endif


/* ----------- Routines dealing with system allocation -------------- */

/* Returns an mmapped chunk; used for large block 
   sizes or as a fallback when (av==NULL).

  [1] Round the size up to the nearest page.
  [2] Add padding if MALLOC_ALIGNMENT is larger than 
      CHUNK_HDR_SZ. (This is ideally not possible)
  [3] Add CHUNK_HDR_SZ at the end so that mmap chunks 
      have the same layout as regular chunks. [?]
*/
static void* sysmalloc_mmap(
  INTERNAL_SIZE_T nb, 
  size_t pagesize, 
  int extra_flags
){
  size_t padding = MALLOC_ALIGNMENT - CHUNK_HDR_SZ;
  /* Effectively 0, as both the macros have the same 
     values in all the three configurations. [GDB] */

  /* At least one page. */
  size_t size = ALIGN_UP(nb + padding + CHUNK_HDR_SZ, pagesize);

  char *mm = (char*) MMAP(
    NULL, 
    size,
		mtag_mmap_flags | PROT_READ | PROT_WRITE,
		extra_flags
  );

  if (mm == MAP_FAILED)
    return mm;

  if (extra_flags == 0)
    madvise_thp(mm, size);    /* [?] */

  /* [?] */
  __set_vma_name(mm, size, " glibc: malloc");

  /* Add malloc_chunk to the base of the newly mmapped segment */
  mchunkptr p = mmap_set_chunk(
    (uintptr_t)(mm), 
    size, 
    padding, 
    extra_flags != 0
  );

  /* No idea of the code below. */

  int new = atomic_fetch_add_relaxed(&mp_.n_mmaps, 1) + 1;
  atomic_max(&mp_.max_n_mmaps, new);

  unsigned long sum;
  sum = atomic_fetch_add_relaxed (&mp_.mmapped_mem, size) + size;
  atomic_max (&mp_.max_mmapped_mem, sum);

  check_chunk(NULL, p);
  return chunk2mem(p);
}

/* It returns an mmapped memory. It is used as a 
   fallback when sbrk can not be used. */
static void* sysmalloc_mmap_fallback(
  size_t *s, 
  size_t size, 
  size_t minsize,
  size_t pagesize, 
  int extra_flags
){
  size = ALIGN_UP(size, pagesize);

  /* If we are relying on mmap as backup, then use 
     larger units. */
  if (size < minsize)
    size = minsize;

  char *mbrk = (char*) MMAP(
    NULL, 
    size,
    mtag_mmap_flags | PROT_READ | PROT_WRITE,
    extra_flags
  );

  if (mbrk == MAP_FAILED)
    return MAP_FAILED;

  if (extra_flags == 0)
    madvise_thp(mbrk, size);

  /* The actual size that is mmapped is assigned to (s). */
  *s = size;
  return mbrk;
}

static void* sysmalloc(INTERNAL_SIZE_T nb, mstate av)
{
  mchunkptr old_top;               /* The top chunk in this arena. */
  INTERNAL_SIZE_T old_size;        /* Size of the top chunk. */
  char *old_end;                   /* End of the top chunk. */

  size_t  size;                    /* Arg to first MORECORE or mmap call. */
  char   *brk;                     /* The previous program break. */

  long correction;                 /* Arg to 2nd MORECORE call. */
  char *snd_brk;                   /* 2nd sbrk's return val. */

  INTERNAL_SIZE_T front_misalign;  /* Unusable bytes at front of new space. */
  INTERNAL_SIZE_T end_misalign;    /* Partial page left at end of new space. */
  char *aligned_brk;               /* Aligned offset into brk. */

  mchunkptr p;                     /* The allocated/returned chunk. */
  mchunkptr remainder;             /* Remainder from allocation. */
  unsigned long remainder_size;    /* The size of rmeainder. */


  size_t pagesize = GLRO(dl_pagesize);
  bool tried_mmap = false;


  /* [PATH 1]: Use sysmalloc_mmap if:
      [1] there are no usable arenas (the rare case), or

      [2A] the request size meets the mmap threshold, and
      [2B] the number of existing mmapped regions is less 
            than the maximum allowed.

      Large requests are generally serviced via mmap to 
      avoid consuming arena space. It allows the kernel 
      to reclaim a large mapping when it is freed, 
      keeping the memory footprint stable.
  */
  if (
    av == NULL ||
    (
      (unsigned long)(nb) >= (unsigned long)(mp_.mmap_threshold) &&
      (mp_.n_mmaps < mp_.n_mmaps_max)
    )
  ){
    char *mm;

    /* [PATH 1A]: Use huge pages if the requested size is more 
        than the huge page size and huge pages are enabled.

       We don't have to issue the THP madvise call. [WHY]
    */
    if (mp_.hp_pagesize > 0 && nb >= mp_.hp_pagesize){
      mm = sysmalloc_mmap(nb, mp_.hp_pagesize, mp_.hp_flags);
      if (mm != MAP_FAILED)
        return mm;
    }

    /* [PATH 1B]: Use standard page size. */
    mm = sysmalloc_mmap(nb, pagesize, 0);
    if (mm != MAP_FAILED)
      return mm;

    tried_mmap = true;
  }

  /* [FAIL SAFE PATH]: If there are no usable arenas and 
      mmap also failed, we can not do anything. */
  if (av == NULL)
    return NULL;


  /* [PATH 1] Analysis.

    [CASE 1] ~~ (av == NULL)
    - Path-1 is explored mandatorily.
    - Upon success, memory is returned.
    - Upon failure, the fail-safe path would return NULL.
    - Regardless of the outcome, "return" is confirmed. 
      If we are here, this was not the case.

    [CASE 2] ~~ (av != NULL), (nb >= mmap_threshold)

      [2A] ~~ (cur(mmap) < max(mmap)
      - nb belongs to [MMAP_THRESHOLD, PTRDIFF_MAX], which 
        is a huge range. Success depends on the kernel.
      - Upon success, memory is returned. If we are here, 
        path-1 has failed.
      - This can be the case.

      [2B] ~~ (cur(mmap) >= max(mmap)
      - We have reached the maximum number of mmapped 
        regions, so mmap can not be used.
      - This can be the case.

    [CASE 3] ~~ (av != NULL) and (nb < mmap_threshold)
    - If there was an arena and nb didn't cross the mmap 
      threshold, path-1 is inaccessible.
    - This can be the case.

    The kernel's willingness to service a request depends 
    on a variety of factors. Replicating that in user space 
    requires knowledge of the kernel's policies and current 
    state, which is either unavailable or become stale 
    quickly.

    In practice, modest requests often succeed, but it is 
    still an observed behavior. Therefore, the allocator 
    doesn't make assumptions based on size. 
    The most reliable method to know if the kernel will 
    fulfill a request is to ask the kernel.

    The virtual memory subsystem in Linux is the place 
    where these policies are written and enforced, making 
    it the right place to study this.
  */


  /* The current configuration of top. */
  old_top  = av->top;
  old_size = chunksize(old_top);
  old_end  = (char*) chunk_at_offset(old_top, old_size);

  /* Initialize the program breaks. */
  brk = snd_brk = (char*)(MORECORE_FAILURE);

  /* If it is the first time, the top chunk must point 
     to initial_top(av) and its size must be 0.

    If it is not the first time, 
    [1] the size of the top chunk (old_size) must be 
        at least MINSIZE bytes, 
    [2] the PREV_INUSE bit must be set (1), and 
    [3] the top chunk must end at a page aligned 
        boundary. 

    Why the top chunk must end at a page-aligned 
    boundary remains unanswered. See open-questions.md

    The preconditions are enforced with asserts(s) 
    which are compiled out in production builds. 
    Their are no if-blocks asking this is true in 
    production builds.

    The most important condition here is the third one. 
    Neither sysmalloc, nor _int_malloc checks this. It 
    remains unanswered.
    - Checkout this GDB experiment.
    - See open-questions.md
  */

  assert(
    /* First malloc. */
    (old_top == initial_top(av) && old_size == 0) ||
    /* nth malloc. */
    (
      (unsigned long)(old_size) >= MINSIZE &&
      prev_inuse(old_top) &&
      ((unsigned long)(old_end) & (pagesize-1)) == 0
    )
  );

  /* sysmalloc is all about extending the top chunk. 
     This requires the the top size to be less than 
     the required bytes + MINSIZE bytes.

    If the top size is equal to nb and it is used as 
    it is, it will cease to exist after the request 
    is served. Therefore, the top chunk must have at 
    least (nb + MINSIZE) bytes to remain valid 
    afterwards.

    It is checked separately by the pathways below.
  */
  assert((unsigned long)(old_size) < (unsigned long)(nb + MINSIZE));

  /* [PATH 2]: Non-main arena. */
  if (av != &main_arena){
    heap_info *old_heap;     /* Base of the old heap segment. */
    heap_info *heap;         /* Base of the new heap segment. */
    size_t old_heap_size;    /* Size of the existing heap segment. */

    old_heap = heap_for_ptr(old_top);
    old_heap_size = old_heap->size;


    /* [PATH 2A]: If the top chunk doesn't have enough 
        memory, call grow_heap() and extend it.

      There are three possibilities.
      [1] If top chunk doesn't have enough size, the result 
          is a positive value and grow_heap is called.
      [2] If top chunk had enough memory, growth is not 
          required.
      [3] If the final size exceeded PTRDIFF_MAX, the value 
          would be negative and the request is rejected.
    */
    if (
      (long)((MINSIZE + nb) - old_size) > 0 && 
      grow_heap(old_heap, MINSIZE + nb - old_size) == 0
    ){
      /* grow_heap will update the heap size. Subtracting 
         the old one from the new one will give us the 
         amount of new memory.
      */
      av->system_mem += (old_heap->size - old_heap_size);

      /* Update the size of the top chunk. */
      set_head(
        old_top, 
        (((char*)(old_heap) + old_heap->size) - (char*)(old_top)) | PREV_INUSE
      );
    }

    /* [PATH 2B]: If the current heap segment can't 
        be used, allocate a new heap segment.

      If the size exceeds HEAP_MAX_SIZE, the request 
      is rejected immediately. Otherwise, it depends 
      on the kernel.
    */

    /* [CONDITION BLOCK EXPLAINER]

      Request a new heap segment, assign the returned 
      pointer in `heap` and enter the branch if the 
      returned value is not NULL.
    */
    else if (
      (heap = new_heap(nb + (MINSIZE + sizeof(*heap)), mp_.top_pad))
    ){
      /* Attach this heap to the arena. */
      heap->ar_ptr = av;

      /* Add this heap to the linked list of heaps managed 
         by this arena. */
      heap->prev = old_heap;

      /* Update the memory footprint. */
      av->system_mem += heap->size;

      /* Establish the top chunk in the new heap segment 
         and update av->top to it.

        Currently, the top chunk owns all the memory in 
        this heap, except the initial bytes, which are 
        used for heap_info.
      */
      top(av) = chunk_at_offset(heap, sizeof(*heap));
      set_head(
        top(av), 
        (heap->size - sizeof(*heap)) | PREV_INUSE
      );

      /* Setup two fencepost chunks at the end of the old 
         heap segment and regularize the top chunk in it.

        MINSIZE bytes are required to setup two fenceposts. 
        They are taken from the existing top chunk and the 
        remaining top bytes are aligned to a 
        MALLOC_ALIGNMENT boundary.

        The allocator maintains the invariant that the top 
        chunk always has at least MINSIZE bytes. Therefore, 
        we don't have to worry about the subtraction.

        There are two unanswered questions here. See 
        open-questions.md
        [1] Why the remaining top size is aligned down when 
            we maintain the invariant that the top chunk 
            always ends at page-aligned boundaries?
        [2] In a rare situation, if the top was misaligned, 
            aligning down would create some bytes that do 
            not belong to the top chunk. What happens to 
            them? They can not be carried by the fenceposts 
            as it would disturb their alignment.
      */

      old_size = (old_size - MINSIZE) & ~MALLOC_ALIGN_MASK;

      /* Fencepost-2. */
      set_head(
        chunk_at_offset(old_top, old_size + CHUNK_HDR_SZ),
		    0 | PREV_INUSE
      );

      /* There are three possibilities based on what was 
         in old_size before subtracting space for the 
         fenceposts.

        [CASE 1] ~~ (old_size == MINSIZE)
        - After subtracting, old_top will vanish completely.
        - There is nothing to regularize.

        [CASE 2] ~~ (old_size >= 2*MINSIZE)
        - After subtracting, old_top is still a valid chunk, 
          a small chunk.
        - We can regularize the top chunk here.

        [CASE 3] ~~ (old_size == (MINSIZE + CHUNK_HDR_SZ))
        - After subtracting, old_size will be CHUNK_HDR_SZ.
        - Top chunk is no longer a valid chunk, so there is 
          nothing to regularize. The remaining bytes are 
          carried away by fencepost-1. Since they are 
          CHUNK_HDR_SZ, they don't disturb the alignment of 
          the fenceposts.
      */

      /* If the old top chunk has enough space to exist, 
         regularize it. */
      if (old_size >= MINSIZE){
        /* Setup fencepost-1. */
        /* The top chunk is kept as an in-use chunk. Until it 
           is regularized, it must be kept as an in-use chunk.
        */
        set_head(
          chunk_at_offset(old_top, old_size),
          CHUNK_HDR_SZ | PREV_INUSE
        );

        /* Set the mchunk_prev_size of fencepost-2 to the size 
           of fencepost-1. */
        set_foot(
          chunk_at_offset(old_top, old_size), 
          CHUNK_HDR_SZ
        );

        /* Update the size and the lower bits of the top chunk. */
        set_head(
          old_top, 
          old_size | PREV_INUSE | NON_MAIN_ARENA
        );

        /* Regularize the top chunk of the old heap and bin it. */
        _int_free_chunk(av, old_top, chunksize(old_top), 1);
      }

      /* If (old_size == CHUNK_HDR_SZ) */
      else{
        /* Fencepost-1 */
        set_head (old_top, (old_size + CHUNK_HDR_SZ) | PREV_INUSE);

        /* Fencepost-2 updated with the size of fencepost-1. */
        set_foot (old_top, (old_size + CHUNK_HDR_SZ));
      }
    }


    /* The current heap can not be extended and a new heap 
       can't be setup. This leaves us with the only option, 
       i.e. an mmapped chunk.
    */

    /* [PATH 2C]: Use sysmalloc_mmap to get an mmaped chunk.

      new_heap has already tried huge pages and failed. So 
      it is unlikely that another attempt would succeed.
      Therefore, we attempt with standard page size only.
    */
    else if (!tried_mmap){
      char *mm = sysmalloc_mmap(nb, pagesize, 0);
      if (mm != MAP_FAILED)
      return mm;
    }
  }


  /* [PATH 3]: Main arena. */
  else{
    /* [STEP 1]: Calculate the size to request from sbrk. */

    size = nb + mp_.top_pad + MINSIZE;

    /* [?]
      If not contiguous already, we don't subtract old_size 
      from size. [WHY]

      If contiguous, we do subtract, but we wait until sbrk 
      actually return contiguous memory. If it doesn't, we 
      undo this step.
    */
    if (contiguous(av))
      size -= old_size;

    thp_init();

    /* If huge pages are enabled, `size` is aligned up to 
       the next multiple of the huge page size. Otherwise, 
       use standard page size.
    */
    if (__glibc_unlikely (mp_.thp_pagesize != 0)){
      /* The current program break. */
      uintptr_t lastbrk = (uintptr_t) MORECORE(0);

      /* The new program break after aligning size to 
         a huge page. */
      uintptr_t top = ALIGN_UP(lastbrk + size, mp_.thp_pagesize);

      /* The final aligned size. */
      size = (top - lastbrk);
    }
    else{
      size = ALIGN_UP(size, GLRO(dl_pagesize));
    }
    /* [STEP 1] Completed. */


    /* [STEP 2]: Get new memory. */

    /* [PATH 3A]: Call sbrk. */

    /* `size` is an unsigned quantity, but sbrk takes a 
       signed quantity. Therefore, we interpret size as 
       a signed value to ensure it represents a positive 
       value.
    */
    if ((ssize_t)(size) > 0){
      brk = (char*) MORECORE((long)(size));
      if (brk != (char*)(MORECORE_FAILURE))
        madvise_thp(brk, size);

      LIBC_PROBE (memory_sbrk_more, 2, brk, size);
    }

    /* [PATH 3A Analysis]

      A successful sbrk() indicates that "the program 
      break has been moved successfully". Whether the 
      expansion was contiguous as per the allocator's 
      bookkeeping is something the allocator has to 
      find itself.

      A failed sbrk() indicates that "sbrk can not be 
      used to satisfy this request". The user space has 
      no realiable means to know why the kernel refused 
      the request. Maybe the request exceeded the maximum 
      limit for the data segment (RLIMIT_DATA), or there 
      is a "hole" in the address space that is preventing 
      contiguous growth.

      What is a "hole" in the address space? It is the 
      second threat to sbrk's contiguity.
      - Suppose our program break is at 0x10000 (65536).
      - The next mapping is at 0x14000 (81920).
      - The difference between the program break and this 
        mapping is 0x4000 bytes (16384), i.e. four 4-KiB 
        pages.
      - As long as the expansion fits under these four 
        pages, contiguous growth is not objected. However, 
        the kernel will refuse any extension beyond this.
      - Therefore, a hole is an unmapped gap between two 
        VMAs. Such gaps are a normal consequence of the 
        kernel managing different types of mappings within 
        a process's virtual address space. But it becomes 
        a limitation when the requested program break 
        extension is larger than the contiguous unmapped 
        space available. 
    */


    /* [PATH 3B]: Use sysmalloc_mmap_fallback if path-3a 
        failed.

      Since memory can not be obtained via sbrk for this 
      request, we try mmap. We ignore the mmap_threshold 
      and max mmapped regions count as it is not used as 
      a standalone mmapped chunk.
    */

    if (brk == (char*)(MORECORE_FAILURE)){
      /* Size to request. The actual size (after alignment) 
         is assigned into `size`. */
      size_t fallback_size = nb + mp_.top_pad + MINSIZE;

      /* mbrk is probably "mmap returned break"! */
      char *mbrk = MAP_FAILED;

      /* [PATH (3B, 1)]: Use huge pages if enabled. */
      if (mp_.hp_pagesize > 0){
        mbrk = sysmalloc_mmap_fallback(
          &size, fallback_size,
          mp_.hp_pagesize,
          mp_.hp_pagesize, mp_.hp_flags
        );
      }

      /* [EXPLAIN MMAP_AS_MORECORE_SIZE] */
      /* [PATH (3B, 2)]: Use standard page size if huge 
          pages were not enabled, or that path failed. */
      if (mbrk == MAP_FAILED){
        mbrk = sysmalloc_mmap_fallback(
          &size, fallback_size,
          MMAP_AS_MORECORE_SIZE,
          pagesize, 0
        );
      }

      /* If mmap succeeded, we can not use sbrk to find 
         the end. Therefore, we have to update brk and 
         snd_brk appropriately. */
      if (mbrk != MAP_FAILED){
        /* [?] */
        __set_vma_name(mbrk, fallback_size, " glibc: malloc");

        /* [REVISIT] */
        /* The allocator no longer assumes future sbrk 
           growth will be contiguous. After the first 
           time mmap is used as backup, we do not ever 
           rely on contiguous space as this could 
           incorrectly bridge the regions.
        */

        /* Update the NONCONTIGUOUS_BIT. */
        set_noncontiguous(av);

        /* The start of the mmapped memory. */
        brk = mbrk;

        /* The end of the mmapped memory. */
        snd_brk = brk + size;
      }
    }

    /* [PATH 3B ANALYSIS]

      If this path has failed, we have exhausted all the 
      avenues and this request can not be served. The rest 
      of the code is essentially a no-op. errno is set and 
      NULL is returned in the end.

      If this path has succeeded, we have an mmapped region.
    */

    /* [STEP 2] Completed. All the avenues are checked. */


    /* [STEP 3]: Assess which path has succeeded (if any) 
        and operate accordingly. */

    /* If any of the path succeeded, brk will contain a 
       valid pointer. */
    if (brk != (char*)(MORECORE_FAILURE)){
      /* If malloc is called for the first time, store 
         the base program break. [WHY] */
      if (mp_.sbrk_base == NULL)
        mp_.sbrk_base = brk;

      /* Update the total memory the arena is managing. */
      av->system_mem += size;

      /* [Path 3A] has succeeded and no foreign sbrk is 
          detected if
          [1] old_end and brk have the same address, and 
          [2] snd_brk is still MORECORE_FAILURE.

          That means, program break extension is contiguous 
          with the allocator's bookkeeping and the top chunk 
          can be safely extended. */
      if (
        brk == old_end && 
        snd_brk == (char*)(MORECORE_FAILURE)
      )
        set_head(old_top, (size + old_size) | PREV_INUSE);


      /* [Path 3A] has succeeded but a negative foreign 
          sbrk is detected if 
          [1] the program break extension was contiguous 
              so far, 
          [2] old_size is non-zero (top chunk exists), and 
          [3] the program break returned by sbrk is behind 
              the current top end.

          In this situation, the allocator's state is 
          corrupted and we simply terminate the process. */
      else if (
        contiguous(av) && 
        old_size && 
        brk < old_end
      )
        malloc_printerr ("break adjusted to free malloc space");


      /* Otherwise, make adjustments:

        * If the first time through or noncontiguous, we need to call sbrk
          just to find out where the end of memory lies.

        * We need to ensure that all returned chunks from malloc will meet
          MALLOC_ALIGNMENT

        * If there was an intervening foreign sbrk, we need to adjust sbrk
          request size to account for fact that we will not be able to
          combine new space with existing space in old_top.

        * Almost all systems internally allocate whole pages at a time, in
          which case we might as well use the whole last page of request.
          So we allocate enough more memory to hit a page boundary now,
          which in turn causes future contiguous calls to page-align.
      */

      /* If we are here, either a positive foreign sbrk 
         is detected, or a hole in the address space. */
      else{
        front_misalign = 0;
        end_misalign   = 0;
        correction  = 0;
        aligned_brk = brk;    /* Initialize with the previous 
                                 program break (the end of the 
                                 foreign sbrk region). */

        /* If program break was contiguous so far and foreign 
           sbrk is detected for the first time. */
        if (contiguous(av)){
          /* Because the foreign sbrk is made by the process 
             only, the allocator counts it in the system memory 
             as well. But the gap is not treated as usable 
             allocator space.

            The foreign sbrk would have advanced the program 
            break. When the allocator would call sbrk again, 
            a successful call would have returned the old 
            program break, which is the end of the foreign 
            sbrk memory. When we subtract it from the old_end, 
            which is the program break before the foreign sbrk 
            was made, we get the number of bytes the foreign 
            sbrk was called with. */
          if (old_size)
            av->system_mem += (brk - old_end);


          /* While the first sbrk call has already requested 
             enough space to service the request, the second 
             sbrk call is about restoring the top chunk. So, 
             calculate the new bytes and store them inside 
             `correction` */

          /* The size of the foreign sbrk might not be aligned 
             at a MALLOC_ALIGNMENT boundary, which is necessary 
             for maintaining chunk integrity.

             To tackle this, we have to move brk to the next 
             MALLOC_ALIGNMENT boundary.
             - We calculate how many bytes brk is misaligned from 
               the previous alignment boundary. This is called 
               front_misalign.
             - We subtract front_misalign from MALLOC_ALIGNMENT to 
               obtain the number of bytes brk is misaligned from 
               the next boundary. The result is called `correction` 
               bytes.
             - Add correction to brk, we get the aligned_brk. 
               Remember, it is aligned to a MALLOC_ALIGNMENT boundary, 
               not a page boundary. It is easy to confuse as page 
               alignment is what we are dealing with in malloc.
             - For example, on 64-bit, if brk is 100, the previous 
               MALLOC_ALIGNMENT boundary is 96. So, brk is 4 bytes 
               front_misaligned.

            [NOTE]: I don't understand why a complicated method is 
             used. (brk + 2*SIZE_SZ) will be as much misaligned as 
             `brk` is. Take brk=100.
              - `brk + (2*SIZE_SZ)`. is (100 + 16) on 64-bit. And 
                (116 & 15) is 4.
              - If we do `brk & MALLOC_ALIGN_MASK` directly, we get, 
                (100 & 15), i.e. 4. Exactly same output. */

          front_misalign = (INTERNAL_SIZE_T) chunk2mem(brk) & MALLOC_ALIGN_MASK;
          if (front_misalign > 0){
            correction = MALLOC_ALIGNMENT - front_misalign;
            aligned_brk += correction;
          }

          /* The new top will have the same space as the 
             existing one. */
          correction += old_size;

          /* (brk) is the address marking the end of the foreign 
              sbrk extension.
             (brk + size) brings us to the current program break.
             (brk + size + correction) is where we will end up 
              after adding correction to the current program break. 
              The result is stored in end_misalign. */
          end_misalign = (INTERNAL_SIZE_T) (brk + size + correction);

          /* Align end_misalign to a standard page boundary and 
             subtract end_misalign from it, we get the number 
             of bytes end_misalign is far from the next "page 
             boundary". Add this to correction to obtain the 
             actual page aligned bytes to call sbrk with. */
          correction += (ALIGN_UP(end_misalign, pagesize)) - end_misalign;
          assert(correction >= 0);

          /* Call sbrk with the new size. */
          /* correction = (
               ALIGN_UP(
                 ((MALLOC_ALIGNMENT - front_misalign) +
                 old_size +
                 foreign_sbrk_size),
                 pagesize
               ) - 
               foreign_sbrk_size
             ) */
          snd_brk = (char*) MORECORE(correction);

          /* If can't allocate correction, try to at least 
             find out current brk. It might be enough to 
             proceed without failing.

             Note that if second sbrk did NOT fail, we assume 
             that space is contiguous with first sbrk. This is 
             a safe assumption unless program is multithreaded 
             but doesn't use locks and a foreign sbrk occurred 
             between our first and second calls. */
          if (snd_brk == (char*)(MORECORE_FAILURE)){
            correction = 0;    // Reset correction
            snd_brk = (char*) MORECORE(0);
          }
          else
            madvise_thp (snd_brk, correction);
        }

        /* The arena has already been marked as non-contiguous 
           by previous malloc calls. */
        else{
          /* This is always true on on 32-bit, 64-bit and 
             INTERNAL_SIZE_T=4. So, the else block is 
             effectively dead code. */
          if (MALLOC_ALIGNMENT == CHUNK_HDR_SZ){
            /* MORECORE/mmap must correctly align */
            assert (((unsigned long) chunk2mem(brk) & MALLOC_ALIGN_MASK) == 0);
          }
          else{
            front_misalign = (INTERNAL_SIZE_T) chunk2mem(brk) & MALLOC_ALIGN_MASK;
            if (front_misalign > 0){
              /* Skip over some bytes to arrive at an aligned 
                 position. We don't need to specially mark these 
                 wasted front bytes. They will never be accessed 
                 anyway because prev_inuse of av->top (and any 
                 chunk created from its start) is always true 
                 after initialization. */
              aligned_brk += (MALLOC_ALIGNMENT - front_misalign);
            }
          }

          /* Find the current end of the memory. */
          if (snd_brk == (char*)(MORECORE_FAILURE)){
            snd_brk = (char*) MORECORE(0);
          }
        }

        /* Adjust the top chunk based on the second sbrk. */
        if (snd_brk != (char*) (MORECORE_FAILURE)){
          /* Setup the new top chunk starting from aligned_brk. 
             The new top chunk starts from aligned_brk and 
             contains both the `size` memory and `correction` 
             memory. */
          av->top = (mchunkptr)(aligned_brk);
          set_head(
            av->top, 
            (snd_brk - aligned_brk + correction) | PREV_INUSE
          );

          /* Update the arena's total memory with correction bytes. */
          av->system_mem += correction;

          /* Insert double fencepost. 
             A valid top chunk already has at least MINSIZE 
             bytes, which is enough for two fenceposts.

             [NOTE]: If top size was (MINSIZE + MALLOC_ALIGNMENT),
              like, 48 bytes on 64-bit, the old_top is not really 
              a valid chunk anymore. */
          if (old_size != 0){
            old_size = (old_size - 2 * CHUNK_HDR_SZ) & ~MALLOC_ALIGN_MASK;
            set_head (old_top, old_size | PREV_INUSE);

            /* Fencepost-1 */
            set_head(
              chunk_at_offset(old_top, old_size),
              CHUNK_HDR_SZ | PREV_INUSE
            );

            /* Fencepost-2 */
            set_head(
              chunk_at_offset(old_top, old_size + CHUNK_HDR_SZ),
              CHUNK_HDR_SZ | PREV_INUSE
            );

            /* Regularize the old top and bin it to be used as a 
               normal chunk. */
            if (old_size >= MINSIZE){
              _int_free_chunk(av, old_top, chunksize(old_top), 1);
            }
          }
        }
      }
    }
  }

  /* Update max_system_mem if applicable. */
  if ((unsigned long)(av->system_mem) > (unsigned long)(av->max_system_mem))
    av->max_system_mem = av->system_mem;

  check_malloc_state(av);

  /* The existing top configuration. */
  p = av->top;
  size = chunksize(p);

  /* If an allocation pathway succeeded, the top chunk will 
     have memory now. */
  if ((unsigned long)(size) >= (unsigned long)(nb + MINSIZE)){
    remainder_size = (size - nb);
    remainder = chunk_at_offset(p, nb);
    av->top = remainder;

    /* The chunk to be returned. */
    set_head(p, nb | PREV_INUSE | (av != &main_arena ? NON_MAIN_ARENA : 0));

    /* The top chunk. */
    set_head(remainder, remainder_size | PREV_INUSE);

    check_malloced_chunk(av, p, nb);
    return chunk2mem(p);
  }

  /* If none of the pathways succeeded. */
  __set_errno(ENOMEM);
  return NULL;
}


/* systrim is an inverse of sorts to sysmalloc. It gives 
   memory back to the system (via negative arguments to 
   sbrk) if there is unused memory at the `high' end of 
   the malloc pool.

  It is called automatically by free() when top space 
  exceeds the trim threshold. It is also called by the 
  public malloc_trim routine.

  It returns 1 if it has actually released any memory, 
  else 0.
*/
static int systrim(size_t pad, mstate av)
{
  long  top_size;        /* Amount of memory in the top chunk. */
  long  extra;           /* Amount to release. */
  long  released;        /* Amount actually released. */
  char* current_brk;     /* Address returned by pre-check sbrk call. */
  char* new_brk;         /* Address returned by post-check sbrk call. */
  long  top_area;

  /* Current top size. */
  top_size = chunksize(av->top);

  /* Subtracting MINSIZE from the existing top size, 
     we get the actual amount of bytes that can be 
     trimmed without eliminating the top chunk itself. */
  top_area = (top_size - MINSIZE - 1);

  /* If the bytes available to trim are less than 
     the bytes to keep in the top chunk, return. */
  if (top_area <= pad)
    return 0;

  /* Align top_arena down to the previous huge page or 
     standard page. */
  if (__glibc_unlikely (mp_.thp_pagesize != 0))
    extra = ALIGN_DOWN (top_area - pad, mp_.thp_pagesize);
  else
    extra = ALIGN_DOWN (top_area - pad, GLRO(dl_pagesize));

  /* If the aligned bytes are 0, there is nothing to trim. */
  if (extra == 0)
    return 0;

  /* Only proceed if the end of memory is where 
     we have set it last. This avoids problems 
     if there were foreign sbrk calls. */
  current_brk = (char*) MORECORE(0);
  if (current_brk == (char*)(av->top) + top_size){

    /* Attempt to release memory. We ignore the MORECORE 
       return value and instead call again to find out 
       the new end of memory. This avoids problems if the 
       first call releases less than we asked, of if 
       failure somehow altered brk value. We could still
       encounter problems if it altered brk in some very 
       bad way, but the only thing we can do is adjust 
       anyway, which will cause some downstream failure.
    */

    MORECORE(-extra);
    new_brk = (char*) MORECORE(0);
    LIBC_PROBE(memory_sbrk_less, 2, new_brk, extra);

    if (new_brk != (char*)(MORECORE_FAILURE)){
      /* A defensive check. */
      released = (long)(current_brk - new_brk);

      /* Success. Adjust top. */
      if (released != 0){
        av->system_mem -= released;
        set_head(av->top, (top_size - released) | PREV_INUSE);
        check_malloc_state(av);
        return 1;
      }
    }
  }
  return 0;
}

static void munmap_chunk(mchunkptr p)
{
  size_t pagesize = GLRO(dl_pagesize);

  assert(chunk_is_mmapped(p));

  uintptr_t mem = (uintptr_t) chunk2mem(p);
  uintptr_t block = mmap_base(p);
  size_t total_size = mmap_size(p);

  /* Unfortunately we have to do the compilers job by hand here.
     Normally we would test BLOCK and TOTAL-SIZE separately for 
     compliance with the page size.  But gcc does not recognize 
     the optimization possibility (in the moment at least) so we 
     combine the two values into one before the bit test.
  */
  if (
    __glibc_unlikely((block | total_size) & (pagesize - 1)) != 0 ||
    __glibc_unlikely(!powerof2(mem & (pagesize - 1)))
  )
    malloc_printerr ("munmap_chunk(): invalid pointer");

  atomic_fetch_add_relaxed (&mp_.n_mmaps, -1);
  atomic_fetch_add_relaxed (&mp_.mmapped_mem, -total_size);

  /* If munmap failed the process virtual memory address space 
     is in a bad shape. Just leave the block hanging around, 
     the process will terminate shortly anyway since not much 
     can be done.
  */
  __munmap ((char*)(block), total_size);
}

#if HAVE_MREMAP

static mchunkptr mremap_chunk (mchunkptr p, size_t new_size)
{
  bool is_hp = mmap_is_hp(p);
  size_t pagesize = is_hp ? mp_.hp_pagesize : GLRO(dl_pagesize);

  INTERNAL_SIZE_T offset = mmap_base_offset(p);
  INTERNAL_SIZE_T size = chunksize(p);

  char *cp;
  assert(chunk_is_mmapped(p));

  uintptr_t block = mmap_base(p);
  uintptr_t mem = (uintptr_t) chunk2mem(p);
  size_t total_size = mmap_size(p);

  if (
    __glibc_unlikely ((block | total_size) & (pagesize - 1)) != 0 || 
    __glibc_unlikely (!powerof2 (mem & (pagesize - 1)))
  )
    malloc_printerr("mremap_chunk(): invalid pointer");

  /* Note the extra CHUNK_HDR_SZ overhead as in mmap_chunk(). */
  new_size = ALIGN_UP(new_size + offset + CHUNK_HDR_SZ, pagesize);

  /* No need to remap if the number of pages does not change. */
  if (total_size == new_size)
    return p;

  cp = (char*) __mremap(
                (char*)(block), 
                total_size, 
                new_size,
                MREMAP_MAYMOVE
              );

  if (cp == MAP_FAILED)
    return NULL;

  /* mremap preserves the region's flags - this means that 
     if the old chunk was marked with MADV_HUGEPAGE, the 
     new chunk will retain that. */
  if (total_size < mp_.thp_pagesize)
    madvise_thp (cp, new_size);

  p = mmap_set_chunk ((uintptr_t)(cp), new_size, offset, is_hp);

  INTERNAL_SIZE_T new;
  new = atomic_fetch_add_relaxed (&mp_.mmapped_mem, new_size - size - offset)
        + new_size - size - offset;
  atomic_max (&mp_.max_mmapped_mem, new);
  return p;
}
#endif /* HAVE_MREMAP */

/* ---------------- Public wrappers ---------------- */

#if USE_TCACHE

/* We overlay this structure on the user-data portion 
   of a chunk when the chunk is stored in the per-thread 
   cache. */
typedef struct tcache_entry
{
  struct tcache_entry *next;

  /* This field exists to detect double frees. */
  uintptr_t key;
} tcache_entry;

/* There is one of these for each thread, which contains 
   the per-thread cache (hence "tcache_perthread_struct").
   - Keeping overall size low is mildly important.
   - The 'entries' field is a linked list of free blocks, 
     while 'num_slots' contains the number of free blocks 
     that can be added. Each bin may allow a different 
     maximum number of free blocks, and can be disabled by 
     initializing 'num_slots' to zero.
*/
typedef struct tcache_perthread_struct
{
  /* Max number of chunks the corresponding 
     tcache bin can hold. */
  uint16_t      num_slots[TCACHE_MAX_BINS];

  /* Array of tcache bins. */
  tcache_entry* entries[TCACHE_MAX_BINS];
} tcache_perthread_struct;

static const union
{
  struct tcache_perthread_struct inactive;
  struct
  {
    char pad;
    struct tcache_perthread_struct disabled;
  };
} __tcache_dummy;

/* TCACHE is never NULL; it's either "live" or points 
   to one of the above dummy entries. The dummy 
   entries are all zero so act like an empty/unusable 
   tcache. */
static __thread tcache_perthread_struct *tcache =
  (tcache_perthread_struct *) &__tcache_dummy.inactive;

/* This is the default, and means "check to see if a 
   real tcache should be allocated."  */
static __always_inline bool
tcache_inactive (void)
{
  return (tcache == &__tcache_dummy.inactive);
}

/* This means "the user has disabled the tcache but we 
   have to point to something."  */
static __always_inline bool
tcache_disabled (void)
{
  return (tcache == &__tcache_dummy.disabled);
}

/* This means the tcache is active. */
static __always_inline bool
tcache_enabled (void)
{
  return (!tcache_inactive() && !tcache_disabled());
}

/* Sets the tcache to DISABLED state. */
static __always_inline void
tcache_set_disabled (void)
{
  tcache = (tcache_perthread_struct*)(&__tcache_dummy.disabled);
}

/* Process-wide key to try and catch a double-free 
   in the same thread. */
static uintptr_t tcache_key;

/* The value of tcache_key does not really have to 
   be a cryptographically secure random number. It 
   only needs to be arbitrary enough so that it does
   not collide with values present in applications.
   - If a collision does happen consistently enough, 
     it could cause a degradation in performance since 
     the entire list is checked to check if the block 
     indeed has been freed the second time.
   - The odds of this happening are exceedingly low 
     though, about 1 in 2^wordsize. There is probably 
     a higher chance of the performance degradation 
     being due to a double free where the first free 
     happened in a different thread; that's a case 
     this check does not cover. */
static void tcache_key_initialize(void)
{
  /* We need to use the _nostatus version here, 
  see BZ 29624.  */
  if (__getrandom_nocancel_nostatus_direct(
      &tcache_key, 
      sizeof(tcache_key),
      GRND_NONBLOCK
    ) != sizeof (tcache_key)
  )
    tcache_key = 0;

  /* We need tcache_key to be non-zero (otherwise 
     tcache_double_free_verify's clearing of e->key 
     would go unnoticed and it would loop getting 
     called through __libc_free), and we want 
     tcache_key not to be a commonly-occurring value 
     in memory, so ensure a minimum amount of one and
     zero bits. */
  int minimum_bits = __WORDSIZE / 4;
  int maximum_bits = __WORDSIZE - minimum_bits;

  while (
    tcache_key <= 0x1000000 || 
    tcache_key >= ((uintptr_t)(ULONG_MAX)) - 0x1000000 || 
    stdc_count_ones(tcache_key) < minimum_bits || 
    stdc_count_ones(tcache_key) > maximum_bits
  ){
    tcache_key = random_bits();
#if __WORDSIZE == 64
    tcache_key = (tcache_key << 32) | random_bits();
#endif
  }
}

/* The tcache bin number corresponding to a 
   large size. */
static __always_inline size_t
large_csize2tidx(size_t nb)
{
  /* Just like normal large bins, tcache large bins are 
     also logarithmically spaced. 

    __builtin_clz is a built-in compiler intrinsic in GCC, 
    which is used to count the number of leading zero bits 
    in an unsigned integer, starting from the most significant 
    bit. For example, __builtin_clz(1024) will be 53 because 
    the first set bit from MSB is the 54th bit.

    Because TCACHE_SMALL_BINS and MAX_TCACHE_SMALL_SIZE are 
    known values, they are effectively constants, making the 
    calculation: 
      => idx = 64 + 53 - __builtin_clz(nb)
      => idx = 117 - __builtin_clz(nb) 

    The count of leading zero changes only when the value 
    crosses a power-of-2. For example, 1024 is a power-of-2 
    value. The next power-of-2 value is 2048. All the sizes 
    in [1024, 2048) will have the same count of leading 0s, 
    i.e. 53. That effectively forms the range of a large 
    tcache bin. Because the MAX_TCACHE_SMALL_SIZE is 1040, 
    the first tcache large bin has the range [1056, 2048) 
    as csize2tidx(1040) will produce 63, which is within 
    the tcache small bin count, i.e. [0, 63]. */
  size_t idx = TCACHE_SMALL_BINS
      	       + __builtin_clz(MAX_TCACHE_SMALL_SIZE)
      	       - __builtin_clz(nb);
  return idx;
}

/* Put a chunk in a tcache bin, not necessarily on the 
   head. The caller must ensure that tc_idx is valid 
   and there's room for more chunks. */
static __always_inline void
tcache_put_n(
  mchunkptr chunk, 
  size_t tc_idx, 
  tcache_entry **ep, 
  bool mangled
){
  tcache_entry *e = (tcache_entry*) chunk2mem(chunk);

  /* Mark this chunk as "in the tcache" so the test 
     in __libc_free will detect a double free. */
  e->key = tcache_key;

  /* If not mangled, *ep is a plain pointer, like 
     the start of the tcache bin. Example:
     ep = &tcache->entries[tc_idx];

     In this case, it can be used directly. */
  if (!mangled){
    e->next = PROTECT_PTR(&e->next, *ep);
    *ep = e;
  }

  /* If mangled, *ep is a safe link pointer, so 
     we have to use REVEAL_PTR(*ep) and later 
     PROTECT_PTR while storing the updated link. */
  else{
    e->next = PROTECT_PTR(&e->next, REVEAL_PTR(*ep));
    *ep = PROTECT_PTR(ep, e);
  }

  /* Because a chunk is added, reduce the available 
     slots for this tcache bin. */
  --(tcache->num_slots[tc_idx]);
}

/* Get a chunk out of a tcache bin. The caller must 
   ensure that know tc_idx is valid and there are 
   chunks available to remove. Removes chunk from 
   the middle of the list. */
static __always_inline void*
tcache_get_n (size_t tc_idx, tcache_entry **ep, bool mangled)
{
  tcache_entry *e;

  /* Store the chunk in `e`. */
  if (!mangled)
    e = *ep;
  else
    e = REVEAL_PTR(*ep);

  if (__glibc_unlikely(misaligned_mem(e)))
    malloc_printerr ("malloc(): unaligned tcache chunk detected");

  /* Update the list. */
  if (!mangled)
    *ep = REVEAL_PTR(e->next);
  else
    *ep = PROTECT_PTR(ep, REVEAL_PTR(e->next));

  ++(tcache->num_slots[tc_idx]);
  e->key = 0;

  return (void*)(e);
}

static __always_inline void
tcache_put (mchunkptr chunk, size_t tc_idx)
{
  tcache_put_n (chunk, tc_idx, &tcache->entries[tc_idx], false);
}

/* Like the above, but removes from the head of the list. */
static __always_inline void*
tcache_get (size_t tc_idx)
{
  return tcache_get_n (tc_idx, &tcache->entries[tc_idx], false);
}

/* SOMTHING ALONG THE LINES. */
/* Finds the chunk to take or push a new chunk at. */
static __always_inline tcache_entry**
tcache_location_large(
  size_t nb, size_t tc_idx,
  bool *mangled, 
  tcache_entry **demangled_ptr
){
  /* `tep` is the address of the link pointing to 
     the first valid chunk and `te` is the actual 
     demangled pointer to the chunk. */

  tcache_entry **tep = &(tcache->entries[tc_idx]);
  tcache_entry *te = *tep;

  while (
    (te != NULL) && 
    __glibc_unlikely (chunksize(mem2chunk(te)) < nb)
  ){
    tep = &(te->next);

    /* The actual pointer to the chunk, demangled */
    te = REVEAL_PTR(te->next);

    /* It is a property of tep, as it points to a 
       link now. */
    *mangled = true;
  }

  *demangled_ptr = te;
  return tep;
}

static __always_inline void
tcache_put_large (mchunkptr chunk, size_t tc_idx)
{
  tcache_entry **entry;
  bool mangled = false;

  tcache_entry *te;
  entry = tcache_location_large (chunksize(chunk), tc_idx, &mangled, &te);

  return tcache_put_n (chunk, tc_idx, entry, mangled);
}

static __always_inline void*
tcache_get_large (size_t tc_idx, size_t nb)
{
  tcache_entry **entry;
  bool mangled = false;

  tcache_entry *te;
  entry = tcache_location_large (nb, tc_idx, &mangled, &te);

  if (
    te == NULL || 
    nb != chunksize(mem2chunk(te))
  )
    return NULL;

  return tcache_get_n (tc_idx, entry, mangled);
}

static void tcache_init (mstate av);

/* Get a chunk from a tcache bin with a custom 
   alignment. */
static __always_inline void*
tcache_get_align (size_t nb, size_t alignment)
{
  /* [Note]: nb is normalized bytes. */
  if (nb < mp_.tcache_max_bytes){
    /* Obtain the tcache bin corresponding to nb. */
    size_t tc_idx = csize2tidx(nb);

    /* If nb was a large size, csize2tidx result 
       is not correct, so we call large_csize2tidx 
       instead. I don't understand why it was not 
       established first. */
    /* Small tcache bin index belongs to [0, 63]. */
    if (__glibc_unlikely(tc_idx >= TCACHE_SMALL_BINS))
      tc_idx = large_csize2tidx(nb);

    /* The tcache bin pointers don't use safe linking 
       but the next pointers in tcache_entery do. */
    tcache_entry **tep = &tcache->entries[tc_idx];
    tcache_entry *te = *tep;
    bool mangled = false;
    size_t csize;

    /* We have to find a chunk which is an exact-fit 
       and the payload memory starts at the required 
       alignment boundary. */
    while (
      te != NULL && 
      (
        (csize = chunksize(mem2chunk(te))) < nb || 
        (
          csize == nb 
          && !PTR_IS_ALIGNED(te, alignment)
        )
      )
    ){
      tep = &(te->next);
      te = REVEAL_PTR(te->next);
      mangled = true;
    }

    /* GCC compiling for -Os warns on some architectures 
       that csize may be uninitialized. However, if 'te' 
       is not NULL, csize is always initialized in the 
       loop above. */
    DIAG_PUSH_NEEDS_COMMENT;
    DIAG_IGNORE_Os_NEEDS_COMMENT (12, "-Wmaybe-uninitialized");

    if (
      te != NULL && 
      csize == nb && 
      PTR_IS_ALIGNED(te, alignment)
    )
    	return tag_new_usable(tcache_get_n(tc_idx, tep, mangled));

    DIAG_POP_NEEDS_COMMENT;
  }
  /* If no exact-fit is found. */
  return NULL;
}

/* Verify if the suspicious tcache_entry is double free.
   It's not expected to execute very often, mark it as 
   noinline. */
static __attribute__ ((noinline)) void
tcache_double_free_verify (tcache_entry *e)
{
  tcache_entry *tmp;
  for (
    size_t tc_idx = 0; 
    tc_idx < TCACHE_MAX_BINS; 
    ++tc_idx
  ){
    size_t cnt = 0;
    LIBC_PROBE (memory_tcache_double_free, 2, e, tc_idx);

    for (
      tmp = tcache->entries[tc_idx];
      tmp;
      tmp = REVEAL_PTR (tmp->next), ++cnt
    ){
      if (cnt >= mp_.tcache_count)
        malloc_printerr ("free(): too many chunks detected in tcache");

      if (__glibc_unlikely (misaligned_mem (tmp)))
        malloc_printerr ("free(): unaligned chunk detected in tcache 2");

      if (tmp == e)
        malloc_printerr ("free(): double free detected in tcache 2");
    }
  }

  /* No double free detected - it might be in a tcache of 
     another thread, or user data that happens to match the 
     key. Since we are not sure, clear the key and retry 
     freeing it. */
  e->key = 0;
  __libc_free(e);
}

static void tcache_thread_shutdown(void)
{
  int i;
  mchunkptr p;
  tcache_perthread_struct *tcache_tmp = tcache;
  int need_free = tcache_enabled();

  /* Disable the tcache and prevent it from being 
     reinitialized. */
  tcache_set_disabled();
  if (!need_free)
    return;

  /* Free all of the entries and the tcache itself 
     back to the arena heap for coalescing. */
  for (i = 0; i < TCACHE_MAX_BINS; ++i){
    while (tcache_tmp->entries[i]){
      tcache_entry *e = tcache_tmp->entries[i];

      if (__glibc_unlikely(misaligned_mem(e)))
        malloc_printerr ("tcache_thread_shutdown(): unaligned tcache chunk detected");

      /* Move to the next chunk. It is essentially 
          tcache_tmp->entries[i] = e->next */
  	  tcache_tmp->entries[i] = REVEAL_PTR(e->next);
	    e->key = 0;

	    p = mem2chunk(e);
  	  _int_free_chunk(arena_for_chunk(p), p, chunksize(p), 0);
    }
  }

  p = mem2chunk(tcache_tmp);
  _int_free_chunk(arena_for_chunk(p), p, chunksize(p), 0);
}

/* Initialize tcache. In the rare case there isn't any 
   memory available, later calls will retry initialization. */
static void tcache_init(mstate av)
{
  /* By default, the per-thread cache is initialized 
     as inactive. When _int_malloc is called, it checks 
     if USE_TCACHE is enabled and tcache is not 
     initialized. If that is the case, it calls 
     tcache_init to setup it. This can cause an infinite 
     loop. To avoid this, it is set disabled. */
  tcache_set_disabled();

  if (mp_.tcache_count == 0)
    return;

  /* The size of tcache_perthread_struct. We need 
     to call the allocator for a chunk of these 
     many bytes to hold the tcache metadata. */
  size_t bytes = sizeof(tcache_perthread_struct);

  /* When _int_malloc already calls checked_request2size 
     to normalize the request, why we are passing 
     request2size(bytes) here? Even __libc_malloc2 
     calls _int_malloc without normalization, so I 
     don't understand what is the point.

    As per this git blame,
      https://github.com/bminor/glibc/blame/master/malloc/malloc.c#L3234C57-L3234C57
    and commit
      https://github.com/bminor/glibc/commit/2bf2188fae1f3e48d12fdd26f56ff6881fd0b316
    the if block never existed earlier and it used 
    request2size from the start. */

  if (av){
    tcache = (tcache_perthread_struct*) _int_malloc(av, request2size(bytes));
  }
  else{
    tcache = (tcache_perthread_struct*) __libc_malloc2(bytes);
  }

  if (tcache == NULL){
    /* If the allocation failed, don't try again. */
    tcache_set_disabled();
  }
  else{
    memset(tcache, 0, bytes);

    /* Initialize the count of chunks for each 
       tcache bin. */
    for (int i = 0; i < TCACHE_MAX_BINS; i++)
      tcache->num_slots[i] = mp_.tcache_count;
  }
}

#else  /* !USE_TCACHE */

static void tcache_thread_shutdown(void)
{
  /* Nothing to do if there is no thread cache. */
}

#endif /* !USE_TCACHE  */

#if IS_IN (libc)

static void* __attribute_noinline__
__libc_malloc2 (size_t bytes)
{
  mstate ar_ptr;
  void *victim;

  /* [PATH 1]: The process is single threaded. */
  if (SINGLE_THREAD_P){
    victim = tag_new_usable (_int_malloc(&main_arena, bytes));
    assert(
      !victim || 
      chunk_is_mmapped(mem2chunk(victim)) ||
      &main_arena == arena_for_chunk(mem2chunk(victim))
    );

    return victim;
  }

  /* [PATH 2]: The process is multi-threaded. */

  /* Acquire an arena and lock the corresponding mutex. */
  arena_get(ar_ptr, bytes);

  /* Obtain memory. */
  victim = _int_malloc(ar_ptr, bytes);

  /* If _int_malloc failed but there are more arenas, 
     try them out if any of them is usable. */
  if (!victim && ar_ptr != NULL){
    LIBC_PROBE(memory_malloc_retry, 1, bytes);

    ar_ptr = arena_get_retry(ar_ptr, bytes);
    victim = _int_malloc(ar_ptr, bytes);
  }

  if (ar_ptr != NULL)
    __libc_lock_unlock(ar_ptr->mutex);

  victim = tag_new_usable(victim);

  assert(
    !victim || 
    chunk_is_mmapped(mem2chunk(victim)) ||
    ar_ptr == arena_for_chunk(mem2chunk(victim))
  );

  return victim;
}

void* __libc_malloc (size_t bytes)
{
  /* This part is complied only when the thread cache 
     infrastructure is active, which is, in most modern 
     systems. We check if the tcache infra can service 
     this request. Otherwise, we fall back to the core 
     system. 
  */
#if USE_TCACHE
  size_t nb = checked_request2size(bytes);

  /* The normalized size must be less than the maximum 
     size that the tcache bins manage. 

    If (bytes > PTRDIFF_MAX), nb would be SIZE_MAX, 
    which is magnitudes greater than the maximum 
    size managed by tcache bins, so this path will 
    not be taken.
  */
  if (nb < mp_.tcache_max_bytes){
    /* The tcache bin index. */
    size_t tc_idx = csize2tidx(nb);

    /* If nb is small. */
    if (__glibc_likely(tc_idx < TCACHE_SMALL_BINS)){
      if (tcache->entries[tc_idx] != NULL)
      return tag_new_usable (tcache_get(tc_idx));
    }

    /* Since nb is large, large_csize2tidx is required 
       to obtain the correct index. */
    else{
      tc_idx = large_csize2tidx(nb);
      void *victim = tcache_get_large(tc_idx, nb);
      if (victim != NULL)
        return tag_new_usable(victim);
    }
	}
#endif

  return __libc_malloc2(bytes);
}
libc_hidden_def (__libc_malloc)

static void __attribute_noinline__
tcache_free_init (void *mem)
{
  tcache_init (NULL);
  __libc_free (mem);
}

void __libc_free(void *mem)
{
  /* Chunk corresponding to mem. */
  mchunkptr p;

  /* free(0) has no effect. */
  if (mem == NULL)
    return;

  /* Quickly check that the freed pointer matches the tag for 
     the memory. This gives a useful double-free detection. */
  if (__glibc_unlikely (mtag_enabled))
    *(volatile char*)(mem);

  p = mem2chunk(mem);

  /* Mark the chunk as belonging to the library again. */
  tag_region(chunk2mem(p), memsize(p));

  INTERNAL_SIZE_T size = chunksize(p);

  if (__glibc_unlikely(misaligned_chunk(p)))
    return malloc_printerr_tail("free(): invalid pointer");

  /* If USE_TCACHE is enabled, the chunk is intercepted 
     by the tcache layer. If the chunk has an accepted 
     size and the corresponding tcache bin has space to 
     accommodate it, the chunk is kept by the tcache bin. 
     Otherwise, it is passed to the arena.
  */

#if USE_TCACHE
  /* Chunk size must be less than the max size 
     managed by the tcache bins. */
  if (__glibc_likely(size < mp_.tcache_max_bytes))
  {
    /* Check if the chunk is already in the tcache. */
    tcache_entry *e = (tcache_entry*) chunk2mem(p);

    /* Check for double free - verify if the key matches. */
    if (__glibc_unlikely(e->key == tcache_key))
      return tcache_double_free_verify(e);

    /* Obtain the tcache bin index corresponding to 
       the size and call the right handler to place 
       the chunk in the tcache bin.
    */
    size_t tc_idx = csize2tidx(size);
    if (__glibc_likely(tc_idx < TCACHE_SMALL_BINS))
    {
      if (__glibc_likely(tcache->num_slots[tc_idx] != 0))
  	    return tcache_put(p, tc_idx);
	  }
    else{
      tc_idx = large_csize2tidx(size);
      if (size >= MINSIZE && __glibc_likely(tcache->num_slots[tc_idx] != 0))
  	    return tcache_put_large (p, tc_idx);
	  }

    /* I don't understand this yet. */
    if (__glibc_unlikely(tcache_inactive()))
    	return tcache_free_init(mem);
  }
#endif

  /* Check (size >= MINSIZE) and (p + size) does 
     not overflow. */
  if (__glibc_unlikely(
    INT_ADD_OVERFLOW((uintptr_t)(p), size-MINSIZE)
  ))
    return malloc_printerr_tail ("free(): invalid size");

  _int_free_chunk(arena_for_chunk(p), p, size, 0);
}
libc_hidden_def (__libc_free)

void* __libc_realloc (void *oldmem, size_t bytes)
{
  mstate ar_ptr;
  INTERNAL_SIZE_T nb;         /* padded request size */

  void *newp;                 /* chunk to return */

  /* realloc(NULL, bytes) is the same as malloc(bytes). */
  if (oldmem == NULL)
    return __libc_malloc(bytes);

  /* realloc(mem, 0) is the same as __libc_free(mem). */

#if REALLOC_ZERO_BYTES_FREES
  if (bytes == 0){
    __libc_free(oldmem); return NULL;
  }
#endif

  /* Perform a quick check to ensure that the pointer's tag 
     matches the memory's tag.  */
  if (__glibc_unlikely (mtag_enabled))
    *(volatile char*)(oldmem);

  /* The chunk corresponding to oldmem. */
  const mchunkptr oldp = mem2chunk(oldmem);

  /* Return the chunk as is if the request grows within 
     usable bytes, typically into the alignment padding.

    We want to avoid reusing the block for shrinkages 
    because it ends up unnecessarily fragmenting the 
    address space. This is also why the heuristic misses 
    alignment padding for THP for now.
  */

  size_t usable = musable(oldmem);
  if (bytes <= usable){
    size_t difference = (usable - bytes);

    /* If the difference is small, don't shrink. */
    /* If the fragment was significant enough, do not 
       return. */
    if ((unsigned long)(difference) < (2 * sizeof(INTERNAL_SIZE_T)))
      return oldmem;
    }

  /* Size of the old chunk. */
  const INTERNAL_SIZE_T oldsize = chunksize(oldp);

  /* Little security check which won't hurt performance: 
     the allocator never wraps around at the end of the 
     address space. Therefore we can exclude some size 
     values which might appear here by accident or by 
     "design" from some intruder.
  */

  if (__glibc_unlikely (
    (uintptr_t)(oldp) > (uintptr_t)(-oldsize) || 
    misaligned_chunk(oldp)
  ))
    malloc_printerr ("realloc(): invalid pointer");

  if (bytes > PTRDIFF_MAX){
    __set_errno (ENOMEM);
    return NULL;
  }
  nb = checked_request2size(bytes);

  /* Use the "memory remap" syscall if the chunk is mmapped. */
  if (chunk_is_mmapped(oldp)){
    void *newmem;

#if HAVE_MREMAP
    newp = mremap_chunk(oldp, nb);
    if (newp){
  	  void *newmem = chunk2mem_tag(newp);

	    /* Give the new block a different tag. This helps 
         to ensure that stale handles to the previous 
         mapping are not reused. There's a performance 
         hit for both us and the caller for doing this, 
         so we might want to reconsider.
      */
	    return tag_new_usable(newmem);
    }
#endif

    /* Return the original pointer if mremap was unsuccessful, 
       as shrinking is not possible for an mmapped chunk and 
       the old pointer already has enough memory (regardless 
       of the fragment being small or large).
    */
    if (bytes <= usable)
    	return oldmem;

    /* Must alloc, copy, free. */
    newmem = __libc_malloc(bytes);
    if (newmem == NULL)
      return NULL;

    /* Copy the old contents at the new memory. */
    memcpy(newmem, oldmem, oldsize - CHUNK_HDR_SZ);

    /* Unmap the old chunk. */
    munmap_chunk(oldp);
    return newmem;
  }

  /* Obtain the arena for this chunk. */
  ar_ptr = arena_for_chunk(oldp);

  if (SINGLE_THREAD_P){
    newp = _int_realloc(ar_ptr, oldp, oldsize, nb);

    assert(
      !newp || 
      chunk_is_mmapped(mem2chunk(newp)) ||
      ar_ptr == arena_for_chunk(mem2chunk(newp))
    );

    return newp;
  }

  /* If multithreaded, lock the arena. */
  __libc_lock_lock(ar_ptr->mutex);

  newp = _int_realloc (ar_ptr, oldp, oldsize, nb);

  __libc_lock_unlock (ar_ptr->mutex);

  assert(
    !newp || 
    chunk_is_mmapped(mem2chunk(newp)) ||
    ar_ptr == arena_for_chunk(mem2chunk(newp))
  );

  /* If the current arena didn't satisfied the 
     request, try with others. */
  if (newp == NULL){
    LIBC_PROBE (memory_realloc_retry, 2, bytes, oldmem);

    /* __libc_malloc will acquire new arenas. */
    newp = __libc_malloc(bytes);
    if (newp != NULL){
  	  size_t sz = memsize(oldp);
	    memcpy(newp, oldmem, sz);

	    (void) tag_region(chunk2mem(oldp), sz);
      _int_free_chunk(ar_ptr, oldp, chunksize (oldp), 0);
    }
  }

  return newp;
}
libc_hidden_def (__libc_realloc)

void* __libc_memalign (size_t alignment, size_t bytes)
{
  return _mid_memalign (alignment, bytes);
}
libc_hidden_def (__libc_memalign)

/* For ISO C17.  */
void* weak_function
aligned_alloc (size_t alignment, size_t bytes)
{
/* Similar to memalign, but starting with ISO C17 
   the standard requires an error for alignments 
   that are not supported by the implementation. 
   Valid alignments for the current implementation 
   are non-negative powers of two.
  */

  if (!powerof2(alignment) || alignment == 0){
    __set_errno(EINVAL);
    return NULL;
  }

  return _mid_memalign(alignment, bytes);
}

/* For ISO C23. */
void weak_function
free_sized (void *ptr, __attribute_maybe_unused__ size_t size)
{
  /* We do not perform validation that size is the same 
     as the original requested size at this time. We 
     leave that to the sanitizers. We simply forward to 
     `free`. This allows existing malloc replacements
     to continue to work.
  */

  /* Try harder to allocate memory in other arenas. */
  free(ptr);
}

/* For ISO C23. */
void weak_function
free_aligned_sized (
  void *ptr, 
  __attribute_maybe_unused__ size_t alignment,
  __attribute_maybe_unused__ size_t size
){
  /* We do not perform validation that size and alignment is 
     the same as the original requested size and alignment 
     at this time. We leave that to the sanitizers. We simply 
     forward to `free`. This allows existing malloc replacements 
     to continue to work.
  */
  free (ptr);
}

static void* _mid_memalign(size_t alignment, size_t bytes)
{
  mstate ar_ptr;
  void *p;

  /* If the required alignment is less than the minimum 
     alignment that malloc gives, just relay to malloc. */
  if (alignment <= MALLOC_ALIGNMENT)
    return __libc_malloc(bytes);

  /* Otherwise, ensure that it is at least a minimum chunk 
     size. */
  if (alignment < MINSIZE)
    alignment = MINSIZE;

  /* If the alignment is greater than ((SIZE_MAX / 2) + 1), 
     it cannot be a power of 2 and will cause overflow in 
     the check below. */
  if (alignment > ((SIZE_MAX / 2) + 1)){
    __set_errno (EINVAL);
    return NULL;
  }

  /* Make sure alignment is power-of-2 value. */
  if (!powerof2(alignment)){
    size_t a = (MALLOC_ALIGNMENT * 2);

    while (a < alignment)
      a <<= 1;

    alignment = a;
  }

  /* [PATH 1]: Use the tcache bins if available. */

#if USE_TCACHE
  /* If the first argument was SIZE_MAX due to 
     (bytes > PTRDIFF_MAX), the return will be NULL. */
  void *victim = tcache_get_align (checked_request2size(bytes), alignment);
  if (victim != NULL)
    return tag_new_usable(victim);
#endif

  if (SINGLE_THREAD_P){
    p = _int_memalign(&main_arena, alignment, bytes);

    assert(
      !p || 
      chunk_is_mmapped(mem2chunk(p)) ||
      &main_arena == arena_for_chunk(mem2chunk(p))
    );

    return tag_new_usable(p);
  }

  /* If multithreaded, acquire an arena. */
  arena_get(ar_ptr, bytes + alignment + MINSIZE);

  p = _int_memalign(ar_ptr, alignment, bytes);
  if (!p && (ar_ptr != NULL)){
    LIBC_PROBE (memory_memalign_retry, 2, bytes, alignment);

    ar_ptr = arena_get_retry (ar_ptr, bytes);
    p = _int_memalign(ar_ptr, alignment, bytes);
  }

  if (ar_ptr != NULL)
    __libc_lock_unlock (ar_ptr->mutex);

  assert(
    !p || 
    chunk_is_mmapped(mem2chunk(p)) ||
    ar_ptr == arena_for_chunk(mem2chunk(p))
  );

  return tag_new_usable(p);
}

void* __libc_valloc (size_t bytes)
{
  return _mid_memalign (GLRO(dl_pagesize), bytes);
}

void* __libc_pvalloc (size_t bytes)
{
  size_t pagesize = GLRO(dl_pagesize);
  size_t rounded_bytes;

  /* ALIGN_UP with overflow check. */
  if (__glibc_unlikely(__builtin_add_overflow(
    bytes,
    pagesize - 1,
    &rounded_bytes))
  ){
    __set_errno(ENOMEM);
    return NULL;
  }

  return _mid_memalign(pagesize, rounded_bytes & -pagesize);
}

static void* __attribute_noinline__
__libc_calloc2 (size_t sz)
{
  mstate av;
  mchunkptr oldtop, p;
  INTERNAL_SIZE_T oldtopsize, csz;
  void *mem;
  unsigned long clearsize;

  if (SINGLE_THREAD_P)
    av = &main_arena;
  else
    arena_get(av, sz);

  if (av){
    /* Check if we hand out the top chunk, in which 
       case there may be no need to clear. */

#if MORECORE_CLEARS
    oldtop = top(av);
    oldtopsize = chunksize(top(av));

#if MORECORE_CLEARS < 2
    /* Only newly allocated memory is guaranteed to be cleared. */
    if (
      av == &main_arena &&
	    oldtopsize < (mp_.sbrk_base + av->max_system_mem - (char*)(oldtop))
    )
    	oldtopsize = (mp_.sbrk_base + av->max_system_mem - (char*)(oldtop));
#endif

    if (av != &main_arena){
      heap_info *heap = heap_for_ptr(oldtop);

  	  if (oldtopsize < ((char*)(heap) + heap->mprotect_size - (char*)(oldtop)))
	      oldtopsize = (char*)(heap) + heap->mprotect_size - (char*)(oldtop);
    }
#endif
  }
  else{
    /* No usable arenas.  */
    oldtop = NULL;
    oldtopsize = 0;
  }
  mem = _int_malloc(av, sz);

  assert (
    !mem || 
    chunk_is_mmapped(mem2chunk(mem)) ||
    av == arena_for_chunk(mem2chunk(mem))
  );

  if (!SINGLE_THREAD_P){
    if (mem == NULL && av != NULL){
      LIBC_PROBE (memory_calloc_retry, 1, sz);

      av = arena_get_retry(av, sz);
      mem = _int_malloc(av, sz);
    }

    if (av != NULL)
      __libc_lock_unlock(av->mutex);
  }

  /* Allocation failed even after a retry. */
  if (mem == NULL)
    return NULL;

  p = mem2chunk(mem);

  /* If we are using memory tagging, then we need to 
     set the tags regardless of MORECORE_CLEARS, so 
     we zero the whole block while doing so. */
  if (__glibc_unlikely(mtag_enabled))
    return tag_new_zero_region(mem, memsize(p));

  csz = chunksize(p);

  /* Two optional cases in which clearing not necessary. */
  if (chunk_is_mmapped(p)){
    if (__glibc_unlikely(perturb_byte))
      return memset(mem, 0, sz);

    return mem;
  }

#if MORECORE_CLEARS
  if (
    (perturb_byte == 0) && 
    ((p == oldtop) && (csz > oldtopsize))
  ){
    /* Clear only the bytes from non-freshly-sbrked memory. */
    csz = oldtopsize;
  }
#endif

  clearsize = (csz - SIZE_SZ);
  return clear_memory((INTERNAL_SIZE_T*)(mem), clearsize);
}

/* calloc(size_t n)

  Returns a contiguous zero-initialized region capable of 
  storing n objects of size elemsize.

  It ensures that (nmemb * elem_size) doesn't overflow.
*/
void* __libc_calloc (size_t n, size_t elem_size)
{
  size_t bytes;

  if (__glibc_unlikely(
    __builtin_mul_overflow(n, elem_size, &bytes)
  )){
    __set_errno(ENOMEM);
    return NULL;
  }

#if USE_TCACHE
  size_t nb = checked_request2size(bytes);

  /* If nb is SIZE_MAX, this path will not execute. */
  if (nb < mp_.tcache_max_bytes){
    size_t tc_idx = csize2tidx(nb);

    if (__glibc_unlikely(tc_idx < TCACHE_SMALL_BINS)){
      if (tcache->entries[tc_idx] != NULL){
	      void *mem = tcache_get (tc_idx);

	      if (__glibc_unlikely(mtag_enabled))
      		return tag_new_zero_region(mem, memsize(mem2chunk(mem)));

        /* Zero the payload memory. */
	      return clear_memory(
          (INTERNAL_SIZE_T*)(mem), 
          tidx2usize(tc_idx)
        );
	    }
    }
    else{
      tc_idx = large_csize2tidx(nb);
      void *mem = tcache_get_large(tc_idx, nb);

      if (mem != NULL){
	      if (__glibc_unlikely(mtag_enabled))
	        return tag_new_zero_region(mem, memsize(mem2chunk(mem)));

        /* Zero the payload memory. */
	      return memset(mem, 0, memsize(mem2chunk(mem)));
	    }
    }
  }
#endif

  return __libc_calloc2(bytes);
}
#endif /* IS_IN (libc) */

/* -------------------- malloc -------------------- */

/* [PRECONDITION]: The caller must ensure a valid arena 
    exist. */
static void* _int_malloc(mstate av, size_t bytes)
{
  INTERNAL_SIZE_T nb;               /* Normalized request size. */
  unsigned int idx;                 /* Bin number associated with nb. */
  mbinptr bin;                      /* The fake chunk whose fd/bk overlap 
                                       with the bin headers. */

  mchunkptr victim;                 /* Chunk being inspected. */
  INTERNAL_SIZE_T size;             /* Victim's size. */
  int victim_index;                 /* Victim's bin number. */

  mchunkptr remainder;              /* Remainder from a split. */
  unsigned long remainder_size;     /* Remainder's size. */

  unsigned int block;               /* Bit map traverser. */
  unsigned int bit;                 /* Bit map traverser. */
  unsigned int map;                 /* Current word of binmap. */

  mchunkptr fwd;                    /* Temporary variables for */
  mchunkptr bck;                    /* handling chunks. */

#if USE_TCACHE
  size_t tcache_unsorted_count;	    /* The count of unsorted bin chunks 
                                       processed during this malloc call. */
#endif

  /* [STEP 1]: Align `bytes` to an internally usable form. */

  /* While checked_request2size also has the same check, 
     what happens after it is proven true is different.

    Some callers validate the request size before calling 
    checked_request2size(), making the internal check 
    redundant. Other callers rely on the helper to perform 
    the validation.

    The callers that check the size manually are often the 
    ones that implement API behavior. They set errno and 
    return NULL. However, the callers that rely on 
    checked_request2size are often helper functions, so 
    setting errno and returning NULL is out of their scope. 
    checked_request2size returns SIZE_MAX as a sentinel 
    value and the callers safely fall back to the core 
    pathways that can propagate the error.

    Another difference between the dedicated check and 
    the one inside checked_request2size is that the 
    latter is wrapped inside glibc_unlikely. */

  if (bytes > PTRDIFF_MAX){
    __set_errno (ENOMEM);
    return NULL;
  }
  nb = checked_request2size(bytes);

  /* [PATH 1]: If there are no usable arenas, we have 
      to use mmap, as there is no other option to 
      fulfill this request.

    As of now, it remains unanswered. See open-questions.md.
  */
  if (__glibc_unlikely(av == NULL)){
    void *p = sysmalloc(nb, av);

    if (p != NULL)
    	alloc_perturb(p, bytes);

    return p;
  }

  /* [PATH 2]: If a small request, check the corresponding 
      smallbin. This is an exact fit path. */
  if (in_smallbin_range(nb)){
    idx = smallbin_index(nb);
    bin = bin_at(av, idx);

    /* Ensure the bin is non-empty. */
    if ((victim = last(bin)) != bin){
      bck = victim->bk;

  	  if (__glibc_unlikely(bck->fd != victim))
        malloc_printerr ("malloc(): smallbin double linked list corrupted");

      /* Set the PREV_INUSE bit of the chunk next 
         to victim in memory. */
      set_inuse_bit_at_offset(victim, nb);

      /* Unlink victim. */

      /* bin_handler->bk = prev_victim */
      bin->bk = bck;

      /* prev_victim->fd = next_victim */
      bck->fd = bin;

      /* Set the NON_MAIN_ARENA bit, if applicable. */
      if (av != &main_arena)
  	    set_non_main_arena(victim);

      check_malloced_chunk(av, victim, nb);

      /* If the tcache infra is active, we check if the 
         smallbin has more chunks. If yes, we stash them 
         into the corresponding tcache bin for fast 
         access by later malloc calls for that size. */

#if USE_TCACHE
	    size_t tc_idx = csize2tidx(nb);
	    if (tc_idx < mp_.tcache_small_bins){
	      mchunkptr tc_victim;

        /* Setup the tcache infra, if first time. */
	      if (__glibc_unlikely(tcache_inactive()))
      		tcache_init(av);

        tc_victim = last(bin);
	      while(
          tcache->num_slots[tc_idx] != 0 && 
          tc_victim != bin
        ){
    		  if (tc_victim != NULL){
  		      bck = tc_victim->bk;

            /* Chunks in the tcache bins are considered 
               inuse, so set the inuse bit of the chunk 
               next to this one in memory. */
            set_inuse_bit_at_offset(tc_victim, nb);

            if (av != &main_arena)
        			set_non_main_arena (tc_victim);

            /* Unlink the victim. */
  		      bin->bk = bck;
	  	      bck->fd = bin;

            /* Place in the tcache bin. */
  		      tcache_put(tc_victim, tc_idx);
          }
        }
	    }
#endif

      /* Pointer to the payload memory. */
      void *p = chunk2mem(victim);

      /* [?] */
      alloc_perturb(p, bytes);

      /* Return the payload memory to the process. */
      return p;
    }
  }

  /* [PATH 2] Analysis. 

    If we are here, either
      [1] the smallbin was empty, or
      [2] nb is not a small size.
  */

  /* If nb is a large size, obtain the large bin index. */
  else{
    idx = largebin_index(nb);
  }

#if USE_TCACHE
  INTERNAL_SIZE_T tcache_nb = 0;
  size_t tc_idx = csize2tidx(nb);

  /* If nb is small, assign it to tcache_nb. */
  if (tc_idx < mp_.tcache_small_bins)
    tcache_nb = nb;

  int return_cached = 0;
  tcache_unsorted_count = 0;
#endif

  /* I don't understand what this outer loop is for. */
  for (;;){
    int iters = 0;

    /* [PATH 3]: Check the unsorted bin. */
    while(
      (victim = unsorted_chunks(av)->bk) 
      != unsorted_chunks(av)
    ){
      /* The chunk before the victim chunk in the 
         unsorted bin. */
      bck = victim->bk;

      /* Size of the victim chunk. */
      size = chunksize(victim);

      /* The chunk after the victim chunk in memory. */
      mchunkptr next = chunk_at_offset(victim, size);

      /* Consistency checks.

        They are performed on victim, the chunk before victim 
        "in the bin" and the chunk next to victim "in the memory".

        We start with the last chunk in the unsorted bin. 
        Therefore, there is no chunk next to the victim in 
        the first case. As we traverse, every chunk next 
        to the victim is already analyzed. But we do need 
        the next chunk "in memory" to ensure the prev_size 
        metadata is correct.

        If the victim is not fit for this request, it is 
        binned appropriately. We have to update the bin 
        links, so we ensure that the chunk previous to 
        victim "in the bin" is OK.
      */

      /* [TEST 1]: The size of the victim chunk must be >= 
         MINSIZE and less than the total memory managed by 
         the arena. */
      if (
        __glibc_unlikely(size <= CHUNK_HDR_SZ) || 
        __glibc_unlikely(size > av->system_mem)
      )
        malloc_printerr("malloc(): invalid size (unsorted)");

      /* [TEST 2]: The size of the chunk next to the victim 
         chunk in memory must be >= MINSIZE and less than the 
         total memory managed by the arena. */
      if (
        __glibc_unlikely(chunksize_nomask(next) < CHUNK_HDR_SZ) || 
        __glibc_unlikely(chunksize_nomask(next) > av->system_mem)
      )
        malloc_printerr("malloc(): invalid next size (unsorted)");

      /* [TEST 3]: The prev_size of the next chunk in memory must 
         be equal to the size of the victim. */
      if (__glibc_unlikely(
        (prev_size(next) & ~(SIZE_BITS)) != size
      ))
        malloc_printerr("malloc(): mismatching next->prev_size (unsorted)");

      /* [TEST 4]: The pointers in the unsorted bin must not be 
         corrupted. */
      if (
        __glibc_unlikely(bck->fd != victim) || 
        __glibc_unlikely(victim->fd != unsorted_chunks(av))
      )
        malloc_printerr("malloc(): unsorted double linked list corrupted");

      /* [TEST 5]: The PREV_INUSE bit of the next chunk must be set. */
      if (__glibc_unlikely(prev_inuse(next)))
        malloc_printerr("malloc(): invalid next->prev_inuse (unsorted)");



      /* [PATH 3A]: Use (av->last_remainder) if
          [1] nb is a small size,
          [2] the unsorted bin has only one chunk, which 
              is last_remainder, and
          [3] the chunk has enough size to exist after 
              splitting.

        I don't understand why the unsorted bin has to 
        be singleton here. See open-questions.md.
      */
      if (
        in_smallbin_range(nb) &&
        bck == unsorted_chunks(av) &&
        victim == av->last_remainder &&
        (unsigned long)(size) > (unsigned long)(nb + MINSIZE)
      ){
        /* Split. */
        remainder_size = (size - nb);
        remainder = chunk_at_offset(victim, nb);

        /* Update the unsorted bin links and av->last_remainder 
           to the new remainder. */
        unsorted_chunks(av)->bk = unsorted_chunks(av)->fd = remainder;
        av->last_remainder = remainder;
        remainder->bk = remainder->fd = unsorted_chunks(av);

        /* If the resulting remainder is not a small chunk, 
           set the skip list pointers NULL as the unsorted 
           chunks have them NULL. */
        if (!in_smallbin_range(remainder_size)){
          remainder->fd_nextsize = NULL;
          remainder->bk_nextsize = NULL;
        }

        /* Update the metadata of the victim chunk. */
        set_head(
          victim, 
          nb | PREV_INUSE | (av != &main_arena ? NON_MAIN_ARENA : 0)
        );

        /* Update the size of remainder. */
        set_head(
          remainder, 
          remainder_size | PREV_INUSE
        );

        /* Update the prev_size of the chunk next to remainder 
           in memory. */
        set_foot(remainder, remainder_size);

        check_malloced_chunk(av, victim, nb);
        void *p = chunk2mem(victim);
        alloc_perturb(p, bytes);

        return p;
      }

      /* Unlink the victim. */
      unsorted_chunks(av)->bk = bck;
      bck->fd = unsorted_chunks(av);

      /* [PATH 3B]: Take the victim if exact fit. */
      if (size == nb){
        set_inuse_bit_at_offset(victim, size);

        if (av != &main_arena)
      		set_non_main_arena(victim);

      /* If the tcache infra is not active, we return 
         the victim. If not, all the exact-fit chunks 
         are stashed in the tcache bin. Only when the 
         tcache bin is full and there is a victim left, 
         do we return it. Otherwise, we wait for the 
         next tcache block to return a chunk. */

#if USE_TCACHE
        /* Setup the tcache infra if not already. */
	      if (__glibc_unlikely(tcache_inactive()))
      		tcache_init(av);

	      if(
          (tcache_nb > 0) && 
          tcache->num_slots[tc_idx] != 0
        ){
    		  tcache_put(victim, tc_idx);
    		  return_cached = 1;

          /* Move to the next chunk in unsorted bin. */
    		  continue;
        }
	      else{
#endif

          check_malloced_chunk(av, victim, nb);
          void *p = chunk2mem(victim);
          alloc_perturb(p, bytes);

          return p;
#if USE_TCACHE
        }
#endif
      }

      /* Because the victim is neither an exact fit nor the 
         last_remainder, classify it into its appropriate bin.

        [OBSERVATION]
        It is reasonable to think that every chunk in the 
        unsorted bin must be given a chance to satisfy the 
        current request. But the implementation seems to 
        have a different policy.

        Either the unsorted bin is singleton with its only 
        chunk as the last_remainder, or the victim is an 
        exact fit. Otherwise, the victim is classified and 
        binned, even if it is large enough to satisfy the 
        request.

        Right now, victims are binned and later rediscovered 
        through regular bin search, and I have no explanation 
        regarding why it is better than directly using the 
        victim while it is in the unsorted bin.

        For me, this is one of those questions that have no 
        visible starting point, making them much harder to 
        reason about.
      */

      /* Why the smallbin path is wrapped in glibc_unlikely?
         This is a relatively recent change. Here is the link: 
          https://sourceware.org/git/?p=glibc.git;a=commit;h=e2436d6f5aa47ce8da80c2ba0f59dfb9ffde08f3

        Before this, both small and large chunks went to the 
        unsorted bin. As per the commit message, "there is no 
        added-advantage to putting small chunks int the 
        unsorted bin".

        Later on, when the fastbins infrastructure was removed, 
        malloc_consolidate() was removed too. This is also done 
        recently. Here is the link: 
          https://sourceware.org/git/?p=glibc.git;a=commit;h=e3062b06c5767f672baf9574c4d7cbebf7d0ee6e

        Looking purely on the code, we can notice that the 
         - freeing mechanism is designed to put small chunks 
           directly in the corresponding small bin, while 
           large chunks are placed in the unsorted and given 
           a second chance on the next malloc request before 
           binning.
         - only way small chunks enter the unsorted bin is 
           when a large chunk is split.

        Therefore, the chances of the victim chunk being a 
        small chunk are relatively low, so the allocator 
        communicates this expectation to the compiler using 
        __glibc_unlikely().
      */

      /* [OBSERVATION]: We can notice "do no repeat yourself" 
          or the DRY principle, in action. Both the branches 
          effectively populate right values in fwd and bck 
          variables and only the large bin updates the skip 
          list pointers. The generic fd/bk pointers, which are 
          common to both the paths are shared. */
      if (__glibc_unlikely(in_smallbin_range(size))){
        victim_index = smallbin_index(size);
        bck = bin_at(av, victim_index);
        fwd = bck->fd;
        /* New chunks are always added in the front. */
      }
      else{
        victim_index = largebin_index(size);
        bck = bin_at(av, victim_index);
        fwd = bck->fd;

        /* A large bin contains chunks of multiple sizes, 
           ordered by size. Therefore, insertion must 
           maintain that. */

        /* If the bin is empty, we only have to update the 
           skip list pointers as the generic pointers are 
           shared. Therefore, the if-block ensures that the 
           bin has at least one chunk. */
        if (fwd != bck){
          /* Or with inuse bit to speed comparisons. */
          size |= PREV_INUSE;

          /* Chunks in the unsorted bin do not have the 
             NON_MAIN_ARENA bit set. This compile-time 
             assertion checks that. However, the macro's 
             name and definition doesn't indicate what 
             it is really doing. */
          assert(chunk_main_arena(bck->bk));

          /* If the size of the victim is smaller than the 
             the smallest chunk the large bin is currently 
             having, bypass the loop. 

             Remember, large bins are ordered in descending 
             fashion. So the back pointer is where the 
             smallest chunk is. If we take the first large 
             bin in category #1, that is how it will look 
             like: [1072, 1056, 1040, 1024]
                   ^ fwd              bck ^ */

          if ((unsigned long)(size) < (unsigned long)chunksize_nomask(bck->bk)){
            /* [PERSONAL OPINION]: 
               bck = bin_at(av, idx) is not very great cognitively. 
               - The ouput of bin_at is a fake chunk whose fd/bk 
                 overlap with the two ends of a bin. So, it is 
                 better to represent it as a bin_handler.
               - Variable names represent an author's personal 
                 choice. So, it is not good to fight over that. We 
                 will continue using the best solution we have, 
                 i.e. comments. */

            fwd = bck;          /* bin_handler */
            bck = bck->bk;      /* bin_handler->bk,  i.e. the back end */

            /* The error statement already proves that this check 
               is about the correctness of the skip list pointers. 
               If you are one of those people who is having a hard 
               time comprehending it, let me join you.

              Take this pyramid as an example.
                fwd >         bin_handler
                                   /\
                                  /  \
                                 /    \
                                /      \
                               /        \
                              /          \
                             /            \
                            /              \
                           /                \
                          /                  \
                         /                    \
                         C1 <-> C2 <-> C3 <-> C4
                         ^                     ^
                      fwd->fd                   bck 

              It shows what fwd, bck and fwd->fd imply after the 
              above two assignments are done. To understand this 
              check, we must understand how skip list pointers are 
              managed. This has already been discussed in this lab 
              [INSERT LINK]. If you are feeling rusty, visit it 
              again.

              To make it easier, we will restrict ourselves to the 
              category #1 largebin on 64-bit that manages 4 chunks 
              of sizes falling in the range [base+1, base+64], where 
              base is the last size class managed by the previous 
              bin. For the first bin in this cateogry, base will be 
              1008 bytes.

              Now start visualizing.
              - If there are 4 chunks, all unique, 
                - what will be the bk_nextsize of the first chunk? It will 
                  be the chunk on the other end. Suppose it is X.
                - what will be the fd_nextsize of X? It will be the chunk 
                  on the other end, i.e. the first chunk.
              - If there are 4 chunks of same size, let's say, 1040 bytes,
                - what will be the first chunk's bk_nextsize? Suppose it is X.
                - what will be the fd_nextsize of X? Both will be the first 
                  chunk.
              - what will fwd->fd represent? The first chunk. */
            if (__glibc_unlikely(fwd->fd->bk_nextsize->fd_nextsize != fwd->fd))
              malloc_printerr ("malloc(): largebin double linked list corrupted (nextsize)");

            /* Because the incoming size is smaller than the 
               smallest size available in this large bin, the 
               large bin has no chunk of victim's 
               size, so victim is uniquely sized, making its 
               skip list pointers non-null. */
            victim->fd_nextsize  = fwd->fd;
            victim->bk_nextsize  = fwd->fd->bk_nextsize;

            fwd->fd->bk_nextsize = victim->bk_nextsize->fd_nextsize = victim;
          }

          /* The comparison in the previous path can fail in 
             two scenarios.
             1. If the victim is not smaller than the smallest.
             2. If the victim is a duplicate of the smallest 
                size. */


          /* We traverse the large bin in forward direction. 
             - If a chunk of victim's size is not available 
               already, we insert it at the position the loop 
               is stopped.
             - If a chunk of victim's size is already present, 
               we are currently pointing to it after the loop 
               is stopped. We insert the victim after this 
               chunk. This is in accord with the allocator's 
               policy of FIFO for duplicates. Basically, the 
               first duplicate is the allocators choice to 
               satisfy a request. If there are no duplicates, 
               we use the unique chunk directly. */
          else{
            assert(chunk_main_arena(fwd));

            while ((unsigned long)(size) < chunksize_nomask(fwd)){
              fwd = fwd->fd_nextsize;
      			  assert(chunk_main_arena(fwd));
            }

            /* If it is a duplicate, skip setting the skip list 
               pointers as they are already set NULL before they 
               are pushed to the unsorted bin. The shared logic 
               will insert this chunk after its unique counterpart, 
               as discussed. */
            if ((unsigned long)(size) == (unsigned long) chunksize_nomask(fwd))
              fwd = fwd->fd;

            /* Because the victim is unique, we have to set the 
               skip list pointers. */
            else{
              /* The chunk previous to victim may or may not be 
                 unique. But the chunk in fwd is is definitely a 
                 unique one, so we use that to find the correct 
                 skip list chunks for victim. */
              victim->fd_nextsize = fwd;
              victim->bk_nextsize = fwd->bk_nextsize;

              /* Consistency check. We have already explored this 
                 earlier. */
              if (__glibc_unlikely(fwd->bk_nextsize->fd_nextsize != fwd))
                malloc_printerr ("malloc(): largebin double linked list corrupted (nextsize)");

              /* Update the existing skip list pointers. */
              fwd->bk_nextsize = victim;
              victim->bk_nextsize->fd_nextsize = victim;
            }

            bck = fwd->bk;
            if (bck->fd != fwd)
              malloc_printerr ("malloc(): largebin double linked list corrupted (bk)");
          }
        }
        else{
          victim->fd_nextsize = victim->bk_nextsize = victim;
        }
      }

      /* This is already simple. */
      mark_bin(av, victim_index);
      victim->bk = bck;
      victim->fd = fwd;
      fwd->bk = victim;
      bck->fd = victim;

#if USE_TCACHE
      /* If we've processed as many chunks as we're 
         allowed while filling the cache, return one 
         of the cached ones. */
      ++tcache_unsorted_count;
      if (
        return_cached && 
        mp_.tcache_unsorted_limit > 0 && 
        tcache_unsorted_count > mp_.tcache_unsorted_limit
      ){
        return tcache_get(tc_idx);
      }
#endif

#define MAX_ITERS       10000
      if (++iters >= MAX_ITERS)
        break;
    }

#if USE_TCACHE
    /* If all the small chunks we found ended 
       up cached, return one now. */
    if (return_cached){
	    return tcache_get (tc_idx);
    }
#endif

    /* [PATH 4]: If a large request, scan the associated 
        large bin.

      Chunks are scanned from smallest to biggest size. 
      The goal is to find the smallest chunk that can 
      satisfy this request. The smallest size that can 
      satisfy the request is the size itself. If not this, 
      then the next greater size available in the bin.

      We use the skip list to traverse the bin quickly 
      and find the smallest fit. If there are multiple 
      chunks of that size class, we use a duplicate and 
      keep the unique one to prevent rerouting the skip 
      list.

      The annotations for "bins" under the "Internal data 
      structures" section mentions that chunks of same size 
      in a large bin follow FIFO ordering. So, new chunks 
      are added in front and chunks are taken from back 
      (the oldest chunk after the unique one) to satisfy 
      the request. 
      - Take this large bin as an example: 
          [1072, 1056_u, 1056_d2, 1056_d1, 1040, 1024_u, 1024_d1]
      - We have requested a 1056 bytes chunk. The chunk 
        that will satisfy this request will be 1056_d1.

      We will start from the forward end as it is always 
      confirmed to contain a unique chunk and use its 
      bk_nextsize pointer to go to the smallest chunk the 
      bin has. We will loop over until the victim becomes 
      >= (nb).
      - The loop has stopped on a unique chunk. Now we have 
        to find the oldest duplicate corresponding to the 
        victim's size.
      - That chunk is (victim->fd_nextsize)->bk. See? No 
        searching is required. And the best part, if there 
        was no duplicate, it will point to the unique 
        chunk itself and with a simple branch, we can check 
        if there are no duplicates. This is mandatory as we 
        have to reroute the skip list in this case.


      [VALIDATE THIS LRU CLAIM AND PUT THIS IN QUESTIONS.md]
      However, the current malloc doesn't do it. It takes 
      the duplicate just after the unique chunk. And I have 
      no answer to the "why". You can check out this lab to 
      see it in action: [INSERT LINK].
      - dlmalloc@2.7.0 uses a chunk-by-chunk traversal to 
        land on the oldest duplicate. Read the line #3412 
        here: 
          https://github.com/DenizThatMenace/dlmalloc/blob/main/malloc-2.7.0.c
      - Upon seeing the git blame for the line (victim = victim->fd), 
        I have found that it was last changed 13 years ago 
        when the source was "formatted to gnu style". The 
        next was 19 years ago and then 25 years ago. The 25 
        years ago version does exactly what we have discussed. 
      - The 25 years ago version closely resembles what 
        dlmalloc@2.7.0 did.
      - These are the links to the git blames. Note that I 
        have used the bminor/glibc mirror on GitHub as the 
        sourceware interface is kinda hard to use. But that 
        doesn't change the point,

          Mirror Link: https://github.com/bminor/glibc
          #13y ago: https://github.com/bminor/glibc/blame/master/malloc/malloc.c#L4159C16-L4159C16 (line #4159)
          #19y ago: https://github.com/bminor/glibc/blame/9a3c6a6ff602c88d7155139a7d7d0000b7b7e946/malloc/malloc.c (line #3491)
          #25y ago: https://github.com/bminor/glibc/blame/e53f0f51a62061e0c654d4b2f82d4c71b4d71932/malloc/malloc.c (line #4242)
    */

    if (!in_smallbin_range(nb)){
      /* We have already obtained the large bin index 
         corresponding to nb. */
      bin = bin_at(av, idx);

      if (
        (victim = first(bin)) != bin && 
        (unsigned long)chunksize_nomask(victim) >= (unsigned long)(nb)
      ){
        /* Start with the smallest available chunk. */
        victim = victim->bk_nextsize;

        /* Stop when the victim becomes >= the required size. */
        while(
          ((unsigned long)(size = chunksize(victim)) < (unsigned long)(nb))
        )
          victim = victim->bk_nextsize;

        /* Take the chunk next to victim if a duplicate 
           is available. */
        if (
          victim != last(bin) && 
          chunksize_nomask(victim) == chunksize_nomask(victim->fd)
        )
          victim = victim->fd;

        /* Unlink the chunk. */
        remainder_size = (size - nb);
        unlink_chunk(av, victim);

        /* Exhaust. */
        if (remainder_size < MINSIZE){
          set_inuse_bit_at_offset(victim, size);

          if (av != &main_arena){
            set_non_main_arena(victim);
          }
        }
        /* Or, split. */
        else{
          remainder = chunk_at_offset(victim, nb);

          /* We cannot assume the unsorted list is empty 
             and therefore have to perform a complete 
             insert here. */
          /* What does this even imply? Regardless of it 
             being empty or not, the insertion remains 
             the same. I don't know what it is pointing at. */
          bck = unsorted_chunks(av);
          fwd = bck->fd;

          if (__glibc_unlikely (fwd->bk != bck))
            malloc_printerr ("malloc(): corrupted unsorted chunks");

          remainder->bk = bck;
          remainder->fd = fwd;
          bck->fd = remainder;
          fwd->bk = remainder;

          /* If the remainder is large, set the skip list 
             pointers NULL. */
          if (!in_smallbin_range(remainder_size)){
            remainder->fd_nextsize = NULL;
            remainder->bk_nextsize = NULL;
          }

          set_head(
            victim, 
            nb | PREV_INUSE | (av != &main_arena ? NON_MAIN_ARENA : 0)
          );

          set_head(
            remainder, 
            remainder_size | PREV_INUSE
          );

          set_foot(remainder, remainder_size);
        }

        check_malloced_chunk (av, victim, nb);
        void *p = chunk2mem (victim);
        alloc_perturb (p, bytes);
        return p;
      }
    }

    /* [PATH 5]: Search a chunk in other large bins if 
        the large bin associated with nb is not able 
        to satisfy the request. 

      Use binmap to quickly check which large bins are 
      empty, starting from the next largest bin. 

      This search is strictly best-fit. The smallest 
      with ties going to approximately the least 
      recently used chunk that fits is selected.
      [UPDATE THIS LRU CLAIM]
    */

    /* The next large bin. */
    ++idx;

    /* The bin handler for this large bin. */
    bin = bin_at(av, idx);

    /* The binmap number this large bin belongs to. 
       Bin# [64, 95]  belongs to binmap 2.
       Bin# [96, 127] belongs to binmap 3. */
    block = idx2block(idx);

    /* The actual binmap. */
    map = av->binmap[block];

    /* (idx & 31) is the actual bit corresponding to 
       a bin number. But we can not use it directly. 
       So, we obtain the bit mask associated with this 
       bit. In simple words, it is the weight that 
       the bit carries. Later, we take a bitwise AND 
       with the actual binmap to find the status of 
       this bin. */
    bit = idx2bit(idx);

    for (;;){
      /* If the weight of the associated bit is greater 
         than the binmap itself, this large bin and the 
         large bins after it are all empty. So, skip all 
         the large bins in this block.
         [WHAT ABOUT bit==0 ?] */
      if (bit > map || bit == 0){
        do {
          /* We have 4 binmaps, only 2 of which are 
             for large bins, i.e. 2 and 3. If the 
             binmap number exceeds the max count, all 
             the large bins are empty, so use the top 
             chunk. */
          if (++block >= BINMAPSIZE)
            goto use_top;

          /* Move to the next binmap and break only if 
             it is non-empty. */
        } while((map = av->binmap[block]) == 0);

        /* If none of the large bins were usable, we 
           would have already jumped to the use_top 
           label. If we are here, a bin map had a 
           usable bin. */

        /* Left shift the binmap with BINMAPSHIFT to 
           obtain the index of the first large bin in 
           that binmap. */
        bin = bin_at(av, (block << BINMAPSHIFT));

        /* The first bin corresponds to the first bit, 
           i.e. 0. It's bit mask is 1 (i.e. 1 << 0). */
        bit = 1;
      }

      /* If we are here, there is a non-empty large bin 
         and the next goal is to find it. */
      /* Advance to bin with set bit. There must be one. */
      while ((bit & map) == 0){
        /* Advance to the next large bin's handler. */
        bin = next_bin(bin);

        /* Advance to the next bit. */
        bit <<= 1;

        /* Ensure we have not gone past the 31st bit, as 
           the result will wrap around. */
        assert (bit != 0);
      }

      /* Take the chunk at back. */
      victim = last(bin);

      /* When a large bin becomes empty, the allocator 
         doesn't clear its bit immediately. It defers 
         until that bin is explored via _int_malloc and 
         is found empty.

         A possible explanation is that most bins contain 
         multiple chunks and an extra branch would be 
         required every single time to check if the current 
         large bin has been emptied. */

      /* If a false alarm (empty bin), clear the bit. */
      if (victim == bin){
        av->binmap[block] = (map &= ~bit);
        bin = next_bin(bin);
        bit <<= 1;
      }
      /* If a usable bin is found, do what we have done 
         multiple times so far. */
      else{
        size = chunksize(victim);

        /*  We know the first chunk in this bin is big enough to use. */
        assert ((unsigned long)(size) >= (unsigned long)(nb));
        remainder_size = (size - nb);

        /* unlink */
        unlink_chunk(av, victim);

        /* Exhaust */
        if (remainder_size < MINSIZE){
          set_inuse_bit_at_offset(victim, size);

          if (av != &main_arena)
            set_non_main_arena (victim);
        }
        /* Split */
        else{
          remainder = chunk_at_offset(victim, nb);

          /* [AGAIN, NO IDEA]
             We cannot assume the unsorted list is empty 
             and therefore have to perform a complete 
             insert here. */
          bck = unsorted_chunks(av);
          fwd = bck->fd;

          if (__glibc_unlikely (fwd->bk != bck))
            malloc_printerr ("malloc(): corrupted unsorted chunks 2");

          remainder->bk = bck;
          remainder->fd = fwd;
          bck->fd = remainder;
          fwd->bk = remainder;

          /* advertise as last remainder */
          if (in_smallbin_range(nb))
            av->last_remainder = remainder;

          /* Update the skip the list pointers if the 
             remainder is large. */
          if (!in_smallbin_range(remainder_size)){
            remainder->fd_nextsize = NULL;
            remainder->bk_nextsize = NULL;
          }

          set_head(
            victim, 
            nb | PREV_INUSE | (av != &main_arena ? NON_MAIN_ARENA : 0)
          );
          set_head(remainder, remainder_size | PREV_INUSE);
          set_foot(remainder, remainder_size);
        }

        check_malloced_chunk(av, victim, nb);
        void *p = chunk2mem(victim);
        alloc_perturb(p, bytes);
        return p;
      }
    }


    /* [PATH 6]: Use the top chunk. */

    use_top:
      victim = av->top;
      size = chunksize(victim);

      /* Ensure the top size is not corrupted. */
      if (__glibc_unlikely (size > av->system_mem))
        malloc_printerr("malloc(): corrupted top size");

      /* [PATH 6A]: If the top chunk has enough memory 
          to exist after serving this request, split it.
      */
      if ((unsigned long)(size) >= (unsigned long)(nb + MINSIZE)){
        remainder_size = (size - nb);
        remainder = chunk_at_offset(victim, nb);
        av->top = remainder;

        /* Set the metadata of the chunk to return. */
        set_head(
          victim, 
          nb | PREV_INUSE | (av != &main_arena ? NON_MAIN_ARENA : 0)
        );

        /* Update the top chunk size. */
        set_head(remainder, remainder_size | PREV_INUSE);

        check_malloced_chunk(av, victim, nb);
        void *p = chunk2mem(victim);
        alloc_perturb(p, bytes);
        return p;
      }

      /* [PATH 6B]: Call sysmalloc and extent the top chunk if 
         the top chunk doesn't have enough memory.  */
      else{
        void *p = sysmalloc(nb, av);
        if (p != NULL)
          alloc_perturb(p, bytes);
          return p;
      }
  }
}

/* -------------------- free -------------------- */

/* Free chunk P of SIZE bytes to the arena.
   - HAVE_LOCK indicates where the arena for P has 
     already been locked.
   - Caller must ensure chunk and size are valid. */
static void _int_free_chunk(
  mstate av, mchunkptr p, 
  INTERNAL_SIZE_T size, 
  int have_lock
){
  /* Consolidate other non-mmapped chunks as they arrive. */
  if (!chunk_is_mmapped(p)) {

    /* Preserve errno in case block merging results in munmap. */
    int err = errno;

    /* If we're single-threaded, don't lock the arena. */
    if (SINGLE_THREAD_P)
      have_lock = true;

    if (!have_lock)
      __libc_lock_lock(av->mutex);

    _int_free_merge_chunk(av, p, size);

    if (!have_lock)
      __libc_lock_unlock(av->mutex);

    __set_errno(err);
  }

  /* If the chunk was allocated via mmap, release via munmap(). */
  else {

    /* Preserve errno in case munmap sets it. */
    int err = errno;

    /* See if the dynamic brk/mmap threshold needs adjusting.
       Dumped fake mmapped chunks do not affect the threshold. */
    if (
      !mp_.no_dyn_threshold && 
      chunksize_nomask(p) > mp_.mmap_threshold && 
      chunksize_nomask(p) <= DEFAULT_MMAP_THRESHOLD_MAX
    ){
      mp_.mmap_threshold = chunksize(p);
      mp_.trim_threshold = (2 * mp_.mmap_threshold);

      LIBC_PROBE(
        memory_mallopt_free_dyn_thresholds, 
        2, mp_.mmap_threshold, mp_.trim_threshold
      );
    }

    munmap_chunk(p);
    __set_errno(err);
  }
}

/* Try to merge chunk P of SIZE bytes with its neighbors. 
   - Put the resulting chunk on the appropriate bin list.
   - P must not be on a bin yet. It can be in use.

  Basically, backwards consolidation. */
static void _int_free_merge_chunk(
  mstate av, mchunkptr p, 
  INTERNAL_SIZE_T size
){
  /* Chunk (p+1) */
  mchunkptr nextchunk = chunk_at_offset(p, size);
  check_inuse_chunk(av, p);

  /* Lightweight tests */

  /* [TEST 1]: Check whether the block is already the top block. */
  if (__glibc_unlikely(p == av->top))
    malloc_printerr("double free or corruption (top)");

  /* [TEST 2]: Check whether the next chunk is beyond the 
     boundaries of the arena. */
  if (__glibc_unlikely(
    contiguous(av) && 
    (char*)(nextchunk) >= ((char*)(av->top) + chunksize(av->top))
  ))
    malloc_printerr("double free or corruption (out)");

  /* [TEST 3]: Check whether the block is actually not marked used. */
  if (__glibc_unlikely(!prev_inuse(nextchunk)))
    malloc_printerr("double free or corruption (!prev)");

  INTERNAL_SIZE_T nextsize = chunksize(nextchunk);
  if (__glibc_unlikely(
    chunksize_nomask(nextchunk) <= CHUNK_HDR_SZ || 
    nextsize >= av->system_mem
  ))
    malloc_printerr("free(): invalid next size (normal)");

  /* [?] */
  free_perturb(chunk2mem(p), size - CHUNK_HDR_SZ);


  /* Consolidate backward. */
  /* If the (p-1) chunk is free, consolidate it with `p`. */
  if (!prev_inuse(p)){
    INTERNAL_SIZE_T prevsize = prev_size(p);

    /* Add the size of `(p-1)` and `p` chunks in the size 
       variable of chunk `p`. */
    size += prevsize;

    /* Update `p` with the pointer to the `(p-1)` chunk. */
    p = chunk_at_offset(p, -((long)(prevsize)));

    if (__glibc_unlikely(chunksize(p) != prevsize))
      malloc_printerr("corrupted size vs. prev_size while consolidating");

    // Unlink chunk (p-1).
    unlink_chunk(av, p);
  }

  /* Perform forward consolidation (if possible), bin the chunk 
     and return the size of the final chunk. */
  size = _int_free_create_chunk(av, p, size, nextchunk, nextsize);
  _int_free_maybe_trim(av, size);
}

/* Create a chunk at P of SIZE bytes, with SIZE potentially 
   increased to cover the immediately following chunk 
   NEXTCHUNK of NEXTSIZE bytes (if NEXTCHUNK is unused).
   - The chunk at P is not actually read and does not have 
     to be initialized. After creation, it is placed on the 
     appropriate bin list.
   - The function returns the size of the new chunk.

  Basically, forward consolidation. */
static INTERNAL_SIZE_T _int_free_create_chunk(
  mstate av, mchunkptr p, 
  INTERNAL_SIZE_T size,
	mchunkptr nextchunk, 
  INTERNAL_SIZE_T nextsize
){
  /* [PATH 1]: The nextchunk isn't the top chunk. */
  if (nextchunk != av->top){
    bool nextinuse = inuse_bit_at_offset (nextchunk, nextsize);

    /* Consolidate forward. */
    if (!nextinuse){
    	unlink_chunk(av, nextchunk);
      size += nextsize;
      /* Size either contains (p-1, p, p+1)->mchunk_sizes, or
         (p, p+1)->mchunk_sizes; depending on whether backward
         consolidation happened or not. */
    }

    /* If nextchunk is an in-use chunk, we can not perform
       forward consolodation, but we have to update the 
       PREV_INUSE bit of this chunk to reflect that the 
       chunk previous to it is now free. */
    else{
      clear_inuse_bit_at_offset(nextchunk, 0);
    }


    /* After consolidation, we have to bin the resulting chunk. */

    /* Front and back pointers for the bin. */
    mchunkptr bck, fwd;

    /* [PATH 1A]: If large chunk, place it in the unsorted bin.

      Large chunks are placed in the unsorted bin. They are 
      given a chance to service the next malloc call, if 
      possible. They might improve the locality of chunks.
    */
    if (!in_smallbin_range(size)){
      bck = unsorted_chunks(av);
      fwd = bck->fd;

      if (__glibc_unlikely (fwd->bk != bck))
        malloc_printerr ("free(): corrupted unsorted chunks");

      /* Skip list pointers are not maintained in unsorted chunks. */
      p->fd_nextsize = NULL;
      p->bk_nextsize = NULL;
    }

    /* [PATH 1B]: If small chunk, place it in the appropriate smallbin. */
    else{
      int chunk_index = smallbin_index(size);
      bck = bin_at(av, chunk_index);
      fwd = bck->fd;

      if (__glibc_unlikely (fwd->bk != bck))
        malloc_printerr ("free(): chunks in smallbin corrupted");

      mark_bin(av, chunk_index);
    }

    /* Skip list pointers are not maintained in small chunks. */
    p->bk = bck;
    p->fd = fwd;

    bck->fd = p;
    fwd->bk = p;

    /* Update the size and PREV_INUSE bit of the resulting chunk. */
    set_head(p, size | PREV_INUSE);

    /* Update the next chunk's mchunk_prev_size with this chunk's size. */
    set_foot(p, size);

    check_free_chunk(av, p);
  }

  /* [PATH 2]: If the nextchunk is the top chunk, 
     consolidate it with the top chunk and update 
     av->top to point to `p`. */
  else{
    size += nextsize;

    set_head(p, size | PREV_INUSE);
    av->top = p;

    check_chunk(av, p);
  }

  return size;
}

/* If the total unused topmost memory exceeds the 
   trim threshold, ask malloc_trim to reduce top. */
static void _int_free_maybe_trim(
  mstate av, 
  INTERNAL_SIZE_T size
){
  /* We don't want to trim on each free. As a compromise, 
     trimming is attempted if ATTEMPT_TRIMMING_THRESHOLD 
     is reached. */
  if (size >= ATTEMPT_TRIMMING_THRESHOLD){
    /* For the main_arena, we call systrim. */
    if (av == &main_arena){

#ifndef MORECORE_CANNOT_TRIM
      if (chunksize(av->top) >= mp_.trim_threshold)
  	    systrim(mp_.top_pad, av);
#endif
    }

    /* For non-main arenas, we call heap_trim. Always try 
       heap_trim, even if the top chunk is not large, 
       because the corresponding heap might go away. */
    else{
      heap_info *heap = heap_for_ptr(top(av));
      assert(heap->ar_ptr == av);
      heap_trim(heap, mp_.top_pad);
    }
  }
}

/* -------------------- realloc -------------------- */

static void* _int_realloc(
  mstate av, mchunkptr oldp, 
  INTERNAL_SIZE_T oldsize,
	INTERNAL_SIZE_T nb
){
  mchunkptr        newp;            /* chunk to return */
  INTERNAL_SIZE_T  newsize;         /* its size */
  void*            newmem;          /* corresponding user mem */

  mchunkptr        next;            /* next contiguous chunk after oldp */

  mchunkptr        remainder;       /* extra space at end of newp */
  unsigned long    remainder_size;  /* its size */

  /* oldmem size */
  if (__glibc_unlikely(
    chunksize_nomask(oldp) <= CHUNK_HDR_SZ || 
    oldsize >= av->system_mem || 
    oldsize != chunksize(oldp)
  ))
    malloc_printerr("realloc(): invalid old size");

  check_inuse_chunk(av, oldp);

  /* All callers already filter out mmap'ed chunks. */
  assert(!chunk_is_mmapped(oldp));

  next = chunk_at_offset(oldp, oldsize);
  INTERNAL_SIZE_T nextsize = chunksize(next);

  if (__glibc_unlikely(
    chunksize_nomask(next) <= CHUNK_HDR_SZ || 
    nextsize >= av->system_mem
  ))
    malloc_printerr("realloc(): invalid next size");

  if ((unsigned long)(oldsize) >= (unsigned long)(nb)){
    /* already big enough; split below */
    newp = oldp;
    newsize = oldsize;
  }

  else{
    /* Try to expand forward into top. */
    if (
      next == av->top &&
      (unsigned long)(newsize = oldsize + nextsize) >= (unsigned long)(nb + MINSIZE)
    ){
      set_head_size(oldp, nb | (av != &main_arena ? NON_MAIN_ARENA : 0));
      av->top = chunk_at_offset (oldp, nb);
      set_head (av->top, (newsize - nb) | PREV_INUSE);
      check_inuse_chunk (av, oldp);
      return tag_new_usable(chunk2mem(oldp));
    }

    /* Try to expand forward into next chunk; split off remainder below. */
    else if (
      next != av->top &&
      !inuse(next) &&
      (unsigned long)(newsize = oldsize + nextsize) >= (unsigned long)(nb)
    ){
      newp = oldp;
      unlink_chunk(av, next);
    }

    /* Allocate, Copy, Free. */
    else{
      newmem = _int_malloc(av, nb - MALLOC_ALIGN_MASK);
      if (newmem == NULL)
        return NULL;

      newp = mem2chunk(newmem);
      newsize = chunksize(newp);

      /* Avoid copy if newp is next chunk after oldp. */
      if (newp == next){
        newsize += oldsize;
        newp = oldp;
      }
      else{
        void *oldmem = chunk2mem(oldp);
        size_t sz = memsize(oldp);
        (void)tag_region(oldmem, sz);
        newmem = tag_new_usable(newmem);

        memcpy(newmem, oldmem, sz);
        _int_free_chunk(av, oldp, chunksize (oldp), 1);
        check_inuse_chunk(av, newp);
        return newmem;
      }
    }
  }

  /* If possible, free extra space in old or extended chunk. */

  assert ((unsigned long)(newsize) >= (unsigned long)(nb));
  remainder_size = (newsize - nb);

  /* not enough extra to split off */
  if (remainder_size < MINSIZE){
    set_head_size (newp, newsize | (av != &main_arena ? NON_MAIN_ARENA : 0));
    set_inuse_bit_at_offset (newp, newsize);
  }

  /* Split remainder. */
  else{
    remainder = chunk_at_offset(newp, nb);

    /* Clear any user-space tags before writing the header. */
    remainder = tag_region(remainder, remainder_size);
    set_head_size(newp, nb | (av != &main_arena ? NON_MAIN_ARENA : 0));
    set_head(
      remainder, 
      remainder_size | PREV_INUSE | (av != &main_arena ? NON_MAIN_ARENA : 0)
    );

    /* Mark remainder as inuse so free() won't complain. */
    set_inuse_bit_at_offset(remainder, remainder_size);
    _int_free_chunk(av, remainder, chunksize(remainder), 1);
  }

  check_inuse_chunk(av, newp);
  return tag_new_usable(chunk2mem(newp));
}

/* -------------------- memalign -------------------- */

/* BYTES is user requested bytes, not requested chunksize bytes.
   ALIGNMENT is a power of 2 larger than or equal to MINSIZE. */
static void* _int_memalign(mstate av, size_t alignment, size_t bytes)
{
  mchunkptr p, newp;

  if ((bytes > PTRDIFF_MAX) || (alignment > PTRDIFF_MAX)){
    __set_errno (ENOMEM);
    return NULL;
  }
  size_t nb = checked_request2size(bytes);

  /* [?] */
  /* Call malloc with worst case padding to hit alignment.
    - ALIGNMENT is a power-of-2, so it tops out at 
      (PTRDIFF_MAX >> 1) + 1, leaving plenty of space to 
      add MINSIZE and whatever checked_request2size adds 
      to BYTES to get NB.
    - Consequently, total below also does not overflow. */
  void *m = _int_malloc(av, nb + alignment + MINSIZE);

  if (m == NULL)
    return NULL;

  p = mem2chunk(m);

  if (chunk_is_mmapped(p)){
    newp = mem2chunk(PTR_ALIGN_UP(m, alignment));
    p = mmap_set_chunk(
      mmap_base(p), 
      mmap_size(p),
      (uintptr_t)(newp) - mmap_base(p), 
      mmap_is_hp(p)
    );
    return chunk2mem(p);
  }

  size_t size = chunksize(p);

  /* If not already aligned, align the chunk. Add 
     MINSIZE before aligning so we can always free 
     the alignment padding. */
  if (!PTR_IS_ALIGNED(m, alignment)){
    newp = mem2chunk(ALIGN_UP((uintptr_t)m + MINSIZE, alignment));
    size_t leadsize = PTR_DIFF(newp, p);
    size -= leadsize;

    /* Create a new chunk from the alignment padding and free it. */
    int arena_flag = av != (&main_arena ? NON_MAIN_ARENA : 0);
    set_head (newp, size | PREV_INUSE | arena_flag);
    set_inuse_bit_at_offset (newp, size);
    set_head_size (p, leadsize | arena_flag);
    _int_free_merge_chunk (av, p, leadsize);
    p = newp;
  }

  /* Free a chunk at the end if large enough. */
  if (size - nb >= MINSIZE){
    mchunkptr nextchunk = chunk_at_offset(p, size);
    mchunkptr remainder = chunk_at_offset(p, nb);
    set_head_size(p, nb);
    size = _int_free_create_chunk(
      av, remainder, 
      size - nb, nextchunk,
      chunksize(nextchunk)
    );
    _int_free_maybe_trim(av, size);
  }

  check_inuse_chunk(av, p);
  return chunk2mem(p);
}


/* ------------------ malloc_trim ------------------ */

/* It does two things.

  [1] Walk every bin and check if a chunk's payload 
      memory contains at least one complete page. If 
      yes, notify the kernel to reclaim the physical 
      backing associated with that page. The virtual 
      memory mapping remains intact and it is backed 
      with physical memory in future if it is accessed 
      again, not necessarily by the same physical page 
      frame, though. It is a way to reduce pressure on 
      physical memory.
  [2] Call systrim to release memory from the top chunk.
*/
static int mtrim(mstate av, size_t pad)
{
  /* Page size. */
  const size_t ps = GLRO(dl_pagesize);

  /* Bin number corresponding to page size. */
  int psindex = bin_index(ps);

  /* Page mask (I hope so). */
  const size_t psm1 = ps - 1;

  /* Traverse all the bins. */

  int result = 0;
  for (int i = 1; i < NBINS; ++i){
    if (i == 1 || i >= psindex){
      /* Bin handler for bin #i. */
      mbinptr bin = bin_at(av, i);

      /* Start with the smaller size end. */
      for(
        (mchunkptr p = last(bin)); 
        (p != bin); 
        (p = p->bk)
      ){
        INTERNAL_SIZE_T size = chunksize(p);

        /* The payload memory must have at least one page. */
        if (size > (psm1 + sizeof(struct malloc_chunk))){
          /* Align the payload memory pointer to the next page. */
          char *paligned_mem = (char*)( 
            ((uintptr_t)(p) + sizeof(struct malloc_chunk) + psm1) 
            & ~psm1
          );

          assert ( ((char*) chunk2mem(p) + (2 * CHUNK_HDR_SZ)) <= paligned_mem);
          assert ( ((char*)(p) + size) > paligned_mem);

          /* 
            (paligned_mem - p) is the number of bytes from the 
             start of the chunk (including the header) that are 
             a part of a different page, not the first page 
             aligned address within the payload memory.

            (size - (paligned_mem - p)) is the number of bytes 
             from the first page-aligned address inside the 
             payload memory region up to the end of the chunk.
             These are the bytes that can be potentially freed.
             But it may or may not be page-aligned. To remove 
             the fragments of an incomplete page, we align it 
             down to the previous page.
          */
          size -= (paligned_mem - (char*)(p));

          /* Must be at least one page. */
          if (size > psm1){
#if MALLOC_DEBUG
            /* When debugging we simulate destroying the memory content. */
            memset(paligned_mem, 0x89, size & ~psm1);
#endif
            __madvise (paligned_mem, size & ~psm1, MADV_DONTNEED);
            result = 1;
          }
        }
      }
    }
  }

#ifndef MORECORE_CANNOT_TRIM
  return result | (av == &main_arena ? systrim(pad, av) : 0);
#else
  return result;
#endif
}


/* Keep at least `s` bytes at the top of the arena 
   and release the rest. */
int __malloc_trim(size_t s)
{
  int result = 0;
  mstate ar_ptr = &main_arena;

  /* Iterate over each arena. */
  do{
    __libc_lock_lock(ar_ptr->mutex);
    result |= mtrim(ar_ptr, s);

    __libc_lock_unlock(ar_ptr->mutex);
    ar_ptr = ar_ptr->next;
  } while (ar_ptr != &main_arena);

  return result;
}


/* --------------- malloc_usable_size --------------- */

static size_t musable(void *mem)
{
  mchunkptr p = mem2chunk(mem);

  if (chunk_is_mmapped(p))
    return memsize(p);

  else if (inuse(p))
    return memsize(p);

  return 0;
}

#if IS_IN (libc)
size_t __malloc_usable_size(void *m)
{
  if (m == NULL)
    return 0;

  return musable(m);
}
#endif

/* -------------------- mallinfo -------------------- */

/* Accumulate malloc statistics for arena AV into M. */
static void int_mallinfo(mstate av, struct mallinfo2 *m)
{
  size_t i;
  mbinptr b;
  mchunkptr p;
  INTERNAL_SIZE_T avail;
  int nblocks;

  check_malloc_state(av);

  /* Account for top. */
  avail = chunksize(av->top);
  nblocks = 1;  /* top always exists */

  /* Traverse regular bins. */
  for (i = 1; i < NBINS; ++i){
    b = bin_at (av, i);
    for (p = last(b); (p != b); (p = p->bk)){
      ++nblocks;
      avail += chunksize(p);
    }
  }

  m->ordblks  += nblocks;
  m->fordblks += avail;
  m->uordblks += av->system_mem - avail;
  m->arena += av->system_mem;

  if (av == &main_arena){
    m->hblks  = mp_.n_mmaps;
    m->hblkhd = mp_.mmapped_mem;
    m->usmblks  = 0;
    m->keepcost = chunksize(av->top);
  }
}

struct mallinfo2 __libc_mallinfo2(void)
{
  struct mallinfo2 m;
  mstate ar_ptr;

  memset(&m, 0, sizeof (m));
  ar_ptr = &main_arena;
  do{
    __libc_lock_lock (ar_ptr->mutex);
    int_mallinfo (ar_ptr, &m);
    __libc_lock_unlock (ar_ptr->mutex);

    ar_ptr = ar_ptr->next;
  } while (ar_ptr != &main_arena);

  return m;
}
libc_hidden_def(__libc_mallinfo2)

struct mallinfo __libc_mallinfo(void)
{
  struct mallinfo m;
  struct mallinfo2 m2 = __libc_mallinfo();

  m.arena    = m2.arena;
  m.ordblks  = m2.ordblks;
  m.smblks   = 0;
  m.hblks    = m2.hblks;
  m.hblkhd   = m2.hblkhd;
  m.usmblks  = m2.usmblks;
  m.fsmblks  = 0;
  m.uordblks = m2.uordblks;
  m.fordblks = m2.fordblks;
  m.keepcost = m2.keepcost;

  return m;
}


/* ------------------ malloc_stats ------------------ */

void __malloc_stats(void){
  int i;
  mstate ar_ptr;
  unsigned int in_use_b = mp_.mmapped_mem, system_b = in_use_b;

  _IO_flockfile (stderr);
  int old_flags2 = stderr->_flags2;
  stderr->_flags2 |= _IO_FLAGS2_NOTCANCEL;

  for (i = 0, ar_ptr = &main_arena;; i++){
    struct mallinfo2 mi;

    memset (&mi, 0, sizeof (mi));
    __libc_lock_lock (ar_ptr->mutex);
    int_mallinfo (ar_ptr, &mi);

    fprintf(stderr, "Arena %d:\n", i);
    fprintf(stderr, "system bytes     = %10u\n", (unsigned int) mi.arena);
    fprintf(stderr, "in use bytes     = %10u\n", (unsigned int) mi.uordblks);

#if MALLOC_DEBUG > 1
    if (i > 0)
      dump_heap (heap_for_ptr (top (ar_ptr)));
#endif

    system_b += mi.arena;
    in_use_b += mi.uordblks;

    __libc_lock_unlock(ar_ptr->mutex);
    ar_ptr = ar_ptr->next;

    if (ar_ptr == &main_arena)
      break;
  }

  fprintf(stderr, "Total (incl. mmap):\n");
  fprintf(stderr, "system bytes     = %10u\n", system_b);
  fprintf(stderr, "in use bytes     = %10u\n", in_use_b);
  fprintf(stderr, "max mmap regions = %10u\n", (unsigned int) mp_.max_n_mmaps);
  fprintf(stderr, "max mmap bytes   = %10lu\n", (unsigned long) mp_.max_mmapped_mem);

  stderr->_flags2 = old_flags2;
  _IO_funlockfile(stderr);
}


/* --------------- mallopt --------------- */

static __always_inline int
do_set_trim_threshold(size_t value)
{
  LIBC_PROBE(
    memory_mallopt_trim_threshold, 
    3, value, mp_.trim_threshold,
    mp_.no_dyn_threshold
  );
  mp_.trim_threshold = value;
  mp_.no_dyn_threshold = 1;
  return 1;
}

static __always_inline int
do_set_top_pad(size_t value)
{
  LIBC_PROBE(
    memory_mallopt_top_pad,
    3, value, mp_.top_pad,
	  mp_.no_dyn_threshold
  );
  mp_.top_pad = value;
  mp_.no_dyn_threshold = 1;
  return 1;
}

static __always_inline int
do_set_mmap_threshold(size_t value)
{
  LIBC_PROBE(
    memory_mallopt_mmap_threshold, 
    3, value, mp_.mmap_threshold,
	  mp_.no_dyn_threshold
  );
  mp_.mmap_threshold = value;
  mp_.no_dyn_threshold = 1;
  return 1;
}

static __always_inline int
do_set_mmaps_max(int32_t value)
{
  LIBC_PROBE(
    memory_mallopt_mmap_max, 
    3, value, mp_.n_mmaps_max,
	  mp_.no_dyn_threshold
  );
  mp_.n_mmaps_max = value;
  mp_.no_dyn_threshold = 1;
  return 1;
}

static __always_inline int
do_set_mallopt_check(int32_t value)
{
  return 1;
}

static __always_inline int
do_set_perturb_byte(int32_t value)
{
  LIBC_PROBE (memory_mallopt_perturb, 2, value, perturb_byte);
  perturb_byte = value;
  return 1;
}

static __always_inline int
do_set_arena_test(size_t value)
{
  LIBC_PROBE (memory_mallopt_arena_test, 2, value, mp_.arena_test);
  mp_.arena_test = value;
  return 1;
}

static __always_inline int
do_set_arena_max(size_t value)
{
  LIBC_PROBE (memory_mallopt_arena_max, 2, value, mp_.arena_max);
  mp_.arena_max = value;
  return 1;
}

#if USE_TCACHE
static __always_inline int
do_set_tcache_max(size_t value)
{
  if (value > PTRDIFF_MAX)
    return 0;

  size_t nb = request2size(value);
  size_t tc_idx = csize2tidx(nb);

  if (tc_idx >= TCACHE_SMALL_BINS)
    tc_idx = large_csize2tidx(nb);

  LIBC_PROBE(
    memory_tunable_tcache_max_bytes, 
    2, value, mp_.tcache_max_bytes
  );

  if (tc_idx < TCACHE_MAX_BINS){
    if (tc_idx < TCACHE_SMALL_BINS)
      mp_.tcache_small_bins = tc_idx + 1;

    mp_.tcache_max_bytes = nb + 1;
    return 1;
  }

  return 0;
}

static __always_inline int
do_set_tcache_count(size_t value)
{
  if (value <= MAX_TCACHE_COUNT){
    LIBC_PROBE(
      memory_tunable_tcache_count, 
      2, value, mp_.tcache_count
    );
    mp_.tcache_count = value;
    return 1;
  }
  return 0;
}

static __always_inline int
do_set_tcache_unsorted_limit (size_t value)
{
  LIBC_PROBE (memory_tunable_tcache_unsorted_limit, 2, value, mp_.tcache_unsorted_limit);
  mp_.tcache_unsorted_limit = value;
  return 1;
}
#endif

static __always_inline 
int do_set_mxfast (size_t value)
{
  return 1;
}

static __always_inline int
do_set_hugetlb (size_t value)
{
  if (value == 0){
    mp_.thp_mode = malloc_thp_mode_never;
  }
  else if (value == 1){
    mp_.thp_mode = __malloc_thp_mode();
    if (
      mp_.thp_mode == malloc_thp_mode_madvise || 
      mp_.thp_mode == malloc_thp_mode_always
    )
      mp_.thp_pagesize = __malloc_default_thp_pagesize ();
  }
  else if (value >= 2){
    __malloc_hugepage_config (value == 2 ? 0 : value, &mp_.hp_pagesize, &mp_.hp_flags);
  }
  return 0;
}

int __libc_mallopt(int param_number, int value)
{
  mstate av = &main_arena;
  int res = 1;

  __libc_lock_lock(av->mutex);

  LIBC_PROBE(memory_mallopt, 2, param_number, value);

  /* Many of these helper functions take a size_t. We do 
     not worry about overflow here, because negative int 
     values will wrap to very large size_t values and the 
     helpers have sufficient range checking for such 
     conversions. Many of these helpers are also used by 
     the tunables macros in arena.c.
  */

  switch (param_number){
    case M_MXFAST:
      res = do_set_mxfast (value);
      break;

    case M_TRIM_THRESHOLD:
      res = do_set_trim_threshold (value);
      break;

    case M_TOP_PAD:
      res = do_set_top_pad (value);
      break;

    case M_MMAP_THRESHOLD:
      res = do_set_mmap_threshold (value);
      break;

    case M_MMAP_MAX:
      res = do_set_mmaps_max (value);
      break;

    case M_CHECK_ACTION:
      res = do_set_mallopt_check (value);
      break;

    case M_PERTURB:
      res = do_set_perturb_byte (value);
      break;

    case M_ARENA_TEST:
      if (value > 0)
      	res = do_set_arena_test (value);
      break;

    case M_ARENA_MAX:
      if (value > 0)
        res = do_set_arena_max (value);
      break;
  }

  __libc_lock_unlock(av->mutex);
  return res;
}
libc_hidden_def(__libc_mallopt)


/* --------------- Alternative MORECORE functions --------------- */

/* General Requirements for MORECORE.

   The MORECORE function must have the following properties:

  If MORECORE_CONTIGUOUS is false:

  * MORECORE must allocate in multiples of pagesize. It will
    only be called with arguments that are multiples of pagesize.
  * MORECORE(0) must return an address that is at least
    MALLOC_ALIGNMENT aligned. (Page-aligning always suffices.)

  else (i.e. If MORECORE_CONTIGUOUS is true):

  * Consecutive calls to MORECORE with positive arguments
    return increasing addresses, indicating that space has been
    contiguously extended.
  * MORECORE need not allocate in multiples of pagesize.
    Calls to MORECORE need not have args of multiples of pagesize.
  * MORECORE need not page-align.

  In either case:

  * MORECORE may allocate more memory than requested. (Or even less,
    but this will generally result in a malloc failure.)
  * MORECORE must not allocate memory when given argument zero, but
    instead return one past the end address of memory from previous
    nonzero call. This malloc does NOT call MORECORE(0)
    until at least one call with positive arguments is made, so
    the initial value returned is not important.
  * Even though consecutive calls to MORECORE need not return contiguous
    addresses, it must be OK for malloc'ed chunks to span multiple
    regions in those cases where they do happen to be contiguous.
  * MORECORE need not handle negative arguments -- it may instead
    just return MORECORE_FAILURE when given negative arguments.
    Negative arguments are always multiples of pagesize. MORECORE
    must not misinterpret negative args as large positive unsigned
    args. You can suppress all such calls from even occurring by 
    defining MORECORE_CANNOT_TRIM,

  There is some variation across systems about the type of the
  argument to sbrk/MORECORE. If size_t is unsigned, then it cannot
  actually be size_t, because sbrk supports negative args, so it is
  normally the signed type of the same width as size_t (sometimes
  declared as "intptr_t", and sometimes "ptrdiff_t").  It doesn't much
  matter though. Internally, we use "long" as arguments, which should
  work across all reasonable possibilities.

  Additionally, if MORECORE ever returns failure for a positive
  request, then mmap is used as a noncontiguous system allocator. This
  is a useful backup strategy for systems with holes in address spaces
  -- in this case sbrk cannot contiguously expand the heap, but mmap
  may be able to map noncontiguous space.

  If you'd like mmap to ALWAYS be used, you can define MORECORE to be
  a function that always returns MORECORE_FAILURE.

  If you are using this malloc with something other than sbrk (or its
  emulation) to supply memory regions, you probably want to set
  MORECORE_CONTIGUOUS as false.  As an example, here is a custom
  allocator kindly contributed for pre-OSX macOS.  It uses virtually
  but not necessarily physically contiguous non-paged memory (locked
  in, present and won't get swapped out).  You can use it by
  uncommenting this section, adding some #includes, and setting up the
  appropriate defines above:

    *#define MORECORE osMoreCore
    *#define MORECORE_CONTIGUOUS 0

  There is also a shutdown routine that should somehow be called for
  cleanup upon program exit.

    *#define MAX_POOL_ENTRIES 100
    *#define MINIMUM_MORECORE_SIZE  (64 * 1024)

    static int next_os_pool;
    void *our_os_pools[MAX_POOL_ENTRIES];

    void *osMoreCore(int size){
      void *ptr = 0;
      static void *sbrk_top = 0;

      if (size > 0){
        if (size < MINIMUM_MORECORE_SIZE)
          size = MINIMUM_MORECORE_SIZE;

        if (CurrentExecutionLevel() == kTaskLevel)
          ptr = PoolAllocateResident(size + RM_PAGE_SIZE, 0);

        if (ptr == 0){
          return (void *) MORECORE_FAILURE;
        }

        // save ptrs so they can be freed during cleanup
        our_os_pools[next_os_pool] = ptr;
        next_os_pool++;
        ptr = (void *) ((((unsigned long) ptr) + RM_PAGE_MASK) & ~RM_PAGE_MASK);
        sbrk_top = (char *) ptr + size;
        return ptr;
      }

      else if (size < 0){
        // we don't currently support shrink behavior
        return (void *) MORECORE_FAILURE;
      }

      else{
        return sbrk_top;
      }
    }

    // cleanup any allocated memory pools
    // called as last thing before shutting down driver
    void osCleanupMem(void){
      void **ptr;

      for (ptr = our_os_pools; ptr < &our_os_pools[MAX_POOL_ENTRIES]; ptr++){
        if (*ptr){
          PoolDeallocate(*ptr);
          *ptr = 0;
        }
      }
    }

*/


/* Helper code. */

extern char **__libc_argv attribute_hidden;

static void malloc_printerr(const char *str)
{
#if IS_IN (libc)
  __libc_message("%s\n", str);
#else
  __libc_fatal(str);
#endif
  __builtin_unreachable();
}

#if USE_TCACHE

static volatile int dummy_var;

static __attribute_noinline__ void
malloc_printerr_tail(const char *str)
{
  /* Ensure this cannot be a no-return function. */
  if (dummy_var)
    return;
  malloc_printerr(str);
}
#endif

#if IS_IN (libc)

/* We need a wrapper function for one of the additions of POSIX. */
int __posix_memalign(void **memptr, size_t alignment, size_t size)
{
  void *mem;

  /* Test whether the SIZE argument is valid. It must 
     be a power of two multiple of sizeof(void*). */
  if (
    (alignment % sizeof(void*) != 0) ||
    !powerof2(alignment / sizeof(void*)) || 
    alignment == 0
  )
    return EINVAL;

  mem = _mid_memalign(alignment, size);

  if (mem != NULL){
    *memptr = mem;
    return 0;
  }

  return ENOMEM;
}

weak_alias (__posix_memalign, posix_memalign)
#endif


int __malloc_info(int options, FILE *fp)
{
  /* For now, at least. */
  if (options != 0)
    return EINVAL;

  int n = 0;
  size_t total_nblocks = 0;
  size_t total_avail   = 0;
  size_t total_system  = 0;
  size_t total_max_system = 0;
  size_t total_aspace  = 0;
  size_t total_aspace_mprotect = 0;

  fputs ("<malloc version=\"1\">\n", fp);

  /* Iterate over all arenas currently in use. */
  mstate ar_ptr = &main_arena;
  do {
    fprintf (fp, "<heap nr=\"%d\">\n<sizes>\n", n++);

    size_t nblocks = 0;
    size_t avail = 0;

    struct{
      size_t from;
      size_t to;
      size_t total;
      size_t count;
    } sizes[NBINS - 1];

#define  nsizes  (sizeof(sizes) / sizeof(sizes[0]))

    __libc_lock_lock (ar_ptr->mutex);

    /* Account for the top chunk. */
    avail = chunksize (ar_ptr->top);
    nblocks = 1;    /* Top always exists. */

    mbinptr bin;
    struct malloc_chunk *r;

    for (size_t i = 1; i < NBINS; ++i){
      bin = bin_at (ar_ptr, i);
      r = bin->fd;
      sizes[i - 1].from = ~((size_t) 0);
  	  sizes[i - 1].to = sizes[i - 1].total = sizes[i - 1].count = 0;

      if (r != NULL){
        while (r != bin){
          size_t r_size = chunksize_nomask (r);
          ++sizes[i - 1].count;
          sizes[i - 1].total += r_size;
          sizes[i - 1].from = MIN (sizes[i - 1].from, r_size);
          sizes[i - 1].to = MAX (sizes[i - 1].to, r_size);
          r = r->fd;
        }
      }

      if (sizes[i - 1].count == 0)
        sizes[i - 1].from = 0;

      nblocks += sizes[i - 1].count;
      avail += sizes[i - 1].total;
    }

    size_t heap_size = 0;
    size_t heap_mprotect_size = 0;
    size_t heap_count = 0;
    if (ar_ptr != &main_arena)

    {
      /* Iterate over the arena heaps from back to front. */
      heap_info *heap = heap_for_ptr (top (ar_ptr));
      do{
	      heap_size += heap->size;
	      heap_mprotect_size += heap->mprotect_size;
	      heap = heap->prev;
	      ++heap_count;
	    } while (heap != NULL);
    }

    __libc_lock_unlock(ar_ptr->mutex);

    total_nblocks += nblocks;
    total_avail   += avail;

    for (size_t i = 1; i < nsizes; ++i)
      if (sizes[i].count != 0)
        fprintf(
          fp, 
          "<size from=\"%zu\" to=\"%zu\" total=\"%zu\" count=\"%zu\"/>\n",
		      sizes[i].from, sizes[i].to, sizes[i].total, sizes[i].count
        );

    if (sizes[0].count != 0)
    	fprintf(
        fp, 
        "<unsorted from=\"%zu\" to=\"%zu\" total=\"%zu\" count=\"%zu\"/>\n",
		    sizes[0].from, sizes[0].to, sizes[0].total, sizes[0].count
      );

    total_system += ar_ptr->system_mem;
    total_max_system += ar_ptr->max_system_mem;

    fprintf(
      fp,
      "</sizes>\n"
      "<total type=\"rest\" count=\"%zu\" size=\"%zu\"/>\n"
      "<system type=\"current\" size=\"%zu\"/>\n"
      "<system type=\"max\" size=\"%zu\"/>\n",
      nblocks, avail, ar_ptr->system_mem, ar_ptr->max_system_mem
    );

    if (ar_ptr != &main_arena){
  	  fprintf(
        fp,
	  	  "<aspace type=\"total\" size=\"%zu\"/>\n"
	  	  "<aspace type=\"mprotect\" size=\"%zu\"/>\n"
	  	  "<aspace type=\"subheaps\" size=\"%zu\"/>\n",
	  	  heap_size, heap_mprotect_size, heap_count
      );
  	  total_aspace += heap_size;
  	  total_aspace_mprotect += heap_mprotect_size;
    }

    else{
      fprintf(
        fp,
        "<aspace type=\"total\" size=\"%zu\"/>\n"
        "<aspace type=\"mprotect\" size=\"%zu\"/>\n",
        ar_ptr->system_mem, ar_ptr->system_mem
      );
      total_aspace += ar_ptr->system_mem;
      total_aspace_mprotect += ar_ptr->system_mem;
    }

    fputs ("</heap>\n", fp);
    ar_ptr = ar_ptr->next;
  } while (ar_ptr != &main_arena);

  fprintf(
    fp,
	  "<total type=\"rest\" count=\"%zu\" size=\"%zu\"/>\n"
	  "<total type=\"mmap\" count=\"%d\" size=\"%zu\"/>\n"
	  "<system type=\"current\" size=\"%zu\"/>\n"
	  "<system type=\"max\" size=\"%zu\"/>\n"
	  "<aspace type=\"total\" size=\"%zu\"/>\n"
	  "<aspace type=\"mprotect\" size=\"%zu\"/>\n"
	  "</malloc>\n",
	  total_nblocks, total_avail,
	  mp_.n_mmaps, mp_.mmapped_mem,
	  total_system, total_max_system,
	  total_aspace, total_aspace_mprotect
  );

  return 0;
}

#if IS_IN (libc)
weak_alias (__malloc_info, malloc_info)

strong_alias (__libc_calloc, __calloc) weak_alias (__libc_calloc, calloc)
strong_alias (__libc_free, __free) strong_alias (__libc_free, free)
strong_alias (__libc_malloc, __malloc) strong_alias (__libc_malloc, malloc)
strong_alias (__libc_memalign, __memalign)
weak_alias (__libc_memalign, memalign)
strong_alias (__libc_realloc, __realloc) strong_alias (__libc_realloc, realloc)
strong_alias (__libc_valloc, __valloc) weak_alias (__libc_valloc, valloc)
strong_alias (__libc_pvalloc, __pvalloc) weak_alias (__libc_pvalloc, pvalloc)
strong_alias (__libc_mallinfo, __mallinfo)
weak_alias (__libc_mallinfo, mallinfo)
strong_alias (__libc_mallinfo2, __mallinfo2)
weak_alias (__libc_mallinfo2, mallinfo2)
strong_alias (__libc_mallopt, __mallopt) weak_alias (__libc_mallopt, mallopt)

weak_alias (__malloc_stats, malloc_stats)
weak_alias (__malloc_usable_size, malloc_usable_size)
weak_alias (__malloc_trim, malloc_trim)
#endif

#if SHLIB_COMPAT (libc, GLIBC_2_0, GLIBC_2_26)
compat_symbol (libc, __libc_free, cfree, GLIBC_2_0);
#endif
