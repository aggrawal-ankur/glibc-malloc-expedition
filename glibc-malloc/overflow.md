_int_malloc is the gateway to the core architecture 
that deals with syscalls and stuff. Therefore, the 
first thing we do is checking if the requested bytes 
are valid.
- If the requested bytes are greater than PTRDIFF_MAX, 
  we can not service this request. We set errno and 
  return NULL.
- While it might look easy on surface, to understand 
  it better, such that, there are no confusions left 
  whatsoever, we have to explore pointers a little bit.

# Overflow Mechanics

A pointer is a variable that contains a memory address. A 
memory address is a regular stream of numbers interpreted
specially. But if we remove the interpretation part, it 
just a number inside a variable.

C is statically typed, so every variable has a type. We 
have a few primitive types and a handful of aliases (type 
definitions) to them. They exist to improve how humans 
interpret a variable. For example, a variable of type 
`pid_t` contains a process identifier, but under the hood, 
it is just an `int`.

A type can be signed or unsigned. Unsigned types manage 
positive integers only, while signed types can manage 
both positive and negative numbers.

Each data type has a width, which represents the largest 
number a type can manage. The number of bits is what the 
width of a container is.
- Because unsigned integers only grow upward, we don't 
  talk about the smallest value possible. But that's not 
  true for signed types.
- A signed type has both largest and smallest possible 
  values.

Signed types come with a problem. They carry an extra piece 
of information, i.e. signedness. We can't magically say if 
a signed integer is positive or negative. The number must 
carry its signedness, as they generally do on paper. To make 
this happen, we utilize the MSB to represent the signedness 
of a number and the remaining bits are used for magnitude. 
This reduces the magnitude that a signed type can manage, but 
it allows signed types to exist properly.

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

What happens when try to stuff data beyond its limit? An 
overflow occurs.
  - What happens in an overflow? The data exceeds the 
    limits of the container, so it wraps around the 
    other side of the range.
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


A pointer variable is different than a normal variable 
in a way that its width is decided by the hardware. It 
doesn't matter if we create an int*, or a char*, every 
pointer is 8-bytes wide on 64-bit.

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
    which is an alias to `long` on GNU/Linux. The 
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

