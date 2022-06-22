#ifndef __CACHE_H__
#define __CACHE_H__

#include "csapp.h"
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

typedef struct _cache_node cache_node;
struct _cache_node {
    char flag;
    char uri[100];
    char respHeader[1024];
    char respBody[MAX_OBJECT_SIZE];
    int respHeaderLen;
    int respBodyLen;
    cache_node* prev;
    cache_node* next;
};

typedef struct _cache_t{
    cache_node* head;
    cache_node* tail;    
    int nitems;
    int size;
} cache_t;

//write to cache
//read cache
//search cache

void initializeCache(cache_t* );

void writeToCache(cache_t* cache, cache_node* node);

#endif /* __CACHE_H__ */