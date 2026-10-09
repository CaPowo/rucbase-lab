# 为什么 `memcpy` 使用 `str_val.size()` 而不是 `len`

相关代码：

```cpp
if (len < (int)str_val.size()) {
    throw StringOverflowError();
}
memset(raw->data, 0, len);
memcpy(raw->data, str_val.c_str(), str_val.size());
```

## 两个长度的含义不同

- `str_val.size()`：源字符串实际拥有的字符数。
- `len`：目标字段在记录中分配的固定容量。

例如：

```text
str_val = "cat"       实际长度为 3
len = 10              字段容量为 10
```

此时只应从源字符串复制 3 个字符，剩余 7 个字节保持为前面 `memset` 写入的 0。

## 如果使用 `len` 会发生什么

当 `len > str_val.size()` 时，`memcpy` 会尝试从 `str_val.c_str()` 指向的源区域读取超过字符串实际内容的字节。`c_str()` 至少保证末尾有一个 `\0`，但不保证后面还有 `len` 个可读字节，因此这可能造成越界读取，属于未定义行为。

当 `len < str_val.size()` 时，复制会截断字符串。当前代码在复制前已经抛出 `StringOverflowError`，所以这个情况不会继续执行。

## 为什么先 `memset` 再复制

这是定长字段的常见做法：

```text
先把整个字段容量清零
再复制实际字符串内容
```

这样 `cat` 存入 10 字节字段后，内存效果近似为：

```text
'c' 'a' 't' 0 0 0 0 0 0 0
```

因此虽然 `str_val.size()` 不包含字符串结尾的 `\0`，字段后面的零值已经由 `memset` 提供。

## 结论

这里应使用 `str_val.size()`，因为 `memcpy` 的第三个参数表示“从源地址读取多少字节”，而不是目标缓冲区总容量。`len` 表示目标字段容量，适合用于分配、清零和越界检查。
