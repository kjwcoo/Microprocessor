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
 *///
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
    "qteam",
    /* First member's full name */
    "Jinwoong Kim",
    /* First member's email address */
    "kjw.kim@gmail.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/*
 * Basic constants and macros
 */

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* Aligned size of size_t */
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* Word and header/footer size in bytes */
#define WSIZE   4

/* Double word size in bytes */
#define DSIZE   8

/* Extend heap by this amount in bytes */
#define CHUNKSIZE   (1<<12)

/* Max function */
#define MAX(x,y)    ((x) > (y) ? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)   ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)  (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

/* Reads a address of a pointer */
#define GET_ADDR(p) (&(p))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p)    (GET(p) & 0x1)

/* Read the predecessor and successor from block pointer bp */
#define GET_PRED(bp)    (bp) 
#define GET_SUCC(bp)    (((char *)(bp)) + WSIZE)

/* Write a pointer value at address p */
#define PUT_PTR(p, ptr)   ((*(unsigned int *)(p)) = ((unsigned int)(ptr)))    

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)    ((char *)(bp) - WSIZE)
#define FTRP(bp)    ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)   ((char *)(bp) + GET_SIZE((char *)(bp) - WSIZE))
#define PREV_BLKP(bp)   ((char *)(bp) - GET_SIZE((char *)(bp) - DSIZE))

/* Given block ptr bp, compute address of next and previous free blocks */
#define NEXT_FREE(bp)   (*(char **)(GET_SUCC(bp)) - WSIZE)
#define PREV_FREE(bp)   (*(char **)(GET_PRED(bp))) 

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


/*
 * Structures of the free and allocated blocks
 *
 * Allocated block
 | 31 | |||||||||||||||||||||||||||||||| 3 | 2 | 1 | 0 |
 -------------------------------------------------------
 |              Block size                 |    a/f    |
 -------------------------------------------------------
 |                      Payload                        |
 -------------------------------------------------------
 |                  Padding (if any)                   |
 -------------------------------------------------------
 |              Block size                 |    a/f    |
 -------------------------------------------------------
 *
 * Free block
 | 31 | |||||||||||||||||||||||||||||||| 3 | 2 | 1 | 0 |
 -------------------------------------------------------
 |              Block size                 |    a/f    |
 -------------------------------------------------------
 |                      Predecessor                    |
 -------------------------------------------------------
 |                      Successor                      |
 -------------------------------------------------------
 |                      Remainder bits                 |
 -------------------------------------------------------
 |                  Padding (if any)                   |
 -------------------------------------------------------
 |              Block size                 |    a/f    |
 -------------------------------------------------------
 *
 */

/*
 * Basic heap structure
 *
 * Start | Prologue hdr | ftr | ||| blocks ||| | Epilogue ftr
 *   0   |      8/1     | 8/1 | |||||||||||||| |      0/1
 *
 */

/*
 * Global variables
 */
void *heap_listp;
unsigned int *free_list;
void **free_listp = (void **)(&free_list);

/*
 * Helper function prototypes
 */
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);
static void insert_node(void *bp);
static void delete_node(void *bp);
static void move_node(void *bp);


/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    /* Creates the initial empty heap */
    if((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0); // Alignment padding
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1)); // Prologue header
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1)); // Prologue footer
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));     // Epilogue header
    heap_listp += (2*WSIZE);    // heap_listp now pointing prologue footer
    
    PUT_PTR(free_listp, NULL);

    /* Extends the empty heap with a free block of CHUNKSIZE bytes */
    if(extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
   
    return 0;
}

/* 
 * mm_malloc - Allocates a block by either using the free list or requesting more heap
 *     Always allocates a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;   // Adjusted block size
    size_t extendSize;  // Amount to extend heap if no fit
    char *bp;
    printf("size: %d\n", size);///////////////////////////////////
    /* Ignores spurious requests */
    if(size == 0)
        return NULL;

    /* Adjusts block size to include overhead and alignment reqs */
    asize = ALIGN(size);

    /* Searches the free list for a fit (first fit) */
    if((bp = find_fit(asize)) != NULL)
    {
        place(bp, asize);
        return bp;
    }

    /* No fit found. Gets more meory and place the block */
    extendSize = MAX(asize, CHUNKSIZE);
    if((bp = extend_heap(extendSize/WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}

/*
 * mm_free - Frees a block and uses boundary-tag coalescing to merge
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));
    
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    insert_node(bp);
    coalesce(bp);   
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    
}

/*
 * mm_check - Checking heap consistency
 */
int mm_check(void *ptr)
{
    char * pointer = *(char **)free_listp - WSIZE;
    printf("Free list: ROOT -> ");
    
    while(GET_SUCC(pointer) != NULL)
    {
        printf("%p -> ", pointer);
        pointer = NEXT_FREE(pointer);
    }

    printf("NULL\n");
    return 0;
}

/*
 * extend_heap - Extends heap with a new free block
 */
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;
    /* Allocates an even number of words to maintain alignment */
    size = (words % 2) ? ((words + 1) * WSIZE) : (words * WSIZE);
    if((long)(bp = mem_sbrk(size)) == -1)
        return NULL;
    
    /* Initialize free block header/footer and epilogue header */
    PUT(HDRP(bp), PACK(size, 0));   // Free block header
    PUT(FTRP(bp), PACK(size, 0));   // Free block footer
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));   // New epilogue header
    
    /* Initialize predecessor and successor */
    insert_node(bp);
    
    /* Coalesce if the previous block was free */
    return coalesce(bp);
}

/*
 * coalesce - Coalesces accroding to four cases
 */
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    printf("prev_alloc: %d ", prev_alloc);
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    printf("next_alloc: %d\n", next_alloc);
    size_t size = GET_SIZE(HDRP(bp));

    /* Case 1: prev and next both allocated */
    if(prev_alloc && next_alloc)
        return bp;

    /* Case 2: prev allocated and next free */
    else if(prev_alloc && !next_alloc)
    {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        delete_node(NEXT_FREE(bp));
        insert_node(bp);
    }

    /* Case 3: prev free and next allocated */
    else if(!prev_alloc && next_alloc)
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
        delete_node(NEXT_FREE(bp));
        insert_node(bp);
    }

    /* Case 4: prev and next both free */
    else
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
        delete_node(NEXT_FREE(bp));
        delete_node(NEXT_FREE(NEXT_FREE(bp)));
        insert_node(bp);
    }
    
    return bp;
}

/*
 * find_fit - Finds the fit using the first-fit policy
 */
static void *find_fit(size_t asize)
{
    void *ptr;
    
    for(ptr = (char *)free_listp - WSIZE; GET_SIZE(HDRP(ptr)) > 0; ptr = NEXT_FREE(ptr))
    {
        if(asize <= GET_SIZE(HDRP(ptr)))
            return ptr;
    }
    return NULL;    // no fit
}

/* 
 * place - Places the new block
 */
static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));

    if((csize - asize) >= (2*DSIZE))
    { 
        PUT_PTR((char *)(bp) + asize, GET_PRED(bp));
        PUT_PTR((char *)(bp) + asize + WSIZE, GET_SUCC(bp));
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize-asize, 0));
        PUT(FTRP(bp), PACK(csize-asize, 0));
        move_node(bp);
    }
    else
    {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
        delete_node(bp);
    }
}

/*
 * insert_node - Inserts a node into the free list
 */
static void insert_node(void *bp)
{
    PUT_PTR(GET_PRED(bp), NULL);    // pred to NULL
    PUT_PTR(GET_SUCC(bp), free_listp);      // succ to original next
    PUT_PTR(free_listp, GET_SUCC(bp));
    
} 

/*
 * delete_node - Deletes a node from the free list
 */
static void delete_node(void *bp)
{
    PUT_PTR(GET_SUCC(PREV_FREE(bp)), GET_SUCC(NEXT_FREE(bp)));  // prev block succ
    PUT_PTR(GET_PRED(NEXT_FREE(bp)), GET_PRED(PREV_FREE(bp)));  // next block pred
}

/*
 * move_node - Moves a node in the free list
 * Splitting ensures pred and succ would be copied
 */
static void move_node(void *bp)
{
    PUT_PTR(GET_SUCC(PREV_FREE(bp)), GET_SUCC(bp)); // prev block succ
    PUT_PTR(GET_PRED(NEXT_FREE(bp)), GET_PRED(bp)); // next block pred
}


