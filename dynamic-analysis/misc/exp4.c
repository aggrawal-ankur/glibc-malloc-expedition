/* [EXPERIMENT #4] */

/* [OBJECTIVE]: Verify that "very large size" chunks are 
                serviced via mmap.
*/

/* [SETUP] 

  Mmap threshold is a tunable value, and the default is 
  DEFAULT_MMAP_THRESHOLD_MIN, which is (128*1024) bytes. 
  A request larger than this is considered a large size.

  A large size can be obtained in a few ways.
  [1] The process requested a large size directly, like 
      (mmap_threshold+10).

  [2] A size less than the mmap threshold, but exceeds 
      it with request2size alignment. For example, 
        (mmap_threshold-15).

  [3] A size that is only a few standard pages less than 
      the mmap threshold but the alignment math makes it 
      large. For example (mmap_threshold + 4096*n - 50).

  In case-1, (bytes > mmap_threhsold).
  In case-2, (nb > mmap_threshold).
  In case-3, (size > mmap threshold).

  Now look at _int_malloc's path stack. There is no notion 
  of "very large size". Either there is small size or large 
  size. When a request can not be fulfilled, sysmalloc is 
  called.

  Now look at sysmalloc's path stack. The first path checks 
  if the "normalized size" (nb) is "very large". None of the 
  paths check how the "aligned size" (size) compares to the 
  mmap_threshold.

  So, we will request a chunk more than the mmap_threshold 
  and trace the path taken. It must be path-1 in sysmalloc.

  Use (s) to step into _int_malloc and sysmalloc. Use (n) 
  to move to the next instruction.

  [NOTE]: When the execution reaches path-1 in sysmalloc, 
          step into sysmalloc_mmap and check the value of 
          padding. It should be zero as the expression is 
          a compile-time zero.
          ```
          size_t padding = MALLOC_ALIGNMENT - CHUNK_HDR_SZ;
          ```
*/

#include <stdlib.h>

int main(void){
  char* c = malloc((128*1024) + 4096);
  int breakp = 1;
}

/* [ANALYSIS] 

  Inspect the returned chunk just at the breakpoint. 
  Since it is an mmapped chunk, the IS_MMAPPED bit (0x2) 
  would be set. Print the chunk, and take bitwise AND of 
  0x2 with mchunk_size. The result would be 0x2.

*/
