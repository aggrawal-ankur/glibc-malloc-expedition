# Overflow Mechanics

C is statically typed, so every variable has a type.

We have a few primitive types and a handful of aliases (type definitions) to them. Aliases improve how humans interpret a variable. For example, a variable of type `pid_t` contains a process identifier, but under the hood, it is just an `int`.

Each data type has a width, which is the number of bits used to represent a value in that type.

A data type can be signed or unsigned. Unsigned types manage positive integers only, while signed types manage both positive and negative integers.

Note that unsigned and signed are just interpretations of the same bits.

Computers use 2s complement to implement the signed interpretation of integers, where the unsigned range for a set of bits is divided in two equal halves and the upper half is used to represent negative integers.
  - The total number of combinations a set of bits can represent remains unchanged, only what those combinations could be, is changed.
  - When a number is interpreted as signed, its MSB carries a negative weight and the result interprets both the positive and negative integers properly.

---

The total number of combinations a set of bits can represent are 2<sup>n</sup>, where n is the number of bits.

The range of values a set of bits can represent is determined by the interpretation. (n is the number of bits)
  - Unsigned Interpretation: [0, (2<sup>n</sup> - 1)]
  - Signed   Interpretation: [(-2)<sup>(n-1)</sup>, 2<sup>(n-1)</sup> - 1]

The logic behind these formulas is very simple.
  1. If `m` is the total possible combinations, these combinations can be counted starting from anywhere. If we start from 0, we have to exclude `m` itself, otherwise, the total combinations will be (m+1), not m.
  2. 2<sup>n</sup> is always equal to the sum of all values of n (in [0, (n-1)]) raised to a power-of-2. Therefore, the MSB represents all the combinations in the upper half, so (n-1) is used instead of n while calculating the range of values in both the halves.

For a 64-bit container (or, a set of 64 bits), these ranges are:

  - Unsigned: [0, (2<sup>64</sup> - 1)],
              i.e. [0, 18446744073709551615],
              or [0, SIZE_MAX]
  - Signed:   [-(2)<sup>63</sup>, 2<sup>63</sup> - 1], 
              i.e. [-9223372036854775808, +9223372036854775807]

---

What happens when we put a value that is not in the range of values of a type? For examples,
  - an unsigned 4-bit container manages [0, 15] and we put 16 in it, or
  - a signed 4-bit container manages [-8, +7] and we put 10 in it.

We can notice that the range of positive values that a signed container can represent is basically a subset of the equivalent unsigned container. This insight holds the answer to the question above.

In unsigned interpretation, the range of values align exactly with what the bits can represent because we are counting from zero.
  - If the number is beyond this range, the number requires a bit that is not available to this container.
  - So, the extra bit(s) are discarded and the available bits are retained. This is what modulo 2<sup>n</sup> wraparound is.

In signed interpretation, the range of values doesn't align exactly with what the bits can represent because we have changed the meaning of combinations in the upper half.
  - The number we are trying to put in a signed container might be invalid according to its range, but it could be valid in the equivalent unsigned container.
  - That's what the above example is. 10 is valid as a 4-bit number, which is what the unsigned interpretation is. But the signed interpretation simply doesn't have 10, so it becomes invalid. The bit pattern is the same, only the interpretation is different.

In unsigned interpretation, overflow is characterized by physical spillage. In signed interpretation, overflow happens when the result can not be represented by the applicable part of the signed range and because the upper half of the unsigned range is conceptually split to represent negative integers, overflow appears to be wrapping around the other side of the signed range. But as we have seen, it is just a consequence of what is said earlier.

So, if 16 is put in an unsigned 4-bit container, it will be interpreted as 0 and if 10 is put in a signed 4-bit container, it will be interpreted as -6.

How about putting 16 in a signed 4-bit container? 16 is not valid even in the unsigned range.
  - 15 will be `1111`, and (-8 + 7) is -1.
  - 16 will be `1 0000`, and the output will be 0.
  - 17 will be `1 0001`, and the output will be 1.
  - And so on....

---

A pointer variable is special in a way that it is interpreted specially. In the end, it also contains a number just like non-pointer variables.

Another thing that makes pointers different from non-pointer variables is that their size is determined by the hardware, regardless of the type it is created for. An `int*` and a `char*` have equal widths, the only difference is in the value the address they contains points to. This is true for flat memory architectures, like x64 and ARM64.

Pointer arithmetic defines operations on pointers. They include
  1. addition/subtraction of an integer from a pointer,
  2. subtraction of two pointers, and
  3. comparison of two pointers.

Increment/decrement are basically addition/subtraction under the hood.

---

Take two non-equal pointers p1 and p2, such that p2>p1. We can do (p1-p2) or (p2-p1). The magnitude will be the same, only the sign will be different; (p2-p1) will be positive and (p1-p2) will be negative.

The output of pointer subtraction is a number which is not a pointer. Which container is most suitable to store this output?
  - The width is fixed by the hardware, so 64-bit on x64.
  - Because the output can be negative as well, we need a signed container.

So, a 64-bit signed container is the most suitable option to store pointer difference.

The C standard mandates a type called `ptrdiff_t`, which is a signed 64-bit type (an alias to `long`) on LP64 GNU/Linux. The range of this type is [PTRDIFF_MIN, PTRDIFF_MAX], which are macros to [-9223372036854775808, +9223372036854775807].

As long as (p2-p1) doesn't exceed PTRDIFF_MAX, and (p1-p2) doesn't fall below PTRDIFF_MIN, the difference will be valid as per the signed 64-bit interpretation.

If we add 1 to PTRDIFF_MAX and interpret it as signed (%ld), we get -9223372036854775808, i.e. PTRDIFF_MIN. If we interpret it as unsigned (%lu), we get +9223372036854775808, as the unsigned interpretation uses all the bits for magnitude.

# How the allocator deals with it?

The symbol `malloc` is a weak alias to `__libc_malloc`. When __libc_malloc is called, it checks if the thread-cache infrastructure is available (via the USE_TCACHE macro).

If it is, it checks if a tcache-bin can be used to service the request. Otherwise, `__libc_malloc2` is called and its output is returned. `__libc_malloc2` calls `_int_malloc` and returns its output.

`_int_malloc` orchestrates the core architecture (chunks, bins, sbrk and mmap) and the tcache layer (if, active). It receives the requested bytes in a size_t container.

Before any allocation pathway is accessed, the requested size is aligned as per the size model. While doing this, the allocator checks the possibility of the size being so enormous that the system might fail to fulfill the request.

Because `size` comes in a size_t container, SIZE_MAX feels like the natural upper bound. However, the allocator uses PTRDIFF_MAX instead, and the reason is **pointer subtraction**.

Recently, we have understood that the difference of two pointers can be positive or negative, so it is a signed quantity.
  - If we allocate an object which is so enormous that two pointers inside that object yield a difference which overflow's the signed 64-bit interpretation, we will not be able to represent the result safely.
  - What are the two pointers in an object that can yield the largest possible difference (magnitude only)? It is the pointers that mark the start and end of the object, i.e. [start, (start+size-1)].

If size was more than PTRDIFF_MAX, the largest pointer difference can not be managed by `ptrdiff_t` and the request is rejected. For this reason, PTRDIFF_MAX is the upper ceiling for a request size to be valid.

---

By validating size against PTRDIFF_MAX, _int_malloc sets the fundamental precondition that every allocation path relies on. After this point, the allocator assumes the request represents a valid C object and no longer revalidates that property.


# References

[ISO/IEC 9899:202y — N3854 working draft](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n3854.pdf)
  - Section 6.5.7 Additive operators
  - Page 88, Point 10.
