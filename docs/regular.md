# Ranges and `std::regular`

When designing class types in C++, there are some operations that you need to think about, because they fundamentally determine how your users can interact with these types. These include the following:

1. default-construction
2. movability (move construction and move assignment)
3. copyability (copy construction and copy assignment)
4. equality comparison

A type that provides all of these (with sane semantics) is said to be *regular*, and a type that only provides the first three is said to be *semi-regular*. In C++20 there are concepts that encompass these properties: [`std::regular`](https://en.cppreference.com/w/cpp/concepts/regular) and [`std::semiregular`](https://en.cppreference.com/w/cpp/concepts/semiregular).

There are many more good resources on this topic [^references], but the points we'd like to make here are:
* (Semi-)regular types are much easier to use and reason about.
* It is generally recommended to make your types (semi-)regular if possible (see [Core Guidelines C.11](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Rc-regular)).
* *Containers* (the ranges that existed before `std::ranges::`) are (semi-)regular[^container_exceptions].

These are good reasons to make *range adaptors* also (semi-)regular, if possible.

[^references]: e.g. https://www.modernescpp.com/index.php/regular-type/ , https://www.stepanovpapers.com/DeSt98.pdf

[^container_exceptions]: All containers are semi-regular if the element type is semi-regular, and all containers are regular if the element type is regular. Many containers even model these properties if the element type does not meet all requirements.


## Semantics


|      op \ T                |  `std::string`                | `std::string *`  | `std::string_view` |
|----------------------------|-------------------------------|------------------|--------------------|
| `T s{};`                   | empty string                  | `nullptr`        | empty string       |
| `T s = std::move(other);`  | moves elements or rebinds ptr | rebinds ptr      | rebinds ptr        |
| `T s = other;`             | copies elements               | rebinds ptr      | rebinds ptr        |
| `s == other`               | compares elements             | compares address | compares elements  |

All three of the following are regular types: `std::string`, `std::string *`, `std::string_view`.

But the semantics of the discussed operations are different:
  * `std::string` has *container semantics*, i.e. copy and compare are in *O(n)*.
  * `std::string *` has *pointer semantics*, i.e. copy and compare are in *O(1)*.
  * `std::string_view` has an in-between position where copy is in *O(1)* but compare is in *O(n)*.

In contrast to `std::string *`, `std::string_view` is a range!

## Multi-pass ranges


| Standard library types                          |  `{}`            | move | copy          | compare         |
|-------------------------------------------------|:----------------:|:----:|:-------------:|:---------------:|
| `std::vector<int>`                              |   ✓              |  ✓   |  ✓            |  ✓              |
| `std::string_view`                              |   ✓              |  ✓   |  ✓            |  ✓              |
| `std::span<int>`                                |   ✓              |  ✓   |  ✓            | ❌ [^no_compare] |
| `std::vector<int>&  │ std::views::reverse`      |  ❌ [^no_default] |  ✓   |  ✓            | ❌ [^no_compare] |
| `std::vector<int>&& │ std::views::reverse`      |   ✓              |  ✓   |  ❌ [^no_copy] | ❌ [^no_compare] |

Although range adaptors were guaranteed to be at least semi-regular in range-v3 (the precursor to `std::ranges`), the standard library chose a different route.
This is mostly due to the committee not being able to agree on the desired semantics of the operations and then deciding to rather not provide them at all. Detailed explanations are given in the footnotes.

The result is that average programmers are no longer able to predict these properties.
Furthermore, generic code often has to accommodate these special cases.

[^no_compare]: Non-owning range adaptors were considered to be more like pointers-to-containers than containers, so the committee didn't want `==` to compare elements. This is the same design reason behind making them *shallow const* (see [the page on const](./const.md) ). However, one was (rightfully) afraid users wouldn't understand if `==` compared the underlying addresses instead of the elements, so we ended up with no `==`. RADR embraces the example of `std::string_view` and always compares elements for all adaptors.

[^no_default]: Since `std::ranges::ref_view` (the basis for non-owning standard library adaptors) does not store iterators (but a pointer to the range), a default-initialised object would not be an empty range but an invalid range, and it wouldn't be possible to invoke a zero-overhead implementation of `empty()` without undefined behaviour. RADR always stores iterators and default-initialised multi-pass iterators are valid and compare equal, so our default-initialised adaptors are always valid and calling `empty()` is well-defined and returns true.

[^no_copy]: `std::ranges::owning_view` (the basis for owning standard library adaptors) is not copyable, because of the way owning adaptors were introduced into the ranges machinery. Because not all non-owning adaptors in the standard are borrowed ranges, copyability is used as a marker for "owning" vs "non-owning". RADR is not affected by this limitation, because we use the `borrowed_range` concept to differentiate between "owning" and "non-owning".


| RADR types                                      |  `{}` | move | copy | compare |
|-------------------------------------------------|:-----:|:----:|:----:|:-------:|
| `std::vector<int>`                              |   ✓   |  ✓   |  ✓   |  ✓      |
| `std::string_view`                              |   ✓   |  ✓   |  ✓   |  ✓      |
| `radr::borrowing_rad<int*>`                     |   ✓   |  ✓   |  ✓   |  ✓      |
| `std::vector<int>&  │ radr::reverse`            |   ✓   |  ✓   |  ✓   |  ✓      |
| `std::vector<int>&& │ radr::reverse`            |   ✓   |  ✓   |  ✓   |  ✓      |

For this library we chose to provide a consistent behaviour:
* All adaptors on multi-pass ranges are at least semi-regular.
* If the element type is equality comparable, they are regular.
* Underlying ranges that would prevent our adaptors from being at least semi-regular (because they themselves lack certain properties) are rejected by our adaptors.[^reject]

[^reject]: Underlying ranges do not generally need to be regular for adaptors on them to be regular. See the respective documentation pages for `radr::owning_rad` and `radr::borrowing_rad`.

For owning adaptors, the semantics are modelled after containers. And for borrowing adaptors, they are modelled after `std::string_view`.
In particular, we always compare elements, and default-initialised range adaptors are fully-formed, valid, empty ranges.
Copying an owning adaptor copies the elements.[^copycontainer]

[^container]: There is a surprising complexity to implementing this generically. It works for all our adaptors on arbitrary containers, but it may not work on user-defined adaptor types unless they implement one of our customisation points. More documentation on this will be added in the future.

We realise that having an abstraction that covers both owning and non-owning ranges does not fit every type and use-case perfectly.
However, we also believe that not providing default-construction or copyability provides few benefits, and we generally prefer a consistent, predictable behaviour.

With `string_view` having been available in C++17, we believe there is a good precedent for chosing this as a role-model.

## Single-pass ranges

Single-pass ranges typically contain (or refer to) some resource that changes state upon iteration.
This makes them fundamentally different from containers and adaptors on containers.
Examples are: ranges that contain an iostream or a coroutine-based generator.
These resources are themselves often not default-constructible and/or not copyable.
In fact, not even the iterators of such ranges are required to be semi-regular.

Thus, it makes little sense to design single-pass ranges as (semi-)regular types.
All our single-pass ranges (including adaptors on single-pass ranges) are implemented as `std::generator`.
This results in all of them being non-default-initialisible and move-only (and has other benefits like type erasure).

It should be noted that we are not only improving consistency between ranges, we are also improving consistency between a range and its iterators.

## Footnotes



