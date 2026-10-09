# `DiskManager::open_file` 草稿检查

当前草稿的主要问题是把“文件存在”和“文件已经打开”混为一谈。

## 两种状态要区分

```text
is_file(path)       文件系统中是否存在普通文件
path2fd_.contains   当前 DiskManager 是否已经打开该文件
```

`open_file` 的控制流应是：

```text
文件不存在 → FileNotFoundError
当前对象已经打开 → 重复打开错误（按项目约定）
调用 open(path, O_RDWR)
成功 → 更新 path2fd_ 和 fd2path_，返回 fd
失败 → 根据 errno 转换异常
```

## 当前草稿的问题

```cpp
if (is_file(path)) {
    throw FileExistsError(path);
}
```

这会在文件正常存在时直接抛错；但正常的 `open_file` 正是要打开一个已经存在的文件。应先处理“不存在”和“已经由当前对象打开”这两个不同条件。

另外：

- `O_RDWR` 不创建文件，因此 `errno == EEXIST` 不属于这里的主要失败分支；
- 传给不含 `O_CREAT` 的 `open` 的 `0600` 模式参数没有实际作用；
- 成功后必须写入 `path2fd_[path]` 和 `fd2path_[fd]`；
- 函数必须返回成功得到的 `fd`；
- 失败消息中的 `raed` 是拼写错误；
- 当前版本没有成功路径的 `return`。

## 与 `create_file` 的区别

```text
create_file：文件应该不存在；创建后关闭临时 fd
open_file：文件应该已经存在；打开后保留 fd 并登记映射
```

## 对新草稿的检查

新草稿中的第一段：

```cpp
if (is_file(path)) {
    throw FileNotFoundError(path);
}
```

判断和异常都不对：

- 文件存在正是 `open_file` 的正常前提，因此应关注“不存在”的情况；
- `FileNotFoundError` 应对应 `!is_file(path)`，而不是文件存在。

第二段：

```cpp
if (path2fd_.contains(path)) {
    throw FileNotClosedError(path);
}
```

方向是对的：该映射存在时，说明当前 `DiskManager` 已经打开了该路径。课程代码没有单独的“重复打开”异常，使用哪个项目异常应以测试和现有接口约定为准；`FileNotClosedError` 的语义是“文件仍处于打开状态”。

打开调用成功后还必须完成两个动作：

```text
path2fd_ 记录 path → fd
fd2path_ 记录 fd → path
返回 fd
```

失败分支中：

- `O_RDWR` 不创建文件，`ENOENT` 更可能表示文件不存在，应对应 `FileNotFoundError`；
- `EEXIST` 不是这里的主要错误分支，因为没有使用 `O_CREAT`；
- `0600` 只有在 `O_CREAT` 时才是权限参数，这里可以省略；
- 当前代码没有成功路径的 `return`，会导致非 `void` 函数缺少返回值。

建议先按状态转换画图：

```text
磁盘中不存在 → FileNotFoundError
磁盘中存在但本对象已打开 → 重复打开错误
磁盘中存在且未打开 → open → 登记两个映射 → return fd
```
