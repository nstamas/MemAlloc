/******************************************************************************
 * FILENAME: mem.c
 * AUTHOR:   cherin@cs.wisc.edu <Cherin Joseph>
 * DATE:     20 Nov 2013
 * PROVIDES: Contains a set of library functions for memory allocation
 * MODIFIED BY:  Nick Stamas, section# 1
 * *****************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include "mem.h"

/* this structure serves as the header for each block */
typedef struct block_hd{
  /* The blocks are maintained as a linked list */
  /* The blocks are ordered in the increasing order of addresses */
  struct block_hd* next;

  /* size of the block is always a multiple of 4 */
  /* ie, last two bits are always zero - can be used to store other information*/
  /* LSB = 0 => free block */
  /* LSB = 1 => allocated/busy block */

  /* For free block, block size = size_status */
  /* For an allocated block, block size = size_status - 1 */

  /* The size of the block stored here is not the real size of the block */
  /* the size stored here = (size of block) - (size of header) */
  int size_status;

}block_header;

/* Global variable - This will always point to the first block */
/* ie, the block with the lowest address */
block_header* list_head = NULL;


/* Function used to Initialize the memory allocator */
/* Not intended to be called more than once by a program */
/* Argument - sizeOfRegion: Specifies the size of the chunk which needs to be allocated */
/* Returns 0 on success and -1 on failure */
int Mem_Init(int sizeOfRegion)
{
  int pagesize;
  int padsize;
  int fd;
  int alloc_size;
  void* space_ptr;
  static int allocated_once = 0;
  
  if(0 != allocated_once)
  {
    fprintf(stderr,"Error:mem.c: Mem_Init has allocated space during a previous call\n");
    return -1;
  }
  if(sizeOfRegion <= 0)
  {
    fprintf(stderr,"Error:mem.c: Requested block size is not positive\n");
    return -1;
  }

  /* Get the pagesize */
  pagesize = getpagesize();

  /* Calculate padsize as the padding required to round up sizeOfRegio to a multiple of pagesize */
  padsize = sizeOfRegion % pagesize;
  padsize = (pagesize - padsize) % pagesize;

  alloc_size = sizeOfRegion + padsize;

  /* Using mmap to allocate memory */
  fd = open("/dev/zero", O_RDWR);
  if(-1 == fd)
  {
    fprintf(stderr,"Error:mem.c: Cannot open /dev/zero\n");
    return -1;
  }
  space_ptr = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
  if (MAP_FAILED == space_ptr)
  {
    fprintf(stderr,"Error:mem.c: mmap cannot allocate space\n");
    allocated_once = 0;
    return -1;
  }
  
  allocated_once = 1;
  
  /* To begin with, there is only one big, free block */
  list_head = (block_header*)space_ptr;
  list_head->next = NULL;
  /* Remember that the 'size' stored in block size excludes the space for the header */
  list_head->size_status = alloc_size - (int)sizeof(block_header);
  
  return 0;
}


/* Function for allocating 'size' bytes. */
/* Returns address of allocated block on success */
/* Returns NULL on failure */
/* Here is what this function should accomplish */
/* - Check for sanity of size - Return NULL when appropriate */
/* - Round up size to a multiple of 4 */
/* - Traverse the list of blocks and allocate the first free block which can accommodate the requested size */
/* -- Also, when allocating a block - split it into two blocks when possible */
/* Tips: Be careful with pointer arithmetic */
void* Mem_Alloc(int size)
{
  //Check to see if allocated size is valid size
  if(size <= 0){
	return NULL;
  }
  
  //Make size a multiple of 4 if it is not
  int sizeBy4;
  if(size%4 > 0){
  	sizeBy4 = size + (4 - size%4);
  }else{sizeBy4 = size;}
  
  //Add the size of head to get total block size
  int sizeWithHead;
  sizeWithHead = sizeBy4 + (int)sizeof(block_header);

  //Assign pointer to head of block list
  block_header* ptr;
  ptr = list_head;
  
  //Check if pointer has a next field
  while(ptr->next != NULL){

	//Check if block is free
	if((ptr->size_status%2) == 0 ){
		
		//Check if free block is big enough to hold requested data size
		if(ptr->size_status >= sizeWithHead){
			
			//Check if block can be split into a new block
			if((ptr->size_status - sizeWithHead) >= ((int)sizeof(block_header) + 4)){
				//Create new block head pointer for newly split block. Update size
				//of allocated block and new split block. The update next fields.
				block_header* newBlock;
				newBlock = (ptr + (sizeWithHead) / (int)sizeof(block_header));
				newBlock->next = ptr->next;
				ptr->next = newBlock;
				newBlock->size_status = (ptr->size_status - sizeWithHead);
				ptr->size_status = sizeBy4;
				//Set allocated block to allocated status
				ptr->size_status += 1;
				return(ptr + 1);
			}else{
				//Block is perfect size for allocation or does not have enough
				//space to split into t a new block
				ptr->size_status += 1;
				return(ptr + 1);
			}

		}else{ptr = ptr->next;}
	
	}else{ptr = ptr->next;}
  }
  //Check if last block can fit our requested data size. If so check if block can be split.
  //Then update size and next fields correctly.
  if((ptr->size_status%2) == 0 ){
			  	
	if(ptr->size_status >= sizeWithHead){

		
			if((ptr->size_status - sizeWithHead) >= ((int)sizeof(block_header) + 4)){
				block_header* newBlock2;
				newBlock2 = (ptr + (sizeWithHead) / (int)sizeof(block_header));
				newBlock2->next = ptr->next;
				ptr->next = newBlock2;
				newBlock2->size_status = (ptr->size_status - sizeWithHead);
				ptr->size_status = sizeBy4;
				ptr->size_status += 1;
				return(ptr + 1);
			}else{

				ptr->size_status += 1;
				return(ptr + 1);
			 }

	}else{return NULL;}
	
  }else{return NULL;}

}

/* Function for freeing up a previously allocated block */
/* Argument - ptr: Address of the block to be freed up */
/* Returns 0 on success */
/* Returns -1 on failure */
/* Here is what this function should accomplish */
/* - Return -1 if ptr is NULL */
/* - Return -1 if ptr is not pointing to the first byte of a busy block */
/* - Mark the block as free */
/* - Coalesce if one or both of the immediate neighbours are free */
int Mem_Free(void *ptr)
{
  //Check for valid pointer
  if(ptr == NULL){
    return(-1);
  }

  //Assign new pointer to pointer we are provided.
  block_header* newPtr;
  newPtr = (block_header*)ptr;
  //Move bacl 8 bits to get to start of block header.
  newPtr = newPtr - 1;

  //Check if requested block has allocated status
  if((newPtr->size_status%2) != 1){
	return(-1);
  }

  //Set alloctaed block to free status
  newPtr->size_status += -1;

  //Check if front block is free. If so coalesce blocks.
  if(newPtr->next != NULL){
  	if((newPtr->next->size_status%2) == 0) {
		newPtr->size_status += newPtr->next->size_status + (int)sizeof(block_header);
		newPtr->next = newPtr->next->next;
  	}
  }
  
  //Check if newly freed block is the first block in the list.
  if(newPtr == list_head){
	return(0);
  }

  //If block is not first block in the list, check if block before it is free.
  block_header* temp;
  temp = list_head;
  //Previous pointer used to locate previous block
  block_header* prev;
  prev = NULL;
  
  //Variable used to indicate whether the proper block was found
  int found;
  found = 0;
  //Loop through list and find previous block
  while(temp != NULL && found == 0){
	if(temp == newPtr){
	   found = 1;
	}else{
                prev = temp;
		temp = temp->next;
	}
  }
 
  //Check if previous block is valid and if it is a free block. If block is free, coalesce.
  if(prev != NULL){
  	if(prev->size_status%2 == 0){
		prev->size_status = prev->size_status + prev->next->size_status + (int)sizeof(block_header);
		prev->next = prev->next->next;
	}
  }

  return(0);
}

/* Function to be used for debug */
/* Prints out a list of all the blocks along with the following information for each block */
/* No.      : Serial number of the block */
/* Status   : free/busy */
/* Begin    : Address of the first useful byte in the block */
/* End      : Address of the last byte in the block */
/* Size     : Size of the block (excluding the header) */
/* t_Size   : Size of the block (including the header) */
/* t_Begin  : Address of the first byte in the block (this is where the header starts) */
void Mem_Dump()
{
  int counter;
  block_header* current = NULL;
  char* t_Begin = NULL;
  char* Begin = NULL;
  int Size;
  int t_Size;
  char* End = NULL;
  int free_size;
  int busy_size;
  int total_size;
  char status[5];

  free_size = 0;
  busy_size = 0;
  total_size = 0;
  current = list_head;
  counter = 1;
  fprintf(stdout,"************************************Block list***********************************\n");
  fprintf(stdout,"No.\tStatus\tBegin\t\tEnd\t\tSize\tt_Size\tt_Begin\n");
  fprintf(stdout,"---------------------------------------------------------------------------------\n");
  while(NULL != current)
  {
    t_Begin = (char*)current;
    Begin = t_Begin + (int)sizeof(block_header);
    Size = current->size_status;
    strcpy(status,"Free");
    if(Size & 1) /*LSB = 1 => busy block*/
    {
      strcpy(status,"Busy");
      Size = Size - 1; /*Minus one for ignoring status in busy block*/
      t_Size = Size + (int)sizeof(block_header);
      busy_size = busy_size + t_Size;
    }
    else
    {
      t_Size = Size + (int)sizeof(block_header);
      free_size = free_size + t_Size;
    }
    End = Begin + Size;
    fprintf(stdout,"%d\t%s\t0x%08lx\t0x%08lx\t%d\t%d\t0x%08lx\n",counter,status,(unsigned long int)Begin,(unsigned long int)End,Size,t_Size,(unsigned long int)t_Begin);
    total_size = total_size + t_Size;
    current = current->next;
    counter = counter + 1;
  }
  fprintf(stdout,"---------------------------------------------------------------------------------\n");
  fprintf(stdout,"*********************************************************************************\n");

  fprintf(stdout,"Total busy size = %d\n",busy_size);
  fprintf(stdout,"Total free size = %d\n",free_size);
  fprintf(stdout,"Total size = %d\n",busy_size+free_size);
  fprintf(stdout,"*********************************************************************************\n");
  fflush(stdout);
  return;
}
