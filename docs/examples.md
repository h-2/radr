## Normal usage


<table>
<tr>
<th>

`std::`

</th>
<th>

`radr::`

</th>
</tr>

<tr>
<td>

**lvalues of borrowed ranges**

</td>
<td>

**lvalues of borrowed ranges**

</td>
</tr>



<tr>

<td>

```cpp
/* capture of lvalue borrow */
std::string_view sv = "foo";
auto vue1 = sv | std::views::take(2);
```

--

```cpp
/* copy */
auto copy = vue1;
```

Well-formed ( $O(1)$ ).


</td>


<td>

```cpp
/* capture of lvalue */
std::string_view sv = "foo";
auto rad2 = sv | radr::take(2);
```

Same syntax.

```cpp
/* copy */
auto copy = rad1;
```

Well-formed ( $O(1)$ ).
</td>
</tr>

<tr>
<td>

**lvalues of containers**

</td>
<td>

**lvalues of containers**

</td>
</tr>

<tr>
<td>

```cpp
/* capture of lvalue */
std::vector vec{1, 2, 3};
auto vue1 = vec | std::views::take(2);
```

--

```cpp
/* copy */
auto copy = vue1;
```

Well-formed ( $O(1)$ ).
</td>
<td>

```cpp
/* capture of lvalue */
std::vector vec{1, 2, 3};
auto rad2 = std::ref(vec) | radr::take(2);
```

You need to use `std::ref()` here!

```cpp
/* copy */
auto copy = rad1;
```

Well-formed ( $O(1)$ ).
</td>
</tr>

<tr>
<td>

**rvalues**

</td>
<td>

**rvalues**

</td>
</tr>

<tr>
<td>

```cpp
/* create adaptor on temporary */
auto vue0 = std::vector{1, 2, 3} | std::views::take(2);

/* move existing */
std::vector vec{1, 2, 3};
auto vue1 = std::move(vec) | std::views::take(2);
```

Same syntax.

```cpp
/* copy */
// auto copy = vue1;
```

Ill-formed, owning views are not copyable.

</td>
<td>

```cpp
/* create adaptor on temporary */
auto rad0 = std::vector{1, 2, 3} | radr::take(2);

/* move existing */
std::vector vec{1, 2, 3};
auto rad1 = std::move(vec) | radr::take(2);
```

Same syntax.

```cpp
/* copy */
auto copy = rad1;
```

Well-formed; our owning adaptors are ( $O(n)$ ).

</td>
</tr>

</table>

See [the page on fundamental range properties](./range_properties.md) for more details.


## Dangling references

<table>
<tr>
<th>

`std::`

</th>
<th>

`radr::`

</th>
</tr>
<tr>
<td>

**dangling references**

</td>
<td>

**dangling references**

</td>
</tr>

<tr>
<td>

```cpp
{
  std::string_view v{"FOO"};
  /*…*/
  return v | std::views::take(2);
}
```

Well-formed; not dangling.

```cpp
{
  std::string_view v{"FOO"};
  /*…*/
  return std::move(v) | std::views::take(2);
}
```

Well-formed; not dangling.

```cpp
{
  std::vector v{1,2,3};
  /*…*/
  return v | std::views::take(2);
}
```

Well-formed, **dangling** 💣

```cpp
{
  std::vector v{1,2,3};
  /*…*/
  return std::move(v) | std::views::take(2);
}
```

Well-formed, not dangling.
</td>

<td>

```cpp
{
  std::string_view v{"FOO"};
  /*…*/
  return v | radr::take(2);
}
```

Well-formed, not dangling.

```cpp
{
  std::string_view v{"FOO"};
  /*…*/
  return std::move(v) | radr::take(2);
}
```

Well-formed; not dangling.

```cpp
// {
//   std::vector v{1,2,3};
//   /*…*/
//   return v | std::views::take(2);
// }
```

Ill-formed; dangling prevented.


```cpp
{
  std::vector v{1,2,3};
  /*…*/
  return std::move(v) | std::views::take(2);
}
```

Well-formed; not dangling.
</td>
</tr>
</table>

This library's syntax explicitly forces you to choose between `std::move()`-ing or `std::ref()`-ing existing variables
into adaptors. This avoids unintendedly creating dangling references.


## Const

<table>
<tr>
<th>

`std::`

</th>
<th>

`radr::`

</th>
</tr>
<tr>
<td>

**const-iterating**

</td>
<td>

**const-iterating**

</td>
</tr>

<tr>
<td>

```cpp
void print(auto const & r)
{
  for (char c : r)
    std::cout << c;
}
```

--

```cpp
std::vector vec{'f','o','o'};
auto v = vec | std::views::take(2);
print(v);
```

Well-formed.

```cpp
std::vector vec{'f','o','o'};
auto v = vec | std::views::transform(/**/);
print(v);
```

Well-formed.

```cpp
std::vector vec{'f','o','o'};
auto v = vec | std::views::filter(/**/);
// print(v);
```

Ill-formed; filter not const-iterable.

</td>

<td>

```cpp
void print(auto const & r)
{
  for (char c : r)
    std::cout << c;
}
```

--

```cpp
std::vector vec{'f','o','o'};
auto v = vec | radr::take(2);
print(v);
```

Well-formed.

```cpp
std::vector vec{'f','o','o'};
auto v = vec | radr::transform(/**/);
print(v);
```

Well-formed.

```cpp
std::vector vec{'f','o','o'};
auto v = vec | radr::filter(/**/);
print(v);
```

Well-formed.
</td>
</tr>

<tr>
<td>

**shallow const**

</td>
<td>

**deep const**

</td>
</tr>

<tr>
<td>

```cpp
void mutcon(auto const & r)
{
  for (char & c : r)
    c = 'A';
}
```

--

```cpp
std::vector vec{'f','o','o'};
// mutcon(v);
```

Ill-formed; can't write through `const`.

```cpp
std::vector vec{'f','o','o'};
auto v = vec | std::views::take(2);
mutcon(v);
```

Well-formed; vec is changed 🙈
</td>

<td>

```cpp
void mutcon(auto const & r)
{
  for (char & c : r)
    c = 'A';
}
```

--

```cpp
std::vector vec{'f','o','o'};
// mutcon(v);
```

Ill-formed; can't write through `const`.

```cpp
std::vector vec{'f','o','o'};
auto v = vec | std::views::take(2);
// mutcon(v);
```

Ill-formed; can't write through `const`.
</td>
</tr>

</table>

* Many standard library views are not const-iterable; all our adaptors on containers are.
* Standard library views allow writing through `const`; all our adaptors on containers forbid it.
* See [the page on const](./const.md) for more details.

## Undefined behaviour

<table>
<tr>
<th>

`std::`

</th>
<th>

`radr::`

</th>
</tr>
<tr>
<td>

**undefined behaviour**

</td>
<td>

**no undefined behaviour**

</td>
</tr>

<tr>
<td>

```cpp
std::vector vec{1,2,2,3}
auto is_even = /**/;

auto vue = vec | std::view::filter(is_even);
auto b = vue.begin(); // on first '2'
*b = 1; // no longer satisfies predicate
```

Undefined bahaviour 💣

</td>

<td>

```cpp
std::vector vec{1,2,2,3}
auto is_even = /**/;

auto rad = vec | radr::filter(is_even);
auto b = vue.begin(); // on first '2'
// *b = 1;
```

Our filter disallows changing vec.
</td>
</tr>
</table>

This library helps users avoid undefined bahaviour by disallowing assigning through range adaptors that would invalidate those range adaptors in non-obvious ways.
