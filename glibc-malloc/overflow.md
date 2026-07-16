_int_malloc orchestrates the core architecture (chunks, 
bins, syscalls) and the tcache layer (USE_TCACHE), which 
is like a fast path over the core architecture. Before 
these dedicated paths are accessed, we align the size as 
per the allocator's size and alignment model. But before 
that, we check if the requested bytes can be serviced in 
general.
- If the requested bytes are greater than PTRDIFF_MAX, 
  the request is very big to be serviced as a single 
  malloc and the kernel refuses it. We set errno and 
  return NULL. Otherwise, we call request2size.
  Note:
    1. The request can still be near PTRDIFF_MAX while 
       being <= PTRDIFF_MAX.
    2. The request has not overflowed as size belongs to 
       an unsigned container and the max limit is SIZE_MAX.
- While it looks simple on surface, to remove confusions, 
  we have to explore pointers a little bit.

# Overflow Mechanics

C is statically typed, so every variable has a type. We 
have a few primitive types and a handful of aliases (type 
definitions) to them. They exist to improve how humans 
interpret a variable. For example, a variable of type 
`pid_t` contains a process identifier, but under the hood, 
it is just an `int`.

A type can be signed or unsigned. Unsigned types manage 
positive integers only, while signed types can manage 
both positive and negative numbers.

Each data type has a width, which is the number of bits
used to represent values. Using this, we can obtain the
range of values a type can represent.
- Because unsigned integers only grow upward, we don't 
  talk about the smallest value possible. But that's not 
  true for signed types.
- A signed type has both largest and smallest possible 
  values.

Signed types come with a problem. They carry an extra 
piece of information, i.e. signedness. We can't magically 
say if a signed integer is positive or negative. The 
number must carry its signedness, as they generally do 
on paper.
- We use 2s complement, where the MSB carries a negative 
  weight when the number is interpreted as signed.
- This reduces the magnitude that a signed type can manage, 
  but it standardizes signed types.

To find the range of numbers a primitive type can manage, 
just raise the number of bits to the power of 2.
- Unsigned Types: [0, ((2^n) - 1)]
- Signed   Types: [-(2^(n-1)), ((2^(n-1)) - 1)]

For 64-bit types, the ranges are:
- Unsigned: [0, ((2^64) - 1)],
            [0, 18446744073709551615] 
            i.e. [0, SIZE_MAX]
- Signed:   [-(2^63), ((2^63) - 1)], 
            [-9223372036854775808, +9223372036854775807]

What happens when we try to stuff data beyond its limit?
The answer is different for signed and unsigned integers.
- For an unsigned integer, when the data goes beyond 
  SIZE_MAX, we need the 65th bit to represent it, which 
  we don't have. So we discard the 65th bit. But if the 
  data uses any of the 64 bits, it remains intact. For 
  example,
  - SIZE_MAX+1 only has the 65th bit ON and the rest of 
    the bits are 0.
  - But SIZE_MAX+2 has the 65th bit and the 1st bit 
    (counting from 0) ON, making the result 1, and so on.

- For a signed integer, the lower 63 bits carry a positive 
  weight, while the MSB carries a negative weight.
  - For a positive number, the MSB remains unset (0).
  - For a negative number, the MSB is set and it is 
    interpreted with a negative weight. Mathematically, the 
    weight of the MSB is subtracted from the lower 63 bits 
    generating the right negative number.
  - For example, (1) is 63 zeroes followed by a 1 and (-1) 
    is all the 64 bits 1.
    - The weight of the 64th bit (MSB) in signed terms is 
      -9223372036854775808.
    - The weight of the the lower 63 bits combined is 
      9223372036854775807.
    - (-9223372036854775808) + 9223372036854775807 is (-1).

Therefore,
- an unsigned overflow implies that the resulting value 
  can not be represented by the currently available 
  bits, and 
- a signed overflow implies that the resulting value does 
  fit the currently available bits, but it can not be 
  represented correctly.

An overflow occurs. What happens in an overflow? The data 
exceeds the limits of the container, so it wraps around 
the other side of the range.
- Examples:
  - Unsigned 64-bit:
    [1]: What happens if we subtract 1 from 0?
         ((size_t)(0) - 1) => 18446744073709551615, 
         : we get the maximum.
    [2]: What happens if we add 1 to the maximum 
         possible value?
         ((size_t)(SIZE_MAX) + 1) => 0.
         : we get the minimum.

  - Signed 64-bit:
    [3]: What happens if we subtract 1 from the minimum?
         ((ssize_t)(SIZE_MAX/2) - 1) => 9223372036854775807, 
         : we get the maximum.
    [4]: ((ssize_t)((SIZE_MAX/2)+1) + 1) => -9223372036854775808,
         : we get the minimum.


A pointer is a variable that contains a memory address. A 
memory address is a regular stream of numbers interpreted
specially. But if we remove the interpretation part, it 
just a number inside a variable.

Pointers differ from ordinary variables in that their 
size is determined by the hardware, regardless of the 
type it is created for. An `int*` and a `char*` have 
equal widths, the only difference is in the value the 
address they contains points to. This is true for flat 
memory architectures, like x64 and ARM64.

Pointer arithmetic defines operations on pointers. They 
include addition/subtraction of an integer from a pointer, 
subtraction of two pointers, and comparison. Increment 
and decrement are addition/subtraction under the hood.

Let's look at subtraction of two pointers.
  - Take two non-equal pointers p1 and p2, such that 
    p2>p1. We can do (p1-p2) or (p2-p1).
  - (p2-p1) will yield a positive output and (p1-p2) 
    will yield a negative output.
  - The output is a number which is not a pointer.
    Which container is the most suitable to store 
    this output? We need a signed 64-bit container so 
    that both the positive and negative output can be 
    stored.
  - The C standard mandates a type called `ptrdiff_t`, 
    which is an alias to `long` on LP64 GNU/Linux. The 
    range of this type is [PTRDIFF_MIN, PTRDIFF_MAX], 
    which are macros to what is discussed earlier. 
  - As long as (p2-p1) doesn't exceed PTRDIFF_MAX, and 
    (p1-p2) doesn't fall below PTRDIFF_MIN, overflow 
    will not occur.

If we add 1 to PTRDIFF_MAX and interpret it as 
  - signed (%ld), we get -9223372036854775808. The 
    output has wrapped around the other side of the 
    range.
  - unsigned (%lu), we get +9223372036854775808, as 
    the unsigned range uses all the bits for magnitude.

While it is impossible to allocate a size as huge as 
PTRDIFF_MAX to a single malloc request, suppose we did 
asked it. After request2size(sz), it will become 
9223372036854775824.


Overflow is not about pointers themselves. A pointer can
point to any addressable byte, which is why uintptr_t is
used for pointers. But there is no use of ptrdiff_t in
malloc.c, while there is a lot of pointer arithmetic
happening already.

The pointer difference here represents size, which is an
unsigned quantity. But signed doesn't mean "negative only",
it means "negatives along with positives".

mmap itself takes a size_t argument, which means, it can
effectively service a request > PTRDIFF_MAX, at least in
virtual memory, getting it backed by physical memory is
a different thing. So I still don't understand the point
of PTRDIFF_MAX. Wait!

Because mmap takes a size_t argument, the effective
ceiling for mmap is SIZE_MAX, not PTRDIFF_MAX. But sbrk
takes an inptr_t argument, ssize_t, basically. Here, the
limit is PTRDIFF_MAX. Because PTRDIFF_MAX itself is a
very high value, we can use it as a single check for
both sbrk and mmap, as mmap won't be able to service
such a request either.





See, I understand that the paths inside sysmalloc are stacked such that the request size reduces thru each pass. I don't understand how, though. Let's take sbrk. There is soft limit that glibc enforces, called MMAP_THRESHOLD. sbrk can't be used for a size above this. But with glossing look over the main arena path in sysmalloc, I can't find anything that ensures this.




GLIBC sets a soft limit on sbrk, that it can not
be used to service requests beyond MMAP_THRESHOLD.
But how it is enforced? I see no condition that
ensures this in the main arena pathway in sysmalloc.

Apart from this soft limit, can sbrk and mmap succeed
for any size < PTRDIFF_MAX? Provided that page alignment
math is ensured and the kernel has enough memory to give?

