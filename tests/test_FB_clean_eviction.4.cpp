#include "vm_app.h"
#include <cstring>
#include <iostream>
/*
 * Tests the eviction of a clean file backed page
 * Access the file back page read on
 * Force evicton
 **/

 int main() {
    std::cout << "In main, about to call to file name with null ptr\n";
    char* filename = static_cast<char*>(vm_map(nullptr, 0));
    std::cout << "str copy, reading from a file into a swap block. Maybe wrong\n";
    strcpy(filename, "data1.bin");
    std::cout << "str cpy success\n";

    std::cout << "writing to a new page p1 using vm map and file. File backed\n";
    char* p1 = static_cast<char*>(vm_map(filename, 0));

    // Only read and this means the page should be clean
    std::cout << "reading char p1 at 0\n";
    char c = p1[0]; (void)c;
    std::cout << "done\n";

    return 0;
}