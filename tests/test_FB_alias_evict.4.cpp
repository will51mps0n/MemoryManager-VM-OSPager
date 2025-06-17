
#include <iostream>
#include <cstring>
#include "vm_app.h"

/*
 * This is two file back virtual pages that map to the same file block reflect changes
 * Makes sure the reloading doesnt break the connection 
*/
int main() {
    std::cout << "beginning fb alias evict \n";
    char* filename_page = (char*) vm_map(nullptr, 0);
    strcpy(filename_page, "data1.bin");

    std::cout << "beginning the mapping to file name \n";

    char* fb1 = (char*) vm_map(filename_page, 0);
    // These have an alias to the same block
    char* fb2 = (char*) vm_map(filename_page, 0); 
    std::cout << "here\n";

    // Write fault
    fb1[0] = 'A';  // Write fault
    // Give time for eviction
    vm_yield();    // Give time for possible eviction
    // This should see same underlying file because we mapped them to the same earlier
    fb2[1] = 'B';  // Should see same underlying file

    // This should be B now
    std::cout << fb1[1];  
    std::cout << fb2[0];
    std::cout << "all done \n";
    return 0;  
}