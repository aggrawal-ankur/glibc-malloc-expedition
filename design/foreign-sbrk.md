Status: Done

# Foreign SBRK

The memory obtained through program break expansion (sbrk) forms a contiguous region.

When a process uses a virtual memory allocator, like glibc's malloc, the process doesn't have to use sbrk/mmap directly. The allocator manages them internally.

However, if a component calls sbrk directly, it moves the program break and invalidates the allocator's assumptions about the current program break and the top chunk. This is what a foreign sbrk is.

## The Allocator's Assumption

*A successful program break extension returns the previous program break. It must be equal to the end of the top chunk owned memory.*

In the illustration below, when the program break is extended to new_brk, old_brk will be returned and (old_brk == top_end).
```
   | ........ |
   ^          ^
top_end    new_brk
old_brk
```

When the process extends the program break outside of the allocator, the program break returned to the allocator will be different. Look at the illustration below.
```
   | ........ | ........ |
   ^          ^          ^
top_end    for_brk    new_brk
           old_brk
```
  - Here, (top_end != old_brk).

---

A successful sbrk() indicates that "the program break has been moved successfully". Whether that expansion is contiguous to the allocator is the allocator's problem.

That's why, the allocator always checks if sbrk returned a memory that is in continuation with the previous program break.

---

There are multiple points at which a foreign sbrk can occur. Then there is concurrency, something I don't understand yet. Therefore, I only understand one scenario of foreign sbrk, where sbrk is called first and the allocator is invoked later. Everything below considers that scenario only.

When the allocator notice a foreign sbrk, it doesn't abandon sbrk. Instead, it regularizes the existing top chunk, put fenceposts and calls sbrk again to setup the new top chunk, while the previous sbrk memory is used to service the request.

While the process looks simple, it doesn't happen in isolation. As a result, the implementation is slightly complicated.
