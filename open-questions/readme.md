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

## Ques: In what scenario (av == NULL) ?

As per `/glibc/elf/libc_early_init.c`, the function __libc_early_init calls __ptmalloc_init() to setup the early allocator metadata, including the main arena. By the time the first malloc call is made, an arena already exist.

## Ques: What is the use of INTERNAL_SIZE_T=4 configuration?

On a 64-bit machine with INTERNAL_SIZE_T = 4:
  - The allocator still has to satisfy 16-byte alignment.
  - The chunk layout still has to preserve that alignment.
  - So reducing the width of the size fields does not automatically shrink every chunk header by 8 bytes.

## Ques: What is the purpose of the outer for loop after path-2 in \_int\_malloc?

## Ques: Why old_size is aligned down after carving space for fencepost chunks?

The allocator maintains the invariant that the top chunk is always aligned to a page boundary. Size is always normalized to align it to a MALLOC_ALIGNMENT boundary.

In what situation(s) the top chunk can be misaligned?

---

For an instance, let's accept the top chunk was misaligned. After aligning it down, there will be some bytes that no longer belong to the top chunk. What happens to them? The fencepost can not absorb them as it would disturb their alignment.

## Ques: In the unsorted bin pathway (path-3) in \_int\_malloc, the victim must be an exact fit or the last_remainder. Otherwise, it is binned. Why?

It is possible that a large chunk is there and is suitable to satisfy the request. But we bin every chunk only to rediscover them later through regular bin search. Why every chunk is not given an equal opportunity?
