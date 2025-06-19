# Getting started

The usage patterns of this library are almost identical to the standard library.

* Just replace `std::views::FOO` with `radr::FOO`; see below for slight differences in range capture.
* You can use all functions, algorithms and concepts from `std::ranges::` on our types.
* Optionally, you can use `radr::begin()` / `radr::end()` instead of `std::ranges::begin()` / `std::ranges::end()`.

## Capturing the underlying range

```cpp
/* temporaries */
auto vue0 = std::vector{1, 2, 3} | std::views::take(2); // standard library
auto rad0 = std::vector{1, 2, 3} | radr::take(2);       // this library

/* move existing */
std::vector vec1{1, 2, 3};
auto vue1 = std::move(vec1) | std::views::take(2);      // standard library
auto rad1 = std::move(vec1) | radr::take(2);            // this library

/* refer to existing borrowed range */
std::span s = vec1;
auto vue2 = s | std::views::take(2);                    // standard library
auto rad2 = s | radr::take(2);                          // this library

/* refer to existing container */
std::vector vec2{1, 2, 3};
auto vue3 =          vec2  | std::views::take(2);       // standard library
auto rad3 = std::ref(vec2) | radr::take(2);             // this library      ← ONLY DIFFERENCE!
```

As you can see, only the syntax for creating an indirection is slightly different, i.e. when adapting an existing container, you need to explicitly state whether you want to `std::move()` or `std::ref()` it.
This is a bit more verbose, but allows a cleaner design and avoids unintended dangling references. See the [this example](./safety.md) for more details.

**We recommend not mixing our adaptors with standard library views.**
See [this page](./range_properties.md#Caveat) for more details.

## Terminology

We fundamentally differentiate three types of ranges:

1. Multi-pass ranges that own their elements ("containers").
2. Multi-pass ranges that do not own their elements ("borrowed ranges").
3. Single-pass ranges ("generators").

Multi-pass ranges can be accessed multiple times while single-pass ranges cannot generally be read again from the beginning.
See [Range properties](./range_properties.md) for formal definitions.

A *range adaptor* is a range that is created on top of another range, typically referred to as the *underlying range* or
the *original range*.
Our range adaptors can be created on any of these three categories, and will result in an adapted range of the same
category, except that `std::ref()`-ing a container leads to a borrowed range (see the last example above).

In particular, it should be noted that *range adaptors* are not a separate category and no range properties are specific to them; e.g. an adaptor on a `std::string_view` (a borrowed range) is also just a borrowed range—while an adaptor on a `std::vector` (first example above) is also just a "container".
We do not use the term "view" and the formal and informal definitions of "view" and "viewable range" are irrelevant for this library.
