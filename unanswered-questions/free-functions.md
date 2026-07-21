# Premise

These are the functions involved in freeing a chunk.
```c
/* free is its weak alias. */
void __libc_free(void *mem);

static void _int_free_chunk(
  mstate av, mchunkptr p, 
  INTERNAL_SIZE_T size, 
  int have_lock
);

static void _int_free_merge_chunk(
  mstate av, mchunkptr p, 
  INTERNAL_SIZE_T size
);

static INTERNAL_SIZE_T _int_free_create_chunk(
  mstate av, mchunkptr p, 
  INTERNAL_SIZE_T size,
	mchunkptr nextchunk, 
  INTERNAL_SIZE_T nextsize
);

static void _int_free_maybe_trim(
  mstate av, 
  INTERNAL_SIZE_T size
)
```

__libc_free is the top level function and we will ignore it in this discussion.

The prefix `_int` might be used to indicate ***an internal function***. `_free` might indicate that this function is related to freeing a chunk. `_trim` might be used to represent that this function attempts trimming. But I am confused about the rest.

Based on my understanding of these functions, 
  - _int_free_chunk is a thin wrapper that decides between regular free functions and munmap.
  - _int_free_merge_chunk is backward consolidation.
  - _int_free_create_chunk is forward consolidation.
  - _int_free_maybe_trim calls the right function to attempt trimming if the threshold is crossed.

---

# Question #1: How the name of these functions were decided?

A codebase that needs to be managed for a long time, I presume there is some naming convention in glibc!

The modern malloc implementation is based on ptmalloc2, which is itself based on dlmalloc@2.7.x. This repository on [GitHub](https://github.com/DenizThatMenace/dlmalloc/) lists some of the versions of dlmalloc, if not all. There are 2.7.0, 2.7.1 and 2.7.2 versions and all of them use a single free function. So, the naming is definitely not a historical artifact that glibc inherited.

This commit introduced _int_free_merge_chunk, _int_free_create_chunk, and _int_free_maybe_consolidate.
```
--------------------------------------------------
Commit: 542b1105852568c3ebc712225ae78b8c8ba31a78
Author: Florian Weimer <fweimer@redhat.com>
Date:   Fri Aug 11 14:48:17 2023
Message: malloc: Enable merging of remainders in memalign (bug 30723)
--------------------------------------------------

Previously, calling _int_free from _int_memalign could put remainders
into the tcache or into fastbins, where they are invisible to the
low-level allocator.  This results in missed merge opportunities
because once these freed chunks become available to the low-level
allocator, further memalign allocations (even of the same size are)
likely obstructing merges.

Furthermore, during forwards merging in _int_memalign, do not
completely give up when the remainder is too small to serve as a
chunk on its own.  We can still give it back if it can be merged
with the following unused chunk.  This makes it more likely that
memalign calls in a loop achieve a compact memory layout,
independently of initial heap layout.

Drop some useless (unsigned long) casts along the way, and tweak
the style to more closely match GNU on changed lines.
```

This commit introduced __int_free_chunk.
```
--------------------------------------------------
Commit: c621d4f74fcbb69818125b5ef128937a72f64888
Author: Wangyang Guo <wangyang.guo@intel.com>
Date:   Thu Aug 29 11:57:28 2024
Message: malloc: Split _int_free() into 3 sub functions
--------------------------------------------------

Split _int_free() into 3 smaller functions for flexible combination:
* _int_free_check -- sanity check for free
* tcache_free -- free memory to tcache (quick path)
* _int_free_chunk -- free memory chunk (slow path)
```

These commits do tell why a big function was broken down into smaller ones. And from the perspective of writing modular code, this act doesn't require much reasoning. But why these strange names were chosen is something that is missing.

I perfectly understand that naming can be a personal choice, and I am OK with that. But this is not a personal project.

Each of these functions have good annotations preceding them, and I didn't face any issues understanding them. But a function's name represents what it does and it is possible to have better names that reflect it.
