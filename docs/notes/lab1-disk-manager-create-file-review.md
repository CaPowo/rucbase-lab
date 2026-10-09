# `DiskManager::create_file` 草稿检查

## 目标行为

`create_file(path)` 的职责是：

```text
确认路径不存在 → 创建一个空文件 → 立即关闭创建时得到的 fd
```

之后由 `open_file(path)` 再打开文件并加入 `path2fd_` / `fd2path_` 映射。

## 草稿中的问题

### 1. 不要使用 `exit(1)`

项目的文件管理接口使用异常表示错误。创建失败或文件已存在时，应抛出项目定义的错误类型，而不是终止整个数据库进程。

### 2. `open` 需要明确访问模式

`O_CREAT` 只表示“必要时创建”，不表示读写访问方式。创建阶段通常需要一个写入访问模式，并配合权限参数。

### 3. 防止重复创建

`is_file(path)` 的预检查可以帮助给出清晰错误，但单独依赖“先检查、后创建”存在竞态：另一个线程或进程可能在两步之间创建同名文件。

更稳妥的思路是让 `open` 使用“排他创建”语义，让系统在目标已存在时直接失败；然后把系统错误转换成项目异常。具体标志组合应结合课程注释和本机 `open(2)` 语义确认。

### 4. 创建得到的 fd 要关闭

这里和 `write_page` 相反：`create_file` 自己调用 `open` 获得了临时 fd，因此创建成功后应关闭它。不要把这个 fd 放进 `path2fd_`，因为该映射表示通过 `open_file` 保持的已打开文件。

### 5. `perror` 不是主要错误接口

`perror` 可以用于临时调试，但测试检查的是项目异常类型，例如文件已存在时的 `FileExistsError`。因此应优先保证抛出的异常类型正确。

## 自查清单

1. 已存在的普通文件是否会报告 `FileExistsError`？
2. 创建失败是否转换为合适的项目异常？
3. 创建成功后临时 fd 是否关闭？
4. 是否没有修改 `path2fd_` / `fd2path_`？
5. 是否没有使用 `exit(1)`？
6. 是否考虑了 `O_EXCL` 解决检查与创建之间的竞态？

## 与 `open_file` 的区别

```text
create_file：创建空文件，关闭临时 fd，不加入打开文件表
open_file：打开已有文件，返回 fd，更新打开文件表
```

## 对当前草稿的逐行反馈

当前草稿：

```cpp
if (flag) {
    perror("file exists");
    throw FileExistsError(path);
}
int fd = open(path, O_CREAT | O_EXCL, 0600);
if (fd == -1) {
    perror("Failed to create file");
    close(fd);
    throw InternalError("Failed to create file");
    exit(1);
}
```

需要调整：

1. 成功路径最后还要关闭这个临时 `fd`，否则会泄漏文件描述符。
2. `fd == -1` 时没有有效描述符，不能调用 `close(fd)`。
3. `throw` 之后的 `exit(1)` 永远不会执行，应删除。
4. 预检查可以保留用于清晰报错，但 `O_EXCL` 仍应保留，用于处理检查和创建之间的竞态。
5. `open` 失败时应根据 `errno` 区分“文件已存在”和其他系统错误；其他错误可使用项目已有的 Unix 异常风格。
6. 成功后执行 `close(fd)`，不要把它加入 `path2fd_` 或 `fd2path_`。

可以用这个控制流自查，而不是把它当作可直接粘贴的实现：

```text
若路径已存在 → FileExistsError
尝试排他创建 → 得到临时 fd
若创建失败 → 根据 errno 报告错误
创建成功 → 关闭临时 fd
```
