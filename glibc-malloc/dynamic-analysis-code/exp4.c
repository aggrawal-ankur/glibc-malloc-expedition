/* EXPERIMENT 4 */

/* OBJECTIVE: Bin #64 is the first largebin in category #1, represented by the headers bin[126] and bin[127]. */

/* METHOD:

  Step1: Allocate four chunks of sizes (10, 1008, 10, 1100). After alignment, c2 will become a 1024 bytes chunk. Chunk2 will be the focus and chunk3 will act as barrier b/w chunk2 and the top chunk. Because chunk2 will go to the unsorted bin first, we have to make another malloc call with a higher size to get c2 binned.

  Step2: Free c2, malloc a large chunk of size 1100 bytes, and set a breakpoint on line # (`int x = 45`).

  Step3: Inspect the state of main_arena using the print functionality. Look for bin #64, which is represented by <main_arena+1016>. Since this bin is non-empty, this header will not be visible, which proves the point. Look for <main_arena+1000> instead.

  Step4: The address after <main_arena+1000> entries represent the address of the first free chunk in the list. Typecast it to a malloc_chunk* and dereference it. Focus on mchunk_size, it should be 1025. The 1 is for the prev_inuse bit.

*/

#include <stdlib.h>

int main(void){
  char* c1 = malloc(10);
  char* c2 = malloc(1008);
  char* c3 = malloc(10);

  free(c2);
  char* c4 = malloc(1100);
  int x = 45;

  free(c1);
  free(c3);
  free(c4);
}
