#ifndef HASH_MAP_H
#define HASH_MAP_H

#include <stdlib.h>
#include <stdint.h>

// 定义链表节点（同时支持字符串 key 和整数 key）
typedef struct hashmap_entry
{
    char *key; // 字符串 key，如果是整数哈希，则该字段为 NULL
    int int_key; // 整数 key，如果是字符串哈希，则该字段无意义
    void *value; // 储存的值
    struct hashmap_entry *next;
} hashmap_entry_t;

// 定义哈希表主体
typedef struct hashmap
{
    size_t bucket_count; // 当前桶数量
    size_t size; // 当前键值对数量
    hashmap_entry_t **buckets; // 指向桶数组（每个桶是条链表）的指针
} hashmap_t;

// 函数声明
hashmap_t* hashmap_create(size_t bucket_count);

int hashmap_put(hashmap_t *map, const char *key, void *value);
void *hashmap_get(hashmap_t *map, const char *key);
int hashmap_remove(hashmap_t *map, const char *key);

int hashmap_put_int(hashmap_t *map, int key, void *value);
void *hashmap_get_int(hashmap_t *map, int key);
int hashmap_remove_int(hashmap_t *map, int key);

void hashmap_destroy(hashmap_t *map);

int hashmap_resize(hashmap_t *map);

#endif