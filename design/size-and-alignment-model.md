# The Size Model

Everything in glibc-malloc is directly or indirectly related with size. Therefore, the allocator has a size model to work with "size" efficiently.

The size model is described using macros. It contains two types of macros.
  1. Macros which resolve to certain numeric values.
  2. Macros that are named blocks of code.

The reason preprocessing is preferred over inlining, **in some scenarios**, is that, modern compilers are intelligent due to decades of compiler research. But optimization techniques like **function inlining** were in a complicated state in the early days.
  - The compilers did support inlining, but it was limited to a single translation unit and not as aggressive as we see it today.
  - Even when C99 formally introduced `inline`, it was still a hint, not a guarantee.

As a result, preprocessing was the only native option for the programmers to achieve similar effects.

Another reason macros are still advantageous (in some scenarios) is that inlining is not a simple copy-paste. 
  - The compiler has to understand the overall situation to inline a function such that everything works in harmony.
  - On the other hand, preprocessing is a simple copy-paste routine with ZERO runtime overhead.

---

The size model is about making the **request size** usable as per the bookkeeping process.

The first thing we need to understand is the allocator's choice for receiving the request size.

### The use of size_t

malloc() takes a size (in bytes) as its argument.

Each data type has a maximum addressable limit. We need a type which can contain the largest addressable value in an architecture. The answer is `size_t`.

***`size_t` is a guarantee from the C standard to be an "unsigned integer data type", which is exactly as wide as the "pointer type" in an architecture, taking the platform's data model in account. In simple words, size_t is a data type which is wide enough to contain the largest addressable value in an architecture.***

| Arch | size_t |
| :--- | :----- |
| 32-bit Linux | 4 bytes |
| 64-bit Linux | 8 bytes |

***`size_t` and the clever use of preprocessing is the basis of an architecture-agnostic implementation.***

---

But size_t is not used directly. It is masked with a type definition.
```c
#define INTERNAL_SIZE_T  size_t
```
This makes `size_t` a tunable parameter.

***A parameter whose value can be tweaked at compile-time is called a tunable parameter (or, a tunable).*** There are multiple such parameters provided for the programmers to customize malloc to their needs.

size_t being a tunable creates a third possibility, where pointers are 8 bytes and INTERNAL_SIZE_T is 4 bytes wide. But this looks counterintuitive to the definition of size_t.

In certain IoT and embedded system configurations, the architecture is modern (64-bit) with memory constraints. Tweaking INTERNAL_SIZE_T to 4 does two things.
  1. The metadata size per in-use chunk is shrunk by half, reducing the overall memory footprint. With INTERNAL_SIZE_T=4, mchunk_prev_size and mchunk_size size 4 bytes each, totaling to 8 bytes.
  2. The maximum request size is reduced drastically to ~4 GiB of virtual memory.

Tuning INTERNAL_SIZE_T is not problematic because, we use size_t to store a size, not an address. The tradeoff is also explicit and acceptable provided the constraints.

Also, INTERNAL_SIZE_T=4 doesn't create possibility for padding bytes in the struct as each member is naturally aligned to its own width given the layout order.

---

To summarize, there are the three configurations the allocator must handle.

| Config # | size_t width | Pointer width |
| :------- | :----------- | :------------ |
| 1 | 4 bytes | 4 bytes |
| 2 | 8 bytes | 8 bytes |
| 3 | 4 bytes | 8 bytes |

---

Now we are set to explore the macros that resolve to a numerical value.

### Macro #1 -> SIZE_SZ

It is the width of `size_t` on the target machine's architecture.
```c
/* The corresponding word size. */
#define SIZE_SZ  (sizeof(INTERNAL_SIZE_T))
```

| Arch   | SIZE_SZ |
| :---   | :------ |
| 32-bit | 4 bytes |
| 64-bit | 8 bytes |

---

### Macro #2 -> CHUNK_HDR_SZ

It stands for "chunk header size", which refers to the metadata bytes that sit at the beginning of an in-use chunk i.e. "mchunk_prev_size and mchunk_size".
```c
#define CHUNK_HDR_SZ    (2 * SIZE_SZ)
```

| Arch   | CHUNK_HDR_SZ |
| :---   | :----------- |
| 32-bit | 8 bytes |
| 64-bit | 16 bytes |
| INTERNAL_SIZE_T=4 | 16 bytes |

---

**Note: It refers to the structural overhead, not the functional overhead. Because, if it were functional, we wouldn't have count mchunk_prev_size, as it is a property of the previous chunk.**

---

### Macro #3 -> MIN_CHUNK_SIZE

It is the size of the "structurally" smallest possible chunk in an architecture.
```c
#define MIN_CHUNK_SIZE    offsetof(struct malloc_chunk, fd_nextsize)
```

`offsetof` is an ANSI C macro, defined in `stddef.h`, used to determine the byte offset of a specific member from the beginning of its parent structure.

Let's derive it manually on 64-bit.
```
  0-7 bytes -> mchunk_prev_size
 8-15 bytes -> mchunk_size
16-23 bytes -> fd
24-31 bytes -> bk
32-39 bytes -> fd_nextsize
40-47 bytes -> bk_nextsize
```
So, MIN_CHUNK_SIZE would be 32 on 64-bit.

| Config # | MIN_CHUNK_SIZE |
| :------- | :------------- |
| 32-bit   | 16 bytes |
| 64-bit   | 32 bytes |
| INTERNAL_SIZE_T=4 | 24 bytes |

---

### Macro #4 -> MALLOC_ALIGNMENT

It defines the minimum alignment for in-use chunks.
```c
#define MALLOC_ALIGNMENT  (                   \
  (2 * SIZE_SZ) < __alignof__(long double)    \
  ? __alignof__(long double)                  \
  : 2 * SIZE_SZ
)
```

`__alignof__` is an operator that returns the alignment requirement of a data type (in bytes).

| Arch   | \_\_alignof__(long double) |
| :---   | :------------------------- |
| 32-bit |  4 bytes |
| 64-bit | 16 bytes |

The macro becomes:
```c
// 32-bit (size_t=4)
MALLOC_ALIGNMENT == (8 < 4)  ?  4  :  8  == 8

// 64-bit (size_t=8)
MALLOC_ALIGNMENT == (16 < 8)  ?  16  :  16  == 16
```

In both the cases, the alignment is kept twice of the maximum addressable width. This is because malloc is a general purpose allocator. It is unaware of what the caller will store in the returned memory. So it ensures that the returned memory is aligned to all the fundamental types the C standard supports. This is in accord to what we have discussed in the mchunk_size section.

However, it's real importance is in that third configuration, where pointers are 8 bytes and INTERNAL_SIZE_T is 4 bytes wide.
```c
MALLOC_ALIGNMENT = (2 * 4) < 16  ?  16  :  16
```

Notice how the alignment is decided based on the largest fundamental type in the architecture, not the value of INTERNAL_SIZE_T.

---

| Config # | MALLOC_ALIGNMENT |
| :------- | :--------------- |
| 32-bit   |  8 bytes |
| 64-bit   | 16 bytes |
| INTERNAL_SIZE_T=4 | 16 bytes |

---

### Macro #5 -> MALLOC_ALIGN_MASK

MALLOC_ALIGNMENT is a power-of-2 value and MALLOC_ALIGN_MASK is the bit mask of it.
```c
#define MALLOC_ALIGN_MASK    (MALLOC_ALIGNMENT - 1)
```

MALLOC_ALIGN_MASK has the lowest 4 bits set which are clear in MALLOC_ALIGNMENT.
```bash
# 64-bit
MALLOC_ALIGNMENT  = 16 = 0001 0000
MALLOC_ALIGN_MASK = 15 = 0000 1111
```

It is used in a variety of bitwise operations.
  1. Check if a size/address is aligned to the alignment boundary (the lower 4-bits in the addr/size must be all 0 to yield a zero against all 1s of the bit-mask).
     ```c
     (addr & MALLOC_ALIGN_MASK) == 0  :=  aligned
     (addr & MALLOC_ALIGN_MASK) != 0  :=  misaligned
     ```
  2. Round a size/address up to the alignment boundary.
     ```c
     -> (size + MALLOC_ALIGN_MASK) & ~MALLOC_ALIGN_MASK
     -> (34 + 15) & ~15
     -> 49 & -16
     -> 48
     ```
  3. Round a size/address down to the alignment boundary.
     ```c
     -> size & ~MALLOC_ALIGN_MASK
     -> 41 & ~15
     -> 32
     ```
---

| Config # | MALLOC_ALIGN_MASK |
| :------- | :---------------- |
| 32-bit   |  7 |
| 64-bit   | 15 |
| INTERNAL_SIZE_T=4 | 15 |

---

### Macro #6 -> MINSIZE

It is the smallest size that malloc supports.
```c
#define MINSIZE    (unsigned long)( \
  ( (MIN_CHUNK_SIZE + MALLOC_ALIGN_MASK) & ~MALLOC_ALIGN_MASK)
)
```

We know that an in-use chunk requires (2 * SIZE_SZ) bytes for storing metadata and when it is freed, it requires (2 * ptr_width) bytes to manage fd/bk. That totals to 16 bytes on 32-bit and 32 bytes on 64-bit.

In INTERNAL_SIZE_T=4, the metadata overhead is 24 bytes, but 24 is not aligned to the alignment boundary (16-div), so we round it up to the next boundary, making MINSIZE 32 bytes.

| Config # | MINSIZE |
| :------- | :------ |
| 32-bit   | 16 bytes |
| 64-bit   | 32 bytes |
| INTERNAL_SIZE_T=4 | 32 bytes |

---

In the MIN_CHUNK_SIZE section, we have seen that it is the size of the structurally smallest chunk possible in an architecture. MINSIZE is the actual smallest chunk size possible in an architecture after adding alignment constraints.

The values happen to be equal in the first two configurations because the struct layout aligned with the alignment constraints. However, it broke with INTERNAL_SIZE_T=4.

---

### Macro #7 -> request2size

This macro is responsible for enforcing the size model on the requested size.

**It is the closest we can "statically" see the boundary tag method in implementation.**

It is defined as:
```c
#define request2size(req)    (                     \
  (req + SIZE_SZ + MALLOC_ALIGN_MASK < MINSIZE)    \
  ? MINSIZE                                        \
  : (req + SIZE_SZ + MALLOC_ALIGN_MASK) & ~MALLOC_ALIGN_MASK    \
)
```

Let's take an example on 64-bit architecture: `malloc(20)`.
  - (20 + 8 + 15) < 32; 43 < 32; Therefore, the false case is chosen.
  - aligned_size = (20 + 8 + 15) & ~15
  - aligned_size = 43 & ~15 = 32.
  - In these 32 bytes, we need 20 bytes of usable memory. That leaves us 12 bytes of memory for metadata. But metadata requires 16 bytes of space. We are short on 4 bytes. Visually:
    ```
          8           8         8     8
    -----------------------------------------------------------------
    | prev_size | mchunk_size | fd | bk | fd_nextsize | bk_nextsize |
    -----------------------------------------------------------------
                                ^ ptr_to_mem
    ```
  - But in the boundary tag discussion, we have agreed on a dummy chunk in the end. That would make the situation this:
    ```
          8           8         8     8
    -----------------------------------------------------------------
    | prev_size | mchunk_size | fd | bk | fd_nextsize | bk_nextsize |
    -----------------------------------------------------------------
                                              8
                                        -----------------------------------------------------------------
                                        | prev_size | mchunk_size | fd | bk | fd_nextsize | bk_nextsize |
                                        -----------------------------------------------------------------
                                ^ ptr_to_mem
    ```
  - That dummy chunk has a name. **The top chunk** is a special chunk that sits after all the malloced chunk. We'll talk about it later in detail.

So, request2size deliberately leaves out SIZE_SZ bytes of memory in every chunk because the payload memory of a chunk is allowed to "spill over" and occupy the prev_size of the next chunk. For the last allocated chunk, the prev_size is provided by the top chunk.

---

### Macro #8 -> chunk2mem

`chunk2mem` takes a pointer to a chunk, casts it to `char*` (for pointer arithmetic) and add "chunk header size" to it.
```c
#define chunk2mem(p)    ( (void*) ((char*)(p) + CHUNK_HDR_SZ) )
```

This will land us at the `fd` field in the struct, where the payload memory starts in an in-use chunk, just the way we have discussed.

---

***That's what the author probably meant when he said, "This struct declaration is misleading (but accurate and necessary)."***

Let's verify these things at runtime.

---
