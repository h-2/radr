# 📡 C++ ℝange 𝔸𝕕aptors ℝeimagined (radr)

This library explores an opinionated re-design of C++ Range Adaptors, commonly referred to as "views". It tries to reduce complexity in the conceptual space and provide a better, more consistent user experience.
At the same time, the usage patterns and naming remain close enough to the standard library to be used almost as a drop-in replacement.

## 🪞 Differences and similarities to std::ranges

```cpp
auto adapted_range  =  original_range  |  range_adaptor_object  |  range_adaptor_object2;
```

The general pattern for creating adapted ranges is the same in our library and in the standard library.
However, we aim to establish clearer rules for what you can and cannot do with the `adapted_range`.
To achieve this, we are sometimes stricter about what the `original_range` needs to provide.


### ⌨️ Summary for the casual C++ programmer

* [Similar usage patterns](./docs/getting_started.md) to the standard library.
* Fewer surprises: range adaptors on containers (probably most that you use) behave a lot more like containers, e.g. you can [default-construct them, compare them, copy them](./docs/regular.md) and [pass them by `const &`](./docs/const.md) (this is not true for many standard library adaptors).
* Fewer footguns: you are less likely to return [dangling references](./docs/safety.md) and cause certain forms of *undefined behaviour*; [`const` will protect you](./docs/const.md) from changing a range and its elements.
* [Simpler types](./docs/simpler_types.md) and better error messages (at least we are trying :sweat_smile:).
* Less confusion: you don't need to understand what a "view" is, because it is irrelevant for this library.

### 🤓 Summary for the Ranges nerd

This library [fundamentally differentiates between multi-pass and single-pass ranges](./docs/range_properties.md).

1. multi-pass ranges:
  * Adaptors return one of two class templates: `radr::owning_rad` or `radr::borrowing_rad` for owning/borrowing ranges respectively.
  * The semantics are implemented completely in terms of an iterator-sentinel pair [which is always stored/cached](./docs/caching_begin.md).
  * This means adaptors on borrowed ranges are also borrowed ranges, and no "view" concept is required.
  * Adaptors never contain mutable state, are always const-iterable and calling `.begin()` (non-const) has no side effects.
2. single-pass ranges:
  * Adaptors on single-pass ranges always return a `std::generator`.
  * This results in simpler types, fewer nested template instantiations, and avoids lots of special cases in the multi-pass implementations.


## 📖 Further reading

**Please have a look at the docs folder.**
In particular, you may be interested in:

* [Getting started](./docs/getting_started.md): short introduction on how to use this library and the terminology used in the documentation.
* [Implementation status](./docs/implementation_status.md): overview of which adaptors are already available.
* [Examples](./docs/examples.md): examples that illustrate standard library usage vs radr usage ("tony tables").
* [Trade-offs](./docs/tradeoffs.md): things to be aware before switching to this library.

## 🗒️ Library facts

* Easy to use: header-only and no dependencies
* License: Apache with LLVM exception[^boost]
* Compilers: GCC≥11 or Clang≥17
* Standard libraries: both libstdc++ and libc++ are tested.
* C++20 required.[^std]

[^boost]: The file `generator.hpp` is licensed under the Boost Software license. It is used only if your standard library does not provide `std::generator`.

[^std]: A bonus of using this library is getting access to the equivalent of C++23 and C++26 adaptors in a C++20-based codebase.

## 👪 Credits

Not everything presented here is novel—in fact, many of the ideas are based on older "ranges" designs (e.g. old ISO papers, Boost ranges and old range-v3).
One "innovation" is applying different rules to multi-pass and single-pass ranges, and one important decision is opting out of the whole "What is a view" discussion.

This library uses code from the standard library implementation of the LLVM project. It also uses a draft implementation of `std::generator` from https://github.com/lewissbaker/generator.

I want to thank various members of the DIN AK Programmiersprachen for thoughtful discussions on the topic.


