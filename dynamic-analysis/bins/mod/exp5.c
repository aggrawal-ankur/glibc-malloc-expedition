/* [EXPERIMENT 5] */

/* [OBJECTIVE]: Small free chunks use only the fd/bk pointers. */

/* [SETUP] 

  [Step1]: Request two chunks of 60 bytes and two 
           barrier chunks.

  [Step2]: Set a breakpoint on line #34 and inspect 
           the initial chunk state.

  [Step3]: Write some stuff on these memory locations.

  [Step4]: Set a breakpoint on line #38 and inspect 
           the chunk state.

  [Step5]: Free the chunks.

  [Step6]: Set a breakpoint on line #42 and inspect 
           the chunk state.
*/

#include <stdlib.h>
#include <string.h>

int main(void){
  char *c1 = malloc(60);
  char *b1 = malloc(20);

  char *c2 = malloc(60);
  char *b2 = malloc(20);

  int breakp = 1;

  memcpy(c1, "My name is Anna. I love low level systems.\n", 43);
  memcpy(c2, "My name is John, and I too love low level systems.\n", 51);
  breakp = 2;

  free(c1);
  free(c2);
  breakp = 3;

  free(b1);
  free(b2);
}

/* [ANALYSIS] 

  Print the initial chunk state:
  ```
  (gdb) p *(mchunkptr)(c1-16)
  $1 = {
    mchunk_prev_size = 0, 
    mchunk_size = 81, 
    fd = 0x0, 
    bk = 0x0, 
    fd_nextsize = 0x0, 
    bk_nextsize = 0x0
  }

  (gdb) p *(mchunkptr)(c2-16)
  $2 = {
    mchunk_prev_size = 0, 
    mchunk_size = 81, 
    fd = 0x0, 
    bk = 0x0, 
    fd_nextsize = 0x0, 
    bk_nextsize = 0x0
  }
  ```
  - All the pointers are NULL as the chunks are in-use.

  Continue. Now print the chunks again.
  ```
  (gdb) p *(mchunkptr)(c1-16)
  $3 = {
    mchunk_prev_size = 0, 
    mchunk_size = 81, 
    fd = 0x20656d616e20794d, 
    bk = 0x2e616e6e41207369, 
    fd_nextsize = 0x2065766f6c204920, 
    bk_nextsize = 0x6576656c20776f6c
  }

  (gdb) p *(mchunkptr)(c2-16)
  $4 = {
    mchunk_prev_size = 0, 
    mchunk_size = 81, 
    fd = 0x20656d616e20794d, 
    bk = 0x2c6e686f4a207369, 
    fd_nextsize = 0x74204920646e6120, 
    bk_nextsize = 0x2065766f6c206f6f
  }
  ```

  We know that an in-use chunk has only the first 
  two fields in malloc_chunk valid. The rest of 
  the fields are used as payload memory. So, the 
  values in the four fields are the contents of 
  the payload memory "interpreted as addresses".


  Free c1 and c2 and print them again.
  ```
  (gdb) p *(mchunkptr)(c1-16)
  $5 = {
    mchunk_prev_size = 0, 
    mchunk_size = 81, 
    fd = 0x7f71fb0bccc8 <main_arena+72>, 
    bk = 0x55dd637fa070, 
    fd_nextsize = 0x2065766f6c204920, 
    bk_nextsize = 0x6576656c20776f6c
  }

  (gdb) p *(mchunkptr)(c2-16)
  $6 = {
    mchunk_prev_size = 0, 
    mchunk_size = 81, 
    fd = 0x55dd637fa000, 
    bk = 0x7f71fb0bccc8 <main_arena+72>, 
    fd_nextsize = 0x74204920646e6120, 
    bk_nextsize = 0x2065766f6c206f6f
  }
  ```

  Now the fd/bk fields are updated with 
  real addresses and we can verify that 
  by printing the address of these chunks.
  ```
  (gdb) print (c1-16)
  $7 = 0x55dd637fa000 ""

  (gdb) print (c2-16)
  $8 = 0x55dd637fa070 ""
  ```

  The nextsize pointers continue to have 
  the same garbage value.

  This proves that small chunks use only 
  the fd/bk fields.

*/
