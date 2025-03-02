# Safety

## Dangling references

This library's syntax explicitly forces you to choose between `std::move()`-ing or `std::ref()`-ing existing variables
into adaptors.[^stdref] This avoids unintendedly creating dangling references.

See the [examples](./examples.md#dangling-reference).


[^stdref]: Note that we are not technically storing `std::reference_wrapper`, we are just using `std::ref` as a marker for opting in to the indirection.


## Undefined behaviour

An unsorted list of instances where usage patterns that result in standard library undefined behaviour are ill-formed (and thus prevented) with this library:

* Assigning through range adaptors that would invalidate those range adaptors in non-obvious ways (e.g. changing the element underlying a filter adaptor so that it no longer satisfies the predicat).
* Some (but not all) examples of inadvertantly creating a multi-pass adaptor that violates the multi-pass guarantee. See also [tradeoffs](./tradeoffs.md#stricter-constraints-on-functors).

Many of these have examples on the [examples page](./examples.md#undefined-behaviour).


<!--

TODO

## side-effects

.begin()

.empty()

-->
