# Safety

## Dangling references

This library's syntax explicitly forces you to choose between `std::move()`-ing or `std::ref()`-ing existing variables
into adaptors.[^stdref] This avoids unintendedly creating dangling references.

See the [examples](./examples.md#dangling-reference).


[^stdref]: Note that we are not technically storing `std::reference_wrapper`, we are just using `std::ref` as a marker for opting in to the indirection.

<!--

TODO

## side-effects

.begin()

.empty()

-->
