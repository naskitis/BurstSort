/*******************************************************************************
 * // Begin statement                                                          *
 *                                                                             *
 * Author:        Dr. Nikolas Askitis                                          *
 * Email:         askitisn@gmail.com                                           *
 * Github.com:    https://github.com/naskitis                                  *
 *                                                                             *
 * Copyright @ 2016.  All rights reserved.                                     *
 *                                                                             *
 * Permission to use my software is granted provided that this statement       *
 * is retained.                                                                *
 *                                                                             *
 * My software is for non-commercial use only.                                 *
 *                                                                             *
 * If you want to share my software with others, please do so by               *
 * sharing a link to my repository on github.com.                              *
 *                                                                             *
 * If you would like to use any part of my software in a commercial or public  *
 * environment/product/service, please contact me first so that I may          *
 * give you written permission.                                                *
 *                                                                             *
 * This program is distributed without any warranty; without even the          *
 * implied warranty of merchantability or fitness for a particular purpose.    *
 *                                                                             *
 * Note:                                                                       *
 *                                                                             *
 * The copy-based burst sort algorithm is one of the fastest in-memory         *
 * sorting algorithms for variable-length strings, as detailed in the          *
 * the computing literature. Nonetheless, the original implementation of       *
 * the copy-based burst sort by its authors is not readily available. As       *
 * such, I have written my own implementation of the copy-based burst sort     *
 * in C, using my knowledge of cache-conscious string data structures. I am    *
 * not sure if my implementation is as fast or as memory-efficient as the      *
 * original used by the authors, but I am confident that it can serve as       *
 * a good baseline for your experiments :)                                     *
 *                                                                             *
 * My implementation of the copy-based burst sort algorithm is a little        *
 * different from how its explained in literature. First, the strings that     *
 * are stored in containers are length-encoded --- the original algorithm      *
 * simply stored null-terminated strings. Second, containers are not sized     *
 * to fit into the L2 cache of a typical CPU, as is the intention of the       *
 * burst sort algorithm. Instead, much like my implementation of the array     *
 * burst trie and HAT-trie, containers are burst once they store more than a   *
 * given number of strings.                                                    *
 *                                                                             *
 * (Usage: ./naskitis_copybased-burst_sort                                     *
 *                                [container-size] [number-of-files-to-insert] *
 *                                [file1] [file2] ... )                        *
 * Output: (printed to stderr)                                                 *
 * Copybased burst sort 520.94 446.67 12.60 28772169 64 ...                    *
 * [algo]               [virtual mem] [estimated mem] [time to build]          *
 *                      [num keys inserted] [container size] ...               *
 * // End statement                                                            *
 ******************************************************************************/

#include "include/common.h"
#include "sort_module.h"

#include <assert.h>

#define BUCKET_OVERHEAD (2 + sizeof(uint32_t))
#define STRING_EXHAUST_TRIE 31
#define STRING_EXHAUST_CONTAINER 2
#define CONSUMED 0
#define ALLOC_OVERHEAD 16

/* array of pointers used to sort a bucket */
ptr_struct *str_ptr;

/* stores the path of characters encountered as you traverse a trie */
char *path;

/* variables needed to maintain trie nodes */
char **trie_pack=NULL;
uint32_t trie_pack_idx=0;
uint32_t trie_counter=0;
uint32_t trie_pack_entry_capacity=32768;
uint32_t trie_pack_capacity=256;
uint32_t total_trie_pack_memory=0;
char *trie_buffer;
char *current_bucket;
char *root_trie;

uint64_t BUCKET_SIZE_LIM=35;
uint64_t inserted=0;
uint64_t searched=0;
uint64_t depth=0;
uint64_t num_buckets=0;
uint64_t num_tries=0;
uint64_t trie_buffer_capacity = 65536;
uint64_t trie_buffer_size = 0;
uint64_t bucket_mem=0;
uint64_t max_trie_depth=0;
uint64_t depth_accumulator=0;
uint64_t mtf_counter=0;

void destroy();
void split_container(char *, char **);
void burst_container(char *, char, char **);
void resize_container(char **, uint32_t, uint32_t);
	
uint32_t add_to_bucket_no_search(char *bucket,  
                     char path, 
		     char *query_start, 
		     char **c_trie);
		     
uint32_t add_to_bucket_no_search_with_len(char *bucket,  
                     char path, 
		     char *query_start, 
		     char **c_trie, int len);

/* resize a container, using the techniques I developed for the array hash table */
void resize_container(char **bucket, uint32_t array_offset, uint32_t required_increase)
{
    #ifdef EXACT_FIT

    char *tmp = malloc(array_offset + required_increase + BUCKET_OVERHEAD );
    if(tmp == NULL) fatal (MEMORY_EXHAUSTED);

    /* copy the existing array into the new one */
    if(array_offset==0)  
    {  
      memcpy(tmp, *bucket, array_offset+BUCKET_OVERHEAD);
    } 
    else
    {
      /* once extra byte to transfer the end-of-container flag */
      memcpy(tmp, *bucket, array_offset+1+BUCKET_OVERHEAD);  
    }

    /* free the old array and assign the container pointer to the new array */ 
    free( *bucket );
    *bucket = tmp;
 
    /* else grow the array in blocks or pages */
    #else 

    if(array_offset==0)
    {
      /* otherwise, grow the array with paging */
      /* if the required space is less than 32 bytes, than allocate a 32 byte block */
      if(required_increase + BUCKET_OVERHEAD <= _32_BYTES)
      {
        char *tmp = malloc(_32_BYTES);
        if(tmp == NULL) fatal (MEMORY_EXHAUSTED);

        memcpy(tmp, *bucket, array_offset+BUCKET_OVERHEAD);

        /* free the old array and assign the container pointer to the new array */ 
        free( *bucket );
        *bucket = tmp; 
      }
      /* otherwise, allocate as many 64-byte blocks as required */
      else
      {
        uint32_t number_of_blocks = ((int)( (required_increase - 1 + BUCKET_OVERHEAD) >> 6)+1);   

        char *tmp = malloc(number_of_blocks << 6);
        if(tmp == NULL) fatal (MEMORY_EXHAUSTED);

        memcpy(tmp, *bucket, array_offset+BUCKET_OVERHEAD);

        /* free the old array and assign the container pointer to the new array */ 
        free( *bucket );
        *bucket = tmp; 
      }

      return;
    }

    uint32_t old_array_size = array_offset + 1 + BUCKET_OVERHEAD;
    uint32_t new_array_size = (array_offset + required_increase + BUCKET_OVERHEAD);
    
    /* if the new array size can fit within the previously allocated 32-byte block, 
     * then no memory needs to be allocated.
     */
    if ( old_array_size <= _32_BYTES  &&  new_array_size <= _32_BYTES )
    {
      return;
    }
    /* if the new array size can fit within a 64-byte block, then allocate only a
     * single 64-byte block.
     */
    else if ( old_array_size <= _32_BYTES  &&  new_array_size <= _64_BYTES)
    {  
      char *tmp = malloc(_64_BYTES);
      if(tmp == NULL) fatal (MEMORY_EXHAUSTED);
      
      /* copy the old array into the new */
      memcpy(tmp, *bucket, old_array_size);
      
      /* delete the old array */ 
      free( *bucket );

      /* assign the container pointer to the new array */
      *bucket = tmp;

      return;
    }
    /* if the new array size can fit within a 64-byte block, then return */
    else if  (old_array_size <= _64_BYTES && new_array_size <= _64_BYTES )
    {
      return;
    }
    /* resize the current array by as many 64-byte blocks as required */
    else
    {
      uint32_t number_of_blocks = ((int)( (old_array_size-1) >> 6) + 1);
      uint32_t number_of_new_blocks = ((int)( (new_array_size-1) >> 6) + 1);

      if(number_of_new_blocks > number_of_blocks)
      {
        /* allocate as many blocks as required */
        char *tmp = malloc(number_of_new_blocks << 6);
        if (tmp==NULL) fatal(MEMORY_EXHAUSTED);
        
        /* copy the old array, a word at a time, into a new array */
        node_cpy( (uint32_t *) tmp, (uint32_t *) *bucket, number_of_blocks<<6); 
        
        /* free the old array */
        free( *bucket );
        
        /* assign the container pointer to the new array */
        *bucket = tmp;
      } 
    } 

  #endif 
}	     
    
/* need to implement if it runs of out packs. See source of HAT-trie for more details. */
char * new_trie()
{
  if(trie_counter == trie_pack_entry_capacity)
  {
    trie_pack_idx++;
    assert(trie_pack_idx<128);

    *(trie_pack+trie_pack_idx) = calloc(trie_pack_entry_capacity*TRIE_SIZE, sizeof(char));
    trie_counter=0;
  }

  return *(trie_pack + trie_pack_idx) + (trie_counter++ * TRIE_SIZE);
}


/* take a pointer and return 1 if it points to a trie node.  This can
 * be determined by checking whether the address lies within the blocks
 * of memory used to store the trie nodes 
 */
int is_it_a_trie(char *x)
{
  register int idx=0;
  for(; idx <= trie_pack_idx; idx++)
  { 
     if ( x >= *(trie_pack+idx) && x <= (*(trie_pack+idx)+(TRIE_SIZE * (trie_pack_entry_capacity-1) )) ) 
       return 1;
  } 

  return 0;
}

/* initialize the burst trie structure */
void init()
{
  char **c_trie=NULL;
  int i=0;
  
  /* allocate the array of pointers that will be used to point to the
   * blocks of memory that house the trie nodes.
   */
  trie_pack = (char **) calloc (trie_pack_capacity, sizeof(char *));
  trie_pack_idx=0;
  trie_counter=0;

  /* assign the first pointer in the trie_pack array to block of memory */ 
  *(trie_pack+trie_pack_idx) = calloc(trie_pack_entry_capacity*TRIE_SIZE, sizeof(char));
  
  /* allocate a new trie node and assign it as the root trie node */
  root_trie=new_trie();
  c_trie = (char **)root_trie;

  /* make sure its pointers are null */
  for(i=0; i<128; i++) *(c_trie+i)=NULL; 

  /* make sure you clear the string-exhaust flag in the trie node */
  *(c_trie+STRING_EXHAUST_TRIE)=0;
}

/* add a string to a container, using the techniques I developed for the HAT-trie.
 * This method simply appends a length-encoded string to the end of a bucket.
 */
uint32_t add_to_bucket_no_search(char *bucket,  
         char path, 
		     char *query_start, 
		     char **c_trie)
{
  char *array, *array_start, *query;
  char *tmp=*(c_trie+path);
    
  char *consumed=0;
  uint32_t array_offset;
  uint32_t register len;
  uint32_t num=0;

  array = (char *)(bucket+BUCKET_OVERHEAD);
  consumed = (char *)(bucket+CONSUMED);
  
  array_start=array;
  query = query_start;
 
  /* set a flag to indicate that the bucket now stores a string */
  if(*consumed == 0) { *consumed = 1; goto insert; }
  
  /* scan the container until you reach the null (end-of-bucket) character */
  while( *array != '\0') 
  {
  
    if( ( len = (unsigned int) *array ) >= 128 )
    {
      len = (unsigned int) ( ( *array & 0x7f ) << 8 ) |  (unsigned int) ( *(++array) & 0xff );
    }
    array = (array+1) + len;
    ++num;
  }

  insert:

  /* get the length of the string to insert */
  for(; *query != '\0'; query++);
   
  len = query - query_start;

  /* get the size of the array */
  array_offset = array-array_start;

  /* resize the array to fit the new string */
  resize_container((char **)(c_trie+path), array_offset, ( len < 128 ) ? len+2 : len+3);
 
  /* reinitialize the array pointers, the point to the end of the array */
  array = (char *)( *(c_trie+path) + BUCKET_OVERHEAD);
  array_start=array;  
  array += array_offset;

  /* if the length of the string is less than 128 characters, then only a single byte is
   * needed to store its length
   */
  if( len < 128 )
  {
    *array = (char) len;

  }  
  /* if the length of the string is greater than 128 characters, then two bytes are required to
   * store the string 
   */
  else 
  {
    *array     = (char) ( len >> 8) | 0x80;
    *(++array) = (char) ( len ) & 0xff; 

  }
  array++;

  /* copy the string into the array */ 
  while( *query_start != '\0')
  {
    *array++ = *query_start++;
  }

  /* make sure the array is null terminated */
  *array='\0';
  ++num;

  return num;    
}

/* add a string with its length to a container, using the techniques I developed for the HAT-trie.
 * This method simply appends a length-encoded string to the end of a bucket.
 */
uint32_t add_to_bucket_no_search_with_len(char *bucket,  
         char path, 
		     char *query_start, 
		     char **c_trie, int query_len)
{
  char *array, *array_start;
  char *tmp=*(c_trie+path);
  
  uint32_t len;
  char *consumed=0;
  uint32_t array_offset;

  array    = (char *)(bucket+BUCKET_OVERHEAD);
  consumed = (char *)(bucket+CONSUMED);
  
  array_start=array;

  /* set a flag to indicate that the bucket now stores a string */
  if(*consumed == 0) { *consumed = 1; goto insert; }
   
  /* scan the container until you reach the null (end-of-bucket) character */
  while( *array != '\0')
  {
    if( ( len = (unsigned int) *array ) >= 128 )
    {
      len = (unsigned int) ( ( *array & 0x7f ) << 8 ) | (unsigned int) ( *(++array) & 0xff );
    }
    array = (array+1) + len;
  }
  
  insert:

  /* get the length of the string to insert */
  len = query_len;
  
  /* get the size of the array */
  array_offset = array-array_start;
   
  /* resize the array to fit the new string */
  resize_container((char **)(c_trie+path), array_offset, ( len < 128 ) ? len+2 : len+3);
   
  /* reinitialize the array pointers, the point to the end of the array */
  array = (char *)( *(c_trie+path) + BUCKET_OVERHEAD);
  array_start=array;  
  array += array_offset;
  
  /* if the length of the string is less than 128 characters, then only a single byte is
   * needed to store its length
   */
  if( len < 128 )
  {
    *array = (char) len;
  }
  /* if the length of the string is greater than 128 characters, then two bytes are required to
   * store the string 
   */
  else 
  {
    *array     = (char) ( len >> 8) | 0x80;
    *(++array) = (char) ( len ) & 0xff; 
  }
  array++;
   
  /* copy the string into the array */
  while( len!=0 )
  {
    *array++ = *query_start++;
    len--;
  }

  /* make sure the array is null terminated */
  *array = '\0';
  return 1;    
}

/* allocate a new container */
int new_container(char **c_trie, char path, char *word)
{
  char *x;
  
  /* allocate space for the container */
  x=malloc(BUCKET_OVERHEAD);
  if (x==NULL) fatal (MEMORY_EXHAUSTED);

  /* make sure the string-exhaust flag is cleared, and the
   * bytes used to store the pointer to the head of the list is
   * null.
   */
  *(x+CONSUMED)=0;
  *(uint32_t *)(x+STRING_EXHAUST_CONTAINER)=0;

   /* assign the parent pointer to the new container */
  *(c_trie + path)=x;
  
  if( *word == '\0')
  {
    *(uint32_t *)(x+STRING_EXHAUST_CONTAINER)=1;
  }
  else
  {
    add_to_bucket_no_search(x, path, word, c_trie); 
  }
  return 1;
}

int search(char *word)
{
  return 0;
}

/* insert a string into the copy based burst sort algorithm (i.e., burst trie) */
int insert(char *word)
{
  char **c_trie=  (char **) root_trie;
  char *x; 
  int r=0;

 /* grab the leading character from the query string */
  while( *word != '\0')
  {
    /* if the pointer that maps to the leading character is null,
     * then create a new container to house the string, to complete
     * the insertion process
     */
    if ( (x = *(c_trie +  *word)) == NULL) 
      return new_container(c_trie, *word, word+1); 
         
    /* check whether the pointer that maps to the leading character 
     * leads to a trie node or to a container
     */
    if( is_it_a_trie(x) ) 
    {
       c_trie = (char **)x;
    }
    else
    {
      /* consume the lead character */
      word++;
      
      /* if the query string has been consumed entirely, then set
       * the string-exhaust flag within the current node to complete
       * the insertion 
       */
      if( *word == '\0') 
      { 
        *(uint32_t *)(x+STRING_EXHAUST_CONTAINER) = *(uint32_t *)(x+STRING_EXHAUST_CONTAINER) + 1;
        return 1;
      } 

      /* otherwise, a container is acquired.  Attempt to add the string
       * to the container.  If the function returns a non-null value,
       * then the insertion was a success. In this case, check to see
       * whether the container needs to be burst 
       */
      if( (r=add_to_bucket_no_search(x, *(word-1), word, c_trie)) )
      {
        x = *(c_trie +  *(word-1));

	 /* if the number of entries in the current container exceed the
         * container limit, then the container needs to be burst 
         */
        if( r > BUCKET_SIZE_LIM ) 
        {
	  burst_container(x, *(word-1), c_trie);
        }

        return 1;
      }
      return 0;
    }

    /* consume the current character and continue with the traversal */
    word++;
  }

  /* if the string was consumed prior to reaching a container, then 
   * set the string-exhaust flag within the current trie node to 
   * complete the insertion. 
   */
  *(uint64_t *)(c_trie+STRING_EXHAUST_TRIE) = *(uint64_t *)(c_trie+STRING_EXHAUST_TRIE) + 1;
  return 1;
}

int main(int argc, char **argv)
{
   char *to_insert=NULL, *to_search=NULL;
   int num_files=0;
   int i=0;
   int j=0;
   double mem=0;
   double insert_real_time=0.0, search_real_time=0.0;
 
   /* get the container limit */
   BUCKET_SIZE_LIM = atoi(argv[1]);

   /* make sure the user supplied a valid bucket size */
   if (BUCKET_SIZE_LIM < 64 || BUCKET_SIZE_LIM > 512)
   {
     puts("Keep bucket size between 128 and 256 strings, inclusive");
     exit(1);
   }

   /* since the bursting limit is actually a soft-limit, we need
    * to make room for some extra ptrs.
    */
   str_ptr = (ptr_struct *)calloc(BUCKET_SIZE_LIM*64, sizeof(ptr_struct *));
   path = calloc(524288, sizeof(char));

   /* get the number of files to insert */ 
   num_files = atoi(argv[2]);
   
   init();

   /* insert the files in sequence into the standard-chain burst trie and
    * accumulate the time required
    */
   for(i=0, j=3; i<num_files; i++, j++)
   {
     to_insert=argv[j];     
     insert_real_time+=perform_insertion(to_insert);
   }

   uint64_t vsize=0;
   {
     pid_t mypid;
     FILE * statf;
     char fname[1024];
     uint64_t ret;
     uint64_t pid; 
     char commbuf[1024];
     char state;
     uint64_t ppid, pgrp, session, ttyd, tpgid;
     uint64_t flags, minflt, cminflt, majflt, cmajflt;
     uint64_t utime, stime, cutime, cstime, counter, priority;
     uint64_t timeout, itrealvalue;
     uint64_t starttime;
     uint64_t rss, rlim, startcode, endcode, startstack, kstkesp, ksteip;
     uint64_t signal, blocked, sigignore, sigcatch;
     uint64_t wchan;
     uint64_t size, resident, share, trs, drs, lrs, dt;
    
     mypid = getpid();
     snprintf(fname, 1024, "/proc/%u/stat", mypid);
     statf = fopen(fname, "r");
     ret = fscanf(statf, "%lu %s %c %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu "
       "%lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu",
       &pid, commbuf, &state, &ppid, &pgrp, &session, &ttyd, &tpgid,
       &flags, &minflt, &cminflt, &majflt, &cmajflt, &utime, &stime,
       &cutime, &cstime, &counter, &priority, &timeout, &itrealvalue,
       &starttime, &vsize, &rss, &rlim, &startcode, &endcode, &startstack,
       &kstkesp, &ksteip, &signal, &blocked, &sigignore, &sigcatch,
       &wchan);
      
     if (ret != 35) {
        fprintf(stderr, "Failed to read all 35 fields, only %d decoded\n",
          ret);
     }
     fclose(statf);
   }

   destroy();
   
   mem=((total_trie_pack_memory/(double)TO_MB) + ((double)bucket_mem/TO_MB));
   	
   fprintf(stderr, "Copybased burst sort %.2f %.2f %.2f %d %d --- A version of the burst-sort algorithm "
                   "implemented by Dr. Nikolas Askitis, Copyright @ 2016, askitisn@gmail.com ", vsize / (double) TO_MB, 
          mem, insert_real_time, get_inserted(), BUCKET_SIZE_LIM);
  
#ifdef PAGING
   fprintf(stderr, "%s\n", "Paging ");
#endif
 
#ifdef EXACT_FIT
   fprintf(stderr, "%s\n", "Exact-fit ");
#endif

   free(str_ptr);
   free(path);
   return 0; 
}

void burst_container(char *bucket, char path, char **c_trie)
{
    char *n_trie;

    /* allocate a new trie node as a parent */
    n_trie = new_trie();
    *(c_trie+path)=n_trie;
     
    c_trie = (char **) n_trie;  
    
    /* make sure you transfer the string-exhaust flag from the old container to the new trie node */
    uint64_t tmp=0; /* redundant step to ensure memory is zeroed */
    tmp =  (uint64_t) *(uint32_t *)(bucket+STRING_EXHAUST_CONTAINER);

    *(uint64_t *)(c_trie+STRING_EXHAUST_TRIE) = tmp;
    
    /* reset the string exhaust flag in the container */
    *(uint32_t *)(bucket+STRING_EXHAUST_CONTAINER)=0;

    /* split the container, passing the reference to the new trie node into the function */
    split_container(bucket, c_trie);
}

void split_container(char *bucket, char **c_trie)
{
  char *array = (char *)(bucket+BUCKET_OVERHEAD), *word_start;
  char *x;
  uint32_t len;

  /* scan the container until you reach the end-of-container (null) flag */
  while(*array != '\0')
  {
    /* get the length of the current string in the container */
    if( (len = (unsigned int) *array ) >= 128)
    {
      len = (unsigned int) ( ( *array & 0x7f ) << 8 ) | (unsigned int)  ( *(++array) & 0xff );
    }
  
    /* point to the first letter of the current string */
    array++;
    word_start = array;
   
    /* use the first letter to acquire a pointer in the parent trie */
    x = *(c_trie + *array);

    /* if the parent trie node pointer is null, then create a new container */  
    if (x == NULL)
    {
       /* allocate space for the container */
       x=malloc(BUCKET_OVERHEAD);
       if(x==NULL) fatal(MEMORY_EXHAUSTED);

       /* makes sure the string-exhaust and consumed flags are cleared and
        * assign the container to the parent trie
        */
       *(x+CONSUMED)=0;
       *(uint32_t *)(x+STRING_EXHAUST_CONTAINER)=0;
       *(c_trie + *array)=x;
    }   
    
    /* if after consuming the first character in the current string, you consume
     * the string, then set the string-exhaust flag in the current container
     */
    if( (len-1)==0 ) 
    {
      *(uint32_t *)(x+STRING_EXHAUST_CONTAINER) = *(uint32_t *)(x+STRING_EXHAUST_CONTAINER) + 1;
    }
    else
    {
      add_to_bucket_no_search_with_len(x, *array, array+1, c_trie, len-1); 
    }
    
    array = word_start  +  len;
  }
 
  /* you don't need the original bucket anymore */
  free(bucket);
}

/* run an in-order traversal of the burst trie to print out the strings
 * in ASCII-7 order, and also to accumulate the amount of memory 
 * allocated and to free the space allocated
 */
void in_order(char **c_trie, int local_depth, char *path)
{
  unsigned int i=0,j=0;
  char *x;
  
  if(local_depth > max_trie_depth)  max_trie_depth=local_depth;
  num_tries++;

  /* get the number of strings consumed by this trie */
  uint64_t num_consumed_trie = *(uint64_t *)(c_trie+STRING_EXHAUST_TRIE);

  for(j=0; j<num_consumed_trie; ++j)
  {
     printf("%s\n", path);         
  } 
  
  /* scan the trie node from left to right */
  for(i=MIN_RANGE; i<MAX_RANGE; i++)
  { 
    if ( (x = *(c_trie + i)) == NULL) 
    {
      continue;
    }

    path[local_depth-1]=(char)i;
    path[local_depth]='\0';
      
    if( is_it_a_trie(x) ) 
    {
      in_order( (char **)x, local_depth+1, path);
    }
    else
    {   
      char *x_start = x;
      char *tmp_str;
      unsigned int j=0;
      unsigned int k=0;
      unsigned int len=0;
      unsigned int num=0;
      unsigned int num_consumed_bucket=0;
      char *consumed=0;

      consumed = (char *)(x+CONSUMED);
      num_consumed_bucket=*(uint32_t *)(x+STRING_EXHAUST_CONTAINER);
      x=(char *)(x+BUCKET_OVERHEAD);

      for(j=0; j<num_consumed_bucket; ++j)
      { 
        printf("%s\n", path);         
      } 
 
      if(*consumed==1)
      {
         /* assign each string in the bucket to a pointer */
         while( *x != '\0')
         {
            if( ( len = (unsigned int) *x ) >= 128 )
            {
               len = (unsigned int) ( ( *x & 0x7f ) << 8 ) | (unsigned int) ( *(++x) & 0xff );
	    }
            ++x;      
            str_ptr[num].key=x; 
            str_ptr[num++].len=len;
            x=x+len;
         }

         /* sort the set of string pointers */
         tuned_qsort(str_ptr, num);

         /* iterate through the set of sorted string pointers to print out the strings */
         for(j=0; j<num; ++j)
         {
           tmp_str=str_ptr[j].key;
           len=str_ptr[j].len;

           /* we need to reconstruct the string before we print it, by storing
            * the path as the prefix. 
            */
           for(k=0; k<len; ++k)
           {
             *(path+local_depth+k)=*tmp_str;
             ++tmp_str;
           }
           *(path+local_depth+k)='\0';
           printf("%s\n", path);
         }
      }

#ifdef EXACT_FIT
      bucket_mem += ((x-x_start)+1); 
#else
      int temp= ((x-x_start)+1);

      if(temp<=_32_BYTES)
      {
        temp=_32_BYTES;
      }
      else 
      {
        if(temp <= _64_BYTES) 
        {
          temp = _64_BYTES;
        }
        else 
        {
          /* round up to the nearest 64-byte block */
          temp +=  _64_BYTES-(temp & (_64_BYTES -1 )); 
        }

        bucket_mem += temp; 
      }
#endif
      bucket_mem += ALLOC_OVERHEAD;
      num_buckets++;

      free(x_start);
      depth_accumulator+=local_depth;
     }
   }
}

/* free the memory allocated by the burst trie, including the trie nodes */
void destroy()
{
  int i=0;
  in_order((char **)root_trie, 1, path); 
  
  for(i=0; i<=trie_pack_idx; i++)  
  {
    total_trie_pack_memory += (((trie_pack_entry_capacity*TRIE_SIZE) + sizeof(char))+ALLOC_OVERHEAD);
    free ( *(trie_pack + i ) );
  }
  free(trie_pack);
}
