/* EXPERIMENT 1 */

/* OBJECTIVE: Verify that bin #2 is the first smallbin, represented by the headers bin[2] and bin[3], of size class 32 bytes (MINSIZE on 64-bit). */

/* METHOD:

  Step1: Allocate three three chunks: the last chunk borders with the top, the first chunk stays as a barrier from the front and the middle chunk is freed. The size of the middle chunk is 10 bytes, which after alignment becomes 32 bytes.

  Step2: Free the middle chunk, i.e. `c2` and set a breakpoint on the next line, i.e line #25 (`int x = 45`). This separates other frees from free(c2).

  Step3: Inspect the state of main_arena using the print functionality. Look for bin #2, which is represented by <main_arena+24>. Since this bin is non-empty, this header will not be visible, which proves the point. Look for <main_arena+8> instead.

  Step4: The address after <main_arena+8> entries represent the address of the first free chunk in the list. Typecast it to a malloc_chunk* and dereference it. Focus on mchunk_size, it should be 33. The 1 is for the prev_inuse bit.

*/

#include <stdlib.h>

int main(void){
  char* c1 = malloc(10);
  char* c2 = malloc(10);
  char* c3 = malloc(10);

  free(c2);
  int x = 45;

  free(c1);
  free(c3);
}
