/* [EXPERIMENT #3] */

/* [OBJECTIVE]: Bin #64 is the first largebin in 
                category #1, represented by the 
                headers bins[126] and bins[127].
*/

/* [SETUP] 

  [Step1]: Request a chunk of MIN_LARGE_SIZE bytes 
           and a barrier chunk.

  [Step2]: Free the large chunk and request another 
           large chunk of size greater than 
           MIN_LARGE_SIZE so that `c` is binned.

  [Step3]: Set a breakpoint on line #31.
*/

#include <stdlib.h>

int main(void){
  char* c = malloc(1008);
  char* b = malloc(10);

  free(c);

  /* Request a chunk of size greater than `c` to 
     initiate binning. */
  char* ib = malloc(1100);
  int breakp = 1;

  free(b);
  free(ib);
}

/* [ANALYSIS] 

  Inspect the bins.
  ```
  (gdb) p main_arena.bins[126]
  $1 = (mchunkptr) 0x563604ad9000

  (gdb) p main_arena.bins[127]
  $2 = (mchunkptr) 0x563604ad9000
  ```

  Inspect the chunk at the address.
  ```
  (gdb) p *(mchunkptr) (0x563604ad9000)
  $3 = {
    mchunk_prev_size = 0,
    mchunk_size = 1025,
    fd = 0x7fb9068bd078 <main_arena+1016>,
    bk = 0x7fb9068bd078 <main_arena+1016>,
    fd_nextsize = 0x563604ad9000,
    bk_nextsize = 0x563604ad9000
  }
  ```
*/
