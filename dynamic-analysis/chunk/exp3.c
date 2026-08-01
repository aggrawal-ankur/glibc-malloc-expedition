/* [EXPERIMENT #3] */

/* [OBJECTIVE]: The top chunk and the boundary tag implementation. */

/* [SETUP] 

  [1] Allocate a chunk of 20 bytes and set a breakpoint.
  [2] Populate the memory and set a breakpoint.
  [3] Free the chunk and set a breakpoint.

*/

#include <stdlib.h>
#include <string.h>

int main(void){
  char *c = malloc(20);
  int breakpoint = 1;

  memcpy(c, "Systems are my buddy", 20);
  breakpoint = 2;

  free(c);
  breakpoint = 3;
}

/* [ANALYSIS] 

  Inspect the chunk at breakpoint-1.
  ```
  (gdb) print *(mchunkptr)(c-16)
  $1 = {
    mchunk_prev_size = 0, 
    mchunk_size = 33, 
    fd = 0x0, 
    bk = 0x0, 
    fd_nextsize = 0x0, 
    bk_nextsize = 0x20fe1
  }
  ```

  As per the boundary tag implementation, there has to 
  be a dummy chunk after the last allocated chunk. 
  Let's access it.
  ```
  (gdb) p *(mchunkptr) ((c-16) + ((mchunkptr)(c-16))->mchunk_size-1)
  $2 = {
    mchunk_prev_size = 0, 
    mchunk_size = 135137, 
    fd = 0x0, 
    bk = 0x0, 
    fd_nextsize = 0x0, 
    bk_nextsize = 0x0
  }
  ```

  (c-16) brings us to the head of `c`. Then we access 
  the mchunk_size of `c`, subtract 1 for the PREV_INUSE 
  bit and add it to the base address. In other words,
  ```
  mchunkptr ptr = (mchunkptr) (c-16);
  size_t c_size = ptr->mchunk_size;
  mchunkptr top = (mchunkptr) ((char*)(ptr) + c_size);
  ```

  135137 is 0x20fe1 in hex. That's what bk_nextsize is 
  showing.

  This is that dummy chunk or, the top chunk, that exist 
  after the last usable chunk to implement the boundary 
  tag method.

  ---

  If we try to build the structure of memory, it'd be:

  ...............Main Chunk (request2size(20))...............
  [ prev_size  mchunk_size  fd  bk  fd_nextsize  bk_nexsize ]
        0          33       0x0 0x0     0x0        135137     0x0 0x0     0x0          0x0
                                    [ prev_size  mchunk_size  fd  bk  fd_nextsize  bk_nextsize ]
                                    ..........................Top Chunk.........................

  [NOTE]: The fields overlapped because of MINSIZE.

  ---

  Continue the program with `c` and the memcpy instruction 
  fills the payload memory with a stream of characters. If 
  we print the contents, we get:
  ```
  (gdb) p c
  $3 = 0x55b582cf2010 "Systems are my buddy"
  ```

  Now print the chunk.
  ```
  (gdb) p *(mchunkptr)(c-16)
  $4 = {
    mchunk_prev_size = 0, 
    mchunk_size = 33, 
    fd = 0x20736d6574737953, 
    bk = 0x6220796d20657261, 
    fd_nextsize = 0x79646475, 
    bk_nextsize = 0x20fe1
  }
  ```

  Now print the top chunk.
  ```
  (gdb) p *(mchunkptr)(c-16+32)
  $5 = {
    mchunk_prev_size = 2036622453, 
    mchunk_size = 135137, 
    fd = 0x0, 
    bk = 0x0, 
    fd_nextsize = 0x0, 
    bk_nextsize = 0x0
  }
  ```

  We have already cheked that 135137 is 0x20fe1. Now 
  check 0x79646475. Open a python shell.
  ```
  >>> 0x79646475
  2036622453
  ```

  Earlier, mchunk_prev_size of the top chunk was 0x0. 
  After populating the payload memory, the prev_size 
  field is used, hence the garbage value. Otherwise, 
  it would have remained zero. That's boundary tag 
  implementation.

  ---

  Now free this chunk and inspect again. The size of 
  this chunk has increased a lot. Now print the 
  address of the top chunk via the arena. Don't worry, 
  we are not required to understand arenas yet.
  ```
  (gdb) p main_arena->top
  ```
  - It is the same as `c`.

  Understanding the top chunk is essential to obtain 
  a free chunk. If we subtract the old top size with 
  the current size, we get __. That means, when the 
  chunk bordering the top chunk is freed, it is 
  coalesced into the the top chunk. Therefore, to 
  obtain a free chunk, a barrier must exist between 
  the chunk to be freed and the top chunk.

  In the next experiment, we will explore how to 
  obtain a free chunk.
*/
