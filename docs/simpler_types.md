# Simpler types

The types returned by range adaptors can get complicated very quickly. This makes debugging difficult, increases
compile times and leads to template bloat when passing adaptors to other (function) templates.
We have made an effort to simplify the types wherever possible.

## Types can customise what their default borrowed range type is

The standard library:

```cpp
// the full type of the underlying range is reflected in the adaptor
std::string const str = "foobar";
auto adapt0 = str | std::views::take(2);
static_assert(std::same_as<decltype(adapt0),
                           std::ranges::take_view<std::ranges::ref_view<std::string const>>);
```

Our library:

```cpp
// a slice of a string constant is just a string_view, so that's what we return
std::string const str = "foobar";
auto adapt0 = std::ref(str) | radr::take(2);
static_assert(std::same_as<decltype(adapt0),
                           std::string_view>);
```


## Contiguous ranges are type erased within adaptors

The standard library:

```cpp
// the full type of the underlying range is reflected in the adaptor
std::vector<int> container0{1, 2, 3};
auto adapt0 = container0 | std::views::take(2);
static_assert(std::same_as<decltype(adapt0),
                           std::ranges::take_view<std::ranges::ref_view<std::vector<int>>>);

// even the size of an array is contained
std::array<int, 3> container1{1, 2, 3};
auto adapt1 = container1 | std::views::take(2);
static_assert(std::same_as<decltype(adapt1),
                           std::ranges::take_view<std::ranges::ref_view<std::array<int, 3>>>);

// the adaptors have different types
static_assert(!std::same_as<decltype(adapt1), decltype(adapt2)>);
```

Our library:

```cpp
// our adaptors only contain the iterators and these decay to pointers for contiguous ranges
std::vector<int> container0{1, 2, 3};
auto adapt0 = std::ref(container0) | radr::take(2);
static_assert(std::same_as<decltype(adapt0),
                           radr::borrowing_rad<int *>>);

// the type of the underlying range is erased completely
std::array<int, 3> container1{1, 2, 3};
auto adapt1 = std::ref(container1) | radr::take(2);
static_assert(std::same_as<decltype(adapt1),
                           radr::borrowing_rad<int *>>);

// thus, both adaptors have the same type
static_assert(std::same_as<decltype(adapt1), decltype(adapt2)>);
```

## Folding of "slicing" adaptors

Standard library:

```cpp
// chained "slicing" adaptors lead to nested templates [contiguous range]
std::vector<int> container0{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
auto adapt0 = container0 | std::views::take(10) | std::views::drop(1) | std::views::take(5) | std::views::drop(2);
static_assert(std::same_as<decltype(adapt0),
    std::ranges::drop_view<
        std::ranges::take_view<
            std::ranges::drop_view<
                std::ranges::take_view<std::ranges::ref_view<std::vector<int>>>>>>);

// chained "slicing" adaptors lead to nested templates [forward range]
std::forward_list<int> container1{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
auto adapt1 = container1 | std::views::take(10) | std::views::drop(1) | std::views::take(5) | std::views::drop(2);
static_assert(std::same_as<decltype(adapt1),
    std::ranges::drop_view<
        std::ranges::take_view<
            std::ranges::drop_view<
                std::ranges::take_view<std::ranges::ref_view<std::forward_list<int>>>>>>);

```

Our library:

```cpp
// chained "slicing" adaptors are type-erased completely [contiguous range]
std::vector<int> container0{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
auto adapt0 = std::ref(container0) | radr::take(10) | radr::drop(1) | radr::take(5) | radr::drop(2);
static_assert(std::same_as<decltype(adapt0),
                           radr::borrowing_rad<int *>>);

// even for forward ranges, the adaptor type does not change after the first take()
std::forward_list<int> container1{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
auto adapt1 = std::ref(container1) | radr::take(10);
auto adapt2 = std::ref(container1) | radr::take(10) | radr::drop(1) | radr::take(5) | radr::drop(2);
static_assert(std::same_as<decltype(adapt1),
                           decltype(adapt2)>);
```

This works for these adaptors: `radr::take`, `radr::take_exactly`, `radr::slice`, `radr::drop` and `radr::drop_while`
(but not `radr::take_while`).

## Folding of non-slicing adaptors

Chaining multiple `std::views::transform` or `std::views::filter` leads to the entire type being nested.
In this library, the iterators are not nested, only the functor within the iterator is nested/chained.

TODO example

## All single-pass ranges are generators

TODO example
