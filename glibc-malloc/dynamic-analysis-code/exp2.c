/* EXPERIMENT 2 */

/* OBJECTIVE: Bin #63 is the last smallbin, represented by the headers bin bin[124] and bin[125], of size class 1008 bytes (MINSIZE, on 64-bit). */

/* METHOD: 

  Step1: Allocate three chunks: the last chunk borders with the top, the first chunk stays as-is and the middle chunk is freed. The size of the middle chunk is 994 bytes, which, after alignment, becomes 1008 bytes.

  Step2: Free the middle chunk, i.e. `c2` and set a breakpoint on the next line, i.e line #33 (`int x = 45`). This separates other frees from free(c2).

  Step3: Inspect the state of main_arena using the print functionality. Look for bin #63, which is represented by <main_arena+1000>. Since this bin is non-empty, this header will not be visible, which proves the point. Look for <main_arena+984> instead.

  Step4: The address after <main_arena+984> entries represent the address of the first free chunk in the list. Typecast it to a malloc_chunk* and dereference it. Focus on mchunk_size, it should be 1009. The 1 is for the PREV_INUSE bit.

*/

/* OBSERVATIONS: 

  1. Because headers that represent bins[124] and bin[125] are accessed, that implies the last smallbins is bin #63.
  2. It also proves that the upper bound for smallbins count is 63. Therefore, the bounds for smallbins numbers is [2, 63].
  3. Based on the bounds, the total number of smallbins are 62.

*/

#include <stdlib.h>

int main(void){
  char* c1 = malloc(10);
  char* c2 = malloc(994);
  char* c3 = malloc(10);

  free(c2);
  int x = 45;

  free(c1);
  free(c3);
}
