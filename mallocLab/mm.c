/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc(size_t size)
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

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
#define ALIGNMENT   8

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

/* Read and write a pointer at address p */
#define GET_PTR(p) (*(unsigned int **)(p))
#define PUT_PTR(p, ptr) (*((unsigned int **)(p)) = ((unsigned int *)(ptr)))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p)    (GET(p) & 0x1)

/* Read the predecessor and successor from the block pointer bp */
#define GET_PRED(bp)    ((char *)(bp))
#define GET_SUCC(bp)    ((char *)(bp) + WSIZE)

/* Get the block pointer bp from pred and succ */
#define GET_BP_PRED(pred)   ((char *)(pred))
#define GET_BP_SUCC(succ)   ((char *)(succ) - WSIZE)

/* Given block pointer bp, computer the address of its header and footer */
#define HDRP(bp)    ((char *)(bp) - WSIZE)
#define FTRP(bp)    ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block pointer bp, compute the address of next and previous blocks */
#define NEXT_BLKP(bp)   ((char *)(bp) + GET_SIZE((char *)(bp) - WSIZE))
#define PREV_BLKP(bp)   ((char *)(bp) - GET_SIZE((char *)(bp) - DSIZE))

/* Obtain an address that pred and succ are pointing to */
#define PREV_FREE(pred) (*(char **)(pred))
#define NEXT_FREE(succ) (*(char **)(succ))

/* Given block pointer bp, compute the previous and next block in the free list */
#define GET_PREV_LIST(bp)   (GET_BP_PRED(PREV_FREE(GET_PRED(bp))))
#define GET_NEXT_LIST(bp)   (GET_BP_SUCC(NEXT_FREE(GET_SUCC(bp))))

/* Rounds up to the neares multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

/*
 * Structures of the free and allocated blocks
 *
 * Allocated block
 | 31 |||||||||||||||||||||||||||||||||||| 3 | 2 | 1 | 0 |
 ---------------------------------------------------------
 |                   Block size              |    a/f    |
 ---------------------------------------------------------
 |                      Payload                          |
 ---------------------------------------------------------
 |                  Padding (if any)                     |
 ---------------------------------------------------------
 |                   Block size              |    a/f    |
 ---------------------------------------------------------
 *
 * Free block
 | 31 |||||||||||||||||||||||||||||||||||| 3 | 2 | 1 | 0 |
 ---------------------------------------------------------
 |                   Block size              |    a/f    |
 ---------------------------------------------------------
 |                     Predecessor                       |
 ---------------------------------------------------------
 |                     Successor                         |
 ---------------------------------------------------------
 |                     Remainder                         |
 ---------------------------------------------------------
 |                   Block size              |    a/f    |
 ---------------------------------------------------------
 *
 */
 
/*
 * Basic heap structure
 *
 | Start | Prologue hdr | ftr ||| block ||| Epilogue ftr |
 |   0   |      8/1     | 8/1 |||||||||||||     0/1      |
 *
 * Free list structure
      | Free_listp | -> | 1st node | -> ||||||| -> | Last node |
 Pred     No pred          NULL                     (N-1) pred
 Succ     1st succ        2nd succ                     NULL
 *
 */

 /*
  * Global variables
  */
 void *heap_listp;
 unsigned int *free_var = NULL;
 void **free_listp = (void **)(&free_var);
 int freeCnt = 0;//////////////////////////////
 int mallocCnt = 0;////////////////////////
 /* 
  * Function prototypes
  */
 /* Core functions */
 void *mm_malloc(size_t size);
 void mm_free(void *bp);
 void *mm_realloc(void *ptr, size_t size);
 /* Heap */
static int heap_init(void);
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void place(void *bp, size_t asize);
/* Free list */
static void insert_node(void *bp);
static void delete_node(void *bp);
static void move_node(void *curr_bp, void *moved_bp, size_t asize);
static int is_empty(void);
static void *find_fit(size_t asize);
/* Error-checking */
static void heap_print_fw(void);
static void heap_print_bw(void);
static void list_print_fw(void);
static void list_print_bw(void);

/* 
 * mm_init
 * Input: none
 * Output: -1(error), 0(no error)
 * Task: initializes heap and free list, extends heap by CHUNKSIZE bytes
 */
int mm_init(void)
{
    if(heap_init() == -1)
        return -1;
    
    if(extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    return 0;
}

/* 
 * mm_malloc
 * Input: size in bytes to be allocated
 * Output: allocated start address
 * Task: Allocated the block depending on the size, if not possible, extend the haep
 */
void *mm_malloc(size_t size)
{
    size_t asize;   // Adjusted block size
    size_t extendSize;  // Amount to extend heap if no fit
    char *bp;
    mallocCnt++;
    printf("mallocCnt: %d\n", mallocCnt);
    /* Ignores spurious requests */
    if(size == 0)
        return NULL;

    /* Adjusts block size to include overhead and alignment reqs */
    if(size <= DSIZE)
        asize = 2*DSIZE;
    else
        asize = DSIZE * ((size + DSIZE + DSIZE - 1) / DSIZE);

    /* Searches the free list for a fit (first fit) */
    if((bp = find_fit(asize)) != NULL)
    {
        place(bp, asize);
        heap_print_fw();
        list_print_fw();
        return bp;
    }

    /* No fit found. Gets more meory and place the block */
    extendSize = MAX(asize, CHUNKSIZE);
    if((bp = extend_heap(extendSize/WSIZE)) == NULL)
        return NULL;
    else
    {
        place(bp, asize);
        heap_print_fw();
        list_print_fw();
        return bp;
    }
}

/*
 * mm_free
 * Input: bp of block pointer wanted to be freed
 * Output: none
 * Task: manipulates the block, coalesces the block
 * Implementation Warning: node is managed by coalesce
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
    freeCnt++;
    printf("FreeCnt: %d\n", freeCnt);
    heap_print_fw();
    list_print_fw();   
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    return (void *)0;    
}

/*
 * heap_init
 * Input: none
 * Output: -1(error), 0(no error)
 * Task: initiates heap
 * Warning: 1. Must be followed by extend_heap at least once
 */
static int heap_init(void)
{
    if((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0); // Alignment padding
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));    // Prologue header
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));    // Prologue footer
    PUT(heap_listp + (3*WSIZE), PACK(0,1)); // Epilogue header
    heap_listp += (2*WSIZE);    // heap_listp now pointing prologue footer
    return 0;
}

/*
 * extend_heap
 * Input: size of increment in words (4 bytes)
 * Output: coalesced address
 * Task: extends heap with a new free block
 * Implementation Warning: 1. Must insert the new block to the free list
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
    
    /* Coalesce if the previous block was free */
    return coalesce(bp);
}

/*
 * coalesce
 * Input: bp of the block to be coalesced
 * Output: coalesced address
 * Task: coalesces the block based on the allocation of prev & next blocks
 * Implementation Warning: 1. Must consider the no element case (free_listp)
 *                         2. Must insert/delete nodes properly
 */
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    /* Case 1: prev and next both allocated */
    if(prev_alloc && next_alloc)
    {
        insert_node(bp);
    }
    /* Case 2: prev allocated and next free */
    else if(prev_alloc && !next_alloc)
    {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        delete_node(NEXT_BLKP(bp)); // next free block must be merged
        insert_node(bp);
    }

    /* Case 3: prev free and next allocated */
    else if(!prev_alloc && next_alloc)
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
        if(!is_empty())
            delete_node(bp);    // deletes the previous blk from the free list
        insert_node(bp);    // inserts the merged blk to the free list
    }

    /* Case 4: prev and next both free */
    else
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
        delete_node(bp);    // deletes the previous blk from the free list
        delete_node(NEXT_BLKP(NEXT_BLKP(bp)));  // deletes the next blk as well
        insert_node(bp);    // inserts the merged blk
    }
    
    return bp;
}

////////////////MUST MOVE THIS FUNCTION
/*
 * find_fit - Finds the fit using the first-fit policy
 * assuming heap space allocated at the beginning
 */
static void *find_fit(size_t asize)
{
    void *ptr = *free_listp;
        
    while(ptr != NULL)
    {
        if(asize <= GET_SIZE(HDRP(GET_BP_SUCC(ptr))))
            return ptr - WSIZE;
        ptr = NEXT_FREE(ptr);
    }
    return NULL;    // no fit
}

/* 
 * place
 * Input: block pointer of blk to be allocated, its size
 * Output: none
 * Task: allocate the blk by manipulating hdr, ftr, free_listp
 * Implementation Warning: 1. Must consider if the remainig space is large enough
 *                         2. If the remaining space is large enough, move the node
 * Warning: 1. Space should be larger than asize
 */
static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));

    if((csize - asize) >= (2*DSIZE))
    { 
        // Mark blk as allocated
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        // Mark the remaining blk
        PUT(HDRP(bp), PACK(csize-asize, 0));
        PUT(FTRP(bp), PACK(csize-asize, 0));
        move_node(PREV_BLKP(bp), bp, asize);
    }
    else
    {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
        delete_node(bp);
    }
}

/*
 * insert_node
 * Input: blk to be inserted
 * Output: none
 * Task: puts new blk right after the root (a node is inserted into the free list)
 * Implementation Warning: 1. Empty case should be handled seperately
 */
static void insert_node(void *bp)
{
    if(is_empty())
    {
        PUT_PTR(GET_PRED(bp), NULL);
        PUT_PTR(GET_SUCC(bp), NULL);
        PUT_PTR(free_listp, GET_SUCC(bp));
    }
    else
    {
        PUT_PTR(GET_PRED(bp), NULL);    // pred to NULL
        PUT_PTR(GET_SUCC(bp), NEXT_FREE(free_listp));      // succ to original next
        PUT_PTR(free_listp, GET_SUCC(bp));  // root pointing to current blk
        PUT_PTR(GET_PRED(GET_NEXT_LIST(bp)), GET_PRED(bp));  // original next backward-pointing current blk
    }
} 

/*
 * delete_node
 * Input: blk to be deleted from the free list
 * Output: none
 * Task: removes a blk from the free list
 * Warning: 1. delete_node must come after at least one insert_node
 */
static void delete_node(void *bp)
{
    if(is_empty())
        printf("ERROR: Deleting from an empty free list!\n");
    else if((PREV_FREE(GET_PRED(bp)) == NULL) && (NEXT_FREE(GET_SUCC(bp)) == NULL)) // deleting an unique node
    {
        PUT_PTR(free_listp, NULL);
    }
    else if(PREV_FREE(GET_PRED(bp)) == NULL)    // deleting the first node
    {
        PUT_PTR(free_listp, NEXT_FREE(GET_SUCC(bp)));
        PUT_PTR(GET_PRED(GET_NEXT_LIST(bp)), NULL);
    }
    else if(NEXT_FREE(GET_SUCC(bp)) == NULL)    // deleting the last node
    {
        PUT_PTR(GET_SUCC(GET_PREV_LIST(bp)), NULL);
    }
    else    // deleting an ordinary node
    {
        PUT_PTR(GET_SUCC(GET_PREV_LIST(bp)), NEXT_FREE(GET_SUCC(bp)));
        PUT_PTR(GET_PRED(GET_NEXT_LIST(bp)), PREV_FREE(GET_PRED(bp)));
    }
}

/*
 * move_node
 * Input: current blk pointer, destination blk pointer
 * Output: none
 * Task: Moves a node inside the heap without altering topology of the free list
 */
static void move_node(void *curr_bp, void *moved_bp, size_t asize)
{
    PUT_PTR((char *)(curr_bp) + asize, PREV_FREE(GET_PRED(curr_bp)));
    PUT_PTR((char *)(curr_bp) + asize + WSIZE, NEXT_FREE(GET_SUCC(curr_bp)));

    // Predecessor of the next block on the list is altered
    if(NEXT_FREE(GET_SUCC(curr_bp)) != NULL)
        PUT_PTR(GET_PRED(GET_NEXT_LIST(curr_bp)), GET_PRED(moved_bp));
    // Successor of the previous block on the list is altered
    if(PREV_FREE(GET_PRED(curr_bp)) != NULL)
        PUT_PTR(GET_SUCC(GET_PREV_LIST(curr_bp)), GET_SUCC(moved_bp));
    else PUT_PTR(free_listp, GET_SUCC(moved_bp));   // if the block is the first one
}

/*
 * is_empty
 * Input: none
 * Output: 1(empty), 0 (not empty)
 * Task: Checks if the free list is empty or not
 */
static int is_empty(void)
{
    if(NEXT_FREE(free_listp) == NULL)
        return 1;
    else return 0;
}

/*
 * heap_print_fw
 * Input: none
 * Output: none
 * Task: print heap from start to end
 */
static void heap_print_fw(void)
{
    char *bp = (char *)heap_listp;
    printf("HEAP START: ");
    while(GET_SIZE(HDRP(bp)) != 0)
    {
        printf("Alloc: %d, Size: %d, Addr: %p -> ", GET_ALLOC(HDRP(bp)), GET_SIZE(HDRP(bp)),  bp);
        bp = NEXT_BLKP(bp);
    }
    printf("HEAP END\n");
}

/* 
 * heap_print_bw
 * Input: none
 * Output: none
 * Task: print heap from end to start
 */
static void heap_print_bw(void)
{
    char *bp = (char *)heap_listp;
    while(GET_SIZE(HDRP(NEXT_BLKP(bp))) != 0)
        bp = NEXT_BLKP(bp);

    printf("END: ");
    while(bp != ((char *)heap_listp + WSIZE))
    {
        printf("Size: %d, Addr: %p -> ", GET_SIZE(HDRP(bp)), bp);
        bp = PREV_BLKP(bp);
    }
    printf("START\n");
}

/*
 * list_print_fw
 * Input: none
 * Output: none
 * Task: print free list from start to end
 */
static void list_print_fw(void)
{
    char *node = *(char **)free_listp;  // 'node' stores the successor of the node
    printf("LIST START: ");
    while(NEXT_FREE(node) != NULL)
    {
        node = NEXT_FREE(node);
        printf("Size: %d, Addr: %p, Prev: %p, Next: %p -> ", GET_SIZE(HDRP(GET_BP_SUCC(node))), GET_BP_SUCC(node), GET_PREV_LIST(GET_BP_SUCC(node)), GET_NEXT_LIST(GET_PRED(GET_BP_SUCC(node))));
    }
    printf("LIST END\n");
}
