#include "vm_pager.h"
#include "pager_internal.h"
#include <map>
#include <algorithm>
#include <cstring>

// pager state namespace from pager_internal
using namespace PagerState;

void vm_init(unsigned int memory_pages, unsigned int swap_blocks){
    //std::cerr << "This is an error message6!" << std::endl;
    // Set memory pages and SB of namespace pager state object
    memoryPages = memory_pages; 
    swapBlocks = swap_blocks;
    //std::cerr << "memory size " << memoryPages << "\n";
    // Initialize physical memory data
    physicalMemoryInfo.clear();
    physicalMemoryInfo.resize(memory_pages);
    // Default construct a physical page data for each page
    for (unsigned int i = 0; i < memory_pages; ++i) {
        physicalMemoryInfo[i] = PhysicalPageData();
    }
    // Set up zero page
    // Allocate and mark as not free
    // update_physical_memory(zeroPage, 0, "", -1, false, false, false, false, false);
    updatePhysicalPage(zeroPage, 0 ,false, false, true, false);
    // Fill zero page with 0's. Physical memory is in vm physmem, so access:
    void* zeroPageAddr = static_cast<char*>(vm_physmem) + PagerState::zeroPage * VM_PAGESIZE;
    memset(zeroPageAddr, 0, VM_PAGESIZE);
    // Populate free swap block list
    freeSwapBlocks.clear();
    //freeSwapBlocks.resize(swap_blocks);
    for (unsigned int i = 0; i < swap_blocks; ++i) {
        freeSwapBlocks.push_back(i);
    }
    // Populate free memory to include all but the pinned '0' page
    freeMemoryBlocks.clear();
    //freeMemoryBlocks.resize(memory_pages - 1);
    for (unsigned int i = 1; i < memory_pages; ++i) {
        freeMemoryBlocks.push_back(i);
    }
    // std::cerr << freeSwapBlocks.size() << "\n";
}

int vm_create(pid_t parent_pid, pid_t child_pid){
    Process childProcess(child_pid);

    if (PagerState::freeSwapBlocks.size() < 1) {
        return -1;  
    }
    processMap[child_pid] = childProcess;
    //std::cerr << "This is an error message12!" << std::endl;
    return 0;
}

void vm_switch(pid_t pid){
    // Update pid
    currentProcessID = pid;
    // Locate process in process map
    auto it = PagerState::processMap.find(pid);
    assert(it != PagerState::processMap.end());
    // Set MMU page table base
    page_table_base_register = it->second.pageTable.data();
    return;
}

int vm_fault(const void* addr, bool write_flag) {
    // first get the VPN that is faulting
    bool dirtyUpdate = false;
    bool copyOnWrite = false;
    uintptr_t virtualAddr = reinterpret_cast<uintptr_t>(addr);
    
    // Check if the address is outside arena 
    if (virtualAddr < reinterpret_cast<uintptr_t>(VM_ARENA_BASEADDR) ||
        virtualAddr >= reinterpret_cast<uintptr_t>(VM_ARENA_BASEADDR) + VM_ARENA_SIZE ||
        virtualAddr + VM_ARENA_SIZE < reinterpret_cast<uintptr_t>(VM_ARENA_BASEADDR)) {
        //std::cout << "Virtual Address problem :(\n";
        return -1;
    }
    unsigned int vpn = (virtualAddr - reinterpret_cast<uintptr_t>(VM_ARENA_BASEADDR)) / VM_PAGESIZE;

    // Access the current process
    pid_t pid = PagerState::currentProcessID;
    auto& process = PagerState::processMap[pid];
    auto& page = process.OSTable[vpn];
    // Ensure VPN is valid
    if (!page.valid) {
        //std::cout << "Process at vpn invalid :(\n";
        return -1;
    }
    int ppn = -1;
    // Resident page
    if (page.resident) {
        //std::cerr << "HERERRE\n";
        ppn = page.physicalPage;
        // File backed resident
        if (page.fileBacked) {
            auto& fileMap = PagerState::fileBackMap[page.filename][page.fileBlockNum];
            for (auto& pair : fileMap) {
                for (auto& vpn_p : pair.second) {
                    dirtyUpdate = false;
                    if(write_flag || PagerState::physicalMemoryInfo[ppn].dirty) dirtyUpdate = true;
                    auto& fileProcess = PagerState::processMap[pair.first];
                    update_page_table_entry(ppn, vpn_p, true, dirtyUpdate, fileProcess);
                    update_os_table_entry(ppn, vpn_p, true, dirtyUpdate, page.fileBlockNum, fileProcess);
                }
            }
        }
        // Swap backed resident
        else { 
            if(write_flag && page.physicalPage == 0) {
                //std::cerr << "HER\n";
                if (PagerState::freeMemoryBlocks.size() < 1) {
                    PagerState::freeMemoryBlocks.push_back(evictionClock.evict_from_clock());
                } 
                ppn = freeMemoryBlocks.front();
                freeMemoryBlocks.pop_front();
                PagerState::evictionClock.clockQueue.push_back(ppn);
                void* phys_addr = static_cast<char*>(vm_physmem) + ppn * VM_PAGESIZE;
                memset(phys_addr, 0, VM_PAGESIZE);
                PagerState::physicalMemoryInfo[ppn].ownerInfo[PagerState::currentProcessID].push_back(vpn);
                //copyOnWrite = true;
            }
            page.dirty = true;
            PagerState::physicalMemoryInfo[ppn].fileBacked = false;
        }
    } 
    // *************
    // Non resident page (need to bring it in from memory)
    // *************
    else {
        // File backed non-resident
        if (page.fileBacked) {
            //std::cerr << "nonono\n";
            bool found = false;
            //TODO: Double check that we don't already map to same file/block like in vm_map
            for (unsigned int i = 0; i < physicalMemoryInfo.size(); i++) {
                if (physicalMemoryInfo[i].fileBlockNum == page.fileBlockNum 
                        && physicalMemoryInfo[i].filename == page.filename
                        && physicalMemoryInfo[i].fileBacked) {
                    //std::cerr << "hehrherhehrher\n";
                    ppn = i;
                    found = true;
                    break;
                }
            }
            if(!found) {
                if (PagerState::freeMemoryBlocks.size() < 1) {
                    //std::cerr << "in here\n";
                    PagerState::freeMemoryBlocks.push_back(evictionClock.evict_from_clock());
                }
                //std::cerr << PagerState::freeMemoryBlocks.size() << " size\n";
                ppn = PagerState::freeMemoryBlocks.front();
                PagerState::freeMemoryBlocks.pop_front();
                PagerState::evictionClock.clockQueue.push_back(ppn);
                //std::cerr << ppn << " help\n";
            }
            //std::cerr << page.filename << " file " << page.fileBlockNum << "\n";
            PagerState::physicalMemoryInfo[ppn].fileBlockNum = page.fileBlockNum;
            PagerState::physicalMemoryInfo[ppn].filename = page.filename;
            auto& fileMap = PagerState::fileBackMap[page.filename][page.fileBlockNum];
            for (auto& pair : fileMap) {
                for (auto& vpn_p : pair.second) {
                    dirtyUpdate = false;
                    if(write_flag || PagerState::physicalMemoryInfo[ppn].dirty) dirtyUpdate = true;
                    auto& fileProcess = PagerState::processMap[pair.first];
                    update_page_table_entry(ppn, vpn_p, true, dirtyUpdate, fileProcess);
                    update_os_table_entry(ppn, vpn_p, true, dirtyUpdate, page.fileBlockNum, fileProcess);
                }
            }
            //std::cerr << ppn << " is the fb page\n";
            PagerState::physicalMemoryInfo[ppn].fileBacked = true;
            void* phys_addr = static_cast<char*>(vm_physmem) + ppn * VM_PAGESIZE;
            if (file_read(page.filename.c_str(), page.fileBlockNum, phys_addr) == -1) {
                return -1;
            }
        }
        // Swap backed non-resident
        else {
            if(page.dirty || write_flag) {
                if (PagerState::freeMemoryBlocks.size() < 1) {
                    freeMemoryBlocks.push_back(evictionClock.evict_from_clock());
                } 
                ppn = PagerState::freeMemoryBlocks.front();
                PagerState::freeMemoryBlocks.pop_front();
                PagerState::evictionClock.clockQueue.push_back(ppn);
                PagerState::physicalMemoryInfo[ppn].fileBacked = false;
                PagerState::physicalMemoryInfo[ppn].ownerInfo[PagerState::currentProcessID].push_back(vpn);
                //std::cerr << "ppn2: " << ppn << "\n";

                if(page.dirty) {
                    void* phys_addr = static_cast<char*>(vm_physmem) + ppn * VM_PAGESIZE;
                    int swapblock = page.swapBlock;
                    if (file_read(nullptr, swapblock, phys_addr) == -1) {
                        return -1;
                    }
                }
                page.dirty = true;
            }
            else {
                ppn = 0;
            }
            PagerState::physicalMemoryInfo[ppn].fileBacked = false;
        }
    }
    page.resident = true;
    //std::cerr << "ppn: " << ppn << "\n";
    dirtyUpdate = false;
    if(write_flag || PagerState::physicalMemoryInfo[ppn].dirty) dirtyUpdate = true;
    if (copyOnWrite) dirtyUpdate = false;

    updatePhysicalPage(ppn, vpn, dirtyUpdate, true, true, dirtyUpdate);
    update_page_table_entry(ppn, vpn, true, dirtyUpdate, process);
    update_os_table_entry(ppn, vpn, true, dirtyUpdate, page.swapBlock, process);
    return 0;
}

 void vm_destroy() {
    // Find current process in map (the one that is being destroyed)
    pid_t pid = PagerState::currentProcessID;
    auto& process = processMap[pid];
    // For each entry in process OS table:
    for (unsigned int vpn = 0; vpn < NUM_VPAGES; ++vpn) {
        auto& page = process.OSTable[vpn];
        auto& filename = page.filename;
        auto& blockNum = page.fileBlockNum;

        // Once we get to an invalid page, we know we're done
        if (!page.valid) break;
        // Take care of physical page internals if this page is resident
        if (page.resident) {
            // Check if physical page is valid (bounds check)
            if (page.physicalPage < 0 || page.physicalPage >= PagerState::memoryPages) {
                //std::cerr << "This should never print";
                continue;
            }
            auto& physicalPage = physicalMemoryInfo[page.physicalPage];
            // File-backed case
            if (physicalPage.fileBacked) {
                auto& fileMap = PagerState::fileBackMap[filename][blockNum];
                fileMap.erase(pid);
                if (fileMap.empty()) {
                    //std::cerr << "HEHEHEHEHHEHE\n\n\n\n";
                    //physicalPage.free = true;
                    //physicalPage.referenced = false;
                }
                physicalPage.ownerInfo.erase(pid);
            }
            // Swap-backed case
            else {
                // Free this physical page
                physicalPage.free = true;
                //physicalMemoryInfo[page.physicalPage].free = true;
                physicalPage.ownerInfo.clear();

                // TODO: Erase this page from clock queue and bring it to the end
                auto it = std::find(evictionClock.clockQueue.begin(), evictionClock.clockQueue.end(), page.physicalPage);
                if(it != evictionClock.clockQueue.end()) {
                    evictionClock.clockQueue.erase(it);
                    freeMemoryBlocks.push_back(page.physicalPage);
                }
            }
        }
        //fileback map clean up
        if(page.fileBacked) {
            auto& fileMap = PagerState::fileBackMap[filename][blockNum];
                fileMap.erase(pid);
        }
        // In all swap-backed cases, free its swap block
        else if (!page.fileBacked && page.swapBlock != 0) {
            //std::cerr << "swapped block " << page.swapBlock << "\n";
            freeSwapBlocks.push_back(page.swapBlock);
        }
        else {
            //std::cerr << "NEVER HAPPEN\n";
        }
    }
    // Remove process from process map 
    PagerState::processMap.erase(pid);
    //std::cerr << "\n\n\n\n\n";
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

    // Determine if file-backed
    bool fileBacked = (filename != nullptr);
    
    // File-backed initialization
    if (fileBacked) {
        
        process.OSTable[vpn] = VirtualPageData();
        process.pageTable[vpn] = page_table_entry_t();

        // Get filename 
        std::string totalFilename = "";
        totalFilename = read_filename_from_arena(filename, process);
        if (totalFilename == "n") return nullptr;
        //std::cerr << totalFilename << " is the filename\n";
        //std::cerr << vpn << " is the vpn\n";

        // Init file-backed specifics
        process.OSTable[vpn].filename = totalFilename;
        process.OSTable[vpn].fileBlockNum = block;
        process.OSTable[vpn].fileBacked = true;
        process.OSTable[vpn].valid = true;

        // Fill fileBackMap
        auto& page = PagerState::fileBackMap[totalFilename][block];
        page[pid].push_back(vpn);

        bool alreadyResident = false;
        
        // Loop through physical memory, checks if same file + block present
        for (unsigned int i = 1; i < physicalMemoryInfo.size(); i++) {
            auto& tempFile = physicalMemoryInfo[i].filename;
            unsigned int tempBlock = physicalMemoryInfo[i].fileBlockNum;

            // If the file and block line up, assign new vpn to existing physical page
            if (totalFilename == tempFile && block == tempBlock && physicalMemoryInfo[i].fileBacked) {
                
                // We know this page is already resident + we know its physical page
                process.OSTable[vpn].physicalPage = i;
                process.OSTable[vpn].resident = true;

                process.pageTable[vpn].ppage = i;
                process.pageTable[vpn].read_enable = true;
                
                // Check physical memory to know about the write enable
                if (physicalMemoryInfo[i].dirty) {
                    process.pageTable[vpn].write_enable = true;
                } else {
                    process.pageTable[vpn].write_enable = false;
                }

                process.pageTable[vpn].read_enable = true;
                
                // Sets switch for next if-statement
                alreadyResident = true;
            }
        }

        // If we didn't assign the page to a physical block
        if (!alreadyResident) {
            process.pageTable[vpn].read_enable = false;
            process.pageTable[vpn].write_enable = false;
        }
        // END OF FILE BACKED PAGE

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

int file_read(const char* filename, unsigned int block, void* buf);

int file_write(const char* filename, unsigned int block, const void* buf);

extern void* const vm_physmem;
