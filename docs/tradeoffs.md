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

Note that, `radr::owning_rad` provides a `.container()` member. This allows you to directly observe/extract the value-storing range at the root of a (chained) owning adaptor, which may be useful in certain contexts.

## Stricter constraints on functors

TODO

