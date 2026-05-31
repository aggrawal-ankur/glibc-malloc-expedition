#include <stdlib.h>
#include <string.h>

int main(void){
  char *c1 = malloc(3072-15);    // bin #96
  memset(c1, 0, 3072-15);

  char *b1 = malloc(1000-15);
  memset(b1, 0, 1000-15);
  
  char *c2 = malloc(3120-15);    // bin #96
  memset(c2, 0, 3120-15);

  char *b2 = malloc(1000-15);
  memset(b2, 0, 1000-15);

  char *c3 = malloc(3136-15);    // bin #97
  memset(c3, 0, 3136-15);

  char *b3 = malloc(1000-15);
  memset(b3, 0, 1000-15);

  char *c4 = malloc(3568-15);    // bin #97
  memset(c4, 0, 3568-15);

  char *b4 = malloc(1000-15);
  memset(b4, 0, 1000-15);
  
  char *c5 = malloc(3584-15);    // bin #98
  memset(c5, 0, 3584-15);

  char *b5 = malloc(1000-15);
  memset(b5, 0, 1000-15);
  
  char *c6 = malloc(4080-15);    // bin #98
  memset(c6, 0, 4080-15);

  char *b6 = malloc(1000-15);
  memset(b6, 0, 1000-15);

  free(c1);
  free(c2);
  free(c3);
  free(c4);
  free(c5);
  free(c6);

  char *ib1 = malloc(5000);
  memset(ib1, 0, 5000);
  int breakp = 1;




  char *c7 = malloc(10240-15);
  memset(c7, 0, 10240-15);

  char *b7 = malloc(5100-15);    // High size than the existing freed chunks, otherwise, reusal can be triggered. 
  memset(b7, 0, 5100-15);

  char *c8 = malloc(10736-15);
  memset(c8, 0, 10736-15);

  char *b8 = malloc(5100-15);
  memset(b8, 0, 5100-15);

  char *c9 = malloc(10752-15);
  memset(c9, 0, 10752-15);

  char *b9 = malloc(5100-15);
  memset(b9, 0, 5100-15);

  char *c10 = malloc(12272-15);
  memset(c10, 0, 12272-15);

  char *b10 = malloc(5100-15);
  memset(b10, 0, 5100-15);

  char *c11 = malloc(12288-15);
  memset(c11, 0, 12288-15);

  char *b11 = malloc(5100-15);
  memset(b11, 0, 5100-15);

  char *c12 = malloc(16368-15);
  memset(c12, 0, 16368-15);

  char *b12 = malloc(5100-15);
  memset(b12, 0, 5100-15);

  free(c7);
  free(c8);
  free(c9);
  free(c10);
  free(c11);
  free(c12);

  char *ib2 = malloc(20500);
  memset(ib2, 0, 20500);
  breakp = 2;




  char *c13 = malloc(36864-15);
  memset(c13, 0, 36864-15);

  char *b13 = malloc(24600-15);
  memset(b13, 0, 24600-15);

  char *c14 = malloc(40944-15);
  memset(c14, 0, 40944-15);

  char *b14 = malloc(24600-15);
  memset(b14, 0, 24600-15);

  char *c15 = malloc(40960-15);
  memset(c15, 0, 40960-15);

  char *b15 = malloc(24600-15);
  memset(b15, 0, 24600-15);

  char *c16 = malloc(65520-15);
  memset(c16, 0, 65520-15);

  char *b16 = malloc(24600-15);
  memset(b16, 0, 24600-15);

  char *c17 = malloc(65536-15);
  memset(c17, 0, 65536-15);

  char *b17 = malloc(24600-15);
  memset(b17, 0, 24600-15);

  char *c18 = malloc(98288-15);
  memset(c18, 0, 98288-15);

  char *b18 = malloc(24600-15);
  memset(b18, 0, 24600-15);

  free(c13);
  free(c14);
  free(c15);
  free(c16);
  free(c17);
  free(c18);

  char *ib3 = malloc(99000);
  memset(ib3, 0, 99000);
  breakp = 3;




  // This way, we can also demonstrate the default MTRIM_THRESHOLD not allowing more than 128k.
  char *c19 = malloc(131072-15);
  memset(c19, 0, 131072-15);

  char *b19 = malloc(28000-15);
  memset(b19, 0, 28000-15);

  char *c20 = malloc(163824-15);
  memset(c20, 0, 163824-15);

  char *b20 = malloc(28000-15);
  memset(b20, 0, 28000-15);

  char *c21 = malloc(163840-15);
  memset(c21, 0, 163840-15);

  char *b21 = malloc(28000-15);
  memset(b21, 0, 28000-15);

  char *c22 = malloc(262188-15);
  memset(c22, 0, 262188-15);

  char *b22 = malloc(28000-15);
  memset(b22, 0, 28000-15);

  char *c23 = malloc(262144-15);
  memset(c23, 0, 262144-15);

  char *b23 = malloc(28000-15);
  memset(b23, 0, 28000-15);

  char *c24 = malloc(524272-15);
  memset(c24, 0, 524272-15);

  char *b24 = malloc(28000-15);
  memset(b24, 0, 28000-15);

  char *c25 = malloc(524288-15);
  memset(c25, 0, 524288-15);

  char *b25 = malloc(28000-15);
  memset(b25, 0, 28000-15);

  free(c19);
  free(c20);
  free(c21);
  free(c22);
  free(c23);
  free(c24);
  free(c25);

  char *ib4 = malloc(30000);
  memset(ib4, 0, 3000);
  breakp = 4;
}
