#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <inttypes.h>
#include <assert.h>
#include <errno.h>
#include <string.h>

/* structure to represent the key, which in this case, is a string pointer */
typedef struct ptr_struct
{
  uint8_t *key;
  uint64_t len;   // 64-bit int here, to help keep alignment.
}
ptr_struct;

/* in-place iterative qsort, tuned to reduced instruction usage while maximizing cache usage */
void tuned_qsort(ptr_struct *data, const uint64_t N);
