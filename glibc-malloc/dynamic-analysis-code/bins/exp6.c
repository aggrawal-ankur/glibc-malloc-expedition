/* EXPERIMENT: */

/* OBJECTIVE: The smallbin size classes are in the range: `[MINSIZE, MIN_LARGE_SIZE-SMALLBIN_WIDTH]`, with a step of SMALLBIN_WIDTH. */

/* SETUP: We have to allocate all the sizes in this range.

We have two ways to do this.
  1. Manually find the sizes which, after alignment, become a valid size class in the sequence.
  2. Find a method that can calculate the size which, after alignment, becomes a valid size in the sequence.

Either way, the approach is going to be the same. Option #2 just automates the process. Let's find out how.

We have 62 smallbins. For each bin, we need 1 chunk. So we need 62 chunks, which implies, 62 pointer variables. We can already see cracks in the manual aproach now.

For each chunk, we need a barrier chunk next to it. Otherwise, the freed chunks would coalesce. So, 62 more chunks. Therefore, we need 124 chunks in total.

We can run a for loop to allocate both the main chunk and the barrier chunk in each iteration. In 62 iterations, we will get 124 chunks.

We can fix the size of the barrier chunk to 20 bytes. Now we have to find the formula to obtain a size which upon alignment becomes the next size class.
  - From our previous experiments, we know that a size of 20 bytes leads to a 32 bytes chunk. Let's consider it our starting point.
  - On 64-bit, the request2size(sz) macro basically adds 23 to `sz`, before taking bitwise AND with MALLOC_ALIGN_MASK. When we add 23 to 20, we get 43. And 43 & ~15 is 32.
  - If we add 10 to the base size, we get 30 in the next iteration. Now 30 + 23 is 53. And 53 & ~15 is 48. We got the next size class.
  - If we add 10 again, we get 40. Now 40 + 23 is 63. And 63 & ~15 is 48. Here we got it wrong. That means, 10 is not the correct value.
  - When we add 23 to the base size, the resultant value must be in this range: [target_size_class, next_size_class). Only then the bitwise AND can bring it to the targeted size class. When we add 23 to 40, our target size class was 64, but the resulting value was 63. As a result, we end up having the wrong size class.
  - We are using 20 as the base size. It is 12 bytes lesser than its size class (32 bytes). When we had 30 (20+10), it was 18 bytes lesser than its size class (48 bytes). When we had 40, it was 24 bytes lesser than its size class (64 bytes). If we had 50, it'd be 30 bytes lesser than its size class.
  - The difference b/w the requried value and our value (Size, without +23) is increasing, which explains why 10 is not a correct value. It needs to be consistent.
  - We have to automate the process upto some values so that we can skip the manual labor. The ./bins/add_va_finder.py script does exactly the same. Run it. It will generate some files, with some output. Have a look it at. To understand the crux, read the code that is commented in the script.
  - We can notice that the only add value that works is 16. The difference b/w target and Size is is 12 bytes. Because it generates the right size classes, we can use 16 as the add value.

The for loop will request memory, but the life of c1 and c2 chunks is the for loop block. To save the pointers, we will create a `char*` array for 124 pointers.
  - The even indices have the chunks to be freed.
  - The odd indices have the barrier chunks.

*/

/* ANALYSIS: 

  1. Set a breakpoint on line #63 and run.
  2. Now print the bins[] array: `p main_arena.bins`
  3. Headers for the unsorted bin, i.e. <main_arena+8>, are visible. That means, all the sizes are within the smallbin size classes range, i.e [MINSIZE, MIN_LARGE_SIZE-SMALLBIN_WIDTH]
  4. Further, we can pick the first and the last addresses and print the chunk at them. The sizes will be 33 and 1009, proving the range correct.

*/

#include <stdlib.h>

int main(void){
  char* arr[124];
  size_t base = 20;

  for (int i=0; i<62; i++){
    char *c1 = malloc(base);
    arr[i*2] = c1;
    base += 16;

    char *c2 = malloc(20);
    arr[(i*2)+1] = c2;
  }

  for (int i=0; i<62; i++){
    free(arr[i*2]);
  }
  int x = 4;

  for (int i=0; i<62; i++){
    free(arr[(i*2)+1]);
  }
}
