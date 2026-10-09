# `DiskManager::close_file` 草稿检查

当前草稿：

```cpp
if (!fd2path_.count(fd)) {
    throw FileNotClosedError("文件未打开");
}

if (close(fd) == -1) {
    throw InternalError("Failed to close file: " + fd2path_.count(fd));
}

fd2path_[fd] = nullptr;
```

## 需要修正的地方

### 1. 未打开时使用 `FileNotOpenError`

`FileNotClosedError` 的语义是“文件仍然打开”，主要给 `destroy_file` 使用。关闭一个根本没有登记的 fd，应使用：

```cpp
FileNotOpenError(fd)
```

### 2. `count(fd)` 返回数量，不是路径

```cpp
fd2path_.count(fd)
```

返回 `0` 或 `1`，不能和字符串直接拼接。需要先从映射中取得真实路径；同时，关闭前保存路径，因为成功后要删除两张表中的记录。

### 3. 不能把 `std::string` 赋值为 `nullptr`

```cpp
fd2path_[fd] = nullptr;
```

`fd2path_` 的 value 类型是 `std::string`，而且关闭后不应留下一个空值记录。应删除映射项：

```text
fd2path_ 删除 fd
path2fd_ 删除对应 path
```

### 4. `close` 失败时不要更新映射

如果系统调用失败，文件可能仍处于打开状态。此时必须保留两个映射，方便上层重试或报告状态；只有 `close` 成功后才删除映射。

## 状态转换

```text
fd 不在 fd2path_ → FileNotOpenError
fd 在 fd2path_ → 保存 path
调用 close(fd)
失败 → 保留映射并抛错
成功 → 删除 fd2path_[fd] 和 path2fd_[path]
```

## 自查问题

1. 关闭不存在的 fd 时，异常是否携带 fd？
2. 是否保存了 `fd` 对应的路径？
3. `close` 失败时是否保留打开表？
4. `close` 成功后两张映射是否都删除？
5. 是否没有使用 `operator[]` 给不存在的 key 插入新项？
