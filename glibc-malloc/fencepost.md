
- When it comes to freeing a chunk, we check if consolidation is possible.
- If chunk `p` is asked to be freed, we check if it can be consolidated with 
  `(p-1)` and `(p+1)` chunks, i.e. backward and forward consolidation.

- For backward consolidation, we need to know if the `(p-1)` chunk is free.
  We can identify this by the PREV_INUSE bit of the `p` chunk. So, we need
  [p-1, p] chunks for backward consolidation.
- For forward consolidation, we need to know if the `(p+1)` chunk is free.
  To identify this, we have to check the PREV_INUSE bit of the `(p+2)` 
  chunk. Therefore, we need [p, p+1, p+2] chunks for forward consolidation.

- As long as chunk `p` is in the middle of the allocated memory, surrounded 
  by multiple chunks, consolidation will not incur any problems. However, 
  there are two edge cases which greatly affect this process.
- [CASE 1]: (p == (av->top - av->top->mchunk_size))
  - Here, `p` is the chunk that is just before the top chunk.
  - During backward consolidation, there can be two cases. If `p` is not 
    the first chunk, there is already a chunk before it. However, if `p` 
    is the first chunk in memory, there is no chunk preceding it. But 
    because the first chunk has the PREV_INUSE bit set always, backward
    consolidation will never be triggered.
  - During forward consolidation, if `p` is the chunk before the top chunk,
    the `(p+1)` chunk would be the top chunk. There is no chunk after that,
    so accessing the `(p+2)` chunk is not possible here.
- [CASE 2]: (p == av->top), i.e. the top chunk itself.
  - Because there is nothing after the top chunk, we can not perform forward
    consolidation.

- Both of these edge cases require special-casing. We have to chunk if the
  - current chunk borders with the top chunk,
  - current chunk is the top chunk,
  - the current chunk is the first chunk.

- For a few calls, the overhead of these extra checks is almost insignificant.
  But as the number of calls increase, this can create performance issues.
- The question is, how can we avoid this special casing? By having two more 
  chunks after the top chunk? Even if we had these extra chunks, how we will
  identify them as "special" or "the terminating ones" without any branching?
  Because, if we used any branching, that would defeat the purpose.

- Let's rewind how forward consolidation works. To merge `p` with the `(p+1)`
  chunk, we have to check the PREV_INUSE bit of the `(p+2)` chunk. If the
  `(p+2)` chunk had the PREV_INUSE bit set (1), forward consolidation can not 
  be performed.
- The two extra chunks that we will create after the top chunk, they must 
  satisfy some conditions.
  1. They should work correctly regardless of the `p` chunk bordering the 
     top chunk, or being the top chunk itself.
  2. They themselves can not undergo consolidation.

- Let's go case-by-case. We will call these extra chunks E1 and E2. The 
  state of memory would be: [...., top, E1, E2]
- When the `p` chunk is bordering with the top chunk, forward consolidation 
  with the top chunk is possible. The question is, what should be the 
  PREV_INUSE bit of the E1 chunk? How should be keep the top chunk? in-use 
  or free?
