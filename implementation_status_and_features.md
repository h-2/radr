# Implementation status

## Work-in-progress

| Range adaptors             | Impl | C++XY | Tests | Equivalent in `std::`   | Remarks                                  |
|----------------------------|:----:|:-----:|------:|-------------------------|------------------------------------------|
| `radr::as_const`           |  ✔   | 20/23 | ✔     | `std::views::as_const`  | potentially different behaviour in C++23 |
| `radr::as_rvalue`          |  ✔   | 20/23 | ✔     | `std::views::as_rvalue` | returns only input ranges in C++20       |
| `radr::drop(n)`            |  ✔   | 20    | TODO  | `std::views::drop`      |                                          |
| `radr::drop_while(fn)`     |  ✔   | 20    | TODO  | `std::views::drop_while`|                                          |
| `radr::filter(fn)`         |  ✔   | 20    | ✔     | `std::views::filter`    |                                          |
| `radr::reverse`            |  ✔   | 20    | ✔     | `std::views::reverse`   |                                          |
| `radr::slice(m, n)`        |  ✔   | 20    | TODO  | *not yet available*     | get subrange between m and n             |
| `radr::take(n)`            |  ✔   | 20    | TODO  | `std::views::take`      |                                          |
| `radr::take_exactly(n)`    |  ✔   | 20    | TODO  | *not yet available*     | turns unsized into sized                 |
| `radr::transform(fn)`      |  ✔   | 20    | ✔     | `std::views::transform` |                                          |
| `radr::make_single_pass`   |  ✔   | 20    | ✔     | *not yet available*     | demotes range category to input          |

C++XY denotes the required C++ standard. Multiple years imply different feature set and/or behaviour depending on
standard.

| Standalone ranges          | Impl | C++XY | Tests | Equivalent in `std::`   |
|----------------------------|:----:|:-----:|------:|-------------------------|
|                            |      |       |       |                         |

Note that most of our standalone ranges are not implemented as "factory" objects, but just as plain types.


| Customisation points               | tag                            | Remarks                                         |
|------------------------------------|:------------------------------:|-------------------------------------------------|
| `radr::subborrow(r, it, sen[, s])` | `radr::custom::subborrow_tag`  | Used when creating subranges from other ranges  |



## Feature table

**Range adaptor objects:**

| Range adaptor              | $O(n)$ constr | min cat | max cat  | sized | common    | Remarks                                  |
|----------------------------|:-------------:|---------|----------|:-----:|:---------:|------------------------------------------|
| `radr::as_const`           |               | input   | contig   |  =    |  =        | potentially different behaviour in C++23 |
| `radr::as_rvalue`          |               | input   | input/ra |  =    |  =        | returns only input ranges in C++20       |
| `radr::drop(n)`            |  yes          | input   | contig   |  =    |  🟰        |                                          |
| `radr::drop_while(fn)`     |  yes          | input   | contig   |  🟰    |  🟰        |                                          |
| `radr::filter(fn)`         |  yes          | input   | bidi     |  -    |  =        |                                          |
| `radr::reverse`            | on non-common | bidi    | ra       |  =    |  +        |                                          |
| `radr::slice(m, n)`        |               | input   | contig   |  =    |  =        | get subrange between m and n             |
| `radr::take(n)`            |               | input   | contig   |  =    |  ra+sized |                                          |
| `radr::take_exactly(n)`    |               | input   | contig   |  +    |  ra+sized | turns unsized into sized                 |
| `radr::transform(fn)`      |               | input   | ra       |  =    |  =        |                                          |
| `radr::make_single_pass`   |               | input   | input    |  -    |  -        | demotes range category to single-pass    |

**min cat** underlying range required to be at least input (`input_range`), fwd (`forward_range`), bidi (`bidirectional_range`),
ra (`random_access_range`) or contig (`contiguous_range`)<br>
**max cat** maximum category that is preserved<br>
`-` means property is lost<br>
`=` means property is preserved<br>
`+` means property is gained (this is rare)<br>
fancy unicode symbols indicate differences from the standard library adaptors

Range adaptors in this library return a specialisation of one of the following types:
  * `std::generator` if the underlying range is single-pass
  * `radr::borrowing_rad` if the underlying range is multi-pass, const-iterable and borrowed,
  * `radr::owning_rad` if the underlying range is multi-pass and const-iterable,
  * otherwise the call is ill-formed

There are no distinct type templates per adaptor (like e.g. `transform_view` for `views::transform` in the standard library).
If you absolutely need to operate on a multi-pass range that is not const-iterable, demote it to a single-pass range via
`radr::make_single_pass`.
