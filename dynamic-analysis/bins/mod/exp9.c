/* [EXPERIMENT #] */

/* [OBJECTIVE]: Verify the number of largebins in category #1. */

/* [SETUP]: 

  We have already proven that a largebin is 
  essentially a collection of size classes, 
  with a differenceof SMALLBIN_WIDTH. We will 
  use this fact to verify whether the 33rd 
  largebin belongs to category #2 or #1.

  Run dynamic-analysis/scripts/bin_info_macro.py 
  to know the runtime size to bin index mapping 
  performed by the macros, while 
    dynamic-analysis/scripts/bin_info_pyramid.py
  is the theoretical reality as expressed by the 
  annotations.

  A category #1 largebin manage 4 size classes 
  and a category #2 largebin manage 32 size 
  classes. We will allocate 5 chunks starting 
  3072. If they appear in two bins, bin #96 is 
  a category #1 largebin, otherwise a category 
  #2 largebin.

  The sizes we are allocating are: 
    [1] 3057+(16*0) => 3057,
    [2] 3057+(16*1) => 1073,
    [3] 3057+(16*2) => 3089,
    [4] 3057+(16*3) => 3105, and
    [5] 3057+(16*4)] => 3121

  After request2size, they become:
    [3072, 3088, 3104, 3120, 3136]

  Set a breakpoint on line #.
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

    char* b = malloc(5000);
    memset(b, 0, 5000);
    bptrs[i] = b;

    BASE += 16;
  }

  for (int i=0; i<5; i++){
    free(cptrs[i]);
  }

  char* ib = malloc(5000);
  memset(ib, 0, 5000);
  int breakp = 1;

  for (int i=0; i<5; i++){
    free(bptrs[i]);
  }
  free(ib);
}


/* [ANALYSIS] 

  Print main_arena.bins.
  ```
  0x7fce9d04f268 <main_arena+1512>, 
  0x560ef441fee0, 0x560ef441a000, 0x560ef4421ea0, 0x560ef4421ea0, 
  0x7fce9d04f298 <main_arena+1560>,
  ```

  The bins associated to these addresses are #96 
  and #97 with the following offsets respectively, 
  <main_arena+1528> and <main_arena+1544>.
  
  We can dereferernce these addresses to find the 
  chunks associated to them.
  ```
  --- Bin #96 ---
  (gdb) p *(mchunkptr) (0x560ef441fee0)
  $2 = {
    mchunk_prev_size = 0,
    mchunk_size = 3121,
    fd = 0x560ef441df30,
    bk = 0x7fce9d04f278 <main_arena+1528>,
    fd_nextsize = 0x560ef441df30,
    bk_nextsize = 0x560ef441a000
  }

  (gdb) p *(mchunkptr) (0x560ef441df30)
  $3 = {
    mchunk_prev_size = 0,
    mchunk_size = 3105,
    fd = 0x560ef441bf90,
    bk = 0x560ef441fee0,
    fd_nextsize = 0x560ef441bf90,
    bk_nextsize = 0x560ef441fee0
  }

  (gdb) p *(mchunkptr) (0x560ef441bf90)
  $4 = {
    mchunk_prev_size = 0,
    mchunk_size = 3089,
    fd = 0x560ef441a000,
    bk = 0x560ef441df30,
    fd_nextsize = 0x560ef441a000,
    bk_nextsize = 0x560ef441df30
  }

  (gdb) p *(mchunkptr) (0x560ef441a000)
  $5 = {
    mchunk_prev_size = 0,
    mchunk_size = 3073,
    fd = 0x7fce9d04f278 <main_arena+1528>,
    bk = 0x560ef441bf90,
    fd_nextsize = 0x560ef441fee0,
    bk_nextsize = 0x560ef441bf90
  }

  --- Bin #97 ---
  (gdb) p *(mchunkptr) (0x560ef4421ea0)
  $6 = {
    mchunk_prev_size = 0,
    mchunk_size = 3137,
    fd = 0x7fce9d04f288 <main_arena+1544>,
    bk = 0x7fce9d04f288 <main_arena+1544>,
    fd_nextsize = 0x560ef4421ea0,
    bk_nextsize = 0x560ef4421ea0
  }
  ```


  This proves that 
    - bin #96 is a category #1 largebin, 
    - condition #1 in largbin_index64 accepts 
      an extra bin, and 
    - there are 33 largebins in category #1.
*/
