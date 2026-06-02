# Why Virtual Memory

Each process believes it owns all the available memory.

**Simplified memory management and predictability** as every process can assume they start from `0x0` or something else.
  - The Memory Management Unit (MMU) and OS handle address translation from virtual to physical, freeing programmers from manual memory placement and relocation.

**Memory protection and isolation** is enforced via page permissions, eliminating the chance of one process invading other process.


---


# Virtual Address Space

Virtual address space is the runtime representation of a binary.

Virtual Address Space (or VAS) is divided into two parts.
  - Kernel Space
  - User Space

```
High Address
                     Top Of The Virtual Address
                                Space
0xFFFFFFFFFFFFFFFF *-----------------------------* End Of Kernel Space ↓
                   |                             |
                   |        Kernel Space         |
                   |                             |
                   |       Size: ~128 TiB        |
                   |                             |
                   |         Upper Half          |
                   |                             |
0xFFFF800000000000 *-----------------------------* Start Of Kernel Space ↑
                   |                             |
                   |    Unused / Guard Space     |
                   |                             |
0x0000800000000000 *-----------------------------* End of User Space ↓
                   |                             |
                   |         User Space          |
                   |                             |
                   |       Size: ~128 TiB        |
                   |                             |
                   |         Lower Half          |
                   |                             |
0x0000000000400000 *-----------------------------* Start Of User Space ↑
                   |                             |
                   |     Reserved / Unmapped     |
                   |                             |
0x0000000000000000 *-----------------------------*

                    Bottom Of The Virtual Address
                                Space
Low Address
```

User space and Kernel space are two logical divisions in virtual memory.

This logical division is achieved by access control privileges and protection rights, which are enforced both at hardware level (CPU) and software level (OS).

Most program instructions run entirely in user mode. Only specific operations that require hardware or system resources transition into kernel mode via system calls.

There is no limit on what you can execute, which creates problems. This division ensures that programs can be contained by default.
  - Any attempt to access privileged area doesn't get unnoticed. If it is inappropriate, the system denies it.

There is a proper mechanism through which the execution mode changes from user space to kernel space, when required.

## Analogy

Consider an office space with employees of different kinds and a room for the boos.

The boss's room is what kernel space is. Only privileged access is allowed and rest has to undergo a process to come there.

Then there is general area which is accessible to everyone as long as they are an employee in the company. This is what user space is.

When you need to do something that requires boss' permission, you go through a standard process, which is exactly how the execution context changes from user space to kernel space when required.

## Hardware Enforced Privilege Levels

**Rings** are hardware-enforced CPU privilege levels, which forms a core part of how modern processors (like x64) separates trusted code (kernel) from untrusted (user) code.

CPUs implement multiple **protection rings** numbered 0 to 3, with:
  - **Ring 0 = highest privilege (kernel mode)**
  - **Ring 3 = lowest privilege (user mode)**
  - **Rings 1 and 2** exist but are rarely used in mainstream Linux.


---


# Virtual Address

***A virtual address is a combination of information that helps in finding the corresponding physical address.***

In x64 systems, we have 64-bit wide registers. It would be huge to manage 2^64 addresses. So we stick to 48-bit virtual addresses. The remaining 16-bits are sign extension of the 47th bit.

That means, there are 2^48 unique virtual addresses, which gives us a huge VAS, sized about ~256 TiB. The lower half represents the user space and the upper half represents the kernel space.
```
User space   -> (Lower half, ~128 TiB) -> 0x0000_0000_0000_0000 -> 0x0000_7FFF_FFFF_FFFF
Kernel space -> (Upper half, ~128 TiB) -> 0xFFFF_8000_0000_0000 -> 0xFFFF_FFFF_FFFF_FFFF
```
The middle region, represented by *0x0000_7FFF_FFFF_FFFF to 0xFFFF_8000_0000_0000* is the **unused guard space**.

---

A virtual address on x64 for 4-level paging looks like:
```
+--------------------+ +--------------+ +--------------+ +------------+ +------------+ +----------------------+
| sign_ext(47th_bit) | | PML4: 9-bits | | PDPT: 9-bits | | PD: 9-bits | | PT: 9-bits | | Page Offset: 12-bits |
+--------------------+ +--------------+ +--------------+ +------------+ +------------+ +----------------------+
63                  48 47            39 38            30 29          21 20          12 11                     0
```

Every page table has 512 entries, requiring 9-bits to represent them. So, (9*4) 36-bits are reserved for them.

Each 9-bit group is an index in the corresponding page table.

Page offset is the actual byte being addressed within a page. Because a page has 4096 bytes, 12-bit are required to represent it.

Each lookup narrows the address space by 9 bits until the final page is found.

---

**Note: Total Addressable space ≠ Total Usable space.**
  - The program never runs out of virtual memory.
  - It only runs out of mappings in the physical memory.


---


# User Space

The user space is where unprivileged jobs are managed.

This is the general layout of the user space:
```
0x0000800000000000 *-----------------------------* End of User Space ↓
                   |   Stack (grows downward) ↓  |
                   *-----------------------------*
                   |    Memory-Mapped Region     |
                   |  (shared libs, mmap, ....)  |
                   *-----------------------------*
                   |         Free Space          |
                   *-----------------------------*
                   |    Heap (grows upward) ↑    |
                   *-----------------------------*
                   |  Static && Extern Variables |
                   |       (.bss / .data)        |
                   *-----------------------------*
                   |        Code (.text)         |
0x0000000000400000 *-----------------------------* Start Of User Space ↑
```

`.data` and `.bss` are packed together because they are functionally the same thing, just differ in initialization.

---

Memory was flat, is flat and will be flat. Stack and heap are two of the dozens of approaches to manage this memory. There are no specialized regions in the physical memory which refer to stack or heap.

---

A stack of plates grows upwards, but the stack in memory grows downwards. It is because the stack is placed at the top of the user space memory. It can't grow upwards, so it grows downwards. Simple.

As the stack grows, the address decreases.

The stack growing downwards bothers everyone for some time, but we get used to it.
  - A simple solution is to invert the diagram. Now the stack grows upward again.
  - Jokes aside, the addresses still decrease, not increase.

---

People say, "stack is fast, heap is slow". That can be attributed to how they are managed.
  - Stack is sequential, so you don't have the overhead to manage every single allocation.
  - Heap isn't sequential, you can allocate anywhere in the heap, which is why you have to keep extra bookkeeping to manage allocations.


---


# Kernel space layout ASCII


---


# `brk` System Call

The `brk` syscall extends the **program break** of a process to a specified address **if possible**.

In the early days of dynamic memory allocation, the data segment was data/bss and heap together.

It is logical as compilation reveals how much space you need for static/globals so the lower part of the data segment was reserved for static/globals and the upper part was extends for heap.

*Therefore, program break is the boundary which logically separates the data/bss from heap.*

For example, the data segment starts at `0d1000` and ends at `0d1015`. This means that 16 bytes are reserved for data/bss and the program break is at `0d1016` just one byte after the data/bss allocation.

When the `brk` syscall is invoked to allocate the memory by a malloc-family function, the syscall extends the program break. This newly allocated space is what "heap" is.

Subsequent calls to malloc-family functions that are resolved by `brk` extends the heap region and the program break points to the end of the newly allocated space.

The brk system call always extends the heap contiguously.

## Signature

This is the signature for the `brk` syscall:
```bash
int brk(void* end_data_segment);
```
  - On success, brk returns 0.
  - On failure, brk returns -1.

`end_data_segment` specifies the new address for the program break. brk will request the kernel to extend/shrink the heap until that address, if possible.

# `libc` Wrappers

`brk` has two wrappers in libc. We can use `man 2 brk` to obtain their signature.
```bash
int brk(void *addr);
void *sbrk(intptr_t increment);
```

`brk()` takes an address and changes the program break to it.

`sbrk()` takes an increment value, adjusts the program break by that amount and returns the previous program break address. `sbrk(0)` can be used to retrieve the current program break address.


---


# Memory Map (mmap)

***An mmap calls creates a virtual memory area (VMA), which is a contiguous virtual address range, backed by something (a file, anonymous, or nothing), with specific permissions and replacement semantics.***

Multiple mmap calls can overlap and replace VMAs depending on flags.

Unlike brk, which extends the heap contiguously, `mmap` can allocate memory anywhere in the mmap region of the virtual address space.

Every time we run a program on Linux, the dynamic linker (`ld.so`) uses `mmap` to load shared libraries in its address space. So, we don't use `mmap` directly, unless we're doing systems programming; but we are incomplete without it.

## Signature

This is the signature of the mmap syscall:
```bash
void *mmap(void *addr, size_t total_bytes, int prot_rights, int add_flags, int fd, off_t offset);
```

On success, it returns a pointer to the allocated memory.

On failure, `MAP_FAILED`, which means `(void *) -1`.

## Protection Flags

| Flag | Description |
| :--- | :---------- |
| PROT_NONE  | Pages exist purely in the VM map. Any access faults. No backing pages are instantiated. Used purely to reserve address space. |
| PROT_READ  |
| PROT_WRITE |
| PROT_EXEC  |

## Memory Attributes (Replacement Guidelines)

| Flag | Description |
| :--- | :---------- |
| MAP_ANONYMOUS | No file-backing. Pages are created lazily. Kernel guarantees zero-fill on first fault. This is used for BSS extensively. |
| MAP_PRIVATE | It can be file-backed or anonymous. Uses CoW (copy-on-write) and writes don not modify the file. |
| MAP_FIXED | The kernel must place mapping exactly at the given address. Any existing mappings in that range are unmapped first. Has no safety checks, can lead to silent destruction of previous VMAs. It overwrites **unconditionally**. |
| MAP_FIXED_NOREPLACE | The kernel must place mapping exactly at the given address. Fails if any VMA already exists there, doesn't overwrite. Implicitly SAFE than MAP_FIXED. |

## Use cases

`mmap` has a variety of use cases and dynamic memory allocation is one of them.

1. **File mapping.** Map a file into memory and access it like an array.
2. **Anonymous mapping:** Heap-like memory without touching the process break.
3. **Shared memory:** Two processes can map the same file and see each other's updates.


---


# 4-Level Paging

The 4-level paging architecture utilizes 4 pointer tables, each reducing the sample space by (1/512), helping in finding the right 4-KiB page in just 4 iterations.

The 4 page tables are:
  1. Page Table (PT).
  2. Page Directory (PD).
  3. Page Directory Pointer Table (PDPT).
  4. Page Map Level 4 Table (PML4).

Each table has exactly 512 entries because the page size (4-KiB) divided by the number of addressable bytes (4096/8) is 512.

Each entry is a pointer, so every entry is 8 bytes in size.

Remember,
  - *A page is a collection of individual bytes.*
  - *A 4-KiB page is a collection of 4096 addressable bytes in the virtual memory.*

## Page Table (PT)

Definitions:
  - A page table is a collection of 512 4-KiB pages.
  - A page table is a gateway to 512 4-KiB pages.

A page table has 512 pointers, each to a 4-KiB page.

Every entry in a page table represents a page of size 4 KiB (or 4096 bytes). Therefore, a page table manage a total of (512 * 4096) 2,097,152 bytes, which is 2 MiB.

## Page Directory (PD)

Definitions:
  - A page directory is a collection of 512 page tables.
  - A page directory is a gateway to 512 page tables.

A page directory has 512 pointers, each to a page table.

A page table manage 512 pages. Therefore, a page directory would manage (512 * 512) 262,144 pages.

A single entry in the page directory table manage 2 MiB. Therefore, a page directory would manage a total of (512 * 2,097,152) 1,073,741,824 bytes, which is 1 GiB.

## Page Directory Pointer Table (PDPT)

Definitions:
  - A page directory pointer table is a collection of 512 page directories.
  - A page directory pointer table is a gateway to 512 page directories.

A page directory pointer table has 512 pointers, each to a page directory.

A page directory manage 262,144 pages. Therefore, a page directory pointer table would manage (512 * 262,144) 134,217,728 pages.

A PDPT entry manage 1 GiB. Therefore, a page directory pointer table would manage a total of (512 * 1,073,741,824) 549,755,813,888 bytes, which is 512 GiB.

## Page Map Level 4 (PML4) Table

Definitions:
  - A PML4 table is a collection of 512 PDPTs.
  - A PML4 table is a gateway to 512 PDPTs.

A PML4 table has 512 pointers, each to a PDPT.

A PDPT manage 134,217,728 pages. Therefore, a PML4 table would manage (512 * 134,217,728) 68,719,476,736 pages.

A PML4 entry manage 512 GiB. Therefore, a PML4 table would manage a total of (512 * 549,755,813,888) 281,474,976,710,656 bytes, which is 256 TiB.

## Calculation

| Quantity | In Bytes   |
| :------- | :--------- |
| 1 KiB    | 1024 bytes |
| 1 MiB    | 1024 KiB = 1024 * 1024 bytes = 1,048,576 bytes |
| 2 MiB    | 2 * 1,048,576 bytes = 2,097,152 bytes |
| 1 GiB    | 1024 MiB = 1024 * 1024 KiB = (1024 * 1024 * 1024) bytes = 1,073,741,824 bytes |
| 512 GiB  | 5 * 1,073,741,824 bytes = 549,755,813,888 bytes |
| 1 TiB    | 1024 GiB = 1024 * 1024 MiB = 1024 * 1024 * 1024 KiB = 1024 * 1024 * 1024 * 1024 bytes = 1,099,511,627,776 bytes |
| 256 TiB  | 256 * 1,099,511,627,776 bytes = 281,474,976,710,656 bytes |

# Insights

| Tables | Total Entries | Size (each entry) | Each entry is gateway to | Total bytes managed | Pages per entry | Total Pages |
| :----- | :------------ | :---------------- | :----------------------- | :-------------------| :-------------- | :---------- |
| PT     | 512 | 8 bytes | 4096 bytes            | 2,097,152           | 1 (2 MiB)   | 512 (2^9) |
| PD     | 512 | 8 bytes | 2,097,152 bytes       | 1,073,741,824       | 512 (1 GiB) | 262,144 (2^18) |
| PDPT   | 512 | 8 bytes | 1,073,741,824 bytes   | 549,755,813,888     | 262,144 (512 GiB) | 134,217,728 (2^27) |
| PML4   | 512 | 8 bytes | 549,755,813,888 bytes | 281,474,976,710,656 | 134,217,728 (256 TiB) | 68,719,476,736 (2^36) |

In theory, the maximum fan-out possible is:
  1. 1 PML4 table,
  2. 512 PDPTs,
  3. 512^2 PDs,
  4. 512^3 PTs, and
  5. 512^4 pages.

But in practice, no process occupies this much memory so only a subset of this hierarchy is actually populated.


---


# Demand Paging

***Demand paging loads a page into memory only when it is accessed. The idea is only actively used pages should occupy physical memory.***

The remaining pages stay on disk until a virtual address accesses them and a #PF occurs, which triggers demand paging.

It reduces memory usage and startup time by avoiding loading pages which have no immediate use.


---


# Page Fault

***A page fault is an exception (interrupt) generated by the CPU when a virtual address cannot be translated to a physical address during a page walk. The MMU triggers it, and control is transferred to the OS.***

The page table entry (PTE) for the virtual address is marked not present (Present bit = 0).

The CPU stops the current instruction, pushes error information on stack and jumps to the *page fault handler* defined in the OS's *interrupt descriptor table*.

The OS checks why the page fault occurred.
  - If the page isn't loaded yet, the OS brings it from disk (demand paging).
  - If it's a protection violation, the OS may kill the process (segmentation fault).

The OS then updates the page tables to reflect the new mapping or terminate the process and returns to the instruction that caused the fault.

# Page Fault Reasons

The main reasons a page fault can occur include:

A page that is not present (Present-bit is 0).
  - The virtual address has no valid mapping in the page tables.
  - A typical case for demand paging: the page hasn't been loaded from disk yet.

Page protection violation (Present-bit is 1, but invalid access).
  - Writing to a read-only page.
  - User-mode process accessing a kernel page.
  - Executing from a non-executable page (NX bit).

# Why #PF

*A page is considered loaded when it has a valid physical frame mapped in the page tables and the Present bit = 1 in the PTE.*
  - If the present bit is 0, the page is not loaded yet. A #PF occurs on access.
  - If the present bit is 1, the page exists and the virtual address just translates to a physical frame and the access succeeds.

Multiple virtual addresses can map to the same page [at varying offsets] but a #PF is about the virtual address not translated successfully, not the physical frame.

---

Pages aren't always pre-loaded when a process starts. The OS uses lays down the required metadata to load the information later when it is required via demand paging.

The OS reserves virtual address ranges based on the requirements as specified in the executable file and updates the process's page tables to map virtual addresses for that range.
  - But the OS does not immediately copy all those bytes from the file into RAM.
  - Instead, the PTEs are set up with Present = 0 (not present) and some metadata about where to load the page from (file offset, protection flags, etc.).
  - Now the page 'exists' virtually as a range in the process's address space.
  - The physical page starts to exist when it is first accessed.

When a virtual address triggers a #PF, the OS loads the page from the disk, updates the PTE's to make the present bit as 1 and the virtual address resolves to a physical frame.

---

A #PF is simply the CPU trying to access a virtual page that has a valid mapping in the OS's bookkeeping but isn't backed by a physical frame yet.
  - It's not about virtual addresses never translated in general, but about this particular virtual page not being present in RAM yet.
  - Once the page is loaded, any access to any virtual address mapped to it will succeed (no #PF).

Therefore, a #PF is not worrisome. It is often the very first access to a virtual address whose page isn't in RAM yet, even if the virtual memory layout is prepared by the OS.

---

This architecture exists because executable files and shared libraries can be huge, and not every page is required immediately. Therefore, just like we defer relocating certain symbols which aren't required immediately, we defer mapping most of the pages because that boosts startup time and is a more efficient option that front-loading everything.

So, "*loading an executable file in virtual memory for execution*" is largely about laying the metadata in the address space and page tables so that when those pages aer demanded, the system can load them immediately.

A virtual page exists when the system lays down the necessary metadata to load a page and the page table entries have present bit set to 0.

A physical page exists when the CPU tries to use the value at a virtual address but can't do that because a physical page doesn't exist yet. A #PF is triggered and OS's #PF handler takes charge and loads the page.


---


# Page Walk

***A page walk is the CPU's hardware-driven process of traversing the multi-level page tables to translate a virtual address into a physical address when the TLB lacks a cached mapping.***

The MMU (Memory Management Unit) inside the CPU does performs the page walk. It's 100% hardware-driven and happens without any OS intervention, unless a fault occurs.

---

A page walk is triggered any time the CPU needs to translate a virtual address to a physical address, and it doesn't already have the translation cached in the TLB. For example:
  - When the CPU fetches the next instruction, it has a virtual address (from RIP on x64) that needs translation.
  - Any access to memory using a virtual address also needs translation.

The CPU sees a virtual address and checks the translation lookaside buffer for a cached mapping.
  - If a mapping is found, it is a **TLB hit**, which means *no page walk is required*.
  - If a mapping is not found, it is a **TLB miss**, which means *a page walk is required*.

The CPU reads the CR3 register, which points to the PML4 table, which sits at the top of the 4-level paging hierarchy.

Each table is itself a physical page, so every memory access during the walk is also translated.

# Example

Let's take a random virtual address: 0x000055F7C34D1000

It's binary representation would be
```
0000000000000000010101011111011111000011010011010001000000000000
```

If we make groups of bits just like how the virtual address is conceptually structured, we get
```
0000000000000000 010101011 111011111 000011010 011010001 000000000000
sign_extension     PML4i     PDPTi      PDi       PTi         PO
                    171       479       26        209
```

The CR3 register holds the root of the 4-level paging hierarchy, the PML4 table.

To find which page directory pointer table entry we are looking for in the PML4 table, we use the PML4i value. Therefore, CR3[171] is the one we are looking for.

The 51-12 bits in the entry pointed by CR3[171] will tell where this PDPT is in the physical memory.

---

We're inside a page directory pointer table, which has 512 page directories. The page directory we're looking for is given by PDPTi, that is, CR3[171][479].

The 51-12 bits in the entry pointed by CR3[171][479] will tell where this PD is in the physical memory.

---

We're inside a page directory, which has 512 page tables. The page table we are looking for is given by PDi, that is, CR3[171][479][26].

The 51-12 bits in the entry pointed by CRI[171][479][26] will tell where this page table is in the physical memory.

---

We're inside a page table, which has 512 pages. The page we are looking for is given by PTi, that is CR3[171][479][26][209].

The 51-12 bits in the entry pointer by CR3[171][479][26][209] is the page frame number, which tells where this page is in the physical memory.

We're in the page that has our value. To pinpoint it, we use the page offset value in the virtual address.

---

The final physical address corresponding to 0x000055F7C34D1000 is given by CR3[171][479][26][209][0].


---


# Entries In The PML4, PDPT and PD Page Tables

The PML4, PDPT and PD entries are different from PT entries.

A PML4E, PDPTE and PDE is likes this:
```bash
*----* *-------------* *---------------------------* * ------*
| NX | | OS Reserved | | phy_addr(NEXT_PAGE_TABLE) | | Flags |
*----* *-------------* *---------------------------* *-------*
63     62           52 51                         12 11      0
```
Only a few flag bits are different, the rest are the same as a PTE.

# Page Table Entry (PTE)

A page table entry (PTE) is the real gateway to a page.

***A page table entry encapsulates information which helps in translating a virtual page into its corresponding physical page frame.***

A page table entry for 4 KiB pages on x64 architecture looks like:
```bash
*----* *-------------* *-------------------* * ------*
| NX | | OS Reserved | | Page Frame Number | | Flags |
*----* *-------------* *-------------------* *-------*
63     62           52 51                 12 11      0
```

## Flag Bits

The flag bits are the 12 lower bits (0-11), which are as follows:
```bash
*-------------*---*-----*---*---*-----*-----*----*----*---*
| OS Reserved | G | PAT | D | A | PCD | PWT | US | RW | P |
*-------------*---*-----*---*---*-----*-----*----*----*---*
    11 - 9      8    7    6   5    4     3    2    1    0
```

Description of flag bits:

| Flag Bits | Name | Description |
| :-------- | :--- | :---------- |
| 0 | P   | Present bit: tells if the virtual page is mapped to a physical frame. (0: NO, 1: YES) |
| 1 | RW  | Writeable bit: tells if the page is writeable (1) or read-only (0). |
| 2 | US  | User mode: tells if the page is accessible from user-space. (0: NO, 1: YES) |
| 3 | PWT | Page write through: 0 = write-back caching (default), 1 = write-through caching. Controls how writes propagate to memory. |
| 4 | PCD | Page cache disable: 0 = caching enabled, 1 = caching disabled. Used for memory-mapped I/O or device regions. |
| 5 | A   | Accessed bit: set by CPU on any access (read/write/exec). OS uses it for page replacement decisions. Cleared by software when resetting aging info. |
| 6 | D   | Dirty bit: set by CPU when the page is written to. Relevant only for writable mappings. Used to decide if a page must be written back to disk. |
| 7 | PAT | Page attribute table index: selects one of the memory types from PAT (used with PCD/PWT). Defines caching behavior. |
| 8 | G   | Global bit: If set, translation stays in TLB across CR3 reloads. Used for kernel-space pages that remain constant across processes. |
| 9-11 |  | OS Reserved |

## Page Frame Number (PFN)

***The page frame number (PFN) identifies the physical page in the RAM that backs a virtual page.***

In x64 PTEs, bits 51-12 hold the PFN. Each PFN points to a 4-KiB-aligned physical frame.


---


# Types Of Paging

| Serial | Type | Description |
| :----- | :--- | :---------- |
| 1 | Demand paging | Pages are loaded only when accessed; causes page faults on first access. |
| 2 | Pre-paging (anticipatory paging) | Anticipates future accesses; pre-loads pages to reduce faults. |
| 3 | Copy-on-write (COW) paging | Shared physical page until one process writes, then copied. |
| 4 | Swapping/Page replacement paging | Pages moved between memory and disk under pressure.   |
| 5 | Clustered (group) paging   | Loads or evicts pages in contiguous clusters to reduce overhead. |
| 6 | Zero-fill-on-demand paging | Allocates new pages initialized with zeros when first accessed.  |
| 7 | Mapped file paging | Pages backed by files instead of anonymous memory. |


---


# Why page tables?

Modern systems comes with 64-bit addressable length.

Just like a flat table is a nightmare to manage every single byte, that's true for page tables as well.

A flat pointer table of 4 KiB pages for 8 GiB RAM would manage 2,097,152 entries.

A page table is a data structure that manage pages.

With multiple page tables sitting in an hierarchy, we are able to cut the sample space of possibilities. The approach is similar to binary search.

This hierarchical approach is known as ***4-level paging***.

Every process has its own page tables.

---

Take a sorted array of 1000 elements. The element we want is sitting at 762 index.
  - With linear search, it will take 763 rounds.
  - With binary search, it will only take 9 rounds, 1.18% of linear search.

Binary search reduces the search space by half (1/2) with each iteration.

4-level paging reduces the search space by (1/512) with each iteration.

More precisely, both binary search and 4-level paging reduce the sample space of possibilities **logarithmically**.

---

4-level paging uses a hierarchy of 4 page table, named:

  1. Page Table (PT).
  2. Page Directory (PD).
  3. Page Directory Pointer Table (PDPT).
  4. Page Map Level 4 Table (PML4).


---


# Why Paging?

Memory is **byte-addressable**. In 2025, most laptops comes with 8 GiB RAM at least. How many bytes does 8 GiB have?

  - 1 GiB = 1024 MiB
  - 1 MiB = 1024 KiB
  - 1 KiB = 1024 bytes
  - Therefore, 1 GiB = 1024 * 1024 * 1024 bytes = 1,073,741,824 bytes.
  - So, 8 GiB = 8 * 1073741824 = 8589934592 bytes or ~8.6 billion bytes.

Tracking every single byte in a flat table would make ~8.6 billion entries.
  - A 16 GiB RAM would have to manage ~17.2 billion bytes.
  - A 32 GiB RAM would have to manage ~34.4 billion bytes.

---

***Instead of managing these bytes flat, we manage them in groups. These group of bytes are called pages.***

Mainstream computing on x86 and x64 processors defaults to a page size of 4 KiB. But huge page sizes do exist. For example, macOS on Intel-based Macs uses 4 KiB pages, adhering to the standard for the x86-64 architecture and 16 KiB pages on Apple Silicon (ARM64), which is optimized for the performance characteristics of Apple's M-series chips.

4 KiB = 4 * 1024 bytes or 4096 bytes.
  - Therefore, *a page is a gateway to 4096 unique byte-addressable locations.*

