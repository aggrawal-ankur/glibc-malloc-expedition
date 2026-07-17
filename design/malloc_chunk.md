- [Chunk Description](#chunk-description)
  - [Layout Description](#layout-description)
  - [Usage Description](#usage-description)
  - [Fragmentation](#fragmentation)
  - [Coalescing](#coalescing)
- [Dynamic Analysis](#dynamic-analysis)


# Chunk Description

When malloc is called, the allocator carves a piece of dynamic memory, attaches some bookkeeping and returns it to the process. This bookkeeping is kept in a structure called `malloc_chunk`.

***A chunk is a piece of metadata associated with a portion of dynamic memory.*** But when we say "a chunk of memory", it refers to "the *chunk metadata* and the *dynamic memory*" **together**.

This metadata sits right before the payload memory, like this:
```
metadata  payload
          ^
          pointer returned to the process
```

---

The layout of this chunk is:
```c
struct malloc_chunk {

  INTERNAL_SIZE_T       mchunk_prev_size;
  INTERNAL_SIZE_T       mchunk_size;

  struct malloc_chunk*  fd;
  struct malloc_chunk*  bk;

  struct malloc_chunk*  fd_nextsize;
  struct malloc_chunk*  bk_nextsize;
};
```

The authors have acknowledged that this layout is "misleading".
```
/*
  This struct declaration is misleading (but accurate and necessary).
  It declares a "view" into memory allowing access to necessary
  fields at known offsets from a given base. See explanation below.
*/
```
But in my understanding, the layout is well-reasoned. It is the reasoning which is not documented properly, making it complicated to understand. The following is my attempt to make that reasoning visible.

Let's start with understanding each field in malloc_chunk.

## Layout Description

The **allocation size** is divided into **small** and **large** based on a threshold. Therefore, we have two types of chunks based on **size**: small chunks and large chunks.

A chunk can exist in two states: **in-use** and **free**.
  - **In-use chunks** (both small and large) are self-managed and require nothing other than the usual chunk metadata.
  - **Free chunks** require extra bookkeeping as they can be reused in servicing future malloc requests. Small free chunks and large free chunks are managed differently.

---

Based on the information above, the allocator has 3 chunk states to manage.
  1. **In-use chunks**: chunks the process is actively using (both small and large).
  2. **Small free chunks**: small chunks the process has freed.
  3. **Large free chunks**: large chunks the process has freed.

Here is a high level description of how malloc_chunk is used to represent these 3 states of chunks.

`mchunk_prev_size` holds the size of the previous chunk and `mchunk_size` holds the size of the current chunk.
  - **Note1: size means (chunk_metadata + dynamic_memory).**
  - **Note2: INTERNAL_SIZE_T is discussed later. For the time being, treat it like `size_t`.**
  - **Note3: mchunk_prev_size is discussed later.**

---

Free chunks are managed via bins, which are "**circular doubly linked lists**". We have small bins for small chunks and large bins for large chunks.

Small bins manage free chunks of exact (fixed) size classes, while large bins manage free chunks falling in specific size ranges. For example:
  - a small bin of size class 80 bytes contains free chunks of size 80 bytes.
  - a large bin of size range [1024, 1088) bytes contains free chunks of sizes falling in that range, like 1024, 1040 and so on.

A small bin uses only the `fd/bk` fields of malloc_chunk to form a doubly circular linked list. A large bin uses both the `fd/bk` and the `fd_nextsize/bk_nextsize` pointers. This is a part of the bookkeeping section and it is discussed there in detail.

---

In simple words, ***`malloc_chunk` is a generic implementation, designed to provide a single interface for all the three states in which a chunk can exist.*** This is both advantageous and cumbersome.

This is the reasoning behind the layout. Let's discuss how this layout is used.

## Usage Description

**Note: For simplicity, all the calculations are for 64-bit Linux. But the rules are the same for 32-bit Linux. Just use 4 instead of 8.**

On 64-bit Linux, both size_t and pointers are 8-bytes wide. That means, the size of malloc_chunk is (8*6) 48 bytes. We can verify this with sizeof as well. Create a .c file, copy the definition, and print `sizeof(struct malloc_chunk)`.

malloc_chunk being a generic implementation is advantageous as it allows all the three 3 states of a chunk to be represented by a single struct definition. But in reality, these 3 states require only a subset of the whole implementation.
  1. mchunk_prev_size and mchunk_size are necessary in all the cases.
  2. In an in-use chunk, the four pointers aren't useful.
  3. In a small free chunk, fd/bk are required and fd_nextsize/bk_nextsize aren't useful.
  4. In a large free chunk, everything is required.

Therefore, we need to use malloc_chunk such that,
  - fd/bk/fd_nextsize/bk_nextsize remain garbage in an in-use chunk, and
  - fd_nextsize/bk_nextsize remain garbage in a small free chunk.

We have two ways to use malloc_chunk.
  - **Method1:** Set the required members appropriately and the not required ones NULL.
  - **Method2:** Only set the required members and leave the rest.

Let's calculate how these methods perform for an in-use chunk.

In method1, we have to allocate full 48 bytes for the metadata, followed by the payload memory. This wastes 24 bytes per in-use chunk, regardless of small or large. Visually:
```
-----------------------------------------------------------------
| prev_size | mchunk_size | fd | bk | fd_nextsize | bk_nextsize |
-----------------------------------------------------------------
                                                                  ^
                                                                  Dynamic memory starts here
```

In method2, only the initial 16 bytes in the metadata struct are usable, followed by the payload memory. Visually:
```
-----------------------------------------------------------------
| prev_size | mchunk_size | fd | bk | fd_nextsize | bk_nextsize |
-----------------------------------------------------------------
                            ^
                            Dynamic memory starts here
```
  - Method2 prevents the wastage of the trailing 24 bytes.
  - Those fields still exist, but are garbage.

Method2 is how glibc does it.

---

**Important Note**

As someone new to this, the design is not very beginner-friendly. If you can't understand it in your first attempt, remember this, I've invested weeks to understand every part of glibc-malloc, fighting this design. The document you are reading is a result of multiple rewrites.

A lot of times, we prevent ourselves from understanding the author's design because, we think that the problem should be solved in a certain different way. This is completely an unconscious act, which is why we are not aware of it.

To improve yourself, you can try this.
  - Acknowledge the author's design even if you have to do it against your will.
  - Write your design on a paper or in your IDE.
  - Contradict your design with the author's design and notice which performs better.

Either this act will make the author's design clearer, or you'll end up finding a better one. Either way, it's a win.

---

This is how a single malloc_chunk exists. But a standard Linux process calls malloc and free several times. This repeated allocation-deallocation fragments the memory and creates a problem for the allocator.

## Fragmentation

***When memory is allocated-deallocated multiple times, it creates gaps of "unused memory" in the address space.***

This increases pressure on physical memory because, the freed chunks are still backed by physical memory but not utilized by the process. The only way to reduce this pressure is to reuse (reallocate) the freed chunks, which entirely depends on the process asking for a size which is available as a free chunk.

The fragmented memory can exist in two layouts, depending on the malloc-free sequence.
  1. In-use and free chunks in an alternating sequence, like this: {...., in-use, free, in-use, free, ....}
  2. Multiple free chunks adjacent to each other, like this: {...., in-use, free, free, in-use, ....}

Suppose two chunks of 48 bytes were freed. Now we have 96 bytes of memory which can be reused. The next malloc request asked for 96 bytes. Can we reuse the 96 bytes? No. Because, those 96 bytes are not contiguous. That is fragmentation in layout1.

Suppose two adjacent chunks, each of size 48 bytes, were freed and the next malloc request asked for 96 bytes. Can we reuse these 96 bytes? NO. Because, the memory is contiguous yet fragmented across two chunks. That is fragmentation in layout2.

---

The allocator can't do anything about the fragmentation in layout1. But the allocator can manage layout2 fragmentation to some extent. Take this:
  - The probability of the process asking for another 48 bytes bytes chunk might be less, but the probability of asking a size which falls in the range of 48-96 bytes is definitely higher.
  - But this is possible only when the two adjacent free chunks are coalesced, making one big block of free memory. Basically, converting layout2 memory to layout1 memory.

To implement coalescing, we need two things.
  1. A way to identify if the next/prev chunk is free.
  2. If the next/prev chunk is free, we need a way to reach that chunk from the current chunk.

A computer scientist and mathematician named **Donald Knuth** has discussed multiple strategies to manage dynamic memory. One of these strategies describe a way to embed coalescing support directly in the chunk metadata. It is discussed in his book *The Art Of Computer Programming, Volume 1: Fundamental Algorithms*, paragraph 4, page 440. It is called, **the boundary tag method**.

The authors of this allocator have implemented the same strategy. Let's dive into coalescing.

## Coalescing

Coalescing can happen in two ways.
  1. **Forward coalescing**, where we coalesce the n<sup>th</sup> chunk with the (n+1)<sup>th</sup> chunk.
  2. **Backward coalescing**, where we coalesce the n<sup>th</sup> chunk with the (n-1)<sup>th</sup> chunk.

Forward coalescing is simple to implement. Just add the size of the current chunk into the pointer and we are on the next chunk. But backward coalescing is complicated as we don't know the size of the previous chunk.

For this reason, malloc_chunk comes with `mchunk_prev_size`. This field stores the size of the previous chunk and we can use it to offset back to the (n-1)<sup>th</sup> chunk.

Now we need a way to find if the next/prev chunk is free. To do this, we use mchunk_size. Let's understand how.

---

### The second use of 'mchunk_size'

We know that `malloc()` returns a memory which can store the largest fundamental type supported by the ISO C standard.

The largest type in both 32-bit and 64-bit architectures is twice the maximum addressable width, i.e. `double` (8 bytes) on 32-bit and `long double` (16 bytes) on 64-bit.

That means, the size is always a multiple of 8, regardless of the architecture (32-bit or 64-bit). That means, the lower 3 bits in mchunk_size are always 0 (or better, **unused**).

We can use these bits of mchunk_size to store state information. It does change the size value, but we can mask the lower 3 bits to get the actual size.

Here is a description of these bits.

| Bit # | Bit Name | State | Description |
| :---: | :------- | :---- | :---------- |
| 0 | PREV_INUSE (P) | **0** (clear) | The (n-1)<sup>th</sup> chunk is free and the prev_size of the n<sup>th</sup> chunk stores the size of the (n-1)<sup>th</sup> chunk. |
| | | **1** (set) | The (n-1)<sup>th</sup> chunk is in-use and the prev_size of the n<sup>th</sup> chunk doesn't store the size of the (n-1)<sup>th</sup> chunk. |
| 1 | IS_MMAPPED (M) | **0** (clear) | 
| | | **1** (set) | 
| 2 | NON_MAIN_ARENA (A) | **0** (clear) | The chunk belongs to the main arena. |
| | | **1** (set) | The chunk belongs to a non-main arena. |

---

Right now, only the 0th bit concerns us. The remaining two bits are discussed in the appropriate sections.

Now we can implement coalescing through the boundary tag method.

---

### The Boundary Tag Method

***Boundary tag method is a dynamic memory management technique, where the size is stored both in the head and the tail of the chunk.***

It suggests to have metadata before and after the payload memory. `malloc_chunk` compensates for what comes before the payload memory, what compensates for the trailing size field?

If we create a separate struct, like `malloc_chunk_trail` and put it after the payload memory, that creates bookkeeping havoc.

How about putting another size field in the front of malloc_chunk and use it as a property of the previous chunk?
  - We don't have to create a new metadata struct.
  - The first chunk's prev_size would be a waste, as nothing exist before it. But we are ready for that tradeoff.
  - There will be some sort of dummy chunk in the end to compensate for the last malloced chunk.

The layout would look something like this:
```
Structurally -> [ Chunk1                                     ] [ Chunk2                                     ] [ Dummy Chunk                                ]
                ---------------------------------------------- ---------------------------------------------- ----------------------------------------------
                | prev_s | chunk_s | fd | bk | fd_ns | bk_ns | | prev_s | chunk_s | fd | bk | fd_ns | bk_ns | | prev_s | chunk_s | fd | bk | fd_ns | bk_ns |
                ---------------------------------------------- ---------------------------------------------- ----------------------------------------------
Functionally ->          [ Chunk1                                       ] [ Chunk2                                     ]
```

***The mchunk_prev_size of the n<sup>th</sup> chunk is "by-use" a part of the (n-1)<sup>th</sup>chunk. Structurally, it is still a part of the n<sup>th</sup> chunk.*** This is boundary tag method in implementation.

-- **Important Note** --

***Again, as someone new to this, the design is not beginner-friendly at all. If you can't understand it in your first attempt, don't worry. What you are reading is months of work and a result of multiple rewrites.***

***I don't how long it will take you to understand it, but it took me more than a month worth of efforts just to have a fragile understanding of it, which was later corrected by another idea that came to me, that I tested and found correct.***

***Therefore, give yourself time.***

---

Now we largely understand the size fields. To complete our understanding, we have to explore one last piece.

# Dynamic Analysis

All walkthroughs target 64-bit.

These are the experiments.

1. The smallest chunk size is MINSIZE bytes.
2. Structural analysis of a chunk.
3. The dummy chunk (top) and the boundary tag implementation.
4. Free chunk analysis and the need for a barrier chunk.
5. prev_size and state of PREV_INUSE bit.
6. The pointer fields are garbage in in-use chunks.

---

The facts we can't verify yet, because we don't know what exactly is small and large size.

1. Small free chunks only use fd/bk. 
2. Large free chunks use every field.
