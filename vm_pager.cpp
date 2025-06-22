#include "vm_pager.h"
#include "pager_internal.h"
#include <map>
#include <algorithm>
#include <cstring>

// pager state namespace from pager_internal
using namespace PagerState;

void vm_init(unsigned int memory_pages, unsigned int swap_blocks){
    // Set memory pages and SB of namespace pager state object
    memoryPages = memory_pages;
    swapBlocks = swap_blocks;
    
    // Initialize physical memory data
    physicalMemoryInfo.clear();
    physicalMemoryInfo.resize(memory_pages);

    // Default construct a physical page data for each page
    for (unsigned int i = 0; i < memory_pages; ++i) {
        physicalMemoryInfo[i] = PhysicalPageData();
    }

    physicalMemoryInfo[0].free = false;
    physicalMemoryInfo[0].referenced = true;
    physicalMemoryInfo[0].dirty = false;
    physicalMemoryInfo[0].valid = true;
    
    // Fill zero page with 0's. Physical memory is in vm physmem, so access:
    void* zeroPageAddr = static_cast<char*>(vm_physmem);
    memset(zeroPageAddr, 0, VM_PAGESIZE);

    // Populate free swap block list
    freeSwapBlocks.clear();
    for (unsigned int i = 0; i < swap_blocks; ++i) {
        freeSwapBlocks.push_back(i);
    }

    // Populate free memory to include all but the pinned '0' page
    freeMemoryBlocks.clear();
    for (unsigned int i = 1; i < memory_pages; ++i) {
        freeMemoryBlocks.push_back(i);
    }
}

/* If parent process has an empty arena, it means:\
 * No valid Pages
 * No mapped or swapped
 * No need to copy pages or metadata
 * No set up for copy on write or managed shared pages
 * So- just create a new process and add it to the process map
 */
int vm_create(pid_t parent_pid, pid_t child_pid){
    Process childProcess(child_pid);
    processMap[child_pid] = childProcess;
    return 0;
}

// This function needs to swtich to the given process, so it must:
// Update currentProcessID and update the MMU page tabe pointer
// I dont think theres anything else to do
void vm_switch(pid_t pid){
    // Update pid
    currentProcessID = pid;
    // Locate process in process map
    auto it = PagerState::processMap.find(pid);
    assert(it != PagerState::processMap.end());
    // Set MMU page table base
    page_table_base_register = it->second.pageTable.data();
}

int vm_fault(const void* addr, bool write_flag) {
    // first get the VPN that is faulting
    uintptr_t virtualAddr = reinterpret_cast<uintptr_t>(addr);
    
    uintptr_t base_addr = reinterpret_cast<uintptr_t>(VM_ARENA_BASEADDR);
    uintptr_t end_addr = base_addr + VM_ARENA_SIZE;
    // Check if the address is outside arena 
    // Check removed... 
    if (virtualAddr < base_addr || virtualAddr >= end_addr) {
        return -1;
    }
    unsigned int vpn = (virtualAddr - base_addr) / VM_PAGESIZE;

    // Access the current process
    pid_t pid = PagerState::currentProcessID;
    auto& process = PagerState::processMap[pid];
    auto& virtualPage = process.OSTable[vpn];
    unsigned int ppn;
    void* phys_addr;

    // Ensure VPN is valid
    if (!virtualPage.valid) {
        return -1;
    }

    // If page is outside of memory, we need to bring it in 
    if (!virtualPage.resident) {
        // Gets arbitrary free page or evict a page if no free page exists
        // This should update all the physical page bits
        ppn = get_open_page();

        if (ppn == 0) {
            return -1;
        }

        phys_addr = static_cast<char*>(vm_physmem) + ppn * VM_PAGESIZE;

        // Update incoming file-backed page
        if (virtualPage.fileBacked) {

            // Do the file_read before anything else.
            // Before this fails, we should have either: A) evicted a page or B) done nothing. 
            if (file_read(virtualPage.filename.c_str(), virtualPage.fileBlockNum, phys_addr) != 0) {
                return -1;
            }

            // Once read goes through, we can do the rest of the updating:

            // Find pid, vpn for each filename::block combo
            auto key = std::make_pair(virtualPage.filename, virtualPage.fileBlockNum);
            auto it = PagerState::boundFileBacked.find(key);

            // Update all incoming file-backed pages
            if (it != PagerState::boundFileBacked.end()) {
                const auto& ownerSet = it->second;
                // Iterate through set of <pid, vpn> pairs
                for (const auto& [pid, vpn] : ownerSet) {
                    PagerState::processMap[pid].pageTable[vpn].read_enable = 1;
                    PagerState::processMap[pid].pageTable[vpn].write_enable = write_flag;
                    PagerState::processMap[pid].pageTable[vpn].ppage = ppn;

                    PagerState::processMap[pid].OSTable[vpn].resident = true;
                    PagerState::processMap[pid].OSTable[vpn].physicalPage = ppn;
                    PagerState::processMap[pid].OSTable[vpn].dirty = write_flag;
                }
            }

        }
        // Update incoming swap-backed page
        else {

            // Read Page SWAP BLOCK --> PHYSICAL MEM 
            if (file_read(nullptr, virtualPage.swapBlock, phys_addr) != 0) {
                return -1;
            }

            // Update owner with current process and vpn:
            PagerState::physicalMemoryInfo[ppn].ownerInfo = {PagerState::currentProcessID, vpn};
            // Update the swap-backed page table
            PagerState::processMap[PagerState::currentProcessID].pageTable[vpn].read_enable = 1;
            PagerState::processMap[PagerState::currentProcessID].pageTable[vpn].write_enable = write_flag;
            PagerState::processMap[PagerState::currentProcessID].pageTable[vpn].ppage = ppn;

            // Update the swap-backed os table
            PagerState::processMap[PagerState::currentProcessID].OSTable[vpn].resident = true;
            PagerState::processMap[PagerState::currentProcessID].OSTable[vpn].physicalPage = ppn;
            PagerState::processMap[PagerState::currentProcessID].OSTable[vpn].dirty = write_flag;

        }

        // In both swap/file-backed, we successfully brought a page in!
        // Move the new page to back of clock queue
        PagerState::evictionClock.clockQueue.push_back(ppn);

        // Also- take away the ppn from the freeMemory blocks!
        PagerState::freeMemoryBlocks.pop_front();
        
    // Resident case
    } else {
        assert(virtualPage.physicalPage >= 0);
        ppn = virtualPage.physicalPage;

        // Swap-backed resident
        if (!virtualPage.fileBacked) {
            // COPY ON WRITE //
            if (virtualPage.physicalPage == 0 && write_flag) {
                // Assign a new physical page for the copied on write page
                ppn = get_open_page();

                // Failed to open up a page!
                if (ppn == 0) {
                    return -1;
                }

                // Succeed in getting a page, add it to clock queue
                PagerState::evictionClock.clockQueue.push_back(ppn);
                // This jawn is no longer free since we brought it in.
                PagerState::freeMemoryBlocks.pop_front();

                phys_addr = static_cast<char*>(vm_physmem) + ppn * VM_PAGESIZE;
                memset(phys_addr, 0, VM_PAGESIZE);
                PagerState::physicalMemoryInfo[ppn].ownerInfo = {PagerState::currentProcessID, vpn};

            }

            // Update the swap-backed os table
            if (write_flag) {
                virtualPage.dirty = true;
            }
            virtualPage.resident = true;
            virtualPage.physicalPage = ppn;

            // Update the swap-backed page table
            PagerState::processMap[PagerState::currentProcessID].pageTable[vpn].read_enable = 1;
            PagerState::processMap[PagerState::currentProcessID].pageTable[vpn].write_enable = virtualPage.dirty;
            PagerState::processMap[PagerState::currentProcessID].pageTable[vpn].ppage = ppn;
            
        } 
        // File backed resident
        else {
            // Update incoming virtual page and its bretheren
            auto key = std::make_pair(virtualPage.filename, virtualPage.fileBlockNum);
            auto it = PagerState::boundFileBacked.find(key);

            if (it != PagerState::boundFileBacked.end()) {
                const auto& ownerSet = it->second;
                // Iterate through set of <pid, vpn> pairs
                for (const auto& [p, v] : ownerSet) {
                    auto& tempProcess = PagerState::processMap[p];
                    
                    // Update OSTable
                    if (write_flag) {
                        tempProcess.OSTable[v].dirty = true;
                    }
                    tempProcess.OSTable[v].resident = true;
                    tempProcess.OSTable[v].physicalPage = ppn;

                    // Update Page Table
                    tempProcess.pageTable[v].read_enable = 1;
                    tempProcess.pageTable[v].write_enable = tempProcess.OSTable[v].dirty;
                    tempProcess.pageTable[v].ppage = ppn;

                }
            }
        }
        
    }

    // Update physical memory for bringing in page
    PagerState::physicalMemoryInfo[ppn].dirty = virtualPage.dirty;
    PagerState::physicalMemoryInfo[ppn].fileBacked = virtualPage.fileBacked;
    PagerState::physicalMemoryInfo[ppn].fileBlockNum = virtualPage.fileBlockNum;
    PagerState::physicalMemoryInfo[ppn].filename = virtualPage.filename;
    PagerState::physicalMemoryInfo[ppn].free = false;
    PagerState::physicalMemoryInfo[ppn].referenced = true;
    PagerState::physicalMemoryInfo[ppn].valid = true;

    return 0;
}



/* 
 * For current process:
 * 1. Free up any physical memory it's pointing to
 * 2. If file-backed page, ensure it can be written to file block later by clock
 * 3. If swap-backed page, add its swap block back to freeSwapBlocks
 * 4. Erase pid from processMap.
 */

 void vm_destroy() {
    // Find current process in map (the one that is being destroyed)
    
    pid_t pid = PagerState::currentProcessID;
    auto& process = processMap[pid];
    // For each entry in process OS table:
    for (unsigned int vpn = 0; vpn < NUM_VPAGES; ++vpn) {
        
        auto& page = process.OSTable[vpn];

        // Once we get to an invalid page, we know we're done
        if (!page.valid) break;

        // If we are freeing a swap-backed zero-pointing page, just give its swap block away and continue
        if (!page.fileBacked && page.physicalPage == 0) {
            freeSwapBlocks.push_back(page.swapBlock);
            continue;
        }

        // Free up this page if it's a swap-backed resident
        if (page.resident && !page.fileBacked) {
            // Check if physical page is valid (bounds check)
            if (page.physicalPage < 0 || page.physicalPage >= PagerState::memoryPages) {
                //std::cerr << "This should never print";
                continue;
            }
            auto& physicalPage = physicalMemoryInfo[page.physicalPage];

            // Free this physical page
            physicalPage.free = true;
            physicalPage.valid = false;
            physicalPage.dirty = false;
            physicalPage.referenced = false;

            // Erase this page from clock

            // ***IMPORTANT***
            // As a result...
            // We must make sure we add to clockQueue when the next freeMemoryBlock is taken in vm_fault...
            auto it = std::find(evictionClock.clockQueue.begin(), evictionClock.clockQueue.end(), page.physicalPage);

            // std::cerr << "page.physicalPage: " << page.physicalPage;
            assert(it != evictionClock.clockQueue.end() && "Physical page somehow not in clock");
            evictionClock.clockQueue.erase(it);

            // Free up the memory block. Next page brought in should use one of these free pages- not clock
            freeMemoryBlocks.push_back(page.physicalPage);

        }
        // All file-backed pages need their entry removed out of page tracker
        if (page.fileBacked) {
            // Remove this vpn, pid from the fileBacked page tracker (boundFileBacked)
            auto key = std::make_pair(page.filename, page.fileBlockNum);
            auto it = boundFileBacked.find(key);

            // Erase this entry
            if (it != boundFileBacked.end()) {
                it->second.erase({pid, vpn});
            }

            // If the set is empty, erase the set from the map
            // This ensures next vm_map to this <filename, block> succeeds
            if (it->second.empty()){
                boundFileBacked.erase(it);
            }

        } 
        // In swap backed case, we always need to free its reserved swap block
        else {
            freeSwapBlocks.push_back(page.swapBlock);
        }
    }

    // Remove process from process map 
    PagerState::processMap.erase(pid);
}

 /*
  First get the current process,
  Find the first invalid virtual page in the arena
  Check swap space availability
  then Update the OS table entry
  Update the table entry from MMU view
  Return virtual addr
 */
void* vm_map(const char* filename, unsigned int block){
    // Access process
    pid_t pid = currentProcessID;
    auto& process = processMap[pid];

    // Assign it the next free VPN
    unsigned int vpn = process.nextFreeVPN;
    if (vpn >= NUM_VPAGES) {
        return nullptr;
    }
    
    // File-backed initialization
    if (filename != nullptr) {

        // Read filename
        std::string totalFilename = "";
        totalFilename = read_filename_from_arena(filename, process);

        // Bad filename = terminate vm_map
        if (totalFilename == "bad-name"){
            return nullptr;
        }

        // Once we have a correct filename + a virtual page free, we should be chilling to add this process
        process.OSTable[vpn] = VirtualPageData();
        process.pageTable[vpn] = page_table_entry_t();

        // If <filename, block> is the same as another page, set its contents to be the same
        auto pageKey = std::make_pair(totalFilename, block);
        auto it = boundFileBacked.find(pageKey);

        // DANGER!!!!!
        // This assumes every time we have this key map to something, it has something in there
        if (it != boundFileBacked.end() && !it->second.empty()) {
            // Take pid, vpn from the first entry in the set (assuming there is a first entry)
            auto& tempEntry = *it->second.begin();
            auto& tempPID = tempEntry.first;
            auto tempVPN = tempEntry.second;
            
            // Initialize OSTable + pageTable with existing values
            process.OSTable[vpn] = processMap[tempPID].OSTable[tempVPN];
            process.pageTable[vpn] = processMap[tempPID].pageTable[tempVPN];
        }
        // <filename, block> doesn't already exist, create all new initialization
        else {
            
            // All new OS initialization (only differs by these 4 from default constructor)
            process.OSTable[vpn].valid = true;
            process.OSTable[vpn].fileBacked = true;
            process.OSTable[vpn].fileBlockNum = block;
            process.OSTable[vpn].filename = totalFilename;

            // We know this page isn't accessible yet
            process.pageTable[vpn].read_enable = 0;
            process.pageTable[vpn].write_enable = 0;
            
            int tblock = block;
            // Make sure there isn't a dead filename:block somewhere in physical memory we can map to
            for (size_t i = 1; i < physicalMemoryInfo.size(); i++) {
                auto& physicalPage = physicalMemoryInfo[i];
                if (physicalPage.filename == totalFilename && physicalPage.fileBlockNum == tblock) {
                    
                    process.OSTable[vpn].physicalPage = i;
                    process.OSTable[vpn].dirty = physicalPage.dirty;
                    process.OSTable[vpn].resident = true;

                    // This shouldn't necessarily be 1:
                    // Could be an unreferene page, meaning read-enable should be false
                    process.pageTable[vpn].read_enable = physicalPage.referenced;
                    process.pageTable[vpn].write_enable = physicalPage.dirty;

                    if (!physicalPage.referenced) {
                        process.pageTable[vpn].write_enable = false;
                    }

                    process.pageTable[vpn].ppage = i;

                    break;
                }
            }
            
        }
        // Add new file-backed page to map
        boundFileBacked[{totalFilename, block}].insert({pid, vpn});

    // Init swap-backed page
    } else { 
        // Eager backing: Ensure there's a swap space
        if (PagerState::freeSwapBlocks.empty()) {
            return nullptr;  
        }
        // Update OS Table with standard bits + physical page = 0
        // Also gives it a swap block
        initSwapBackedOS(process, vpn);

        // Read enable, write disable, ppage = 0
        initSwapBackedPageTable(process, vpn);
    }
        
    // For all cases: Incrememnt next VPN and return virtual addr
    process.nextFreeVPN++;
    uintptr_t vaddr = reinterpret_cast<uintptr_t>(VM_ARENA_BASEADDR) + (vpn * VM_PAGESIZE);
    return reinterpret_cast<void*>(vaddr);
}

/*
 * file_read
 *
 * Read page from the specified file and block into buf.
 * If filename is nullptr, the data is read from the swap file.  buf should
 * be an address in vm_physmem.
 * Returns 0 on success; -1 on failure.
 */
int file_read(const char* filename, unsigned int block, void* buf);

int file_write(const char* filename, unsigned int block, const void* buf);

extern void* const vm_physmem;

// Get process to get said page
/*
 * File backed - need to know everything about a file 
 * IF a process dies free it from the clock :>
 * See if process in clock is equal to the one in the process
 * 
 * 
 * */