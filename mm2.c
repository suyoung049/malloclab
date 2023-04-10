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

// define 오른쪽 실행을 왼쪽 명령어로 실행
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<12)

/*if (x > y) {
    retrun x   
} else {
    return y
}
*/
#define MAX(x, y) ((x) > (y)? (x):(y))
#define PACK(size, alloc) ((size)|(alloc))

#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

static void *coalesce(void *bp){

    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    // case 1
    if (prev_alloc && next_alloc){
        return bp;
    }
    // case 2
    else if (prev_alloc && !next_alloc){
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));   
    }
    //case 3
    else if (!prev_alloc && next_alloc){
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    else {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    return bp;

}

static void *extend_heap(size_t words){
    char *bp;
    size_t size;
    /*if (words % 2) {
        size = (words + 1) * WSIZE;
    } else {
        size = words * WSIZE;
    }*/
    //요청한 크기를 인접 2워드의 배수(8바이트)로 반올림하며, 그 후에 메모리 시스템으로 부터 추가적인 힙 공간 요청
    size = (words % 2) ? (words +1) * WSIZE : words * WSIZE;
    if ((long) (bp = mem_sbrk(size)) == -1)
        return NULL;

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0,1));

    return coalesce(bp);
}
/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    char *heap_listp;

    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0);
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));
    heap_listp += (2*WSIZE);

    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    return 0;
}

static void *find_first(size_t asize){
    char *bp;
    bp = (char *)mem_heap_lo() + DSIZE;

    while (bp <= (char *)mem_heap_hi())
    {
        if (!GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) >= asize){
            return (void *)bp;
        } 
        bp = NEXT_BLKP(bp);
    }

    return NULL;  
}

void place(char *bp, size_t asize){
    size_t empty_size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(asize, 1));
    PUT(FTRP(bp), PACK(asize, 1));

    if (empty_size == asize){
        return;
    }

    size_t remain_size = empty_size - asize;
    // 남은 공간이 최소 블록 크기보다 큰 경우
    PUT(HDRP(NEXT_BLKP(bp)), PACK(remain_size, 0));
    PUT(FTRP(NEXT_BLKP(bp)), PACK(remain_size, 0));
    
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
   size_t asize;
   size_t extendsize;
   char *bp;

   if (size == 0){
    return NULL;
   }

   if (size <= DSIZE){
    asize = 2*DSIZE;
   } else {
    asize = DSIZE * ((size + (DSIZE) + (DSIZE-1))/DSIZE);
   }
   
   if ((bp = find_first(asize)) != NULL){
    place(bp, asize);
    return bp;
   }

   extendsize = MAX(asize, CHUNKSIZE);
   if ((bp = extend_heap(extendsize/WSIZE)) == NULL) {
    return NULL;
   }
   place(bp, asize);
   return bp;

}
/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));
    
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));

    coalesce(ptr);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size){
    if(size <= 0){ 
        mm_free(ptr);
        return 0;
    }
    if(ptr == NULL){
        return mm_malloc(size); 
    }
    void *newp = mm_malloc(size); 
    if(newp == NULL){
        return 0;
    }
    size_t oldsize = GET_SIZE(HDRP(ptr));
    if(size < oldsize){
    	oldsize = size; 
	}
    memcpy(newp, ptr, oldsize); 
    mm_free(ptr);
    return newp;
}

