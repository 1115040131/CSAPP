#include "cachelab.h"
#include <stdio.h>
#include <getopt.h> // unix系统读如命令行内容相关
#include <string.h>
#include <stdlib.h>

static int S;
static int E;
static int B;
static int hits = 0;
static int misses = 0;
static int evictions = 0;

typedef struct _LRUNode LRUNode;
struct _LRUNode {
    unsigned tag;
    LRUNode* left;
    LRUNode* right;
};

typedef struct _LRU LRU;
struct _LRU {
    unsigned size;
    LRUNode* head;
    LRUNode* tail;
};

int parseOption(int argc, char** argv, char* fileName){
    char option;
    while ((option = getopt(argc, argv, "s:E:b:t:")) != -1) {
        switch (option) {
            case 's':
                S = atoi(optarg);
                break;
            case 'E':
                E = atoi(optarg);
                break;
            case 'b':
                B = atoi(optarg);
                break;
            case 't':
                strncpy(fileName, optarg, 100);
                break;
        }
    }

    return 1 << S;
}

LRU* initializeCache(int setNum) {
    LRU* lru = malloc(setNum * sizeof(LRU));
    for (int i = 0; i < setNum; i++) {
        lru[i].size = 0;
        lru[i].head = malloc(sizeof(LRUNode));
        lru[i].tail = malloc(sizeof(LRUNode));
        lru[i].head->right = lru[i].tail;
        lru[i].tail->left = lru[i].head;
    }
    return lru;
}

void removeNode(LRUNode* curr) {
    curr->left->right = curr->right;
    curr->right->left = curr->left;
}

void insertNode(LRU* lru, LRUNode* curr) {
    curr->left = lru->head;
    curr->right = lru->head->right;
    lru->head->right->left = curr;
    lru->head->right = curr;
}

void update(LRU* lru, unsigned addr) {
    unsigned mask = 0xffffffff;
    unsigned setMask = mask >> (32 - S);
    unsigned setIdx = (addr >> B) & setMask;
    unsigned tag = addr >> (B + S);

    lru += setIdx;
    int find = 0;
    LRUNode* curr = lru->head->right;
    while(curr != lru->tail) {
        if(curr->tag == tag) {
            find = 1;
            break;
        }
        curr = curr->right;
    }

    if (find) {
        printf(" hit");
        hits++;
        if (curr != lru->head->right) {
            removeNode(curr);
            insertNode(lru, curr);
        }
    }
    else {
        printf(" miss");
        misses++;
        if (lru->size == E) {
            printf(" eviction");
            evictions++;
            removeNode(lru->tail->left);
            lru->size--;
        }
        LRUNode* newNode = malloc(sizeof(LRUNode));
        newNode->tag = tag;
        insertNode(lru, newNode);
        lru->size++;
    }
}

void cacheSimulator(char* fileName, int setNum) {
    LRU* lru = initializeCache(setNum);

    FILE* file = fopen(fileName, "r");
    char op;
    unsigned addr;
    int size;
    while(fscanf(file, " %c %x,%d", &op, &addr, &size) > 0) {
        printf("%c %x,%d", op, addr, size);
        switch(op) {
            case 'M':
                update(lru, addr);
            case 'L':
            case 'S':
                update(lru, addr);
                break;
        }
        printf("\n");
    }
}

int main(int argc, char** argv)
{
    // step 1: parse option
    char fileName[101];
    int setNum = parseOption(argc, argv, fileName);

    // step 2: cache simulator
    cacheSimulator(fileName, setNum);

    printSummary(hits, misses, evictions);
    return 0;
}
