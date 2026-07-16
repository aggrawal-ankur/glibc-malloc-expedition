/* EXPERIMENT 9 */

/* OBJECTIVE: Verify the number of largebins in category #1. */

/* METHOD: 

We have already proven that a largebin is essentially a collection of fixed size classes, separated by SMALLBIN_WIDTH. We will use this fact to verify whether the 33rd largebin belongs to category #2 or category #1.

Run fake_node_addr_to_bin.py. A mapping.txt file is generated. Open it, and we can find a table that maps the output of `(gdb) p main_arena.bins` with bin details.
  - The last largebin in category #1, i.e. bin 95, is for size 3008 bytes, having a width of 64 bytes, with 4 size classes in it.
  - The first largebin in category #2, i.e. bin #96, starts at 3072 bytes, having a width of 512 bytes, with 32 size classes in it.

As per largebin_index_64, we know that bin #96 is in confusing position as both condition #1 and condition #2 allow that size.

To find whether this bin belongs to category #1 or #2, we can simply allocate 5 chunks of size > 3056, as this is the last size bin #95 can manage.
  - If bin #33 is a largebin in category #2, it can contain all the 5 chunks, as it is 512 bytes wide from the base size, so it can contain 32 unique sized chunks.
  - If bin #33 is a largebin in category #1, it will only accept the first four chunks and the fifth chunk will go to bin #97.

*/

#include <stdlib.h>
#include <string.h>

int main(void){
  unsigned int BASE = 3057;
  unsigned int SMALLBIN_WIDTH = 16;

  char* cptrs[5];
  char* bptrs[5];

  for (int i=0; i<5; i++){
    char* c = malloc(BASE);
    memset(c, 0, BASE);
    cptrs[i] = c;

    char* b = malloc(8000);
    memset(b, 0, 8000);
    bptrs[i] = b;

    BASE += 16;
  }

  for (int i=0; i<5; i++){
    free(cptrs[i]);
  }

  char* ib = malloc(8000);    // To initiate binning.
  memset(ib, 0, 8000);
  int breakp = 1;

  for (int i=0; i<5; i++){
    free(bptrs[i]);
  }
  free(ib);
}


/* ANALYSIS: 

The sizes we are allocating are: [3057+(16*0), 3057+(16*1), 3057+(16*2), 3057+(16*3), 3057+(16*4)], i.e. [3057, 3073, 3089, 3105, 3121]

After request2size, they become: [((3057+(16*0)+23) & ~15), ((3057+(16*1)+23) & ~15), ((3057+(16*2)+23) & ~15), ((3057+(16*3)+23) & ~15), ((3057+(16*4)+23) & ~15)], i.e. [3072, 3088, 3104, 3120, 3136]

Set a breakpoint on line #49. Run. Print the main_arena.bins

This is what the output would be like:
```
0x7f25cd396268 <main_arena+1512>, 0x7f25cd396268 <main_arena+1512>, 0x55984cad6220, 0x55984cace000, 0x55984cad8da0, 0x55984cad8da0, 
0x7f25cd396298 <main_arena+1560>, 0x7f25cd396298 <main_arena+1560>
```

Now open mapping.txt again and check what bins these <main_arena+x> belong to.
  - <main_arena+1512> is bin #95, i.e. 32th largebin in category #1.
  - <main_arena+1560> is bin #98, i.e.  3rd largebin in category #2.

We can already notice two non-empty bins. Now print the chunk at 0x55984cad8da0.
```
(gdb) p *(mchunkptr) 0x55984cad8da0
$2 = {mchunk_prev_size = 0, mchunk_size = 3137, fd = 0x7f25cd396288 <main_arena+1544>, bk = 0x7f25cd396288 <main_arena+1544>, fd_nextsize = 0x55984cad8da0, bk_nextsize = 0x55984cad8da0}
```

*/

/* OBSERVATIONS: 

  1. This proves that condition #1 accepts an extra bin, which is the first largebin in cateogry #2.
  2. There are 33 largebins in category #1, not 32.

*/
