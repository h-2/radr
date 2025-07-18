# Implementation status

This library is a **work-in-progress**.
We try to stay close to standard library interfaces, but we neither promise full API compatibility with `std::ranges::` nor do we currently promise stability between releases of this library.
The latter will likely change at some point.

### Progress implementing std::views::X equivalents (adaptor objects and factory objects)

| Standard    | adaptors |factories | Comment                    |
|-------------|---------:|---------:|----------------------------|
| C++20       | 14/15    |   5/5    | `lazy_split` not planned   |
| C++23       |  2/13    |   1/1    |                            |
| C++26       |  1/03    |   --     |                            |
| C++29       |  1/??    |   --     |                            |
| extra       |     1    |          |                            |

See below for details. Note that the list of adaptors in C++26 and C++29 is not yet final.

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

| Range adaptors (objects)  | C++XY | | Equivalent in `std::`          | C++XY     | Differences of `radr` objects            |
|---------------------------|-------|-|--------------------------------|-----------|------------------------------------------|
| `radr::all`               | C++20 | | `std::views::all`              | C++20     |                                          |
| `radr::as_const`          | C++20 | | `std::views::as_const`         | **C++23** | make the range *and* its elements const  |
| `radr::as_rvalue`         | C++20 | | `std::views::as_rvalue`        | **C++23** | *returns only input ranges in C++20      |
| `radr::drop(n)`           | C++20 | | `std::views::drop`             | C++20     |                                          |
| `radr::drop_while(fn)`    | C++20 | | `std::views::drop_while`       | C++20     |                                          |
| `radr::elements<I>`       | C++20 | | `std::views::elements`         | C++20     |                                          |
| `radr::filter(fn)`        | C++20 | | `std::views::filter`           | C++20     |                                          |
| `radr::join`              | C++20 | | `std::views::join`             | C++20     |                                          |
| `radr::keys`              | C++20 | | `std::views::keys`             | C++20     |                                          |
| `radr::reverse`           | C++20 | | `std::views::reverse`          | C++20     |                                          |
| `radr::slice(m, n)`       | C++20 | | *not yet available*            |           | get subrange between m and n             |
| `radr::split(pat)`        | C++20 | | `std::views::split`            | C++20     |                                          |
| *not planned*             | C++20 | | `std::views::lazy_split`       | C++20     | use `radr::to_single_pass ╎ radr::split` |
| `radr::take(n)`           | C++20 | | `std::views::take`             | C++20     |                                          |
| `radr::take_while(fn)`    | C++20 | | `std::views::take_while`       | C++20     |                                          |
| `radr::to_common`         | C++20 | | `std::views::common`[^diff]    | C++20     | turns non-common into common             |
| `radr::to_single_pass`    | C++20 | | `std::views::to_input`[^diff]  | **C++26** | demotes range category to input          |
| `radr::transform(fn)`     | C++20 | | `std::views::transform`        | C++20     |                                          |
| `radr::values`            | C++20 | | `std::views::values`           | C++20     |                                          |
| `radr::unchecked_take(n)` | C++20 | | `std::views::unchecked_take`   | **C++29** | turns unsized into sized                 |

All range adaptors from this library are available in C++20, although `radr::as_rvalue` behaves slightly different between modes.

[^diff]: These range adaptors have relevant differences between `std::` and `radr::`. Usually the names have been chosen differently to highlight this.


## Range factory objects

| Range factories (objects)     | C++XY | | Equivalent in `std::`   | C++XY     | Remarks                                   |
|-------------------------------|-------|-|-------------------------|-----------|-------------------------------------------|
| `radr::counted(it, n)`        | C++20 | | `std::views::counted`   | C++20     | multi-pass version of counted             |
| `radr::counted_sp(it, n)`     | C++20 | | `std::views::counted`   | C++20     | single-pass version of counted            |
| `radr::empty<T>`              | C++20 | | `std::views::empty`     | C++20     |                                           |
| `radr::iota(val[, bound])`    | C++20 | | `std::views::iota`      | C++20     | multi-pass version of iota                |
| `radr::iota_sp(val[, bound])` | C++20 | | `std::views::iota`      | C++20     | single-pass version of iota               |
| `radr::istream<Val>`          | C++20 | | `std::views::istream`   | C++20     |                                           |
| `radr::repeat(val[, bound])`  | C++20 | | `std::views::repeat`    | **C++23** | allows indirect storage and static bounds |
| `radr::single(val)`           | C++20 | | `std::views::single`    | C++20     | allows indirect storage                   |

The standard library groups `counted` with "range adaptors" and not "range factories", 
although you create it on an iterator and not a range.

## Notable functions

| Notable functions                  | CP   | Remarks                                              |
|------------------------------------|:----:|------------------------------------------------------|
| `radr::subborrow(r, it, sen[, s])` | ✔   | Used when creating subranges from other ranges        |
| `radr::subborrow(r, i, j)`         | (✔) | Position-based slice                                  |
| `radr::borrow(r)`                  | (✔) | `= radr::subborrow(r, r.begin(), r.end(), r.size())`  |

CP denotes functions that you can customise for your own types, e.g. specify a different subrange-type for a specific container.
