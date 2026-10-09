# `Condition` 与 `CompOp`

代码位于 `src/common/common.h`：

```cpp
struct Condition {
    TabCol lhs_col;
    CompOp op;
    bool is_rhs_val;
    TabCol rhs_col;
    Value rhs_val;
};

enum CompOp { OP_EQ, OP_NE, OP_LT, OP_GT, OP_LE, OP_GE };
```

## `Condition` 表示什么

`Condition` 表示查询中的一个比较条件，例如：

```sql
WHERE student.age >= 18
```

可以拆成：

```text
左侧操作数：student.age
比较操作：>=
右侧操作数：整数 18
```

## 每个成员的含义

### `lhs_col`

`lhs` 是 *left-hand side* 的缩写，表示左侧操作数是一个列。

例如：

```sql
student.age >= 18
```

这里 `lhs_col` 表示 `student.age`。

### `op`

保存比较运算符，类型是 `CompOp`。它表示要进行相等、不等或大小比较。

### `is_rhs_val`

`rhs` 是 *right-hand side* 的缩写。这个布尔值决定右侧操作数是哪一种：

- `true`：右侧是一个具体值，使用 `rhs_val`；
- `false`：右侧是另一个列，使用 `rhs_col`。

例如：

```sql
WHERE student.age >= 18
```

右侧是常量 `18`，所以 `is_rhs_val == true`，使用 `rhs_val`。

```sql
WHERE student.id = score.student_id
```

右侧是列 `score.student_id`，所以 `is_rhs_val == false`，使用 `rhs_col`。

### `rhs_col`

右侧操作数是列时，保存该列的表名和列名。

注释中的“right-hand side column”是准确的，但它只有在 `is_rhs_val == false` 时才有效。

### `rhs_val`

右侧操作数是常量时，保存这个值。它不一定是“右侧列的值”，更准确的说法是：

> 右侧常量值，或右侧值操作数。

例如可以保存整数、浮点数或字符串：

```sql
WHERE age >= 18
WHERE score < 60.0
WHERE name = 'Alice'
```

## `CompOp` 枚举

`enum` 是枚举类型，用一组有名字的常量表示有限选项。这里的每个枚举项对应一种比较操作：

| 枚举项 | SQL 运算符 | 含义 |
| --- | --- | --- |
| `OP_EQ` | `=` | 等于 |
| `OP_NE` | `<>` 或 `!=` | 不等于 |
| `OP_LT` | `<` | 小于 |
| `OP_GT` | `>` | 大于 |
| `OP_LE` | `<=` | 小于或等于 |
| `OP_GE` | `>=` | 大于或等于 |

例如：

```cpp
Condition condition;
condition.lhs_col = {"student", "age"};
condition.op = OP_GE;
condition.is_rhs_val = true;
condition.rhs_val.set_int(18);
```

它表达的逻辑就是：

```sql
student.age >= 18
```

## 这个结构不是在执行比较

`Condition` 只是保存一条条件的结构化表示，类似一张“比较说明”。真正执行比较的代码会读取 `lhs_col`、`op` 以及 `rhs_val` 或 `rhs_col`，取出实际数据后再判断条件是否成立。
