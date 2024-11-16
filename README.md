# C++ 𝗥ange 𝗔𝐝aptors 𝗥eimagined

This library explores a different design for C++ Range Adaptors, commonly referred to as "Views". It tries to reduce complexity in the conceptual space and provide a better, more consistent user experience.
At the same time, the usage patterns and naming remain close enough to the standard library to be used almost as a drop-in replacement.

## Differences and similarities to std::ranges

```cpp
//                              ↓  what are the requirements on the original range?
auto adapted_range   =   original_range   |   range_adaptor_object   |   second_range_adaptor_object;
//      ↑    what are the properties of the new range?
```

The general pattern for creating adapted ranges is shown above. It is the same in our library as in the standard library.
However, we aim at providing clearer rules for what you can and cannot do with the `adapted_range`.
To achieve this, we are sometimes stricter about what the `original_range` needs to provide.


### Summary for the casual C++ programmer

* Similar usage patterns to the standard library.
* Fewer surprises: range adaptors on containers (probably most that you use) behave a lot more like containers, e.g. you can default-construct them, compare them, copy them and pass them by `const &` (this is not true for many standard library adaptors).
* Fewer footguns: you are less likely to return dangling references, because references to existing ranges need to be created explicitly.
* Less confusion: you don't need to understand what a "view" is, because it is irrelevant for this library.

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


## Further reading

**Please have a look at the Wiki wich has extensive documentation!**

In particular, you may be interested in:

* [Getting started]: short introduction how to use this library and the terminology used in the documentation.
* [Implementation status and feature table](./implementation_status_and_features.md): overview of which adaptors are available in the library.
* [Comparison tables](./comparison_tables.md): examples that illustrate standard library usage vs radr usage ("tony tables").


## Library facts

* Easy to use: header-only and no dependencies
* License: Apache with LLVM exception[^2]
* Compilers: GCC≥11 or Clang≥17
* Standard libraries: both libstdc++ and libc++ are tested.

[^2]: The file `generator.hpp` is licensed under the Boost Software license. It is used only if your standard library does not provide `std::generator`.

## Credits

This library uses code from the standard library implementation of the LLVM project. It also uses a draft implementation of `std::generator` from https://github.com/lewissbaker/generator.

I want to thank various members of the DIN AK Programmiersprachen for thoughtful discussions on the topic.


