# `fd2path_` 在哪里、表示什么

## 定义位置

文件：

```text
src/storage/disk_manager.h
```

大约第 90 行：

```cpp
std::unordered_map<int, std::string> fd2path_;
```

它位于 `DiskManager` 的 `private:` 区域，因此只能由 `DiskManager` 自己的成员函数直接访问。

## 类型拆解

```cpp
std::unordered_map<int, std::string>
```

可以理解为一个哈希表：

```text
键 key：int              文件描述符 fd
值 value：std::string    文件路径 path
```

名称 `fd2path_` 就是：

```text
fd → path
```

下划线通常表示这是类的成员变量。

## 为什么需要它

`open()` 返回的是整数文件描述符，例如：

```text
fd = 3
path = "table.tbl"
```

关闭文件时调用者只传入 `fd`：

```cpp
close_file(3);
```

`DiskManager` 需要通过 `fd2path_` 找回对应路径，随后同步更新反向映射 `path2fd_`。

两张表的关系是：

```text
path2fd_：path → fd
fd2path_：fd   → path
```

## 当前代码中如何使用

### 打开成功后登记

```cpp
path2fd_[path] = fd;
fd2path_[fd] = path;
```

### 关闭前检查并取路径

```cpp
if (!fd2path_.count(fd)) {
    throw FileNotOpenError(fd);
}
std::string path = fd2path_.at(fd);
```

### 关闭成功后删除记录

```cpp
fd2path_.erase(fd);
path2fd_.erase(path);
```

## 三个常用操作

```cpp
fd2path_.count(fd)
```

返回键是否存在的数量：通常是 `0` 或 `1`。

```cpp
fd2path_.at(fd)
```

读取已有键对应的路径；键不存在时会抛出异常。

```cpp
fd2path_.erase(fd)
```

删除该键及其映射。

## 如何自己定位

在仓库根目录执行：

```bash
grep -RIn 'fd2path_' src
```

或者直接跳到声明：

```bash
nl -ba src/storage/disk_manager.h | sed -n '87,94p'
```
