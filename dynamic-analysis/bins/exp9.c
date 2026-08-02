/* [EXPERIMENT #9] */

/* [OBJECTIVE]: The top chunk is always preceded by 
                an in-use chunk. 
*/

/* [SETUP] 

  [1] Allocate two chunks.
  [2] Free the second chunk, the one bordering with 
      the top chunk.
  [3] Set a breakpoint on the free call and step into 
      it with (s).
  [4] Analyze the path.
*/

#include <stdlib.h>

int main(void){
  char* c1 = malloc(20);
  char* c2 = malloc(20);
  free(c2);
}

/* [ANALYSIS] 

  After the tcache logic, we step into _int_free_chunk.
  Since it is a regular chunk, so the !chunk_is_mmapped 
  is taken and _int_free_merge_chunk is called. Step 
  into it. We know that this function performs backward 
  consolidation. Since c1 is in-use, it is a no-op.

  Next comes _int_free_create_chunk. It performs forward 
  consolidation. We have passed the arena, the chunk, its 
  size, the nextchunk and its size. The next chunk after 
  `c` must be the top chunk. Let's verify by printing the 
  chunk in nextchunk and av->top.
  ```
  (gdb) p *(mchunkptr) (nextchunk)
  $1 = {
    mchunk_prev_size = 0,
    mchunk_size = 135105,
    fd = 0x0,
    bk = 0x0,
    fd_nextsize = 0x0,
    bk_nextsize = 0x0
  }

  (gdb) p *(mchunkptr) (av->top)
  $2 = {
    mchunk_prev_size = 0,
    mchunk_size = 135105,
    fd = 0x0,
    bk = 0x0,
    fd_nextsize = 0x0,
    bk_nextsize = 0x0
  }

  (gdb) p (nextchunk == av->top)
  $3 = 1
  ```

  Let's step into _int_free_create_chunk. The else 
  path would be taken as nextchunk == av->top.
  - We add the size of the top chunk and `c2`.
  - Then we make a new chunk at the combined mem.
  - Last, we update av->top to point to it.

  Therefore, the top chunk is always preceded by an 
  in-use chunk, which is why freshly carved chunks 
  always have their PREV_INUSE bit set.
*/
