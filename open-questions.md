Status: Pending.

# Open Questions

As per the git repository, I am on the quest of understanding malloc since February 27, 2026. As of writing this, July 21, 2026, **4 months and 24 days** have passed, or 144 days.

In this journey, I have asked so many questions. Some were easy to answer, some were hard. But a handful of questions are unanswered.
  - I have consulted the historical dlmalloc source and the commits associated to malloc.c and arena.c.
  - I have worked on those questions for multiple days, fighting irritation and agitation.
  - I have asked AI to help me reason about those questions, but it led to nowhere.
  - I have tried a lot, but I couldn't find an explanation for those questions.

I am listing those questions, along with the angles I have explored, in hope that someone who has maintained this code and knows it better can show me the way to find my answers.

I have learned from my teacher to always show the work I have done to solve the problem before reaching out to him, so that he can correct my course and I can find the solution myself. I will write my reasoning along with the questions. There is one caveat, though.

I have already explored malloc.c and large parts of arena.c. Right now, I am polishing my work to present it properly. While new questions are rising too, a lot of the questions are an artifact of past. Therefore, a lot of these questions are already explored and the original rigor in the reasoning is lost, and what remains is a polished version of what I thought is right to preserve.

The reason I don't want to explore those questions once again to form fresh reasoning is that I didn't had a very pleasant experience with them. When I find unanswerable questions, I always say to myself that I still don't have the hold on this code and I am missing something. A codebase as historical as malloc can not lack reasoning for an action. I go beyond the limit to answer it. That means, multiple days wrestling with the question. When I have tried all the avenues and the question is still answered, I don't know what is the stopping condition. If I stopped, I feel like I didn't tried enough. If I continued, I don't know if it is even worth investing my time, energy and attention.

I don't fear facing this dilemma. But because I know that I have tried multiple methods already, I am not sure if it is worth repeating, unless I somehow naturally stumble upon something and it made me ask something and something clicked. Then I am back in the game and ready to explore every possibility once again. the condition is *it must happen naturally, not by force*.

---

An interesting thing with questions is that everyone interprets them differently. A question might be trivial to an experienced individual, but really complex for a naive.

All I am saying is that a question itself doesn't carry the element of complexity. Complexity is perceived differently by different people because of different levels of experience and familiarity.

---

## [QUES-1]: In what scenario (av == NULL) ?

As per `/glibc/elf/libc_early_init.c`, the function __libc_early_init calls __ptmalloc_init() to setup the early allocator metadata, including the main arena. By the time the first malloc call is made, an arena already exist.

## [QUES-2]: What is the use of INTERNAL_SIZE_T=4 configuration?

On a 64-bit machine with INTERNAL_SIZE_T=4:
  - The allocator still has to satisfy 16-byte alignment. The chunk layout still has to preserve that alignment.
  - So, what benefit does reducing the width of the size fields create?

## [QUES-3]: What is the purpose of the outer for loop after path-2 in \_int\_malloc?

## [QUES-4]: Why old_size is aligned down after carving space for fencepost chunks?

When the allocator maintains the invariant that the top chunk is always aligned to a page boundary, when the input size is always normalized to a MALLOC_ALIGNMENT boundary, what is the scenario when the top chunk is misaligned?

For a moment, let's accept the top chunk was misaligned. After aligning it down, there will be some bytes that no longer belong to the top chunk. What happens to them? The fencepost can not absorb them as it would disturb their alignment.

## [QUES-5]: In the unsorted bin pathway (path-3) in \_int\_malloc, the victim must be an exact fit or the last_remainder, otherwise it is binned. Why?

It is possible that a large chunk exists in the unsorted bin that can be used to satisfy the request under best-fit rule. But we bin every chunk only to rediscover them later through regular bin search. Why every chunk is not given equal opportunity?

## [QUES-6]: In sysmalloc, when the top chunk configuration is accessed before accessing the non-main arena or the main arena path, why there are only asserts and no if-blocks? The asserts are compiled out in production builds and only one of three conditions are pre-checked by _int_malloc.

## [QUES-7]: Why the fencepost setup is different in main_arena and non-main arena?

For more information, read [consolidation.md](../design/consolidation.md)

## [QUES-8]: A non-main arena can have multiple heap segments, but there will be only one top chunk and that will belong to the last heap segment. In _int_free_maybe_trim, what is the point of extracting the heap corresponding the top chunk, then passing it to heap_trim, then obtain the arena corresponding to it and the top chunk from the arena? Why don't we pass the top chunk directly?

## [QUES-9]: How the names of the free functions were decided?

Based on my understanding of these functions, 
  - _int_free_chunk is a thin wrapper that decides between the regular free functions and munmap.
  - _int_free_merge_chunk is backward consolidation.
  - _int_free_create_chunk is forward consolidation.
  - _int_free_maybe_trim calls the right function to attempt trimming if the threshold is crossed.

A codebase that needs to be managed for a long time, I assume there is some naming convention in glibc!

The prefix `_int` might indicate ***an internal function***. `_free` might indicate that this is a free-family function. `_trim` might be used to represent trimming. But I am confused about the rest.

---

The modern malloc implementation is based on ptmalloc2, which is itself based on dlmalloc@2.7.x. This repository on [GitHub](https://github.com/DenizThatMenace/dlmalloc/) is a mirror of the official one. dlmalloc v2.7.x use a single free function. So, the naming is definitely not a historical artifact that glibc inherited.

This commit introduced _int_free_merge_chunk, _int_free_create_chunk, and _int_free_maybe_consolidate.
```
--------------------------------------------------
Commit: 542b1105852568c3ebc712225ae78b8c8ba31a78
Author: Florian Weimer <fweimer@redhat.com>
Date:   Fri Aug 11 14:48:17 2023
Message: malloc: Enable merging of remainders in memalign (bug 30723)
--------------------------------------------------

Previously, calling _int_free from _int_memalign could put remainders
into the tcache or into fastbins, where they are invisible to the
low-level allocator.  This results in missed merge opportunities
because once these freed chunks become available to the low-level
allocator, further memalign allocations (even of the same size are)
likely obstructing merges.

Furthermore, during forwards merging in _int_memalign, do not
completely give up when the remainder is too small to serve as a
chunk on its own.  We can still give it back if it can be merged
with the following unused chunk.  This makes it more likely that
memalign calls in a loop achieve a compact memory layout,
independently of initial heap layout.

Drop some useless (unsigned long) casts along the way, and tweak
the style to more closely match GNU on changed lines.
```

This commit introduced __int_free_chunk.
```
--------------------------------------------------
Commit: c621d4f74fcbb69818125b5ef128937a72f64888
Author: Wangyang Guo <wangyang.guo@intel.com>
Date:   Thu Aug 29 11:57:28 2024
Message: malloc: Split _int_free() into 3 sub functions
--------------------------------------------------

Split _int_free() into 3 smaller functions for flexible combination:
* _int_free_check -- sanity check for free
* tcache_free -- free memory to tcache (quick path)
* _int_free_chunk -- free memory chunk (slow path)
```

These commits do tell why a big function was broken down into smaller ones. From the perspective of writing modular code, this act doesn't require much reasoning. But why these strange names were chosen is something that is missing.

I perfectly understand that naming can be a personal choice, but this is certainly not a personal project.

Each of these functions have good annotations preceding them, and I didn't face any issues understanding them. But a function's name represents what it does and it is possible to have better names that reflect it.
