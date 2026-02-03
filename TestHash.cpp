

#include <cstddef>
#include <cstdio>
#include <iostream>
#include <chrono>
#include <cxxabi.h>

#include "ImportantInclude.h"
#include "SimpleHashTable.h"

#include <map>
#include <flat_map>
#include <unordered_map>

#define MILLION 1000000
#define ITERATIONS 10
struct MemInfo
{
    MemInfo(int v)
    {
        counter = v;
    }
    bool array = false;
    bool shouldDelete = true;
    bool forceDelete = false;
    int lockCount = 0; //amount of currently active uses
    int counter = 0; //amount of references (not necessarily in use)
    void (*deleteFunc)(void*, bool) = nullptr; //the delete function. 1st argument is the pointer. 2nd argument specifies if it is an array or not.
    size_t sizeInBytes = 0;
};

size_t getTimeNano()
{
    return std::chrono::high_resolution_clock::now().time_since_epoch().count();
}


template<typename T, typename ...Args>
size_t benchmarkFunction(T&& func, Args&... v)
{
    size_t startTime = getTimeNano();
    for(int i=0; i<ITERATIONS; i++)
    {
        func(v...);
    }
    size_t endTime = getTimeNano();
    return (endTime-startTime)/ITERATIONS;
}

template<typename T>
void createEmptyDataStructure()
{
    T map = T();
}


template<typename T>
void fillWithIterableData()
{
    T map;
    for(int i=0; i<MILLION; i++)
    {
        // map.insert({std::to_string(i), MemInfo(1)});
        map.insert({i, MemInfo(1)});
    }
}

template<typename T>
void fillWithIterableDataRef(T& map)
{
    map.clear();
    for(int i=0; i<MILLION; i++)
    {
        // map.insert({std::to_string(i), MemInfo(1)});
        map.insert({i, MemInfo(1)});
    }
}

template<typename T>
void fillWithRandomDataRef(T& map)
{
    map.clear();
    for(int i=0; i<MILLION; i++)
    {
        uint32_t randInt = rand() % 32768;
        // map.insert({std::to_string(randInt), MemInfo(1)});
        map.insert({randInt, MemInfo(1)});
    }
}

template<typename T>
void fillWithRandomData()
{
    T map;
    for(int i=0; i<MILLION; i++)
    {
        uint32_t randInt = rand() % 32768;
        // map.insert({std::to_string(randInt), MemInfo(1)});
        map.insert({randInt, MemInfo(1)});
    }
}

template<typename T>
void clear(T& map)
{
    map.clear();
}

template<typename T>
void __declspec(noinline) search(T& map, std::vector<MemInfo>& collectedData) 
{
    // auto it = map.find("313131");
//     auto it = map.find(std::to_string(rand() % MILLION));
    auto it = map.find(rand() % MILLION);
    if(it != map.end())
    {
        collectedData.push_back(it->second);
    }
}


template<typename T>
void remove(T& map)
{
    for(int i=0; i<10000; i++)
    {
        // map.erase(std::to_string(i));
        map.erase(i);
    }
}

template<typename T>
size_t benchmarkClearTime(T& map)
{
    size_t totalTime = 0;
    for(int i=0; i<ITERATIONS; i++)
    {
        fillWithIterableDataRef(map);
        size_t startTime = getTimeNano();
        map.clear();
        size_t endTime = getTimeNano();
        totalTime += endTime - startTime;
    }
    return totalTime/ITERATIONS;
}

template<typename T>
size_t benchmarkClearRandTime(T& map)
{
    size_t totalTime = 0;
    for(int i=0; i<ITERATIONS; i++)
    {
        fillWithRandomDataRef(map);
        size_t startTime = getTimeNano();
        map.clear();
        size_t endTime = getTimeNano();
        totalTime += endTime - startTime;
    }
    return totalTime/ITERATIONS;
}

template<typename T>
size_t benchmarkDeleteTime(T& map)
{
    size_t totalTime = 0;
    for(int i=0; i<ITERATIONS; i++)
    {
        fillWithIterableDataRef(map);
        size_t startTime = getTimeNano();
        remove(map);
        size_t endTime = getTimeNano();
        totalTime += endTime - startTime;
    }
    return (totalTime/10000)/ITERATIONS;
}

template<typename T>
void benchmarkAllOps()
{
    std::vector<MemInfo> collectedData;
    int status;
    char* p = abi::__cxa_demangle(typeid(T).name(), NULL, NULL, &status);
    std::string demangledName = p;
    delete[] p;

    T defaultMap = T();
    printf("Time to benchmark %s\n", demangledName.c_str());
    
    size_t avgCreationTime = benchmarkFunction(createEmptyDataStructure<T>);
    printf("\tAverage Creation Time = %llu\n", avgCreationTime);
    
    //clear time needs existing data to check its proper time. That interferes with the timing of just map.clear() or whatever it is so this can't be done directly with a benchmark function without an init function or something
    size_t avgClearTime = benchmarkClearTime(defaultMap);
    size_t avgRandClearTime = benchmarkClearRandTime(defaultMap);
    
    printf("\tAverage Clear Time = %llu\n", avgClearTime);
    printf("\tAverage Random Clear Time = %llu\n", avgRandClearTime);

    size_t avgFillTime = benchmarkFunction(fillWithIterableDataRef<T>, defaultMap);
    printf("\tAverage In Order Insert Time = %llu\n", avgFillTime - avgClearTime);
    printf("\t\tUnique Items = %llu\n", defaultMap.size());

    size_t avgRandomFillTime = benchmarkFunction(fillWithRandomDataRef<T>, defaultMap);
    printf("\tAverage Random Insert Time = %llu\n", avgRandomFillTime - avgRandClearTime);
    printf("\t\tUnique Items = %llu\n", defaultMap.size());

    fillWithIterableDataRef(defaultMap);
    

    size_t avgSearchTime = 0;
    for(int i=0; i<100; i++)
    {
        avgSearchTime += benchmarkFunction(search<T>, defaultMap, collectedData);
    }
    printf("\tAverage Search Time = %llu\n", avgSearchTime/100);
//     printf("%llu\n", collectedData.size());
//     if(!collectedData.empty())
//         printf("%llu\n", collectedData.front().sizeInBytes);

    size_t avgRemoveTime = benchmarkDeleteTime(defaultMap);
    printf("\tAverage Remove Time = %llu\n", avgRemoveTime);
}

template<typename T>
bool checkingIfValid()
{
    return std::__is_transparent_v<T>;
}

int main()
{
//     printf("STD MAPS:______________________\n");
//     benchmarkAllOps<std::unordered_map<size_t, MemInfo>>(); //(baseline)
    
//     printf("TEST MAPS:______________________\n");
//     benchmarkAllOps<smpl::SimpleHashMap<size_t, MemInfo, std::hash<size_t>>>();


    std::unordered_map<size_t, MemInfo> map = std::unordered_map<size_t, MemInfo>();
    fillWithIterableDataRef<std::unordered_map<size_t, MemInfo>>(map);

    double totalSize = (map.size() * (sizeof(std::unordered_map<size_t, MemInfo>::value_type) + sizeof(void*)) + // data list
 map.bucket_count() * (sizeof(void*) + sizeof(size_t))) * 1.5;

 printf("Expected Total Size: %.3f\n", totalSize);

    return 0;
}

// clang++ -g -std=c++23 -O2 TestHash.cpp -o testHashBig.exe
// clang++ -g -fsanitize=address -std=c++23 -O2 TestHash.cpp -o testHashBig.exe