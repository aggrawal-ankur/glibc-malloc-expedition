/* [EXPERIMENT #2] */

/* [OBJECTIVE]: Perform structural analysis of an 
                in-use chunk.
*/

/* [SETUP]: Allocate a chunk of 20 bytes and create 
    a breakpoint on line #15.
*/

#include <stdlib.h>

int main(void){
  char *c = malloc(20);
  int break_point = 1;

  free(c);
}

/* [ANALYSIS] 

  On breakpoint-1, we will analyze the in-use state 
  of the chunk.
  ```
  (gdb) print *(mchunkptr)(c - 16)
  $1 = {
    mchunk_prev_size = 0, 
    mchunk_size = 33, 
    fd = 0x0, 
    bk = 0x0, 
    fd_nextsize = 0x0, 
    bk_nextsize = 0x20fe1
  }
  ```

  [1] The size of this chunk is 32 bytes, not 20 bytes.
      This is the result of request2size(20).

  [2] The PREV_INUSE bit of every newly allocated chunk 
      (not reused) is always set because they are carved 
      from the top chunk and the top chunk is always 
      preceded by in-use chunks. If the chunk before it 
      is freed, it is coalesced. This is explored in a 
      future experiment.

  [3] mchunk_prev_size is not maintained as the PREV_INUSE 
      bit is set. However, it often has garbage values 
      when the memory in that region is used up.

  [4] The fd/bk fields are 0x0 as the chunk is freshly 
      carved and the memory returned by the kernel is 
      zeroed. However, when the payload memory is used, 
      or the chunk is reused, these fields may contain 
      garbage values.

  [5] The nextsize fields are functionally not available 
      to this chunk, as the size is MINSIZE bytes only. 
      When they are, they are 0x0, if freshly carved, or 
      garbage, if reused. Meanwhile, here these fields 
      are overlapped with the top chunk's prev_size and 
      mchunk_size fields, which explains the values in 
      them.
*/
