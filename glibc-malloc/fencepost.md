
- When it comes to freeing a chunk, we check if consolidation is possible.
- If chunk `p` is asked to be freed, we check if it can be consolidated with 
  `(p-1)` and `(p+1)` chunks, i.e. backward and forward consolidation.

- For backward consolidation, we need to know if the `(p-1)` chunk is free.
  We can identify this by the PREV_INUSE bit of the `p` chunk. So, we need
  [p-1, p] chunks for backward consolidation.
- For forward consolidation, we need to know if the `(p+1)` chunk is free.
  To identify this, we have to check the PREV_INUSE bit of the `(p+2)` 
  chunk. Therefore, we need [p, p+1, p+2] chunks for forward consolidation.

- As long as chunk `p` is surrounded by 2 or more chunks, consolidation 
  will not incur any problems. However, there are two edge cases.
- [CASE 1]: (p == (av->top - av->top->mchunk_size))
  - Here, `p` is the chunk just before the top chunk.
  - During backward consolidation, there can be two cases. If `p` is not 
    the first chunk, there is already a chunk before it. However, if `p` 
    is the first chunk in memory, there is no chunk preceding it. But 
    because the first chunk has the PREV_INUSE bit set always, backward
    consolidation will never be triggered.
  - During forward consolidation, if `p` is the chunk before the top chunk,
    the `(p+1)` chunk would be the top chunk. Since there is no chunk after 
    that, we can not access the `(p+2)` chunk.
- [CASE 2]: (p == av->top), i.e. the top chunk itself.
  - Both `(p+1)` and `(p+2)` chunks are inaccessible as they don't exist,
    which means, forward consolidation can not be done.

- Both of these edge cases require special-casing. We have to check if the
  - current chunk borders with the top chunk,
  - current chunk is the top chunk,
  - the current chunk is the first chunk.

- For a few calls, the overhead of these extra checks is almost invisible.
  However, it becomes significant as the number of calls increase.
- The question is, how can we avoid this special casing? By having two more 
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
