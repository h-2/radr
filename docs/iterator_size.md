# Performance — Iterator size

Since all functionality in this library is implemented in terms of iterators, these iterators need to store more data
in some situations, e.g. in transform adaptors, the functor needs to be stored somewhere.
In our library, it is stored in the iterator whereas in the standard library, it is stored in the adaptor (and the iterator holds a pointer to the adaptor to use the functor).
This makes our iterators larger, *at least in theory*. In practice, this depends strongly on the size of the functor.
The very common use-case of a lambda with an empty capture uses no space in practice (by means of `[no_unique_address]`).

A common argument against our approach is that chaining multiple adaptors leads a growing size of the iterator, because
the iterator of each adaptor caches the iterator of the underlying/previous adaptor.
*In theory,* you would expect linear growth with the number of applied adaptors or even exponential growth when
adaptors are involved where each iterator also needs to store the end (like `filter`).
In practice, however, we can mitigate this almost completely for many common use cases; see below.

We'd like to highlight that there is a paper suggesting a change to `std::views::transform` to behave more like our `radr::transform`.[^1]

[^1]: https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2024/p3117r0.html


## Examples

|                               |         | `std::views::` |           | |        |  `radr::`    |            |
|-------------------------------|--------:|---------------:|----------:|-|-------:|-------------:|-----------:|
|  `sizeof()`                   |  `v`    |  `v.begin()`   | `v.end()` | |  `v`   |  `v.begin()` | `v.end()`  |
| 4x transform                  |     8   |            40  |        40 |→|   16   |           8  |         8  |
| 4x transform string capt.     |    168  |            40  |        40 |←|  288   |         144  |       144  |
| 4x transform string capt. ref |     40  |            40  |        40 |←|   96   |          48  |        48  |
| 4x filter                     |    112  |            40  |        40 |→|   16   |          16  |         1  |
| alter. transform/filter       |     56  |            40  |        40 |→|   24   |          16  |         1  |
| alter. take/drop              |     48  |             8  |         8 |→|   16   |           8  |         8  |
| alter. take/drop on bidi      |     96  |            24  |         1 |→|   16   |          16  |         1  |
| join                          |      8  |            32  |        32 |←|   96   |          48  |        48  |

The table shows some examples. The code for "4x transform" looks roughly like this (where `plusX` are lambdas with
empty captures):

```cpp
// std::views::transform
auto v =          vec  | transform(plus1) | transform(plus2) | transform(plus3) | transform(plus4);
// radr::transform
auto v = std::ref(vec) | transform(plus1) | transform(plus2) | transform(plus3) | transform(plus4);
```

More examples and the full code can be found in our test-suite. [^2]

[^2]: https://github.com/h-2/radr/blob/main/tests/unit/integration/iterator_size.cpp

As you can see: for many typical use-cases, our iterators are *smaller* than those in `std::ranges`, even when
chaining multiple adaptors. There is no linear or even exponential growth.

A notable exception are large objects captured in the arguments to e.g. `transform` or `filter`; shown in
line 2 of the table (each of the four transform lambdas captures a `std::string` by value).
We recommend storing such functors separately and wrapping them in `std::ref()` when passing to the adaptor.
This alleviates the problem (line 3 of the table).

