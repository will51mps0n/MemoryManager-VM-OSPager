#include <iostream>
#include <cstring>
#include <unistd.h>
#include <assert.h>
#include "vm_app.h"

// Testing swap backed page lifetimes
/*
 Tests swap page initialization, lifetime across processes
*/
int main() { 
    /* 4 pages of physical memory in the system */
    std::cout << "Creating page through fork on an empty arena\n";
    if (fork() != 0) { // parent
        std::cout << "In parent\n";
        auto *page0 = static_cast<char *>(vm_map(nullptr, 0));
        auto *page1 = static_cast<char *>(vm_map(nullptr, 0));
        auto *page2 = static_cast<char *>(vm_map(nullptr, 0));
        std::cout << "Created 3 pages and writing to all of them\n";
        vm_yield();
        page0[0] = page1[0] = page2[0] = 'a';
    } 
    else { // child
        std::cout << "In the child case, making a swap back page\n";
        auto *page0 = static_cast<char *>(vm_map(nullptr, 0));
        std::cout << "Write to child page 0\n";
        vm_yield();
        std::cout <<"DONE\n";
        std::strcpy(page0, "aweeee, kitty");
        //printf("%s", page0);
        std::cout <<"DONE\n";
    }

    std::cout << "\nDone with test\n";
    vm_yield();
    //exit(0);
    return 0;
}
