/* [EXPERIMENT 3] */

/* [OBJECTIVE]: The smallbin size classes are in the 
                range: 
                  [MINSIZE, MIN_LARGE_SIZE-SMALLBIN_WIDTH], 

                .... with a step of SMALLBIN_WIDTH.
*/

/* [SETUP]: Allocate all the sizes in this range 
            programatically.

  [NOTE]: This experiment assumes 64-bit architecture.

  We have 62 smallbins.
  - We need one chunk per bin. So, 62 chunks, meaning 
    62 pointers.
  - For each chunk, we need a barrier chunk to prevent 
    coalescing. So, 62 more chunks.
  - In total, we need 124 chunks.

  We will use a for loop to allocate these chunks 
  and store the returned pointers in an array. 
  Normal chunk on even indices and barrier chunks 
  on odd.

  We can fix the size of the barrier chunk to 20 
  bytes. Now we have to find the formula to obtain 
  the next size which, upon alignment becomes the 
  next size class.
  - We know that a size of 20 bytes leads to a 32 
    bytes chunk. Let's consider it our starting 
    point.
  - request2size(sz) basically adds 23 to sz, before 
    taking bitwise AND with MALLOC_ALIGN_MASK. When 
    we add 23 to 20, we get 43. And 43 & ~15 is 32.
  - If we add 10 to the base size, we get 30 in the 
    next iteration. 30 + 23 is 53. And 53 & ~15 is 
    48. We got the next size class.
  - If we add 10 again, we get 40. 40 + 23 is 63. And 
    63 & ~15 is 48. But we need 64. That means, 10 is 
    not the right base value.
  - When we add 23 to the base size, the resultant 
    value must be in this range: 
      [target_size_class, next_size_class).

    .... only then the bitwise AND can bring it down 
    to the targeted size class. When we add 23 to 40, 
    our target size class was 64, but the resulting 
    value was 63. As a result, we go the wrong size 
    class.
  - 20 is 12 bytes less than its size class (32 bytes). 
    30 (20+10) is 18 bytes less than its size class 
    (48 bytes).
    40 (30+10) is 24 bytes less than its size class 
    (64 bytes).
    50 (40+10) is 30 bytes less than its size class 
    (80 bytes).
  - The difference b/w the requried size and our size 
    is increasing. That explains why 10 is not the 
    right additive.
  - We have to try different additives. There is a 
    script named add_val_finder.py that automates 
    this. It is in `/dynamic-analysis/scripts`. Read 
    it to understand what it does and how it does it.
  - We can notice that the only addend that works is 
    16. The difference b/w the target and the input 
    size is 12 bytes. Because it generates the right 
    size classes, we can use 16 as the add value.


  Set a breakpoint on line #94 and run.
*/

#include <stdlib.h>

int main(void){
  char* arr[124];
  size_t base = 20;

  for (int i=0; i<62; i++){
    char* c = malloc(base);
    arr[i*2] = c;

    char* b = malloc(20);
    arr[(i*2)+1] = b;

    base += 16;
  }

  for (int i=0; i<62; i++){
    free(arr[i*2]);
  }
  int breakp = 1;

  for (int i=0; i<62; i++){
    free(arr[(i*2)+1]);
  }
}

/* [ANALYSIS] 

  Print the main_arena. The elements starting from <main_arena+24> up to <main_arena+1000> will have chunk addresses. The unsorted bin is empty, meaning all the sizes are small.

  In the next experiment, we will explore that bin #64 is the first largebin, which will further prove this point.
*/
