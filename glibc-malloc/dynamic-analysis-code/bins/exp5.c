/* EXPERIMENT 5 */

/* OBJECTIVE: Small chunks use only the fd/bk pointers. */

/* ADD BREAKPOINT NOTES. */

/* ANALYSIS: 

Initial print:
```
(gdb) p *(mchunkptr)(c1-16)
$1 = {mchunk_prev_size = 0, mchunk_size = 81, fd = 0x0, bk = 0x0, fd_nextsize = 0x0, bk_nextsize = 0x0}

(gdb) p *(mchunkptr)(c2-16)
$2 = {mchunk_prev_size = 0, mchunk_size = 81, fd = 0x0, bk = 0x0, fd_nextsize = 0x0, bk_nextsize = 0x0}
```

Notice all the pointers are NULL right now. Now execute memcpy: `c` and print the chunks again.
```
(gdb) p *(mchunkptr)(c1-16)
$3 = {mchunk_prev_size = 0, mchunk_size = 81, fd = 0x20656d616e20794d, bk = 0x2e616e6e41207369, fd_nextsize = 0x2065766f6c204920, 
  bk_nextsize = 0x6576656c20776f6c}

(gdb) p *(mchunkptr)(c2-16)
$4 = {mchunk_prev_size = 0, mchunk_size = 81, fd = 0x20656d616e20794d, bk = 0x2c6e686f4a207369, fd_nextsize = 0x74204920646e6120, 
  bk_nextsize = 0x2065766f6c206f6f}
```

We already know the struct of malloc_chunk and how the memory looks like from chunk description experiments. After the initial 32 bytes (0-31), the next 16 bytes of the payload memory overlap with the nextsize pointer fields. So, what we are seeing here in the four fields in actually garbage, or the contents of the payload memory "interpreted as addresses".

Now free the pointers (`c`) and print them again.
```
$5 = {mchunk_prev_size = 0, mchunk_size = 81, fd = 0x7f71fb0bccc8 <main_arena+72>, bk = 0x55dd637fa070, fd_nextsize = 0x2065766f6c204920, 
  bk_nextsize = 0x6576656c20776f6c}

(gdb) p *(mchunkptr)(c2-16)
$6 = {mchunk_prev_size = 0, mchunk_size = 81, fd = 0x55dd637fa000, bk = 0x7f71fb0bccc8 <main_arena+72>, fd_nextsize = 0x74204920646e6120, 
  bk_nextsize = 0x2065766f6c206f6f}
```

Notice that the fd/bk fields are now containing something else. These are valid pointers and we can verify that by printing the address of these chunks.
```
(gdb) print  (c1-16)
$7 = 0x55dd637fa000 ""

(gdb) print  (c2-16)
$8 = 0x55dd637fa070 ""
```

0x7f71fb0bccc8 <main_arena+72> is the bin header for this bin.

The nextsize pointers continue to have the same garbage value. This proves that small chunk use only the fd/bk fields.

*/

/* OBSERVATIONS: 

  1. Small chunks use only the fd/bk pointers.
  2. Large chunks use all the four pointers.

*/

#include <stdlib.h>
#include <string.h>

int main(void){
  char *c1 = malloc(60);
  char *b1 = malloc(20);
  char *c2 = malloc(60);
  char *b2 = malloc(20);
  int x = 1;

  memcpy(c1, "My name is Anna. I love low level systems.\n", 43);
  memcpy(c2, "My name is John, and I too love low level systems.\n", 51);
  x = 2;

  free(c1);
  free(c2);
  x = 3;

  free(b1);
  free(b2);
}
