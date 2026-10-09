# DiskManager 测试诊断记录

## 现象

直接运行：

```bash
./build/debug/bin/lab1_disk_manager_test
```

曾出现大量失败，例如：

- `open_file` 没有抛出预期异常；
- `is_file` 返回 `false`；
- `create_file` 没有抛出预期异常；
- 页面操作中 `fd` 无效并触发断言。

## 原因

检查文件时间后发现，源码 `src/storage/disk_manager.cpp` 的更新时间晚于测试二进制 `build/debug/bin/lab1_disk_manager_test`。也就是说，直接运行的是旧的可执行文件，没有包含当前源码的最新改动。

## 正确操作

修改源码后，先重新编译对应测试目标：

```bash
cmake --build --preset debug --target lab1_disk_manager_test -j 4
```

再运行：

```bash
./build/debug/bin/lab1_disk_manager_test
```

## 最新结果

重新编译后：

```text
DiskManagerTest.FileOperation 通过
DiskManagerTest.PageOperation 通过
2 个测试全部通过
退出状态：0
```

## 经验

测试失败时先确认：

1. 源码是否已保存；
2. 测试目标是否重新编译；
3. 可执行文件的时间是否晚于源码；
4. 再根据最新输出定位逻辑问题。
