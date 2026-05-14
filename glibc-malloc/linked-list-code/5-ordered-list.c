/*

Now the list is ordered based on the data field. Consequently, separate function for HEAD/TAIL based traversal are not meaningful anymore. 

Insertion can be approached from either side; the position of the node will remain the same. However, it can be helpful in heuristic based insertion in long lists, where we try to find which end might require less traversal (based on the data field). But obtaining the tail end is as simple as fake_node->bk.

*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

struct node {
  int data;
  struct node* next;
  struct node* prev;
};
typedef struct node Node;

Node* createNode(int initVal){
  Node* n = malloc(sizeof(Node));
  if (!n) return NULL;

  n->data = initVal;
  n->next = NULL;
  n->prev = NULL;
  return n;
}

int linkNode(Node** ListRef, int initVal){
  Node* new_node = createNode(initVal);
  if (!new_node) return -1;

  Node* fake_node = *ListRef;
  Node* cur_node = fake_node->next;    // Start with the head node.

  while (1){
    if (
      (new_node->data < cur_node->data) ||
      (cur_node == fake_node)
    ){
      new_node->next = cur_node;
      new_node->prev = cur_node->prev;

      (cur_node->prev)->next = new_node;
      cur_node->prev = new_node;

      return 0;
    }
    cur_node = cur_node->next;
  }

  return 1;
}

void unlinkNode(Node* node2remove){
  (node2remove->prev)->next = node2remove->next;
  (node2remove->next)->prev = node2remove->prev;
}

void displayList(Node** ListRef){
  Node* fake_node = *ListRef;
  if (fake_node->next == fake_node) return;

  int i = 0;
  Node* tmp = fake_node->next;
  do {
    printf("Node%d: %d\n", i, tmp->data);
    i++;
    tmp = tmp->next;
  } while(tmp != fake_node);
}

void displayFormattedList(Node** ListRef){
  Node* fake_node = *ListRef;
  if (fake_node->next == fake_node)  return;

  Node* tmp = fake_node->next;
  printf(".. <--> ");
  do {
    printf("(%d) <--> ", tmp->data);
    tmp = tmp->next;
  } while (tmp != fake_node);
  printf("..\n");
}

int initListHeaders(Node** listHdrs, int listCount){
  if (!listHdrs)  return -1;
  for (int i=0; i<listCount; i++){
    Node* fake_node = (Node*)((char*)(&listHdrs[i*2])-8);
    fake_node->next = fake_node;
    fake_node->prev = fake_node;
  }
  return 0;
}

int main(void){
  unsigned long listCount = 10;
  Node* listHeaders[listCount*2];
  initListHeaders(listHeaders, listCount);

  Node* ListRef = (Node*)((char*)(&listHeaders[0])-8);
  linkNode(&ListRef, 5);
  displayFormattedList(&ListRef);
  sleep(1);

  linkNode(&ListRef, -5);
  displayFormattedList(&ListRef);
  sleep(1);

  linkNode(&ListRef, 46);
  displayFormattedList(&ListRef);
  sleep(1);

  linkNode(&ListRef, 0);
  displayFormattedList(&ListRef);
  sleep(1);

  linkNode(&ListRef, -2);
  displayFormattedList(&ListRef);
  sleep(1);

  linkNode(&ListRef, 8);
  displayFormattedList(&ListRef);
  sleep(1);

  linkNode(&ListRef, 5);
  displayFormattedList(&ListRef);
  sleep(1);

  Node* n2r = (ListRef->next)->next;
  unlinkNode(n2r);
  printf("List after unlinking a node:  ");
  displayFormattedList(&ListRef);
  printf("\n");
}
