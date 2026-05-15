#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

struct node {
  int data;
  struct node* next;
  struct node* prev;
  struct node* skip_next;
  struct node* skip_prev;
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

  int status=0;
  while (1){
    if (
      (new_node->data < cur_node->data) ||
      (cur_node == fake_node)
    ){
      new_node->next = cur_node;
      new_node->prev = cur_node->prev;

      (cur_node->prev)->next = new_node;
      cur_node->prev = new_node;

      status=1;
      break;
    }
    cur_node = cur_node->next;
  }

  if (status){
    status = 0;
    Node* unique_node = fake_node->next;
    Node* tmp = (fake_node->next)->next;

    while (1){
      if (tmp != fake_node){

        if (unique_node->data == tmp->data){
          tmp->skip_next = NULL;
          tmp->skip_prev = NULL;
        }
        else if (unique_node->data != tmp->data){
          unique_node->skip_next = tmp;
          tmp->skip_prev = unique_node;
  
          unique_node = tmp;
        }
      }

      else if (tmp == fake_node){
        unique_node->skip_next = tmp->next;
        (tmp->next)->skip_prev = unique_node;

        status=1;
        break;
      }

      tmp = tmp->next;
    }
  }

  if (status)  return 0;
  return 1;
}

// To be written
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

void displaySkipList(Node** ListRef){
  Node* fake_node = *ListRef;
  if (fake_node == fake_node->next)  return;

  Node* tmp = fake_node->next;
  printf(".. <-> ");

  do {
    printf("(%d) <-> ", tmp->data);
    tmp = tmp->skip_next;
  } while (tmp != fake_node->next);
  printf("..\n");
}

void prettyPrintNode(Node* n2pp){
  printf("\n%p\n", n2pp);
  printf("{\n");
  printf("   .data = %d,\n", n2pp->data);
  printf("   .next = %p,\n", n2pp->next);
  printf("   .prev = %p,\n", n2pp->prev);
  printf("   .skip_next = %p,\n", n2pp->skip_next);
  printf("   .skip_prev = %p,\n", n2pp->skip_prev);
  printf("}\n");
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
  for (int i=0; i<5; i++){
    linkNode(&ListRef, i+5);
    linkNode(&ListRef, i+5);
    linkNode(&ListRef, i+5);
    linkNode(&ListRef, i+5);
    linkNode(&ListRef, i+5);
  }

  printf("Complete list: ");
  displayFormattedList(&ListRef);

  printf("\nSkip list: ");
  displaySkipList(&ListRef);

  printf("\nPretty Print: \n");
  Node* tmp = ListRef->next;
  do {
    prettyPrintNode(tmp);
    tmp = tmp->next;
  } while (tmp != ListRef);
}
