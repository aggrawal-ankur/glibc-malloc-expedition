/* [EXPERIMENT 2] */

/* [OBJECTIVE]: Verify the state of smallbins.

  [1] The total number of smallbins are 62. 
      The bounds for bin number are [2, 63].
  [2] Bin #2 is the first smallbin of size 
      class MINSIZE bytes.
  [3] Bin #63 is the last smallbin of size 
      class (MIN_LARGE_SIZE-MINSIZE) bytes.
  [4] The bounds for small size are 
        [MINSIZE, MIN_LARGE_SIZE-MINSIZE].
  [5] Small chunks are binned directly upon 
      freeing.
*/

/* [SETUP] 

  [Step1]: Request two chunks of sizes 10 and 994 
           bytes and two barrier chunks.

  [Step2]: Free c1 and c2.

  [Step3]: Set a breakpoint on line #38.
*/

#include <stdlib.h>

int main(void){
  char* c1 = malloc(10);
  char* b1 = malloc(10);

  char* c2 = malloc(994);
  char* b2 = malloc(10);

  free(c1);
  free(c2);
  int breakp = 1;

  free(b1);
  free(b2);
}

/* [ANALYSIS] 

  c1 and c2 are the main chunks, while b1 and b2 
  are barrier chunks which prevent coalescing.

  After alignment, the size of c1 is MINSIZE and 
  the size of c2 is (MIN_LARGE_SIZE-MINSIZE) bytes.

  Run the program. Now inspect main_arena.

  Bin #2 is represented by bins[2] and bins[3].
  Bin #63 is represented by bins[124] and bins[125].

  The <main_arena> offsets associated to them are 
  <main_arena+24> and <main_arena+1000>. If these 
  bins are non-empty, these two offsets must be 
  invisible.

  Print main_arena.bins[1] and main_arena.bins[124].
  ```
  (gdb) p main_arena.bins[2]
  $1 = (mchunkptr) 0x55d26f758000

  (gdb) p main_arena.bins[3]
  $2 = (mchunkptr) 0x55d26f758000

  (gdb) p main_arena.bins[124]
  $3 = (mchunkptr) 0x55d26f758040

  (gdb) p main_arena.bins[125]
  $4 = (mchunkptr) 0x55d26f758040
  ```

  Print the chunks on these addresses.
  ```
  (gdb) p *(mchunkptr) (0x55d26f758000)
  $5 = {
    mchunk_prev_size = 0,
    mchunk_size = 33,
    fd = 0x7f011f257c98 <main_arena+24>,
    bk = 0x7f011f257c98 <main_arena+24>,
    fd_nextsize = 0x20,
    bk_nextsize = 0x20
  }

  (gdb) p *(mchunkptr) (0x55d26f758040)
  $6 = {
    mchunk_prev_size = 0,
    mchunk_size = 1009,
    fd = 0x7f011f258068 <main_arena+1000>,
    bk = 0x7f011f258068 <main_arena+1000>,
    fd_nextsize = 0x0,
    bk_nextsize = 0x0
  }
  ```

  In the upcoming experiments, we will explore that bin #64 is the first largebin, which will further prove this point.
*/
