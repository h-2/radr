# C++ 𝗥ange 𝗔𝐝aptors 𝗥eimagined

This library explores a different design for C++ Range Adaptors, commonly referred to as "Views". It tries to reduce complexity in the conceptual space and provide a better, more consistent user experience.
The library only replaces certain components from the Standard's Ranges library, and aims to maintain a sufficient level of compatibility with existing code.

## Important differences from std::ranges

The following is a short overview of the main differences. See the Wiki for in-depth reasoning.

**Terminology, names and namespaces**

* We call the class (template) that adapts another range a *range adaptor* (short "rad"); `std::ranges::transform_view<>` → `radr::transform_rad<>`.
* Each range adaptor has an auxiliary object in the `radr::pipe::` namespace that enables composing in pipelines (via the `|`-operator); `std::views::transform` → `radr::pipe::transform`.
* In several places we use the term "bounds" to refer to the iterator-sentinel-pair of a range, e.g. `radr::range_bounds` is a range that stores another range's iterator-sentinel-pair (~ `std::ranges::subrange`).

**Range capture**

Range adaptors can capture ranges from *rvalues* and *lvalues*. Capturing **rvalues** works the same as in the standard library:

```cpp
/* temporaries */
auto vue0 = std::vector{1, 2, 3} | std::views::take(2);  // standard library
auto rad0 = std::vector{1, 2, 3} | radr::pipe::take(2);  // this library

/* move existing */
std::vector vec{1, 2, 3};
auto vue1 = std::move(vec) | std::views::take(2);  // standard library
auto rad1 = std::move(vec) | radr::pipe::take(2);  // this library
```

But capturing **lvalues** is *explicit* in this library:

```cpp
std::vector vec{1, 2, 3};
auto vue =          vec  | std::views::take(2);  // standard library
auto rad = std::ref(vec) | radr::pipe::take(2);  // this library
```

This is a bit more verbose, but allows a cleaner design and avoids unintended dangling references. See the [wiki-article on range capture](TODO) for more details.

**Range properties**

A major problem of standard library range adaptors is that it is difficult for users to predict which of a range's properties are preserved by the adaptor and which aren't.
This library divides ranges into two major domains and ties certain properties to these domains:


|                             |  single-pass ranges    | multi-pass ranges         |
|-----------------------------|:----------------------:|:-------------------------:|
| category                    |  `input_range` only    | `forward_range` or better |
| example                     | `std::generator<int>`  | `std::vector<int>`        |
| copyable                    | no                     | yes                       |
| iterating¹ w/o side-effects | no                     | yes                       |
| const-iterable              | no                     | yes                       |

<sup><sub>¹ calling `begin()` (non-const), dereferencing the returned iterator, and/or incrementing it<sup><sub>

A **single-pass range** is typically a generator or range that creates elements from a resource like a stream.
Iterating over the range changes it.
This library: range adaptors on single-pass ranges are always implemented as coroutines and are never copyable, const-iterable etc.

**Multi-pass ranges** are containers and similar ranges.
They can be copied, iterating over them does not change them, and it is thus also possible to iterate over a `const` container.
Standard library views do not promise to preserve any of these properties, e.g. `std::vector{1, 2, 3} | std::views::filter(/**/)` is not copyable, calling `.begin()` has side-effects, and it is not const-iterable.
This library: all properties are preserved, e.g. `std::vector{1, 2, 3} | radr::filter(/**/)` is copyable, const-iterable and iterating over it has no side-effects.

See the [wiki-article on range properties](TODO) to learn the trade-offs required for this design.


**Const-ness**

There are various peculiarities in the standard library regarding `const` and range adaptors.
These also have implications for thread-safety.

1. For several standard library views `foo_view<T> const` isn't a range (independent of `T`). In this library, `foo_rad<T> const` is a range iff `forward_range<T const>`. See also the previous section.
2. If a standard library `foo_view<T> const` is a range, it is inconsistent what the implications are. For some views, the underlying range is accessed as const ("deep const"), but for other views the const-ness does not propagate ("shallow const"), potentially allowing a call of the adaptor's `.begin() const` to have side effects. In this library, range adaptors are always deep const.

See the [wiki-article on const](TODO) for more surprising examples from the standard library and an in-depth explanation of our design.

## Compatibility

TODO

## Implementation status

We aim to replicate all standard library range adaptors and not much else.

**Borrowed ranges:**

|  Standard library             |  radr                   |  Remarks                 |
|-------------------------------|-------------------------|--------------------------|
| `std::span`                   | `std::span`             | *we don't replace this*  |
| `std::string_view`            | `std::string_view`      | *we don't replace this*  |
| `std::ranges::ref_view`       | `radr::range_ref`       | deep const               |
| `std::ranges::subrange`       | `radr::range_bounds`    | deep const               |

**Range adaptor objects:**

|  Standard library             |   radr                                            | Remarks                          |
|-------------------------------|---------------------------------------------------|----------------------------------|
| `std::views::drop`            | `radr::pipe::drop` ²                              |                                  |
| `std::views::filter`          | `radr::pipe::filter` ²                            |                                  |
| `std::views::take`            | `radr::pipe::take`                                |                                  |
| *not yet available*           | `radr::pipe::take_exactly`                        | turns unsized into sized         |
| `std::views::transform`       | `radr::pipe::transform`                           |                                  |
| *not yet available*           | `radr::pipe::make_single_pass`                    | reduces range category to input  |

Note, that our adaptor objects automatically dispatch single-pass ranges to the coroutine and multi-pass ranges
to the range adaptor template. Like in the standard library, some of them contain additional logic.

² These adaptors cache the begin iterator on construction (for some inputs) whereas the standard library equivalents
cache it on the first call of `begin()`. This makes the returned ranges in RADR const-iterable.

## Credits

This library relies heavily on the standard library implementation of the LLVM project. It also uses a draft implementation of std::generator from https://github.com/lewissbaker/generator.

I want to thank various members of the DIN AK Programmiersprachen for thoughtful discussions on the topic.
