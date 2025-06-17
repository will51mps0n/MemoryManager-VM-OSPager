#include <iostream>
#include <cstring>
#include <unistd.h>
#include <assert.h>
#include "vm_app.h"


/*
 * Shared physical page for file backed, copy on write on write
*/
int main() { /* 4 pages of physical memory in the system */
    std::cout << "In main, creating a new page swap back\n";
    auto *filename = static_cast<char *>(vm_map(nullptr, 0));
    std::cout << "Creating a child, having a baby, etc\n";
    std::strcpy(filename, "papers.txt");
    if (fork()) { // parent
        std::cout << "In parent\n";
        auto *fb_page = static_cast<char *>(vm_map(filename, 0));
        fb_page[0] = 'B';
        vm_yield();
    } else { // child
        std::cout << "In baby. Awwwwweeee\n";
        auto *fb_page = static_cast<char *>(vm_map(filename, 0));
        //assert(fb_page[0] == 'B');
        fb_page[0] = 'H';
    }
    std::cout << "Test done\n";
    return 0;
}
