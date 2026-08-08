Status: In-progress

# What actually backs the main_arena?

***While non-main arenas are purely mmap-backed, things work differently with the main_arena.***

The main_arena can be backed by both sbrk and mmap, but only one syscall can be used at a time. Therefore, at any given moment, the main_arena is either backed by sbrk, or mmap.

We can ask why not both, and the answer is, **there can be one top chunk per arena**.

The next question would be, why separate top chunks don't exist, like av->sbrk_top and av->mmap_top. It remains unanswered, but I have a hypothesis. If we had separate top chunks, that design would work only for the main_arena as non-main arenas are purely mmap backed. Also, it might be simply unnecessary. But it is not a convincing hypothesis. Anyways.

---

***What decides which syscall will back the main_arena?***

We know that the allocator wants to service relatively large requests via mmap. The threshold is defined in malloc params as mmap_threshold.

When mmap_threshold is crossed, **mmapped chunks** are used to service the request. Note that mmapped chunks are a completely different thing. It is available both in the main_arena and non-main arenas.

When we talk about mmap as a mechanism to back the main_arena, we are talking about setting up a region with a top chunk. Normally that region is backed by sbrk. In case sbrk can not be used, we use mmap.

---

The question is, in what scenario sbrk can not be used?

1. **If a system doesn't have sbrk.** MORECORE is more like a backend. On standard systems, it is often sbrk. But we can plug a different mechanism that can provide memory.

2. The system does support sbrk, but it is not available in this moment, for some reason.

3. The allocator was using sbrk but the system suddenly refused to increase the program break and release more memory.

In these scenarios, we have to use an alternate mechanism to fulfill the demands, and mmap comes as a rescue, when available, obviously.

---

There is a phenomenon called "holes in the address space".

We know that VMAs (virtual memory areas) themselves are extensible, but if there is another VMA soon after the current one, the kernel will not be able to extend the current VMA beyond the available unmapped space.

These unmapped regions between two VMAs are what "holes in the address space" are.

---

The program break region belongs to one VMA, while other allocations belong to other VMAs. Suppose two scenarios.
  1. The process asked (and used) so much of program break memory that it exceeded the kernel's limits for the program break region.
  2. The kernel's limits aren't exceeded yet, but the VMA needs extension. But the next VMA is very close to the program break one and the total unmapped space available combined with the unused sbrk memory available is not enough to serve the request.

In the first scenario, sbrk can not be used anymore. In the second scenario, sbrk can not be used for this request only. A future request might be such that sbrk could be used.

***If sbrk has failed and mmap is backing the main_arena now, why not use mmap always from this moment?***

When the allocator calls sbrk, either it gets the memory, or not. The allocator has no idea why the request failed. It only knows the request failed and another mechanism (if available) needs to be used.

There is no indication that the kernel will always refuse sbrk from now. It may succeed depending on the size. Therefore, the allocator doesn't refrain from calling sbrk. That means, if the current sbrk call failed and mmap succeeded, mmap will back the main_arena. In future, if the available top was required to be extended, and sbrk succeeded, a new top chunk will be initialized which will be backed by sbrk.

This does look strange, so take this scenario.
  - The main_arena is backed by sbrk right now.
  - The top chunk had 4 pages of memory.
  - The total unused memory available before the next VMA is 12 pages.
  - Pages are standard 4-KiB ones.
  - The maximum size that sbrk can be used for is 16 pages. However, a request for 20 pages will result in a MORECORE_FAILURE. To remind you, 20 4-KiB pages are 81920 bytes. This is far below the minimum mmap threshold. So it doesn't qualify for an mmapped chunk.
  - Because sbrk has failed, mmap will be used. A region of MMAP_AS_MORECORE_SIZE size will be requested to back the main_arena.
  - Once the mmap backed top is utilized to the point that it requires extension, we will try sbrk again. Suppose the request cam for 10 pages. The mmap backed top had 4 pages while the total unused space that sbrk can occupy is 12 pages. sbrk will succeed and that unused gap will be utilized properly.

So, if we think big, this approach does have some merit. The only problem is understanding how it is actually implemented. The process itself is quite simple; call mmap, get memory, regularize the old top, setup the new top and return. But that doesn't happen in isolation, making the actual implementation slightly complicated.
