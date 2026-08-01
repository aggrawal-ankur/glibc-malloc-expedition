/* [EXPERIMENT #4] */

/* [OBJECTIVE]: Obtain and analyze a free chunk. */

/* [SETUP] 

  [1] Allocate two chunks of size MINSIZE bytes. The 
      first one will be the main chunk and the second 
      one will exist as a barrier between the first 
      chunk and the top chunk. Set a breakpoint on 
      line #23.
  [2] Free the first chunk and set a breakpoint on 
      line #26.
*/

#include <stdlib.h>
#include <string.h>

int main(void){
  char *c = malloc(20);
  memcpy(c, "Low level systems.\n", 19);
  char *b = malloc(20);
  int breakpoint = 1;

  free(c);
  breakpoint = 2;

  free(b);
}

/* [ANALYSIS] 

  Print the initial state of chunks at breakpoint-1.
  ```
  (gdb) p *(mchunkptr) (c-16)
  $1 = {
    mchunk_prev_size = 0,
    mchunk_size = 33,
    fd = 0x6576656c20776f4c,
    bk = 0x6d6574737973206c,
    fd_nextsize = 0xa2e73,
    bk_nextsize = 0x21
  }
  (gdb) p *(mchunkptr) (b-16)
  $2 = {
    mchunk_prev_size = 667251,
    mchunk_size = 33,
    fd = 0x0,
    bk = 0x0,
    fd_nextsize = 0x0,
    bk_nextsize = 0x20fc1
  }
  ```
  - The pointer fields of `c` and the prev_size of 
    `b` are interpreting the contents of the payload 
    memory, so they are garbage.
  - The nextsize pointers of the barrier chunk are 
    overlapped with the size fields in the top chunk.


  Continue and print the state of both chunks after 
  freeing them at breakpoint-2.
  ```
  (gdb) p *(mchunkptr) (c-16)
  $1 = {
    mchunk_prev_size = 0,
    mchunk_size = 33,
    fd = 0x7fb390734c98 <main_arena+24>,
    bk = 0x7fb390734c98 <main_arena+24>,
    fd_nextsize = 0x20,
    bk_nextsize = 0x20
  }

  (gdb) p *(mchunkptr) (b-16)
  $2 = {
    mchunk_prev_size = 32,
    mchunk_size = 32,
    fd = 0x0,
    bk = 0x0,
    fd_nextsize = 0x0,
    bk_nextsize = 0x20fc1
  }
  ```

  That's our free chunk.
  [1] It has real addresses in the fd/bk fields, but we 
      are not primed yet to understand these addresses. 
      So they are explored in a future lab.
  [2] The chunk is small, so the nextsize pointers are 
      garbage.

  The PERV_INUSE bit of the barrier chunk is cleared as 
  the chunk previous to it is freed and mchunk_prev_size 
  is no longer garbage.

  ---

  Based on the boundary tag implementation, prev_size of 
  (n+1)th chunk is functionally a property of the nth 
  chunk. So, 16 of the 20 bytes structurally belong to 
  `c`, while the remaining 4 bytes belong to `b`. This 
  is visible when the payload memory is populated with 
  memset.

  Once `c` is freed, the prev_inuse field is updated with 
  the size of `c`, which was garbage earlier.
*/
