#include "threads/malloc.h"
#include <debug.h>
#include <list.h>
#include <round.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"

/* A simple implementation of malloc().

   The size of each request, in bytes, is rounded up to a power
   of 2 and assigned to the "descriptor" that manages blocks of
   that size.  The descriptor keeps a list of free blocks.  If
   the free list is nonempty, one of its blocks is used to
   satisfy the request.
 
   Otherwise, a new page of memory, called an "arena", is
   obtained from the page allocator (if none is available,
   malloc() returns a null pointer).  The new arena is divided
   into blocks, all of which are added to the descriptor's free
   list.  Then we return one of the new blocks.

   When we free a block, we add it to its descriptor's free list.
   But if the arena that the block was in now has no in-use
   blocks, we remove all of the arena's blocks from the free list
   and give the arena back to the page allocator.

   We can't handle blocks bigger than 2 kB using this scheme,
   because they're too big to fit in a single page with a
   descriptor.  We handle those by allocating contiguous pages
   with the page allocator and sticking the allocation size at
   the beginning of the allocated block's arena header. */

/* Descriptor. */
struct desc{
  size_t block_size;          /* Size of each element in bytes. */
  struct list free_list;      /* List of free blocks. */
};

/* Magic number for detecting arena corruption. */
#define ARENA_MAGIC 0x9a548eed
  
struct lock lock;           /* Lock. */

/* Arena. */
struct arena {
  unsigned magic;             /* Always set to ARENA_MAGIC. */
  struct list_elem elem;
};

/* Free block. */
struct block{
  uint8_t tmp[2]; // t[0] for index,t[1] for checking if block is in use 
  struct list_elem free_elem; /* Free list element. */
};

/* Our set of descriptors. */
static struct desc descs[10];   /* Descriptors. */
static size_t desc_cnt;         /* Number of descriptors. */

static struct arena *block_to_arena (struct block *);
static struct block *arena_to_block (struct arena *);
bool compare(const struct block * b1,const struct block * b2,void * aux);
void splitBigBlock(struct block *b,int level,int oldLevel);
static struct list arenaList;
struct block* getBuddy(struct block*,int index);
void printMemory();
/* Initializes the malloc() descriptors. */
void
malloc_init (void) 
{
  size_t block_size;

  for (block_size = 16; block_size < PGSIZE / 2; block_size *= 2)
    {
      struct desc *d = &descs[desc_cnt++];
      ASSERT (desc_cnt <= sizeof descs / sizeof *descs);
      d->block_size = block_size;
      //d->blocks_per_arena = (PGSIZE - sizeof (struct arena)) / block_size;
      list_init (&d->free_list);
    }
  lock_init (&lock);
  list_init(&arenaList);

}


void splitBigBlock(struct block *b,int level,int oldLevel){
  struct block *temp;

  while(1){
    if(level==oldLevel)break;
    temp = getBuddy(b,level-1);
    temp->tmp[0] = level-1;
    temp->tmp[1] = 0;  
    list_insert_ordered(&(descs[level-1].free_list),&temp->free_elem,&compare,(void *)NULL);
    level--;
    printf("\n\nYHA kyu PE bhi THA MAI,level=%d,oldLevel=%d\n\n",level,oldLevel );

  }

}


/* Obtains and returns a new block of at least SIZE bytes.
   Returns a null pointer if memory is not available. */
void *
malloc (size_t size) 
{
  struct desc *d;
  struct block *b;
  struct arena *a;

  /* A null pointer satisfies a request for 0 bytes. */
  if (size == 0)
    return NULL;

  /* Find the smallest descriptor that satisfies a SIZE-byte
     request. */
  size=size+2*sizeof(char);
  int level=0,oldLevel=0;
  desc_cnt=7;
  for (d = descs; d < descs + desc_cnt; d++,level++){
    if (d->block_size >= size)
      break;
  }
  
  printf("\n\ndesk_cnt=%d\n\n",level);

  if (d == descs + desc_cnt) 
    {
      /* SIZE is too big for any descriptor.
         Allocate enough pages to hold SIZE plus an arena. */
      size_t page_cnt = DIV_ROUND_UP (size + sizeof *a, PGSIZE);
      a = palloc_get_multiple (0, page_cnt);
      if (a == NULL)
        return NULL;

      /* Initialize the arena to indicate a big block of PAGE_CNT
         pages, and return it. */
      a->magic = ARENA_MAGIC;
      list_push_front(&arenaList,&a->elem);
      b = arena_to_block(a);
      b->tmp[0]=7;
      b->tmp[1]=1;
      b=(void *)((char*)(b)+2);
      return b;
    }

  lock_acquire (&lock);

  /* If the free list is empty, create a new arena. */
  oldLevel=level;
  printf("level=%d , oldLevel=%d \n",level,oldLevel );


  if (list_empty (&d->free_list)){
    while(list_empty(&d->free_list)){
      d++;
      level++;
      if(level==7)break;
    }
    if(level==7){
      
      
      a=palloc_get_page(0);
      if(a==NULL){
        lock_release(&lock);
        return NULL;
      }
      a->magic = ARENA_MAGIC;
      list_push_front(&arenaList,&a->elem);
      b = arena_to_block(a);
    }
    else
      b = list_entry(list_pop_front(&(d->free_list)),struct block,free_elem);  
  }
  else
    b = list_entry(list_pop_front(&(d->free_list)),struct block,free_elem);



  if(level!=oldLevel)
    splitBigBlock(b,level,oldLevel);

  b->tmp[0]=oldLevel;
  b->tmp[1]=1;
  b=(void *)((char*)(b) + 2);
  lock_release (&lock);
  return b;
}


/* Allocates and return A times B bytes initialized to zeroes.
   Returns a null pointer if memory is not available. */
void *
calloc (size_t a, size_t b) 
{
  void *p;
  size_t size;

  /* Calculate block size and make sure it fits in size_t. */
  size = a * b;
  if (size < a || size < b)
    return NULL;

  /* Allocate and zero memory. */
  p = malloc (size);
  if (p != NULL)
    memset (p, 0, size);

  return p;
}

/* Returns the number of bytes allocated for BLOCK. */
static size_t
block_size (void *block) 
{
  struct block *b =(struct block *)( (char *)block - 2);
  return 1<<(b->tmp[0]+4);
}


/* Attempts to resize OLD_BLOCK to NEW_SIZE bytes, possibly
   moving it in the process.
   If successful, returns the new block; on failure, returns a
   null pointer.
   A call with null OLD_BLOCK is equivalent to malloc(NEW_SIZE).
   A call with zero NEW_SIZE is equivalent to free(OLD_BLOCK). */
void *
realloc (void *old_block, size_t new_size) 
{
  if (new_size == 0) 
    {
      free (old_block);
      return NULL;
    }
  else 
    {
      void *new_block = malloc (new_size);
      if (old_block != NULL && new_block != NULL)
        {
          size_t old_size = block_size (old_block) - 2;
          size_t min_size = new_size < old_size ? new_size : old_size;
          memcpy (new_block, old_block, min_size);
          free (old_block);
        }
      return new_block;
    }
}

/* Frees block P, which must have been previously allocated with
   malloc(), calloc(), or realloc(). */

/*
struct block* getBuddy(struct block *b){
  uintptr_t tmp = (uintptr_t) b ^ (1<<(b->tmp[0]+4));
  struct block *buddy=(struct block*) tmp;
  printf("%d\n", b->tmp[0]);
  printf("%u is the buddy of %u\n",buddy,b);
  return buddy;
}

*/
struct block * getBuddy(struct block * p,int index){
  struct block * startblock = arena_to_block(block_to_arena(p));
  return (struct block *)((((uintptr_t) p - (uintptr_t) startblock)^(1<<(index+4))) + (uintptr_t) startblock);

}


void mergeBlocks(struct block *b,struct block* buddy,struct arena* a){
  struct block *newBuddy;
  while (buddy->tmp[0] == b->tmp[0] && buddy->tmp[1]==0){
    list_remove(&(buddy->free_elem));

    if (buddy < b)
          b = buddy;
    b->tmp[0] += 1;
    if (b->tmp[0] == 7) break;

        newBuddy =getBuddy(b,b->tmp[0]);
        buddy = newBuddy;
      }
      printf("here\n");
      if ( b->tmp[0] >= 7 ){
        a = block_to_arena(b);
        list_remove(&a->elem);
        palloc_free_page(a);
      }
      else{
        b->tmp[1] = 0;
        list_insert_ordered (&(descs[b->tmp[0]].free_list),&b->free_elem,&compare,(void *)NULL);
      }
}


void
free (void *p) 
{
  if (p != NULL)
    {
      p = (char *) p -  2;
      struct block *b = p;
      struct arena *a = block_to_arena (b);
 /*     
 #ifndef NDEBUG
//            //Clear the block to help detect use-after-free bugs. 
    memset (b, 0xcc, d->block_size);
 #endif*/
  
      lock_acquire (&lock);
      struct block * buddy = getBuddy(b,b->tmp[0]);

      mergeBlocks(b,buddy,a);

      lock_release (&lock);
    }    
}


/* Returns the arena that block B is inside. */
static struct arena *
block_to_arena (struct block *b)
{
  struct arena *a = pg_round_down (b);

  /* Check that the arena is valid. */
  ASSERT (a != NULL);
  ASSERT (a->magic == ARENA_MAGIC);
  ASSERT ((pg_ofs(b) - sizeof *a)%16==0);
  return a;
}

/* Returns the (IDX - 1)'th block within arena A. */
static struct block *
arena_to_block (struct arena *a)//, size_t idx) 
{
  ASSERT (a != NULL);
  ASSERT (a->magic == ARENA_MAGIC);
  return (struct block *) ((uint8_t *) a
                           + sizeof *a);
}


bool compare(const struct block * b1,const struct block * b2,void * aux){
  return (uintptr_t)b1 < (uintptr_t)b2;
}


void printMemory()
{
  int i,j=0;
  struct list_elem * ele;
  if (list_empty(&arenaList))
    printf("No free blocks in memory\n");
  else
    for (ele = list_begin(&arenaList); ele != list_end(&arenaList);ele=list_next(ele)){
      printf("Page no %d\n",j++);
      struct arena *a = list_entry(ele,struct arena,elem);
      printf("Page address = %u \n", a);
      for(i=0;i<7;i++)
      {
          struct list_elem *e;

          printf("Blocks of size %d :",1<<(i+4));
          // if (list_empty(&descs[i].free_list)) printf("list_empty %d",i );
          for (e = list_begin (&descs[i].free_list); e != list_end (&descs[i].free_list);e = list_next (e))
          {
            struct block *b = list_entry (e, struct block, free_elem);
            if (a == block_to_arena(b))
              printf("%u  ",b);
            
          }
          printf("\n");
      }

  }
}