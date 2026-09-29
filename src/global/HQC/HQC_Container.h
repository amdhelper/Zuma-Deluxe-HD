#pragma once

// HQC 容器补齐（TODO.txt / ROADMAP 4.3）：Vector 删元素 + 单向链表 + 字符串键字典
// 独立实现（只用 malloc/free），便于 tests/container_test.c 单独链接做单元测试。

#include <stddef.h>

// ── Vector 扩展 ────────────────────────────────────────────────────────────
// ⚠️ Vector 的结构定义在 HQC_Vector.c 里（不透明）：这里是 void* 别名，
//    HQC_Container.c 内部有一份**字段顺序完全一致**的镜像结构，改动必须两边同步。
typedef void* HQC_VectorContainer;

void  HQC_Container_VectorRemove(HQC_VectorContainer vector, int index);      // 保序删除
void* HQC_Container_VectorPop(HQC_VectorContainer vector);                    // 删末尾，返回其指针
int   HQC_Container_VectorIndexOf(HQC_VectorContainer vector, const void* element,
                                  int (*equals)(const void* a, const void* b));

// ── 单向链表 ──────────────────────────────────────────────────────────────
typedef void* HQC_ListContainer;

HQC_ListContainer HQC_Container_CreateList(size_t elementSize);
void   HQC_Container_ListPushFront(HQC_ListContainer list, const void* element);
void   HQC_Container_ListPushBack(HQC_ListContainer list, const void* element);
void*  HQC_Container_ListGet(HQC_ListContainer list, int index);
size_t HQC_Container_ListCount(HQC_ListContainer list);
void   HQC_Container_ListRemove(HQC_ListContainer list, int index);
void   HQC_Container_FreeList(HQC_ListContainer list);

// ── 字符串键字典 ──────────────────────────────────────────────────────────
typedef void* HQC_Dictionary;

HQC_Dictionary HQC_Container_CreateDict(size_t valueSize);
void   HQC_Container_DictSet(HQC_Dictionary dict, const char* key, const void* value);
void*  HQC_Container_DictGet(HQC_Dictionary dict, const char* key);   // 未命中 = NULL
int    HQC_Container_DictHas(HQC_Dictionary dict, const char* key);
size_t HQC_Container_DictCount(HQC_Dictionary dict);
void   HQC_Container_DictRemove(HQC_Dictionary dict, const char* key);
void   HQC_Container_FreeDict(HQC_Dictionary dict);