# RADR library agent summary

This C++ software project is Modern C++ header-only library that reimplements range adaptors and range factories (`std::views::*`) from the C++20 standard library with the aim of fixing design mistakes and improving usability.


## Documentation

Most important bits are available in the following files:

* [Main README](./README.md): high-level overview and motivation.
* [Getting started](./docs/getting_started.md): short introduction on how to use this library and the terminology used in the documentation.
* [Implementation status](./docs/implementation_status.md): overview of what has been implemented.

The project has high-quality, detailled human-written documentation in the `docs` folder. You may look up the longer files in that folder, but only when the respective topic is actually relevant.

There is little, but highly relevant, doxygen-style in-code documentation for every adaptor/factory object, typically towards the end of the file.


## Important bits to keep in mind

* Names of the adaptor/factory objects are *almost* always identical to the standard library (`radr::FOO` instead of `std::views::FOO`).
* Range capture of lvalues of containers requires wrapping them in `std::ref()`, e.g. `auto v = std::ref(vec) | radr::FOO`.
* The implementation for multi-pass and single-pass ranges is always separate.
  * Multi-pass adaptors are always implemented as an iterator-sentinel pair stored in `radr::owning_rad` or `radr::borrowing_rad` for owning/borrowing ranges respectively.
  * Single-pass adaptors are always implemented as `radr::generator`.


## Testing

How to build the test-suite (out-of-source):

```
cmake ${PROJECT_ROOT}/tests/unit -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=${COMPILER} -DCMAKE_CXX_FLAGS="-std=c++20"

make -j 4 ${TARGET}

ctest
```

* PROJECT_ROOT: Place of the source code.
* COMPILER: e.g. `g++13` on FreeBSD or `g++-13` on Linux.
* TARGET: one of the test targets (can be omitted to build everything).
* Once you are finished with changes, make sure they work with both `-std=c++20` and `-std=c++23` (see below for details).
* You may add additional CMAKE_CXX_FLAGS if they help you, e.g. to create SARIF output for better machine-readability.


## Coding advice and rules

**You should never:**
  * Perform modifying git operations, like commits or rebases.
  * Modify or create anything in the `docs` folders (except updating `implementation_status.md`). Documentation is reserved for humans.

**You should ask:**
  * If user instructions are vague or contradictory (in themselves or in combination with this file).
  * Before creating any new entities in namespace `radr::` that were not explicitly requested.

**You should:**
  * Always search for and reuse existing entities in the codebase and/or the standard library, if possible.
  * Emulate existing style of the codebase.
  * Write clean, minimal, modern C++.
  * You can use everything from C++20, but nothing from later standards.
    * Exception: code that implements zip-adaptors, zip-factories or related features needs C++23. This code needs to be guarded by `RADR_FEATURE_ZIP` (look for existing examples if needed).
  * Prefer functional and declarative paradigms over imperative and OOP patterns, but don't be dogmatic.
  * Use modern meta-programming with `constexpr`, immediately evaluated lambdas, `overloaded()`… when possible.
  * Only encode in constraints what is necessary for overload resolution. Implement other requirements as `static_assert()`.
  * For unit tests, follow the `UNIT_TEST_TEMPLATE.cxx` file.
  * Don't worry about formatting details, just reformat code with `clang-format17` after performing changes.
