/* EXPERIMENT 8 */

/* OBJECTIVE: An in-depth analysis of the pointer fields in large chunks. */

/* SETUP: We will use the first largebin in category #1.

We need multiple chunks of sizes that can be binned in this bin, i.e. 1024, 1040, 1056, and 1072.
  - We will allocate 8 chunks, two chunk of each size. Remember, these are request2size(size) values, not the size itself.
  - Next, we will allocate 8 barrier chunks of size 2000 bytes each. No freed chunk is sized enough to be reused.
  - Last, we will allocate another chunk of size 2000 bytes to initiate binning of all the free chunks.

We will make sure that their order is random. This is intentional and will be discussed properly.

After the chunks are freed, the state of memory would be:

    [.., b1, .., b2, .., b3, .., b4, .., b5, .., b6, .., b7, .., b8, c9 ]

*/

#include <stdlib.h>
#include <string.h>

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

  char *c9 = malloc(2000);
  int x = 45;

  free(b1);
  free(b2);
  free(b3);
  free(b4);
  free(b5);
  free(b6);
  free(b7);
  free(b8);
  free(c9);
}

/* ANALYSIS: 

Set a breakpoint on line #58.

Print main_arena. Our bin is <main_arena+1016>, which is not visible as there are chunks in it.
```
(gdb) p main_arena.bins[126]
$1 = (mchunkptr) 0x563ab979f3d0

(gdb) p main_arena.bins[127]
$2 = (mchunkptr) 0x563ab97a0be0
```

These are the chunks on the two ends of the list. If we pick one and print each chunk using fd, we will get this:
```
(gdb) p *(mchunkptr) 0x563ab979f3d0
$3 = {mchunk_prev_size = 0, mchunk_size = 1073, fd = 0x563ab97a17c0, bk = 0x7f3f7dd36078 <main_arena+1016>, fd_nextsize = 0x563ab979dbe0, bk_nextsize = 0x563ab979d000}

(gdb) p *(mchunkptr) 0x563ab97a17c0
$4 = {mchunk_prev_size = 0, mchunk_size = 1073, fd = 0x563ab979dbe0, bk = 0x563ab979f3d0, fd_nextsize = 0x0, bk_nextsize = 0x0}

(gdb) p *(mchunkptr) 0x563ab979dbe0
$5 = {mchunk_prev_size = 0, mchunk_size = 1057, fd = 0x563ab979ffe0, bk = 0x563ab97a17c0, fd_nextsize = 0x563ab979e7e0, bk_nextsize = 0x563ab979f3d0}

(gdb) p *(mchunkptr) 0x563ab979ffe0
$6 = {mchunk_prev_size = 0, mchunk_size = 1057, fd = 0x563ab979e7e0, bk = 0x563ab979dbe0, fd_nextsize = 0x0, bk_nextsize = 0x0}

(gdb) p *(mchunkptr) 0x563ab979e7e0
$7 = {mchunk_prev_size = 0, mchunk_size = 1041, fd = 0x563ab97a23d0, bk = 0x563ab979ffe0, fd_nextsize = 0x563ab979d000, bk_nextsize = 0x563ab979dbe80}
1025
(gdb) p *(mchun6kptr) 0x563ab97a23d0
$8 1041= {mchunk_prev_size = 0, mchun8k_size = 1041, fd = 0x563ab979d000, bk = 0x563ab979e7e0, fd_nextsize = 0x0, bk_nextsize = 0x0}
1025
(gdb) p *(mchunkptr) 0x563ab979d0600
$9 1041= {mchunk_prev_size = 0, mchunk_size = 1025, fd = 0x563ab97a0be0, bk = 0x563ab97a23d0, fd_nextsize = 0x563ab979f3d0, bk_nextsize = 0x563ab979e7e80}
1025
(gdb) p *(mchun6kptr) 0x563ab97a0be0
$101041 = {mchunk_prev_size = 0, mchunk_size = 1025, fd = 0x7f3f7dd36078 <main_arena+1016>, bk = 0x563ab979d000, fd_nextsize = 0x0, bk_nextsize = 0x0}
```

We can notice that the chunks are ordered based on their size, not based on the order of freeing. This insight couldn't be obtained if the chunks were not malloced in a random order.

Based on the fd/bk fields, we can construct a list.

  [....0x7f3f7dd36078 <main_arena+1016> -> 0x563ab979f3d0 <-> 0x563ab97a17c0 <-> 0x563ab979dbe0 <-> 0x563ab979ffe0 <-> 0x563ab979e7e0 <-> 0x563ab97a23d0 <-> 0x563ab979d000 <-> 0x563ab97a0be0 -> 0x7f3f7dd36078 <main_arena+1016>....8]
1025
The only thing missing in this list is which address correspond to which chunk. To find that information, we have6 to print each chu1041nk manually. But we have to print after subtracting 2*sizeof(size_t) bytes the addresses in bins are for the start of the chunk while the addresses print directly are for the payload memory.
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
`8``
1025
B6ased on this information, we can update our list.1041

PLEASE KEEP THE WORD WRAP ENABLED, OR ENABLED IF NOT ALREADY.

                                              c4(1073)           c7(1073)           c2(1057)           c5(1057)           c3(1041)
  [....0x7f3f7dd36078 <main_arena+1016> -> 0x563ab979f3d0 <-> 0x563ab97a17c0 <-> 0x563ab979dbe0 <-> 0x563ab979ffe0 <-> 0x563ab979e7e0 <-> 0x563ab97a23d0 <-> 0x563ab979d000 <-> 0x563ab97a0be0 -> 0x7f3f7dd36078 <main_arena+1016>....]
         c8(1041)           c1(1025)           c6(1025)                

So, the order is: [ c4 -> c7 -> c2 -> c5 -> c3 -> c8 -> c1 -> c6 ]

If we try to extract some more information from this order, we can notice that c4, c2, c3 and c1 are the first allocation of their sizes and c7, c5, c8, and c6 are the second ones.

To find if this order is special or just a downstream effect of the order of freeing, we have to free these chunks in reverse order. 
  - Although it is possible to do this inside the same lab by reallocating chunks of same sizes again and then free them again in reverse order and to not flood the experiment's code with mallocs, we can use a array to contain the pointers. But that's a lot of hassle.
  - Therefore, there is another file named exp8(2).c, which contains only the code and the free sequence in reverse order. We will use it to find whether this order is special or not.
  - There are two ways we can do this. Either we can close the current gdb session and start the new one; or we can background this with ctrl+z, initiate the new one, verify, close, and come back. That's what we will do.
  - Although I will paste the results from my side, it is highly recommended to do it yourself.

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

The list:
                                         c7(1073)          c4(1073)          c5(1057)            c2(1057)        c8(1041)          
  [....0x7f1f3e268078 <main_arena+1016> 0x562c04f207c0 -> 0x562c04f1e3d0 -> 0x562c04f1efe0 -> 0x562c04f1cbe0 -> 0x562c04f213d0 -> 0x562c04f1d7e0 -> 0x562c04f1fbe0 -> 0x562c04f1c000 -> 0x7f1f3e268078 <main_arena+1016>.... ]
     c3(1041)          c6(1025)          c1(1025)

So, it does depend on the order of freeing. Anyways, let's foreground the main experiment by pressing `fg`.

---

Based on this, we can say that the fd/bk pointers are used to manage an ordered list of chunks in largebins.

Have a look at nextsize pointers now. We can notice that 4 of the chunks of valid nextsize pointers, while the remaining 4 are basically NULL (0x0). Based on them, we can construct another list.

.... <- 0x563ab979d000 <-> 0x563ab979f3d0 -> 0x563ab979dbe0 <-> 0x563ab979e7e0 -> ....
           c1(1025)           c4(1073)          c2(1057)           c3(1041)

0x0 <- 0x563ab97a17c0 -> 0x0
          c7(1073)
0x0 <- 0x563ab979ffe0 -> 0x0
          c5(1057)
0x0 <- 0x563ab97a23d0 -> 0x0
          c8(1025)
0x0 <- 0x563ab97a0be0 -> 0x0
          c6(1041)

*/

/* OBSERVATIONS: 

  1. A large chunk uses all the fields.
  2. The fd/bk pointers maintain an ordered list (based on size and oreder of freeing). That's the order of insertion within bins. When there are chunks of same size, they come after the chunk (of the same size) that is sitting before them.
  3. The fd_nextsize/bk_nextsize pointers maintain a skip list of unique nodes. There is only one such list. The remaining nodes have the skip pointers set to NULL. Why not multiple skip lists though?

*/
