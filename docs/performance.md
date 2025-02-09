# Performance

## Iterator size

See [Iterator size](./iterator_size.md).

## cost of caching begin

See [Caching begin()](./caching_begin.md).


## Dynamic alloc in owning_rad

When creating an `owning_rad` (e.g. implicitly by `std::vector{1, 2, 3} | radr::take(1)`), the container is moved into indirect storage which involves a dynamic allocation: A `std::vector` is heap-allocated and move-constructed with the provided temporary.
This heap allocation is small (*just* the control data, not the elements), but it is necessary so that iterators remain valid when the `owning_rad` is moved.

In contexts where heap allocations are undesirable, we recommend always using indirections and avoiding `owning_rad` for now.

Possible future directions:
  * Create an opt-in trait for containers whose iterators remain valid during move (e.g. all standard library containers other than `std::array`). Such containers could be stored directly.
  * If there is demand, an additional interface could be added that allows providing an allocator.
