#include "vm_app.h"
#include <iostream>
/*
 * this tests a file name outside the arena
 * It shoudl fail
*/

int main() {
    std::cout << "In main, mapping a new file to invalid block\n";
    const char* failure = reinterpret_cast<char*>(0x12345678); 
    std::cout << "Casting result to failed\n";
    void* result = vm_map(failure, 0);
    if (result == nullptr) {
        std::cout << "vm_map failed\n";
    }
    std::cout << "Done with testing a failure\n";
    return 0;
}