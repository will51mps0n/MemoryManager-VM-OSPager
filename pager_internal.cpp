#include "pager_internal.h"
#include "vm_pager.h"

Process::Process() : id(-1) {
    pageTable.resize(NUM_VPAGES);
    OSTable.resize(NUM_VPAGES);
}
Process::Process(int pid) : id(pid) {
    pageTable.resize(NUM_VPAGES);
    OSTable.resize(NUM_VPAGES);
}

std::string read_filename_from_arena(const char* filenamePtr, Process& process){
    std::string result;
    uintptr_t base = reinterpret_cast<uintptr_t>(VM_ARENA_BASEADDR);
    uintptr_t max = base + VM_ARENA_SIZE;

    while (true) {
        uintptr_t vaddr = reinterpret_cast<uintptr_t>(filenamePtr);
        //std::cerr << base << " " << vaddr << "\n";
        // Not in a valid place
        if (vaddr < base || vaddr >= max) {
            //std::cerr << "here\n";
            return "n"; 
        }
        // This is if its not resident. If if its not then fault it in
        unsigned int vpn = (vaddr - base) / VM_PAGESIZE;
        unsigned int offset = (vaddr - base) % VM_PAGESIZE;

        if (!process.OSTable[vpn].valid) {
            return "";
        }

        // Page not resident? Fault it in.
        if (!process.OSTable[vpn].resident) {
            if (vm_fault(reinterpret_cast<const void*>(vaddr), false) == -1) {
                return "";
            }
        }

        int ppn = process.pageTable[vpn].ppage;
        
        if (ppn >= PagerState::memoryPages) {
            return "";
        }
        char* phys = static_cast<char*>(vm_physmem) + ppn * VM_PAGESIZE;
        char byte = phys[offset];
        result.push_back(byte);
        if (byte == '\0') break;

        filenamePtr++;
    }
    return result;
 }

/*
 * Initialize OS Table Entry for a Swap-backed page 
 * This always points to the zero page
 */
void initSwapBackedOS(Process& process, unsigned int vpn){
    // Blank OS Table Page
    process.OSTable[vpn] = VirtualPageData();

    // Initialize it with same bits as zero page
    process.OSTable[vpn].valid = true;
    process.OSTable[vpn].fileBacked = false;
    process.OSTable[vpn].physicalPage = 0;
    process.OSTable[vpn].resident = true;

    // Give it a swap block
    process.OSTable[vpn].swapBlock = PagerState::freeSwapBlocks.front();
    PagerState::freeSwapBlocks.pop_front();
    //std::cout << "block given " << process.OSTable[vpn].swapBlock << "\n";

}
/*
 * Initialize the Swap-Backed page 
 * This always points to the zero page
 */
void initSwapBackedPageTable(Process& process, unsigned int vpn){
    // Blank Physical Page Table
    process.pageTable[vpn] = page_table_entry_t(); 
    // You can read the zero page but you can't write to it
    process.pageTable[vpn].read_enable = true;
    process.pageTable[vpn].write_enable = false;
    process.pageTable[vpn].ppage = 0;
}

void updateSwapBackedPage(int vpn, Process& process, bool resident) {
    if(!resident) {
        update_os_table_entry(0, vpn, false, false, process.OSTable[vpn].swapBlock, process);
        update_page_table_entry(0, vpn, false, false, process);
    }
}

/* 
 * initialize_OS_table_entry
 * Resets the meta data of a virtual page using the default constructor
 */
void update_page_table_entry(unsigned int ppn, unsigned int vpn, bool read_enable, bool write_enable, Process& process) {
    process.pageTable[vpn].ppage = ppn;
    process.pageTable[vpn].read_enable = static_cast<int>(read_enable);
    process.pageTable[vpn].write_enable = static_cast<int>(write_enable);
}

void update_os_table_entry(unsigned int ppn, unsigned int vpn, bool read_enable, bool write_enable, int swapBlock, Process& process) {
    process.OSTable[vpn].resident = read_enable;
    process.OSTable[vpn].swapBlock = swapBlock;
    process.OSTable[vpn].physicalPage = ppn;
}

void updatePhysicalPage(unsigned int ppn, unsigned int vpn, bool dirty, bool referenced, bool read_enable, bool write_enable) {
    auto& page = PagerState::physicalMemoryInfo[ppn];
    page.dirty = dirty;
    page.referenced = referenced;
    //page.ownerInfo[PagerState::currentProcessID].push_back(vpn);
    
}

/* 
 * update_page_table_entry
 * updates a page table entry to the specified ppn, and sets the read and write enable bits for that entry
 * returns 0 on failure
 */
void initialize_OS_table_entry(Process& process, unsigned int vpn, bool valid, bool filebacked, std::string filename, unsigned int block) {
    process.OSTable[vpn] = VirtualPageData(); 
    process.OSTable[vpn].valid = valid;
    process.OSTable[vpn].fileBacked = filebacked;
    process.OSTable[vpn].fileBlockNum = block;
    process.OSTable[vpn].filename = filename;
    
    // Set swapBlock, ensuring there's a swap space available (eager swap reservation)
    if (!filebacked) {
        process.OSTable[vpn].swapBlock = PagerState::freeSwapBlocks.front();
        PagerState::freeSwapBlocks.pop_front();
    }else {
        process.OSTable[vpn].swapBlock = -1;
    }
    process.numValidPages++;
}

void Clock::initialize(unsigned int size) {
    for (unsigned int i = 1; i < size; i ++) {
        clockQueue.push_back(i);
    }
 }
 
 unsigned int Clock::evict_from_clock() {
    //std::cerr << "in clock\n";
    unsigned int ppn = 0;
    while (true) {
        ppn = clockQueue.front();
        //std::cerr << ppn << " clock\n";
        clockQueue.pop_front();
        void* phys_addr = static_cast<char*>(vm_physmem) + ppn * VM_PAGESIZE;
        auto& page = PagerState::physicalMemoryInfo[ppn];

        // Give it warning
        if (page.referenced) {
            page.referenced = false;
            if (!page.fileBacked) {
                auto& ownerInfoList = page.ownerInfo;
                auto it = ownerInfoList.begin();
                assert(it != ownerInfoList.end()); 
                pid_t ownerPid = it->first;
                auto& vpnList = it->second;
                assert(!vpnList.empty());
                auto& process = PagerState::processMap[ownerPid];
                update_page_table_entry(ppn, vpnList.front(), false, false, process);
            }
            else {
                auto& fileMap = PagerState::fileBackMap[page.filename][page.fileBlockNum];
                for (auto& pair : fileMap) {
                    for (auto& vpn_p : pair.second) {
                        auto& fileProcess = PagerState::processMap[pair.first];
                        update_page_table_entry(ppn, vpn_p, false, false, fileProcess);
                    }
                }
            }
            clockQueue.push_back(ppn);
        }
        // Evict
        else {
            // Evicit File-backed
            if (page.fileBacked) {
                //std::cerr << "here2\n";
                auto& fileMap = PagerState::fileBackMap[page.filename][page.fileBlockNum];
                for (auto& pair : fileMap) {
                    for (auto& vpn_p : pair.second) {
                        //std::cerr << "fileBack update table entry\n";
                        auto& fileProcess = PagerState::processMap[pair.first];
                        update_page_table_entry(ppn, vpn_p, false, false, fileProcess);
                        update_os_table_entry(ppn, vpn_p, false, false, page.fileBlockNum, fileProcess);
                    }
                }
                if (page.dirty) {
                    int result = file_write(page.filename.c_str(), page.fileBlockNum, phys_addr);
                    assert(result == 0);
                }
                PagerState::physicalMemoryInfo[ppn].fileBacked = false;
            }
            // Evict Swap-backed
            else {
                //std::cerr << "here\n";
                // Assuming swap back always only have one owner
                auto& ownerInfoList = page.ownerInfo;
                auto it = ownerInfoList.begin();
                assert(it != ownerInfoList.end()); 
                pid_t ownerPid = it->first;
                auto& vpnList = it->second;
                assert(!vpnList.empty()); 
                auto& process = PagerState::processMap[ownerPid];
                //std::cerr << "vpnlist size " << vpnList.size() << "\n";
                for (unsigned int i = 0; i < vpnList.size(); i++) {
                    unsigned int vpn = vpnList[i];
                    int swapBlock = process.OSTable[vpn].swapBlock;
                    assert(swapBlock != -1);
                    // May need to check for failure
                    if (page.dirty) {
                        if(file_write(nullptr, swapBlock, phys_addr) == 0) {
                            //std::cerr << "vpn " << vpn << " ppn " << ppn << "\n";
                        }
                    }
                    updateSwapBackedPage(vpn, process, false);
                }
            }
            // Always set 
            page.ownerInfo.clear();
            // vpn doesnt matter here - pass in 0 
            break;
        }
    }              
    PagerState::physicalMemoryInfo[ppn].dirty = false;
    //std::cerr << ppn << " im lost\n";
    return ppn;
}

unsigned int Clock::get_ppn() {
    
    std::deque<unsigned int>& clock = PagerState::evictionClock.clockQueue;
    
    // Looks for the first free page in clock
    for (unsigned int i = 0; i < clock.size(); ++i) {
        // If we find a free page, we will write to it (no longer free), push it to end of clock, and erase where it was before
        if (PagerState::physicalMemoryInfo[clock[i]].free) {
            PagerState::physicalMemoryInfo[clock[i]].free = false;
            int ppn = clock[i];
            clock.push_back(ppn);
            clock.erase(clock.begin() + i);
            return ppn;
        }
    }
    // No free page so we have to evict to get it
    return PagerState::evictionClock.evict_from_clock();
}

