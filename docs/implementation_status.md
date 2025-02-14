# Implementation status

## Range adaptor classes


| Range adaptors (classes)   | Equivalent in `std::`                                           | Remarks                                         |
|----------------------------|-----------------------------------------------------------------|-------------------------------------------------|
| `radr::generator<>`        | `std::generator<>`                                              | alias for std::generator if available           |
| `radr::borrowing_rad<>`    | `std::ranges::subrange`, (`std::span`, `std::ranges::ref_view`) | stores (iterator, sentinel) pair                |
| `radr::owning_rad<>`       | `std::ranges::owning_view`                                      | stores rvalues of containers                    |

There are no distinct type templates per adaptor (like e.g. `std::ranges::transform_view` for `std::views::transform` in the standard library).
Instead all range adaptor objects in this library (see below) return a specialisation of one of the above type templates.

## Range adaptor objects

We plan to add equivalent objects for all standard library adaptors.

| Range adaptors (objects) | C++XY | | Equivalent in `std::`          | C++XY | Differences of `radr` objects            |
|--------------------------|-------|-|--------------------------------|-------|------------------------------------------|
| `radr::as_const`         |       | | `std::views::as_const`         | C++23 | make the range *and* its elements const  |
| `radr::as_rvalue`        | *     | | `std::views::as_rvalue`        | C++23 | *returns only input ranges in C++20      |
| `radr::drop(n)`          |       | | `std::views::drop`             |       |                                          |
| `radr::drop_while(fn)`   |       | | `std::views::drop_while`       |       |                                          |
| `radr::filter(fn)`       |       | | `std::views::filter`           |       |                                          |
| `radr::join`             |       | | `std::views::join`             |       |                                          |
| `radr::reverse`          |       | | `std::views::reverse`          |       |                                          |
| `radr::slice(m, n)`      |       | | *not yet available*            |       | get subrange between m and n             |
| `radr::split(pat)`       |       | | `std::views::split`            |       |                                          |
| `radr::take(n)`          |       | | `std::views::take`             |       |                                          |
| `radr::take_exactly(n)`  |       | | *not yet available*            |       | turns unsized into sized                 |
| `radr::to_common`        |       | | `std::views::common`[^diff]    |       | turns non-common into common             |
| `radr::to_single_pass`   |       | | `std::views::to_input`[^diff]  | C++26 | demotes range category to input          |
| `radr::transform(fn)`    |       | | `std::views::transform`        |       |                                          |

C++XY is C++20 unless otherwise noted.

[^diff]: These range adaptors have relevant differences between `std::` and `radr::`. Usually the names have been chosen differently to highlight this.

## Range factory objects

| Range factories               | Equivalent in `std::`   | Remarks                                              |
|-------------------------------|-------------------------|------------------------------------------------------|
| `radr::empty<T>`              | `std::views::empty`     |                                                      |
| `radr::repeat(val, bound)`    | `std::views::repeat`    | allows indirect storage and static bounds            |
| `radr::single(val)`           | `std::views::single`    | allows indirect storage                              |

## Notable functions

| Notable functions                  | CP   | Remarks                                              |
|------------------------------------|:----:|------------------------------------------------------|
| `radr::subborrow(r, it, sen[, s])` | ✔   | Used when creating subranges from other ranges        |
| `radr::subborrow(r, i, j)`         | (✔) | Position-based slice                                  |
| `radr::borrow(r)`                  | (✔) | `= radr::subborrow(r, r.begin(), r.end(), r.size())`  |

CP denotes functions that you can customise for your own types, e.g. specify a different subrange-type for a specific container.
