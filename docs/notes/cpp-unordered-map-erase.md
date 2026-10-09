# `unordered_map::erase`

## 它在哪里定义

`fd2path_` 的类型是：

```cpp
std::unordered_map<int, std::string>
```

`erase` 是 C++ 标准库 `std::unordered_map` 自带的成员函数，声明来自：

```cpp
#include <unordered_map>
```

不是项目自己定义的函数。

## 当前代码中的用法

```cpp
fd2path_.erase(fd);
path2fd_.erase(path);
```

它们分别删除：

```text
fd2path_ 中键为 fd 的键值对
path2fd_ 中键为 path 的键值对
```

例如：

```cpp
std::unordered_map<int, std::string> files;
files[3] = "table.tbl";
files.erase(3);
```

调用后，键 `3` 和它对应的值都会从哈希表中移除。

## 常见重载

### 按键删除

```cpp
container.erase(key);
```

返回实际删除的元素数量。`unordered_map` 的键唯一，所以通常是 `0` 或 `1`。

### 按迭代器删除

```cpp
auto it = container.find(key);
if (it != container.end()) {
    container.erase(it);
}
```

这种写法适合已经找到元素的情况。

### 删除一个范围

```cpp
container.erase(first, last);
```

删除迭代器区间 `[first, last)` 中的元素。

## 与 `clear()` 的区别

```cpp
container.erase(key);  // 删除一个指定键
container.clear();     // 删除容器中的全部元素
```

## 与 `operator[]` 的区别

读取已有键时：

```cpp
container.at(key);     // 不存在会抛异常
```

而：

```cpp
container[key];        // 不存在时会自动插入默认值
```

因此删除映射时使用 `erase`，不要用赋空值来代替。

## 如何查看定义

源代码中可以先看包含关系：

```text
src/storage/disk_manager.h
└── #include <unordered_map>
```

标准库实现通常位于编译器的头文件目录中，例如 GCC 的 `bits/hashtable.h` 相关文件；实际路径因编译器版本而异。学习时只需记住它属于 `std::unordered_map` 的公开接口即可。
