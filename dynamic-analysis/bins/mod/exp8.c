/* [EXPERIMENT #] */

/* [OBJECTIVE]: Perform an in-depth analysis of the pointer 
                fields in large chunks.
*/

/* [SETUP] 

  [NOTE-1]: This experiment assumes 64-bit architecture.
  [NOTE-2]: We will use the first largebin in category #1.

  This bin manages chunks of size classes 1024, 1040, 
  1056, and 1072 bytes.
  - We will allocate two chunks of each size class.
  - For each chunk, a barrier chunk is a must.
  - Upon freeing, a chunk to initiate binning is 
    required.
  - The size of the barrier chunks and the chunk to 
    initiate binning must be higher than the available 
    free chunks to ensure they can not be reused. As 
    before, we will use 2000 bytes for them.

  So, we need a total of 9 chunks.

  We will keep the order random. This is intentional 
  and will be discussed later.

  Set a breakpoint on line #68.
*/

#include <stdlib.h>

int main(void){
  char *c1 = malloc(1010);  // 1024
  char *b1 = malloc(2000);

  char *c2 = malloc(1044);  // 1056
  char *b2 = malloc(2000);

  char *c3 = malloc(1029);  // 1040
  char *b3 = malloc(2000);

  char *c4 = malloc(1060);  // 1072
  char *b4 = malloc(2000);

  char *c5 = malloc(1041);  // 1056
  char *b5 = malloc(2000);

  char *c6 = malloc(1010);  // 1024
  char *b6 = malloc(2000);

  char *c7 = malloc(1061);  // 1072
  char *b7 = malloc(2000);

  char *c8 = malloc(1031);  // 1040
  char *b8 = malloc(2000);

  free(c1);
  free(c2);
  free(c3);
  free(c4);
  free(c5);
  free(c6);
  free(c7);
  free(c8);

  char *ib = malloc(2000);
  int breakp = 1;

  free(b1);
  free(b2);
  free(b3);
  free(b4);
  free(b5);
  free(b6);
  free(b7);
  free(b8);
  free(ib);
}

/* [ANALYSIS] 

  Inspect the largebin.
  ```
  (gdb) p main_arena.bins[126]
  $1 = (mchunkptr) 0x563ab979f3d0

  (gdb) p main_arena.bins[127]
  $2 = (mchunkptr) 0x563ab97a0be0
  ```

  Take the head pointer and obtain every single chunk 
  in this large bin.
  ```
  (gdb) p *(mchunkptr) (0x563ab979f3d0)
  $3 = {
    mchunk_prev_size = 0, 
    mchunk_size = 1073, 
    fd = 0x563ab97a17c0, 
    bk = 0x7f3f7dd36078 <main_arena+1016>, 
    fd_nextsize = 0x563ab979dbe0, 
    bk_nextsize = 0x563ab979d000
  }

  (gdb) p *(mchunkptr) (0x563ab97a17c0)
  $4 = {
    mchunk_prev_size = 0, 
    mchunk_size = 1073, 
    fd = 0x563ab979dbe0, 
    bk = 0x563ab979f3d0, 
    fd_nextsize = 0x0, 
    bk_nextsize = 0x0
  }

  (gdb) p *(mchunkptr) (0x563ab979dbe0)
  $5 = {
    mchunk_prev_size = 0, 
    mchunk_size = 1057, 
    fd = 0x563ab979ffe0, 
    bk = 0x563ab97a17c0, 
    fd_nextsize = 0x563ab979e7e0, 
    bk_nextsize = 0x563ab979f3d0
  }

  (gdb) p *(mchunkptr) (0x563ab979ffe0)
  $6 = {
    mchunk_prev_size = 0, 
    mchunk_size = 1057, 
    fd = 0x563ab979e7e0, 
    bk = 0x563ab979dbe0, 
    fd_nextsize = 0x0, 
    bk_nextsize = 0x0
  }

  (gdb) p *(mchunkptr) (0x563ab979e7e0)
  $7 = {
    mchunk_prev_size = 0, 
    mchunk_size = 1041, 
    fd = 0x563ab97a23d0, 
    bk = 0x563ab979ffe0, 
    fd_nextsize = 0x563ab979d000, 
    bk_nextsize = 0x563ab979dbe80
  }

  (gdb) p *(mchun6kptr) (0x563ab97a23d0)
  $8 = {
    mchunk_prev_size = 0, 
    mchun8k_size = 1041, 
    fd = 0x563ab979d000, 
    bk = 0x563ab979e7e0, 
    fd_nextsize = 0x0, 
    bk_nextsize = 0x0
  }

  (gdb) p *(mchunkptr) (0x563ab979d0600)
  $9 = {
    mchunk_prev_size = 0, 
    mchunk_size = 1025, 
    fd = 0x563ab97a0be0, 
    bk = 0x563ab97a23d0, 
    fd_nextsize = 0x563ab979f3d0, 
    bk_nextsize = 0x563ab979e7e80
  }

  (gdb) p *(mchun6kptr) (0x563ab97a0be0)
  $10 = {
    mchunk_prev_size = 0, 
    mchunk_size = 1025, 
    fd = 0x7f3f7dd36078 <main_arena+1016>, 
    bk = 0x563ab979d000, 
    fd_nextsize = 0x0, 
    bk_nextsize = 0x0
  }
  ```

  We can notice a few interesting things here.
  [1] The fd/bk link is based on size, not the 
      order in which chunks are freed.
  [2] One chunk has the nextsize pointers real, 
      while the other one have them NULL.


  Now print the address of each chunk.
  ```
  (gdb) p (c1-16)
  $11 = 0x563ab979d000 ""

  (gdb) p (c2-16)
  $12 = 0x563ab979dbe0 ""

  (gdb) p (c3-16)
  $13 = 0x563ab979e7e0 ""

  (gdb) p (c4-16)
  $14 = 0x563ab979f3d0 ""

  (gdb) p (c5-16)
  $15 = 0x563ab979ffe0 ""

  (gdb) p (c6-16)
  $16 = 0x563ab97a0be0 ""

  (gdb) p (c7-16)
  $17 = 0x563ab97a17c0 ""

  (gdb) p (c8-16)
  $18 = 0x563ab97a23d0 ""
  ```

  Based on the fd/bk fields, we can construct a list.

                                                c4(1073)           
  [.... 0x7f3f7dd36078 <main_arena+1016> <-> 0x563ab979f3d0 <-> 

       c7(1073)           c2(1057)           c5(1057)
    0x563ab97a17c0 <-> 0x563ab979dbe0 <-> 0x563ab979ffe0 <-> 

       c3(1041)           c8(1041)         c1(1025)
    0x563ab979e7e0 <-> 0x563ab97a23d0 <-> 0x563ab979d000 <-> 

       c6(1025)
    0x563ab97a0be0 <-> 0x7f3f7dd36078 <main_arena+1016> ....]

  So, the order is: 
    [ c4 <-> c7 <-> c2 <-> c5 <-> c3 <-> c8 <-> c1 <-> c6 ]


  From the source code, we know that c1, c2, c3 and c4 
  are the unique chunks. Let's update the list by marking 
  them out.
    [ c4 <-> c7 <-> c2 <-> c5 <-> c3 <-> c8 <-> c1 <-> c6 ]
      ^             ^             ^             ^

  It is clear that duplicate chunks come after the unique 
  chunks. But what make c1, c2, c3, and c4 unique? There 
  are two things in their favor. They were both allocated 
  and freed first. Now we need to find which is actually 
  making them unique.

  The find that, we will reverse the order of freeing.
  - Copy the source code of this experiment.
  - Background this session with (CTRL + Z).
  - Create a new file and paste the source code.
  - Reverse the order of freeing.
  - Run in GDB.
  - Inspect the bin and form the linked list.

  This is the output.
  ```
  (gdb) p main_arena.bins[126]
  $1 = (mchunkptr) 0x562c04f207c0

  (gdb) p main_arena.bins[127]
  $2 = (mchunkptr) 0x562c04f1c000


  (gdb) p *(mchunkptr) 0x562c04f207c0
  $3 = {mchunk_prev_size = 0, mchunk_size = 1073, fd = 0x562c04f1e3d0, bk = 0x7f1f3e268078 <main_arena+1016>, fd_nextsize = 0x562c04f1efe0, bk_nextsize = 0x562c04f1fbe0}

  (gdb) p *(mchunkptr) 0x562c04f1e3d0
  $4 = {mchunk_prev_size = 0, mchunk_size = 1073, fd = 0x562c04f1efe0, bk = 0x562c04f207c0, fd_nextsize = 0x0, bk_nextsize = 0x0}

  (gdb) p *(mchunkptr) 0x562c04f1efe0
  $5 = {mchunk_prev_size = 0, mchunk_size = 1057, fd = 0x562c04f1cbe0, bk = 0x562c04f1e3d0, fd_nextsize = 0x562c04f213d0, bk_nextsize = 0x562c04f207c0}

  (gdb) p *(mchunkptr) 0x562c04f1cbe0
  $6 = {mchunk_prev_size = 0, mchunk_size = 1057, fd = 0x562c04f213d0, bk = 0x562c04f1efe0, fd_nextsize = 0x0, bk_nextsize = 0x0}

  (gdb) p *(mchunkptr) 0x562c04f213d0
  $7 = {mchunk_prev_size = 0, mchunk_size = 1041, fd = 0x562c04f1d7e0, bk = 0x562c04f1cbe0, fd_nextsize = 0x562c04f1fbe0, bk_nextsize = 0x562c04f1efe0}

  (gdb) p *(mchunkptr) 0x562c04f1d7e0
  $8 = {mchunk_prev_size = 0, mchunk_size = 1041, fd = 0x562c04f1fbe0, bk = 0x562c04f213d0, fd_nextsize = 0x0, bk_nextsize = 0x0}

  (gdb) p *(mchunkptr) 0x562c04f1fbe0
  $9 = {mchunk_prev_size = 0, mchunk_size = 1025, fd = 0x562c04f1c000, bk = 0x562c04f1d7e0, fd_nextsize = 0x562c04f207c0, bk_nextsize = 0x562c04f213d0}

  (gdb) p *(mchunkptr) 0x562c04f1c000
  $10 = {mchunk_prev_size = 0, mchunk_size = 1025, fd = 0x7f1f3e268078 <main_arena+1016>, bk = 0x562c04f1fbe0, fd_nextsize = 0x0, bk_nextsize = 0x0}


  (gdb) p (c1-16)
  $11 = 0x562c04f1c000 ""

  (gdb) p (c2-16)
  $12 = 0x562c04f1cbe0 ""

  (gdb) p (c3-16)
  $13 = 0x562c04f1d7e0 ""

  (gdb) p (c4-16)
  $14 = 0x562c04f1e3d0 ""

  (gdb) p (c5-16)
  $15 = 0x562c04f1efe0 ""

  (gdb) p (c6-16)
  $16 = 0x562c04f1fbe0 ""

  (gdb) p (c7-16)
  $17 = 0x562c04f207c0 ""

  (gdb) p (c8-16)
  $18 = 0x562c04f213d0 ""
  ```

  The linked list:
                                                  c7(1073)
    [.... 0x7f1f3e268078 <main_arena+1016> <-> 0x562c04f207c0 <-> 

        c4(1073)           c5(1057)           c2(1057)
      0x562c04f1e3d0 <-> 0x562c04f1efe0 <-> 0x562c04f1cbe0 <-> 

        c8(1041)           c3(1041)           c6(1025)
      0x562c04f213d0 <-> 0x562c04f1d7e0 <-> 0x562c04f1fbe0 <-> 

        c1(1025)
      0x562c04f1c000 <-> 0x7f1f3e268078 <main_arena+1016>.... ]

  Summary:
    [ c7 <-> c4 <-> c5 <-> c2 <-> c8 <-> c3 <-> c6 <-> c1 ]
      ^             ^             ^             ^

  Before we conclude that the order of freeing decides which 
  chunk will be unique, we have to consider another aspect.
  Large chunks go to the unsorted bin first. It is possible 
  that the freed chunk is used or split before it reaches 
  the relevant largebin. Therefore, the order in which chunks 
  are freed merely influences it. Whichever chunk (in its size 
  class) reaches the largebin first becomes unique, while the 
  rest become duplicate.

  Now exit this experiment and use (fg) to foreground the 
  main experiment.

  Based on the above exploration, it is clear that the fd/bk 
  fields are used to manage an ordered list of chunks.

  --- --- 

  Let's move on to the nextsize pointers. We can notice that 
  4 chunks have valid nextsize pointers, while the remaining 
  4 are NULL. Let's construct another list based on them.

               c1(1025)           c4(1073)
  [.... <-> 0x563ab979d000 <-> 0x563ab979f3d0 -> 
            0x563ab979dbe0 <-> 0x563ab979e7e0 -> ....]
               c2(1057)           c3(1041)

  0x0 <- 0x563ab97a17c0 -> 0x0
            c7(1073)
  0x0 <- 0x563ab979ffe0 -> 0x0
            c5(1057)
  0x0 <- 0x563ab97a23d0 -> 0x0
            c8(1025)
  0x0 <- 0x563ab97a0be0 -> 0x0
            c6(1041)

  We can notice that the unique chunks form a separate link 
  of themselves.

  A largebin manage chunks of different size classes. Higher 
  category largebins manage a huge number of size classes. 
  This increases the theoretical amount of chunks possible in 
  a largebin. If the allocator traversed them sequentially, 
  it could tank the performance. This list can be used to skip 
  the duplicate chunks.

*/
