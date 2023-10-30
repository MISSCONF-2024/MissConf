
#include "../include/alloc.h"
#include "../include/debug.h"
#include "../include/types.h"

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include "string.h"

#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/types.h>

/* This is a somewhat ugly hack for the experimental 'trace-pc-guard' mode.
   Basically, we need to make sure that the forkserver is initialized after
   the LLVM-generated runtime initialization pass, not before. */

#ifdef USE_TRACE_PC
#  define CONST_PRIO 5
#else
#  define CONST_PRIO 0
#endif /* ^USE_TRACE_PC */


/* Globals needed by the injected instrumentation. The __afl_area_initial region
   is used for instrumentation output before __afl_map_shm() has a chance to run.
   It will end up as .comm, so it shouldn't be too wasteful. */

u32  __afl_area_initial[SHM_MAP_SIZE];
u32* __afl_area_ptr = __afl_area_initial;

__thread u32 __afl_prev_loc;


/* SHM setup. */
/* 
  This part of the code needs to be merged into the tool because the extern variable __afl_area_ptr is used in pass.so.cc.
  When constructing the IR statement, GlobalValue::ExternalLinkage is called
*/
void get_file(u8* buf) {
  // char* wrapper_path = getenv("WRAPPER_PATH");
  char* wrapper_path = "/root/clang-wrapper/tools";
  int return_code = system(". /etc/profile");
  if(return_code == -1) {
    SAYF("[+]Warning: enable WRAPPER_PATH failed!\n");
  }
  
    if(!wrapper_path) {
      FATAL("[+]Warning: Please set env WRAPPER_PATH!\n");
  }
  
  int i = 0, index = 0;
  for(; wrapper_path[i] != '\0'; i++) {
    if(wrapper_path[i] == '/') {
      index = i;
    }
  }

  char* temp_path = "/storeFile/shm_id.txt";
  char* fileName = (char*)malloc(index + 1 + strlen(temp_path));
  for(i = 0; i < index; i++){
    *(fileName + i) = *(wrapper_path + i);
  }

  for(i = 0; i < strlen(temp_path); i++) {
    *(fileName + index + i) = *(temp_path + i);
  }
  *(fileName + index + strlen(temp_path)) = '\0';

  char fileName_c[50];
  strcpy(fileName_c, fileName);
  free(fileName);
  fileName = NULL;
  
  FILE* fp; 
	int len;
	if ((fp = fopen(fileName_c, "r")) == NULL) {
    perror("Error: ");
    FATAL("llvm-shm.o : Fail to read shm_id.txt!\nfile path: %s\n", fileName_c); 
  }
	while (fgets(buf, MAX_LINE, fp) != NULL) { 
		len = strlen(buf); 
		buf[len - 1] = '\0'; 
  } 
  fclose(fp);
}

static void __afl_map_shm(void) {
  /* We just read a text file to get shared memory id. */
  
  u8* id_str = (u8*)malloc(sizeof(u8)*1024);
  get_file(id_str);

  if (id_str) {

    u32 shm_id = atoi(id_str);

    __afl_area_ptr = shmat(shm_id, NULL, 0);

    /* Whooooops. */

    // if (__afl_area_ptr == (void *)-1) _exit(1);

    /* Whoops, can't allocate to the share memory region. */
    if(__afl_area_ptr == (void *)-1) {
      FATAL("Whoops, can't allocate to the share memory region.\n");
      _exit(1);
    }

    /* Write something into the bitmap so that even with low AFL_INST_RATIO,
       our parent doesn't give up on us. */

    __afl_area_ptr[0] = 1;

  }
  free(id_str);

}


/* This one can be called from user code when deferred forkserver mode
    is enabled. */

void __afl_manual_init(void) {

  static u8 init_done;

  if (!init_done) {

    __afl_map_shm();
    init_done = 1;

  }

}


/* 
    * Proper initialization routine. 
    * __attribute__((constructor(x))) void func(void) will execute before main() function,
    * parameter x set the priority of this function, x is a int parameter.
*/

__attribute__((constructor(CONST_PRIO))) void __afl_auto_init(void) {

  __afl_manual_init();

}


/* Init callback. Populates instrumentation IDs. Note that we're using
   ID of 0 as a special value to indicate non-instrumented bits. That may
   still touch the bitmap, but in a fairly harmless way. */

void __sanitizer_cov_trace_pc_guard_init(uint32_t* start, uint32_t* stop) {

  u32 inst_ratio = 100;
  u8* x;

  if (start == stop || *start) return;

  x = getenv("AFL_INST_RATIO");
  if (x) inst_ratio = atoi(x);

  if (!inst_ratio || inst_ratio > 100) {
    fprintf(stderr, "[-] ERROR: Invalid AFL_INST_RATIO (must be 1-100).\n");
    abort();
  }

  /* Make sure that the first element in the range is always set - we use that
     to avoid duplicate calls (which can happen as an artifact of the underlying
     implementation in LLVM). */

  *(start++) = R(SHM_MAP_SIZE - 1) + 1;

  while (start < stop) {

    if (R(100) < inst_ratio) *start = R(SHM_MAP_SIZE - 1) + 1;
    else *start = 0;

    start++;

  }

}
