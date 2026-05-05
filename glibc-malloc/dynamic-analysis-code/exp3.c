/* EXPERIMENT 3 */

/* OBJECTIVE: VERIFY that bin #64 is the first largebin in category #1, represented by the headers bin[126] and bin[127]. */

/* METHOD:

  Step1: Allocate three chunks: the last chunk borders with the top, the first chunk stays as a barrier from the front and the middle chunk is freed. The size of the middle chunk is 1010 bytes, which after alignment becomes 1024 bytes.

  Step2: Free the middle chunk, i.e. `c2` and set a breakpoint on the next line, i.e line #38 (`int x = 45`). This separates other frees from free(c2).

  Step3: Inspect the state of main_arena using the print functionality. Look for bin #63, which is represented by <main_arena+1000>. But this bin is empty.

  Step4: Check the headers for bin #1, i.e. the unsorted bin, which are represented by bin[0] and bin[1], or <main_arena+8>. They are not visible.

  Step5: The address before <main_arena+24> entries represent the address of the first free chunk in the list. Typecast it to a malloc_chunk* and dereference it. Focus on mchunk_size, it should be 1025. The 1 is for the prev_inuse bit.

*/

/* OBSERVATION: Large chunks goes to the unsorted bin and small chunks are binned directly. */

/* REASON: Prior to November 29, 2024, both small and large chunks went to the unsorted bin. On this date, a commit changed it. */

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
