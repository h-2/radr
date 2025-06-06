# Feature table

**Range adaptor objects:**

| Range adaptor              | $O(n)$ constr  | min cat | max cat  | sized | common    | Remarks                                  |
|----------------------------|:--------------:|---------|----------|:-----:|:---------:|------------------------------------------|
| `radr::as_const`           |                | fwd     | contig   |  =    |  =        | make the range *and* its elements const  |
| `radr::as_rvalue`          |                | input   | input/ra |  =    |  =        | returns only input ranges in C++20       |
| `radr::drop(n)`            | !(ra+sized)    | input   | contig   |  =    |  ⊜        |                                          |
| `radr::drop_while(fn)`     | always         | input   | contig   |  ⊜    |  ⊜        |                                          |
| `radr::filter(fn)`         | always         | input   | bidi     |  -    |  ⊝        |                                          |
| `radr::join`               |                | input   | (bidi)   |  -    |  =        | less strict than std::views::join        |
| `radr::reverse`            | non-common     | bidi    | ra       |  =    |  +        |                                          |
| `radr::slice(m, n)`        | !(ra+sized)    | input   | contig   |  =    |  =        | get subrange between m and n             |
| `radr::split(pat)`         | always         | input   | fwd      |  -    |  ⊝        |                                          |
| `radr::take(n)`            |                | input   | contig   |  =    |  ra+sized |                                          |
| `radr::take_exactly(n)`    |                | input   | contig   |  +    |  ra+sized | turns unsized into size of n             |
| `radr::take_while(fn)`     |                | input   | contig   |  -    |  -        |                                          |
| `radr::to_common`          | !(common)      | fwd     | contig   |  ⊕    |  +        |                                          |
| `radr::to_single_pass`     |                | input   | input    |  -    |  -        | demotes range category to single-pass    |
| `radr::transform(fn)`      |                | input   | ra       |  =    |  =        |                                          |

**min cat** underlying range required to be at least input (`input_range`), fwd (`forward_range`), bidi (`bidirectional_range`),
ra (`random_access_range`) or contig (`contiguous_range`)<br>
**max cat** maximum category that is preserved<br>
`-` means property is lost<br>
`=` means property is preserved<br>
`+` means property is gained (this is rare)<br>
Encircled symbols or categories in () indicate differences from the standard library adaptors


