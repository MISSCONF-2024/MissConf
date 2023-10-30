#include "../include/types.h"
#include "../include/debug.h"
#include "../include/alloc.h"

#include <fstream>
#include <iostream>
#include <vector>
#include <queue>
#include <set>
#include <list>
#include <string>
#include <sstream>
#include <ctime>
#include <unistd.h>
#include <cstdlib>

#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/stat.h>


using namespace std;
int main(int argc, char** argv) {
    /***
    if(argc != 2) {
        FATAL("Just need one argument to specify FIFO file that stores shm_id!");
    }
    */

    char* wrapper_path = getenv("WRAPPER_PATH");
    if(!wrapper_path) {
        FATAL("Please set env WRAPPER_PATH");
    }
    
    string pathName = wrapper_path;
    pathName += "/../storeFile";
    struct stat bufferStat;
    if(stat(pathName.c_str(), &bufferStat) == -1) {
        // dir not exsits, need to construct
	string order = "mkdir " + pathName;
	system(order.c_str());
    }

    pathName += "/shm_ftok.txt";
    ofstream shmFile(pathName.c_str(), ios::out | ios::trunc);
    shmFile.close();

    key_t key = ftok(pathName.c_str(), 1);
    if(key == -1) {
        FATAL("Produce key_t fail!");
    }
    
    string fileName = wrapper_path;
    fileName +=  "/../storeFile/shm_id.txt";
    ofstream fp(fileName.c_str(), ios::out | ios::trunc);
    if(!fp) {
        FATAL("Open or create file %s fail!", fileName.c_str());
    }

    int shm_id = shmget(key, SHM_MAP_SIZE, IPC_CREAT | IPC_EXCL | 0600);
    if(shm_id < 0) {
        FATAL("Create shared memory fail!");
    }
    else {
        printf("Yes! We create the shared memory successfully!\n");
        cout << "\tThe SHM_ID of the memory is: " << shm_id <<", and its ftok ID is: " << key << endl;
    }
    fp << shm_id << endl;
    fp.close();

    return 0;
}
