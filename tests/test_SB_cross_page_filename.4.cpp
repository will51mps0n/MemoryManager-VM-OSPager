#include <iostream>
#include <cstring>
#include <unistd.h>
#include "vm_app.h"

/*
Validitiy check of multi page c string, write fault on multiple pages
*/
int main() {
    // map two swap-backed pages
    // Bits should be 0 and 1 
    // Filename should be pointing to page 0 - Virtual page 4 before virtual page 0
    // Start page 1 is base addr + vm page
    std::cout << "In main, mapping 2 swap backed pages\n";
    auto *page0 = static_cast<char *>(vm_map(nullptr, 0));
    auto *page1 = static_cast<char *>(vm_map(nullptr, 0));

    // write the filename into virtual memory
    std::cout << "Write file name into virtual memory\n";
    auto *filename = page0 + VM_PAGESIZE - 4;

    // Str cpy will execute instructions: Write fault to page 0 and 1:
    // after str copy, state of page 0 and 1 will be dirty: 
    // Perform stores bite by bite crossing over by writing to pages 0 to 1 - write fault on both to copy and write to both of these
    std::cout << "Calling str copy - should write fault page, store byte by byte and cross over writing to pages 0 to 1\n";
    std::strcpy(filename, "papers.txt");

    // map a file-backed page
    std::cout << "Now mapping a file backed page\n";
    auto *page2 = static_cast<char *>(vm_map(filename, 0));
    std::cout << "Reading from file backed page2\n";
    std::cout << page1[0] << " " << page1[0] << "\n";
    std::cout << "Setting the file to page 1\n";
    filename = page1 + VM_PAGESIZE - 4;
    std::cout << "Creating a new file backed page to the file name from page 1\n";
    auto *page3 = static_cast<char *>(vm_map(filename, 0));
    std::cout << "Reading from new page\n";
    std::cout << page2[0] << "\n";
    std::cout << page3[2] << "\n";
    std::cout << "Test done\n";
    return 0;
}
