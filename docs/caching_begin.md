# Caching begin()

Calling `begin()` $m$ times on a multi-pass range shall have complexity `O(n+m)` where n is the size/distance of the range (TODO quote).
There are no such requirements on single-pass ranges, because it is not generally supported to call `begin()` more than once on a single-pass range (and it may even be difficult to reason about size/distance at all).
Thus, all of the following pertains to `begin()` on multipass ranges.

Certain range adaptors inherently have a linear-size `begin()`, e.g. calling begin on `std::list{/**/} | drop(k)` needs to do k increments where k could be equal to n.
To satisfy the above requirements (`O(n+m)` and not `O(n*m)`), the begin iterator needs to be cached.

There are two possibilities:


## Cache it the first time begin() is called

This is what standard library adaptors do. It has the advantage that no iteration takes place if `begin()` is never called. However, we believe that this is rarely useful, because you want to iterate over your adaptor (or at least check `begin() == end()`)—that's why you create it. See the [page on performance](./performance.md) for more details.

<!--In practice, we have observed two styles commonly employed with range adaptors:
1. Data in the form of containers is passed into an algorithm which locally creates adaptors on said container when it needs the respective transformation/operation. In this case, the adaptors are created just before they are used and `begin()` is always called.
2. Data is loaded/generated initially in the form of containers and stored together with range adaptors that are then repeatedly used during the runtime of the program. In this case, the initial setup cost of the adaptors is likely neglegible.-->

The downsides of this design are, however, quite significant:
* Calling `begin()` has side-effects. This is a fundamental difference to how other existing multi-pass ranges, in particular containers, behave. This has implications for (thread) safety.
* For the same reasons, `.begin() const` does not exist on such ranges, i.e. they are not const-iterable and it is impossible to pass these adaptors by `const &`. This is surprising, because the mutability is not observable by the user, and an operation like "show me all elements except the first 5" is by all reasonable assumptions a read-only operation.


## Cache on construction

This is what our library does. It has the advantage that none of the adaptors require mutable state, and we can provide `.begin() const` as well as `.begin()` (non-const) with the same guarantees as standard library containers (provided that the underlying range also does).

TODO

### But aren't range adaptors supposed to be lazy?

It's possible to have completely lazy adaptors, and it is possible to have adaptors with the complexity constraints described above. But it is not possible to have both at the same time. The standard library adaptors are also not fully lazy, because they also cache, and they are also invalidated by a change of the underlying datastructure.

Are they "lazier" than our adaptors? Arguably. But it is important to point out, that this is not a boolean issue and that the standard library also performs trade-offs with regard to laziness, e.g. it provides `std::views::split` and `std::views::lazy_split` where you almost always want the less lazy one.

We believe that providing better behaviour with regard to `const`-ness and side-effects is the right choice for most people.


