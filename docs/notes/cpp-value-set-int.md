# `Value::set_int` 的注释是否准确

代码：

```cpp
void set_int(int int_val_) {
    type = TYPE_INT;
    int_val = int_val_;
}
```

## 结论

“写入 int 类型数据”基本能让初学者理解，但不够精确。

更准确的描述是：

> 将当前 `Value` 标记为整数类型，并保存一个整数值。

或者写成代码注释：

```cpp
// 将 Value 设置为整数类型，并保存 int 值。
```

## 为什么“写入”不够精确

这个函数没有写磁盘，也没有写入数据库页面或 `raw` 缓冲区。它只修改内存中的 `Value` 对象：

1. `type = TYPE_INT`：设置类型标签；
2. `int_val = int_val_`：保存整数值。

真正把值编码到记录字节缓冲区的动作发生在之后的 `init_raw(int len)` 中：

```cpp
*(int*)(raw->data) = int_val;
```

所以可以区分：

```text
set_int     设置 Value 的逻辑类型和值
init_raw    把 Value 编码到原始字节缓冲区
```

## 参数名中的下划线

`int_val_` 是参数，`int_val` 是对象成员。下划线用于区分同名的参数和成员，避免写成含糊的赋值。
