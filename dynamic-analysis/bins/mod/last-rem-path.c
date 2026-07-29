/* [EXPERIMENT ] */

/* [OBJECTIVE]: 

  Explore what happens when the unsorted bin has a large 
  chunk that can satisfy the request under best-fit rule 
  but it is neither the last_remainder, nor an exact fit.
*/

/* [SETUP]: 

  [Step1]: Request 1080 bytes and we will get a 1088 bytes chunk.

  [Step2]: Request a barrier chunk so that when the first chunk 
           is freed, it doesn't get coalesced into the top chunk.

  [Step3]: Request a chunk of 1010 bytes after freeing c1.
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

Set breakpoints on lines #27 and #30 and run (r).

On the first breakpoint, inspect the address in the unsorted bin.
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
  - Use `set print pretty on` for formatted output.


Move to the next line with (n) and step into __libc_malloc 
with (s). Then use the (n) normally. Use the step command 
again on line #3271 
  (victim = tag_new_usable (_int_malloc (&main_arena, bytes));) 

.... to step into _int_malloc. The goal is to see which 
execution path is taken to satisfy the request.


Since av != NULL, path-1 is not taken. Since nb is large, 
path-2 is not taken.

Next is the unsorted bin path.
  - Since the size is large, this path won't be taken.
  - Since size != nb, this path won't be taken.

The unsorted chunk is binned and rediscovered later, 
used to service the request and the remainder goes 
in the unsorted bin again.

*/