# Exploring glibc malloc 2.43

An in-depth exploration of the virtual memory allocator in the GNU C Library (glibc 2.43).

The repository combines static source code analysis with small, reproducible experiments that validate the observed behavior. Rather than describing allocator behavior from secondary sources, every explanation is derived directly from the implementation and, where appropriate, verified through dynamic analysis.

---

## Repository structure

```
annotated-src/
    Annotated copies of malloc.c and arena.c.

upstream-src/
    Original source files from the official glibc 2.43 release.

design/
    Design documents explaining the allocator's data structures, algorithms, and internal architecture.

dynamic-analysis/
    Small reproducible experiments used to validate observations made during source code analysis.

linked-lists/
    Small programs used to understand the implementation of the allocator's linked-list based data structures.

open-questions.md
    Questions that remain unanswered after studying the implementation.
```

---

## Reading guide

The repository is intended to be read progressively. While every document can be read independently, the following order builds the necessary mental model with the fewest assumptions.

### 1. `malloc_chunk`

The allocator revolves around the `malloc_chunk` structure. Every allocation, free operation, and bookkeeping mechanism ultimately operates on chunks. It is explored in [design/malloc_chunk.md](design/malloc_chunk.md)

---

### 2. Size model

Once chunks are understood, the next step is understanding how glibc represents sizes, alignment, and allocator constraints. It is explored in [design/size-model.md](design/size-model.md).

---

### 3. `bins[]`

The allocator's free-space management revolves around the `bins[]` array.

[design/bins.md](design/bins.md) explains:
  - the different classes of bins,
  - how chunks move between them,
  - why the organization exists.

---

After these documents, start with the annotated malloc.c.

The annotations contain references to the remaining design documents wherever additional background is required.

The source is intended to be read from the beginning, avoiding the tcache related code in the beginning until the core data structures and pathways that build on them are understood.

Once the core architecture is understood, the tcache layer can be explored.

---

## Dynamic analysis

Many design documents conclude with one or more experiments.

These are not demonstrations after the fact. They are part of the exploration itself. Whenever the implementation suggested a particular behavior, a minimal experiment was written to verify that understanding before documenting it.

These experiments are categorized under chunks and bins in `dynamic-analysis/`.

---

## Reproducing the environment

The repository includes a reproducible environment for building and experimenting with glibc 2.43.

The setup downloads the official GNU glibc 2.43 release tarball, builds a custom copy of glibc, and prepares the environment used throughout the experiments.

Open [dynamic-analysis/](dynamic-analysis/) to access the instructions to setup the environment.

---

## Notes

The custom build disables the tcache infrastructure. This allows the allocator's core architecture to be understood without the additional complexity introduced by the per-thread cache.

Once the core allocator has been understood, the tcache layer can be studied as an extension rather than as an intertwined implementation detail.

---

## Methodology

Every explanation in this repository follows the same process:

1. Read the implementation.
2. Form a hypothesis.
3. Construct the smallest possible experiment to verify it.
4. Record both the explanation and the supporting evidence.

The intent is to keep every conclusion traceable to either the implementation, an experiment, or both.

---

## License

The original glibc source files remain under their original license.

All annotations, design documents, experiments, and accompanying documentation are original work.
