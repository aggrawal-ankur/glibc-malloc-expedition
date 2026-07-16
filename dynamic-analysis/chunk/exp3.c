/* EXPERIMENT 3 */

/* OBJECTIVE: The dummy chunk and the boundary tag implementation. */

/* SETUP: 

  1. Start and attach the container to your terminal.
  2. `cd` to /experiments/
  3. Build the lab: `./build exp3.c`
  4. Create a breakpoint on line #109.
  6. Run the program: `r`.

*/

/* ANALYSIS: We wil allocate a chunk of 20 bytes.

In the previous experiment, upon freeing the chunk, it's size increased multifold, pow(2, 12) times, to be exact. Let's find why.

Print the main chunk.
```
(gdb) print *(struct malloc_chunk*)(c-16)
$1 = {mchunk_prev_size = 0, mchunk_size = 33, fd = 0x0, bk = 0x0, fd_nextsize = 0x0, bk_nextsize = 0x20fe1}
```

We had exactly the same output in the previous two experiments, where the first three pointer fields were 0x0, but bk_nextsize was 0x20fe1, every single time.

---

We know that request2size(sz) includes the header size, so, to access next chunk ((n+1)th chunk), we can add mchunk_size of the nth chunk to its address.
```
(gdb) p *(struct malloc_chunk*)((c-16) + ((*(struct malloc_chunk*)(c-16))->mchunk_size)-1)
$7 = {mchunk_prev_size = 0, mchunk_size = 135137, fd = 0x0, bk = 0x0, fd_nextsize = 0x0, bk_nextsize = 0x0}
```

The first expression gets us to the head of the nth chunk and the second expression accesses the mchunk_size field of it. It is equivalent of:
```
struct malloc_chunk* ptr = *(struct malloc_chunk*)((c-16);
size_t c_size = ptr->mchunk_size;
struct malloc_chunk* top = (struct malloc_chunk*) ((char*)ptr + c_size);
```

This chunk is that dummy chunk, or the top chunk that exist as the last chunk to implement the boundary tag method. It's size is 135137 bytes. Why the size is specifically this is something we have not explored yet. So we will avoid it.

135137 is a decimal value. The hexadecimal equivalent is 0x20fe1. You can check it yourself. Open any number system converter online, or use the python3 shell on your distribution and type `hex(135137)`.
```
>>> hex(135137)
'0x20fe1'
```

That's what bk_nextsize is showing. It is clear that fd_nextsize/bk_nextsize of our chunk are overlapping with prev_size/mchunk_size of the top chunk.

If we try to build the structure of memory, it'd be:

...............Main Chunk (request2size(20))...............
[ prev_size  mchunk_size  fd  bk  fd_nextsize  bk_nexsize ]
      0          33       0x0 0x0     0x0        135137     0x0 0x0     0x0          0x0
                                  [ prev_size  mchunk_size  fd  bk  fd_nextsize  bk_nextsize ]
                                  ..........................Top Chunk.........................

---

Now continue the program with `c`. The memcpy instruction fills the 20 bytes with a stream of characters. Continue again. If we print the contents, we get:
```
(gdb) p c
$2 = 0x55b582cf2010 "Systems are my buddy"
```

Now print the chunk.
```
(gdb) p *(struct malloc_chunk*)(c-16)
$1 = {mchunk_prev_size = 0, mchunk_size = 33, fd = 0x20736d6574737953, bk = 0x6220796d20657261, 
  fd_nextsize = 0x79646475, bk_nextsize = 0x20fe1}
```

Now print the top chunk.
```
(gdb) p *(struct malloc_chunk*)(c-16+32)
$3 = {mchunk_prev_size = 2036622453, mchunk_size = 135137, fd = 0x0, bk = 0x0, fd_nextsize = 0x0, 
  bk_nextsize = 0x0}
```

Check what 0x79646475 is in the python shell.
```
>>> 0x79646475
2036622453
```

This proves that the pointer fields are garbage in an in-use chunk.

---

Understanding the top chunk is essential to obtain a free chunk. In our previous experiment, the size after freeing the chunk was 135169. Subtract 135137 from it, we get 32. That means, the chunk bordering the top chunk gets coalesced to it upon freeing. To obtain a free chunk, a barrier must exist between the chunk to be freed and the top chunk.

In the next experiment, we explore how to obtain a free chunk.

*/

/* OBSEVRATIONS: 

  1. When the first chunk comes into existence, a top chunk comes alongside it.
  2. The top chunk's prev_size is structurally it's property, but functionally, it is used by the chunk previous to it. That's boundary tag method.
  3. In case of a MINSIZE chunk, fd_nexsize/bk_nextsize overlaps with the prev_size/mchunk_size fields of the top chunk, which makes the fields read the same value. When the payload memory was populated, the overlapped fields showed the same values.
  4. When the chunk size is > sizeof(struct malloc_chunk), the fields won't overlap.
  5. To obtain a free chunk, a barrier chunk must exist b/w the chunk to be freed and the top chunk.
  6. The pointer fields are garbage in an in-use chunk.

*/

#include <stdlib.h>
#include <string.h>

int main(void){
  char *c = malloc(20);
  int breakpoint = 1;

  memcpy(c, "Systems are my buddy", 20);
  breakpoint = 2;

  free(c);
}
