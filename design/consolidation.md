# Consolidation

When the allocator is asked to free a chunk, it also explores the possibility of consolidation. If chunk (p) is asked to be freed, we check if it can be consolidated with (p-1) and (p+1) chunks, i.e. backward and forward consolidation.

The process of consolidation is quite simple.
  - To consolidate backward, we check the PREV_INUSE bit of the (p) chunk to find if the (p-1) chunk is free. If free, we merge the (p-1) and (p) chunks into (p-1).
  - To consolidate forward, we check the PREV_INUSE bit of the (p+2) chunk to find if the (p+1) chunk is free. If free, we merge the (p) and (p+1) chunks into (p).

To summarize,
  - backwards consolidation requires access to (p-1) and (p) chunks, and
  - forward consolidation requires access to (p), (p+1), and (p+2) chunks.

Remember that
  - backward consolidation is not defined for the first chunk, and
  - the last chunk that can be consolidated is the one that borders with the top chunk.

---

There can be three situations in consolidation.
  1. "Backwards consolidation only", where (p) is merged into (p-1).
  2. "Forward consolidation only", where (p+1) is merged into (p).
  3. "Full consolidation", where (p) is extended into (p-1), followed by (p+1) extending into the resulting one. The (p-1) pointer will point to the consolidated memory.

However, there are three edge cases.

\[EDGE CASE #1\]: Top chunk in the main arena.
  - When (p) borders with the top chunk, i.e. [p, (p+1) == top], forward consolidation will lead to the merger of the top chunk and (p) into (p). So, we have to update av->top to point to (p).
  - Forward consolidation requires access to (p+2) chunk, but there is no chunk after the top chunk. So we have to make this a special case, like `((p+1) == av->top)`.


\[EDGE CASE #2\]: A new heap is created in a non-main arena.
  - In non-main arenas, when the existing heap segment (heap_info) can not be used to service the request, a new heap is created, followed by the creation of the new top chunk and av->top is updated to it.
  - The old top is regularized and binned appropriately so that it can be used as a normal chunk. Now it has all the perks of a regular chunk, including consolidation.
  - Heap segments can not be merged, therefore, forward consolidation is undefined for the last usable chunk in the old heap.
  - In [EDGE CASE #1], the last chunk was specially identified by av->top. When there is no top chunk in a heap segment, how we are going to identify if a chunk is at the high end of the current heap segment?


\[EDGE CASE #3\]: A positive foreign sbrk in the main arena. Explained below.

---

So, we have to handle two cases.
  1. [p, (p+1) == high_end, (p+2) == missing].
  2. [p == high_end, (p+1) == missing, (p+2) == missing].

Both the cases are possible regardless of the type of arena. They just require the right conditions to get triggered.

When (p) is the second last usable chunk, we have a deficit of one chunk. When (p) is the last usable chunk, we have a deficit of two chunks. Therefore, we need two chunks at max to solve this problem. Let's call them extra chunks E1 and E2.
  1. [p, (p+1) == high_end, (p+2) = E1, (p+3) = E2]
  2. [p == high_end, (p+1) = E1, (p+2) = E2]


How much space is required for these chunks? These chunks are always "in-use" and can not be consolidated. Therefore, we only need space for mchunk_prev_size and mchunk_size, i.e. CHUNK_HDR_SZ bytes.

What will be the state of the PREV_INUSE bit?
  - We have to maintain the invariant that a free chunk is always surrounded by in-use chunks.
  - To prevent E1 from consolidating, E2's PREV_INUSE bit must be set (1).
  - E1's PREV_INUSE bit is dependent on the state of the last usable chunk.
  - Both E1 and E2 must are in-use chunks.

Let's see if this solution works.

[p, (p+1) == high_end, (p+2) == E1]
  - We have checked the PREV_INUSE bit of the (p+2) chunk to know if the (p+1) chunk is free.
  - It may or may not be free. So, forward consolidation is possible.

[p == high_end, (p+1) == E1, (p+2) == E2]
  - We have checked the PREV_INUSE bit of the (p+2) chunk. It will always be set (1).
  - Therefore, for the last usable chunk, the next chunk is always "in-use", preventing forward consolidation.


These extra chunks are called fenceposts. They are used at two places in malloc.
  1. In the non-main arena path, when a heap segment can not be used to service a request, a new heap segment is set up. The existing top chunk is regularized and a new top chunk is created. Earlier, the last chunk can be easily identified with av->top. Now it can't be.
  2. In the main arena path, a positive foreign sbrk corrupts the internal bookkeeping of the allocator. So, the allocator have to work extra and reestablish the top chunk after the foreign sbrk and the old top is regularized.

In either case, the existing top chunk is regularized and a new top chunk is established. To create a boundary after the last usable chunk, we use the fencepost chunks.

---

The space for these fencepost chunks is carved out from the old top chunk.

We can notice that we need MINSIZE bytes for these fencepost chunks. But it is not the case always.

\[CASE #1\]: (top_size == MINSIZE)
  - The top chunk has exactly as many bytes as it is required to setup the fencepost chunks.
  - So, there will be no remainder to regularize.

\[CASE #2\]: (top_size >= (2 * MINSIZE))
  - The top chunk has twice as many bytes as it is required to setup the fencepost chunks.
  - So, the remainder is a valid chunk and it can be regularized.

\[CASE #3\]: (top_size == (MINSIZE + CHUNK_HDR_SZ))
  - Here, the remainder will have CHUNK_HDR_SZ bytes. But the smallest possible chunk has MINSIZE bytes in it. Therefore, the remainder can not ve regularized.
  - The extra bytes are carried away by fencepost-1.

---

There is another aspect about the mchunk_size field of the fenceposts.
  - In the non-main arena path, fencepost-2's mchunk_size is 0 bytes, i.e. `(0 | PREV_INUSE)`
  - In the main arena path, fencepost-2's mchunk_size is CHUNK_HDR_SZ bytes, i.e. `(CHUNK_HDR_SZ | PREV_INUSE)`.

I don't know why it is different.
