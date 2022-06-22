#include "csapp.h"
#include "cache.h"

void initializeCache(cache_t* cache){
    cache->size = 0;
    cache->nitems = 0;

    cache->head = Malloc(sizeof(*(cache->head)));
    cache->head->flag = '@';
    cache->head->prev = NULL;
    cache->head->next = NULL;

    cache->tail = Malloc(sizeof(*(cache->tail)));
    cache->tail->flag = '@';
    cache->tail->prev = NULL;
    cache->tail->next = NULL;

    /* construct the doubly linked list */
    cache->head->next = cache->tail;
    cache->tail->prev = cache->head;

    cache->nitems = 0;
}

void writeToCache(cache_t* cache, cache_node* node) {
    /* step1: check current capacity, if full ,delete one */
    while(node->respBodyLen + cache->size >= MAX_CACHE_SIZE && cache->head->next != cache->tail) {
        cache_node* last = cache->tail->prev;
        last->prev->next = last->next;
        last->next->prev = last->prev;

        last->prev = NULL;
        last->next = NULL;
        Free(last);
    }

    /* step2: add into the cache */
    node->prev = cache->head;
    node->next = cache->head->next;
    cache->head->next->prev = node;
    cache->head->next = node;
    cache->size += node->respBodyLen;
}