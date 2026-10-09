// Copyright (c) 2023-2027 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#pragma once

#include <cassert>
#include <cstring>
#include <memory>
#include <string>
#include <utility>

#include "common/defs.h"
#include "common/errors.h"
#include "record/rm_defs.h"

// 表定义结构体，表名和字段名
struct TabCol {
    std::string tab_name;
    std::string col_name;

    // 同一张表内，按照字段名称进行比较
    friend bool operator<(const TabCol& x, const TabCol& y) {
        return std::make_pair(x.tab_name, x.col_name) < std::make_pair(y.tab_name, y.col_name);
    }
};

// 对表中值的定义
struct Value {
    ColType type;  // 值的类型
    union { // union 共享内存区域
        int int_val;      // int value 四字节大小
        float float_val;  // float value 四字节大小
    };
    std::string str_val;  // string value

    std::shared_ptr<RmRecord> raw;  // raw record buffer/ raw类型缓冲区

    void set_int(int int_val_) {    //将 Value 设置为整数类型，并保存 int 值。
        type = TYPE_INT;
        int_val = int_val_;
    }

    void set_float(float float_val_) {  //将 Value 设置为浮点类型，并保存 float 值。
        type = TYPE_FLOAT;
        float_val = float_val_;
    }

    void set_str(std::string str_val_) {  //将 Value 设置为字符串类型，并保存 string 值。
        type = TYPE_STRING;
        str_val = std::move(str_val_);
    }

    void init_raw(int len) {    // raw类型初始化 在这里是实际数据写入缓冲区
        assert(raw == nullptr);      // 初始状态应该是空指针，如果不为空指针会报错
        raw = std::make_shared<RmRecord>(len);
        if (type == TYPE_INT) {     //几种数据类型分别写入
            assert(len == sizeof(int));
            *(int*)(raw->data) = int_val;
        } else if (type == TYPE_FLOAT) {
            assert(len == sizeof(float));
            *(float*)(raw->data) = float_val;
        } else if (type == TYPE_STRING) {
            if (len < (int)str_val.size()) {
                throw StringOverflowError();    // 溢出报错，给的空间比需要的少
            }
            memset(raw->data, 0, len);  //在data中开辟内存空间，清空（用0占位）
            memcpy(raw->data, str_val.c_str(), str_val.size()); //将str_val 的数据拷贝到缓冲区
        }
    }
};

enum CompOp { OP_EQ, OP_NE, OP_LT, OP_GT, OP_LE, OP_GE };  //枚举变量 这些是比较操作
//分别是         =   !=或<>    <      >      <=     >=

struct Condition {  //条件结构体 适配 WHERE student.age >= 18 
    TabCol lhs_col;   // left-hand side column 左侧
    CompOp op;        // comparison operator  操作
    bool is_rhs_val;  // true if right-hand side is a value (not a column) 如果右边的是个值，不是个列
    TabCol rhs_col;   // right-hand side column 右侧列
    Value rhs_val;    // right-hand side value  右侧列的值
};

struct SetClause {  //
    TabCol lhs;     //左侧的列
    Value rhs;      //右边的值
};
