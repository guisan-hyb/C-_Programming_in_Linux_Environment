#include "../include/hash_map.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// 这里定义负载因子阈值，比如 0.75
static const double LOAD_FACTOR_THRESHOLD = 0.75;

// djb2 hash function（不变）
static uint64_t hash_djb2(const char *str)
{
    uint64_t hash = 5381;
    int c;
    while ((c = *str++))
    {
        hash = ((hash << 5) + hash) + (uint8_t)c;
    }
    return hash;
}

// Robert Jenkins’ 32 bit integer mix（不变）
static uint64_t hash_int(int key)
{
    uint32_t x = (uint32_t)key;
    x = (x + 0x7ed55d16) + (x << 12);
    x = (x ^ 0xc761c23c) ^ (x >> 19);
    x = (x + 0x165667b1) + (x << 5);
    x = (x + 0xd3a2646c) ^ (x << 9);
    x = (x + 0xfd7046c5) + (x << 3);
    x = (x ^ 0xb55a4f09) ^ (x >> 16);
    return (uint64_t)x;
}

// 创建哈希表时同时初始化 size = 0
hashmap_t* hashmap_create(size_t bucket_count){
    hashmap_t *map = malloc(sizeof(hashmap_t));
    if(!map)
        return NULL;

    map->size = 0;// 新表，元素个数为0
    map->bucket_count = bucket_count;
    map->buckets = calloc(bucket_count, sizeof(hashmap_entry_t *));
    if(!map->buckets){
        free(map);
        return NULL;
    }

    return map;
}

// 内部函数：给定一个新的桶数量，重新分配并 Rehash 所有旧的条目
int hashmap_resize(hashmap_t *map){
    if(!map) return -1;

    size_t new_bucket_count = map->bucket_count * 2;
    // 分配新的桶数组
    hashmap_entry_t **new_buckets = calloc(new_bucket_count, sizeof(hashmap_entry_t*));
    if(!new_buckets) return -1;

    // 对旧桶中的每个 entry 重新哈希到 new_buckets
    for (size_t i = 0; i < map->bucket_count;i++){
        hashmap_entry_t *entry = map->buckets[i];
        while(entry){
            hashmap_entry_t *next = entry->next;

            // 计算新索引
            size_t new_idx;
            if(entry->key){
                uint64_t h = hash_djb2(entry->key);
                new_idx = h % new_bucket_count;
            }else{
                uint64_t h = hash_int(entry->int_key);
                new_idx = h % new_bucket_count;
            }

            // 插入到 new_buckets[new_idx] 链表的头部
            entry->next = new_buckets[new_idx];
            new_buckets[new_idx] = entry;

            entry = next;
        }
    }

    // 释放旧的桶数组（条目本身已经移到新数组里）
    free(map->buckets);

    // 更新 map 的桶数组指针和桶数量
    map->buckets = new_buckets;
    map->bucket_count = new_bucket_count;
    return 0;
}

// 帮助函数：检查是否需要扩容（仅在“新插入”时调用）
// 如果 load_factor 超过阈值，就进行 resize
static int hashmap_maybe_resize(hashmap_t *map){
    if(!map) return -1;

    double load_factor = (double)(map->size + 1) / (double)(map->bucket_count);
    if(load_factor > LOAD_FACTOR_THRESHOLD){
        return hashmap_resize(map);
    }

    return 0;
}

// 插入或更新字符串 key
int hashmap_put(hashmap_t *map, const char *key, void *value){
    if(!map || !key) return -1;

    // 先检查是否是「新插入」：如果 load_factor 会超标则先扩容
    // 注意：如果 key 已存在，我们后面会覆盖，因此无需扩容
    uint64_t hash = hash_djb2(key);
    size_t idx = hash % map->bucket_count;
    hashmap_entry_t *entry = map->buckets[idx];
    while(entry){
        if(strcmp(entry->key,key) == 0){
            // key 已存在，直接覆盖，不改变 map->size
            entry->value = value;
            return 0;
        }
        entry = entry->next;
    }

    // key 不存在，可能需要扩容
    if(hashmap_resize(map) != 0){
        return -1;// 扩容失败
    }

    // 重新计算 idx（因为扩容后 bucket_count 改变）
    hash = hash_djb2(key);
    idx = hash % map->bucket_count;

    // 创建新 entry 并插入
    entry = malloc(sizeof(hashmap_entry_t));
    if(!entry) return -1;
    entry->key = strdup(key);
    if(!entry->key){
        free(entry);
        return -1;
    }

    entry->value = value;
    entry->int_key = 0;// 无意义
    entry->next = map->buckets[idx];
    map->buckets[idx] = entry;

    // 更新元素个数
    map->size += 1;
    return 0;
}

// 查询字符串 key（不变）
void *hashmap_get(hashmap_t *map, const char *key){
    if(!map || !key) return NULL;

    uint64_t hash = hash_djb2(key);
    size_t idx = hash % map->bucket_count;
    hashmap_entry_t *entry = map->buckets[idx];
    while(entry){
        if(entry->key && strcmp(key,entry->key) == 0){
            return entry->value;
        }
        entry = entry->next;
    }
    return NULL;
}

// 移除字符串 key
int hashmap_remove(hashmap_t *map, const char *key){
    if(!map || !key) return -1;

    uint64_t hash = hash_djb2(key);
    size_t idx = hash % map->bucket_count;
    hashmap_entry_t *entry = map->buckets[idx];
    hashmap_entry_t *prev = NULL;
    while (entry)
    {
        if(entry->key && strcmp(key,entry->key) == 0){
            if(prev){
                prev->next = entry->next;
            }else{
                map->buckets[idx] = entry->next;
            }

            free(entry->key);
            free(entry);
            map->size -= 1;
            return 0;
        }
        prev = entry;
        entry = entry->next;
    }

    return -1;
}

// 销毁整个哈希表（遍历所有桶，释放所有 entry）
void hashmap_destroy(hashmap_t *map){
    if(!map) return;

    for (size_t i = 0; i < map->bucket_count;i++){
        hashmap_entry_t *entry = map->buckets[i];
        while(entry){
            hashmap_entry_t *next = entry->next;
            if(entry->key) free(entry->key);
            free(entry);
            entry = next;
        }
    }

    free(map->buckets);
    free(map);
}

// 以下是整数 key 版本的接口，逻辑与上述字符串版本类似

int hashmap_put_int(hashmap_t *map, int key, void *value)
{
    if (!map)
        return -1;

    // 先检查是否是「新插入」
    uint64_t hash = hash_int(key);
    size_t idx = hash % map->bucket_count;
    hashmap_entry_t *e = map->buckets[idx];
    while (e)
    {
        if (e->key == NULL && e->int_key == key)
        {
            // key 已存在，覆盖
            e->value = value;
            return 0;
        }
        e = e->next;
    }

    // 新插入，需要先扩容（如果必要）
    if (hashmap_maybe_resize(map) != 0)
    {
        return -1;
    }

    // 重新计算 idx
    hash = hash_int(key);
    idx = hash % map->bucket_count;

    // 插入新节点
    e = malloc(sizeof(hashmap_entry_t));
    if (!e)
        return -1;
    e->int_key = key;
    e->value = value;
    e->key = NULL;
    e->next = map->buckets[idx];
    map->buckets[idx] = e;

    map->size += 1;
    return 0;
}

void *hashmap_get_int(hashmap_t *map, int key)
{
    if (!map)
        return NULL;
    uint64_t hash = hash_int(key);
    size_t idx = hash % map->bucket_count;
    for (hashmap_entry_t *e = map->buckets[idx]; e; e = e->next)
    {
        if (e->key == NULL && e->int_key == key)
        {
            return e->value;
        }
    }
    return NULL;
}

int hashmap_remove_int(hashmap_t *map, int key)
{
    if (!map)
        return -1;
    uint64_t hash = hash_int(key);
    size_t idx = hash % map->bucket_count;
    hashmap_entry_t *e = map->buckets[idx];
    hashmap_entry_t *prev = NULL;
    while (e)
    {
        if (e->key == NULL && e->int_key == key)
        {
            if (prev)
                prev->next = e->next;
            else
                map->buckets[idx] = e->next;
            free(e);
            map->size -= 1;
            return 0;
        }
        prev = e;
        e = e->next;
    }
    return -1;
}

