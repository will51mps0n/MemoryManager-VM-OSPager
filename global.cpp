#include "global.h"
#include "pager_internal.h"
// THIS tracks resident file-backed pages across all processes

Process::Process() : id(-1) {
    pageTable.resize(NUM_VPAGES);
    OSTable.resize(NUM_VPAGES);
}
Process::Process(int pid) : id(pid) {
    pageTable.resize(NUM_VPAGES);
    OSTable.resize(NUM_VPAGES);
}

unsigned int PhysicalPageData::get_ref_count() {
   return ownerInfo.size();
}
