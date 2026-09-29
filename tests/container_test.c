// 容器单元测试（TODO.txt / ROADMAP 4.3）
//   只链接 HQC_Container.c —— 零 SDL 依赖，任何机器/CI 上都能跑。
//   跑法：cmake --build build && ctest --test-dir build  （或直接 ./build/bin/zuma_tests）
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/global/HQC/HQC_Container.h"

static int failures = 0;

#define CHECK(cond, msg)                                                    \
    do {                                                                    \
        if (cond) { printf("  ✓ %s\n", msg); }                              \
        else      { printf("  ✗ %s  (%s:%d)\n", msg, __FILE__, __LINE__);   \
                    failures++; }                                           \
    } while (0)


// ── Vector：镜像 HQC_Vector.c 的 struct Vector 字段顺序，手工造一个测试用 vector ──
typedef struct TestVector {
    size_t capacity;
    size_t elementSize;
    void*  elements;
    size_t elementsCount;
} TestVector;

static TestVector* _TestVectorNew(int* values, size_t n) {
    TestVector* v = malloc(sizeof(*v));

    v->elementSize   = sizeof(int);
    v->elementsCount = n;
    v->capacity      = n ? n : 1;
    v->elements      = malloc(v->capacity * v->elementSize);

    for (size_t i = 0; i < n; i++)
        ((int*)v->elements)[i] = values[i];

    return v;
}

static void _TestVectorFree(TestVector* v) {
    free(v->elements);
    free(v);
}


static void test_vector(void) {
    printf("Vector 扩展：\n");

    int vals[] = { 10, 20, 30, 40 };
    TestVector* v = _TestVectorNew(vals, 4);

    HQC_Container_VectorRemove((HQC_VectorContainer)v, 1);          // 删中间的 20
    CHECK(v->elementsCount == 3, "删除后元素数 = 3");
    CHECK(((int*)v->elements)[0] == 10 && ((int*)v->elements)[1] == 30
          && ((int*)v->elements)[2] == 40, "删中间元素后顺序保持（10,30,40）");

    int* popped = (int*)HQC_Container_VectorPop((HQC_VectorContainer)v);
    CHECK(popped && *popped == 40 && v->elementsCount == 2, "Pop 返回末元素 40 且元素数 = 2");

    int needle = 30;
    CHECK(HQC_Container_VectorIndexOf((HQC_VectorContainer)v, &needle, NULL) == 1,
          "IndexOf 找到 30 在下标 1");
    needle = 999;
    CHECK(HQC_Container_VectorIndexOf((HQC_VectorContainer)v, &needle, NULL) == -1,
          "IndexOf 未命中返回 -1");

    HQC_Container_VectorRemove((HQC_VectorContainer)v, 99);         // 越界：必须安全
    CHECK(v->elementsCount == 2, "越界删除不改变元素数（不崩）");

    _TestVectorFree(v);
}


static void test_list(void) {
    printf("单向链表：\n");

    HQC_ListContainer list = HQC_Container_CreateList(sizeof(int));
    CHECK(list != NULL && HQC_Container_ListCount(list) == 0, "新建链表为空");

    for (int i = 1; i <= 3; i++) HQC_Container_ListPushBack(list, &i);   // 尾插 1,2,3
    int zero = 0;
    HQC_Container_ListPushFront(list, &zero);                            // 头插 0

    CHECK(HQC_Container_ListCount(list) == 4, "插入 4 个后元素数 = 4");
    CHECK(*(int*)HQC_Container_ListGet(list, 0) == 0, "头插的元素在下标 0");
    CHECK(*(int*)HQC_Container_ListGet(list, 1) == 1, "尾插顺序正确（下标 1 = 1）");
    CHECK(*(int*)HQC_Container_ListGet(list, 3) == 3, "尾插顺序正确（下标 3 = 3）");
    CHECK(HQC_Container_ListGet(list, 4) == NULL, "越界取元素返回 NULL");

    HQC_Container_ListRemove(list, 0);
    CHECK(HQC_Container_ListCount(list) == 3 && *(int*)HQC_Container_ListGet(list, 0) == 1,
          "删头节点后首元素变 1");

    HQC_Container_ListRemove(list, 2);                                   // 删尾节点
    CHECK(HQC_Container_ListCount(list) == 2, "删尾节点后元素数 = 2");

    // 删尾之后再尾插，验证 tail 指针没被删坏
    int nine = 9;
    HQC_Container_ListPushBack(list, &nine);
    CHECK(HQC_Container_ListCount(list) == 3 && *(int*)HQC_Container_ListGet(list, 2) == 9,
          "删尾后尾插仍正确（tail 指针维护正确）");

    HQC_Container_FreeList(list);
}


static void test_dict(void) {
    printf("字符串键字典：\n");

    HQC_Dictionary d = HQC_Container_CreateDict(sizeof(int));
    CHECK(d != NULL && HQC_Container_DictCount(d) == 0, "新建字典为空");

    int v = 42;
    HQC_Container_DictSet(d, "score", &v);
    CHECK(HQC_Container_DictCount(d) == 1, "插入 1 个键后 count = 1");
    CHECK(*(int*)HQC_Container_DictGet(d, "score") == 42, "取值 = 42");
    CHECK(HQC_Container_DictHas(d, "score") == 1, "Has(\"score\") = 1");
    CHECK(HQC_Container_DictGet(d, "nope") == NULL, "取不存在的键返回 NULL");
    CHECK(HQC_Container_DictHas(d, "nope") == 0, "Has(不存在的键) = 0");

    int v2 = 7;
    HQC_Container_DictSet(d, "score", &v2);                              // 覆盖
    CHECK(HQC_Container_DictCount(d) == 1 && *(int*)HQC_Container_DictGet(d, "score") == 7,
          "同键覆盖不增加 count");

    // 触发扩容/哈希冲突：塞 200 个键
    for (int i = 0; i < 200; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key_%d", i);
        HQC_Container_DictSet(d, key, &i);
    }

    CHECK(HQC_Container_DictCount(d) == 201, "扩容后 201 个键全部保留");
    CHECK(*(int*)HQC_Container_DictGet(d, "key_199") == 199, "扩容后旧键仍可取（key_199）");
    CHECK(*(int*)HQC_Container_DictGet(d, "score") == 7, "扩容后最早插入的键仍在");

    HQC_Container_DictRemove(d, "key_100");
    CHECK(HQC_Container_DictHas(d, "key_100") == 0, "删除后 Has = 0");
    CHECK(HQC_Container_DictCount(d) == 200, "删除后 count = 200");
    CHECK(*(int*)HQC_Container_DictGet(d, "key_101") == 101,
          "删除后探测链补回（key_101 仍可查）");

    HQC_Container_DictSet(d, "key_100", &v);                             // 删了还能再插
    CHECK(HQC_Container_DictHas(d, "key_100") == 1 && HQC_Container_DictCount(d) == 201,
          "删除的键可以重新插入");

    HQC_Container_FreeDict(d);
}


int main(void) {
    printf("=== HQC 容器单元测试 ===\n");

    test_vector();
    test_list();
    test_dict();

    if (failures == 0) {
        printf("\n全部通过 ✅\n");
        return 0;
    }

    printf("\n%d 项失败 ❌\n", failures);
    return 1;
}