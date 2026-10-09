# `create_file` 中 `open()` 标红的常见原因

`disk_manager.cpp` 当前已有：

```cpp
#include <unistd.h>
```

它提供 `close`、`read`、`write`、`lseek` 等声明，但 `open` 及其标志通常需要额外头文件：

```cpp
#include <fcntl.h>   // open, O_CREAT, O_EXCL, O_WRONLY
#include <cerrno>    // errno, EEXIST
#include <cstring>   // std::strerror
```

因此编辑器把 `open()`、`O_CREAT` 或 `EEXIST` 标红时，先检查对应头文件是否存在。修改头文件后重新配置或重新生成 `compile_commands.json`，必要时重启语言服务器。

## 当前代码状态

本次构建检查显示，工作区里的 `src/storage/disk_manager.cpp` 中 `create_file` 仍然是 TODO；你在消息中贴出的实现还没有写入文件。因此编译成功不能证明那段代码已经通过编译。

## 贴出版本的逻辑检查

整体控制流是合理的：

```text
预检查已存在文件 → FileExistsError
O_CREAT | O_EXCL 排他创建 → 检查失败
成功后关闭临时 fd
```

还要注意：

- `fd == -1` 时不要 `close(fd)`；
- `throw` 后不要再写 `exit(1)`；
- `O_EXCL` 失败时用 `errno == EEXIST` 区分“竞争中已经存在”和其他系统错误；
- 成功创建得到的临时 fd 要关闭，但不要放入打开文件映射。
