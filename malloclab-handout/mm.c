/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

//字大小和双字大小
#define WSIZE 4
#define DSIZE 8
//当堆内存不够时，向内核申请的堆空间(1页 = 4K for Linux)
#define CHUNKSIZE (1<<12)
//将val放入p开始的4字节中
#define PUT(p,val) (*(unsigned int*)(p) = (val))
//获得头部和脚部的编码
#define PACK(size, alloc) ((size) | (alloc))
//从头部或脚部获得块大小和已分配位
#define GET_SIZE(p) (*(unsigned int*)(p) & ~0x7)
#define GET_ALLO(p) (*(unsigned int*)(p) & 0x1)
//获得块的头部和脚部
#define HDRP(bp) ((char*)(bp) - WSIZE)
#define FTRP(bp) ((char*)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
//获得上一个块和下一个块
#define NEXT_BLKP(bp) ((char*)(bp) + GET_SIZE(HDRP(bp)))
#define PREV_BLKP(bp) ((char*)(bp) - GET_SIZE((char*)(bp) - DSIZE))

#define MAX(x, y) ((x) > (y) ? (x) : (y))

static void* expend_heap(size_t words);
static void* imme_coalesce(void* bp);
static void* first_fit(size_t asize);
static void place(void* bp, size_t asize);

//指向隐式空闲链表的序言块的有效载荷
static char *heap_listp;
/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    heap_listp = mem_sbrk(4 * WSIZE);
    if (heap_listp == (void*)-1)
        return -1;
    PUT(heap_listp, 0);	//填充块
    PUT(heap_listp + 1 * WSIZE, PACK(DSIZE, 1));  //序言块头部
    PUT(heap_listp + 2 * WSIZE, PACK(DSIZE, 1));  //序言块脚部
    PUT(heap_listp + 3 * WSIZE, PACK(0, 1));      //结尾块

    heap_listp += DSIZE;  //指向序言块有效载荷的指针

    if(expend_heap(CHUNKSIZE/WSIZE) == NULL)	//申请更多的堆空间
		return -1;

    return 0;
}

static void *expend_heap(size_t words) {
    size_t size;
    void* bp;

    size = words % 2 ? (words + 1) * WSIZE : words * WSIZE;
    if((bp = mem_sbrk(size)) == (void*)-1)	//申请空间
		return NULL;

    PUT(HDRP(bp), PACK(size, 0));  //设置头部
    PUT(FTRP(bp), PACK(size, 0));  //设置脚部
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));	//设置新的结尾块

    //立即合并
    return imme_coalesce(bp);
}

static void *imme_coalesce(void *bp){
    void* prev_bp = PREV_BLKP(bp);
    void* next_bp = NEXT_BLKP(bp);
    size_t prev_alloc = GET_ALLO(HDRP(prev_bp));    //获得前面块的已分配位
    size_t next_alloc = GET_ALLO(HDRP(next_bp));    //获得后面块的已分配位
    size_t size = GET_SIZE(HDRP(bp));	//获得当前块的大小

    if(prev_alloc && next_alloc)
        return bp;
    else if (prev_alloc && !next_alloc) {
        size += GET_SIZE(HDRP(next_bp));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        return bp;
    } else if (!prev_alloc && next_alloc) {
        size += GET_SIZE(HDRP(prev_bp));
        PUT(HDRP(prev_bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        return prev_bp;
    } else {
        size += GET_SIZE(HDRP(prev_bp)) + GET_SIZE(HDRP(next_bp));
        PUT(HDRP(prev_bp), PACK(size, 0));
        PUT(FTRP(next_bp), PACK(size, 0));
        return prev_bp;
    }
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize; // 申请空间
    void* bp;

    if(size == 0)
        return NULL;

    //满足最小块要求和对齐要求，size是有效负载大小
    asize = size <= DSIZE ? 2 * DSIZE : ALIGN(size) + DSIZE;
    //首次匹配
	if ((bp = first_fit(asize)) != NULL){
		place(bp, asize);
		return bp;
	}

    if ((bp = expend_heap(MAX(asize, CHUNKSIZE)/WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}

static void *first_fit(size_t asize){
	void *bp = heap_listp;
	size_t size;
	while((size = GET_SIZE(HDRP(bp))) != 0){	//遍历全部块
		if(size >= asize && !GET_ALLO(HDRP(bp)))	//寻找大小大于asize的空闲块
			return bp;
		bp = NEXT_BLKP(bp);
	}
	return NULL;
} 

static void place(void *bp, size_t asize){
    size_t remain_size = GET_SIZE(HDRP(bp)) - asize;
    if (remain_size >= DSIZE) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        PUT(HDRP(NEXT_BLKP(bp)), PACK(remain_size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(remain_size, 0));
    } else {
        PUT(HDRP(bp), PACK(GET_SIZE(HDRP(bp)), 1));
        PUT(FTRP(bp), PACK(GET_SIZE(HDRP(bp)), 1));
    }
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));

    //立即合并
    imme_coalesce(ptr);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t asize;
    size_t oldSize = GET_SIZE(HDRP(ptr));

    if (ptr == NULL)
        return mm_malloc(size);

    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }

    //满足最小块要求和对齐要求，size是有效负载大小
    asize = size <= DSIZE ? 2 * DSIZE : ALIGN(size) + DSIZE;

    if (oldSize == asize)
        return ptr;
    else if (oldSize > asize) {
        place(ptr, asize);
        return ptr;
    } else {
        newptr = imme_coalesce(ptr);
        if (GET_SIZE(HDRP(newptr)) - DSIZE >= asize) {
            memcpy(newptr, ptr, oldSize - DSIZE);
            place(newptr, asize);
        } else {
            if ((newptr = mm_malloc(asize)) == NULL)
                return NULL;
            memcpy(newptr, ptr, oldSize - DSIZE);
            mm_free(ptr);
            return newptr;
        }
    }

    return newptr;
}














