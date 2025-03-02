# Trade-offs

This is a list of things that work differently from the standard library and that could potentially be a drawback, depending on your most common use-cases.

Performance trade-offs are covered on a [separate page](./performance.md).

## For forward ranges, our adaptors require const-iterable

This is a core part of our design. Initially, our range adaptors were conditionally const-iterable, but we believe the current design is more concise, making it easier to use and easier to maintain.

The only widely used forward ranges that are not const-iterable are views from the `std::ranges` library, and we plan to offer equivalent adaptors, so you can replace them.

## Contiguous ranges are always type-erased

We never preserve contiguous iterators, and instead always store pointers. This make our types easier to read and debug, and it prevents needlessly nesting templates. If you do non-standard things with your iterators (like bounds-checking), this capability will be lost.

We believe that this trade-off is worthwhile and that bounds-checking should instead be provided by the language (language contracts, security profiles,  …). Such features will be added library-wide once available in C++. In the meantime, you can create a custom range-adaptor that re-adds bounds-checking and add it to the end of your adaptor pipelines, if desired (or required).

A change submitted to C++26 [^p3349] officially allows this behaviour.

[^p3349]: http://wg21.link/P3349

## It is not possible to "unpeal" chained adaptors

Most standard library adaptor types provide the `.base()` function which returns the underlying range, and, because
the standard library adaptors normally nest/stack, you can recursively "unpeal" a chained adaptor.
Our library tries to avoid nesting templates when possible, so this is not generally possible, see the [page on simpler types](./simpler_types.md).
We believe that the benefits of simpler types greatly outweigh the usefulness of this feature.
It should also be noted that not all standard library adaptors provide `.base()` and that not all standard library adaptors stack all the time, so you cannot rely on this feature in a generic context to reliably reproduce all intermediate steps. For non-generic contexts, on the other hand, you know the combination of applied adaptors and can just recreate intermediate states if desired.

## Stricter constraints on functors

Because this library stores invocables in the iterator and not the range (for multi-pass range adaptors), we have slightly stronger requirements on the invocables.
In particular, they are invoked through a `const &`.
This has the effect that, e.g. you cannot pass a `mutable` lambda expression.

```cpp
std::vector vec{1,2,3,4};
auto plus1 = [] (int i) mutable { return i + 1; };
auto vue = vec | std::views::transform(plus1);        // well-formed
//auto rad = std::ref(vec) | radr::transform(plus1);  // ill-formed
```

Note that the `mutable` keyword has no effect in this particular spot and that invocables that actually do change on iteration are forbidden also in the standard library (they result in *undefined behaviour*).
We thus believe that the usefulness of mutable invocables is niche, and that it may indeed be benificial that we prevent some undefined behaviour at compile-time.
