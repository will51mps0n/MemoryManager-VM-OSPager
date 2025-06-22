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

    uintptr_t ptr = reinterpret_cast<uintptr_t>(filenamePtr);

    // Read each character in the char*
    while (true) {
        // Not in a valid place
        if (ptr < base || ptr >= max) {
            return "bad-name";
        }

        // Find virtual page number + offset for this character
        unsigned int vpn = (ptr - base) / VM_PAGESIZE;
        unsigned int offset = (ptr - base) % VM_PAGESIZE;

        // Invalid filename pointer
        if (!process.OSTable[vpn].valid) {
            return "bad-name";
        }

        // Fault in non-resident pages
        if (!process.pageTable[vpn].read_enable) {
            // If the vm_fault fails we know we have a bad name
            if (vm_fault(reinterpret_cast<const void*>(ptr), false) == -1) {
                return "bad-name";
            }
        }

        // Find physical page of the jawn we're looking at
        int ppn = process.pageTable[vpn].ppage;

        if (ppn < 0 || ppn >= PagerState::memoryPages) {
            return "bad-name";
        }

        // In all cases, set the physical memory to referenced
        PagerState::physicalMemoryInfo[ppn].referenced = true;

        // Now, we have to set the read/write bits for all the associated pages
        // File-backed case (update all linked file-backed pages)
        if (process.OSTable[vpn].fileBacked) {
            // Update incoming virtual page and its bretheren
            auto key = std::make_pair(process.OSTable[vpn].filename, process.OSTable[vpn].fileBlockNum);
            auto it = PagerState::boundFileBacked.find(key);

            if (it != PagerState::boundFileBacked.end()) {
                const auto& ownerSet = it->second;
                // Iterate through set of <pid, vpn> pairs
                for (const auto& [p, v] : ownerSet) {
                    PagerState::processMap[p].pageTable[v].read_enable = 1;
                    PagerState::processMap[p].pageTable[v].write_enable = process.OSTable[vpn].dirty;
                }
            }
            
        }
        // Swap-backed case (update just the one owner)
        else {
            process.pageTable[vpn].read_enable = true;
            process.pageTable[vpn].write_enable = process.OSTable[vpn].dirty;
        }

        char* phys = static_cast<char*>(vm_physmem) + ppn * VM_PAGESIZE;
        char byte = phys[offset];
        
        // Break before we push back the null terminator into result
        if (byte == '\0') break;
        
        result.push_back(byte);

        ptr++;
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

/*
 * Runs the clock algorithm until it finds a page to evict.
 * Evicted page is written back if dirty, it's page tables are updated, and physicalMemory is freed
 * Subtract from clockQueue + add to freeMemoryBlocks
 */
 unsigned int Clock::evict_and_update_clock() {
    //std::cerr << "in clock\n";
    assert(clockQueue.size() == size_t(PagerState::memoryPages - 1));
    unsigned int ppn = 0;
    while (true) {
        // Iterate clock cycle
        ppn = clockQueue.front();

        void* phys_addr = static_cast<char*>(vm_physmem) + ppn * VM_PAGESIZE;
        auto& physicalPage = PagerState::physicalMemoryInfo[ppn];

        // REFERENCED PAGE --> Give it warning (for all of these we set read/write access to zero)
        if (physicalPage.referenced) {
            physicalPage.referenced = false;
            // auto& process = PagerState::processMap[PagerState::currentProcessID];

            /*
             * In the following section we set r/w to zero for all pages that refer to this
             * physical page. This gives us a chance to set referenced bit back to 1
             * on next access to this page (Manos lecture 15 at 1:17:00) 
             */

            // Swap-backed case (just update the current owner bits)
            if (!physicalPage.fileBacked) {
                pid_t tempProcess = PagerState::physicalMemoryInfo[ppn].ownerInfo.first;
                unsigned int tempVPN = PagerState::physicalMemoryInfo[ppn].ownerInfo.second;
                // std::cerr << "Owner Info VPN: " << tempVPN << "\n";
                PagerState::processMap[tempProcess].pageTable[tempVPN].read_enable = 0;
                PagerState::processMap[tempProcess].pageTable[tempVPN].write_enable = 0;
            }
            // File-backed case (use boundFileBacked for bit updates)
            else {
                // Find pid, vpn for each filename::block combo
                auto key = std::make_pair(physicalPage.filename, physicalPage.fileBlockNum);
                auto it = PagerState::boundFileBacked.find(key);

                if (it != PagerState::boundFileBacked.end()) {
                    const auto& ownerSet = it->second;
                    // Iterate through set of <pid, vpn> pairs
                    for (const auto& [pid, vpn] : ownerSet) {
                        PagerState::processMap[pid].pageTable[vpn].read_enable = 0;
                        PagerState::processMap[pid].pageTable[vpn].write_enable = 0;
                    }
                }
            }
        }
        // UNREFERENCED PAGE --> Evict
        else {
            // Evict File-backed
            if (physicalPage.fileBacked) {
                if (physicalPage.dirty) {
                    // This write should never fail. From piazza: "Once a file is accessible, it should ALWAYS be accessible"
                    // Therefore, if we've read from it before (and as a result, it exists in physical memory and can be evicted),
                    // we can assume the write will never fail.
                    // However, this return 0 is still here as a sanity check
                    if (file_write(physicalPage.filename.c_str(), physicalPage.fileBlockNum, phys_addr) != 0) {
                        return 0;
                    }
                }

                // THIS UPDATES ALL OUTGOING PAGES:

                // Find pid, vpn for each filename::block combo that's in the current page
                auto key = std::make_pair(physicalPage.filename, physicalPage.fileBlockNum);
                auto it = PagerState::boundFileBacked.find(key);

                // Updates evicted page tables 
                if (it != PagerState::boundFileBacked.end()) {
                    const auto& ownerSet = it->second;
                    // Iterate through set of <pid, vpn> pairs
                    for (const auto& [pid, vpn] : ownerSet) {
                        // Sets evicted bits
                        PagerState::processMap[pid].pageTable[vpn].read_enable = 0;
                        PagerState::processMap[pid].pageTable[vpn].write_enable = 0;

                        PagerState::processMap[pid].OSTable[vpn].dirty = false;
                        PagerState::processMap[pid].OSTable[vpn].resident = false;
                        PagerState::processMap[pid].OSTable[vpn].physicalPage = -1;
                    }
                }
            }
                
            // Evict Swap-backed
            else {
                // If a page is dirty there will only ever be one owner
                pid_t tempPID = physicalPage.ownerInfo.first;
                unsigned int tempVPN = physicalPage.ownerInfo.second;
                // Write if swap-backed page is dirty
                if (physicalPage.dirty) {
                    // This write should never fail. From piazza + .h file above to file_read/file_write:
                    // "A write to the swap space should never fail"
                    // However, this return 0 is still here as a sanity check
                    unsigned int swapBlock = PagerState::processMap[tempPID].OSTable[tempVPN].swapBlock;
                    if (file_write(nullptr, swapBlock, phys_addr) != 0) {
                        return 0;
                    }
                }
                /*
                 * OUTGOING PAGE TABLE + OS TABLE
                 */
                PagerState::processMap[tempPID].pageTable[tempVPN].read_enable = 0;
                PagerState::processMap[tempPID].pageTable[tempVPN].write_enable = 0;

                PagerState::processMap[tempPID].OSTable[tempVPN].dirty = false;
                PagerState::processMap[tempPID].OSTable[tempVPN].resident = false;
                PagerState::processMap[tempPID].OSTable[tempVPN].physicalPage = -1;

            }

            physicalPage.dirty = false;
            physicalPage.free = true;
            physicalPage.referenced = false;
            
            // Remove evicted page from the clock queue + push it to freeMemoryBlocks
            clockQueue.pop_front();
            PagerState::freeMemoryBlocks.push_front(ppn);

            // Once block is evicted, break out of the while loop
            break;
        }

        // No eviction happens, advance clock hand after dereferencing.
        clockQueue.pop_front();
        clockQueue.push_back(ppn);
    }
    assert(ppn != 0);
    return ppn;
}

/*
 * The idea with this function is to either take one of the free existing memory slots
 * OR to evict using clock. Both of these cases should update the PhysicalMemoryInfo.
 * Clock eviction (above) should also update the OSTable, Page Table, etc. for the outgoing page.
 */
unsigned int get_open_page() {
    unsigned int ppn;
    // If we have free memory blocks, just use one of those
    if (PagerState::freeMemoryBlocks.size() > 0) {
        // If free memory slot exists, don't even use clock
        ppn = PagerState::freeMemoryBlocks.front();

        // We wait until the read in succeeds to pop it from freeMemoryBlocks + add it to clock queue

    }
    // If no free blocks, evict from the clock (this updates what we need for OS + Page tables)
    else {
        ppn = PagerState::evictionClock.evict_and_update_clock();
    }

    return ppn;

}

