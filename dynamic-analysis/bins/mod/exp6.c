/* [EXPERIMENT #] */

/* [OBJECTIVE]: A largebin is basically a collection 
                of several size classes.
*/

/* [GIVEN] 

  [1] We will use the first largebin in category #1.

  [2] This bin spans across 64 bytes from the base 
      size of 1009 bytes (until 1072), managing 
      chunks in [1009, 1072] bytes range.

  [3] The number of fixed size classes managed by 
      this largebin are 4 
        (BIN_WIDTH/SMALLBIN_WIDTH, i.e. 64/16). 

      They are
        [1] (1008+(16*1)) => 1024,
        [2] (1008+(16*2)) => 1040,
        [3] (1008+(16*3)) => 1056, and
        [4] (1008+(16*4)) => 1072.

  [4] Remember, these are request2size(sz) values.
*/

/* [SETUP] 

  We need 4 normal chunks, 4 barrier chunks and 
  one chunk to initiate binning after freeing 
  the chunks.

  The size of the barrier chunks and the chunk 
  to initiate binning must be higher than the 
  freed chunks, so that they can not be reused. 
  Let's take 2000 bytes as it is an outcast here.

  We will use the same formula used in exp2.c.

  Set a breakpoint on line #66.
*/

#include <stdlib.h>

int main(void){
  size_t BASE = 1010;

  char *c1 = malloc(BASE);
  char *b1 = malloc(2000);

  char *c2 = malloc(BASE+16);
  char *b2 = malloc(2000);

  char *c3 = malloc(BASE+32);
  char *b3 = malloc(2000);

  char *c4 = malloc(BASE+48);
  char *b4 = malloc(2000);

  free(c1);
  free(c2);
  free(c3);
  free(c4);
  char *ib = malloc(2000);
  int breakp = 1;

  free(b1);
  free(b2);
  free(b3);
  free(b4);
  free(ib);
}

/* [ANALYSIS] 

  The first largebin is represented by the headers 
  bins[126] and bins[127]. Inspect these headers.
  ```
  (gdb) p main_arena.bins[126]
  $1 = (mchunkptr) 0x55c7784b6b70

  (gdb) p main_arena.bins[127]
  $2 = (mchunkptr) 0x55c7784b3000
  ```

  The addresses are different, indicating that the 
  bin has more than one chunk. Take the address in 
  head and inspect the chunk at it.
  ```
  (gdb) p *(mchunkptr) (main_arena.bins[126])
  $3 = {
    mchunk_prev_size = 0,
    mchunk_size = 1073,
    fd = 0x55c7784b5790,
    bk = 0x7f8ccd210078 <main_arena+1016>,
    fd_nextsize = 0x55c7784b5790,
    bk_nextsize = 0x55c7784b3000
  }
  ```

  Now use the address in fd to reach the next chunk. 
  Repeat this until we reach the bin header.
  ```
  (gdb) p *(mchunkptr) (0x55c7784b5790)
  $4 = {
    mchunk_prev_size = 0,
    mchunk_size = 1057,
    fd = 0x55c7784b43c0,
    bk = 0x55c7784b6b70,
    fd_nextsize = 0x55c7784b43c0,
    bk_nextsize = 0x55c7784b6b70
  }

  (gdb) p *(mchunkptr) (0x55c7784b43c0)
  $5 = {
    mchunk_prev_size = 0,
    mchunk_size = 1041,
    fd = 0x55c7784b3000,
    bk = 0x55c7784b5790,
    fd_nextsize = 0x55c7784b3000,
    bk_nextsize = 0x55c7784b5790
  }

  (gdb) p *(mchunkptr) (0x55c7784b3000)
  $6 = {
    mchunk_prev_size = 0,
    mchunk_size = 1025,
    fd = 0x7f8ccd210078 <main_arena+1016>,
    bk = 0x55c7784b43c0,
    fd_nextsize = 0x55c7784b6b70,
    bk_nextsize = 0x55c7784b43c0
  }
  ```

  This proves that a largebin is essentially 
  a collection of several size classes.
*/
