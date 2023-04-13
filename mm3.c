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
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))//

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

#define PREV_FREE_ADDRESS(bp) (*(void **)(bp))
#define NEXT_FREE_ADDRESS(bp) (*(void **)(bp + WSIZE))


static char* heap_listp = NULL; 


static void add_address(void *bp){
    NEXT_FREE_ADDRESS(bp) = heap_listp;
    PREV_FREE_ADDRESS(heap_listp) = bp;
    PREV_FREE_ADDRESS(bp) = NULL;
    heap_listp = bp;
}

static void remove_address(void *bp){
    if (bp == heap_listp){
        PREV_FREE_ADDRESS(NEXT_FREE_ADDRESS(bp)) = 0;
        heap_listp = NEXT_FREE_ADDRESS(bp);
    }
    else {//넥스트 어드레스의 전 주소를 나의 전 주소로 리셋
    //전 어드레스의 다음 주소를 나의 다음 주소로 리셋
        NEXT_FREE_ADDRESS(PREV_FREE_ADDRESS(bp)) = NEXT_FREE_ADDRESS(bp);
        PREV_FREE_ADDRESS(NEXT_FREE_ADDRESS(bp)) = PREV_FREE_ADDRESS(bp);
    }
}

static void *coalesce(void *bp){

    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    // case 1
    if (prev_alloc && next_alloc){
        add_address(bp);
    }
    // case 2
    else if (prev_alloc && !next_alloc){
        remove_address(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        add_address(bp);
    }
    //case 3
    else if (!prev_alloc && next_alloc){
        remove_address(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        bp = PREV_BLKP(bp);
        add_address(bp);
    }
    else {
        remove_address(NEXT_BLKP(bp));
        remove_address(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
        add_address(bp);
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
    if ((heap_listp = mem_sbrk(6*WSIZE)) == (void *)-1)
        return -1;
        
    PUT(heap_listp, 0);
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE*2, 1));
    PUT(heap_listp + (2*WSIZE), 0);
    PUT(heap_listp + (3*WSIZE), 0);
    PUT(heap_listp + (4*WSIZE), PACK(DSIZE*2, 1));
    PUT(heap_listp + (5*WSIZE), PACK(0, 1));
    
    heap_listp += DSIZE;

    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    return 0;
}

static void *find_first(size_t asize){
    char *bp;
    bp = (void *)heap_listp;

    //GET_ALLOC(HDRP(bp))
    while (bp != NULL)
    { 
        if (GET_SIZE(HDRP(bp)) >= asize){
            return (void *)bp;
        } 
        
        bp = NEXT_FREE_ADDRESS(bp);
    }

    return NULL;  
}

// static void *find_first(size_t asize) {
//     void *bp;
    
//     // header block을 만날 때까지 for문 탐색
//     for (bp = heap_listp; GET_ALLOC(HDRP(bp)) != 1; bp = NEXT_FREE_ADDRESS(bp)) {
//         if (GET_SIZE(HDRP(bp)) >= asize)
//             return bp;
//     }
//     return NULL;
// }

static void place(void *bp, size_t asize) {
    size_t empty_size = GET_SIZE(HDRP(bp));
    remove_address(bp);

    // 최소블럭크기 미만의 오차로 딱 맞다면 - 그냥 헤더, 푸터만 갱신해주면 됨
    if ((empty_size - asize) < 2*DSIZE ) {
        PUT(HDRP(bp), PACK(empty_size, 1));
        PUT(FTRP(bp), PACK(empty_size, 1));
    }
    // 넣고도 최소블럭크기 이상으로 남으면 - 헤더는 업데이트, 남은 블록 별도로 헤더, 푸터 처리
    else {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(empty_size - asize, 0));
        PUT(FTRP(bp), PACK(empty_size - asize, 0));
        add_address(bp);
    }
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





