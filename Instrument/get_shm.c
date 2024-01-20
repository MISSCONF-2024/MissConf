#include "../include/alloc.h"
#include "../include/debug.h"
#include "../include/types.h"

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <string.h>

#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/ipc.h>


// void get_shm() {
  
  
//   exit(0);
// }

int main() {
  char* wrapper_path = getenv("WRAPPER_PATH");
  if(!wrapper_path) {
    FATAL("Please set env WRAPPER_PATH");
  }
    
  FILE* fp; 
	int len;
  const char* fileName = strcat(wrapper_path, "/../storeFile/shm_id.txt");

  char shm_id_str[100];
	if ((fp = fopen(fileName, "r")) == NULL) {
    FATAL("Test Error : Fail to read shm_id.txt!\n"); 
  }
	while (fgets(shm_id_str, MAX_LINE, fp) != NULL) { 
		len = strlen(shm_id_str); 
		shm_id_str[len - 1] = '\0'; 
  }

  int shm_id = atoi(shm_id_str);
  printf("-------------Now attach shared memory and test-------------\n");
  u32* addr = (u32*)shmat(shm_id, NULL, 0);
  if(addr == (void *)-1) {
    FATAL("Error!  The shared memory doesn't exists.\n");
  }
  printf("Test:   Start to print addr[0].  Its value should not be NULL.\n");
  printf("Output: addr[0] is: %d;\n\n", addr[0]);

  for(int i = 1; i < 10; i++) {
    printf("Test:   Start to print addr[%d].  It stands for a config.\n", i);
    printf("Output: addr[%d] is: %d;\n", i, addr[i]);
  }
  // printf("Test:   Start to print addr[1].  It stands for a config.\n");
  // printf("Output: addr[1] is: %d;\n", addr[1]);
  printf("-----------------------Test Done!---------------------\n");
  shmdt(addr);
  return 0;
}
