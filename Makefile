PROJ_ROOT = $(shell pwd)
SRC_PATH = ${PROJ_ROOT}/Instrument
TOOLS_PATH = ${PROJ_ROOT}/Instrument/ClangWrapper
TEST_PATH = ${PROJ_ROOT}/test

target1 = ${TOOLS_PATH}/llvm-shm.o
target2 = ${TOOLS_PATH}/instrument_pass.so
target3 = ${TOOLS_PATH}/clang-wrapper
target4 = ${TOOLS_PATH}/shm_init
target5 = ${TEST_PATH}/test
target6 = ${TOOLS_PATH}/shm_clear
target7 = ${TEST_PATH}/count_bitmap
target8 = ${TOOLS_PATH}/refresh_bitmap

all: ${target1} ${target2} ${target3} ${target4} ${target5} ${target6} ${target7} ${target8}

${target1}: ${SRC_PATH}/llvm-shm.c
	@echo "[+] Build llvm-shm.o ."
	clang -O3 -funroll-loops -Wall -D_FORTIFY_SOURCE=2 -g -Wno-pointer-sign  -fPIC -c ${SRC_PATH}/llvm-shm.c -o ${target1}

${target2}: ${SRC_PATH}/instrument_pass.so.cc
	@echo "[+] Build instrument_pass.so."
	clang++ `llvm-config --cxxflags` -Wl,-znodelete -fno-rtti -fpic -O3 -funroll-loops -Wall -D_FORTIFY_SOURCE=2 -g -Wno-pointer-sign  -Wno-variadic-macros -shared ${SRC_PATH}/instrument_pass.so.cc -o ${target2} `llvm-config --ldflags`


${target3}: ${SRC_PATH}/clang-wrapper.c
	@echo "[+] Compile executable file src/clang-wrapper."
	clang -O3 -funroll-loops -Wall -D_FORTIFY_SOURCE=2 -g -Wno-pointer-sign -DWRAPPER_PATH=\"/root/clang-wrapper\" -DBIN_PATH=\"/usr/local/bin\"   ${SRC_PATH}/clang-wrapper.c -o ${target3}
	ln -sf ${target3} ${TOOLS_PATH}/clang-wrapper++
	
${target4}: ${SRC_PATH}/set_shm_region.cc
	@echo "[+] Compile executable file src/shm_init."
	clang++ -g ${SRC_PATH}/set_shm_region.cc -o ${target4}

${target5}: ${SRC_PATH}/get_shm.c
	@echo "[+] Compile executable file test/test."
	mkdir ${PROJ_ROOT}/test/
	clang -g ${SRC_PATH}/get_shm.c -o ${target5}

${target6}: ${SRC_PATH}/clear_shm_region.cc
	@echo "[+] Compile executable file src/shm_init."
	clang++ -g ${SRC_PATH}/clear_shm_region.cc -o ${target6}

${target7}: ${SRC_PATH}/count.cc
	@echo "[+] Compile executable file test/count_bitmap."
	clang++ -g ${SRC_PATH}/count.cc -o ${target7}

${target8}: ${SRC_PATH}/refresh_bitmap.cc
	@echo "[+] Compile executable file src/refresh_bitmap."
	clang++ -g ${SRC_PATH}/refresh_bitmap.cc -o ${target8}

clean:
	-rm -rf ${target1} ${target2} ${target3} ${target4} ${target5} ${TOOLS_PATH}/clang-wrapper++ ${PROJ_ROOT}/test/ ${target6} ${target8}
	-rm ${PROJ_ROOT}/storeFile/*