# Range adaptors and `const`

Ranges can be readable (i.e. they model `std::ranges::input_range`) and/or writeable (i.e. they model `std::ranges::output_range`). [^elements]
While **write-only** ranges are possible, in practice you will likely encounter only ranges that are readable+writeable (*mutable ranges*, **mut**) or read-only (*constant ranges*, **con**).

Adding `const` to a type typically switches it from **mut** to **con**, e.g. `std::vector<int>` is mutable range, but `std::vector<int> const` is a constant range.

We will see below that this is not always the case, and that substantial knowledge is required to understand how `const` affects standard library range adaptors.

## Overview

Here are some examples from the standard library: [^notation]

| `T`:                                            |  `T`  | `T const`       |
|-------------------------------------------------|:-----:|:---------------:|
| `std::vector<int>`                              |  mut  |  con            |
| `std::string_view`                              |  con  |  con            |
| `std::span<int>`                                |  mut  |  mut            |
| `std::vector<int>&  │ std::views::reverse`      |  mut  |  mut            |
| `std::vector<int>&& │ std::views::reverse`      |  mut  |  con            |
| `std::vector<int>&  │ std::views::filter(/**/)` |  mut  |  n/a[^notrange] |
| `std::vector<int>&& │ std::views::filter(/**/)` |  mut  |  n/a            |

We observe three different semantics here:
* `const` turns **mut** into **con**
* `const` has no effect
* `const` renders the object unusable (it is no longer a range)

And here are the respective examples from this library:[^string_view]

| `T`:                                         |  `T`  | `T const` |
|----------------------------------------------|:-----:|:---------:|
| `std::vector<int>`                           |  mut  |  con      |
| `radr::borrowing_rad<int*>`                  |  mut  |  con      |
| `std::vector<int>&  │ radr::reverse`         |  mut  |  con      |
| `std::vector<int>&& │ radr::reverse`         |  mut  |  con      |
| `std::vector<int>&  │ radr::filter(/**/)`    |  mut  |  con      |
| `std::vector<int>&& │ radr::filter(/**/)`    |  mut  |  con      |

We only have one semantic. If this already convinces you, you do not need to read further :wink:


## Can we make it `const`?

A (non-const) range type is called "const-iterable" if it is also a range once accessed via a `const` object or reference to `const`.
Some ranges are plausibly not const-iterable: *single-pass ranges*, e.g. `std::views::istream` which pulls characters out of a stream.
They (observably) change state when you iterate over them, so making them const-iterable would be a terrible design.

On the other hand, *multi-pass ranges* (e.g. containers) are required by definition to always yield the same elements.
This implies that no observable change happens to the elements simply by iteration.
Still, many multi-pass range adaptors from the standard library are not const-iterable, i.e. you cannot use them through a `const &`.

Here's an example:

```cpp
void print(auto const & range)
{
    std::println("{}", range);
}

std::vector vec{1, 2, 3};
auto v = vec | std::views::filter(is_even);

// print(v); // ill-formed; v cannot be printed.
```

`std::views::filter` always behaves in the described way, but it is not a property of each adaptor per se.
Some standard library range adaptors behave like this only in certain combinations:

| `T`                                                                      |  `T`  | `T const` |
|--------------------------------------------------------------------------|:-----:|:---------:|
| `std::vector<int>& │ std::views::reverse`                                |  mut  |  mut      |
| `std::vector<int>& │ std::views::take_while(/**/)`                       |  mut  |  mut      |
| `std::vector<int>& │ std::views::take_while(/**/) │ std::views::reverse` |  mut  |  n/a      |

Here you can see that `reverse` and `take_while` are const-iterable on their own, but they are not when combined.

The reason for this behaviour is a side-effect of the strategy employed for caching the `begin()` of the range adaptor.
`std::views::filter` caches when `begin()` is called; thus the object is changed and there is no `.begin() const`; and it isn't const-iterable.
For `std::views::reverse` it depends on whether the underlying range whether it caches or not:
It does not cache for `std::vector`, but it does for `take_while_view`.
This is incredibly difficult to learn and teach, and makes `const &` unusable in generic range contexts.

We believe that this is a core problem with the standard library design.
Printing the elements of a filtered vector is by all sensible definitions an operation without mutable state.
It should be doable via a function that takes its argument as `const &`!

To solve this problems, the RADR library employs a different strategy for caching.
**All our range adaptors on multi-pass ranges are const-iterable.**
See [the page on caching begin](./caching_begin.md) for more details.
We also believe that const-iterability is a discerning feature of multi-pass ranges in contrast to single-pass ranges.
See [Terminology](./getting_started.md#Terminology) and [Fundamental Range Properties](./range_properties.md).

## What effect does `const` have?

For those ranges and range adaptors that are const-iterable, we observe two different behaviours in the standard library:

1. "deep const": The range behaves like a container; `const` turns the range into a *constant range*.
2. "shallow const": The range behaves like a pointer; `const` does not protect the elements; the range remains *mutable*.

This is an example of shallow const:

```cpp
// The following is well-formed; you can assign through span const &
void write1(std::span<int> const & s)
{
    s[0] = 42;
}
```

The choice for `std::span` (and subsequently the adaptors in `std::ranges`) to behave in this way was based on the assumption that it replaces "a pointer + size".
But this reasoning misses the point that the *way* `std::span` is used is not the way other shallow types are used, i.e. no `operator*` is needed to follow the indirection.
Instead, the usage patterns are exactly the same as for containers.

To make matters more confusing, when standard library range adaptors where changed to allow owning data (effectively becoming containers),[^what_is_a_view] the behaviour of these owning adaptors was modelled after containers.
This leads to `std::vector<int>& | radr::reverse` resulting in a type with "shallow const" semantics (read-write), and `std::vector<int>&& | std::views::reverse` resulting in a type with "deep const" semantics (read-only).

Fundamentally, there is no silver bullet, because the range concept covers types that manage dynamic memory and those that do not.
However, we believe that "shallow const" for ranges provides few practical benefits and that the current situation in the standard library needlessly prevents average users from knowing what the capabilities and properties of their composed range objects are.

Thus, we have chosen to follow the principle of least surprise and make all multi-pass ranges and range adaptors in our library behave like containers ("deep const").

## Footnotes


[^elements]: In this document, we are always referring to the readability and writeability of the elements of the range. Whether or not a range object can itself be re-assigned is not covered.

[^string_view]: We have omitted `std::string_view` from the list. Using it with our library is absolutely fine. To get a mutable string_view, you can use `radr::borrowing_rad<char*>`.

[^notrange]: This is not a range at all. You cannot do anything with it.

[^notation]: We using some very liberal pseudo-code here to describe "lvalues of a vector" and "rvalues of a vector" as inputs to a range adaptor.

[^what_is_a_view]: This design change happened after the release of C++20 (https://wg21.link/p2415), but was applied as a defect report back into C++20. We think that owning range adaptors are useful in general, but believe this particular change to have had some undesirable ripple-effects.
