- When it comes to freeing a chunk, we also check if 
  consolidation is possible.
- If chunk `p` is asked to be freed, we check if it 
  can be consolidated with `(p-1)` and `(p+1)` chunks, 
  i.e. backward and forward consolidation.

- The process of consolidation is simple.
  - When we consolidate backwards, we check if the `(p-1)` 
    chunk is actually free. We do this by checking the 
    PREV_INUSE bit of the `p` chunk. If free, we merge the 
    `(p-1)` and `p` chunks into `(p-1)`. While the `(p+1)` 
    chunk already has the PREV_INUSE bit clear (0), we are 
    still required to update mchunk_prev_size.
  - When we consolidate forward, we check if the `(p+1)` 
    chunk is free. We do this by checking the PREV_INUSE 
    bit of the `(p+2)` chunk. If free, we merge the `p` and 
    `(p+1)` chunks into `p`. Similarly, we have to update the 
    mchunk_prev_size of the `(p+2)` chunk.
  - Therefore, we need [p-1, p] chunks for backwards 
    consolidation and [p, p+1, p+2] chunks for forward 
    consolidation.
  - **Note**: Backward consolidation is not defined for the 
    first chunk.

- There can be three cases in consolidation.
  - "Backwards consolidation only". Here, `p` will get merged 
    into `(p-1)`.
  - "Forward consolidation only". Here, `(p+1)` is extended 
    into `p`.
  - "Full consolidation". `p` will be extended into `(p-1)`, 
    followed by the `(p+1)` chunk extending into the 
    resulting one. Only the `(p-1)` chunk will exist after 
    this operation, pointing to the consolidated memory.

- However, there is an edge case. When `p` borders with the 
  top chunk, `p` is consolidated into the top chunk. This is 
  opposite to what we have studied earlier, where the previous 
  chunk is what that survives.
- In simpler words, if the `(p+1)` chunk is the top chunk, the 
  chunk that survives consolidation and represents the 
  consolidated size is the top chunk.
- Also, forward consolidation requires the `(p+2)` chunk as well.
  But nothing exists after the top chunk.
- So, this edge case requires special-casing.

- In case of non-main arenas, when a heap segment (heap_info) has 
  reached its maximum capacity (HEAP_MAX_SIZE) and can no longer 
  service a request, a new heap is created. This is followed by 
  the creation of a new top chunk in the segment, and av->top is 
  updated. The old top is regularized and binned appropriately.
- Now that it is a regular chunk, it can be allocated and freed. 
  When it is freed, the allocator will consider the possibility 
  of consolidation, just as it does with other chunks.
- Two heap segments are distinct and can not be merged. What 
  separates them? If there is no separation, we will land into 
  another heap, which will corrupt the allocator state.


[CONTINUE FROM HERE]


- Both of these edge cases require special-casing. We have 
  to check if the
  - current chunk borders with the top chunk,
  - current chunk is the top chunk,
  - the current chunk is the first chunk.

- For a few calls, the overhead of these extra checks is 
  almost invisible. However, it becomes significant as the 
  number of calls increase.
- The question is, how we can avoid this special casing?
  - If `p` borders with the top chunk, we need one extra 
    chunk.
  - If `p` is the top chunk

- By having two more 
  chunks after the top chunk? Even if we had these extra chunks, how we will
  identify them as "special" or "the terminating ones" without any branching?
  Because, if we used any branching, that would defeat the purpose.

- Let's rewind how forward consolidation works. To merge `p` with the `(p+1)`
  chunk, we have to check the PREV_INUSE bit of the `(p+2)` chunk. If the
  `(p+2)` chunk had the PREV_INUSE bit set (1), forward consolidation can not 
  be performed.
- The two extra chunks that we will create after the top chunk must satisfy 
  some conditions.
  1. They should work correctly regardless of the `p` chunk bordering the 
     top chunk, or being the top chunk itself.
  2. They themselves can not undergo consolidation.

- We will call these extra chunks E1 and E2.
  - [CASE 1]: [p, top, E1, E2]   <::> Fwd Consolidation Possible.
  - [CASE 2]: [p=top, E1, E2]    <::> Fwd Consolidation not possible.

- Since we need mchunk_size for the PREV_INUSE bit, we are bound 
  to keep the first two members of the struct. But, we can avoid 
  the pointer fields. They have no use. So, these extra chunks 
  require (2*INTERNAL_SIZE_T) bytes of space.
- In-use, or free? We have this invariant that "a free chunk must 
  be surrounded by in-use chunks only". That's why, the top chunk 
  always had the PREV_INUSE bit set. The top chunk always borders 
  with an in-use chunk. But, what about the state of top chunk? 
  Is the top chunk an in-use chunk, or free chunk?
- Free chunks always reside in bins. If the top chunk was free, it 
  must be binned, which is not the case. If the top chunk was in-use, 
  we don't require any special-casing. Once a chunk is allocated to 
  the process, the allocator doesn't care if it sits idle. We need 
  the same status for the top chunk. We don't want top to be free, 
  but we don't want it to be allocated to the process either. To 
  tell the allocator not to disturb the top chunk, we can mark it 
  in-use, and the allocator will stop bothering us. That means, the 
  E1 chunk was must have the PREV_INUSE bit set (1).
- What about the E1 chunk? We will keep it as in-use. And we don't
  have to worry about the E2 chunk.
- But here is the catch. We still need a branch to identify that 
  the next chunk is av->top or not.
