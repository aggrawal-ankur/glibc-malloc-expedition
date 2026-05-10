/* EXPERIMENT 3 */

/* OBJECTIVE: Bin #64 is the first largebin in category #1, represented by the headers bin[126] and bin[127]. */

/* METHOD: 

  Step1: Allocate three chunks: the last chunk borders with the top, the first chunk stays as-is and the middle chunk is freed. The size of the middle chunk is 1010 bytes, which, after alignment, becomes 1024 bytes.

  Step2: Free the middle chunk, i.e. `c2` and set a breakpoint on the next line, i.e line #43 (`int x = 45`). This separates other frees from free(c2).

  Step3: From what we know already, the headers for bin #64 will be non-empty. They are <main_arena+1016>. But we can see them, which means, they are empty. So the chunk has not landed in the corresponding bin. Because the chunk is freed, it must have landed in some bin.

  Step4: Check the headers for bin #1, i.e. bin[0] and bin[1], or <main_arena+8>. They are not visible.

  Step5: Typecast the address to a (mchunkptr) and dereference it. Focus on mchunk_size, it should be 1025. The 1 is for the PREV_INUSE bit.

*/

/* OBSERVATIONS: 

  1. Bin #1, represented by bin[0] and bin[1] is the unsorted bin.
  2. Large chunks goes to the unsorted bin, before getting binned. Small chunks are binned directly.

*/

/* REASON: Before November 29, 2024, both small and large chunks went to the unsorted bin. On this date, a commit changed it. Why though? */

/* MORE DETAILS:

  COMMIT-ID: e2436d6f5aa47ce8da80c2ba0f59dfb9ffde08f3
  sourceware.org link: https://sourceware.org/git/?p=glibc.git;a=commit;h=e2436d6f5aa47ce8da80c2ba0f59dfb9ffde08f3

*/

#include <stdlib.h>

int main(void){
  char* c1 = malloc(10);
  char* c2 = malloc(1008);
  char* c3 = malloc(10);

  free(c2);
  int x = 45;

  free(c1);
  free(c3);
}
