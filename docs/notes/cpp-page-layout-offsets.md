# `Page::OFFSET_LSN` 与 `Page::OFFSET_PAGE_HDR`

代码：

```cpp
static constexpr size_t OFFSET_LSN = 0;
static constexpr size_t OFFSET_PAGE_HDR = 4;
```

它们是页面内部布局的**字节偏移量**，不是两个页面，也不是 `Page` 对象成员变量的偏移。

## 页面可以看成一段字节数组

```cpp
char data_[PAGE_SIZE];
```

页面数据从 `data_[0]` 开始。偏移量表示某种元数据位于这段数组的第几个字节。

当前布局可以粗略表示为：

```text
字节 0                 字节 4
|----------------------|------------------------------|
| LSN                  | 记录页头 / 其他页面内容     |
|                      |                              |
```

## `OFFSET_LSN = 0`

表示页面的 LSN 从页面数据起点开始：

```cpp
get_data() + OFFSET_LSN
```

就是：

```cpp
get_data() + 0
```

LSN 是 *Log Sequence Number*，日志序列号。恢复模块可以用它判断页面对应的日志更新进度。Lab1 主要认识布局即可，不需要在这里实现恢复逻辑。

## `OFFSET_PAGE_HDR = 4`

表示通用页面区域之后，从第 4 个字节开始放页面的其他头部信息。

当前实现预留前 4 个字节给 LSN，因此记录页的 `RmPageHdr` 从这里开始：

```cpp
page_hdr = reinterpret_cast<RmPageHdr*>(page->get_data() + Page::OFFSET_PAGE_HDR);
```

记录页面布局可以理解为：

```text
Page::OFFSET_LSN = 0
┌─────────────────────────────┐
│ 通用页面信息：LSN（4 字节） │
├─────────────────────────────┤ ← OFFSET_PAGE_HDR = 4
│ RmPageHdr                   │
│ bitmap                      │
│ 定长记录槽                  │
└─────────────────────────────┘
```

## 为什么偏移量是 `size_t`

偏移量不会是负数，且用于和内存地址相加，因此使用无符号的 `size_t` 表示比较合适。

## 一个需要留意的地方

`OFFSET_PAGE_HDR = 4` 表示“记录页头从第 4 字节开始”，不是说所有页面头本身都只有 4 字节。具体的 `RmPageHdr` 大小由：

```cpp
sizeof(RmPageHdr)
```

决定。

## 与访问函数的关系

```cpp
inline lsn_t get_page_lsn() {
    return *reinterpret_cast<lsn_t*>(get_data() + OFFSET_LSN);
}
```

它从偏移 0 处读取 LSN；`set_page_lsn()` 则把新的 LSN 写回偏移 0 处。后续结构不能从偏移 0 开始，否则会覆盖 LSN。
