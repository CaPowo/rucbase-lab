// Copyright (c) 2023-2027 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#pragma once

#include <cstddef>
#include <list>
#include <mutex>
#include <unordered_map>

#include "common/config.h"
#include "replacer/replacer.h"

/*
LRUReplacer实现了LRU替换策略
它在缓存满时淘汰“最久未被访问”的数据块，常用于 Cache、内存页管理和应用级缓存系统。 它基于时间局部性原理：最近访问过的数据更可能再次被访问，因此优先保留最近使用的数据。
frame应该表现为 内存中可以被存储的页面插槽，说白了就是页面在内存中的表现形式
*/
class LRUReplacer : public Replacer {   //父类子类的关系
public:
    /**
     * @description: 创建一个新的LRUReplacer
     * @param {size_t} num_pages LRUReplacer最多需要存储的page数量
     */
    explicit LRUReplacer(size_t num_pages);

    ~LRUReplacer() override;

    bool victim(frame_id_t* frame_id) override;

    void pin(frame_id_t frame_id) override;

    void unpin(frame_id_t frame_id) override;

    size_t Size() override;

private:
    std::mutex latch_;               // 互斥锁
    std::list<frame_id_t> LRUlist_;  // 按加入的时间顺序存放unpinned pages的frame id，首部表示最近被访问
    //哈希表的存在是为了降低时间复杂度，将某些操作从O(n)降低到O(1)，比如按照节点的值进行查找
    std::unordered_map<frame_id_t, std::list<frame_id_t>::iterator> LRUhash_;  // frame_id_t -> unpinned pages的frame id
    // 最大容量（与缓冲池的容量相同）
    size_t max_size_;
};
