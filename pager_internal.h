// pager_internal.h
#ifndef PAGER_INTERNAL_H
#define PAGER_INTERNAL_H
#pragma once

#include <iostream>
#include <unordered_map>
#include <vector>
#include <map>
#include <string>
#include <stack>
#include <sys/types.h>
#include <set>
#include "vm_arena.h"
#include "assert.h"
#include "vm_pager.h"
#include <deque> 


constexpr unsigned int NUM_VPAGES = VM_ARENA_SIZE / VM_PAGESIZE;
struct page_table_entry_t;

struct PhysicalPageData {
    bool free = true;
    bool referenced = false;
    bool dirty = false;
    bool valid = false;
    bool fileBacked = false;
    int fileBlockNum = -1;
    std::string filename = "";

    std::pair<pid_t, unsigned int> ownerInfo;
};

struct VirtualPageData {
    bool valid = false;
    bool fileBacked = false;
    int fileBlockNum = 0;
    std::string filename = "";
    int physicalPage = -1;
    int swapBlock = -1;
    bool resident = false;
    int dirty = 0;
};

struct Process {
    pid_t id;
    std::vector<page_table_entry_t> pageTable;
    std::vector<VirtualPageData> OSTable;
    unsigned int nextFreeVPN = 0;
    unsigned int numValidPages = 0;
    
    Process();      
    Process(int pid);
};

struct Clock {
    std::deque<unsigned int> clockQueue;
    //void initialize(unsigned int size);
    unsigned int evict_and_update_clock();
};

namespace PagerState {
    inline std::vector<PhysicalPageData> physicalMemoryInfo;
    inline std::map<pid_t, Process> processMap;

    // Used for identifying and maintaining file-backed pages pointing to same file::block combo
    inline std::map<std::pair<std::string, unsigned int>, std::set<std::pair<pid_t, unsigned int>>> boundFileBacked;

    inline pid_t currentProcessID = 0;
    inline int memoryPages = 0;
    inline unsigned int zeroPage = 0;
    
    inline unsigned int swapBlocks = 0;
    inline std::deque<unsigned int> freeSwapBlocks;

    inline std::deque<unsigned int> freeMemoryBlocks;
    inline Clock evictionClock;
}


std::string read_filename_from_arena(const char* filenamePtr, Process& process);

inline std::string makeFileKey(const std::string& filename, unsigned int block) {
    return filename + std::to_string(block);
}
void initSwapBackedOS(Process& process, unsigned int vpn);
void initSwapBackedPageTable(Process& process, unsigned int vpn);

void updateSwapBackedPage(int vpn, Process& process, bool resident);
unsigned int get_open_page();

void initialize_OS_table_entry(Process& process, unsigned int vpn, bool valid, bool filebacked, std::string filename, unsigned int block);
void update_page_table_entry(unsigned int ppn, unsigned int vpn, bool read_enable, bool write_enable, Process& process);
#endif // PAGER_INTERNAL_H