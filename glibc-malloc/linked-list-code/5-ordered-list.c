/* Implemented ordered list based on the data field. */

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

int pushAtHead(Node** ListRef, int initVal){
  Node* new_node = createNode(initVal);
  if (!new_node) return -1;

  Node* fake_node = *ListRef;
  Node* cur_node = fake_node->next;    // Head node

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

int pushAtTail(Node** ListRef, int initVal){
  Node *new_node = createNode(initVal);
  if (!new_node){ return -1;}

  Node* fake_node = *ListRef;
  Node* cur_node = fake_node->prev;

  while (1){
    if (
      (cur_node == fake_node) ||
      (new_node->data > cur_node->data)
    ){
      new_node->prev = cur_node;
      new_node->next = cur_node->next;
      
      (cur_node->next)->prev = new_node;
      cur_node->next = new_node;

      return 0;
    }
    cur_node = cur_node->prev;
  }

  return 1;
}

int deleteFromHead(Node** ListRef){
  /* Empty list check. */
  Node* fake_node = *ListRef;
  if (fake_node->next == fake_node)  return -1;

  Node* cur_head = fake_node->next;

  // Step1: Modify the prev link of the node next to the current head node
  (cur_head->next)->prev = fake_node;

  // Step2: Modify the fake node's next link.
  fake_node->next = cur_head->next;

  // Step3: Release the current head node.
  free(cur_head);
}

int deleteFromTail(Node** ListRef){
  /* Empty list check. */
  Node* fake_node = *ListRef;
  if (fake_node->next == fake_node)  return -1;

  Node* cur_tail = fake_node->prev;

  // Step1: Modify the fake node's prev link.
  fake_node->prev = cur_tail->prev;

  // Step2: Modify the next link of the node previous to the last node.
  (cur_tail->prev)->next = fake_node;

  // Step3: free(cur_tail)
  free(cur_tail);
}

void displayFromHead(Node** ListRef){
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

void displayFromTail(Node** ListRef){
  Node* fake_node = *ListRef;
  if (fake_node->prev == fake_node)  return;

  Node* tmp = fake_node->prev;
  do {
    printf("NodeValue: %d\n", tmp->data);
    tmp = tmp->prev;
  } while (tmp != fake_node);
}

void displayFormattedFromHead(Node** ListRef){
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

void displayFormattedFromTail(Node** ListRef){
  Node* fake_node = *ListRef;
  if (fake_node->prev == fake_node)  return;

  Node* tmp = fake_node->prev;
  printf(".. <--> ");
  do {
    printf("(%d) <--> ", tmp->data);
    tmp = tmp->prev;
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
  pushAtTail(&ListRef, 5);
  displayFormattedFromHead(&ListRef);
  sleep(1);

  pushAtTail(&ListRef, -5);
  displayFormattedFromHead(&ListRef);
  sleep(1);

  pushAtTail(&ListRef, 46);
  displayFormattedFromHead(&ListRef);
  sleep(1);

  pushAtTail(&ListRef, 0);
  displayFormattedFromHead(&ListRef);
  sleep(1);

  pushAtTail(&ListRef, -2);
  displayFormattedFromHead(&ListRef);
  sleep(1);

  pushAtTail(&ListRef, 8);
  displayFormattedFromHead(&ListRef);
  sleep(1);

  pushAtTail(&ListRef, 5);
  displayFormattedFromHead(&ListRef);
  sleep(1);
}
