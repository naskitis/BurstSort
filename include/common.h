/*******************************************************************************
 * // Begin statement                                                          *
 *                                                                             *
 * Permission to use this software is granted provided that this statement     *
 * is retained.                                                                *
 *                                                                             *
 * This software is for NON-COMMERCIAL use only!                               *
 *                                                                             *
 * If you would like to use any part of my software in a commercial or public  *
 * environment/product/service, let me know (askitisn@gmail.com) before        *
 * you use it (this statement will also need to be retained along with other   *
 * details), so I can grant you permission.                                    *
 *                                                                             *
 * Please DO NOT distribute my software (in any manner), in part or in whole.  *
 * Thank you.                                                                  *
 *                                                                             *
 * Developer: Dr. Nikolas Askitis                                              *
 * Website:   http://www.naskitis.com                                          *
 * Email:     askitisn@gmail.com                                               *
 * Please show your support by visiting: www.naskitis.com                      *
 *                                                                             *
 * Copyright @ 2010.  All rights reserved.                                     *
 * This program is distributed without any warranty; without even the          *
 * implied warranty of merchantability or fitness for a particular purpose.    *
 *                                                                             *
 *                                                                             *
 * Compiler version used: gcc (Ubuntu/Linaro 4.4.4-14ubuntu5) 4.4.5            *
 * O/S used: Linux ubuntu 2.6.35-22-generic #33-Ubuntu SMP Sun Sep 19 20:32:27 *
 * UTC 2010 x86_64 GNU/Linux                                                   *
 *                                                                             *
 * Developed and implemented by Dr. Nikolas Askitis, April 2004-2009 for       *
 * Linux-based platforms.                                                      *
 *                                                                             *
 * // End statement                                                            *
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <inttypes.h>
#include <assert.h>

#define MEMORY_EXHAUSTED   "Out of memory"
#define BAD_INPUT          "Can not open or read file"
#define TO_MB 1000000
#define CACHE_LINE_SIZE 128

typedef struct timeval timer;

#define false 0
#define true 1
#define MIN_RANGE (char)32
#define MAX_RANGE (char)126
#define TRIE_SIZE 1024

#define SKIPPING /* dont change */
#define MASK     /* best not to turn off mask */


#define _32_BYTES 32
#define _64_BYTES 64

double perform_insertion(char *to_insert);
double perform_search(char *to_search);
void fatal(char *str); 
int32_t scmp(const char  *s1, const char  *s2);
int32_t sncmp(const char *s1, const char *s2, uint64_t, uint64_t);
int32_t get_inserted();
int32_t get_found();
void set_terminator(char *buffer, int length);
int slen(char *word);
void node_cpy(uint32_t *dest, uint32_t *src, uint32_t bytes);


