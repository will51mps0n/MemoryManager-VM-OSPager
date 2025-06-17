// global.h
#ifndef GLOBAL_H
#define GLOBAL_H
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

constexpr unsigned int NUM_VPAGES = VM_ARENA_SIZE / VM_PAGESIZE;

struct PhysicalPageData {
    bool free = true;
    bool referenced = false;
    bool dirty = false;
    bool read_enable = false;
    bool write_enable = false;
    bool fileback = false;
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
    bool dirty = false;
    bool referenced = false;
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

class ResidentFileTracker {
    public:
        struct FileBlockInfo {
            unsigned int ppn;                     
            std::set<pid_t> mappedProcesses;                     
            unsigned int refCount = 0; 
        
        };
        void addResidentBlock(const std::string& key, unsigned int ppn, pid_t pid);
        void removeProcessFromBlock(const std::string& key, pid_t pid);
        bool isBlockResident(const std::string& key) const;
        unsigned int getResidentPPN(const std::string& key) const;
        void erase(const std::string& key);
        void markBlockDirty(const std::string& key);
        std::map<std::string, FileBlockInfo> residentBlocks;
    private:

};

#endif // GLOBAL_H