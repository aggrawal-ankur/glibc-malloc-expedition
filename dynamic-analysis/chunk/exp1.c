/* [EXPERIMENT #1] */

/* [OBJECTIVE]: The size of the smallest chunk is MINSIZE bytes. */

/* [GIVEN] 

  [1] Use the print command (short, `p`) to print 
      the value at an address.

  [2] Use pointer type casting along with `p`.

  [3] size_t is 8 bytes on 64-bit.

  [Syntax]: `p *(struct malloc_chunk*) (addr)`

*/

/* [SETUP] 

  Size is an unsigned quantity and the smallest value 
  possible is 0. We will request a chunk of 0 byte and 
  inspect what we have got.

  [0] Start docker.

  [1] Start and attach the container to your terminal.

  [2] `cd` to /experiments/

  [3] Build the lab: `./build exp1.c`

  [4] Create a breakpoint on line #47 with `b exp1.c:<n>` 
      or `b #line`.

  [6] Run the program with `r`. The execution must be 
      halted and this should be visible:
      ```
      Breakpoint 1, main () at exp1.c:47
      47        int breakpoint = 4;
      ```
*/

#include <stdlib.h>

int main(void){
  char *c = malloc(0);
  int breakpoint = 1;
  free(c);
}

/* [ANALYSIS] 

  malloc() returns a pointer to the first byte in 
  the payload memory. The variable `c` contains 
  that pointer. `p c` will print its address.

  Typecast it to (struct malloc_chunk*) to tell `print` 
  to print sizeof(struct malloc_chunk) bytes of memory 
  starting from `c` as if they represent a malloc_chunk.
  ```
  (gdb) print *(struct malloc_chunk*)(c)
  $1 = {mchunk_prev_size = 0, mchunk_size = 0, fd = 0x0, bk = 0x20fe1, fd_nextsize = 0x0, bk_nextsize = 0x0}
  ```

  We can set pretty printing with ....
  ```
  (gdb) set print pretty on
  ```

  .... and the output will become
  ```
  $2 = {
    mchunk_prev_size = 0, 
    mchunk_size = 0, 
    fd = 0x0, 
    bk = 0x20fe1, 
    fd_nextsize = 0x0, 
    bk_nextsize = 0x0
  }
  ```

  We can also use mchunkptr instead of (struct malloc_chunk*).

  Anyways, the output on your end might be similar to 
  this, but it is incorrect because, `c` points to the 
  payload memory, not the malloc_chunk header, for 
  obvious reasons. So we have to subtract (2*size_t) 
  bytes before casting the pointer to (mchunkptr). On 
  64-bit, we can subtract (2*8).
  ```
  (gdb) print *(mchunkptr*)(c - (2*sizeof(size_t)))
  $3 = {
    mchunk_prev_size = 0, 
    mchunk_size = 33, 
    fd = 0x0, 
    bk = 0x0, 
    fd_nextsize = 0x0, 
    bk_nextsize = 0x20fe1
  }
  ```

  Now we have a valid size and that size is 32 bytes, 
  not 1 byte. The '1' in '33' is for the PREV_INUSE bit.

  ---

  This proves that the smallest possible chunk in 
  glibc malloc is for MINSIZE bytes and malloc(0) 
  is also aligned up to MINSIZE bytes.
*/
