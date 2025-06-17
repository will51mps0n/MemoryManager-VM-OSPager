#include <iostream>
#include <cstring>
#include <unistd.h>
#include <cassert>
#include "vm_app.h"

/*
 * This tests a file back mapping of the same vlovk in different virtual pages
 * this is testing the sharing or aliasing of ppn across vpns
*/
int main() {
    std::cout << "In main, mapping a new page to swap block\n";
    char* fname = static_cast<char*>(vm_map(nullptr, 0));
    std::cout << "copying file name into block\n";
    strcpy(fname, "papers.txt");

    std::cout << "Creating page A, file block to fname\n";
    char* pageA = static_cast<char*>(vm_map(fname, 0));
    std::cout << "Creating page B, also a file block to fname\n";
    char* pageB = static_cast<char*>(vm_map(fname, 0));

    // Write to one of the pages and read from the other
    std::cout << "Writing to page A\n";
    pageA[0] = 'X';
    std::cout << "Checking if that the write to A doesnt reflects B's page memory\n";
    assert(pageB[0] == 'X');
    std::cout << "The aliasing should not be happening, outputting page B not be X: " << pageB[0] << " But this should " << pageA[0] << "\n";
    return 0;
}
