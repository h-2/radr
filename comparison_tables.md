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

<tr>
<td>

**lvalues**

</td>
<td>

**lvalues**

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
</table>

