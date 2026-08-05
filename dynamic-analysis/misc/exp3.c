/* [EXPERIMENT #3] */

/* [OBJECTIVE]: Verify which chunk is taken out from a 
                largebin when multiple chunks of that 
                size are available.
*/

/* [SETUP] 

  [1] Allocate 5 chunks of size 1010 bytes and 5 barrier 
      chunks of size 2000 bytes.
  [2] Free the main chunks.
  [3] Allocate another chunk to initiate binning.
  [4] Set a breakpoint on line #.
  [5] Allocate a chunk of size 1010 bytes.
  [6] Set a breakpoint on line #.
*/

#include <stdlib.h>

int main(void){
  char* c1 = malloc(1010);
  char* b1 = malloc(2000);

  char* c2 = malloc(1010);
  char* b2 = malloc(2000);

  char* c3 = malloc(1010);
  char* b3 = malloc(2000);

  char* c4 = malloc(1010);
  char* b4 = malloc(2000);

  char* c5 = malloc(1010);
  char* b5 = malloc(2000);

  free(c1);
  free(c2);
  free(c3);
  free(c4);
  free(c5);

  char* ib = malloc(5000);
  int bp = 1;

  char* c6 = malloc(1010);
  bp = 2;

  free(b1);
  free(b2);
  free(b3);
  free(b4);
  free(b5);
  free(c6);
}

/* [ANALYSIS] 

  Print the addresses of all the chunks at breakpoint-1.
  ```
  (gdb) p (c1-16)
  $1 = 0x55f7b1b67000 ""

  (gdb) p (c2-16)
  $2 = 0x55f7b1b67be0 ""

  (gdb) p (c3-16)
  $3 = 0x55f7b1b687c0 ""

  (gdb) p (c4-16)
  $4 = 0x55f7b1b693a0 ""

  (gdb) p (c5-16)
  $5 = 0x55f7b1b69f80 ""
  ```

  Based on experiment-6, we can say that c1 will 
  be the unique chunk in this largebin and the 
  order of chunks would be: 
    [c1 <-> c5 <-> c4 <-> c3 <-> c2].

  Let's verify it before anything else.
  ```
  (gdb) p main_arena.bins[126]
  $6 = (mchunkptr) 0x55f7b1b67000

  (gdb) p main_arena.bins[127]
  $7 = (mchunkptr) 0x55f7b1b67be0


  (gdb) p *(mchunkptr) (0x55f7b1b67000)
  $8 = {
    mchunk_prev_size = 0,
    mchunk_size = 1025,
    fd = 0x55f7b1b69f80,
    bk = 0x7f16e7226078 <main_arena+1016>,
    fd_nextsize = 0x55f7b1b67000,
    bk_nextsize = 0x55f7b1b67000
  }

  (gdb) p *(mchunkptr) (0x55f7b1b69f80)
  $9 = {
    mchunk_prev_size = 0,
    mchunk_size = 1025,
    fd = 0x55f7b1b693a0,
    bk = 0x55f7b1b67000,
    fd_nextsize = 0x0,
    bk_nextsize = 0x0
  }

  (gdb) p *(mchunkptr) (0x55f7b1b693a0)
  $10 = {
    mchunk_prev_size = 0,
    mchunk_size = 1025,
    fd = 0x55f7b1b687c0,
    bk = 0x55f7b1b69f80,
    fd_nextsize = 0x0,
    bk_nextsize = 0x0
  }

  (gdb) p *(mchunkptr) (0x55f7b1b687c0)
  $11 = {
    mchunk_prev_size = 0,
    mchunk_size = 1025,
    fd = 0x55f7b1b67be0,
    bk = 0x55f7b1b693a0,
    fd_nextsize = 0x0,
    bk_nextsize = 0x0
  }

  (gdb) p *(mchunkptr) (0x55f7b1b67be0)
  $12 = {
    mchunk_prev_size = 0,
    mchunk_size = 1025,
    fd = 0x7f16e7226078 <main_arena+1016>,
    bk = 0x55f7b1b687c0,
    fd_nextsize = 0x0,
    bk_nextsize = 0x0
  }
  ```
  - [c1 <-> c5 <-> c4 <-> c3 <-> c2]
  - Nothing unexpected.

  Continue the program and allocate c6. Now inspect 
  the largebin again.
  ```
  (gdb) p *(mchunkptr) (0x55f7b1b67000)
  $14 = {
    mchunk_prev_size = 0,
    mchunk_size = 1025,
    fd = 0x55f7b1b693a0,
    bk = 0x7f16e7226078 <main_arena+1016>,
    fd_nextsize = 0x55f7b1b67000,
    bk_nextsize = 0x55f7b1b67000
  }

  (gdb) p *(mchunkptr) (0x55f7b1b693a0)
  $15 = {
    mchunk_prev_size = 0,
    mchunk_size = 1025,
    fd = 0x55f7b1b687c0,
    bk = 0x55f7b1b67000,
    fd_nextsize = 0x0,
    bk_nextsize = 0x0
  }

  (gdb) p *(mchunkptr) (0x55f7b1b687c0)
  $16 = {
    mchunk_prev_size = 0,
    mchunk_size = 1025,
    fd = 0x55f7b1b67be0,
    bk = 0x55f7b1b693a0,
    fd_nextsize = 0x0,
    bk_nextsize = 0x0
  }

  (gdb) p *(mchunkptr) (0x55f7b1b67be0)
  $17 = {
    mchunk_prev_size = 0,
    mchunk_size = 1025,
    fd = 0x7f16e7226078 <main_arena+1016>,
    bk = 0x55f7b1b687c0,
    fd_nextsize = 0x0,
    bk_nextsize = 0x0
  }
  ```
  - [c1 <-> c4 <-> c3 <-> c2]

  The observed behavior is 180 degrees opposite 
  to the annotation. Instead of picking the LRU 
  chunk (first duplicate), the allocator is 
  choosing the most recently used chunk.
*/
