# `PageId::hash_key()` 是否保证唯一

代码：

```cpp
uint64_t hash_key() const noexcept {
    return (static_cast<uint64_t>(static_cast<uint32_t>(fd)) << 16U) |
           static_cast<uint32_t>(page_no);
}
```

## 结论

不能直接说它“确保 `PageId` 唯一”。它生成的是哈希输入，主要用于快速分桶；哈希值允许冲突。

`PageId` 是否相等，最终仍由：

```cpp
operator==(const PageId&, const PageId&)
```

确认。`std::unordered_map<PageId, ...>` 的查找流程可以理解为：

```text
先用 hash_key() 找桶
再用 operator== 区分同一桶里的不同 PageId
```

## 这段位操作做了什么

```cpp
static_cast<uint32_t>(fd)
```

把 `fd` 转成无符号 32 位整数；再转换为 `uint64_t` 后左移 16 位，避免对有符号数移位。

```cpp
static_cast<uint32_t>(page_no)
```

把页号转成无符号形式，然后与前半部分按位或。

可以把它粗略理解为把 `fd` 放到较高位、把 `page_no` 放到低位，但这里左移只有 16 位，而 `page_no` 有 32 位，因此两个字段的位区间会重叠。

## 一个可能冲突的例子

以下两个 `PageId` 不相等：

```text
(fd=1, page_no=0)
(fd=0, page_no=65536)
```

但它们的 `hash_key()` 都可能得到：

```text
0x00010000
```

这就是哈希冲突。它不会破坏 `unordered_map` 的正确性，因为容器还会调用 `operator==` 做最终确认；但冲突过多会降低查找效率。

## 什么时候可以唯一

如果额外保证：

```text
fd 的有效范围适合放入高位
page_no 始终小于 2^16
```

那么在这个受限范围内，16 位低位可以容纳页号，编码可以保持不同组合不同。但当前 `page_id_t` 是 `int32_t`，代码本身没有在 `hash_key()` 中声明页号必须小于 `65536`，所以不能把这种条件当成普遍保证。

## 更准确的注释

原注释：

```cpp
// 将文件描述符fd和页号编码为无符号哈希输入，避免有符号移位。
```

这个注释是准确的。它说的是“哈希输入”，没有声称“唯一编码”。
