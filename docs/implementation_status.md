# Implementation status

This library is a **work-in-progress**.
We try to stay close to standard library interfaces, but we neither promise full API compatibility with `std::ranges::` nor do we currently promise stability between releases of this library.
The latter will likely change at some point.

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

| Range adaptors (objects) | C++XY | | Equivalent in `std::`          | C++XY     | Differences of `radr` objects            |
|--------------------------|-------|-|--------------------------------|-----------|------------------------------------------|
| `radr::as_const`         | C++20 | | `std::views::as_const`         | **C++23** | make the range *and* its elements const  |
| `radr::as_rvalue`        | C++20 | | `std::views::as_rvalue`        | **C++23** | *returns only input ranges in C++20      |
| `radr::drop(n)`          | C++20 | | `std::views::drop`             | C++20     |                                          |
| `radr::drop_while(fn)`   | C++20 | | `std::views::drop_while`       | C++20     |                                          |
| `radr::filter(fn)`       | C++20 | | `std::views::filter`           | C++20     |                                          |
| `radr::join`             | C++20 | | `std::views::join`             | C++20     |                                          |
| `radr::reverse`          | C++20 | | `std::views::reverse`          | C++20     |                                          |
| `radr::slice(m, n)`      | C++20 | | *not yet available*            |           | get subrange between m and n             |
| `radr::split(pat)`       | C++20 | | `std::views::split`            | C++20     |                                          |
| `radr::take(n)`          | C++20 | | `std::views::take`             | C++20     |                                          |
| `radr::take_exactly(n)`  | C++20 | | *not yet available*            |           | turns unsized into sized                 |
| `radr::take_while(fn)`   | C++20 | | `std::views::take_while`       | C++20     |                                          |
| `radr::to_common`        | C++20 | | `std::views::common`[^diff]    | C++20     | turns non-common into common             |
| `radr::to_single_pass`   | C++20 | | `std::views::to_input`[^diff]  | **C++26** | demotes range category to input          |
| `radr::transform(fn)`    | C++20 | | `std::views::transform`        | C++20     |                                          |

All range adaptors from this library are available in C++20, although `radr::as_rvalue` behaves slightly different between modes.

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
