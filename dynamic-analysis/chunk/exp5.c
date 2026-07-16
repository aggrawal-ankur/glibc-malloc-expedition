/* EXPERIMENT 5 */

/* OBJECTIVE: Analyze the state of PREV_INUSE bit in a chunk that is preceded by both free and in-use chunks. */

/* GIVEN: 

  1. We need two chunks.
  2. The first chunk would be freed and the second chunk is the one whose PREV_INUSE Bit would be analyzed.

*/

/* SETUP: 

  1. Start and attach the container to your terminal.
  2. `cd` to /experiments/
  3. Build the lab: `./build exp3.c`
  4. Set breakpoints on line #76, and #79.
  6. Run the program: `r`.

*/

/* ANALYSIS: We will allcoate a 20 bytes chunk, followed by a 36 bytes chunk as a barrier with the top chunk.

Print both the chunks to verify their initial state.
```
(gdb) p c1
$1 = 0x55cac7312010 "I love systems.\n"

(gdb) p *(struct malloc_chunk*)(c1-16)
$2 = {mchunk_prev_size = 0, mchunk_size = 33, fd = 0x6f6c2049202c6948, bk = 0x6574737973206576, fd_nextsize = 0xa2e736d, 
  bk_nextsize = 0x31}

(gdb) p *(struct malloc_chunk*)(c2-16)
$3 = {mchunk_prev_size = 170816365, mchunk_size = 49, fd = 0x0, bk = 0x0, fd_nextsize = 0x0, bk_nextsize = 0x0}
```

Now free c1. Continue with `c`.

Now print both the chunks again.
```
(gdb) p *(struct malloc_chunk*)(c1-16)
$4 = {mchunk_prev_size = 0, mchunk_size = 33, fd = 0x7f7dc7899c98 <main_arena+24>, bk = 0x7f7dc7899c98 <main_arena+24>, fd_nextsize = 0x20, 
  bk_nextsize = 0x30}

(gdb) p *(struct malloc_chunk*)(c2-16)
$5 = {mchunk_prev_size = 32, mchunk_size = 48, fd = 0x0, bk = 0x0, fd_nextsize = 0x0, bk_nextsize = 0x0}
```

---

There is a reason behind choosing this specific setting of chunks.
  - The first chunk is a MINSIZE chunk (32 bytes). The nextsize fields can't be covered by it, so they overlap with the next chunk's prev_size and mchunk_size.
  - Based on the boundary tag implementation, prev_size of (n+1)th chunk is functionally a property of the nth chunk. That means, those 8 bytes are shared.
  - When we populate the memory with 20 bytes, the initial 16 bytes are covered by fd/bk and the remaining 4 bytes are in the prev_size territory. This is visible.
  - c1 has these values: {fd_nextsize = 0xa2e736d, bk_nextsize = 0x31}. c2 has these values: {mchunk_prev_size = 170816365, mchunk_size = 49}. These values are exactly the same. You can verify yourself.
  - prev_size of (n+1)the chunk is used as payload memory as long as the nth chunk is in-use. When c1 is freed, prev_size becomes a part of c1's metadata in footer.
  - To prove this distinction with least amount of friction possible, I chose this setting. We can definitely use memset with a decently sized chunk and still replicate exactly, after all, that's a property applicable in all the cases. It is just that MINSIZE makes the overlap more explicit.

*/

/* OBSERVATION: 

  1. prev_size of (n+1)th chunk represents the payload memory of the nth chunk as long as it is an in-use chunk. Once the nth chunk is freed, it becomes the "metadata in footer".
  2. The PREV_INUSE bit is 0 when the preceding chunk is free and 1 when it is in-use.

*/

#include <stdlib.h>
#include <string.h>

int main(void){
  char *c1 = 20;
  memcpy(c1, "I love systems.\n", 16);

  char *c2 = 36;
  int breakpoint = 1;

  free(c1);
  breakpoint = 2;

  free(c2);
}
