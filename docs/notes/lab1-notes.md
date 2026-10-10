# Lab1 学习笔记：基础知识与 DiskManager 复盘

> 整理日期：2026-10-10。由原来的 18 份正文笔记去重整合。
> 当前进度：DiskManager 已完成，接下来学习 LRUReplacer。
> 这是概念与调试复盘，不是实验完整实现。

## 目录

- [1. 当前进度与学习路线](#progress)
- [2. 文件、页面、记录和页面头](#pages)
- [3. PageId：比较与哈希](#page-id)
- [4. C++ 读代码基础](#cpp-basics)
- [5. common.h 中的查询结构](#common-types)
- [6. 内存与字符串复制](#memory)
- [7. DiskManager：文件状态与资源生命周期](#disk-manager)
- [8. 系统调用与常见错误复盘](#syscalls)
- [9. 测试与旧二进制问题](#tests)
- [10. 下一步：LRUReplacer](#next)
- [11. 自测与源码导航](#navigation)

<a id="progress"></a>
## 1. 当前进度与学习路线

学习起点：会一些 SQL，学过 Java OOP，但 C/C++、数据库内核基础需要边做边补。
不必先学完全部数据库理论或全部 C++，先掌握当前接口涉及的知识。

### 已完成

- 开发环境配置：Ubuntu、GCC、CMake，补齐 flex、bison、pytest。
- Debug 配置、服务端与客户端编译。
- DiskManager 文件与页面操作实现。
- 最近一次已记录的 DiskManager 测试：2 个测试通过，退出状态为 0（见第 9 节）。

这里记录的是之前验证的结果，本次笔记整合没有重新运行实验测试。

### 后续顺序与依赖

```text
学习顺序：DiskManager → LRUReplacer → BufferPoolManager → Record Manager

模块关系：
DiskManager ─────┐
                 ├→ BufferPoolManager → Record Manager
LRUReplacer ─────┘
```

DiskManager 与 LRUReplacer 是缓冲池的两个支撑模块，前者负责文件 I/O，
后者选择可淘汰的帧；并不是 LRUReplacer 本身依赖 DiskManager。

### 按需补充的知识

- 指针、引用、const、构造与析构。
- 标准库容器、迭代器、智能指针。
- 资源所有权和 RAII。
- Linux 文件描述符、系统调用、异常。
- 学习 LRU 时补 list、unordered_map、mutex；
  学习缓冲池时补 pin_count、dirty、Page 与 Frame 的区别。

<a id="pages"></a>
## 2. 文件、页面、记录和页面头

### 2.1 文件划分成页面

“一个数据文件划分成许多页面，每页存一些数据”的理解是正确的。
在本项目中：

```cpp
PAGE_SIZE = 4096;  // 每页 4 KB
```

页号从 0 开始，页面起始位置为：

```text
页号 0 → 文件偏移 0
页号 1 → 文件偏移 4096
页号 2 → 文件偏移 8192
文件偏移 = page_no × PAGE_SIZE
```

页面不是一行记录：一页可以包含多条记录和用于管理它们的元数据。
记录可以理解为一段按字段布局编码的字节序列。

### 2.2 页面头与文件头页面

**页面头**是一页内部的管理信息，例如记录数量、空闲页链接。
它不是用户表中的普通列。

**文件头页面**是专门保存整个文件信息的一页。RUCBase 记录文件中：

```text
记录文件
├── 第 0 页：RmFileHdr（记录大小、页数、空闲页链表头等）
├── 第 1 页：LSN | RmPageHdr | bitmap | 定长记录槽
├── 第 2 页：LSN | RmPageHdr | bitmap | 定长记录槽
└── ...
```

不要把“文件的第 0 页”与“每个数据页开头的页头”混为一谈。

`RmPageHdr` 位于 `src/record/rm_defs.h`，主要保存：

- `next_free_page_no`：本页在空闲页链表中的后继页号。
- `num_records`：本页已使用的记录数量。

bitmap 标记哪些记录槽被占用，记录槽中才是实际记录字节。

### 2.3 LSN 与页面内偏移

LSN 是 **Log Sequence Number（日志序列号）**。
日志描述数据库修改；页面 LSN 表示页面已经应用到哪条相关日志。
恢复模块可以结合页面 LSN 判断哪些修改需要重做。Lab1 先认识布局即可。

```cpp
OFFSET_LSN = 0;
OFFSET_PAGE_HDR = 4;
```

这两个数是相对于 `Page::data_` 起点的**字节偏移**：

```text
data_[0..3]：页面 LSN（lsn_t 当前为 int32_t，4 字节）
data_[4..]：从这里开始放记录页头等内容
```

`OFFSET_PAGE_HDR = 4` 表示记录页头的起点，不表示 `RmPageHdr` 自身只有 4 字节。
其大小由 `sizeof(RmPageHdr)` 决定。

这些偏移也不是整个 C++ `Page` 对象的成员偏移。
`id_`、`is_dirty_`、`pin_count_` 是内存管理状态，不等同于 `data_` 中的磁盘字节。

### 2.4 Page、Frame 与 Buffer Pool

你的初步理解“文件被划分成很多 page，每个 page 保存数据”是对的，
但最好把“磁盘页面”和“代码中的 `Page` 对象”分开理解。

#### 三个层次

```text
磁盘文件中的 page
        ↕ read / write
缓冲池中的 frame（内存槽位）
        ↕
代码里的 Page 对象（保存这一帧当前缓存的页内容和状态）
```

- **磁盘页（disk page）**：文件中的固定大小字节区域，位置由
  `page_no * PAGE_SIZE` 确定。它是持久化副本。
- **Frame（帧）**：Buffer Pool 中的一个固定大小内存槽位。一个 frame
  能容纳一页内容；frame 本身不是磁盘文件，也不是某个永久页号。
- **`Page` 对象**：RUCBase 用来表示一个缓冲池槽位当前状态的 C++ 对象，
  内含 `data_`、`PageId`、`pin_count_` 和 dirty 标志。它可以理解为
  “frame 的软件表示”，而不是单纯的磁盘页。

例如缓冲池大小为 2：

```text
frame 0 → 当前缓存 (fd=3, page_no=7)
frame 1 → 当前缓存 (fd=3, page_no=12)
```

如果淘汰 frame 0，之后它可能改为缓存 `(fd=4, page_no=2)`。因此：

```text
page_no：磁盘页的身份
frame_id：当前内存槽位的身份
```

二者不是同一个编号，也不是永久绑定关系。`BufferPoolManager::page_table_`
负责记录：

```text
PageId → frame_id
```

#### 页面访问过程

```text
fetch_page(PageId)
    ├─ 命中 page_table_：直接返回对应 frame 中的 Page
    └─ 未命中：找空闲 frame 或淘汰候选
                 ├─ 脏页先写回磁盘
                 └─ 从磁盘读入新 page，建立 PageId → frame_id 映射
```

使用结束后调用 `unpin_page`；当 `pin_count` 降到 0，frame 才能进入
LRU 的可淘汰集合。`dirty` 表示 frame 中的内容比磁盘副本更新，
并不表示这个 frame 永远属于该 page。

#### 一个生活类比

```text
磁盘文件 = 仓库
磁盘 page = 仓库里编号固定的箱子
Buffer Pool = 工作台
frame = 工作台上的一个位置
Page 对象 = 对这个位置上当前箱子内容的程序记录
```

工作台位置可以换箱子，但箱子的磁盘编号不会因为换到另一个位置而改变。

<a id="page-id"></a>
## 3. PageId：比较与哈希

定义位置：`src/storage/page.h`。

### 3.1 身份由两个字段组成

```text
PageId = (fd, page_no)
fd：文件描述符
page_no：该文件内的页号
```

相等比较要求两个字段都相同：

```cpp
x.fd == y.fd && x.page_no == y.page_no
```

例如 `(3, 5)` 与 `(4, 5)` 不是同一页，即使页号相同。
这比较的是页面身份，不是页面数据内容。

注意：fd 是当前进程中的打开文件标识，关闭后可能被复用，
所以 `(fd, page_no)` 不是跨文件关闭、重新打开后的永久身份。

### 3.2 大小比较与成员函数

`operator<` 先比较 fd，相同再比较页号：

```text
(2, 9) < (3, 0) → true
(3, 2) < (3, 8) → true
```

它定义排序规则，不表示页面内容或重要程度的大小。

非成员 `operator==` 显式接收左右两个参数；
成员 `operator<` 只接收右侧参数，左侧对象是隐含的 `*this`。

### 3.3 hash_key 不保证唯一

当前组合方式是：

```cpp
(static_cast<uint64_t>(static_cast<uint32_t>(fd)) << 16U) |
static_cast<uint32_t>(page_no)
```

先转无符号、再提升到 64 位并左移，避免对有符号负数移位。
但是页号有 32 位，而 fd 只左移 16 位，二者位区间会重叠。

例如：

```text
(fd=1, page_no=0)     → 65536
(fd=0, page_no=65536) → 65536
```

因此不同 PageId 可以产生同一输入。之后的 `std::hash` 与桶映射也不保证唯一。

哈希表的工作可以简化理解为：

```text
哈希 → 找候选桶 → 用 operator== 确认实际键
```

冲突允许存在；过多冲突影响效率，但不是靠“哈希绝对唯一”保证正确性。
若将页号限定在非负且小于 65536 的范围，当前组合在该受限范围内可区分 fd/页号；
这不是当前 int32_t 页号类型所提供的一般保证。

<a id="cpp-basics"></a>
## 4. C++ 读代码基础

### 4.1 与 Java 的对照

| 熟悉的概念 | C++ 对应 |
| --- | --- |
| HashMap | std::unordered_map |
| LinkedList | std::list（迭代器与接口需要另学） |
| synchronized | std::mutex 配合锁管理 |
| 对象引用 | 指针或引用，生命周期规则不同 |
| 资源管理 | 析构、RAII、智能指针，不是垃圾回收 |

### 4.2 常见签名

- `const PageId&`：只读引用，通常避免复制对象。
- 成员函数末尾 `const`：不修改对象的普通成员状态。
- `noexcept`：承诺异常不会从函数传播出去；违反时会终止程序。
- `friend`：允许指定非成员函数或其他类访问私有成员；
  不会把函数变成成员函数。
- `struct` 默认成员公开，`class` 默认私有。
- 成员变量末尾的 `_` 是命名习惯，不是语言强制规则。
- `std::unique_ptr` 表达独占所有权；`std::shared_ptr` 表达共享所有权。
- 指针只有地址信息，不自动携带缓冲区容量。

### 4.3 两张文件映射与 unordered_map

在 `src/storage/disk_manager.h` 的 private 区域：

```cpp
std::unordered_map<std::string, int> path2fd_;
std::unordered_map<int, std::string> fd2path_;
```

例如：

```text
path2fd_："table.tbl" → 3
fd2path_：3 → "table.tbl"
```

前者用路径查打开状态，后者用 fd 找回路径。
它们是 DiskManager 对象的成员，不是操作系统自动维护的表。

| 调用 | 含义 |
| --- | --- |
| contains(key) | 是否存在该键，返回 bool |
| count(key) | 对 unordered_map 返回 0 或 1 |
| at(key) | 获取值；键不存在时抛异常 |
| find(key) | 返回迭代器；没找到时等于 end() |
| operator[] | 获取值；不存在时插入默认值 |
| erase(key) | 删除整个键值对，返回删除数量 0 或 1 |
| erase(iterator) | 按迭代器删除，返回下一个迭代器 |
| erase(first, last) | 删除区间 [first, last)，返回后继迭代器 |
| clear() | 删除全部元素 |

`erase` 是 `<unordered_map>` 提供的标准库成员函数，不是项目函数。
只有按键删除的重载返回删除数量，不要把所有重载的返回值都理解成 0/1。

`fd2path_.count(fd)` 得到的是数字，不是文件路径。
把字符串字面量与这个数字相加会进行指针运算，而不是拼接路径。
读取路径要读取映射值，拼接数字则使用 `std::to_string`。

给 `std::string` 赋 `nullptr` 不是清空或删除，可能触发未定义行为。
删除键值对应使用 `erase`。

<a id="common-types"></a>
## 5. common.h 中的查询结构

这些是此前阅读框架时遇到的内容，先了解含义即可，
不需要为了 Lab1 提前实现查询执行模块。

### 5.1 TabCol 与 operator<

`TabCol` 保存表名和列名，例如 `student.age`。

```cpp
std::make_pair(x.tab_name, x.col_name) <
std::make_pair(y.tab_name, y.col_name)
```

make_pair 构造一个 pair；pair 按字典序比较：
先表名，表名相同再列名。例如：

```text
course.id < student.id < student.name
```

这为 set/map 等有序容器提供排序规则，不是在执行 SQL 的比较条件。

### 5.2 Value 与 set_int

`Value` 保存类型标签与值；union 中的整数、浮点成员共享存储，
需要结合类型标签理解当前保存的是哪一种值。

`set_int` 的准确描述是：

> 将 Value 标记为整数类型，并保存整数值。

它修改内存对象，不写磁盘，也不直接编码 raw 字节缓冲区。
`init_raw` 才把逻辑值编码到 `RmRecord` 的字节数据中。
`int_val_` 是参数，`int_val` 是成员，下划线用于区分名字。

`raw` 不是叫作“raw”的 C++ 类型，而是一个成员名，
其类型是 `std::shared_ptr<RmRecord>`。
`assert` 是调试断言；不满足条件会终止程序，不是普通异常，
而且断言可被构建配置禁用。

### 5.3 Condition 与 CompOp

Condition 保存一条比较条件的描述，不是执行比较的函数。

| 成员 | 含义 |
| --- | --- |
| lhs_col | 左侧列，lhs = left-hand side |
| op | 比较运算符 |
| is_rhs_val | true 使用右侧常量，false 使用右侧列 |
| rhs_col | 右侧是列时的表名与列名 |
| rhs_val | 右侧常量，不是“右侧列的值” |

例如：

```sql
student.age >= 18                 -- 右侧为常量
student.id = score.student_id     -- 右侧为列
```

CompOp 是枚举类型声明，不是“枚举变量”；每个枚举项代表一种比较：

| 枚举项 | 比较 |
| --- | --- |
| OP_EQ | 等于 = |
| OP_NE | 不等于 <> 或 != |
| OP_LT | 小于 < |
| OP_GT | 大于 > |
| OP_LE | 小于或等于 <= |
| OP_GE | 大于或等于 >= |

`SetClause` 则描述赋值目标列和右侧值，例如 UPDATE 中的 `SET age = 18`。

<a id="memory"></a>
## 6. 内存与字符串复制

### 6.1 memset 不分配内存

```cpp
memset(raw->data, 0, len);
```

表示把**已经存在且容量足够**的内存区域的 len 个字节填成零。
分配内存是在创建 RmRecord 时完成的，不是 memset 完成的。

### 6.2 为什么 memcpy 使用 str_val.size()

```cpp
memcpy(raw->data, str_val.c_str(), str_val.size());
```

- len：目标字段的固定容量。
- str_val.size()：源字符串实际字符数，单位在这里是字节，不包含结尾零字符。
- memcpy 第三个参数：复制字节数，源和目标都必须支持这么多字节。

例如把 "cat" 放进 10 字节字段：

```text
先清零 10 字节，再复制 3 字节：
'c' 'a' 't' 0 0 0 0 0 0 0
```

用 len 从源读取可能超过源的有效范围；
如果源长度超过目标容量，当前代码会先抛 StringOverflowError，避免溢出。

**重要边界**：如果字符串恰好占满 len 个字节，字段内部不会留下结尾零字节。
因此这是定长字节字段，不保证总能按以零结尾的 C 字符串来读取。

### 6.3 sizeof(pointer) 不是缓冲区长度

函数参数 `const char* offset` 是地址。
`sizeof(offset)` 得到指针类型的大小，在当前 64 位环境通常为 8，
不是待写入数据长度；文件 I/O 长度来自 num_bytes。

<a id="disk-manager"></a>
## 7. DiskManager：文件状态与资源生命周期

### 7.1 文件存在与文件打开不同

```text
is_file(path)                → 文件系统里是否存在普通文件
path2fd_.contains(path)      → 本对象是否登记了打开状态
fd2path_.contains(fd)        → 本对象是否登记了该 fd
```

文件正常存在是 open_file 的前提，不是打开错误。
“已经打开”也不等于“磁盘上存在”。

### 7.2 各接口职责

| 接口 | 职责与资源规则 |
| --- | --- |
| is_file | 使用 stat 判断普通文件 |
| create_file | 创建文件，关闭创建时取得的临时 fd，不登记打开表 |
| open_file | 打开已有文件，登记两张映射，返回并保留 fd |
| close_file | 关闭已登记的 fd，结束其打开状态并更新映射 |
| destroy_file | 检查本对象的打开状态后删除路径 |
| read_page/write_page | 借用 fd 做一次 I/O，不擅自关闭 |
| allocate_page | 分配页号，不等于创建记录或立即填满磁盘页面 |

因此，create_file 自己拿到临时资源，负责释放；
read_page/write_page 使用调用者管理的 fd，不负责关闭。

`fd2pageno_` 保存每个 fd 下一次待分配的页号，后缀自增先返回旧值再加一。
文件再次打开时，上层记录/索引模块会根据已有元数据设置分配起点。

### 7.3 两张映射的一致性

正常已打开文件需要同时有：

```text
path → fd
fd → path
```

关闭后应清除对应键值对，而不是保留空字符串占位。
本对象的打开表只记录经由该对象管理的文件，
不是“所有进程是否打开这个文件”的全局检查。

<a id="syscalls"></a>
## 8. 系统调用与常见错误复盘

### 8.1 需要的头文件

| 头文件 | 相关接口 |
| --- | --- |
| <fcntl.h> | open、O_CREAT、O_EXCL、O_RDWR、O_WRONLY |
| <unistd.h> | read、write、lseek、close、unlink |
| <sys/stat.h> | stat、S_ISREG |
| <cerrno> | errno、EEXIST、ENOENT 等 |
| <cstring> | std::strerror、内存字节操作 |
| <unordered_map> | 哈希表与 erase 等接口 |

open 标红时先看诊断信息和头文件，再检查是否保存代码、编译数据库配置。
当前项目会生成 `build/debug/compile_commands.json`，可供编辑器使用。

### 8.2 open 标志、错误与创建竞态

- O_CREAT：路径不存在时创建文件；本身不保证拒绝已有文件。
- O_EXCL 配合 O_CREAT：排他创建，处理“检查后被抢先创建”的情况。
- O_RDWR：读写打开已有文件；不会自行创建文件。
- 0600：创建时的权限，表示所有者可读写；还受 umask 影响。
- 没有创建标志时，open 不需要权限参数。

先检查再创建是两步操作，中间可能被其他线程或进程改变文件状态。
系统返回的 errno 必须在失败后及时查看：

| 场景 | 错误含义 |
| --- | --- |
| 创建已有路径 / EEXIST | 对应 FileExistsError |
| 打开不存在文件 / ENOENT | 对应 FileNotFoundError |
| 关闭未登记 fd | 对应 FileNotOpenError(fd) |
| 文件仍登记为打开，尝试删除 | 对应 FileNotClosedError(path) |
| 其他系统错误 | 保留真实诊断，例如 UnixError 或带详情的 InternalError |

异常名称要匹配状态，不应把任何系统调用失败都说成“文件未关闭”。

### 8.3 页面 I/O 的检查点

- 用 fd 和 page_no 定位页面；SEEK_SET 表示相对于文件起点。
- offset 参数虽然叫 offset，实际是内存缓冲区地址，不是文件偏移。
- 本次传输长度是 num_bytes，可以小于 PAGE_SIZE，例如只处理页头。
- 返回字节数不等于请求长度时，要按实验接口契约报告错误。
- 系统调用使用 ssize_t 表示字节计数或负错误；文件偏移使用 off_t。
- 页号乘页大小时，应在合适的宽类型中计算，避免先做窄整数乘法后再赋给 off_t。
- `!=` 才是不等于；`=!` 会被解析为赋值与逻辑取反。
- 接口错误通常通过异常上报；exit 会结束整个进程。
- throw 之后同一条执行路径中的代码不会继续执行。
- perror 用于基于 errno 打印诊断，短读/短写不一定设置新的 errno。

更完整的 I/O 代码可能处理短读、短写、被信号中断等情形；
这里保留实验“检查返回字节数”的契约，不扩展成完整系统实现。

### 8.4 纠正两条旧笔记结论

**unlink 不是打开状态检测。** Linux 允许移除仍打开文件的目录项；
打开描述符仍可访问文件，直到最后一个引用被关闭。
“未关闭则禁止删除”是本项目通过打开表实现的规则，不是 unlink 自动保证的。
参见 [Linux unlink(2) 手册](https://man7.org/linux/man-pages/man2/unlink.2.html)。

**close 失败不意味着 fd 一定还打开。** 旧笔记中
“失败时始终保留映射、方便重试”过于绝对：
Linux 对有效 fd 的关闭通常在报告后续错误前就已经释放描述符。
盲目重试可能关闭其他线程新取得的同编号 fd。
应区分错误、保留关闭失败的诊断，并按目标平台约定管理状态，
而不是把保留映射和重复 close 当成通用策略。
参见 [Linux close(2) 手册](https://man7.org/linux/man-pages/man2/close.2.html)。

上述纠正仅更新笔记，本次未修改源码。

<a id="tests"></a>
## 9. 测试与旧二进制问题

### 9.1 历史结果，不是当前整套 Lab1 状态

刚开始时曾运行整套 Lab1：8 个测试目标中 3 个通过、5 个失败。
当时多个模块仍是 TODO，这是初始骨架基线。

后来直接运行 DiskManager 测试出现大量错误：
未抛预期异常、创建后文件不存在、无效 fd 断言终止，退出状态 134。
诊断发现源码更新时间晚于可执行文件，运行的是旧二进制。

重新编译后的已记录结果：

```text
DiskManagerTest.FileOperation：通过
DiskManagerTest.PageOperation：通过
[  PASSED  ] 2 tests.
退出状态：0
```

不要把最初的整套失败基线继续当作现在的测试结果。
两个测试通过也不等于所有异常路径和并发场景都已覆盖。

### 9.2 每次改动后先编译再运行

在仓库根目录：

```bash
cmake --build --preset debug --target lab1_disk_manager_test -j 4 &&
./build/debug/bin/lab1_disk_manager_test
```

也可以精确选择一个测试：

```bash
./build/debug/bin/lab1_disk_manager_test --gtest_filter=DiskManagerTest.FileOperation
./build/debug/bin/lab1_disk_manager_test --gtest_filter=DiskManagerTest.PageOperation
```

使用 &&，避免编译失败后又运行旧程序，误以为测到了新代码。

### 9.3 失败排查顺序

1. 源码是否保存？
2. 对应目标是否编译成功？
3. 是否运行当前构建目录的二进制？
4. 先读第一个失败：Expected 是期望，Actual 是实际。
5. 对照对应测试行和接口约定，而不是先修改下游断言。
6. 记录命令、退出状态和关键输出，区分原始问题与连锁失败。

查看时间：

```bash
stat src/storage/disk_manager.cpp build/debug/bin/lab1_disk_manager_test
```

日志导航：单元测试可查看 `build/debug/Testing/Temporary/LastTest.log`；
黑盒测试的失败日志通常在 `build/debug/test-logs/`。

<a id="next"></a>
## 10. 下一步：LRUReplacer

### 阅读范围

```text
src/replacer/replacer.h
src/replacer/lru_replacer.h
src/replacer/lru_replacer.cpp
src/test/storage/lru_replacer_test.cpp
```

先掌握 list、unordered_map、迭代器和互斥锁，理解：
这里管理的是**可淘汰的帧编号**，不是所有磁盘页面。

可以先在纸上追踪：

```text
unpin(1) → unpin(2) → unpin(3) → victim() → victim()
```

问题：

- pin 与 unpin 为什么会改变“可淘汰”集合？
- 同一帧重复 unpin 是否应重复加入？
- victim 在空集合时如何表示没有候选？
- 链表与哈希表应该保持什么一致性？
- 若多个线程操作，两张结构的一次更新怎样避免被打断？

只编译与运行当前目标：

```bash
cmake --build --preset debug --target lab1_lru_replacer_test -j 4 &&
./build/debug/bin/lab1_lru_replacer_test
```

后续 BufferPoolManager 再重点看 pages_、page_table_、free_list_、replacer_，
用容量为 2 的例子记录页身份、帧号、pin_count 和 dirty 的变化。
PageGuard 属于已提供的框架内容，不是 Lab1 要重新实现的接口。

### 10.1 `victim` 的实现检查点

当前 `LRUlist_` 的约定是：

```text
链表头部：最近被 unpin 的帧
链表尾部：最久未被 unpin 的帧
```

因此 `victim` 取 `LRUlist_.back()`、再 `pop_back()` 的方向是正确的。
但 `LRUhash_` 保存着同一帧在链表中的迭代器，淘汰时必须同时删除对应哈希表项，
否则会留下悬空迭代器，破坏以下不变量：

```text
frame 在 LRUlist_ 中 ↔ frame 在 LRUhash_ 中
```

还要注意：

- 函数开头已有 `std::scoped_lock`，链表和哈希表的一次修改应在同一把锁内完成；
- 空集合时不能解引用 `frame_id`，直接返回 `false`；
- 非空时再写入输出指针。调用者按抽象接口传入有效输出地址；不要把输出参数本身和“没有 victim”混为一谈；
- `victim` 成功后，`Size()` 应减少 1；
- 后续实现 `pin` 时也要同时从 list 和 hash 删除；`unpin` 要避免同一帧重复插入；
- `Size()` 读取链表大小时也要考虑与其他操作的并发访问。

最小手算：

```text
unpin(1), unpin(2), unpin(3)
头 → [3, 2, 1] → 尾
victim() 返回 1，剩余 [3, 2]
```

### 10.2 `pin` 与 `find` 的迭代器检查

`pin(frame_id)` 的目标是：如果该帧当前位于可淘汰集合，就把它从链表和哈希表同时移除；
如果不在集合中，则不做任何事。

```cpp
auto it = LRUhash_.find(frame_id);
```

`find` 找不到时返回 `LRUhash_.end()`。此时不能访问：

```cpp
it->second
```

因为 `end()` 不代表一个有效元素。应先比较是否等于 `end()`，找到后再用保存的链表迭代器删除：

```text
未找到 → return
找到 → LRUlist_.erase(it->second)，再删除对应 map 项
```

`std::scoped_lock` 已经保护了这次链表和哈希表的成对修改。删除 map 项时可以使用迭代器，
避免再次按键查找；关键仍是保持两个容器同步。

### 10.3 为什么 `pin` 要从两个容器删除

`LRUlist_` 和 `LRUhash_` 不是两份独立数据，而是同一个可淘汰集合的两种视图：

```text
LRUlist_：维护顺序
LRUhash_：通过 frame_id 快速找到顺序中的位置
```

假设当前状态是：

```text
LRUlist_：头 → [7, 4, 2] ← 尾
LRUhash_：
    7 → 指向链表中的 7
    4 → 指向链表中的 4
    2 → 指向链表中的 2
```

如果 `pin(4)` 只从链表删除，结果会变成：

```text
链表：7, 2
哈希表：仍然有 4 → 原来的链表位置
```

此时哈希表会错误地声称 4 仍可淘汰；而且原链表迭代器已经失效，之后再用它可能产生未定义行为。
如果只从哈希表删除，链表中仍有 4，`victim()` 仍可能把一个已经 pin 的帧淘汰。

所以 `pin` 必须把同一个帧从两个容器都删除。`victim` 也一样：淘汰一个帧时两边都要删除。

### 10.4 `auto` 不是任意类型

`auto` 是 C++ 的**编译期类型推导**关键字，不是 JavaScript 那样的动态类型，也不是“随意放任何类型”。

例如：

```cpp
auto number = 3;                    // 推导为 int
auto name = std::string("Alice");  // 推导为 std::string
auto it = LRUhash_.find(frame_id);  // 推导为该 unordered_map 的迭代器类型
```

第三行的完整类型很长，大致类似：

```cpp
std::unordered_map<frame_id_t, std::list<frame_id_t>::iterator>::iterator it;
```

使用 `auto` 只是省略书写这个复杂类型；编译器在编译时已经确定它是什么类型。

它仍然有固定类型，不能之后把同一个变量改成另一种类型：

```cpp
auto it = LRUhash_.find(frame_id);  // 迭代器
// it = 3;                          // 类型不匹配，编译错误
```

`auto` 通常适合接收函数返回值，尤其是迭代器、模板类型和智能指针；学习阶段也可以暂时写完整类型，帮助观察实际类型。

### 10.5 `id->second` 为什么不是下一个节点

这里有两层不同的迭代器：

```cpp
auto id = LRUhash_.find(frame_id);
```

`id` 是**哈希表的迭代器**。哈希表的一个元素是键值对：

```text
id->first   = frame_id
id->second  = 一个 list<frame_id_t>::iterator
```

因此 `id->second` 不是链表中的下一个元素，而是“指向链表中当前 frame 的位置”。

例如：

```text
LRUlist_：头 → [7] → [4] → [2] ← 尾

LRUhash_：
    7 → iterator 指向 [7]
    4 → iterator 指向 [4]
    2 → iterator 指向 [2]
```

执行：

```cpp
auto id = LRUhash_.find(4);
```

得到的关系是：

```text
id->first  = 4
id->second = 指向 LRUlist_ 中 [4] 的迭代器
```

所以：

```cpp
LRUlist_.erase(id->second);
```

意思是“删除链表中迭代器指向的 `[4]` 节点”。它不会删除 `[4]` 的下一个节点 `[2]`。

如果要访问下一个链表节点，才会写：

```cpp
auto next = std::next(id->second);
```

`std::next` 会从当前链表迭代器向后移动一步，但这不是 `erase(id->second)` 的含义。

也可以把整个类型展开：

```cpp
std::unordered_map<
    frame_id_t,
    std::list<frame_id_t>::iterator
>::iterator id;
```

外层 `id` 遍历哈希表；`id->second` 保存的内层迭代器才用于定位链表节点。

删除时通常分两步：

```text
1. 用 id->second 从 list 删除对应节点
2. 用 id 从 hash 删除对应键值对
```

### 10.6 `unpin` 的检查

当前思路：

```text
加锁 → 查重 → 已存在则返回 → push_front → 记录 begin() 迭代器
```

这与 LRU 约定一致：最近一次 unpin 的帧放在链表头部，`victim()` 从尾部取最旧帧。
`unpin` 的参数已经指定了要取消固定的 frame，它不负责选择 victim；选择 victim 是另一个函数的职责。

还要检查构造函数传入的 `max_size_`：replacer 的可淘汰集合不能超过缓冲池容量。
实现前应明确“达到容量后再 unpin 一个新帧”的处理约定（通常保持容量上限，不再加入），
并观察课程测试是否覆盖这个边界。

重复 unpin 直接返回是必要的，否则同一 frame 会在 list 中出现多次，而 map 只能保存一个位置，
两个容器就会失去一致性。

插入后的不变量应是：

```text
LRUlist_.front() == 最近 unpin 的 frame
LRUhash_[frame] == 指向该 frame 在 LRUlist_ 中节点的迭代器
LRUlist_.size() == LRUhash_.size()
LRUlist_.size() <= max_size_
```

### 10.7 当前 LRUReplacer 测试结果

在修改 `src/replacer/lru_replacer.cpp` 后，执行：

```bash
cmake --build --preset debug --target lab1_lru_replacer_test -j 4
./build/debug/bin/lab1_lru_replacer_test
```

结果：

```text
SimpleTest       通过
MixTest          通过
ConcurrencyTest  通过
3 个测试全部通过，退出状态为 0
```

这说明当前实现已经满足现有三组测试覆盖的基本 LRU 顺序、重复 unpin、pin/victim 配合和并发操作场景。
进入 BufferPoolManager 前仍应自己检查几个测试未必覆盖的边界：

- `unpin` 超过 `max_size_` 时的容量约定；
- `Size()` 与其他操作并发时是否需要同一把锁；
- `victim` 的输出指针和空集合行为；
- 每次 list 修改是否同步修改 hash，避免悬空迭代器。

<a id="navigation"></a>
## 11. 自测与源码导航

### 复习问题

- [ ] 能解释 Page、Frame、Record 的区别。
- [ ] 能区分文件头页、记录页头与 LSN。
- [ ] 能解释 PageId 为什么包含 fd 和 page_no，以及 fd 复用的影响。
- [ ] 知道哈希冲突为何不等于键相等。
- [ ] 能解释 len、str_val.size()、sizeof(pointer) 的区别。
- [ ] 能区分逻辑 Value 与 raw 字节缓冲区。
- [ ] 能解释 is_file 与打开文件表检查的区别。
- [ ] 知道 fd2pageno_ 的含义，以及分配页号不等于分配记录。
- [ ] 能解释 create_file 与 open_file 的资源职责。
- [ ] 知道读取映射与删除映射分别使用什么标准库操作。
- [ ] 知道修改后先构建，再执行测试。

### 定位陌生成员

例如不知道 fd2path_ 在哪里：

```bash
grep -RIn 'fd2path_' src
```

或在编辑器中使用“转到定义”。
先看 .h 中的成员类型和访问权限，再看 .cpp 中如何使用，
不要只看一行调用猜测它的职责。

### 资料入口

- [Lab1 实验文档](../RUCBase-Lab1[存储管理].md)
- [RUCBase 使用文档](../RUCBase使用文档.md)
- [记录模块布局](../../src/record/README.md)
- [Page 定义](../../src/storage/page.h)
- [DiskManager 接口](../../src/storage/disk_manager.h)
- [DiskManager 源码](../../src/storage/disk_manager.cpp)
- [DiskManager 测试](../../src/test/storage/disk_manager_test.cpp)

### 整合说明

原笔记中的学习路线、C++ 类型解释、内存复制、页面布局、文件管理复盘和测试诊断，
都已归入本文。重复的逐轮草稿说明合并成检查点，
过时的“尚未开始 DiskManager”待办替换为 LRU 学习计划。
旧笔记可通过 Git 历史回看；后续同阶段笔记继续补充本文，避免再按每个问题拆文件。
