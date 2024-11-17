# Safety

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
// {
//   std::string_view v{"FOO"};
//   /*…*/
//   return v | radr::take(2);
// }
```

Ill-formed.

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

Ill-formed.


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


## side-effects

.begin()

.empty()
