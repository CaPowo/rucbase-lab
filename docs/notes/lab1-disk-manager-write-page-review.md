# `DiskManager::write_page` 草稿检查

当前草稿的总体流程是对的：

```text
根据 page_no 计算文件偏移 → lseek 定位 → write 写入 num_bytes 字节 → 检查返回值
```

但需要修正以下问题。

## 1. 使用项目已有的 `PAGE_SIZE`

不要在函数里重新定义：

```cpp
int PGAE_SIZE = 4096;
```

项目已经在 `common/config.h` 中定义：

```cpp
static constexpr int PAGE_SIZE = 4096;
```

这样可以避免拼写错误和重复常量。草稿中的 `PGAE_SIZE` 还是 `PAGE_SIZE` 的拼写颠倒。

## 2. `sizeof(offset)` 是指针大小，不是数据长度

函数参数：

```cpp
const char* offset
```

这里的 `offset` 是一个地址。`sizeof(offset)` 得到的是指针大小，通常是 8 字节，而不是调用者想写入的字节数。

`write` 的第三个参数应该使用 `num_bytes`，因为它表示从 `offset` 开始写入多少字节。

## 3. `=!` 不是“不等于”

草稿中：

```cpp
write(...) =! num_bytes
```

这里把 `=!` 写成了赋值符号和逻辑取反的组合，不能表达比较。

不等于运算符是：

```cpp
!=
```

你要比较的是 `write` 的返回值和 `num_bytes` 是否不同。

## 4. `close(fd)` 不应由 `write_page` 负责

`fd` 是由文件管理接口打开并维护的。`write_page` 只是对已打开文件执行一次写入，不应在失败时关闭它，否则上层后续还会拿着已经关闭的文件描述符继续操作。

另外，`throw` 或 `exit` 之后的代码不会执行：

```cpp
throw InternalError(...);
close(fd);       // 到不了这里
exit(1);         // 到不了这里
```

本项目使用异常报告错误，优先保持函数的异常风格，不要在这里调用 `exit(1)`。

## 5. `lseek` 失败时的处理

需要判断 `lseek` 的返回值是否为 `(off_t)-1`。失败时应报告错误并抛出项目使用的异常类型；不要关闭借用的 `fd`。

偏移量的逻辑是：

```text
文件偏移 = page_no × PAGE_SIZE
```

计算时注意把页号和页面大小转换到适合 `off_t` 的类型，避免不必要的有符号类型问题。

## 6. 函数参数的含义

```cpp
fd         已打开文件的描述符
page_no    要写入的页面编号
offset     内存中待写入数据的起始地址
num_bytes  本次实际写入的字节数
```

`offset` 是内存地址，不是文件偏移量；文件偏移量由 `fd`、`page_no` 和 `PAGE_SIZE` 计算出来。

## 7. 按实验注释建立检查清单

先不要一次改很多内容，可以按这个顺序自查：

1. 页面偏移是否为 `page_no * PAGE_SIZE`？
2. `lseek` 是否检查失败？
3. `write` 的第二个参数是否是内存缓冲区 `offset`？
4. `write` 的第三个参数是否是 `num_bytes`？
5. 是否比较了返回字节数和 `num_bytes`？
6. 是否没有关闭调用者传入的 `fd`？
7. 是否使用异常而不是 `exit(1)`？

## 8. 关于短写

本实验注释明确要求检查 `write` 返回值是否等于 `num_bytes`。先按照这个契约完成最小版本，再观察测试结果；更完整的系统代码可能需要循环处理短写，但不要在尚未理解基本流程时同时引入额外复杂度。

## 9. 对 `write` 片段的进一步检查

下面这种顺序有问题：

```cpp
int result = write(fd, offset, num_bytes);
close(fd);
if (result != num_bytes) {
    // ...
}
```

原因：

- `write_page` 借用 `fd`，不拥有它；不能在这里关闭文件。
- 如果 `write` 失败，先调用 `close` 还可能改变后续错误处理所依赖的 `errno`。
- `throw` 后也不需要再 `close`，文件生命周期由 `close_file` 统一管理。

应保持这样的逻辑顺序：

```text
write → 立即检查返回值 → 成功返回，或抛出异常
```
