/* EXPERIMENT 2 */

/* OBJECTIVE: Perform structural analysis of a chunk. */

/* GIVEN: 

  1. Use the print command (short, `p`) to print the value at an address.
  3. Use pointer type casting along with `p`.
  4. size_t is 8 bytes on 64-bit.

  Syntax: `p *(struct malloc_chunk*)__address__`

*/

/* SETUP: We will allocate a chunk of size 20 bytes and analyze it.

  1. Start and attach the container to your terminal, if not already (reading on the web or locally).
  2. `cd` to /experiments/
  3. Build the lab: `./build exp2.c`
  4. To create a breakpoint on line #<n>, use the following syntax: `b exp2.c:<n>`
  5. Create breakpoints on line #84.
  6. Run the program: `r`. The execution must be halted and this should be visible:
     ```
     Breakpoint 1, main () at exp2.c:84
     84          int break_point = 4;
     ```
*/

/* ANALYSIS: 

Let's print the chunk.
```
(gdb) print c
$1 = 0x563dcd5e2010 ""
```

As with the previous experiment, we have to subtract 2*sizeof(size_t) bytes, or 16 bytes on 64-bit to get to the start of the malloc_chunk.
```
(gdb) print *(struct malloc_chunk*)(c - 16)
$2 = {mchunk_prev_size = 0, mchunk_size = 33, fd = 0x0, bk = 0x0, fd_nextsize = 0x0, bk_nextsize = 0x20fe1}
```

**NOTE: We are not typecasting the pointer to a (char*) before casting it to a (malloc_chunk*) as it is already so. However, we have to do this when the pointer type is something else.

request2size(20) is 32 bytes, which is the size of this chunk. The 1 is for the PREV_INUSE bit, which is always set for the first chunk.

We can notice that the members fd/bk/fd_nextsize are zero. However, bk_nextsize is non-zero. Understanding these pointers is out of scope of this experiment, and it is dicussed in another experiment.

---

Now press `n` to move to the next line in the source.
```
(gdb) n
86        free(c);
```
This frees the chunk `c`, making it a "free chunk" now. Let's execute this line.
```
(gdb) n
87        break_point = 2;
```

Let's print the chunk again.
```
(gdb) p *(struct malloc_chunk*)(c-16)
$3 = {mchunk_prev_size = 0, mchunk_size = 135169, fd = 0x0, bk = 0x0, fd_nextsize = 0x0, bk_nextsize = 0x20fe1}
```

Magic! The size of the chunk has increased to 135169. That's 33 * pow(2, 12). How? That is discussed in the next experiment.

*/

/* OBSERVATIONS: 

  1. The size of the chunk is request2size(20) bytes, not 20 bytes.
  2. The PREV_INUSE bit is always set in the first chunk to prevent malformed access.
  3. mchunk_prev_size is 0 as there is no chunk before the first chunk.

*/

#include <stdlib.h>

int main(void){
  char *c = malloc(20);
  int break_point = 1;

  free(c);
  break_point = 2;
}
