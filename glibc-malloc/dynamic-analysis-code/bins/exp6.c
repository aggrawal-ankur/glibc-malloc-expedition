/* EXPERIMENT: */

/* OBJECTIVE: Prove that the formula: (SMALLBIN_WDTH*i) gives the next smallbin size class. */

#include <stdlib.h>

int main(void){
  char* arr[126];
  size_t base = 20;

  for (int i=0; i<=62; i++){
    char *c1 = malloc(20);
    arr[i*2] = c1;
    char *c2 = malloc(base);
    arr[(i*2)+1] = c2;
    base += 16;
  }
  char *b = malloc(20);

  for (int i=0; i<=62; i++){
    free(arr[(i*2)+1]);
  }

  int x = 4;

  for (int i=0; i<=62; i++){
    free(arr[i*2]);
  }
}

// base + x = 16*i
// 20 + x = 16*2
// x = 32-20 = 16

// 20 + x = 16*3
// x = 48-20 = 28