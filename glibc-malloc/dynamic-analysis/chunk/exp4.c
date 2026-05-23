/* EXPERIMENT 4 */

/* OBJECTIVE: Obtain and analyze a free chunk. */

/* SETUP: 

  1. Start and attach the container to your terminal.
  2. `cd` to /experiments/
  3. Build the lab: `./build exp3.c`
  4. Set breakpoints on line #48, and #51.
  6. Run the program: `r`.

*/

/* ANALYSIS: We will allcoate a 20 bytes chunk, followed by a 100 bytes chunk as a barrier with the top chunk.

Print the chunk.
```
(gdb) p *(struct malloc_chunk*)(c-16)
$1 = {mchunk_prev_size = 0, mchunk_size = 33, fd = 0x0, bk = 0x0, fd_nextsize = 0x0, bk_nextsize = 0x20fe1}
```

Now let's allocate the barrier chunk and free the first chunk. Press `c` to continue until the next breakpoint.

Now print the chunk again.
```
$2 = {mchunk_prev_size = 0, mchunk_size = 33, fd = 0x7f6919ca1c98 <main_arena+24>, bk = 0x7f6919ca1c98 <main_arena+24>, fd_nextsize = 0x20, 
  bk_nextsize = 0x70}
```

That is our free chunk.

*/

/* OBSERVATIONS: 

  1. Since the chunk is freed now, the fd/bk pointers contain bookkeping information.
  2. The nextsize pointers continue to remain garbage as it is a small chunk.
  3. Because it is the first chunk, mchunk_prev_size is still 0 and PREV_INUSE bit is 1.
  4. A barrier chunk is necessary to obtain free a chunk if the chunk borders with the top chunk. Otherwise, there are already enough chunks that act as a barrier.

*/

#include <stdlib.h>

int main(void){
  char *c = malloc(20);
  char *b = malloc(100);

  free(c);
  int breakpoint = 1;

  free(b);
}
