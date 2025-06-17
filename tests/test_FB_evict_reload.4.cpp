#include <iostream>
#include <cstring>
#include "vm_app.h"

int main() {
    std::cout << "in main, about to call to file - file backed copying data bin1 - this was used in the last test case as well (diff process)\n";
    char* fn_page = (char*) vm_map(nullptr, 0);
    std::cout << "in main, trying to read from swap back page\n";
    std::cout << fn_page[0] << "\n";
    std::cout << "in main, writing to the swap back page\n";
    strcpy(fn_page, "data1.bin");

    std::cout << "attempting to make a new page, by mapping to the same page as recently defined (not using data bin, but the first processes memory)\n";

    char* fb_page = (char*) vm_map(fn_page, 0);
    std::cout << fb_page[0] << "\n";
    fb_page[0] = 'A';
    std::cout << fb_page[0] << "\n";

    std::cout << "attempting to allocate more memory through swap backed pages\n";

    // Allocate more pages
    for (int i = 0; i < 5; ++i) {
        std::cout << "created new swap back page " << i << "\n";
        std::cout << "attempting to map page " << i << "\n";
        char* temp = (char*) vm_map(nullptr, 0);  
        std::cout << "writing to page " << i << "\n";
        temp[0] = 'X';
    }
    std::cout << "done with swap back pages loop, should have evicted the recent file backed pages\n";
    std::cout << "attemprting to read from original file backed page\n";

    std::cout << fb_page[0];  
    std::cout << "successfully read, test done\n";

    return 0;
}
