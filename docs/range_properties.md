# Fundamental Range Properties


| Properties  |  multi-pass ranges <br> *owning* | multi-pass ranges <br> *borrowed*    |single-pass ranges <br> &nbsp; |
|--------------------------------|:-------------------------:|:---------------------------:|:----------------------:|
| example                        | `std::vector<int>`        | `std::string_view`          | `std::generator<int>`  |
|                                |                           |                             |                        |
| category                       | `forward_range` or better | `forward_range` or better   |  `input_range` only    |
| `borrowed_range`               | no                        | yes                         | no ❘ *irrelevant*      |
| iterating¹ w/o side-effects    | yes                       | yes                         | no                     |
| const-iterable                 | yes ❘ deep-const          | yes ❘ deep-const            | no                     |
|                                |                           |                             |                        |
| default-constructible          | yes                       | yes                         | no                     |
| copyable                       | yes ❘ `O(n)`              | yes ❘ `O(1)`                | no                     |
| equality-comparable²           | yes ❘ `O(n)`              | yes ❘ `O(n)`                | no                     |
|                                | ↓                         | ↓                           | ↓                      |
| radr adaptors return           | `radr::owning_rad</**/>`  | `radr::borrowing_rad</**/>`³| `std::generator</**/>` |

<sup><sub>¹ calling `begin()` (non-const), dereferencing the returned iterator, and/or incrementing it<br>
³ Equality-comparability is guaranteed iff the range elements are equality-comparable.<br>
³ For some ranges more specialised adaptors are returned; this can be customised.
</sup></sub>

**The listed properties define the three range domains and are guaranteed on our adaptors. Ranges and their adaptations stay within a given domain.**
This consistency is a core feature of the library which makes using it and reasoning about adaptors much easier.
But it also makes the code more maintainable by reducing special cases.
The semi-regularity properties (default-constructible, copyable) are guaranteed on our multi-pass adaptors even if the underlying range does not provide them.[^copy]
Equality-comparability is guaranteed if the elements are comparable (and independent of the underlying range's comparison operators).
All other listed properties, in particular const-iterability of multi-pass ranges, are a hard requirement.

Two facilities exist to explicitly switch between domains:
  * `std::ref(r) | ...` to create borrowed multi-pass adaptor from an owning range.
  * `... | radr::to_single_pass` to create a single-pass adaptor from multi-pass range.

In contrast to the standard library, several properties of borrowed multi-pass ranges are designed to mirror those of standard library containers: (semi-)regularity, deep-const, deep comparison.
See the [page on regular](./regular.md) and the [page on const](./const.md).

## Caveat

Not all ranges valid by standard C++ rules fit into the three domains previously defined (that's why this library is necessary!).
All standard library containers do, but, in particular, most *views* other than `std::string_view` do not meet the listed requirements.

In some cases, combining these views with our adaptors will work and we manage to uphold our guarantees (e.g. `std::span` and `std::ranges::empty_view` are not deep-const but our adaptors "fix" this); in other cases it will work but our guarantees might be violated (e.g. `std::ranges::transform_view` is not deep-const and we cannot always fix it); in other cases our library flat out rejects creating the adaptor (e.g. `std::ranges::filter_view` is not const-iterable which is a hard requirement for us).

**We strongly discourage mixing standard library *views* other than `std::string_view` with our library.**
Use our adaptors instead. Help us complete the list if a particular one is missing!

As a workaround to use our adaptors with a non-compliant forward range, you can demote that range to a single-pass range via `radr::to_single_pass`.
It will then be accepted by our adaptors but be in the single-pass domain.



[^copy]: There is some fine-print here. Copy-constructibility is required. Certain exotic iterators may need special handling.
