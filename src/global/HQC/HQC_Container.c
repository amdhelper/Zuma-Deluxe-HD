// HQC 容器补齐（TODO.txt / ROADMAP 4.3）：Vector 删元素 + 单向链表 + 字符串键字典
//
// 为什么单独一个文件、而且只用 malloc/free：
//   HQC 的 Vector 在 HQC_Vector.c 里、错误处理会拉进 SDL；为了能**独立单元测试**
//   （tests/container_test.c 只链接这一个 .c），这里保持零依赖、自带错误打印。
//   游戏侧照旧通过 HQC.h 里的 HQC_Container_* 使用。
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "HQC_Container.h"

// Vector 的镜像结构：必须与 HQC_Vector.c 里的 struct Vector 字段顺序/类型**逐字一致**
//   HQC_Vector.c: typedef struct Vector { size_t capacity; size_t elementSize;
//                                         void* elements; size_t elementsCount; } Vector;
typedef struct HQC_VectorMirror {
    size_t capacity;
    size_t elementSize;
    void*  elements;
    size_t elementsCount;
} HQC_VectorMirror;

////////////////////////////////////////////////////////////////////////////////
// Vector 扩展：删元素（保序）/ 弹出末尾 / 查找
////////////////////////////////////////////////////////////////////////////////

void HQC_Container_VectorRemove(HQC_VectorContainer vector_, int index) {
    HQC_VectorMirror* vector = (HQC_VectorMirror*)vector_;

    if (!vector || index < 0 || (size_t)index >= vector->elementsCount) return;

    size_t tail = vector->elementsCount - (size_t)index - 1;

    if (tail > 0) {
        char* base = (char*)vector->elements;
        memmove(base + (size_t)index * vector->elementSize,
                base + (size_t)(index + 1) * vector->elementSize,
                tail * vector->elementSize);
    }

    vector->elementsCount--;
}


void* HQC_Container_VectorPop(HQC_VectorContainer vector_) {
    HQC_VectorMirror* vector = (HQC_VectorMirror*)vector_;

    if (!vector || vector->elementsCount == 0) return NULL;

    vector->elementsCount--;

    return (char*)vector->elements + vector->elementsCount * vector->elementSize;
}


int HQC_Container_VectorIndexOf(HQC_VectorContainer vector_, const void* element,
                                int (*equals)(const void* a, const void* b)) {
    HQC_VectorMirror* vector = (HQC_VectorMirror*)vector_;

    if (!vector || !element) return -1;

    char* base = (char*)vector->elements;

    for (size_t i = 0; i < vector->elementsCount; i++) {
        const void* cur = base + i * vector->elementSize;

        if (equals) {
            if (equals(cur, element)) return (int)i;
        } else if (memcmp(cur, element, vector->elementSize) == 0) {
            return (int)i;
        }
    }

    return -1;
}


////////////////////////////////////////////////////////////////////////////////
// 单向链表（头插/尾插 O(1)）
////////////////////////////////////////////////////////////////////////////////

typedef struct List {
    size_t elementSize;
    size_t count;

    struct ListNode* head;
    struct ListNode* tail;
} List;

typedef struct ListNode {
    struct ListNode* next;
    char             data[1];      // 变长：elementSize 字节
} ListNode;


HQC_ListContainer HQC_Container_CreateList(size_t elementSize) {
    if (elementSize == 0) elementSize = 1;

    List* list = malloc(sizeof(*list));
    if (!list) { fprintf(stderr, "[HQC List] out of memory\n"); return NULL; }

    list->elementSize = elementSize;
    list->count       = 0;
    list->head        = NULL;
    list->tail        = NULL;

    return list;
}


static ListNode* _ListNodeMake(List* list, const void* element) {
    ListNode* node = malloc(sizeof(ListNode) + list->elementSize);

    if (!node) { fprintf(stderr, "[HQC List] out of memory\n"); return NULL; }

    node->next = NULL;
    memcpy(node->data, element, list->elementSize);

    return node;
}


void HQC_Container_ListPushFront(HQC_ListContainer list_, const void* element) {
    List* list = (List*)list_;
    if (!list) return;

    ListNode* node = _ListNodeMake(list, element);
    if (!node) return;

    node->next = list->head;
    list->head = node;

    if (!list->tail) list->tail = node;

    list->count++;
}


void HQC_Container_ListPushBack(HQC_ListContainer list_, const void* element) {
    List* list = (List*)list_;
    if (!list) return;

    ListNode* node = _ListNodeMake(list, element);
    if (!node) return;

    if (list->tail) list->tail->next = node;
    else            list->head        = node;

    list->tail = node;

    list->count++;
}


void* HQC_Container_ListGet(HQC_ListContainer list_, int index) {
    List* list = (List*)list_;
    if (!list || index < 0 || (size_t)index >= list->count) return NULL;

    ListNode* node = list->head;

    for (int i = 0; i < index && node; i++)
        node = node->next;

    return node ? (void*)node->data : NULL;
}


size_t HQC_Container_ListCount(HQC_ListContainer list_) {
    List* list = (List*)list_;
    return list ? list->count : 0;
}


void HQC_Container_ListRemove(HQC_ListContainer list_, int index) {
    List* list = (List*)list_;
    if (!list || index < 0 || (size_t)index >= list->count) return;

    ListNode* prev = NULL;
    ListNode* node = list->head;

    for (int i = 0; i < index && node; i++) {
        prev = node;
        node = node->next;
    }

    if (!node) return;

    if (prev) prev->next = node->next;
    else      list->head  = node->next;

    if (list->tail == node) list->tail = prev;

    free(node);
    list->count--;
}


void HQC_Container_FreeList(HQC_ListContainer list_) {
    List* list = (List*)list_;
    if (!list) return;

    ListNode* node = list->head;

    while (node) {
        ListNode* next = node->next;
        free(node);
        node = next;
    }

    free(list);
}


////////////////////////////////////////////////////////////////////////////////
// 字典（字符串键 → 定长值）：开放寻址 + 线性探测，负载 > 0.7 扩容
////////////////////////////////////////////////////////////////////////////////

#define DICT_INIT_BUCKETS 16

typedef struct DictEntry {
    char*  key;        // NULL = 空槽
    char*  value;
    size_t hash;
} DictEntry;

typedef struct Dict {
    size_t     valueSize;
    size_t     bucketCount;
    size_t     count;
    DictEntry* buckets;
} Dict;


static char* _StrDup(const char* s) {
    // 不依赖 strdup（POSIX 扩展，-std=c99 严格模式下可能没有声明）
    size_t len = strlen(s) + 1;
    char*  copy = malloc(len);

    if (!copy) return NULL;

    memcpy(copy, s, len);
    return copy;
}


static size_t _DictHash(const char* s) {
    // FNV-1a
    size_t h = 1469598103934665603ULL;

    for (const unsigned char* p = (const unsigned char*)s; *p; p++) {
        h ^= (size_t)*p;
        h *= 1099511628211ULL;
    }

    return h;
}


HQC_Dictionary HQC_Container_CreateDict(size_t valueSize) {
    if (valueSize == 0) valueSize = 1;

    Dict* dict = malloc(sizeof(*dict));
    if (!dict) { fprintf(stderr, "[HQC Dict] out of memory\n"); return NULL; }

    dict->valueSize   = valueSize;
    dict->bucketCount = DICT_INIT_BUCKETS;
    dict->count       = 0;
    dict->buckets     = calloc(dict->bucketCount, sizeof(DictEntry));

    if (!dict->buckets) { free(dict); return NULL; }

    return dict;
}


// 在 dict 里找 key 的槽（找到返回下标；没找到返回第一个空槽下标）
static size_t _DictFindSlot(Dict* dict, const char* key, size_t hash, int* outFound) {
    size_t mask  = dict->bucketCount - 1;
    size_t index = hash & mask;

    for (size_t probe = 0; probe < dict->bucketCount; probe++) {
        DictEntry* e = &dict->buckets[index];

        if (!e->key) { *outFound = 0; return index; }
        if (e->hash == hash && strcmp(e->key, key) == 0) { *outFound = 1; return index; }

        index = (index + 1) & mask;
    }

    *outFound = 0;
    return (size_t)-1;
}


static void _DictGrow(Dict* dict) {
    size_t     oldCount   = dict->bucketCount;
    DictEntry* oldBuckets = dict->buckets;

    dict->bucketCount *= 2;
    dict->buckets      = calloc(dict->bucketCount, sizeof(DictEntry));

    if (!dict->buckets) {                 // 扩容失败：保留旧表继续用
        dict->bucketCount = oldCount;
        dict->buckets     = oldBuckets;
        return;
    }

    for (size_t i = 0; i < oldCount; i++) {
        if (!oldBuckets[i].key) continue;

        int found = 0;
        size_t slot = _DictFindSlot(dict, oldBuckets[i].key, oldBuckets[i].hash, &found);

        if (slot == (size_t)-1) continue;

        dict->buckets[slot] = oldBuckets[i];
    }

    free(oldBuckets);
}


void HQC_Container_DictSet(HQC_Dictionary dict_, const char* key, const void* value) {
    Dict* dict = (Dict*)dict_;
    if (!dict || !key || !value) return;

    if ((dict->count + 1) * 10 > dict->bucketCount * 7)
        _DictGrow(dict);

    size_t hash = _DictHash(key);
    int    found = 0;
    size_t slot = _DictFindSlot(dict, key, hash, &found);

    if (slot == (size_t)-1) return;

    DictEntry* e = &dict->buckets[slot];

    if (!found) {
        e->key   = _StrDup(key);
        e->value = malloc(dict->valueSize);
        e->hash  = hash;

        if (!e->key || !e->value) { free(e->key); free(e->value); e->key = NULL; e->value = NULL; return; }

        dict->count++;
    }

    memcpy(e->value, value, dict->valueSize);
}


void* HQC_Container_DictGet(HQC_Dictionary dict_, const char* key) {
    Dict* dict = (Dict*)dict_;
    if (!dict || !key) return NULL;

    int    found = 0;
    size_t slot = _DictFindSlot(dict, key, _DictHash(key), &found);

    if (!found || slot == (size_t)-1) return NULL;

    return dict->buckets[slot].value;
}


int HQC_Container_DictHas(HQC_Dictionary dict, const char* key) {
    return HQC_Container_DictGet(dict, key) != NULL;
}


size_t HQC_Container_DictCount(HQC_Dictionary dict_) {
    Dict* dict = (Dict*)dict_;
    return dict ? dict->count : 0;
}


void HQC_Container_DictRemove(HQC_Dictionary dict_, const char* key) {
    Dict* dict = (Dict*)dict_;
    if (!dict || !key) return;

    size_t hash = _DictHash(key);
    int    found = 0;
    size_t slot = _DictFindSlot(dict, key, hash, &found);

    if (!found || slot == (size_t)-1) return;

    free(dict->buckets[slot].key);
    free(dict->buckets[slot].value);

    dict->buckets[slot].key   = NULL;
    dict->buckets[slot].value = NULL;
    dict->count--;

    // 把后面的探测链补回来（线性探测删除的标准做法）
    size_t mask  = dict->bucketCount - 1;
    size_t index = (slot + 1) & mask;

    while (dict->buckets[index].key) {
        DictEntry moved = dict->buckets[index];

        dict->buckets[index].key   = NULL;
        dict->buckets[index].value = NULL;
        dict->count--;

        HQC_Container_DictSet((HQC_Dictionary)dict, moved.key, moved.value);

        free(moved.key);
        free(moved.value);

        index = (index + 1) & mask;
    }
}


void HQC_Container_FreeDict(HQC_Dictionary dict_) {
    Dict* dict = (Dict*)dict_;
    if (!dict) return;

    for (size_t i = 0; i < dict->bucketCount; i++) {
        free(dict->buckets[i].key);
        free(dict->buckets[i].value);
    }

    free(dict->buckets);
    free(dict);
}