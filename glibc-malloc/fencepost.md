- Along side freeing, the allocator also explores the 
  possibility of consolidation.
- If chunk `p` is asked to be freed, we check if it 
  can be consolidated with `(p-1)` and `(p+1)` chunks,
  i.e. backward and forward consolidation.


- The process of consolidation is simple.
  - When we consolidate backwards, we check the PREV_INUSE 
    bit of the `(p+2)` chunk to identify if the `(p-1)` 
    chunk is free.
    - If free, we merge the `(p-1)` and `p` chunks into
      `(p-1)`.
    - While the `(p+1)` chunk already has the PREV_INUSE
      bit clear (0), we are still required to update
      it's mchunk_prev_size.
  - When we consolidate forward, we check the PREV_INUSE 
    bit of the `(p+2)` chunk to identify if the `(p+1)` 
    chunk is free. If free, we merge the `p` and `(p+1)` 
    chunks into `p` and update the mchunk_prev_size of 
    the `(p+2)` chunk.
  - To summarize,
    - Backwards consolidation: [p-1, p] chunks. 
    - Forward consolidation: [p, p+1, p+2] chunks.
  - **Notes**:
    - Backward consolidation is not defined for the first
    chunk.
    - The last chunk that can be consolidated is the one
      that borders with the top.


- There can be three cases in consolidation.
  - "Backwards consolidation only". Here, `p` will be
    consolidated into `(p-1)`.
  - "Forward consolidation only". Here, `(p+1)` will be
    consolidated into `p`.
  - "Full consolidation". `p` will be extended into `(p-1)`, 
    followed by `(p+1)` extending into the resulting one.
    The `(p-1)` chunk will point to the consolidated memory.


- There is an edge case. When `p` borders with the top chunk,
  this is how the memory would look like: [p, top].
- When we perform forward consolidation here, the top will be
  consolidated into `p`, and the resulting chunk would be `p`,
  not top. So, we have to change `av->top` to `p`. We don't
  have to unlink `p` as this function is called from
  int_free_merge_chunk() which requires `p` to be not binned
  already.
- Also, the process requires to access the `(p+2)` chunk, which
  is not possible as there is nothing beyond the top chunk.
- So, this case demands special-casing.


- In case of non-main arenas, when a heap segment (heap_info)
  has reached its maximum capacity (HEAP_MAX_SIZE), it can no
  longer service a request. So, a new heap is created, which
  is followed by the creation of the new top chunk for this
  segment, and av->top will be updated.
- The old top is regularized and binned appropriately so the
  process can use it normally. Because it is a regular chunk
  now, it can undergo consolidation as well. But it is the
  last chunk in that heap and two heaps can not be merged. So,
  forward consolidation is not defined for this chunk.
- This brings us to another special-case. If there are 5 heap
  segments, there will be four with no boundaries. How we are
  going to separate them?
- We need a solution that can act as a boundary between two
  heap segments and prevent forward consolidation for the
  chunk that borders with the high end of the heap segment.
- There is another issue we need to take in account. When
  `(p+1)` is the chunk at the high end of the heap segment
  and `p` demands `(p+2)` to know if `(p+1)` is free or not,
  how it will do that? There is no `(p+2)` chunk.

- So, we have two cases to handle.
  - [p, (p+1) == high_end, (p+2) == missing].
  - [p == high_end, (p+1) == missing, (p+2) == missing].
- In simple words, we need two chunks at the high end of a
  heap segment, after the last usable chunk. These chunks
  act as a barrier between the two heaps. Now the question
  is, how they will prevent consolidation?
- Let's call these chunks E1 and E2.
- Let's start with their size. We need mchunk_size for the
  PREV_INUSE bit, so we need the first two members in the
  struct. The pointer fields are useless forever. So,
  `(2 * INTERNAL_SIZE_T)` is enough.
- Now the PREV_INUSE bit. We have to maintain the invariant
  that a free chunk is always surrounded by in-use chunk.
  - To prevent E1 from consolidating, E2's PREV_INUSE must
    be set (1).
  - E1's PREV_INUSE is directly the face of the high end
    chunk. It must be tuned as its state changes.
- All in all, both E1 and E2 must be in-use chunks to
  prevent drama.

- Let's see if this solution works.
  - [p, (p+1) == high_end, (p+2) == E1]
    - We want to consolidate `p` and `(p+1)`. To know if
      `(p+1)` is free, we have checked `(p+2)`. It has
      the PREV_INUSE bit clear (0), so it can be
      consolidated. `p` and `(p+1)` are consolidated into
      `p`.
  - [p == high_end, (p+1) == E1, (p+2) == E2]
    - We want to consolidate `p` forward. We checked `(p+2)`
      chunk and its PREV_INUSE bit is set (1). We can't
      merge with this chunk. Consolidation stopped.

- These two chunks are called fencepost.


- While these fenceposts span across (2 * INTERNAL_SIZE_T) 
  bytes, their mchunk_size is set up differently. The first 
  fencepost has a size of CHUNK_HDR_SZ bytes, while the 
  second fencepost has a size of 0 bytes.
- This done so that, we can access fencepost-2 with the 
  usual chunk arithmetic, but we can not access anything 
  past fencepost-2.
