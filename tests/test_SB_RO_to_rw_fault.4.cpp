#include "vm_app.h"
#include <cstring>
#include <unistd.h>
#include <cassert>
#include <iostream>

/*
 * Read fault then a write fault
 * Should change the state from RO to RW
 */
int main() {
    std::cout << "Mapping to a swap back page\n";
    auto* pg = static_cast<char*>(vm_map(nullptr, 0));

    std::cout << "Reading from teh swap backed page. Should change state and work\n";
    char temp = pg[0]; 
    std::cout << "Setting it to self lol. Writing from the read should be 1 1 now\n";
    pg[0] = temp;       
    std::cout << "Test done\n";
    return 0;
}