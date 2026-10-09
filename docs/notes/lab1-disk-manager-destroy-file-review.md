# `DiskManager::destroy_file` 草稿检查

当前草稿：

```cpp
if (unlink("test.txt") < 0) {
    throw FileNotClosedError("file not closed");
}
```

## 需要修正的地方

### 1. 应使用参数 `path`

函数接收的路径是：

```cpp
const std::string& path
```

删除时应该围绕这个参数思考，而不是固定删除某个测试文件名。否则调用者传入的文件不会被处理。

### 2. `unlink` 失败不一定表示文件仍打开

失败原因还可能是：

- 文件不存在；
- 权限不足；
- 路径不是普通文件；
- 其他系统错误。

因此不能把所有 `unlink` 失败都转换为 `FileNotClosedError`。

### 3. 先检查本对象的打开文件表

`DiskManager` 维护：

```cpp
path2fd_  // 路径 → fd
fd2path_  // fd → 路径
```

如果 `path2fd_` 中仍有该路径，说明文件仍由当前 `DiskManager` 打开，应先抛出 `FileNotClosedError(path)`，不要调用 `unlink`。

### 4. 文件不存在时应报告 `FileNotFoundError`

测试会在删除成功后再次删除同一路径，并期待 `FileNotFoundError`。可以先用 `is_file(path)` 判断，或根据 `unlink` 失败时的 `errno == ENOENT` 区分。

### 5. 删除成功后不需要更新打开表

因为打开检查已经保证该路径没有对应的打开 fd。成功 `unlink` 后，文件名目录项被移除；后续再次创建同名文件时可以重新建立文件。

## 建议的控制流

```text
检查 path 是否在 path2fd_ 中
    是 → FileNotClosedError(path)
检查 path 是否存在
    否 → FileNotFoundError(path)
调用 unlink(path.c_str())
    成功 → 返回
    失败 → 根据 errno 抛出对应系统/内部异常
```

先完成这个控制流，再运行 `lab1_disk_manager_test` 验证。

## 对新草稿的检查

新草稿中：

```cpp
if (!path2fd_.contains(path)) {
    throw FileNotClosedError(path);
}
```

判断方向反了。`path2fd_` 中**存在**路径才表示该文件仍被当前 `DiskManager` 打开；不存在表示没有打开记录，可以继续检查文件是否存在。

此外，`unlink` 的所有失败都抛 `FileNotClosedError` 也不准确。只有打开表检查命中时才使用这个异常；文件不存在应是 `FileNotFoundError`，其他系统错误应保留系统错误信息或转换为内部错误。
