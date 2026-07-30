/* [EXPERIMENT 1] */

/* [OBJECTIVE]: Bin #1 is the unsorted bin, represented 
                by the headers bins[0] and bins[1].
*/

/* [SETUP] 

  [Step1]: Call malloc with sizes 10, 1010 and 10 bytes.

  [Step2]: Free the 1010 bytes chunk and set a breakpoint 
           on the next line, i.e line #26.

  [Step3]: Free c2 and check bins.

*/

#include <stdlib.h>

int main(void){
  char* c1 = malloc(10);
  char* c2 = malloc(1010);
  char* c3 = malloc(10);

  free(c2);
  int breakp = 1;

  free(c1);
  free(c3);
}

/* [ANALYSIS] 

  After alignment, 1010 becomes 1024 bytes, which belongs 
  to the first large bin in cateogry #1. So, the headers 
  for bin #64 will be non-empty. They are <main_arena+1016>.

  Inspect main_arena.bins and look for <main_arena+1016>. 
  It must be invisible. But we can see them, which means, 
  they are empty. That means, the chunk has not landed in 
  the corresponding bin. But the chunk has been freed, so 
  it must be somewhere.

  Find the headers for bin #1, i.e. bins[0] and bins[1], 
  or <main_arena+8>. They are not visible.

  Typecast the address to a (mchunkptr) and dereference 
  it. The mchunk_size whould be 1025 bytes.

  This also proves that large chunks enter the unsorted 
  bin before they are binned to the largebin of 
  corresponding size.
*/
