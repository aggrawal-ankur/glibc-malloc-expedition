# sysmalloc Allocation Pathways

## Path #1 :: The size qualifies for mmap()

1. nb >= mmap_threshold
2. HAVE_MMAP == true
3. n(Currently mmapped chunks) <= n(Max possible mmapped chunks)

Use `sysmalloc_mmap()` if the above conditions satisfy.

## Precursor conditions for the following paths

1. Top chunk config:
   1. Size, excluding the lower 3-bits >= MINSIZE
   2. PREV_INUSE bit is set (1).

2. Top size < (nb + MINSIZE)

## Path #2 :: Non main-arena

av != &main_arena

1. Extend the current heap.
2. Create a new heap.
3. If mmap is not already tried (path #1), try sysmalloc_mmap().

## Path #3 :: Main Arena

av == &main_arena

1. Subtract existing top size from the required size (nb).
2. Round the obtained size to a multiple of page size or huge page size (confirm what).


---

sysmalloc is doing two jobs: 1) delegating control to mmap(), responsibly (via sysmalloc_mmap and sysmalloc_mmap_fallback), and 2) extending the top so that the request can be carved from the "resulting top" (in case of main_arena).
