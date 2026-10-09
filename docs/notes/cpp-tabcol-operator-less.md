# `TabCol` 的 `operator<`

代码位置：`src/common/common.h`

```cpp
friend bool operator<(const TabCol& x, const TabCol& y) {
    return std::make_pair(x.tab_name, x.col_name) < std::make_pair(y.tab_name, y.col_name);
}
```

## `TabCol` 表示什么

```cpp
struct TabCol {
    std::string tab_name;
    std::string col_name;
};
```

它表示一个“表名 + 列名”，例如：

```text
student.name
```

## 这段代码做什么

它定义了两个 `TabCol` 对象之间的“小于”关系，因此可以写：

```cpp
TabCol a{"student", "id"};
TabCol b{"student", "name"};

bool result = a < b;
```

比较规则是字典序：

1. 先比较 `tab_name`；
2. 如果表名相同，再比较 `col_name`。

因此大致顺序是：

```text
course.id
student.id
student.name
```

因为 `course` 在 `student` 前面；同一张表内，`id` 在 `name` 前面。

## `std::make_pair` 的作用

```cpp
std::make_pair(x.tab_name, x.col_name)
```

把两个字符串组合成：

```cpp
std::pair<std::string, std::string>
```

标准库已经为 `std::pair` 定义了字典序比较，所以这里借用 pair 的比较规则，不需要手写两层 `if`。

逻辑上等价于：

```cpp
if (x.tab_name != y.tab_name) {
    return x.tab_name < y.tab_name;
}
return x.col_name < y.col_name;
```

## `friend` 的作用

`operator<` 被定义成非成员函数，但声明为 `friend`，表示它可以访问 `TabCol` 的私有成员。当前 `TabCol` 是 `struct`，成员默认公开，所以这里的 `friend` 实际上不是访问权限所必需的；它也常用于表达“这是这个类型配套的非成员运算符”。

## 为什么需要它

定义后，`TabCol` 可以用于需要排序或有序键的容器，例如：

```cpp
std::set<TabCol> columns;
std::map<TabCol, int> positions;
```

这些容器需要知道两个键哪个排在前面。

## 与 SQL 的关系

在 RUCBase 中，`TabCol` 用来标识查询中的列，例如投影列、排序列和连接列。这里的 `<` 只是 C++ 容器的排序规则，不是 SQL 中的 `WHERE a < b` 条件。
