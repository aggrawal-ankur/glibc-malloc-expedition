/* [EXPERIMENT #1] */

/* [OBJECTIVE]: 

  Explore what happens when the unsorted bin has 
  a large chunk that can satisfy the request under 
  best-fit but it is neither the last_remainder, 
  nor an exact fit.
*/

/* [SETUP]: 

  [Step1]: Request 1080 bytes and we will get a 1088 
           bytes chunk.

  [Step2]: Request a barrier chunk so that when the 
           first chunk is freed, it doesn't get 
           coalesced into the top chunk.

  [Step3]: Request a chunk of 1010 bytes after c1 is 
           freed.

  [Step4]: Set breakpoints on lines #33 and #36.
*/

#include <stdlib.h>

int main(void){
  char* c1 = malloc(1080);
  char* b  = malloc(10);

  free(c1);
  int breakp = 1;

  char* c2 = malloc(1010);
  breakp = 2;
}

/* [ANALYSIS]: 

  On the first breakpoint, inspect the address in the 
  unsorted bin.
  ```
  (gdb) p *(mchunkptr) (addr)

  $2 = {
    mchunk_prev_size = 0,
    mchunk_size = 1089,
    fd = 0x7f6640d95c88 <main_arena+8>,
    bk = 0x7f6640d95c88 <main_arena+8>,
    fd_nextsize = 0x0,
    bk_nextsize = 0x0
  }
  ```

  Move to the next line with (n) and step into 
  __libc_malloc with (s). Then use (n) normally. 
  Use the step command again on line #3271 

    (victim = tag_new_usable (_int_malloc (&main_arena, bytes));) 

  .... to step into _int_malloc. The goal is to 
  see the execution path taken to satisfy this 
  request.

  [1] av != NULL, so path-1 is not taken.
  [2] nb is large, so path-2 is not taken.
  [3] In the unsorted bin path, path-3a is not 
      taken because the size is large, and 
      size != nb, so path-3b is no taken either.

  The unsorted chunk is binned and rediscovered 
  later, used to service the request and the 
  remainder goes in the unsorted bin again.

  ---

  The size was large in this case. Change the size 
  of c2 to a small size and repeat the process. 
  Regardless of the size, if the unsorted bin is 
  not singleton and that chunk is not the 
  last_remainder and the victim's size is not an 
  exact fit, this path will not be taken.
*/
