# `PageId` 的相等与大小比较

代码位于 `src/storage/page.h`：

```cpp
friend bool operator==(const PageId& x, const PageId& y) noexcept {
    return x.fd == y.fd && x.page_no == y.page_no;
}

bool operator<(const PageId& x) const noexcept {
    if (fd != x.fd) return fd < x.fd;
    return page_no < x.page_no;
}
```

## `PageId` 是什么

```cpp
struct PageId {
    int fd;
    page_id_t page_no;
};
```

它用两个信息唯一标识一个磁盘页面：

- `fd`：页面所属文件的文件描述符；
- `page_no`：页面在该文件中的编号。

所以页面身份可以理解为：

```text
(文件, 页面编号)
```

这里比较的是页面的“身份”，不是页面里面存储的数据内容。

## `operator==` 比较什么

```cpp
x == y
```

只有当两个字段都相同时才返回 `true`：

```text
x.fd       == y.fd
并且
x.page_no  == y.page_no
```

例如：

```text
(fd=3, page_no=5) == (fd=3, page_no=5)  // true
(fd=3, page_no=5) == (fd=4, page_no=5)  // false
(fd=3, page_no=5) == (fd=3, page_no=6)  // false
```

即使两个页面编号相同，只要属于不同文件，也不是同一个 `PageId`。

## `operator<` 比较什么

它定义的是字典序，也可以称为先比较 `fd`、再比较 `page_no`：

```cpp
if (fd != x.fd) {
    return fd < x.fd;
}
return page_no < x.page_no;
```

这里左侧对象是隐含的 `*this`，参数 `x` 是右侧对象。因此：

```cpp
a < b
```

实际相当于比较：

```text
(a.fd, a.page_no) < (b.fd, b.page_no)
```

比较示例：

```text
(fd=2, page_no=9) < (fd=3, page_no=0)   // true
(fd=3, page_no=2) < (fd=3, page_no=8)   // true
(fd=3, page_no=8) < (fd=3, page_no=2)   // false
```

## 为什么需要这两个运算符

`PageId` 会作为容器中的键使用，例如缓冲池的页表：

```cpp
std::unordered_map<PageId, frame_id_t, PageIdHash> page_table_;
```

哈希容器主要需要 `operator==` 来确认两个键是否相等；有序容器（例如 `std::map`、`std::set`）则需要 `operator<` 来确定键的顺序。

## 函数签名的区别

### `operator==`

```cpp
friend bool operator==(const PageId& x, const PageId& y)
```

它是非成员函数，所以显式接收两个对象。`friend` 使它成为 `PageId` 配套的运算符函数。

### `operator<`

```cpp
bool operator<(const PageId& x) const
```

它是成员函数。左侧对象由隐含的 `this` 表示，参数 `x` 表示右侧对象，因此只需要写一个显式参数。

## `const` 与 `noexcept`

- 参数前的 `const PageId&`：以只读引用传参，避免拷贝且不修改对象。
- 成员函数末尾的 `const`：比较不会修改左侧对象。
- `noexcept`：承诺比较过程不抛出异常，适合用于容器键操作。
