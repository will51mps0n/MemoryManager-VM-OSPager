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
    // bool read_enable = false;
    // bool write_enable = false;
    bool fileBacked = false;
    int fileBlockNum = -1;
    std::string filename = "";
    std::map<pid_t, std::vector<unsigned int>> ownerInfo;
    
    unsigned int get_ref_count() { return ownerInfo.size(); }
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

    unsigned int get_ppn();
    void initialize(unsigned int size);
    unsigned int evict_from_clock();
};

namespace PagerState {
    inline std::vector<PhysicalPageData> physicalMemoryInfo;
    inline std::map<std::string, std::map<int, std::map<pid_t, std::vector<int>>>> fileBackMap; 
    inline std::map<pid_t, Process> processMap;
    inline pid_t currentProcessID = 0;
    inline int memoryPages = 0;
    inline unsigned int zeroPage = 0;
    
    inline unsigned int swapBlocks = 0;
    inline std::deque<unsigned int> freeSwapBlocks;

    inline std::deque<unsigned int> freeMemoryBlocks;
    inline Clock evictionClock;
}


std::string read_filename_from_arena(const char* filenamePtr, Process& process);
void updatePhysicalPage(unsigned int ppn, unsigned int vpn, bool dirty, bool referenced, bool read_enable, bool write_enable);

inline std::string makeFileKey(const std::string& filename, unsigned int block) {
    return filename + std::to_string(block);
}
void initSwapBackedOS(Process& process, unsigned int vpn);
void initSwapBackedPageTable(Process& process, unsigned int vpn);

void updateSwapBackedPage(int vpn, Process& process, bool resident);

void update_os_table_entry(unsigned int ppn, unsigned int vpn, bool read_enable, bool write_enable, int swapBlock, Process& process);
void initialize_OS_table_entry(Process& process, unsigned int vpn, bool valid, bool filebacked, std::string filename, unsigned int block);
void update_page_table_entry(unsigned int ppn, unsigned int vpn, bool read_enable, bool write_enable, Process& process);
#endif // PAGER_INTERNAL_H