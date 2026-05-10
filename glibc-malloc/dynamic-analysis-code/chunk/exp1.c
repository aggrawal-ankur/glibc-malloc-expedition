/* EXPERIMENT 1 */

/* OBJECTIVE: The size of the smallest chunk is MINSIZE bytes. */

/* GIVEN:

  1. Use the print command (short, `p`) to print the value at an address.
  2. Use pointer type casting along with `p`.
  3. size_t is 8 bytes on 64-bit.

  Syntax: `p *(struct malloc_chunk*)__address__`

*/

/* SETUP: We will request a chunk of size 1 byte as that's the smallest we can request and check what we have received.

  1. Start and attach the container to your terminal, if not already (reading on the web or locally).
  2. `cd` to /experiments/
  3. Build the lab: `./build exp1.c`
  4. To create a breakpoint on line #<n>, use the following syntax: `b exp1.c:<n>`
  5. Create a breakpoint on line #72.
  6. Run the program: `r`. The execution must be halted and this should be visible:
     ```
     Breakpoint 1, main () at exp1.c:72
     72        int breakpoint = 4;
     ```
*/

/* ANALYSIS: 

malloc() returns a pointer to the first byte in the payload memory. The variable `c1` contains that pointer. We can print it using `p`.

The pointer variable `c1` is of type `char`, so the pointer inherits that type. We have to typecast it to (struct malloc_chunk*) to tell `print` to print sizeof(struct malloc_chunk) bytes of memory starting from c1 as if they represent a malloc_chunk.
```
(gdb) print *(struct malloc_chunk*)(c1)
$1 = {mchunk_prev_size = 0, mchunk_size = 0, fd = 0x0, bk = 0x20fe1, fd_nextsize = 0x0, bk_nextsize = 0x0}
```

The output on your end might be similar to this, but it is nowhere correct. The reason is, the pointer that malloc() returns is pointing to the payload memory, not the metadata that comes before. So, we have to subtract (2*size_t) bytes before casting the pointer to a (malloc_chunk*).
```
(gdb) print *(struct malloc_chunk*)(c - (2*sizeof(size_t)))
$2 = {mchunk_prev_size = 0, mchunk_size = 33, fd = 0x0, bk = 0x0, fd_nextsize = 0x0, bk_nextsize = 0x20fe1}

(gdb) p *(struct malloc_chunk*)(c1-16)
$3 = {mchunk_prev_size = 0, mchunk_size = 33, fd = 0x0, bk = 0x0, fd_nextsize = 0x0, bk_nextsize = 0x20fe1}
```
The size is 33 bytes, not 1 byte.

---

Continue the program by ENTERING `c`. Now we are requesting a chunk of 0 bytes, which is quite unsual, but let's see what the allocator returns. Press `c` another time to execute it.

Now print the chunk.
```
(gdb) p *(struct malloc_chunk*)(c2-16)
$4 = {mchunk_prev_size = 0, mchunk_size = 33, fd = 0x0, bk = 0x0, fd_nextsize = 0x0, bk_nextsize = 0x20fc1}
```

*/

/* OBSERVATION: 

  1. The size of a chunk is always request2size(sz), not sz.
  2. If the size after rounding up is less then MINSIZE, the size that gets allocated is MINSIZE. Therefore, the smallest size glibc-malloc can allocate in any configuration is MINSIZE bytes.

*/

#include <stdlib.h>

int main(void){
  char *c1 = malloc(1);
  int breakpoint = 1;

  char *c2 = malloc(0);
  breakpoint = 2;

  free(c1);
  free(c2);
}
