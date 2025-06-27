# Caching begin()

Calling `begin()` $m$ times on a multi-pass range shall have complexity `O(n+m)` where n is the size/distance of the range (TODO quote).
There are no such requirements on single-pass ranges, because it is not generally supported to call `begin()` more than once on a single-pass range (and it may even be difficult to reason about size/distance at all).
Thus, all of the following pertains to `begin()` on multipass ranges.

Certain range adaptors inherently have a linear-size `begin()`, e.g. calling begin on `std::list{/**/} | drop(k)` needs to do k increments where k could be equal to n.
To satisfy the above requirements (`O(n+m)` and not `O(n*m)`), the begin iterator needs to be cached.

There are two possibilities:


## 1) Cache it the first time begin() is called

This is what standard library adaptors do. It has the advantage that no iteration takes place if `begin()` is never called.
The downsides of this design are, however, quite significant:
* Calling `begin()` has side-effects. This is a fundamental difference to how other existing multi-pass ranges, in particular containers, behave. This has implications for (thread) safety.
* For the same reasons, `.begin() const` does not exist on such ranges, i.e. they are not const-iterable and it is impossible to pass these adaptors by `const &`. This is surprising, because the mutability is not observable by the user, and an operation like "show me all elements except the first 5" is by all reasonable assumptions a read-only operation.


## 2) Cache on construction

This is what our library does. It has the advantage that none of the adaptors require mutable state, and we can provide `.begin() const` as well as `.begin()` (non-const) with the same guarantees as standard library containers (provided that the underlying range also does).

### But aren't range adaptors supposed to be lazy?

It's possible to have completely lazy adaptors, and it is possible to have adaptors with the complexity constraints described above. But it is not possible to have both at the same time. The standard library adaptors are also not fully lazy, because they also cache, and they are also invalidated by a change of the underlying datastructure.

Are they "lazier" than our adaptors? Arguably. But it is important to point out, that this is not a boolean issue and that the standard library also performs trade-offs with regard to laziness, e.g. it provides `std::views::split` and `std::views::lazy_split` where you almost always want the less lazy one.

We believe that providing better behaviour with regard to `const`-ness and side-effects is the right choice for most people.

## Performance implications

### Never calling begin()

This library caches on construction, so if you never call `begin()` on the range (i.e. don't use it), you are wasting CPU cycles compared to the standard library (wich caches on calling `begin()`).
However, we believe that this is rarely useful, because you want to iterate over your adaptor (or at least check `begin() == end()`)—that's why you create it.
And you can delay creating the adaptor until you actually need it.[^delay]

[^delay]: In practice, we have observed two styles commonly employed with range adaptors:

    1. Data in the form of containers is passed into an algorithm which locally creates adaptors on said container when it needs the respective transformation/operation. In this case, the adaptors are created just before they are used—and `begin()` is always called.

    2. Data is loaded/generated initially in the form of containers and stored together with range adaptors—that are then repeatedly used during the runtime of the program. In this case, the initial setup cost of the adaptors is likely neglegible.

### "Non-propagating cache"

```cpp
std::list<size_t> vec{1,1,1,1,1, 2,2,2,2,2, 1,1,1,1,1};
auto even = [] (size_t i) { return i % 2 == 0; };

auto vue =          vec  | std::views::filter(even);
auto rad = std::ref(vec) |       radr::filter(even);
```

Let's say we have the above list (five 1s, five 2s, five 1s), and we want to filter it for even values.
The point of caching begin() is to avoid reading over the first five 1s every time `begin()` is called.

We want to find out how many times the predicate is evaluated when using the adaptor, i.e. when creating the adaptor and iterating over it one or more times.

|  `std::`                                                   | # calls     |
|------------------------------------------------------------|------------:|
| `auto vue = vec │ std::views::filter(even);` (constr.)     |  0          |
| `for (size_t i : vue) {}` (1st)                            | 15          |
| `for (size_t i : vue) {}` (2nd+)                           |  9          |
| `for (size_t i : vue │ std::views::take(100)) {}` (comb.)  | 15          |
| **`radr::`**                                               |             |
| `auto rad = std::ref(vec) │ radr::filter(even);` (constr.) |  6          |
| `for (size_t i : rad) {}` (1st)                            |  9          |
| `for (size_t i : rad) {}` (2nd+)                           |  9          |
| `for (size_t i : rad │ radr::take(100)) {}` (comb.)        |  9          |

This tables illustrates the similarities between the standard library and our library:
  * Constructing the adaptor and iterating over it once adds up to 15 predicate calls.
  * The second (and any further) iteration only performs 9 calls.
  * Note that since neither `std` nor `radr` cache the end, the *last* five 1s will always be parsed. *More on this below*.

They differ in that some of the work happens on construction in RADR.
And finally, we see that the libraries differ when the adaptor is combined with another one, e.g. `take(100)`.
The standard library adaptor resets its cache when being moved or copied, because the caching mechanism in the standard library is *non-propagating*.
This is not known to most users of `std::ranges::`, and it is even more surprising since "building up" ranges pipelines in multiple steps is a frequently recommended practice.

The adaptors from this library don't suffer from this problem, because they always cache begin and that cache remains valid on copy and move. They can thus deliver a higher performance in certain use-cases.


### Caching end()

Calling `end()` is required to be in O(1), and it always is (in the standard library and this library).
However, the sentinel sometimes does not refer to the *actual* end in the underlying sequence, as the example above illustrates: The end of the filter could/should refer to the first `1` in the last block.
But it refers to the end of the underlying range, and that is why the tail is parsed every time you iterate over the full range.

Such an "algorithmic end" is typically indicated by a special type, and such ranges typically aren't so-called *common ranges* (`std::ranges::common_range` or `radr::common_range`).
However, in the standard library, some ranges are implemented as common, although they show such behaviour; `std::views::filter` is one of them.
In our library `radr::filter` is not common by default. This has the advantage that we can use `radr::to_common` to *make it common* **which will cache the actual end**.

|  `radr::`                                                                    | # calls     |
|------------------------------------------------------------------------------|------------:|
| `auto rad = std::ref(vec) │ radr::filter(even) │ radr::to_common;` (constr.) |  12         |
| `for (size_t i : rad) {}` (1st)                                              |  4          |
| `for (size_t i : rad) {}` (2nd+)                                             |  4          |
| `for (size_t i : rad │ radr::take(100)) {}` (comb.)                          |  4          |

The head of the range is parsed on construction of `filter` (6 calls), and the tail is parsed on construction of `to_common` (reverse find, also 6 calls).
After this, the adaptor needs to parse neither the head, nor the tail of the underlying range on a full iteration.
The standard library does not offer such facilities.
