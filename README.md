# C++ 𝗥ange 𝗔𝐝aptors 𝗥eimagined

This library explores a different design for C++ Range Adaptors, commonly referred to as "Views". It tries to reduce complexity in the conceptual space and provide a better, more consistent user experience.
The library only replaces certain components from the Standard's Ranges library, and aims to maintain a sufficient level of compatibility with existing code.

## Difference and similarities to std ranges


### Summary for the casual C++ programmer

* Same usage patterns as the standard library,[^1] e.g. `std::vector{1,2,3} | radr::foo(bar)` instead of `std::vector{1,2,3} | std::views::foo(bar)`.
* Fewer surprises: range adaptors on containers (probably most that you use) behave a lot more like containers, e.g. you can default-construct them, compare them, copy them and pass them by `const &` (this is not true for many standard library adaptors).
* Fewer footguns: you are less likely to return dangling references, because references to existing ranges need to be created explicitly.
* Less confusion: you don't need to understand what a "view" is, because it is irrelevant for this library.

[^1]: See IMPLEMENTATION STATUS for which range adaptors have been implemented already.

### Summary for the Ranges nerd

This library fundamentally differentiates between multi-pass and single-pass ranges.

1. multi-pass ranges:
  * Adaptors return exactly one of two class templates: `radr::owning_rad` or `radr::borrowing_rad` for owning/borrowing ranges respectively.
  * The semantics are implemented completely in terms of an iterator-sentinel pair which is always stored/cached.
  * This means adaptors on borrowed ranges are also borrowed ranges, and no "view" concept is required.
  * Adaptors never contain mutable state, are always const-iterable and calling `.begin()` (non-const) has no side effects.
2. single-pass ranges:
  * Adaptors on single-pass ranges always return a `std::generator`.
  * This results in simpler types, fewer nested template instantiations, and potential optimisations through recursive yielding of elements.


<details>
<summary>

**Click here for more details**

</summary>

| Properties  |  multi-pass ranges <br> *owning* | multi-pass ranges <br> *borrowed*    |single-pass ranges <br> &nbsp; |
|-----------------------------|:-------------------------:|:---------------------------:|:----------------------:|
| example                     | `std::vector<int>`        | `std::string_view`          | `std::generator<int>`  |
| category                    | `forward_range` or better | `forward_range` or better   |  `input_range` only    |
| `borrowed_range`            | no                        | yes                         | no ❘ *irrelevant*      |
| iterating¹ w/o side-effects | yes                       | yes                         | no                     |
| const-iterable              | yes ❘ deep-const          | yes ❘ deep-const            | no                     |
| default-constructible       | yes                       | yes                         | no                     |
| copyable                    | yes ❘ `O(n)`              | yes ❘ `O(1)`                | no                     |
| equality-comparable²        | yes ❘ `O(n)`              | yes ❘ `O(n)`                | no                     |
|                             | ↓                         | ↓                           | ↓                      |
| radr adaptor returns        | `radr::owning_rad</**/>`  | `radr::borrowing_rad</**/>`³| `std::generator</**/>` |

<sup><sub>¹ calling `begin()` (non-const), dereferencing the returned iterator, and/or incrementing it<br>
² This property is not required by adaptors, but it is preserved if present.<br>
³ For some ranges more specialised adaptors are returned; this can be customised.
</sup></sub>

**The listed properties are both requirements on the underlying range and guarantees given by the range adaptors, i.e. ranges and their adaptations stay within a given domain.**
Some properties of borrowed ranges are intentionally designed to mirror those of owning ranges (deep-const, deep comparison).
This consistency is a core feature of the library which makes reasoning about it easier, but it also makes the code
more maintainable by reducing special cases.

Caveat: Forward ranges that do not meet the listed requirements are rejected by our library. Such types typically
only exist as the result of applying standard library views—which can easily be replaced by ours.
As a workaround, you can also demote a non-compliant forward range to a single-pass range via `radr::as_single_pass`,
in which case our adaptor accepts it and returns a generator.

</details>

### Simple usage

Range adaptors can capture ranges from *rvalues* and *lvalues*. Capturing **rvalues** works the same as in the standard library:

```cpp
/* temporaries */
auto vue0 = std::vector{1, 2, 3} | std::views::take(2); // standard library
auto rad0 = std::vector{1, 2, 3} | radr::take(2);       // this library

/* move existing */
std::vector vec{1, 2, 3};
auto vue1 = std::move(vec) | std::views::take(2);       // standard library
auto rad1 = std::move(vec) | radr::take(2);             // this library
```

But capturing **lvalues** is *explicit* in this library:

```cpp
std::vector vec{1, 2, 3};
auto vue =          vec  | std::views::take(2);         // standard library
auto rad = std::ref(vec) | radr::take(2);               // this library
```

This is a bit more verbose, but allows a cleaner design and avoids unintended dangling references. See the [this example](./comparison_tables.md#Safety) for more details.

## Further reading

* [Implementation status and feature table](./implementation_status_and_features.md): overview of which adaptors are available in the library.
* [Comparison tables](./comparison_tables.md): examples that illustrate standard library usage vs radr usage ("tony tables").


## Library facts

* Easy to use: header-only and no dependencies
* License: Apache with LLVM exception[^2]
* Compilers: GCC≥11 or Clang≥17
* Standard libraries: both libstdc++ and libc++ are tested.

[^2]: The file `generator.hpp` is licensed under the Boost Software license. It is used only if your standard library does not provide `std::generator`.

## Credits

This library relies heavily on the standard library implementation of the LLVM project. It also uses a draft implementation of `std::generator` from https://github.com/lewissbaker/generator.

I want to thank various members of the DIN AK Programmiersprachen for thoughtful discussions on the topic.


