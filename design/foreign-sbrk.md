Status: In-progress

# Foreign SBRK

The memory obtained through program break expansion (sbrk) forms a contiguous region.

When a process uses a virtual memory allocator, like glibc's malloc, the process doesn't have to use sbrk/mmap directly. The allocator manages them internally.

If the process calls sbrk outside of the allocator, it moves the program break and invalidates the allocator's assumptions about the current program break and the top chunk. This is what a foreign sbrk is.

## The Allocator's Assumption

A successful program break extension returns the previous program break. It must be equal to the end of the top chunk owned memory.

In the illustration below, when the program break is extended to new_brk, old_brk will be returned and `(old_brk == top_end)`.
```
   | ........ |
   ^          ^
top_end    new_brk
old_brk
```

When the process extends the program break outside of the allocator, the program break returned to the allocator will be different. Look at the illustration below.
```
# What the allocator think!
   | ........ |
   ^          ^
top_end    new_brk
old_brk

# What the reality is!
   | ........ | ........ |
   ^          ^          ^
top_end    for_brk    new_brk
old_brk
```
  - The allocator expects old_brk, but gets for_brk.

---

Program break expansion is always contiguous, but the allocator doesn't own the foreign sbrk extended memory.

A successful sbrk() indicates that "the program break has been moved successfully". That expansion being contiguous as per the allocator's bookkeeping is not the kernel's problem.

---

That's why, the allocator always checks if sbrk returned a memory that is contiguous to the previous program break.

When loss of contiguity is detected, the NONCONTIGUOUS_BIT in (m->flags) is updated so that future growth is handled using the non-contiguous strategy.

---

Similarly, a failure returns (void*)(-1). It doesn't tell why!

Why a foreign mmap is not possible? (or, is it?) because of RLIMIT_DATA?

---

The memory obtained through program break expansion 
(sbrk) forms a contiguous region. But there are two 
threats to this contiguity. First is a foreign sbrk.

[AN OBSERVATION]:
- We know that sbrk(0) returns the current program 
  break. It can be compared with the current top end 
  to establish clarity on contiguity. So far, there 
  are no traces of that happening.
- One possible explanation is that querying the current
  program break first provides little benefit. If another
  MORECORE call is required immediately afterward, the
  first query may simply add overhead without reducing the
  amount of work. We'll revisit this after exploring how
  glibc actually detects and manages foreign sbrk events.
