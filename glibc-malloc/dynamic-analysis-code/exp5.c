/* EXPERIMENT 5 */

/* OBJECTIVE: Verify that a largebin is basically a collection of smallbin size classes. */

/* GIVEN: 

  1. We will use the first largebin in category #1.
  2. This category has bins of width 64 bytes from the base size.
  3. The range of bytes this largebin manages on 64-bit is 1009-1072.
  4. The number of smallbin size classes the bins in this category can fit are 4 (BIN_WIDTH/SMALLBIN_WIDTH, i.e. 64/16). These are 1024 (1008+(16*1)), 1040 (1008+(16*2)), 1056 (1008+(16*3)), 1072 (1008+(16*4)).
  5. Remember, these are request2size(sz) values, not the sz itself.

*/

/* METHOD: 

We have to find the correct malloc-free sequence that can trigger the binning of 4 large chunks, without getting merged with the top.

When there was 1 chunk to be freed, we had 2 chunks that surrounded it, like this:
  [c1, to_be_freed, c3]

If we try to continue this for another chunk, it would look like:
  [c1, to_be_freed, c3, to_be_freed, c5]

We can notice that first we have to malloc the chunk to be freed, then we have to malloc a chunk that barriers "the chunk to be freed" with the top chunk. We can reproduce this to whatever length we wish to.

Every time we malloc "the next chunk to be freed", the previously freed chunk is binned.

Every barrier chunk must have a size higher than the freed chunks, such that no reuse happens. This prevents two things:
  1. A reuse will trigger spliiting, which will reduce the size, possibly making the chunk a small chunk.
  2. If the chunk which is supposed to be "a barrier" is obtained from reuse, it is not a barrier chunk. As a result, "the chunk to be freed" will boundary with the top chunk, triggering a merger instead of binning.

*/

/* INSPECTION INSIDE GDB: 

Set break point on line #77 (`int x = 45`). Run.

The first largebin is represented by <main_arena+1016>. Since the bin is non-empty, we will search for <main_arena+1000>. The headers after this represent this largebin.

We can notice a difference in the head/tail pointers, which indicates the presence of multiple chunks in the list.

Take the head address and dereference it. Take the address in fd and dereference it. Do this until we reach the address that corresponds to the header of this bin.

Read fd/bk values to form the unordered links b/w the chunks.

*/

#include <stdlib.h>

int main(void){
  char *c1 = malloc(10);
  char *c2 = malloc(1010);
  char *c3 = malloc(10);
  
  free(c2);    // c2 goes to the unsorted bin.
  char *c4 = malloc(1500);    // c2 is binned.

  char *c5 = malloc(1030);
  char *c6 = malloc(1600);    // A higher size is required as the barrier chunk, otherwise, c5 will boundary with the top, which can trigger merger with top.

  free(c5);    // c5 goes to the unsorted bin.
  char *c7 = malloc(1700);    // c5 is binned.

  char *c8 = malloc(1045);
  char *c9 = malloc(1800);    // A high size barrier chunk.

  free(c8);    // c8 goes to the unsorted bin.
  char *c10 = malloc(1900);    // c8 is binned.

  char *c11 = malloc(1059);
  char *c12 = malloc(1900);    // A high size barrier chunk.

  free(c11);    // c11 goes to the unsorted bin.
  char *c13 = malloc(2000);    // c11 is binned.

  int x = 45;
  free(c1);
  free(c3);
  free(c4);
  free(c6);
  free(c7);
  free(c9);
  free(c10);
  free(c12);
  free(c13);
}
