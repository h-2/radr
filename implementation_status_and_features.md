# Implementation status

## Entity overview

| Range adaptors (classes)   | Equivalent in `std::`                                          | Remarks                                         |
|----------------------------|----------------------------------------------------------------|-------------------------------------------------|
| `radr::generator<>`        | `std::generator<>`                                             | custom implementation atm, but will be an alias |
| `radr::borrowing_rad<>`    | `std::ranges::subrange`, (`std:span`, `std::ranges::ref_view`) | stores (iterator, sentinel) pair                |
| `radr::owning_rad<>`       | `std::ranges::owning_view`                                     | stores rvalues of containers                    |

There are no distinct type templates per adaptor (like e.g. `std::ranges::transform_view` for `std::views::transform` in the standard library).
Instead all range adaptor objects in this library (see below) return a specialisation of one of the above types.

| Range adaptors (objects)   | Equivalent in `std::`   | Remarks                                  |
|----------------------------|-------------------------|------------------------------------------|
| `radr::as_const`           | `std::views::as_const`  | make the range *and* its elements const  |
| `radr::as_rvalue`          | `std::views::as_rvalue` | returns only input ranges in C++20       |
| `radr::drop(n)`            | `std::views::drop`      |                                          |
| `radr::drop_while(fn)`     | `std::views::drop_while`|                                          |
| `radr::filter(fn)`         | `std::views::filter`    |                                          |
| `radr::reverse`            | `std::views::reverse`   |                                          |
| `radr::slice(m, n)`        | *not yet available*     | get subrange between m and n             |
| `radr::take(n)`            | `std::views::take`      |                                          |
| `radr::take_exactly(n)`    | *not yet available*     | turns unsized into sized                 |
| `radr::transform(fn)`      | `std::views::transform` |                                          |
| `radr::make_single_pass`   | *not yet available*     | demotes range category to input          |

We plan to add equivalent objects for most standard library adaptors.


| Standalone ranges          | kind  | Equivalent in `std::`      | Remarks                                         |
|----------------------------|:-----:|----------------------------|-------------------------------------------------|
| `radr::empty_range<T>`     | class | `std::ranges::empty_view`  | a container of fixed size 0                     |
| `radr::single<T>`          | class | `std::ranges::single_view` | a container of fixed size 1                     |

Note that most of our standalone ranges are not implemented as "factory" objects, but just as plain types.


| Notable functions                  | CP   | Remarks                                              |
|------------------------------------|:----:|------------------------------------------------------|
| `radr::subborrow(r, it, sen[, s])` | ✔   | Used when creating subranges from other ranges      |
| `radr::subborrow(r, i, j)`         | (✔) | Position-based slice                                 |
| `radr::borrow(r)`                  | (✔) | `= radr::subborrow(r, r.begin(), r.end(), r.size()`  |
| `radr::borrow_single(v)`           | (✔) | like `radr::single` but doesn't own the element      |

CP denotes functions that you can customise for your own types, e.g. specify a different subrange-type for a specific container.

## Feature table

**Range adaptor objects:**

| Range adaptor              | $O(n)$ constr  | min cat | max cat  | sized | common    | Remarks                                  |
|----------------------------|:--------------:|---------|----------|:-----:|:---------:|------------------------------------------|
| `radr::as_const`           |                | fwd     | contig   |  =    |  =        | make the range *and* its elements const  |
| `radr::as_rvalue`          |                | input   | input/ra |  =    |  =        | returns only input ranges in C++20       |
| `radr::drop(n)`            | !(ra+sized)    | input   | contig   |  =    |  ⊜        |                                          |
| `radr::drop_while(fn)`     | always         | input   | contig   |  ⊜    |  ⊜        |                                          |
| `radr::filter(fn)`         | always         | input   | bidi     |  -    |  ⊝        |                                          |
| `radr::reverse`            | non-common     | bidi    | ra       |  =    |  +        |                                          |
| `radr::slice(m, n)`        | !(ra+sized)    | input   | contig   |  =    |  =        | get subrange between m and n             |
| `radr::take(n)`            |                | input   | contig   |  =    |  ra+sized |                                          |
| `radr::take_exactly(n)`    |                | input   | contig   |  +    |  ra+sized | turns unsized into sized                 |
| `radr::transform(fn)`      |                | input   | ra       |  =    |  =        |                                          |
| `radr::make_single_pass`   |                | input   | input    |  -    |  -        | demotes range category to single-pass    |

**min cat** underlying range required to be at least input (`input_range`), fwd (`forward_range`), bidi (`bidirectional_range`),
ra (`random_access_range`) or contig (`contiguous_range`)<br>
**max cat** maximum category that is preserved<br>
`-` means property is lost<br>
`=` means property is preserved<br>
`+` means property is gained (this is rare)<br>
Encircled symbols indicate differences from the standard library adaptors


