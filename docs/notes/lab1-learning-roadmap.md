# Lab1：存储管理学习路线

## 当前状态

- 开发环境已经配置完成。
- `flex`、`bison`、`python3-pytest` 已安装。
- `cmake --preset debug` 已成功。
- `cmake --build --preset debug-client -j 4` 已成功。
- 已生成：`build/debug/bin/rmdb`、`build/debug/bin/rucbase_client`。
- 工作区源码没有改动。

## Lab1 的模块依赖

```text
DiskManager → LRUReplacer → BufferPoolManager → Record Manager
```

学习和实现也按这个顺序进行，不跳过底层模块直接处理记录管理器。

## 需要补齐的最小前置知识

### C++

- 指针与引用：`Page *page`、`const PageId &page_id`
- `const`、构造函数、析构函数
- `std::unique_ptr`
- `std::list`
- `std::unordered_map`
- `std::mutex`
- RAII 和资源生命周期

Java 对照：

| Java | C++ |
| --- | --- |
| `HashMap` | `std::unordered_map` |
| `LinkedList` | `std::list` |
| `synchronized` | `std::mutex` 加锁 |
| 引用 | 指针或引用 |
| 自动资源管理 | RAII / `unique_ptr` |

### Linux 文件操作

需要理解以下系统调用的作用和返回值：

```text
open  read  write  close  unlink
```

其中 `fd` 是文件描述符，表示一个已经打开的文件。

### 数据库存储概念

- **Page**：磁盘上的固定大小数据块。
- **Frame**：缓冲池内存中的一个槽位。
- **PageId**：由文件描述符和页面编号共同确定页面。
- **pin_count**：页面当前被多少调用者使用。
- **dirty**：内存页面是否比磁盘上的副本更新。
- **eviction**：缓冲池满时淘汰页面。
- **LRU**：优先淘汰最近最少使用的页面。

## 当前测试基线

执行：

```bash
cmake --build --preset debug -j 4
ctest --preset lab1 --output-on-failure
```

当前结果：8 个 Lab1 测试中 3 个通过，5 个失败。失败模块包括：

```text
DiskManager
LRUReplacer
BufferPoolManager
PageGuard
RecordManager
```

这些失败与代码中的未完成 `Todo` 有关，环境本身已经能够完成配置和编译。

## 第一阶段：只读代码，不急着修改

先阅读：

```text
src/common/config.h
src/storage/page.h
src/storage/disk_manager.h
src/storage/disk_manager.cpp
src/test/storage/disk_manager_test.cpp
```

阅读后需要自己回答：

1. `PAGE_SIZE` 是多少？
2. `PageId` 为什么同时需要 `fd` 和 `page_no`？
3. `fd2pageno_` 保存什么状态？
4. `write_page()` 如何定位文件中的页面？
5. `create_file()` 和 `open_file()` 为什么要维护文件映射？

## 第二阶段：DiskManager

按以下流程理解测试：

```text
创建文件 → 打开文件 → 分配页面编号 → 写页 → 读页 → 关闭文件 → 删除文件
```

单独运行：

```bash
./build/debug/bin/lab1_disk_manager_test
```

## 第三阶段：LRUReplacer

阅读：

```text
src/replacer/lru_replacer.h
src/replacer/lru_replacer.cpp
src/test/storage/lru_replacer_test.cpp
```

手动追踪：

```text
unpin(1)
unpin(2)
unpin(3)
victim()
victim()
```

目标是弄清楚每个操作后链表的顺序，以及 `victim()` 为什么返回某个帧。

单独运行：

```bash
./build/debug/bin/lab1_lru_replacer_test
```

## 第四阶段：BufferPoolManager

重点理解四个成员：

```text
pages_
page_table_
free_list_
replacer_
```

先用容量为 2 的缓冲池，手动模拟：

```text
new_page
new_page
unpin_page
fetch_page
new_page
```

需要记录每一步的页面表、空闲帧、pin_count 和 dirty 状态。

## 当前待办

- [ ] 阅读 `config.h` 和 `page.h`。
- [ ] 回答第一阶段的 5 个问题。
- [ ] 阅读 DiskManager 测试，画出文件操作流程。
- [ ] 只运行 `lab1_disk_manager_test`，保存失败信息。
- [ ] 在理解后再开始修改第一个 Todo。
