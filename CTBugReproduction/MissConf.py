import subprocess
import os
import sys
import time
import numpy as np
from ctypes import *
from functools import reduce

SHM_MAP_SIZE = 1024*64
configVec = []
configVecLen = 0
bitmap = []

try:
    rt = CDLL('librt.so')
except:
    tr = CDLL('librt.so.1')

shmat = rt.shmat
shmget = rt.shmget

shmget.argtypes = [c_int, c_size_t, c_int]
shmget.restype = c_int
shmat.argtypes = [c_int, POINTER(c_void_p), c_int]
shmat.restype = c_void_p

WRAPPER_PATH = os.getenv("WRAPPER_PATH")
if WRAPPER_PATH is None:
    print("[+]ERROR: Please set WRAPPER_PATH")
    sys.exit(0)

idFile = str(WRAPPER_PATH) + "/../storeFile/shm_id.txt"
with open(idFile, "r") as f:
    shm_id = f.readline()

if int(shm_id) < 0:
    print("Attach share memory failed!\n")
    sys.exit(0)

addr = shmat(int(shm_id), None, 0)


class diffFuzz:

    # Although we define variables name such as "bit%", the shared map actually store unsigned int value
    def __init__(self, _path, _keyPath, _exe_path):
        self.path         = _path       # test-cases directory
        self.keyPath      = _keyPath    # config key-value path
        self.exe_path     = _exe_path   # obviously the executable file path or shell order
        self.fileList     = []          # a list whose element is a test-case text
        self.configDict   = {}          # see function below : getImpactConfigs()
        self.configKeyVal = {}          # see function below : getConfigKeyVal()
        self.configValue  = {}          # every element of this list contains all possible a config could be
        self.bitMapGap    = []          # see function below : countBackServiceBitmap()
        self.commonSubset = []          # common subset of influnced configs
        self.configCount  = {}          # see function below : setPriority()


    """
        Get all test cases and store them in the list variable "fileList".
        Every element in fileList is a string that contains test case SQL.

        For example, here is a test case:

            create database d1;
            use d1;
            drop d1;
            # end of case

        It will be transformed to string "create database d1;use d1;drop d1;"
        and stored in the list "fileList".
    """
    def getFileList(self):
        fileName = os.listdir(self.path)  # Get the names of all files in the folder.
        fileName.sort()

        # Traverse a folder
        for file in fileName:
            filePath = self.path + "/" + file
            with open(filePath, "r") as f:  # open file
                data = f.readlines()  # read file

            strarr = str(file)
            for sql in data:
                pp = sql.replace("\n", "")
                if pp != "":
                    strarr += str(pp)
            self.fileList.append(strarr)


    def getTaintResult(self):
        taintResultFile = os.getenv("TAINT_RESULT")
        if taintResultFile is None:
            print("[+]Error: Read taint result failed!")
            sys.exit(0)

        global configVec, configVecLen
        with open(taintResultFile, "r") as f:
            result = f.readline()
            while result:
                configVec.append(result.replace("\n", ""))
                while "END_OF" not in result:
                    result = f.readline()
                result = f.readline()

        configVecLen = len(configVec)

        if configVecLen == 0 :
            print("[+]Error: Taint config vector is empty!")
            sys.exit(0)


    def convert(self, num):
        strnum = bin(num).replace("0b", "")
        for i in range(8 - len(strnum)):
            strnum = '0' + strnum
        return strnum


    def convert_u8_to_u32(self, u8bitmap, u32bitmap):
        mapLen = len(u8bitmap)

        i = 0
        while i < mapLen:
            a = self.convert(u8bitmap[i])
            b = self.convert(u8bitmap[i + 1])
            c = self.convert(u8bitmap[i + 2])
            d = self.convert(u8bitmap[i + 3])

            # You Motherfuker!!! Linux store List by litteEnd!! Fuck You!!!
            u32bitmap[int(i/4)] = int((d + c + b + a), 2)
            i += 4


    # bitmap is a list, python will use address passion but not value passion
    def getBitmap(self, bitmapTemp):
        global configVecLen

        # addr is a memory address, string_at() enable us to get data on the address
        _bitmap = string_at(addr, SHM_MAP_SIZE)

        # Be careful!  We need convert u8 to u32 later by a function!!!!
        for i in range(configVecLen * 4):
            # __afl_area_ptr[0] is a flag value, we should read the config bitmap from index 1
            bitmapTemp[i] = _bitmap[i + 4]
        #     if i % 4 != 3:
        #         print(bitmapTemp[i], end="")
        #     else:
        #         print(bitmapTemp[i], end=", ")



    def checkCrash(self, out_error):
        errMsg = out_error
        #if "Seg" in errMsg or "Abort" in errMsg or "Assert" in errMsg or "seg" in errMsg or "abort" in errMsg or "assert" in errMsg:

        # check MySQL crash
        if "Lost" in errMsg or "Assert" in errMsg or "lost" in errMsg or "assert" in errMsg:
            return True
        else:
            return False


    """
        We need to know how many times the back-end service trigers the inserted IR instructions during 1s.
        So that we can avoid false positive result.

        self.bitmapGap : a pair list, each element is a range
    """
    def countBackServiceBitmap(self):
        global configVecLen
        global configVec

        u8bitmapInit = [0] * configVecLen * 4
        u8bitmapExec = [0] * configVecLen * 4

        u32bitmapInit = [0] * configVecLen
        u32bitmapExec = [0] * configVecLen

        circle = 10
        gapVec = [[0 for col in range(circle)] for row in range(configVecLen)]
        # for i in range(configVecLen):
        #     gapVec[i] = []

        for i in range(circle):
            print("Information: exec %ds - %ds"%(i, i+1))
            self.getBitmap(u8bitmapInit)
            time.sleep(1)
            self.getBitmap(u8bitmapExec)

            self.convert_u8_to_u32(u8bitmapInit, u32bitmapInit)
            self.convert_u8_to_u32(u8bitmapExec, u32bitmapExec)

            for x in u32bitmapExec:
                print(x, end=", ")
            print("\n\n")

            for j in range(configVecLen):
                # gap = 0
                # if bitmap_before[i+1] > bitmap_after[i+1]:
                #     gap = bitmap_before[i+1] + 256 - bitmap_after[i+1]
                # else :
                gap = u32bitmapExec[j] - u32bitmapInit[j]
                gapVec[j][i] = gap
                # self.bitMapGap.append(gap)

        for i in range(configVecLen):
            for j in range(circle):
                print(gapVec[i][j], end=", ")
            print()

        # Now we calucate the variance
        for i in range(configVecLen):
            dataList = gapVec[i]
            avg = np.mean(dataList)
            variance = np.std(dataList, ddof=1)
            gap = [avg - variance, avg + variance]
            self.bitMapGap.append(gap)

        # debug info
        # for i in range(configVecLen):
            # print("To config %s, the bit gap range is between %d to %d"%(configVec[i], self.bitMapGap[i][0], self.bitMapGap[i][1]))

    def calCommonSubset(self):
        lists = []
        for testCase in self.fileList:
            # if i == 1:
            #     break
            configImpact = self.configDict[testCase]
            lists.append(configImpact)
        
        sets = [set(lst) for lst in lists]
        intersection_set = reduce(lambda a, b: a.intersection(b), sets)
        self.commonSubset = list(intersection_set)
    

    """
        self.configCount: key   --> hash(testCase)
                          value --> influenced configs and its bits(a value is a hash list containing tow elements. The first is the config name and the second is the bitmap number of the config)
        
        the input of the function "subset" is the value of self.configCount
    """
    def setPriority(self, subset):
        subset = sorted(subset, key=lambda x: x[1], reverse=True)
    """
        For MySQL-testing, performDryRun check is unnecessary,
         and its main function will be merged into function getImpactConfigs.
    """
    # def performDryRun(self):
    #     global bitmap
    #     bitmap = [0] * configVecLen
    #     _bitmap = string_at(addr, SHM_MAP_SIZE)
    #     if _bitmap[0] != 0:
    #         print("[+]Error: The shared memory is not empty. Please set a new shared memory.(Using tool \"shm_clear\" and \"shm_init\")")
    #         sys.exit(0)

    #     process = subprocess.Popen(str(self.exe_path), shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
    #                                    stderr=subprocess.PIPE)
    #     out_info, out_err = process.communicate()
    #     self.getBitmap(bitmap)


    def isBitChange(self, bitGap, i):
        if bitGap > self.bitMapGap[i][1] or bitGap < self.bitMapGap[i][0]:
            return True
        else:
            return False


    """
        self.configDict: key   --> hash(testCase)
                         value --> influenced configs list(its element is config name like "optimizer_search_depth", but not config entry)
    """
    def getImpactConfigs(self):
        global configVecLen
        circle = 3

        for testCase in self.fileList:
            gapVec = [[0 for col in range(circle)] for row in range(configVecLen)]
            realConfig = ""
            for index in range(circle):
                u8bitmapInit = [0] * configVecLen * 4
                u8bitmapExec = [0] * configVecLen * 4

                u32bitmapInit = [0] * configVecLen
                u32bitmapExec = [0] * configVecLen

                configImpact = []
                # this is used to calucate the prority of config 
                config_bit = {}
                process = subprocess.Popen(str(self.exe_path), shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                       stderr=subprocess.PIPE)

                # input SQL using PIPE
                testCaseList = testCase.split(";")
                process.stdin.write(("drop database if exists test;create database test;use test;").encode())

                # get initial bitmap status
                self.getBitmap(u8bitmapInit)
                startTime = time.time()

                # input test case sql using stdin PIPE
                realConfig = testCaseList[0]
                for num in range(1, len(testCaseList)):
                    sql = testCaseList[num]
                    process.stdin.write((str(sql) + ";").encode())
                    ### -----debug info

                out_info, out_err = process.communicate()
                # print("out_info: ", out_info.decode())
                # print("out_err: ", out_err.decode())

                endTime = time.time()
                timeGap = endTime - startTime
                if timeGap < 1.0 :
                    time.sleep(1.0 - timeGap)

                self.getBitmap(u8bitmapExec)
                print("Total time consumption: " , time.time() - startTime)

                # if self.isBitmapChange(bitmapInit, bitmapExec) is False:
                #     print("Test case is not inluenced by any config")
                #     self.configDict[testCase] = configImpact
                #     continue

                # # print debug info
                # print("Start to print bitmapInit:")
                # for i in range(configVecLen) :
                #     print("\tbitmapInit[%d] = %d"%(i, bitmapInit[i]))

                # print("Start to print bitmapExec:")
                # for i in range(configVecLen) :
                #     print("\tbitmapExec[%d] = %d"%(i, bitmapExec[i]))

                self.convert_u8_to_u32(u8bitmapInit, u32bitmapInit)
                self.convert_u8_to_u32(u8bitmapExec, u32bitmapExec)

                for j in range(configVecLen):
                    gapVec[j][index] = u32bitmapExec[j] - u32bitmapInit[j]
                    print(gapVec[j][index], end=",")
                print()

            avgGapList = [0] * configVecLen
            for i in range(configVecLen):
                dataList = gapVec[i]
                avgGapList[i] = np.mean(dataList)

            print("Real Config is: ", realConfig)
            print("Found influenced configs: ", end="")
            tmp_num = 0
            for i in range(configVecLen):
                if self.isBitChange(avgGapList[i], i) :
                    if self.configKeyVal[configVec[i]] not in configImpact:
                        print(self.configKeyVal[configVec[i]], end=", ")
                        tmp_num += 1
                        # print("bitmapInit[%d] = %d, bitmapExec[%d] = %d, bitmapGapRange[%d] = %d - %d" %(i, u32bitmapInit[i], i, u32bitmapExec[i], i, self.bitMapGap[i][0], self.bitMapGap[i][1]))
                        configImpact.append(self.configKeyVal[configVec[i]])
                        
                        config_bit[self.configKeyVal[configVec[i]]] = avgGapList[i]
            print("The number of influenced configs: " + str(tmp_num))
            print("\n")
            self.configDict[testCase] = configImpact
            self.configCount[testCase] = config_bit


    """
        self.configKeyVal : key --> config entry point like "sqlite3.1"
                            val --> PRAGMA name

        self.configValue  : key --> PRAGMA name
                            val --> config possible values list
    """
    def getConfigKeyVal(self):
        with open(self.keyPath, 'r', encoding="utf-8") as f:
            data = f.readlines()

        for line in data:
            if line != "\n":
                flag = False
                pairList = line.split(",")
                key = pairList[0]
                PRAGMA = pairList[1].replace("\n", "")
                if PRAGMA[0] == '*':
                    PRAGMA = PRAGMA[1:len(PRAGMA)]
                    flag = True
                self.configKeyVal[key] = PRAGMA

                index = 2
                # candidate_val = pairList[2].split(" ")
                self.configValue[PRAGMA] = []
                while index < len(pairList):
                    if pairList[index] == "":
                        index+=1
                        continue

                    value = str(pairList[index])
                    if flag is True:
                        temp = value.split("_")
                        value = ""
                        for item in temp:
                            value += item
                            value += " "

                    if index == len(pairList) - 1:
                        if str(pairList[index]) != "\n":
                            self.configValue[PRAGMA].append(value.replace("\n", ""))
                    else:
                        self.configValue[PRAGMA].append(value)
                    index+=1


    def testPriority(self):
        self.calCommonSubset()
        print("Common subset is :", self.commonSubset)
        for testcase in self.fileList:
            config_bit = self.configCount[testcase]
            impactConfigs = self.configDict[testcase]
            complement = [item for item in impactConfigs if item not in self.commonSubset]
            # print("the complemnt is ", complement)

            testCaseList = testcase.split(";")
            print("related config: ", testCaseList[0])
            # debug info, without methods
            print("----------------------without methods-------------------")
            for i in range(len(impactConfigs)):
                print((i+1), " ==> ", impactConfigs[i])

            complement_bits = []
            commonSubset_bits = []
            for config in complement:
                tmp_set = []
                tmp_set.append(config)
                tmp_set.append(config_bit[config])
                complement_bits.append(tmp_set)

            for config in self.commonSubset:
                tmp_set = []
                tmp_set.append(config)
                tmp_set.append(config_bit[config])
                commonSubset_bits.append(tmp_set)


            index = 1
            for element in complement_bits:
                print(index, " ==> ", element)
                index += 1
            for element in commonSubset_bits:
                print(index, " ==> ", element)
                index += 1
            print("\n\n")



            print("------------*********with methods 2222********-------------")
            all_config = []
            for config in impactConfigs:
                tmp_set = []
                tmp_set.append(config)
                tmp_set.append(config_bit[config])
                all_config.append(tmp_set)
            all_config.sort(key=lambda x: x[1], reverse=True)
            index = 1
            for element in all_config:
                print(index, " ==> ", element)
                index += 1
            print("\n\n")



            print("------------*********with methods********-------------")
            complement_bits.sort(key=lambda x: x[1], reverse=True)
            commonSubset_bits.sort(key=lambda x: x[1], reverse=True)


            index = 1
            for element in complement_bits:
                print(index, " ==> ", element)
                index += 1
            
            print("\n交集:")

            for element in commonSubset_bits:
                print(index, " ==> ", element)
                index += 1
            print("\n\n")


    def execFuzz(self):
        """
            In the three 'for' nested loops, the first traverses all test cases,
            while the second traverses all configs that influence the test case,
            and the third will traverse all possible values the config could be.
        """
        ### -----debug info
        i = 0

        for testCase in self.fileList:
            # if i == 1:
            #     break
            configImpact = self.configDict[testCase]
            for config in configImpact:
                for value in self.configValue[config]:

                    # create a SQLite process and get its stdin PIPE and stdout PIPE
                    process = subprocess.Popen(str(self.exe_path), shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                       stderr=subprocess.PIPE)


                    # for MySQL
                    drop_SQL = "drop database if exists test;\n"
                    create_SQL = "create database test;\n"
                    use_SQL = "use test;\n"
                    process.stdin.write(drop_SQL.encode())
                    process.stdin.write(create_SQL.encode())
                    process.stdin.write(use_SQL.encode())

                    # change config
                    config_change_SQL = "SET " + str(config) + "=" + str(value) + ";"
                    process.stdin.write(config_change_SQL.encode())

                    ### -----debug info
                    print(config_change_SQL)

                    # splite the whole test case into several SQL and put them in a list
                    testCaseList = testCase.split(";")
                    testFileNameList = testCaseList[0].split("--")
                    # input test case sql using stdin PIPE。
                    for num in range(1, len(testCaseList)):
                        sql = testCaseList[num]
                        process.stdin.write((str(sql) + ";").encode())
                    print("Return Code is : ", process.poll())

                    out_info, out_err = process.communicate()
                    # print("info : ", out_info.decode(), "\nerrMsg: ", out_err.decode())
                    print("errMsg: ", out_err.decode())

                    if self.checkCrash(out_err.decode()) :
                        print("Successfully found a crash!")
                        print("Now wait 5 seconds for MySQL to restart...")
                        time.sleep(5)
                        # record pair <testCase, config>
                        crashFile = "/root/output/5-7-20/output_" + str(testFileNameList[0])
                        with open(crashFile, 'a', encoding='utf-8') as f:
                            f.write("Config:\n\t==> " + str(config) + " = " + str(value) + "\n")
                            f.write("Test Case:\n\t==> " + str(testCase) + "\n")
                            f.write("ERROR MSG:\n\t==> " + str(out_err.decode()).replace("\n", "\n\t    ") + "\n\n")

                    print("--------------------------------------------------------------------")

            i+=1



if __name__ == '__main__':
    start = time.time()
    # subprocess.call('source /etc/profile', shell=True)
    if len(sys.argv) != 4:
        print("[+]ERROR: Oops... Please input test cases directory just like \"python3 diffTest.py /path/to/test_case/ /path/to/keyVal.txt /path/to/ELF_FILE\"")
        sys.exit(0)

    # fuzz = diffFuzz(sys.argv[1], sys.argv[2], sys.argv[3])
    # MySQL shell contain blank, still don't know how to solve it
    fuzz = diffFuzz(sys.argv[1], sys.argv[2], "mysql -u root -p\'1024\'")
    fuzz.getTaintResult()
    fuzz.getFileList()

    # Debug code
    """
    fuzz.getConfigKeyVal()
    for pair in fuzz.configKeyVal.items():
        print("entry point : ", pair[0], " | PRAGMA name: ", pair[1])
    print("key_value dic len is : ", len(fuzz.configKeyVal))
    for key in configVec:
        print("entry point : ", key, " | PRAGMA name: ", fuzz.configKeyVal[key])
    """
    # fuzz.performDryRun()
    # fuzz.getImpactConfigs()
    # fuzz.getConfigKeyVal()
    # Now, start our config fuzz!
    try:
        # fuzz.performDryRun()
        fuzz.getConfigKeyVal()
        fuzz.countBackServiceBitmap()
        fuzz.getImpactConfigs()
        fuzz.testPriority()
        fuzz.execFuzz()
        # for pair in fuzz.configDict.items():
        #     print("Test case : ", pair[0], "\ninfluenced configs:")
        #     for config in pair[1]:
        #         print("\t", config)

        # print("bitmap gap value:")
        # for i in range(len(fuzz.bitMapGap)):
        #     print("index ", i , " | value : ", fuzz.bitMapGap[i])

        # for pair in fuzz.configValue.items():
        #     print("config name: ", pair[0], "| possible value: ", pair[1])


    except KeyboardInterrupt:
        end = time.time()
        print('The time consumtion is %s Seconds' % (end - start))
        sys.exit(0)

    end = time.time()
    print('The time consumtion is %s Seconds' % (end - start))
