# The Bookkeeping System, Part 0: The Problem

The bookkeeping system sits at the core of glibc-malloc. It is the framework based on which the whole malloc-free pathways are stacked. It is probably the most challenging piece to write.

In the chunk description section, what the author flagged as "misleading, but accurate and necessary" was simply a lack of proper documentation of the design decisions.

The bookkeeping section comes with its own challenges, and those challenges can be easily summed up in one sentence. ***I perceive annotations as the theoretical model of glibc-malloc's functioning, and this model is not aligning with the runtime reality as expressed by the code.***
  1. There are annotations which reflect the 32-bit reality only. The reader is left thinking about the 64-bit reality.
  2. There are annotations which are quite confusing and don't make much sense.
  3. There are two annotations about the same topic which don't converge. *How can something exist in two different states at the same time?* It can only exist in one and we are left finding it out ourselves.
  4. There is an annotation accepting that as times passes, things change. But the problem is, when something is no longer fully correct, or is no longer applicable, it is still not updated.
  5. Many of the annotations are never changed since the time they appeared.

It's not "all bad", but a lot of it is.

---

The situation is quite tiring and it is in the best of our interest to avoid carrying all the three configs together. We will stick to 64-bit and later apply our understanding to the other two configs.

To reduce cognitive load as much as it is possible, I have divided the exploration into three headings that build on each other.
  1. **The implementation of bins data structure**: Here we will explore something similar to `malloc_chunk`, which was not properly documented; "the repositioning trick".
  2. **Static analysis of bins[]**: Here we will read the source (code and annotations) and find all the facts about bins.
  3. **Dynamic analysis of bins[]**: Here we will run some experiments to find the runtime all reality and build the final structure of bins.
  4. Extending our understanding to build the model of the other two configs as well.

---

That's a very big statement from a naive about a project this big. But I request you to continue reading as I will present all the facts I have gathered in this journey in the end and you can decide whether the writer is talking "out of thin air". 

**--- IMPORTANT NOTE ---**
  - I have invested a lot of time, energy and attention on this section.
  - This section has been a great source of frustration and agitation for about 3 months. That's a lot of time, which includes medical issues, environmental issues and psychological issues. So, everything happened all at once.
  - When things calmed down after 3 months, the arrival of fresh insights started reducing my frustration. I am no longer as frustrated and agitated as I was before.
  - I have learned to update my views and beliefs when new information with proper evidence arrives. That's what happened this time as well. Versions before this have a sort of "angry voice" and versions starting from this will have a more "calm voice".
  - If anyone reads the commit history, they are sure to find a lot of things. While I won't say they are "immature", they are just a reaction of my past self, that tried to make sense of this codebase and repeatedly found nothing except confusion.
  - This note is left to acknowledge that.

---

# The Bookkeeping System, Part 1: The Implementation Of Bins

***A bin is a data structure, based on "circular doubly linked list", which is used to manage free chunks. Another name for bins is "free list".***

Conceptually, we have three class of bins: **smallbins** (for small chunks), **largebins** (for large chunks) and a bin to hold chunks temporarily, called **the unsorted bin**. Because these are just different names given to the same data structure, we have two choices in implementing them.
  1. Implement three different variables for each bin type.
  2. Implement one variable that has pointers to all the bins.

Therefore, at the implementation level, we have **only one** data structure, containing pointers to all the bins, i.e. ***an array***.

There are multiple ways to implement this array. To understand which one is the best, we have to understand how a "doubly circular linked list" works.

## Note

Linked list is a fundamental data structure, discussed in every beginners data structure course. So, if you have taken one, it's highly likely you are already familiar with it, but familiarity alone is not enough to understand bins[].

Data structure courses are often structured around object oriented languages like C++/Java. This is what I have seen in the Indian YouTube space. It is different from C's procedural approach.

I have tried to find implementations on the internet, which I can link here to save myself some time and efforts, but I couldn't find a single of them that satisfies my requirements.

For these reasons, I have implemented all the linked lists required to understand `bins[]` myself. They are in the [./linked-lists](./linked-lists/) directory. This is helpful in two ways.
  - Those who feel rusty about their understanding can quickly visit the code to strengthen it. They don't have to waste their time searching one.
  - As a writer, I can be sure that my reader and I have the same base model of the problem. We can, and should, differ in the later ideas, but our foundation is the same.

**Note: Reading them is not a precursor. I have mentioned when it is required to read one.**

Let's start with a single doubly circular linked list.

## Single D.C linked list

A double circular linked list has **head** and **tail** pointers to create circularity. We have two ways to implement it.
  1. Managing the head and tail pointers individually, like this:
     ```c
     int main(void){
       struct Node* head;
       struct Node* tail;
     }
     ```
  2. A distinct struct, like this:
     ```c
     struct List{
       struct* Node head;
       struct* Node tail;
     };
     ```

While method1 is obvious, method2 provides an intuitive abstraction.

In the end, both the methods are identical and based on the tutor's choice, you might have seen both. I have seen both, especially the first one in the cpp space.The [double circular list implementation](./linked-list-code/1-simple-dll.c) is based on method2. Please read it.

## A collection of D.C linked lists

The `bins` data structure is a collection of bins. Regardless of what we have chosen in the previous implementation, we have two options. Either we implement the pointers manually, or we create an array of them. Both the options are demonstrated below.

### Method1: List*
---

Manage multiple `List*` manually.
```c
int main(void){
  struct List* l1;
  struct List* l2;
}
```

Create an array of `List*`.
```c
int main(void){
  unsigned long listCount = 10;
  struct List* lptrs[listCount];
}
```

### Method2: Node*
---

1. Manage the head/tail pointers individually for each list.
```c
int main(void){
  struct Node* l1_head;
  struct Node* l1_tail;
  struct Node* l2_head;
  struct Node* l2_tail;
}
```

2. Create an array of `Node*`.
```c
int main(void){
  unsigned int listCount = 10;
  struct Node* listHeaders[listCount*2];
}
```

---

Managing pointers individually is tiresome and error-prone, while managing an array of pointers is semantically clean and has indexing benefits. We have an array-based implementation for both the options. Please read the [list_ptr-array.c](./linked-list-code/2-list_ptr-array.c) and [node_ptr-array.c](./linked-list-code/3-node_ptr-array.c) implementations.

Now we have two worthy candidates for implementing `bins[]`. glibc-malloc uses the `Node*` implementation. To understand why the `List*` implementation is not used, we have to understand why `Node*` is used.

## The Node* array implementation

So far, we have an array of `Node*` elements, representing the head/tail pointers of lists.
  1. The head/tail pointers point to the first and the last nodes in a list.
  2. The head/tail pointers act as artificial **ends** for a list while the next/prev pointers [per node] create circularity.
  3. An empty list has the head/tail pointers NULL.

The push/delete functions are probably the most important operations. They have to run multiple times. Right now, our push/delete logic is simple, but lacks efficiency.
  - The logic is divided into *single node list* and *multiple nodes list*, and the bottleneck is right at the start of the algorithm.
  - For every list, we have to check if it is a singular list, which is inefficient because, it creates a branch in the happy path. The CPU has no option but to evaluate the special case every single time, making the code inefficient.

We need a solution that makes eliminates this "special casing" and make the the push/delete logic branchless. Let's start thinking.

## How to make the push/delete logic branchless?

The listHeaders in `node_ptr-array.c` are fixed to the head/tail nodes in the list. If the list is empty, they are NULL.

That's how a single list look likes:
```c
head<->node1<->tail
```

When a node is added to it, let's say, on the tail, it becomes:
```c
head<->node1<->node2<->tail
```

When we print the list, we anchor the start/end with the head/tail pointers; we don't rely on the next/prev pointers because, they maintain circularity. When the list is empty, they must be set NULL. This makes the head/tail pointers sentinel beings. They don't directly participate in the push/delete logic but makes it depend on them.

***The solution is about eliminating this "special casing" and make the listHeaders participate in the process completely. They must not be treated specially when the list is empty.*** 

The question is how? ***We have to change how we perceive these listHeaders.***

## Finding the solution

The listHeaders[] is defined as:
```c
struct Node{
  int data;
  struct Node* next;
  struct Node* prev;
}

unsigned long listCount = 10;
struct Node* listHeaders[listCount*2];
```
**Reminder: The struct will have 4 padding bytes after the `data` field to keep alignment.**

We have to find which headers correspond to which list and the calculation depends on how we count the lists.
  - In case of 0-based indexing, list0's headers will be listHeaders[0] and listHeaders[1]; list1's headers will be listHeaders[2] and listHeaders[3]. So, the headers for the nth list will be `listHeaders[i*2]` and `listHeaders[(i*2)+1]`.
  - In case of 1-based indexing, list1's headers will be listHeaders[0] and listHeaders[1]; list2's headers will be listHeaders[2] and listHeaders[3]; So, the headers for the nth list will be `listHeaders[(i-1)*2]` and `listHeaders[((i-1)*2)+1]`.

Either way, the logic is remains the same.

**Note: You don't have to keep the formulas active in your mind. Just be aware of the situation.**

---

If we ask "*what actually participates in the push/delete process*", the answer would be, **nodes**. Does that mean, *to make the headers participate in the process, the headers must be nodes themselves?* Yes. The question is how!

***Moments like these, where you have a rough idea of the outcome you need, and you need to find the process that can lead to it, but you have absolutely no substrate to think upon, one way to deal with this is to ask as many questions as you can, related or unrelated. Eventually, you will find the right one. This is also applicable when the answer to a question is a question itself. Keep asking questions and eventually, the recursion will end.*** So, let's ask some questions.
  - If headers need to be nodes themselves, what are headers right now? They are pointers to nodes, not nodes themselves.
  - If the headers are nodes themselves, *are there distinct nodes for both the headers?*, or, *there is one node that contains both the headers?*
  -  In either of the cases, what the fields of this/these fake nodes will contain? The `data` field will be garbage for obvious reasons. What about the next/prev fields?
  - If we have two fake nodes, the head fake node's next would probably point to the real head node and the tail fake node's prev would point to the real tail node. What happens to the prev and next of the head and the tail fake nodes?
  - Does the head fake node's prev point to the real tail node and the tail fake node's next points to the real head node in the list? If that is true, we will end up with two identical fake nodes. Correct?
  - Does that mean, *we need only one fake node, whose next/prev will point to the real head/tail nodes in the list?* Something like:
    ```c
    ....fake_node<->new_node<->exist_node<->fake_node....
    ```

**Congratulations**. That's the answer. ***We need a fake node whose next/prev point to the head/tail nodes in the list.***

---

Now the rough image of the outcome is crystal clear. Let's think about how we will get to it.

## Implementing the solution

Let's take an example. The numbers represent 64-bit addresses.
```
1000 :: &listHeaders[0]
1008 :: &listHeaders[1]
```

If we need a fake node, such that, it's next/prev align with the addresses that point to the head/tail nodes in a list represented by the above headers, where should the fake node start in the memory? *The answer is 992.*

That means, the fake node for the above list headers can be obtained this way:
```c
// struct Node* fake_node = (Node*) ( (char*)(&listHeaders[0]) -8);
struct Node* fake_node = (Node*)((char*)(&listHeaders[0])-8);

fake_node->next = listHeaders[0];
fake_node->prev = listHeaders[1];
```

To obtain a fake node suitable for the nth list, we can do it this way:
```c
// 0-based indexing
struct Node* fake_node = (Node*)((char*)(&listHeaders[i*2])-8);

// 1-based indexing
struct Node* fake_node = (Node*)((char*)(&listHeaders[(i-1)*2])-8);
```

When the list is empty, the next/prev of the fake_node will simply point to the list header itself, i.e `(Node*)((char*)(&listHeaders[i*2])-8);` or the other one.

## Some concerns

The fake node for the 0th list is at a negative index. How is this legal?

An access is illegal when it doesn't align with the memory protection rights (mprotect). Right now, we are doing this on stack. If we are familiar with how the kernel maps a binary and prepares it for execution, we know that a process's stack is initialized by the kernel with a lot of stuff that comes before the main function. Therefore, we are not accessing a memory we don't own, which is why, we don't get a segfault.

*malloc_state*, which is where the bins[] is allocated, goes on the static storage, and it is not the only thing that goes there and it is not the first thing that goes there. Therefore, we are not touching an address we are not meant to.

However, we have to be cautious about this kind of pointer arithmetic as it touches a piece of memory which might hold crucial information.
  - If the arithmetic failed and we overwrite that memory, we are officially in **the undefined behavior territory**.
  - But we ensure that it doesn't happen by using that pointer only for offsetting to the right headers; we never-ever use that address to write something.

---

To complete your understanding, open the [fake-node-impl.c](./linked-list-code/4-fake-node-impl.c). You might notice it still contains the single vs multiple distinction. It is left to make the leap smooth. Just comment that block and run again. You'll not be surprised that it works. [fake-node-impl(2).c](./linked-list-code/4-fake-node-impl(2).c) contains the final version of this implementation.

---

This is what the author titled as "the repositioning trick". And I am sort of perplexed about the title.
```
  To simplify use in double-linked lists, each bin header acts as 
  a malloc_chunk. This avoids special-casing for headers. But to 
  conserve space and improve locality, we allocate only the fd/bk 
  pointers of bins, and then use "repositioning tricks" to treat 
  these as the fields of a malloc_chunk*.
```

Anyways, if you have understood everything discussed so far but still feel "sort of incomplete", or sensing the absence of a conclusion, that goes like *"and this is how glibc-malloc does it with bins[]...."*, I want to say that "that conclusion is not possible here", for the time being. 

`bin_at` is the macro that operationalize this repositioning trick, which is the conclusion to this heading, but understanding it requires concepts that I have not introduced yet. Therefore, we will explore it later.

---

So to answer how `bins` are implemented, ***they are implemented as an array of bin headers of type mchunkptr (malloc_chunk\*) and "repositioning tricks" are used to find the correct bin.***

Now you know why a `List*` array is not suitable. It creates hurdles in implementing that "repositioning trick". However, a `List` array might make sense because, internally, it is just `Node*` elements. But in this case, it would simply be an abstraction that is no longer required.

# The Bookkeeping System, Part 2: Complete Analysis of bins[]

The source is basically a collection of code and annotations.
  - **Annotations** are comments that never execute. Ideally, they should represent what happens at runtime in "pretty words", i.e. the theoretical model.
  - **Code** is how the theoretical model is implemented, so it should represent the runtime truth.

We will read the code related to bins[], understand it, and form our mental model. Later we will use that information to perform dynamic analysis.

**Note: There are some annotations which indicate multiple things, but to keep things manageable, we will repeat those annotations instead of reading them all at once.**

## Bin counts

### Total number of bins

The total number of bins is 128, as per this annotation and macro definition.
```c
/* There are a lot of these bins (128). */

#define  NBINS  128
```

This is how bins[] is declared in `struct malloc_state`.
```c
typedef struct malloc_chunk* mchunkptr

mchunkptr bins[NBINS * 2 - 2];
```

This is not a very acceptable way of writing code to me. I have to exercise my knowledge of operator precedence when it could have been avoided. I know the expression is not very complex, but that should not be used as a proxy. Anyways, this is how the expression would evaluate:
```
=> (NBINS*2)-2
=> (128*2)-2
=> 256-2
=> 254
=> (NBINS-1)*2
```

So, the number of bins as per the actual declaration of bins[] is 127.
```c
mchunkptr bins[(NBINS-1)*2];
// mchunkptr bins[127*2];
```

---

The NBINS definition says there are 128 bins. But the declaration of the bins[] reserve space for only 127 bins.

NBINS is just a macro, which cease to exist after preprocessing. Therefore, 127 is the real number of bins. Then why the annotation said, "there are 128 bins"? *That's what I meant, when I said that, "the annotation and the implementation are not converging".*

We will consider 127 as the total number of bins.

### Number of smallbins

As per this macro, there are 64 smallbins.
```c
#define  NSMALLBINS  64
```

### Number of largebins

There is no annotation or macro that confirms the count of largebins directly. However, there is an annotation having bins in a pyramid like structure.
```
    64 bins of size          8
    32 bins of size         64
    16 bins of size        512
     8 bins of size       4096
     4 bins of size      32768
     2 bins of size     262144
     1 bin  of size    what's left
```

Since there are 64 smallbins, the bins in the first row must be smallbins and the rest must be largebins, making the number of largebins (32+16+8+4+2+1), i.e. 63.

64 smallbins and 63 largebins makes the total bin count 127, making this annotation converge with the implementation, but it diverges from the previous annotation that said, "there are 128 bins". *That's what I meant, when I said that, two annotations about the same topic aren't converging*.

### The unsorted bin

This annotation confirms that there is only one unsorted bin, although it doesn't use the word "bin" directly.
```
The otherwise unindexable 1-bin is used to hold unsorted chunks.
```

64 smallbins, 63 largebins and 1 unsorted bin, that is 128, but the number of bins is 127. The answer to this confusion is in the annotation itself, but before we access it, we have to find the order of bins inside the array.

### The order of bins within bins[]

There is no annotation that directly confirms the order of bins. Logically, size grows from 0. So small sizes come first, followed by large sizes. That means, smallbins come first, followed by largebins. Also, notice the bin pyramid has 64 bins on the top. Since the sizes are increasing top-to-bottom, it is highly likely that our assumption is correct.

Let's reread that unsorted bin annotation. The `1` in "1-bin" is likely the number of a bin. This number can have two interpretations.
  1. In 0-based indexing, 1-bin is the bin at number 1 in the collection.
  2. In 1-based indexing, 1-bin is the first bin in the collection of bins.

**Note: I am using the term "collection of bins" as bins are implemented as an array of headers and indexing in traditional sense is not applicable here, unless we keep it sloppy. It is explored later in the "bin indexing" section**.

In either case, 1-bin would be a smallbin. If this bin is the unsorted bin, the count of smallbins is reduced to 63.

---

Have a look at this annotation.
```
Bin 0 does not exist. Bin 1 is the unordered list; if that 
would be a valid chunk size the small bins are bumped up one.
```
  - "List" and "bin" are synonymous terms.

The use of "Bin 0" and "Bin 1" terminology strongly indicates the use of 0-based indexing.

"bin 0 doesn't exist" can have two interpretations.
  - Bin 0 doesn't exist **literally**. Or,
  - Bin 0 exist, but is of no function.

If bin 0 doesn't exist literally, that further reduces the count of smallbins to 62 and the total bin count to 126. Based on this information, the order of bins should be:
```
[unsorted_bin, smallbins, largebins]
      1           62         63
```

This line in the annotation: *"if that would be a valid chunk size the small bins are bumped up one."* is honestly not making sense at all.
  - Is the design non-deterministic?
  - It reads like there is a lack clarity about the size of 1-bin, which is why the annotation is asking the validity of that size as a smallbin class. We will look into this later.

### The Findings

1. **Annotation #1**: "there are 128 bins".
2. **Macro NBINS**: "there are 128 bins".
3. **The Implementation of bins[]** reserved space for "127 bins".
4. **Annotation #2**: A bin pyramid which lists some sort of "a class of bins", mentions 127 bins.
5. **Annotation #3**: "Bin 0 doesn't exist", reduces the count of smallbins to 63 and the total count of bins to 126, considering the implementation as the truth.
6. **Annotation #4 and #5**: "Bin 1 is the unordered list" and "The otherwise unindexable 1-bin is used to hold the unsorted chunks" reduces the count of smallbins to 62.

### The Questions

The analysis above raises some questions.
  1. Why the count of bins is 128, when the implementation reserved space for 127 bins only?
  2. Why bin 0 doesn't exist? If "bin 0" was never meant to exist, why the bin count is not 126, instead of 127?
  3. Why bin 1, which is supposed to be a smallbin is [repurposed as] an unsorted bin?

## The order of chunks within each bin type

These annotations explain the ordering of chunks within bins.
```
Chunks in bins are kept in size order, with ties going to the
approximately least recently used chunk.
- Ordering isn't needed for the small bins, which all contain 
  the same-sized chunks, but facilitates best-fit allocation 
  for larger chunks.
- These lists are just sequential. Keeping them in order almost 
  never requires enough traversal to warrant using fancier 
  ordered data structures.
```
  - What does "least recently used" mean?

A smallbin manages free chunks of a specific size class. Therefore, it requires no ordering.

The unsorted bin is like a resting ground, where the recently freed chunks are given a chance to be reused by the next malloc. There is no need for order here.

A largebin manages chunks in a range of bytes. There are two types of linkages b/w these chunks.
  1. The fd/bk fields maintains links based on size, which means, largebins are ordered by size.
  2. The fd_nextsize/bk_nextsize maintains a skip list. We will talk about this later.

## Smallbin Size Classes

A smallbin manages free chunks matching a specific size class.

The macro SMALLBIN_WIDTH defines the difference between two size classes.
```c
#define  SMALLBIN_WIDTH  MALLOC_ALIGNMENT
```
  - MALLOC_ALIGNMENT is (2*SIZE_SZ) in an architecture. For 64-bit, it is 16 bytes.
  - Therefore, the difference between two size classes is 16 bytes on 64-bit.

We add SMALLBIN_WIDTH to an existing size class to obtain the next one. To obtain the i<sup>th</sup> size class, we can use this formula: `(SMALLBIN_WIDTH*i)`.

Now we need to find the bounds for the value of `i`. We are already familiar with the order of bins.
  - We have "64 smallbins", where the starting bin doesn't exist and the next bin is repurposed as the unsorted bin.
  - The indexing is 0-based, so the theoretical bounds would be [0, 63]. We can exclude the first two bins to obtain the bounds as per our analysis, i.e. [2, 63].

The source also has an annotation and a macro regarding this.
```c
#define  SMALLBIN_CORRECTION  (MALLOC_ALIGNMENT > CHUNK_HDR_SZ)

#define  MIN_LARGE_SIZE  ((NSMALLBINS - SMALLBIN_CORRECTION) * SMALLBIN_WIDTH)

// ----------

/* Bins for (sizes < 512 bytes) contain chunks of all
the same size, spaced 8 bytes apart. Larger bins
are approximately logarithmically spaced. */
```

SMALLBIN_CORRECTION is the correction factor, used in config #3 to adjust the calculation. We will discuss it later. On 32-bit and 64-bit, it is 0 and has no effects.

MIN_LARGE_SIZE is the size of the first largebin.
  - On 32-bit, it is (64-0)*8, i.e. 512 bytes. This explains that the annotation is applicable to 32-bit only. Also, the 8 bytes spacing is what SMALLBIN_WIDTH is on 32-bit.
  - On 64-bit, it is (64-0)*16, i.e. 1024 bytes.

Using MIN_LARGE_SIZE, we can find the last smallbin size class. It is, `MIN_LARGE_SIZE-SMALLBIN_WIDTH`.
  - On 32-bit, it is (512-8), i.e. 504 bytes.
  - On 64-bit, it is (1024-16), i.e 1008 bytes.

Based on MIN_LARGE_SIZE, we can say that the annotation is only applicable to 32-bit.

---

Now we have:
  1. the total number of smallbins,
  2. the bounds of `i`,
  3. the last smallbin size class, and
  4. the width of smallbins on 64-bit.

This information is enough to build all the size classes by crawling backwards. Here is a tiny python script.
```py
x = 1024
for i in range(64):
  x = x-16
  print(x, end=", ")
```

The output is definitely "not astonishing" at all.
```
1008, 992, 976, 960, 944, 928, 912, 896, 880, 864, 848, 832, 816, 800, 784, 768, 752, 736, 720, 704, 688, 672, 656, 640, 624, 608, 592, 576, 560, 544, 528, 512, 496, 480, 464, 448, 432, 416, 400, 384, 368, 352, 336, 320, 304, 288, 272, 256, 240, 224, 208, 192, 176, 160, 144, 128, 112, 96, 80, 64, 48, 32,
```

These are the size classes corresponding to 62 smallbins. If there were 64 smallbins, starting from 0, that would include the size classes 16 bytes and 0 bytes, for "bin 1" and "bin 0", respectively.

While a chunk of zero bytes represents nothing, a request of such size is not impossible. It depends on how the allocator deals with it.
  - If the allocator chose to raise an error or simply refuse the request, the allocator has to deal with it separately. That means, an extra branch.
  - If the allocator doesn't want special-casing, the request must be converted to a form which is acceptable to the allocator. That's what request2size(sz) does. Therefore, a request of 0 bytes would be simply rounded up to MINSIZE.
  - Same thing happens with 16 bytes on 64-bit or 8 bytes on 32-bit.

In either of the cases, the resulting chunk will no longer be a fit for "bin 0" or "bin 1". So, there is no utility of these bins.

An allocator is a kind of program which has to be mandatorily memory-efficient. By not having "bin 0", we save (SIZE_SZ\*2) bytes and by repurposing "bin 1" as the unsorted bin, we utilize the remaining (SIZE_SZ\*2) bytes.

---

That resolves the mystery of "bin 0" and "bin 1". What is not resolved is why NBINS is 128, then bins[] reserve space for 127 chunks and after all this, we are still short on one bin. The claim that "there are 64 smallbins" is also misleading. 

Let's revisit the unsorted bin annotation: *"if that would be a valid chunk size the small bins are bumped up one."* annotation.
  - What's the point of it?
  - The size classes can be easily obtained. The math is not complicated at all. I can't understand why it was written in the first place.

---

This whole thing can be approached in one another way.
  - The current narrative crawls backward and then asks what would happen if there were 64 smallbins instead. **We have simply not asked what is the starting point**. The answer is a single word that explains everything, i.e. **MINSIZE**.
  - We don't even have to answer what is MIN_LARGE_SIZE. We would naturally find our way to it. *"The last smallbin class on 64-bit is for 1008 bytes, which covers 993-1008 bytes. So, the first largebin should start from 1008+16, i.e. 1024 bytes"*. Isn't it?
  - We would have understood the "bin 0; bin 1" situation before listing out the size classes and that act would simply conclude this section. Maybe, that's a better ending. *That's a problem I face quite-often but have rarely named it.*
  - The more I drill down, the more I find better ways to understand the thing and explain it. In 90% of the cases, what you are reading is not a first-hand account of "how I understood it".
  - The way I have actually understood these concepts is far more raw and brutal, which never gets captured. I always start by writing the raw version, but it always undergoes multiple rewrites before I am sure about it.
  - The remaining 10% attributes to the scale of this writing. At this point, I simply can't keep track of things, which is another reason why the commit history is basically a lot of rewrites, where new stuff keeps coming, rather than coming individually with an announcement, though I try to do that.
  - And rewrites are tiring. After a long time do I reach the point where a rewrite actually reduces the word count, rather than increasing it.

---

To summarize, there a 62 smallbins, with size classes belong to: `[MINSIZE, MIN_LARGE_SIZE)`, with a step of SMALLBIN_WIDTH.

Let's talk about spacing in largebins.

## Largebin Size Ranges, Part 1

Have a look at this pyramid.
```
    64 bins of size          8
    32 bins of size         64
    16 bins of size        512
     8 bins of size       4096
     4 bins of size      32768
     2 bins of size     262144
     1 bin  of size    what's left
```

We have recently explored smallbin spacing. Keeping the 64/62 smallbins argument aside, ***are there 64 bins of 8 bytes, or 64 bins of size (SMALLBIN_WIDTH\*i) bytes, where i belongs to [0 or 2, 63] ?*** It is not "*n* bins of size *x* bytes". It is "*n* bins of size SMALLBIN_WIDTH\*i bytes, with a step of SMALLBIN_WIDTH bytes".

---

**Note1: This pyramid is applicable to 32-bit only as "the size of 64 bins" is 8 bytes, which is what SMALLBIN_WIDTH is on 32-bit. So, we have no choice but to think in terms of 32-bit. And I hope that it is not an issue as we have internalized the size model already.**

**Note2: The largebins are expressed in 6 rows. While the source doesn't seem to be using the term "largebin category", we will use it. Otherwise, there is no easy to point to them.**

---

If we combine our understanding of MALLOC_ALIGNMENT, request2size(sz) and smallbins, we can say that *the smallest size difference that is possible between two contiguous chunks is MALLOC_ALIGNMENT (or SMALLBIN_WIDTH) bytes.* We can not go smaller than that. This implies that the output of request2size(sz) is always a multiple of SMALLBIN_WIDTH.

In case of smallbins, one bin maps to one size class.

Largebins are said to operate on a range of size. What does that imply? What chunk size can enter that bin?

The last smallbin size class is for (MIN_LARGE_SIZE-SMALLBIN_WIDTH) bytes. On 32-bit, it is 504. The pyramid says that the largebins in category #1 have a width of 64 bytes. Again, that is kind of sloppy. What it really means is that, "a largebin in category #1 spans across 64 bytes starting from the base size."
  - For the first largebin in category #1, the base is immediately after (MIN_LARGE_SIZE-SMALLBIN_WIDTH) bytes, i.e. 505 bytes, as the last smallbin only manages chunks of size 504 bytes.
  - Add 64 to 505, we get 569, which is the base of the next largebin in this category. The range of this largebin comes out to be [509, 568] bytes.
  - What chunk sizes can this bin contain? Can it have a chunk of size 510 bytes? 514 bytes? Obviously not. It can only have a chunk of size which is a multiple of SMALLBIN_WIDTH, i.e. 8 bytes.
  - In the range of [509, 568], we have 8 multiples of SMALLBIN_WIDTH. They are: [512, 520, 528, 536, 544, 552, 560, 568].
  - Isn't this is what a size class means?

***Therefore, a largebin is basically a collection of smallbins.***

To find the number of fixed classes in a category, just divide the bin width by SMALLBIN_WIDTH. For example: On 32-bit,
  - smallbins have a width of 8 bytes and SMALLBIN_WIDTH is 8 bytes too. We get 1, which is correct as one smallbin corresponds to one size class only.
  - largebin category #1 has a width of 64 bytes. Divide it by 8, we get 8, which is correct, as we have seen above.

### Largebin Categories
---

There are 6 categories of largebins, each having a specific number of bins, with a bin width.

If we focus on the bin widths, we can notice that they are all power-of-2 values.

| Bin Width (in bytes) | Bin Width (pow-of-2) |
| :------------------- | :------------------- |
| 8                    | 2<sup>3</sup>        |
| 64                   | 2<sup>6</sup>        |
| 512                  | 2<sup>9</sup>        |
| 4096                 | 2<sup>12</sup>       |
| 32768                | 2<sup>15</sup>       |
| 262144               | 2<sup>18</sup>       |
| what's left          | ?                    |

---

This annotations is about how largebins are spaced.
```
Larger bins are approximately logarithmically spaced.
```

Focus on the table above. We can notice two things.
  1. Across classifications, bin width scale by 3 bits.
  2. Within a classification, bin width remains consistent.

Now read the annotation. It says that, "it is the largebins which are log-spaced", which doesn't seem to be the case. If "each bin was log-spaced", we would obtain an entirely different set of bins.
  1. In a single classifications, bins are linearly spaced with a fixed bin width.
  2. Across classifications, bin width itself scales by 3 bits, which might be what log-spacing is, according to the annotation.

The annotation also mentions the word "approximately". The calculation itself looks quite-precise, so what is "approximate" here?

### The Approximation
---

The bin width is scaling by 3 bits across each classification. It is not arbitrarily chosen. It is what SMALLBIN_WIDTH is on 32-bit.

The pyramid is applicable to 32-bit only. So, bin width scaling by 3 bits is applicable to 32-bit only. What about 64-bit? If we apply the same to 64-bit, bin width should scale by 4 bits across each classification.

If bin width scaled by 4 bits on 64-bit, we will get these set of widths: [2<sup>4</sup> (16), 2<sup>8</sup> (256), 2<sup>12</sup> (4096), 2<sup>16</sup> (65536), 2<sup>20</sup> (1,048,576: 1 MiB), and 2<sup>24</sup> (16,777,216: 16 MiB)] and the "what's left" bin.

Now look at this annotation.
```
The bins top out around 1MB because we expect to service
large requests via mmap.
```
  - First of all, it should be 1 MiB, not 1 MB. That's what the **IEC 80000-13 standard** says is an appropriate unit of measurement in context of memory, as computers work in binary number system.
  - Second, there is no clarity about whether it is applicable to 32-bit only or 64-bit as well. And there is no "easy way" to find that.

Anyways, it acknowledges a design decision that, ***we wish to service "large" requests via mmap.***

Now look at this annotation:
```
// XXX It remains to be seen whether it is good to keep the widths of
// XXX the buckets the same or whether it should be scaled by a factor
// XXX of two as well.
```
  - **Note: I have assumed XXX is a placeholder for something, but that's not the case. I wonder why it was required in the first place. No other annotation has it. Is it a part of some annotation-style guide? I don't know.**
  - "Bucket" seems to be synonymous with bins.

This annotation is questioning whether bin width should scale with the architecture. If bin width scaled with the architecture, the second largest bin would have a width of 16 MiB. In this case, our definition of "large" would change too.
  - I am not a CS major, so I don't really understand how these systems are constructed. There is a lot to it.
  - But benchmarking is possibly one of the things where decisions are tested out. So, if bin width scaled with the architecture, is it proven helpful in benchmarking? There is no mention of that, and benchmarking is beyond the scope of this exploration, so I can't answer this question.

---

If bin width didn't scaled with the architecture, what set of widths we will get?
  - Smallbins would be 16 bytes wide from the base size. That's fixed.
  - The largebins would start from 2<sup>6</sup>, instead of 2<sup>8</sup>.

In this way, we would get a slightly degenerated order of widths for 64-bit: [2<sup>4</sup>, 2<sup>6</sup>, 2<sup>9</sup>, 2<sup>12</sup>, 2<sup>15</sup>, 2<sup>18</sup>], and the "what's left" bin. But in this setting, the spacing between the first largebin category and the smallbins is broken. That is one aspect of the "approximately logarithmically spaced" statement. There is another aspect to it, which we will explore later.

---

The author acknowledged the tension, which is a great thing. But the author forgot to provide clarity on what decision they have eventually made.
  - Obviously, we can figure that out by reading the code. But the question is, what does the author, the management team, the contributors think is valid to be acknowledged and what's not.
  - If the bin width scaled with the architecture, you could have mentioned it directly, without saying, "it remains to be seen".
  - If the bin width didn't scaled with architecture, you could simply say, "the bin widths remain the same on both the architectures, which makes the log-spacing argument slightly-off on 64-bit".
  - But, what was chosen is to hang the reader in the middle. "They will find it themselves."
  - So, the very existence of this annotation is perplexing to me.

To find whether the bin width scales with the architecture, we have to look at the implementation, which we will do after understanding how bin indexing works. This is where we will close a long opened thread: "*how the fake node implementation is operationalized*".

## Bin Indexing

We know that bins are implemented using circular doubly linked lists, and to ensure efficient operations, a fake chunk based implementation is used. As a result, the meaning of bin indexing is slightly different here.

***Bin indexing is the process of mapping the correct bin for a size and calculating an address that can represent a fake malloc_chunk whose fd/bk overlap with the bin headers for the correct bin.***

These macros are responsible for streamlining that process.
```c
#define bin_index(sz)
#define in_smallbin_range(sz)
#define smallbin_index(sz)
#define largebin_index(sz)
#define largebin_index_32(sz)
#define largebin_index_32_big(sz)
#define largebin_index_64(sz)
#define bin_at(M, i)
```

### Macro #1: bin_index(sz)
---

It is a high level handler that takes a size and calls the appropriate bin handler.
```c
#define bin_index(sz)    ( \
  in_smallbin_range(sz)    \
  ? smallbin_index(sz)     \
  : largebin_index(sz)     \
)
```

The unsorted bin has a dedicated macro.
```c
#define  unsorted_chunks(M)  bin_at(M, 1)
```

### Macro #2: in_smallbin_range(sz)
---

It checks whether the size is a valid smallbin size class. Based on the result, the size is passed to the appropriate bin handler.
```c
#define in_smallbin_range(sz)    ( \
  (unsigned long)(sz) < (unsigned long)(MIN_LARGE_SIZE) \
)
```

### Macro #3: smallbin_index(sz)
---

It is the handler for smallbins. It calculates the "index" of the bin corresponding to the size.
```c
#define smallbin_index(sz)    ( \
  ( \
    (SMALLBIN_WIDTH == 16)     \
    ? (((unsigned)(sz)) >> 4)  \
    : (((unsigned)(sz)) >> 3)  \
  ) + SMALLBIN_CORRECTION      \
)
```

The formula to obtain a smallbin size class is: `(SMALLBIN_WIDTH\*i)`, where *i* belongs to [2, 63]. To obtain the index corresponding to a bin, we just have to invert the process, i.e. *divide a size class by SMALLBIN_WIDTH.* We can simplify the macro as:
```c
#define  smallbin_index(sz)  ((sz/SMALLBIN_WIDTH) + SMALLBIN_CORRECTION)
```
.... and the compiler will generate identical assembly for both at -O1 or -O2 as they are fundamentally the same thing. The reason the former exist can be attributed to compiler limitations as discussed in the "preprocessing vs inlining" argument earlier.

---

**Note1:** SMALLBIN_CORRECTION is the macro that adjusts the calculation for config #3, without an extra branching. We will explore it in the end. It has no effects on 32-bit and 64-bit calculation.

**Note2:** The simplification of smallbin_index(sz) is correct, but with a caveat. That caveat can't be understood yet, but keep that in mind.

### Macro #4: largebin_index(sz)
---

It is a high level handler for the largebin index calculation. It calls the appropriate handler after evaluating the config#. This is different from smallbins, where one single macro was enough.
```c
#define largebin_index(sz)    (  \
  (SIZE_SZ == 8)                 \
  ? largebin_index_64(sz)        \
  : (MALLOC_ALIGNMENT == 16)     \
    ? largebin_index_32_big(sz)  \
    : largebin_index_32(sz)      \
)
```

SIZE_SZ=8 is for 64-bit, MALLOC_ALIGNMENT=16 catches the INTERNAL_SIZE_T=4 case (config #3) and the remaining one is for 32-bit. Before we start with `largebin_index_*` macros, we have to discuss another issue.

### The Naming Issue
---

***Note: This heading is chosen because, we already understand "the bin indexing process". It is purely about pointing an issue out.***

***An index implies a value which can be subscripted (or indexed) in an array to find a specific element.*** Array subscripting itself is a syntactic sugar built over: `(base + i*scale)`, where `i*scale` is often called `offset`. In most simplest terms, we have a base address, and we add offset bytes to go at a different address.

Now take this example:
```c
int arr[100];
// considering the width of int 4.
```

Suppose i=15, take these two cases and answer what is the index here.
  1. arr[i]
  2. arr[(i\*4) + 4]

I hope the answers is 15 and 64.
  - arr[5] is (arr + 5*4).
  - arr[(i\*4) + 4] is (arr + ( (i\*4)+4 )*4)

We know this already. We exercise this knowledge all the time. The only reason I took this example is because, it seems like the the author doesn't see it that way.

Look at what smallbin_index is generating for the 1008 bytes size class on 64-bit: `(1008 >> 4) -> 63`.
  - Since bins are implemented as headers, the output has to undergo a calculation to access the correct headers, and the output of smallbin_index participates in that calculation. How can we treat it as **the index**?
  - It is perfectly comparable to example 2 above, where `i` participates in the index calculation process, it is not the index.

---

These are the formulas we have constructed in part 1.
```c
// 0-based indexing
struct Node* fake_node = (Node*)((char*)(&listHeaders[i*2])-8);

// 1-based indexing
struct Node* fake_node = (Node*)((char*)(&listHeaders[(i-1)*2])-8);
```

The equivalent of these formulas is:
```c
typedef struct malloc_chunk *mbinptr;

#define  bin_at(m, i)    (mbinptr) ( \
  ((char*) &( (m)->bins[(i-1)*2] )) - offsetof(struct malloc_chunk, fd)  \
)
```
  - In our example, `8` was the amount of bytes we have to offset back to obtain the correct fake node address for any list. Here it is `2*SIZE_SZ` bytes.
  - We typecast the calculation to a (Node*) and here it is typecast-ed to a (malloc_chunk*).
  - The formula is exactly the same. Keep indexing aside for the time being.

You might still be unconvinced about the issue, and it's not a problem. Let's identify the problem from a different standpoint.
  - Bins are implement using circular doubly linked lists.
  - 2 pointers are required per bin.
  - These pointers have to be together in the array, i.e. [bin0_head, bin0_tail, bin1_head, bin2_tail, ....].
  - To access the headers for the 63rd bin, we have to multiply by 2. But smallbin_index(1008) says that the index for this bin is 63.
  - bins[] is declared with a size of 254. Will we ever be able to access elements after bins[126]? Because, technically, the maximum value the `largebin_index*` macros will generate is 126. Why would bins[] be declared with a size of 254 then?
  - Ask yourself these questions. You are well equipped to answer them.

---

And that's the naming issue is about.
  - The `bin_index` macros are implying to generate a value which is an index. But the macro which calculates the right address of the fake chunk is using the output of bin_index to compute the final index, which when subscripted gives the address which when typecast-ed to a (malloc_chunk*) has its fd/bk overlap with the correct bin headers.
  - Therefore, "bin_number" is a more accurate representation of what these macros are generating.

For some people, it might be a nudge, or nitpicking, and I will not argue with them. Everyone is allowed to think and perceive differently, if that helps.

### Macro #5: bin_at(m, i)
---

`bin_at(m, i)` is the macro that operationalizes the idea of "a fake chunk". It receives "a bin number" corresponding to a size, calculates the address of the fake chunk and typecasts it to a (malloc_chunk*).
```c
#define  bin_at(m, i)    (mbinptr) ( \
  ((char*) &( (m)->bins[(i-1)*2] )) - offsetof(struct malloc_chunk, fd)  \
)
```
  - `m` is the global malloc_state instance.
  - `i` is the output of the bin_index macros.

Once again, these are the formulas we have constructed earlier.
```c
// 0-based indexing
struct Node* fake_node = (Node*)((char*)(&listHeaders[i*2])-8);

// 1-based indexing
struct Node* fake_node = (Node*)((char*)(&listHeaders[(i-1)*2])-8);
```

The use of "bin 0; bin 1" terminology already indicates that the implementation uses 0-based indexing. But when we look at the formula used by bin_at, it aligns with 1-based indexing.

Again, there is no clarity in the source, so we have to find it ourselves.

---

As per our analysis, the structure of `bins[]` is:
```
[unsorted_bin, smallbins, largebins]
      1           62         63
```

In terms of bin headers, `bins[]` would be:
```
[unsb_head, unsb_tail, sbin_MINS_head, sbin_MINS_tail, ...., sbin_before_MIN_LS_head, sbin_before_MIN_LS_tail, largebins....]
```

To access the unsorted bin, what should be the address of the fake chunk?
  - We need the fd/bk of the fake chunk to overlap with unsb_head/unsb_tail. In malloc_chunk, 2 SIZE_SZ fields come before fd/bk.
  - That means, the address represented by `&(unsb_head) - 2*SIZE_SZ` is where the fake chunk should be. How we obtain that address entirely depends on the indexing paradigm in use.

Before we do the step-by-step calculation for both the paradigms, here is a simple question. What's the way to do 1-based indexing in languages where the default is 0-based? Here is an example:
```c
int arr[10];

// 0-based
for (int i=0; i<10; i++){
  printf("arr[%d]: %d\n", i, arr[i]);
}

// 1-based
for (int i=1; i<=10; i++){
  printf("arr[%d]: %d\n", i, arr[(i-1)]);
}
```

Can you spot what we have actually done here? Most of us understand it, but can't articulate it. We have kept the frontend at 1-based indexing and the backend at 0-based. The compiler optimizes the second loop to the first loop to ensure efficient access.

But this assumes that the value of `i` needs a "subtraction by 1". What I am trying to say is that, *an arbitrary value i, where i!=0, can be interpreted as a valid index in either of the indexing systems.

Let's look at the output of smallbin_index(sz) for some sizes on 64-bit.
  1. smallbin_index(32): `(32 >> 4) -> 2`. This implies that, the first smallbin, in the whole collection of bins, is at number 2. 
  2. smallbin_index(16): `(16 >> 4) -> 1`. This implies that, the unsorted bin is at number 1.
  3. smallbin_index(0): `(0 >> 4) -> 0` which is "bin 0", is at number 0.

According to this, the bin indexing macros generate a number which follows the 0-based indexing paradigm. But since "bin 0 doesn't exist", that effectively makes the indexing 1-based. So, to conclude, ***the output of bin index conforms to 0-based indexing, but the way bins are implemented favors 1-based indexing.***

I think that the confusion of "which indexing paradigm is in use" is probably clear now. You might say that "*it stands on the foundation of many if-s being true, and I won't deny that*". Let's build the address of the fake chunk for the unsorted bin.

  - **Step 1**: Obtain the address of the unsorted bin.
    ```c
    // 0-based indexing
    &(bins[0])

    // 1-based indexing
    &(bins[ (1-1) ])
    ```

  - **Step 2**: Subtract (2\*SIZE_SZ) bytes.
    ```c
    // 0-based indexing
    (char*)(&(bins[0])) - offsetof(struct malloc_chunk, fd)

    // 1-based indexing
    (char*)(&(bins[ (1-1) ])) - offsetof(struct malloc_chunk, fd)
    ```

  - **Step 3**: Typecast the address to a (malloc_chunk*).
    ```c
    // 0-based indexing
    mchunkptr fake_chunk = (malloc_chunk*) ((char*)(&(bins[0])) - offsetof(struct malloc_chunk, fd))

    // 1-based indexing
    mchunkptr fake_chunk = (malloc_chunk*) ((char*)(&(bins[ (1-1) ])) - offsetof(struct malloc_chunk, fd))
    ```

  - **Step 4**: Generalize for any bin.
    ```c
    // 0-based indexing
    mchunkptr fake_chunk = (malloc_chunk*) ((char*)(&(bins[i])) - offsetof(struct malloc_chunk, fd))

    // 1-based indexing
    mchunkptr fake_chunk = (malloc_chunk*) ((char*)(&(bins[ (i-1) ])) - offsetof(struct malloc_chunk, fd))
    ```

But, it doesn't align with the one that bin_at uses. That's because, we have forgot to account for bin headers. Now it should be correct.
```c
// 0-based indexing
mchunkptr fake_chunk = (malloc_chunk*) ((char*)(&(bins[i*2])) - offsetof(struct malloc_chunk, fd))

// 1-based indexing
mchunkptr fake_chunk = (malloc_chunk*) ((char*)(&(bins[ (i-1)*2 ])) - offsetof(struct malloc_chunk, fd))
```

This solves the mystery of the indexing paradigm in use and how the fake node implementation is operationalized. Now let's start understanding `largebin_index*` macros.

## Largebin Size Ranges, Part 2

These are the largebin_index macros. We don't have to look at the config #3 macro yet.
```c
#define largebin_index_32(sz)  ( \
  (((unsigned long)(sz) >>  6) <= 38) ?  56 + ((unsigned long)(sz) >>  6) : \
  (((unsigned long)(sz) >>  9) <= 20) ?  91 + ((unsigned long)(sz) >>  9) : \
  (((unsigned long)(sz) >> 12) <= 10) ? 110 + ((unsigned long)(sz) >> 12) : \
  (((unsigned long)(sz) >> 15) <=  4) ? 119 + ((unsigned long)(sz) >> 15) : \
  (((unsigned long)(sz) >> 18) <=  2) ? 124 + ((unsigned long)(sz) >> 18) : \
  126 \
)

#define largebin_index_64(sz)   ( \
  (((unsigned long)(sz) >>  6) <= 48) ?  48 + ((unsigned long)(sz) >>  6) : \
  (((unsigned long)(sz) >>  9) <= 20) ?  91 + ((unsigned long)(sz) >>  9) : \
  (((unsigned long)(sz) >> 12) <= 10) ? 110 + ((unsigned long)(sz) >> 12) : \
  (((unsigned long)(sz) >> 15) <=  4) ? 119 + ((unsigned long)(sz) >> 15) : \
  (((unsigned long)(sz) >> 18) <=  2) ? 124 + ((unsigned long)(sz) >> 18) : \
  126 \
)
```

Bitwise right shift is a very specific case of division.
  1. The denominator is a fixed power-of-2 value.
  2. The decimal part is always lost and the output is an integer, i.e. *floor division*.

We can rewrite these macros as:
```c
#define largebin_index_32(sz)  ( \
  (((unsigned long)(sz) / pow(2,  6)) <= 38) ?  56 + ((unsigned long)(sz) / pow(2,  6)) : \
  (((unsigned long)(sz) / pow(2,  9)) <= 20) ?  91 + ((unsigned long)(sz) / pow(2,  9)) : \
  (((unsigned long)(sz) / pow(2, 12)) <= 10) ? 110 + ((unsigned long)(sz) / pow(2, 12)) : \
  (((unsigned long)(sz) / pow(2, 15)) <=  4) ? 119 + ((unsigned long)(sz) / pow(2, 15)) : \
  (((unsigned long)(sz) / pow(2, 18)) <=  2) ? 124 + ((unsigned long)(sz) / pow(2, 18)) : \
  126 \
)

#define largebin_index_64(sz)   ( \
  (((unsigned long)(sz) / pow(2,  6)) <= 48) ?  48 + ((unsigned long)(sz) / pow(2,  6)) : \
  (((unsigned long)(sz) / pow(2,  9)) <= 20) ?  91 + ((unsigned long)(sz) / pow(2,  9)) : \
  (((unsigned long)(sz) / pow(2, 12)) <= 10) ? 110 + ((unsigned long)(sz) / pow(2, 12)) : \
  (((unsigned long)(sz) / pow(2, 15)) <=  4) ? 119 + ((unsigned long)(sz) / pow(2, 15)) : \
  (((unsigned long)(sz) / pow(2, 18)) <=  2) ? 124 + ((unsigned long)(sz) / pow(2, 18)) : \
  126 \
)
```

Each row in the macro represents a condition that evaluates the bins in that category. We divide the size with the bin width in that category.
  - If the output is `<=` an integer, we add a value to the output and it returned to the caller.
  - If the output is `>` the integer in the condition, the condition is evaluated as false and we move to the next condition in the macro. It is repeated until a condition is met. Otherwise, it is the last bin, the "what's left" bin.

According to these macros, the last bin in the collection is the 126th bin. This proves that there are 126 bins, and our analysis was right.

Anyways, on 64-bit, the first largebin in category #1 has a range of [1009, 1009+64), or [1009, 1073) bytes. The size classes in this range are: [1024, 1040, 1056, 1072].
  - When we right shift these sizes by 6 bits, we get [16, 16, 16, 16]. Nothing interesting. Because these size classes belong to one single bin, they all must yield the same output.
  - When we divide these sizes by 2<sup>6</sup>, we get [16.0, 16.25, 16.50, 16.75]. While it is the same thing as above, hold on to it. It will become interesting at the right moment.

For 32-bit, the first largebin in category #1 has a range of [505, 505+64), or [505, 569) bytes. The size classes in this range are: [512, 520, 528, 536, 544, 552, 560, 568].
  - Right shift by 6 bits, we get: [8, 8, 8, 8, 8, 8, 8, 8].
  - Divide by 2<sup>6</sup>, we get: [8.0, 8.125, 8.25, 8.375, 8.5, 8.625, 8.75, 8.875]

It is both tedious and error prone to do this manually. So, there are two scripts, written to automate this process. They are in the `glibc-malloc/dynamic-analysis/scripts` directory, prefixed with `bin_info_*`. Both the scripts use elementary python constructs and generate one output file per config. Don't run them, just read.

One script is based on the pyramid, while the other is based on the macros. Why? Run both the scripts and read the 64-bit output files. The scripts are dense, as they are constructed with dynamic analysis in mind, so focus on the last 3 columns of the largebin tables.

We can notice a significant difference b/w the theoretical (pyramid-based) output and the runtime (macro-based) output. The boundary bins, where two categories transition, have an unusual amount of fixed classes. What explains that?

The truth is, the pyramid can't be implemented without these distorted bin ranges. It is simply not possible mathematically. That's another huge statement that requires proper evidence.

Just before this, we have seen that dividing the size classes in a largebin by its width generates an output where the integral part remains the same while the fractional part varies. Let's repeat this for the second bin in the first category, i.e. [1088, 1104, 1120, 1136].
  - We get [17.0, 17.25, 17.50, 17.75].
  - This is an arithmetic progression, where the difference between two consecutive elements is a constant.

Now notice the first crack in the bin order in 64-bit. There are 33 bins of width 64 bytes. That's because the condition accepts one extra bin.
  - 1024 will generate 16 and 3056 will generate 47. But it is set at 48. That's the cause of one extra bin.
  - Even if we have corrected this condition, we can't change anything.

Now open the theoretical output for 64-bit and focus on `bin #112`. It is the first largebin in category #3, with a width of 4096 bytes. Now divide 11264 by 4096, we get 2.75. That should not happen as the first size class in any bin yields `X.0`. It can't happen because 11264 is not a multiple of 4096. That's why `bin #112` has an unexpected number of bins and `bin #113` continues normally as 12288 is a multiple of 4096.

Now focus on `bin #120`. It is the first largebin in category #4. It has a width of 32768 bytes. Divide 44032 by 32768 and the script will repeat. "44032 is not a multiple of 32768" and now it is very clearly visible.

---

That's what the second aspect of the statement: "approximately logarithmically spaced" probably is. *"Not all bins are logarithmically spaced."*

If you read past versions of this file, you will notice a lot of rage and angered stuff in this section. I have trusted the pyramid too much and tried to understand `largebin_index_64` from that lens. That was the source of all of the anxiety, frustration, agitation and anger I have gone through.

Just seeing largebin_index_64 would make me angry and no matter how persistent efforts I make, it always drained me empty. It was until I had divided the size with the bin width, did I noticed the difference and the insights followed. By this time, I knew that bitwise right shift is divide by power-of-2. But I wasn't aware that it is a proper floor division.

Anyways, this summarizes the largebin ranges section. The script is the most reliable method to generate the largebins size ranges.

Now it is time for dynamic analysis.

## Dynamic Analysis (Incomplete)

You might be aware of tooling like pwndbg (PWN Debug) and GEF (GDB Extended Features). These are tools built on top of gdb. They provide a nice abstraction over the actual functionality. Because we want to build a strong mental model of the underlying architecture, we will not use any kind of tooling, except the debugger itself.

These are the experiments we will do.

1. Bin #2, represented by the headers bin[2] and bin[3], is the first smallbin of size class 32 bytes (MINSIZE on 64-bit).
2. Bin #63, represented by the headers bin[124] and bin[125] is the last smallbin of size class 1008 bytes (MIN_LARGE_SIZE-SMALLBIN_WIDTH on 64-bit).
3. Bin #1, represented by the headers bin[0] and bin[1] is the unsorted bin.
4. The smallbin size classes are in the range: `[MINSIZE, MIN_LARGE_SIZE-SMALLBIN_WIDTH]`, with a step of SMALLBIN_WIDTH.
5. Small chunks use only fd/bk pointers.
6. Bin #64, represented by the headers bin[126] and bin[127] is the first largebin in category #1.
7. The order in which chunks enter a bin.
8. A largebin is basically a collection of fixed size classes.
9. An in-depth analysis of the pointer fields in large chunks.

10. The total number of largebins.
11. There is no bin for size 0.
12. BIN_WIDTH on 64-bit scale by 3 bits only.
13. Show coalescing (both forward and backward).
14. Show fragmentation (internal, external, l1 and l2).
15. The exact amount at which bins top out.
16. Verify the distorted largebin boundaries.

---

# The Bookkeeping System, Part 3: The Evidence

Since I have made pretty big claims, it is important to prove them historically as well.
