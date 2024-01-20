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
  u32 bef[12] = {0};
  u32 aft[12] = {0};
  for(int i = 0; i < 60; i++) {
    for(int j = 0; j < 12; j++) {
      bef[j] = addr[j];
    }

    sleep(15);
    for(int j = 0; j < 12; j++) {
      aft[j] = addr[j];
    }

    for(int j = 0; j < 12; j++) {
      if(aft[j] < bef[j]) {
        printf("During %ds ~ %ds, addr[%d] at %ds is %d, addr[%d] at %ds is %d, %d - %d = %d\n", i*10, (i+1)*10, j, i*10, bef[j], j, (i+1)*10, aft[j], aft[j], bef[j], aft[j] + 256 - bef[j]);
      }
      else {
        printf("During %ds ~ %ds, addr[%d] at %ds is %d, addr[%d] at %ds is %d, %d - %d = %d\n", i*10, (i+1)*10, j, i*10, bef[j], j, (i+1)*10, aft[j], aft[j], bef[j], aft[j] - bef[j]);
      }
    }
    printf("\n\n");
  }

  shmdt(addr);
  return 0;
}
