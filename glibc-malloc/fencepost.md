- Along side freeing, the allocator also explores the 
  possibility of consolidation.
- If chunk `p` is asked to be freed, we check if it 
  can be consolidated with `(p-1)` and `(p+1)` chunks,
  i.e. backward and forward consolidation.


- The process of consolidation is simple.
  - When we consolidate backwards, we check the PREV_INUSE 
    bit of the `p` chunk to identify if the `(p-1)` chunk
    is free.
    - If free, we merge the `(p-1)` and `p` chunks into
      `(p-1)`.
    - While the `(p+1)` chunk already has the PREV_INUSE
      bit clear (0), we are still required to update it's 
      mchunk_prev_size.
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


- There can be three cases.
  - "Backwards consolidation only", where, `p` is merged into `(p-1)`.
  - "Forward consolidation only", where, `(p+1)` is merged into `p`.
  - "Full consolidation", where `p` is extended into `(p-1)`, 
    followed by `(p+1)` extending into the resulting one. The `(p-1)` 
    chunk will point to the consolidated memory.


- There are two edge cases.

[EDGE CASE #1]
- When `p` borders with the top chunk, i.e. [p, (p+1) == top], 
  forward consolidation will lead to the merger of top and `p`
  into `p`. This would corrupt av->top, and it must be updated
  to point to `p`.
- We don't have to unlink `p` as this function is called from
  int_free_merge_chunk() which requires `p` to be not binned
  already.
- Also, forward consolidation requires the `(p+2)` chunk. But
  nothing exists after the top chunk!
- This is not a big issue. All we need is a special-case that
  checking `((p+1) == av->top)`.

[EDGE CASE #2]
- In non-main arenas, when a heap segment (heap_info) has used 
  its maximum capacity (HEAP_MAX_SIZE), it can no longer service 
  a request. A new heap is created, followed by the creation of 
  the new top chunk for this segment, and av->top is updated.
- The old top is regularized and binned appropriately so that it 
  can be used as a normal chunk. Now it has all the perks of a 
  regular chunk, including consolidation.
- Heap segments are individual entities which can not be merged. 
  That means, the last chunk in the old heap must not exceed its 
  boundaries, making forward consolidation undefined for this 
  chunk. But what separates two heap segments?
- We need a boundary between two heap segments that can prevent
  forward consolidation for the chunk that borders with the high
  end of the heap segment.
- In [EDGE CASE #1], the last chunk was specially identified by
  av->top. When there is no top chunk in a heap segment, how we
  are going to identify if a chunk is at the high end of the
  current heap segment?


- We have two cases to handle.
  - [p, (p+1) == high_end, (p+2) == missing].
  - [p == high_end, (p+1) == missing, (p+2) == missing].
- Clearly, we can sense the absence of two special chunks at the 
  high end of a heap segment. These chunks will act as a barrier 
  between two heaps. Let's call these chunks E1 and E2.
- Let's talk about their structure. We need mchunk_size for the 
  PREV_INUSE bit, so we need the first two members. As usual, the 
  pointer fields are not required. So, `(2 * INTERNAL_SIZE_T)` 
  bytes is enough.
- Now the PREV_INUSE bit. We have to maintain the invariant that a 
  free chunk is always surrounded by in-use chunk.
  - To prevent E1 from consolidating, E2's PREV_INUSE bit must be 
    set (1).
  - E1's PREV_INUSE is directly the face of the last usable chunk 
    at the high end. It must be tuned as its state changes.
- All in all, both E1 and E2 must be in-use chunks to prevent drama.

- Let's see if this solution works.
  - [p, (p+1) == high_end, (p+2) == E1]
    - We want to consolidate `p` and `(p+1)`. To know if `(p+1)` 
      is free, we have checked `(p+2)`. It has the PREV_INUSE bit 
      clear (0), so it can be consolidated. `p` and `(p+1)` are 
      consolidated into `p`.
  - [p == high_end, (p+1) == E1, (p+2) == E2]
    - We want to consolidate `p` forward. We checked `(p+2)` chunk 
      and its PREV_INUSE bit is set (1). We can't merge with this 
      chunk. Consolidation didn't occurred.


- GLIBC has a name for these chunks. The fencepost chunks.
- While these fenceposts span across (2 * INTERNAL_SIZE_T) bytes, 
  their mchunk_size is set up differently. The first fencepost has 
  a size of CHUNK_HDR_SZ bytes, while the second fencepost has a 
  size of 0 bytes.
- This is done so that we can access fencepost-2 with the usual 
  chunk arithmetic, but we can not access anything past fencepost-2.
  (But I am not sure about it).
