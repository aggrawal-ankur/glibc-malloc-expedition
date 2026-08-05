/* [EXPERIMENT #8] */

/* [OBJECTIVE]: Find the total number of bins. */

/* [SETUP] 

  [1] Allocate a chunk to initialize the allocator.
  [2] Tell GDB to print each element on a separate 
      line.
  [3] Tell GDB to print the index corresponding to 
      each element.
  [4] Tell GDB to print unlimited number of elements.
  [5] Now print main_arena.bins.

*/

#include <stdlib.h>

int main(void){
  char* c = malloc(20);
  int breakpoint = 1;
}

/* [ANALYSIS] 

  The last element would be:
  ```
  [253] = 0x7f91cb179468 <main_arena+2024>
  ```

  There are 254 elements and 254/2 is 127. So, 
  there are 127 bins in total.

*/
