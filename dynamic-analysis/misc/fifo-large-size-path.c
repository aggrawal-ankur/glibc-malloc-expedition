/* [EXPERIMENT]: FIFO ordering in equal-sized chunks in large bins. */

/* [OBJECTIVE]: Allocate 5 chunks of same size, force 
              them to get binned and inspect which 
              of them is returned when a chunk of 
              the same size is requested. */

/* [METHOD]: 

Set a break points on lines 47 and 50 and run.

Print the address of every c<n> chunk. Example:
```
(gdb) p (c1-16)
```

These chunks target the first large bin in category #1, 
i.e. bin #64, whose headers are bin[126] and bin[127]. 
Print them to be sure that the chunks have been binned.
```
(gdb) p main_arena.bins[126]
....

(gdb) p main_arena.bins[127]
```

Start with c1 and print every single chunk.
```
(gdb) p *(mchunkptr) (c1-16)
..
..
```

Use their fd/bk to form a doubly linked list. The result 
sould be: [c1, c5, c4, c3, c2].

Now continue.

---

Print the address of chunk `n`.
```
(gdb) p (n-16)
```

Now compare it with the addresses of the chunks above. It 
will match with the address of c5.

To reassure, print all the chunks in the bin and form the 
doubly linked list. */

#include <stdlib.h>

int main(void){
  char* c1 = malloc(1010);
  char* b1 = malloc(2000);

  char* c2 = malloc(1010);
  char* b2 = malloc(2000);

  char* c3 = malloc(1010);
  char* b3 = malloc(2000);

  char* c4 = malloc(1010);
  char* b4 = malloc(2000);

  char* c5 = malloc(1010);
  char* b5 = malloc(2000);

  free(c1);
  free(c2);
  free(c3);
  free(c4);
  free(c5);

  char* ib = malloc(5000);
  int bp = 1;

  char* n = malloc(1010);
  bp = 2;
}
