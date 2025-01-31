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

| Range adaptors (objects)   | Equivalent in `std::`   | Remarks                                  |
|----------------------------|-------------------------|------------------------------------------|
| `radr::as_const`           | `std::views::as_const`  | make the range *and* its elements const  |
| `radr::as_rvalue`          | `std::views::as_rvalue` | returns only input ranges in C++20       |
| `radr::cache_end`          | (`std::views::common`)  | turns non-common into common             |
| `radr::drop(n)`            | `std::views::drop`      |                                          |
| `radr::drop_while(fn)`     | `std::views::drop_while`|                                          |
| `radr::filter(fn)`         | `std::views::filter`    |                                          |
| `radr::join`               | `std::views::join`      |                                          |
| `radr::reverse`            | `std::views::reverse`   |                                          |
| `radr::slice(m, n)`        | *not yet available*     | get subrange between m and n             |
| `radr::split(pat)`         | `std::views::split`     |                                          |
| `radr::take(n)`            | `std::views::take`      |                                          |
| `radr::take_exactly(n)`    | *not yet available*     | turns unsized into sized                 |
| `radr::transform(fn)`      | `std::views::transform` |                                          |
| `radr::make_single_pass`   | *not yet available*     | demotes range category to input          |

We plan to add equivalent objects for most standard library adaptors.

## Standalone ranges

| Standalone ranges          | kind  | Equivalent in `std::`      | Remarks                                              |
|----------------------------|:-----:|----------------------------|------------------------------------------------------|
| `radr::empty_range<T>`     | class | `std::ranges::empty_view`  | a range of fixed size 0                              |
| `radr::single<T[, s]>`     | class | `std::ranges::single_view` | a range of fixed size 1; storage configurable        |

Note that most of our standalone ranges are not implemented as "factory" objects, but just as plain types.


## Notable functions

| Notable functions                  | CP   | Remarks                                              |
|------------------------------------|:----:|------------------------------------------------------|
| `radr::subborrow(r, it, sen[, s])` | ✔   | Used when creating subranges from other ranges        |
| `radr::subborrow(r, i, j)`         | (✔) | Position-based slice                                  |
| `radr::borrow(r)`                  | (✔) | `= radr::subborrow(r, r.begin(), r.end(), r.size())`  |

CP denotes functions that you can customise for your own types, e.g. specify a different subrange-type for a specific container.
